//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2019, Image Engine Design Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//      * Redistributions of source code must retain the above
//        copyright notice, this list of conditions and the following
//        disclaimer.
//
//      * Redistributions in binary form must reproduce the above
//        copyright notice, this list of conditions and the following
//        disclaimer in the documentation and/or other materials provided with
//        the distribution.
//
//      * Neither the name of John Haddon nor the names of
//        any other contributors to this software may be used to endorse or
//        promote products derived from this software without specific prior
//        written permission.
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

#include "TaskMutexTest.h"

#include "GafferTest/Assert.h"

#include "Gaffer/Private/IECorePreview/ParallelAlgo.h"
#include "Gaffer/Private/IECorePreview/TaskMutex.h"

#include "tbb/enumerable_thread_specific.h"
#include "tbb/parallel_for.h"

#include <thread>

using namespace IECorePreview;

void GafferTestModule::testTaskMutex()
{
	// Mutex and bool used to model lazy initialisation.
	TaskMutex mutex;
	bool initialised = false;

	// Tracking to see what various threads get up to.
	tbb::enumerable_thread_specific<int> didInitialisation;
	tbb::enumerable_thread_specific<int> didInitialisationTasks;
	tbb::enumerable_thread_specific<int> gotLock;

	// Lazy initialisation function

	auto initialise = [&]() {

		TaskMutex::ScopedLock lock( mutex );
		gotLock.local() = true;

		if( !initialised )
		{
			// Simulate an expensive multithreaded
			// initialisation process.
			ParallelAlgo::isolate(
				[&]() {
					tbb::parallel_for(
						tbb::blocked_range<size_t>( 0, 1000000 ),
						[&]( const tbb::blocked_range<size_t> &r ) {
							didInitialisationTasks.local() = true;
							std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
						}
					);
				}
			);
			initialised = true;
			didInitialisation.local() = true;
		}
	};

	// Generate a bunch of tasks that will each try to
	// do the lazy initialisation. Only one should do it,
	// but the rest should help out in doing the work.

	tbb::parallel_for(
		tbb::blocked_range<size_t>( 0, 1000000 ),
		[&]( const tbb::blocked_range<size_t> &r ) {
			initialise();
		}
	);

	// Only one thread should have done the initialisation,
	// but everyone should have got the lock, and everyone should
	// have done some work.
	GAFFERTEST_ASSERTEQUAL( didInitialisation.size(), 1 );
	GAFFERTEST_ASSERTEQUAL( gotLock.size(), tbb::tbb_thread::hardware_concurrency() );
	GAFFERTEST_ASSERTEQUAL( didInitialisationTasks.size(), tbb::tbb_thread::hardware_concurrency() );

}
