//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2019, Image Engine Design Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//       other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#ifndef IECOREPREVIEW_TASKMUTEX_H
#define IECOREPREVIEW_TASKMUTEX_H

#include "tbb/spin_mutex.h"
#include "tbb/task_arena.h"
#include "tbb/task_group.h"

#include <iostream> // REMOVE ME!

namespace IECorePreview
{

/// Mutex where threads waiting for access can collaborate on TBB tasks
/// spawned by the holder. Useful for performing expensive delayed
/// initialisation of shared resources.
///
/// Based on an example posted by "Alex (Intel)" on the following thread :
///
/// https://software.intel.com/en-us/forums/intel-threading-building-blocks/topic/703652
struct TaskMutex : boost::noncopyable
{

	TaskMutex()
	{
	}

	/// INVESTIGATE READ/WRITE LOCKING - IT MIGHT NOT BE SO BAD

	class ScopedLock : boost::noncopyable
	{

		public :

			ScopedLock()
				:	m_mutex( nullptr )
			{
			}

			ScopedLock( TaskMutex &mutex, bool acceptWork = true )
				:	m_mutex( nullptr )
			{
				acquire( mutex, acceptWork );
			}

			~ScopedLock()
			{
				if( m_mutex )
				{
					release();
				}
			}

			void acquire( TaskMutex &mutex, bool acceptWork = true )
			{
				tbb::internal::atomic_backoff backoff;
				while( !acquireOr( mutex, [acceptWork](){ return acceptWork; } ) )
				{
					backoff.pause();
				}
			}

			template<typename F>
			void execute( F &&f )
			{
				assert( m_mutex );
				InternalMutex::scoped_lock arenaLock( m_mutex->m_arenaMutex );
				if( !m_mutex->m_arenaAndTaskGroup )
				{
					m_mutex->m_arenaAndTaskGroup = std::make_shared<ArenaAndTaskGroup>();
				}
				//ArenaAndTaskGroupPtr arenaAndTaskGroup = m_mutex->m_arenaAndTaskGroup;
				arenaLock.release();
				/// POSSIBLY WE DON'T NEED TO TAKE THE COPY HERE? BECAUSE WE ARE THE ONLY
				/// ONE WHO WILL WRITE TO m_arenaAndTaskGroup?

				m_mutex->m_arenaAndTaskGroup->arena.execute(
					[this, &f]{ m_mutex->m_arenaAndTaskGroup->taskGroup.run_and_wait( f ); }
				);

				arenaLock.acquire( m_mutex->m_arenaMutex );
				m_mutex->m_arenaAndTaskGroup = nullptr;
			}

			/// Acquires mutex or returns false. Never does TBB tasks.
			bool tryAcquire( TaskMutex &mutex )
			{
				return acquireOr( mutex, [](){ return false; } );
			}

			/// Tries to acquire the mutex, and returns true on success.
			/// On failure, calls workAccepter and if it returns true,
			/// performs TBB tasks until the mutex is released by the
			/// holding thread. Returns false on failure, regardless of
			/// whether or not tasks are done.
			template<typename WorkAccepter>
			bool acquireOr( TaskMutex &mutex, WorkAccepter &&workAccepter )
			{
				assert( !m_mutex );
				if( mutex.m_mutex.try_lock() )
				{
					// Success!
					m_mutex = &mutex;
					// NEED THE LOCK FOR THIS, AT LEAST UNTIL YOU GET ATOMIC
					// OPERATIONS...
					//assert( !mutex.m_arenaAndTaskGroup );
					return true;
				}

				if( !workAccepter() )
				{
					return false;
				}

				InternalMutex::scoped_lock arenaLock( mutex.m_arenaMutex );
				ArenaAndTaskGroupPtr arenaAndTaskGroup = mutex.m_arenaAndTaskGroup;
				arenaLock.release();
				if( !arenaAndTaskGroup )
				{
					return false;
				}

				arenaAndTaskGroup->arena.execute(
					[&arenaAndTaskGroup]{ arenaAndTaskGroup->taskGroup.wait(); }
				);
				return false;
			}

			void release()
			{
				assert( m_mutex );
				m_mutex->m_mutex.unlock();
				m_mutex = nullptr;
			}

		private :

			TaskMutex *m_mutex;

	};

	private :

		typedef tbb::spin_mutex InternalMutex;

		struct ArenaAndTaskGroup
		{
			tbb::task_arena arena;
			tbb::task_group taskGroup;
		};
		typedef std::shared_ptr<ArenaAndTaskGroup> ArenaAndTaskGroupPtr;

		// The actual mutex that is held
		// by the scoped_lock.
		InternalMutex m_mutex;

		InternalMutex m_arenaMutex;
		ArenaAndTaskGroupPtr m_arenaAndTaskGroup;

};

} // namespace IECorePreview

#endif // IECOREPREVIEW_TASKMUTEX_H
