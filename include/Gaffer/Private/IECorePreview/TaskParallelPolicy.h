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

#ifndef IECOREPREVIEW_TASKPARALLELPOLICY_H
#define IECOREPREVIEW_TASKPARALLELPOLICY_H

#include "Gaffer/Private/IECorePreview/LRUCache.h"

#include "Gaffer/Private/IECorePreview/TaskMutex.h"

namespace IECorePreview
{

namespace LRUCachePolicy
{

/// DOCUMENT ME!!!!!!!!!!!
template<typename Key>
bool spawnsTasks( const Key &key )
{
	return true;
}

/// Thread-safe policy that uses TaskMutex so that threads waiting on
/// the cache can still perform useful work.
/// \todo This uses the same binned approach to map storage as the
/// standard Parallel policy. Can we share the code?
template<typename LRUCache>
class TaskParallel
{

	public :

		typedef typename LRUCache::CacheEntry CacheEntry;
		typedef typename LRUCache::KeyType Key;
		typedef tbb::atomic<typename LRUCache::Cost> AtomicCost;

		struct Item
		{
			Item() : recentlyUsed() {}
			Item( const Key &key ) : key( key ), recentlyUsed() {}
			Item( const Item &other ) : key( other.key ), cacheEntry( other.cacheEntry ), recentlyUsed() {}
			Key key;
			mutable CacheEntry cacheEntry;
			// Mutex to protect cacheEntry.
			typedef TaskMutex Mutex;
			mutable Mutex mutex;
			// Flag used in second-chance algorithm.
			mutable tbb::atomic<bool> recentlyUsed;
		};

		// We would love to use one of TBB's concurrent containers as
		// our map, but we need the ability to insert, erase and iterate
		// concurrently. The concurrent_unordered_map doesn't provide
		// concurrent erase, and the concurrent_hash_map doesn't provide
		// concurrent iteration. Instead we choose a non-threadsafe
		// container, but split our storage into multiple bins with a
		// container in each bin. This way concurrent operations do not
		// contend on a lock unless they happen to target the same bin.
		typedef boost::multi_index::multi_index_container<
			Item,
			boost::multi_index::indexed_by<
				// Equivalent to std::unordered_map, using Item::key
				// as the key. This actually has a couple of benefits
				// over std::unordered_map :
				//
				// - Insertion does not invalidate existing iterators.
				//   This allows us to store m_popIterator.
				// - Lookup can be performed using types other than the
				//   key. This provides the possibility of creating a
				//   prehashed key prior to taking a Bin lock, although
				//   this is not implemented here yet.
				boost::multi_index::hashed_unique<
					boost::multi_index::member<Item, Key, &Item::key>
				>
			>
		> Map;

		typedef typename Map::iterator MapIterator;

		struct Bin
		{
			Bin() {}
			Bin( const Bin &other ) : map( other.map ) {}
			Bin &operator = ( const Bin &other ) { map = other.map; return *this; }
			Map map;
			typedef tbb::spin_rw_mutex Mutex;
			Mutex mutex;
		};

		typedef std::vector<Bin> Bins;

		TaskParallel()
		{
			m_bins.resize( tbb::tbb_thread::hardware_concurrency() );
			m_popBinIndex = 0;
			m_popIterator = m_bins[0].map.begin();
			currentCost = 0;
		}

		struct Handle : private boost::noncopyable
		{

			Handle()
				:	m_item( nullptr ), m_writable( false ), m_spawnsTasks( false )
			{
			}

			~Handle()
			{
			}

			const CacheEntry &readable()
			{
				return m_item->cacheEntry;
			}

			CacheEntry &writable()
			{
				assert( m_writable );
				return m_item->cacheEntry;
			}

			template<typename F>
			void execute( F &&f )
			{
				if( m_spawnsTasks )
				{
					// The getter function will spawn tasks. Execute
					// it via the TaskMutex, so that other threads trying
					// to access this cache item can help out. This also
					// means that the getter is executed inside a task_arena,
					// preventing it from stealing outer tasks that might try
					// to get this item from the cache, leading to deadlock.
					m_itemLock.execute( f );
				}
				else
				{
					// The getter won't do anything involving TBB tasks.
					// Avoid the overhead of executing via the TaskMutex.
					f();
				}
			}

			void release()
			{
				if( m_item )
				{
					m_itemLock.release();
					m_item = nullptr;
				}
			}

			bool recursive() const
			{
				return m_itemLock.recursive();
			}

			private :

				bool acquire( Bin &bin, const Key &key, AcquireMode mode, bool spawnsTasks )
				{
					assert( !m_item );

					// Acquiring a handle requires taking two
					// locks, first the lock for the Bin, and
					// second the lock for the Item. We must be
					// careful to avoid deadlock in the case of
					// a GetterFunction which reenters the cache.

					typename Bin::Mutex::scoped_lock binLock;
					while( true )
					{
						// Acquire a lock on the bin, and get an iterator
						// from the key. We optimistically assume the item
						// may already be in the cache and first do a find()
						// using a bin read lock. This gives us much better
						// performance when many threads contend for items
						// that are already in the cache.
						binLock.acquire( bin.mutex, /* write = */ false );
						MapIterator it = bin.map.find( key );
						bool inserted = false;
						if( it == bin.map.end() )
						{
							if( mode != Insert && mode != InsertWritable )
							{
								return false;
							}
							binLock.upgrade_to_writer();
							std::tie<MapIterator, bool>( it, inserted ) = bin.map.insert( Item( key ) );
						}

						// Now try to get a lock on the item we want to
						// acquire. When we've just inserted a new item
						// we take a write lock directly, because we know
						// we'll need to write to the new item. When insertion
						// found a pre-existing item we optimistically take
						// just a read lock, because it is faster when
						// many threads just need to read from the same
						// cached item.
						m_writable = inserted || mode == FindWritable || mode == InsertWritable;

						const bool acquired = m_itemLock.acquireOr(
							it->mutex, /* write = */ m_writable,
							[&binLock, &spawnsTasks]{ binLock.release(); return spawnsTasks; }
						);

						if( acquired )
						{
							if( !m_writable && mode == Insert && it->cacheEntry.status() == LRUCache::Uncached )
							{
								// We found an old item that doesn't have
								// a value. This can either be because it
								// was erased but hasn't been popped yet,
								// or because the item was too big to fit
								// in the cache. Upgrade to writer status
								// so it can be updated in get().
								m_itemLock.upgradeToWriter();
								m_writable = true;
							}
							// Success!
							m_item = &*it;
							m_spawnsTasks = spawnsTasks;
							return true;
						}
					}
				}

				friend class TaskParallel;

				const Item *m_item;
				typename Item::Mutex::ScopedLock m_itemLock;
				bool m_writable;
				bool m_spawnsTasks;

		};

		/// Templated so that we can be called with the GetterKey as
		/// well as the regular Key.
		template<typename K>
		bool acquire( const K &key, Handle &handle, AcquireMode mode )
		{
			return handle.acquire(
				bin( key ), key, mode,
				/// Only accept work for Insert mode, because that is
				/// the one used by `get()`. We don't want to attempt
				/// to do work in `set()`, because there will be no work
				/// to do.
				/// NEED TESTS THAT PROVE THIS IS HAPPENING? OR AT LEAST
				/// SOME PERFORMANCE TESTS FOR USAGE WITH A NULL GETTER.
				mode == AcquireMode::Insert && spawnsTasks( key )
			);
		}

		void push( Handle &handle )
		{
			// Simply mark the item as having been used
			// recently. We will then give it a second chance
			// in pop(), so it will not be evicted immediately.
			// We don't need the handle to be writable to write
			// here, because `recentlyUsed` is atomic.
			handle.m_item->recentlyUsed = true;
		}

		bool pop( Key &key, CacheEntry &cacheEntry )
		{
			// Popping works by iterating the map until an item
			// that has not been recently used is found. We store
			// the current iteration position as m_popIterator and
			// protect it with m_popMutex, taking the position that
			// it is sufficient for only one thread to be limiting
			// cost at any given time.
			PopMutex::scoped_lock lock;
			if( !lock.try_acquire( m_popMutex ) )
			{
				return false;
			}

			Bin *bin = &m_bins[m_popBinIndex];
			typename Bin::Mutex::scoped_lock binLock( bin->mutex );

			typename Item::Mutex::ScopedLock itemLock;
			int numFullIterations = 0;
			while( true )
			{
				// If we're at the end of this bin, advance to
				// the next non-empty one.
				const MapIterator emptySentinel = bin->map.end();
				while( m_popIterator == bin->map.end() )
				{
					binLock.release();
					m_popBinIndex = ( m_popBinIndex + 1 ) % m_bins.size();
					bin = &m_bins[m_popBinIndex];
					binLock.acquire( bin->mutex );
					m_popIterator = bin->map.begin();
					if( m_popIterator == emptySentinel )
					{
						// We've come full circle and all bins were empty.
						return false;
					}
					else if( m_popBinIndex == 0 )
					{
						if( numFullIterations++ > 50 )
						{
							// We're not empty, but we've been around and around
							// without finding anything to pop. This could happen
							// if other threads are frantically setting
							// the `recentlyUsed` flag or if `clear()` is
							// called from `get()`, while `get()` holds the lock
							// on the only item we could pop.
							return false;
						}
					}
				}

				if( itemLock.tryAcquire( m_popIterator->mutex ) )
				{
					if( !m_popIterator->recentlyUsed )
					{
						// Pop this item.
						key = m_popIterator->key;
						cacheEntry = m_popIterator->cacheEntry;
						// Now erase it from the bin.
						// We must release the lock on the Item before erasing it,
						// because we cannot release a lock on a mutex that is
						// already destroyed. We know that no other thread can
						// gain access to the item though, because they must
						// acquire the Bin lock to do so, and we still hold the
						// Bin lock.
						itemLock.release();
						m_popIterator = bin->map.erase( m_popIterator );
						return true;
					}
					else
					{
						// Item has been used recently. Flag it so we
						// can pop it next time round, unless another
						// thread resets the flag.
						m_popIterator->recentlyUsed = false;
						itemLock.release();
					}
				}
				else
				{
					// Failed to acquire the item lock. Some other
					// thread is busy with this item, so we consider
					// it to be recently used and just skip over it.
				}

				++m_popIterator;
			}
		}

		AtomicCost currentCost;

	private :

		Bins m_bins;

		Bin &bin( const Key &key )
		{
			size_t binIndex = boost::hash<Key>()( key ) % m_bins.size();
			return m_bins[binIndex];
		};

		typedef tbb::spin_mutex PopMutex;
		PopMutex m_popMutex;
		size_t m_popBinIndex;
		MapIterator m_popIterator;

};

} // namespace LRUCachePolicy

} // namespace IECorePreview

#endif // IECOREPREVIEW_TASKPARALLELPOLICY_H
