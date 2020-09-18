/*
 * BundleSet.h
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
 */

#ifndef BUNDLESET_H_
#define BUNDLESET_H_

#include "ibrdtn/data/BundleSetImpl.h"
#include "ibrdtn/data/MetaBundle.h"
#include <ibrcommon/data/BloomFilter.h>
#include <set>

namespace dtn
{
	namespace data
	{
		class BundleSet
		{
		public:
			class Listener {
			public:
				virtual ~Listener() = 0;
				virtual void eventBundleExpired(const dtn::data::MetaBundle&) = 0;
			};

			class Factory {
			public:
				virtual ~Factory() = 0;

				virtual BundleSetImpl* create(Listener* listener, Size bf_size) = 0;
				virtual BundleSetImpl* create(const std::string &name, Listener* listener, Size bf_size) = 0;
			};

			/**
			 * Creates a new bundle-set with the default bundle-set factory
			 * @param bf_size Initial size fo the bloom-filter.
			 */
			BundleSet(BundleSet::Listener *listener = NULL, Length bf_size = 1024);

			/**
			 * Creates a new bundle-set with the default bundle-set factory
			 * @param bf_size Initial size fo the bloom-filter.
			 */
			BundleSet(const std::string &name, BundleSet::Listener *listener = NULL, Length bf_size = 1024);

			/**
			 * Destructor
			 */
			virtual ~BundleSet();

			/**
			 * Copy constructor
			 */
			BundleSet(const BundleSet&);

			/**
			 * Generates a copy of the given bundle-setCopies a bundle-set
			 */
			BundleSet& operator=(const BundleSet&);

			/**
			 * Add a bundle to this bundle-set
			 */
			virtual void add(const dtn::data::MetaBundle &bundle) noexcept;

			/**
			 * Clear all entries
			 */
			virtual void clear() noexcept;

			/**
			 * Check if a specific bundle is in this bundle-set
			 */
			virtual bool has(const dtn::data::BundleID &bundle) const noexcept;

			/**
			 * Remove expired bundles in this bundle-set
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
			 * Return the bloom-filter object of this bundle-set
			 */
			const ibrcommon::BloomFilter& getBloomFilter() const noexcept;

			/**
			 * Return bundles not in the given bloom-filter
			 */
			std::set<dtn::data::MetaBundle> getNotIn(const ibrcommon::BloomFilter &filter) const noexcept;

			/**
			 * Synchronize the bundle-set with the persistent set on the disk.
			 */
			void sync() noexcept;

			std::ostream& serialize(std::ostream &stream) const;
			std::istream& deserialize(std::istream &stream);

			friend std::ostream &operator<<(std::ostream &stream, const BundleSet &obj);
			friend std::istream &operator>>(std::istream &stream, BundleSet &obj);

			/**
			 * assigns the current factory for bundle-sets
			 */
			static void setFactory(dtn::data::BundleSet::Factory*);

		private:
			static BundleSetImpl* __create(Listener* listener, Size bf_size);
			static BundleSetImpl* __create(const std::string &name, Listener* listener, Size bf_size);

			refcnt_ptr<BundleSetImpl> _set_impl;

			/**
			 * standard bundle-set factory
			 */
			static dtn::data::BundleSet::Factory *__factory__;
		};
	} /* namespace data */
} /* namespace dtn */
#endif /* BUNDLESET_H_ */
