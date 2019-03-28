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

#include "LRUCacheTest.h"

#include "GafferTest/Assert.h"

#include "Gaffer/Private/IECorePreview/LRUCache.h"
#include "Gaffer/Private/IECorePreview/TaskParallelPolicy.h"

#include "tbb/parallel_for.h"

using namespace IECorePreview;

namespace
{

template<template<typename> class Policy>
struct TestLRUCacheContentionForOneItem
{

	void operator()()
	{
		typedef LRUCache<int, int, Policy> Cache;
		Cache cache(
			[]( int key, size_t &cost ) { cost = 1; return key; },
			100
		);

		tbb::parallel_for(
			tbb::blocked_range<size_t>( 0, 10000000 ),
			[&]( const tbb::blocked_range<size_t> &r ) {
				for( size_t i = r.begin(); i < r.end(); ++i )
				{
					GAFFERTEST_ASSERTEQUAL( cache.get( 1 ), 1 );
				}
			}
		);
	}


};

template<template<template<typename> class> class F>
struct DispatchTest
{

	void operator()( const std::string &policy )
	{
		if( policy == "serial" )
		{
			// Use an arena to limit any parallel TBB work to 1
			// thread, since Serial policy is not threadsafe.
			tbb::task_arena arena( 1 );
			arena.execute(
				[]{ F<LRUCachePolicy::Serial> f; f(); }
			);
		}
		else if( policy == "parallel" )
		{
			F<LRUCachePolicy::Parallel> f; f();
		}
		else if( policy == "taskParallel" )
		{
			F<LRUCachePolicy::TaskParallel> f; f();
		}
		else
		{
			GAFFERTEST_ASSERT( false );
		}
	}

};

} // namespace

void GafferTestModule::testLRUCacheContentionForOneItem( const std::string &policy )
{
	DispatchTest<TestLRUCacheContentionForOneItem>()( policy );
}
