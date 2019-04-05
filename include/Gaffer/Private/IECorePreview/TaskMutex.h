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
#include "tbb/spin_rw_mutex.h"

#include "tbb/task_arena.h"
#include "tbb/task_group.h"

#include <iostream> // REMOVE ME!
#include <thread>

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

	typedef tbb::spin_rw_mutex InternalMutex;

	class ScopedLock : boost::noncopyable
	{

		public :

			ScopedLock()
				:	m_mutex( nullptr ), m_recursive( false )
			{
			}

			ScopedLock( TaskMutex &mutex, bool write = true, bool acceptWork = true )
				:	ScopedLock()
			{
				acquire( mutex, write, acceptWork );
			}

			~ScopedLock()
			{
				if( m_mutex )
				{
					release();
				}
			}

			void acquire( TaskMutex &mutex, bool write = true, bool acceptWork = true )
			{
				tbb::internal::atomic_backoff backoff;
				while( !acquireOr( mutex, write, [acceptWork](){ return acceptWork; } ) )
				{
					backoff.pause();
				}
			}

			template<typename F>
			void execute( F &&f )
			{
				assert( m_mutex );

				if( m_recursive )
				{
					assert( m_mutex->m_executionState );
					assert( m_mutex->m_executionState->threadId == std::this_thread::get_id() );
					f();
					return;
				}

				ExecutionStateMutex::scoped_lock executionStateLock( m_mutex->m_executionStateMutex );
				assert( !m_mutex->m_executionState );
				m_mutex->m_executionState = std::make_shared<ExecutionState>();
				m_mutex->m_executionState->threadId = std::this_thread::get_id();
				executionStateLock.release();

				m_mutex->m_executionState->arena.execute(
					[this, &f]{ m_mutex->m_executionState->taskGroup.run_and_wait( f ); }
				);

				executionStateLock.acquire( m_mutex->m_executionStateMutex );
				m_mutex->m_executionState = nullptr;
			}

			/// Acquires mutex or returns false. Never does TBB tasks.
			bool tryAcquire( TaskMutex &mutex, bool write = true )
			{
				return acquireOr( mutex, write, [](){ return false; } );
			}

			/// Tries to acquire the mutex, and returns true on success.
			/// On failure, calls workAccepter and if it returns true,
			/// performs TBB tasks until the mutex is released by the
			/// holding thread. Returns false on failure, regardless of
			/// whether or not tasks are done.
			template<typename WorkAccepter>
			bool acquireOr( TaskMutex &mutex, bool write, WorkAccepter &&workAccepter )
			{
				assert( !m_mutex );
				if( m_lock.try_acquire( mutex.m_mutex, write ) )
				{
					// Success!
					m_mutex = &mutex;
					m_recursive = false;
					return true;
				}

				ExecutionStateMutex::scoped_lock executionStateLock( mutex.m_executionStateMutex );
				if( !mutex.m_executionState )
				{
					return false;
				}

				if( mutex.m_executionState->threadId == std::this_thread::get_id() )
				{
					m_mutex = &mutex;
					m_recursive = true;
					return true;
				}

				if( !workAccepter() )
				{
					return false;
				}

				ExecutionStatePtr executionState = mutex.m_executionState;
				executionStateLock.release();

				executionState->arena.execute(
					[&executionState]{ executionState->taskGroup.wait(); }
				);
				return false;
			}

			bool upgradeToWriter()
			{
				if( m_recursive )
				{
					/// WE NEED TO ENFORCE THIS PROPERLY BY GUARANTEEING THAT
					/// THE CALLER OF EXECUTE() HAS WRITE ACCESS.
					return true;
				}
				return m_lock.upgrade_to_writer();
			}

			void release()
			{
				assert( m_mutex );
				if( !m_recursive )
				{
					m_lock.release();
				}
				m_mutex = nullptr;
			}

		private :

			InternalMutex::scoped_lock m_lock;
			TaskMutex *m_mutex;
			bool m_recursive;

	};

	private :

		// The actual mutex that is held
		// by the scoped_lock.
		InternalMutex m_mutex;

		// The mechanism we use to allow waiting threads
		// to participate in the work done by `execute()`.
		// This also contains state used to manage recursive
		// locks.
		struct ExecutionState
		{
			// Arena and task group used to allow
			// waiting threads to participate in work.
			tbb::task_arena arena;
			tbb::task_group taskGroup;
			// Thread that called execute().
			std::thread::id threadId;
		};
		typedef std::shared_ptr<ExecutionState> ExecutionStatePtr;

		typedef tbb::spin_mutex ExecutionStateMutex;
		ExecutionStateMutex m_executionStateMutex;
		ExecutionStatePtr m_executionState;

};

} // namespace IECorePreview

#endif // IECOREPREVIEW_TASKMUTEX_H
