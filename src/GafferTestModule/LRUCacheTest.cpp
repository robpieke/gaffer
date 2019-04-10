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
struct TestLRUCache
{

	TestLRUCache( int numIterations, int numValues, int maxCost, int clearFrequency )
		:	m_numIterations( numIterations ), m_numValues( numValues ), m_maxCost( maxCost ), m_clearFrequency( clearFrequency )
	{
	}

	void operator()()
	{
		typedef LRUCache<int, int, Policy> Cache;
		Cache cache(
			[]( int key, size_t &cost ) { cost = 1; return key; },
			m_maxCost
		);

		tbb::parallel_for(
			tbb::blocked_range<size_t>( 0, m_numIterations ),
			[&]( const tbb::blocked_range<size_t> &r ) {
				for( size_t i=r.begin(); i!=r.end(); ++i )
				{
					const int k = i % m_numValues;
					const int v = cache.get( k );
					GAFFERTEST_ASSERTEQUAL( v, k );

					if( m_clearFrequency && (i % m_clearFrequency == 0) )
					{
						cache.clear();
					}
				}
			}
		);
	}

	private :

		const int m_numIterations;
		const int m_numValues;
		const int m_maxCost;
		const int m_clearFrequency;

};

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

template<template<typename> class Policy>
struct TestLRUCacheRecursionOnOneItem
{

	void operator()()
	{
		typedef LRUCache<int, int, Policy> Cache;
		typedef std::unique_ptr<Cache> CachePtr;
		int recursionDepth = 0;

		CachePtr cache;
		cache.reset(
			new Cache(
				// Getter that calls back into the cache with the _same_
				// key, up to a certain limit, and then actually returns
				// a value. This is basically insane, but it models
				// situations that can occur in Gaffer.
				[&cache, &recursionDepth]( int key, size_t &cost ) {
					cost = 1;
					if( ++recursionDepth == 100 )
					{
					return key;
					}
					else
					{
					return cache->get( key );
					}
				},
				// Max cost is small enough that we'll be trying to evict
				// keys while unwinding the recursion.
				20
			)
		);

		for( int i = 0; i < 100000; ++i )
		{
			recursionDepth = 0;
			cache->clear();
			GAFFERTEST_ASSERTEQUAL( cache->currentCost(), 0 );
			GAFFERTEST_ASSERTEQUAL( cache->get( i ), i );
			GAFFERTEST_ASSERTEQUAL( recursionDepth, 100 );
			GAFFERTEST_ASSERTEQUAL( cache->currentCost(), 1 );
		}
	}

};

template<template<template<typename> class> class F>
struct DispatchTest
{

	template<typename... Args>
	void operator()( const std::string &policy, Args&&... args )
	{
		if( policy == "serial" )
		{
			F<LRUCachePolicy::Serial> f( std::forward<Args>( args )... );
			// Use an arena to limit any parallel TBB work to 1
			// thread, since Serial policy is not threadsafe.
			tbb::task_arena arena( 1 );
			arena.execute(
				[&f]{ f(); }
			);
		}
		else if( policy == "parallel" )
		{
			F<LRUCachePolicy::Parallel> f( std::forward<Args>( args )... ); f();
		}
		else if( policy == "taskParallel" )
		{
			F<LRUCachePolicy::TaskParallel> f( std::forward<Args>( args )... ); f();
		}
		else
		{
			GAFFERTEST_ASSERT( false );
		}
	}

};

} // namespace

void GafferTestModule::testLRUCache( const std::string &policy, int numIterations, int numValues, int maxCost, int clearFrequency )
{
	DispatchTest<TestLRUCache>()( policy, numIterations, numValues, maxCost, clearFrequency );
}

void GafferTestModule::testLRUCacheContentionForOneItem( const std::string &policy )
{
	DispatchTest<TestLRUCacheContentionForOneItem>()( policy );
}

void GafferTestModule::testLRUCacheRecursionOnOneItem( const std::string &policy )
{
	DispatchTest<TestLRUCacheRecursionOnOneItem>()( policy );
}
