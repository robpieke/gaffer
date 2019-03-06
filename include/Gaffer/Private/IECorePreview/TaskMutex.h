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
#include "tbb/task.h"

namespace IECorePreview
{

/// Mutex where threads waiting for access can steal and execute TBB tasks
/// while they wait. This is particularly useful when the mutex protects a
/// cached computation that will itself spawn TBB tasks.
///
/// > Note : When spawning TBB tasks while holding a lock, you _must_ use
/// > [task isolation](https://software.intel.com/en-us/blogs/2018/08/16/the-work-isolation-functionality-in-intel-threading-building-blocks-intel-tbb).
struct TaskMutex : boost::noncopyable
{

	TaskMutex()
		:	m_tasks( nullptr )
	{
	}

	class ScopedLock : boost::noncopyable
	{

		public :

			ScopedLock()
				:	m_mutex( nullptr )
			{
			}

			ScopedLock( TaskMutex &mutex )
				:	m_mutex( nullptr )
			{
				acquire( mutex );
			}

			~ScopedLock()
			{
				if( m_mutex )
				{
					release();
				}
			}

			/// Acquires mutex, possibly doing TBB tasks while waiting.
			void acquire( TaskMutex &mutex )
			{
				while( !acquireOr( mutex, [](){ return true; } ) )
				{
				}
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

				InternalMutex::scoped_lock tasksLock( mutex.m_tasksMutex );

				if( mutex.m_mutex.try_lock() )
				{
					// Success!
					m_mutex = &mutex;
					return true;
				}

				if( !workAccepter() )
				{
					return false;
				}

				// Help the current holder of the lock by executing tbb
				// tasks until the lock is released and we can try to
				// acquire it again. The trick used here was suggested by
				// Raf Schietekat on the following thread :
				//
				// https://software.intel.com/en-us/forums/intel-threading-building-blocks/topic/285550
				tbb::empty_task *task = new (tbb::task::allocate_root()) tbb::empty_task;
				WaitingTask *waitingTask = new (task->allocate_child()) WaitingTask;
				task->set_ref_count( 2 );
				waitingTask->next = mutex.m_tasks;
				mutex.m_tasks = waitingTask;
				tasksLock.release();

				// Steals tasks until refcount == 1. Refcount only becomes
				// one after `waitingTask` has been spawned in `release()`.
				task->wait_for_all();
				task->destroy( *task );

				return false;
			}

			void release()
			{
				assert( m_mutex );

				// Lock the tasks first. We don't want more tasks to
				// be added in between us releasing the lock and releasing
				// the existing tasks.
				InternalMutex::scoped_lock tasksLock( m_mutex->m_tasksMutex );

				// Unlock the mutex.
				m_mutex->m_mutex.unlock();

				// Spawn all the waiting tasks, so that the `wait_for_all()`
				// calls in the helper threads can return.
				WaitingTask *task = m_mutex->m_tasks;
				while( task )
				{
					WaitingTask *next = task->next;
					task->parent()->spawn( *task );
					task = next;
				}
				m_mutex->m_tasks = nullptr;
				m_mutex = nullptr;
			}

		private :

			TaskMutex *m_mutex;

	};

	private :

		typedef tbb::spin_mutex InternalMutex;

		// A dummy task we use for stealing
		// work from the tbb scheduler.
		struct WaitingTask : public tbb::empty_task
		{
			// Used to create a cheap
			// linked list of tasks.
			WaitingTask *next;
			virtual tbb::task *execute()
			{
				return nullptr;
			}
		};

		// The actual mutex that is held
		// by the scoped_lock.
		InternalMutex m_mutex;

		// A second mutex to protect the
		// list of tasks. This isn't intended
		// to be visible to the outside world,
		// so must never be held on return from
		// any methods.
		/// \todo There's probably some fancy
		/// lock-free way of managing the list
		/// of tasks.
		InternalMutex m_tasksMutex;
		// Head of a singly linked list of
		// worker tasks.
		WaitingTask *m_tasks;

};

} // namespace IECorePreview

#endif // IECOREPREVIEW_TASKMUTEX_H
