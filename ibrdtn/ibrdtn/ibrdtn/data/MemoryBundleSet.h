/*
 * MemoryBundleSet.h
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  Created on: 18.12.2012
 *	renamed from BundleSet.h 04.04.2013
 */

#ifndef MEMORYBUNDLESET_H_
#define MEMORYBUNDLESET_H_

#include "ibrdtn/data/BundleSetImpl.h"
#include "ibrdtn/data/BundleSet.h"
#include "ibrdtn/data/MetaBundle.h"
#include <ibrcommon/data/BloomFilter.h>
#include <ibrcommon/data/File.h>
#include <set>

namespace dtn
{
	namespace data
	{
		class MemoryBundleSet : public BundleSetImpl
		{
		public:
			/**
			 * Constructor
			 * @param listener This listener gets notified about expired bundles.
			 * @param bf_size Initial size fo the bloom-filter.
			 */
			MemoryBundleSet(BundleSet::Listener *listener = NULL, Length bf_size = 1024);

			/**
			 * Constructor
			 * @param name The identifier of the named bundle-set.
			 * @param listener This listener gets notified about expired bundles.
			 * @param bf_size Initial size fo the bloom-filter.
			 */
			MemoryBundleSet(const std::string &name, BundleSet::Listener *listener = NULL, Length bf_size = 1024);

			/**
			 * Destructor
			 */
			virtual ~MemoryBundleSet();

			/**
			 * copies the current bundle-set into a new temporary one
			 */
			virtual refcnt_ptr<BundleSetImpl> copy() const;

			/**
			 * clears the bundle-set and copy all entries from the given
			 * one into this bundle-set
			 */
			virtual void assign(const refcnt_ptr<BundleSetImpl>&);

			/**
			 * Add a bundle to the bundle-set
			 */
			virtual void add(const dtn::data::MetaBundle &bundle) noexcept;

			/**
			 * Clear the whole bundle-set
			 */
			virtual void clear() noexcept;

			/**
			 * Check if a bundle id is in this bundle-set
			 */
			virtual bool has(const dtn::data::BundleID &bundle) const noexcept;

			/**
			 * Check for expired entries in this bundle-set and remove them
			 */
			virtual void expire(const Timestamp timestamp) noexcept;

			/**
			 * Returns the number of elements in this set
			 */
			virtual Size size() const noexcept;

			/**
			 * Returns the data length of the serialized BundleSet
			 */
			Length getLength() const noexcept;

			/**
			 * Get the bloom-filter of this bundle-set
			 */
			const ibrcommon::BloomFilter& getBloomFilter() const noexcept;

			/**
			 * Return bundles not in the given bloom-filter
			 */
			std::set<dtn::data::MetaBundle> getNotIn(const ibrcommon::BloomFilter &filter) const noexcept;

			/**
			 * Serialize the bloom-filter of this bundle-set into a stream
			 */
			virtual std::ostream &serialize(std::ostream &stream) const;

			/**
			 * Load the bloom-filter from a stream
			 */
            virtual std::istream &deserialize(std::istream &stream);

            /**
             * Sets the store path for MemoryBundleSets
             */
            static void setPath(const ibrcommon::File &path);

			/**
			 * Synchronize the bundle-set with the persistent set on the disk.
			 */
			virtual void sync() noexcept;

		private:
            /**
             * Store the bundle-set in a file
             */
            void store();

            /**
             * Restore the bundle-set from a file
             */
            void restore();

            // If this is a named bundle-set the name is stored here
            const std::string _name;

            // All meta-bundles are stored in this set of bundles
            typedef std::set<dtn::data::MetaBundle> bundle_set;
            bundle_set _bundles;

			// Bundles are sorted in this set to find expired bundles easily
            typedef std::set<BundleSetImpl::ExpiringBundle> expire_set;
            expire_set _expire;

			// The initial size of the bloom-filter
			Length _bf_size;

			// The bloom-filter with all the bundles
			ibrcommon::BloomFilter _bf;

			// The assigned listener
			BundleSet::Listener *_listener;

			// Mark the bloom-filter and set of bundles as consistent
			bool _consistent;

			static ibrcommon::File __store_path__;
			static bool __store_path_set__;
		};
	} /* namespace data */
} /* namespace dtn */
#endif /* MEMORYBUNDLESET_H_ */
