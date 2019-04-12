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

#include "boost/container/flat_set.hpp"
#include "boost/noncopyable.hpp"

#include "tbb/spin_mutex.h"
#include "tbb/spin_rw_mutex.h"

#include "tbb/task_arena.h"
#include "tbb/task_group.h"
// Enable preview feature that allows us to construct a task_scheduler_observer
// for a specific task_arena. This feature becomes officially supported in
// Intel TBB 2019 Update 5, so it is not going to be removed.
#define TBB_PREVIEW_LOCAL_OBSERVER 1
#include "tbb/task_scheduler_observer.h"

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
///
/// MAKE ME BE A CLASS!!!!!!
struct TaskMutex : boost::noncopyable
{

	TaskMutex()
	{
	}

	typedef tbb::spin_rw_mutex InternalMutex; // HIDE ME!!!!!!!!!!!

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

			bool recursive() const
			{
				return m_recursive;
			}

			template<typename F>
			void execute( F &&f )
			{
				assert( m_mutex );

				if( m_recursive )
				{
					assert( m_mutex->m_executionState );
					assert( m_mutex->m_executionState->arenaObserver.containsThisThread() );
					f();
					return;
				}

				ExecutionStateMutex::scoped_lock executionStateLock( m_mutex->m_executionStateMutex );
				assert( !m_mutex->m_executionState );
				m_mutex->m_executionState = std::make_shared<ExecutionState>();
				executionStateLock.release();

				m_mutex->m_executionState->arena.execute(
					[this, &f] {
						// Note : We deliberately call `run()` and `wait()` separately
						// instead of calling `run_and_wait()`. The latter is buggy until
						// TBB 2018 Update 3, causing calls to `wait()` on other threads to
						// return immediately rather than do the work we want.
						m_mutex->m_executionState->taskGroup.run( f );
						m_mutex->m_executionState->taskGroup.wait();
					}
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
			/// may perform TBB tasks until the mutex is released by the
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

				/// MOVING THIS HERE SOLVES A DEADLOCK IN THE TASKPARALLEL POLICY
				/// BECAUSE IT USES THE WORK ACCEPTER TO RELEASE THE BIN LOCK. BUT
				/// IT'S A BIT ODD TO CALL THE WORK ACCEPTER WHEN THERE'S NO WORK TO
				/// DO, OR WE MIGHT GET A RECURSIVE LOCK. ON THE OTHER HAND, IT'S
				/// STRANGE TO NEITHER ACQUIRE NOR CALL THE WORKACCEPTER. WHAT MAKES
				/// THE MOST SENSE?
				if( !workAccepter() )
				{
					return false;
				}

				ExecutionStateMutex::scoped_lock executionStateLock( mutex.m_executionStateMutex );
				if( !mutex.m_executionState )
				{
					return false;
				}

				if( mutex.m_executionState->arenaObserver.containsThisThread() )
				{
					m_mutex = &mutex;
					m_recursive = true;
					return true;
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
					/// THE CALLER OF EXECUTE() HAS WRITE ACCESS. OR WE NEED
					/// TO MAKE IT FAIL FOR RECURSIVE TASKS.
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

				/// DO WE NEED TO WAIT FOR ALL WORKERS HERE???
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

		// Tracks worker threads as they enter and exit an arena, so we can determine
		// whether or not the current thread is inside the arena. We use this to detect
		// recursion and allow any worker thread to obtain a recursive lock provided
		// they are currently performing work in service of `ScopedLock::execute()`.

		class ArenaObserver : public tbb::task_scheduler_observer
		{

			public :

				ArenaObserver( tbb::task_arena &arena )
					:	tbb::task_scheduler_observer( arena )
				{
					observe( true );
				}

				~ArenaObserver()
				{
					observe( false );
				}

				bool containsThisThread()
				{
					Mutex::scoped_lock lock( m_mutex );
					return m_threadIdSet.find( std::this_thread::get_id() ) != m_threadIdSet.end();
				}

			private :

				void on_scheduler_entry( bool isWorker ) override
				{
					assert( !containsThisThread() );
					Mutex::scoped_lock lock( m_mutex );
					m_threadIdSet.insert( std::this_thread::get_id() );
				}

				void on_scheduler_exit( bool isWorker ) override
				{
					Mutex::scoped_lock lock( m_mutex );
					m_threadIdSet.erase( std::this_thread::get_id() );
				}

				using Mutex = tbb::spin_mutex;
				using ThreadIdSet = boost::container::flat_set<std::thread::id>;
				Mutex m_mutex;
				ThreadIdSet m_threadIdSet;

		};

		// The mechanism we use to allow waiting threads
		// to participate in the work done by `execute()`.
		// This also contains state used to manage recursive
		// locks.
		struct ExecutionState : private boost::noncopyable
		{
			ExecutionState()
				:	arenaObserver( arena )
			{
			}

			// Arena and task group used to allow
			// waiting threads to participate in work.
			tbb::task_arena arena;
			tbb::task_group taskGroup;
			// Observer used to track which threads are
			// currently inside the arena.
			ArenaObserver arenaObserver;
		};
		typedef std::shared_ptr<ExecutionState> ExecutionStatePtr;

		typedef tbb::spin_mutex ExecutionStateMutex;
		ExecutionStateMutex m_executionStateMutex;
		ExecutionStatePtr m_executionState;

};

} // namespace IECorePreview

#endif // IECOREPREVIEW_TASKMUTEX_H
