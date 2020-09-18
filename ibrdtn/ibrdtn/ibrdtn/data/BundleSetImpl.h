/*
 * BundleSetImpl.h
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: David Goltzsche <goltzsch@ibr.cs.tu-bs.de>
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
 *  Created on: 04.04.2013
 */

#ifndef BUNDLESETIMPL_H_
#define BUNDLESETIMPL_H_

#include "ibrdtn/data/MetaBundle.h"
#include <ibrcommon/data/BloomFilter.h>
#include <set>

namespace dtn
{
	namespace data
	{
		class BundleSetImpl
		{
		public:
			/**
			 * @param bf_size Initial size fo the bloom-filter.
			 */
			virtual ~BundleSetImpl() = 0;

			/**
			 * copies the current bundle-set into a new temporary one
			 */
			virtual refcnt_ptr<BundleSetImpl> copy() const = 0;

			/**
			 * clears the bundle-set and copy all entries from the given
			 * one into this bundle-set
			 */
			virtual void assign(const refcnt_ptr<BundleSetImpl>&) = 0;

			virtual void add(const dtn::data::MetaBundle &bundle) noexcept = 0;
			virtual void clear() noexcept = 0;
			virtual bool has(const dtn::data::BundleID &bundle) const noexcept = 0;

			virtual void expire(const Timestamp timestamp) = 0;

			/**
			 * Returns the number of elements in this set
			 */
			virtual Size size() const noexcept = 0;

			/**
			 * Returns the data length of the serialized BundleSet
			 */
			virtual Length getLength() const noexcept = 0;

			virtual const ibrcommon::BloomFilter& getBloomFilter() const noexcept = 0;

			virtual std::set<dtn::data::MetaBundle> getNotIn(const ibrcommon::BloomFilter &filter) const noexcept = 0;

			/**
			 * Synchronize the bundle-set with the persistent set on the disk.
			 */
			virtual void sync() noexcept { };

			virtual std::ostream& serialize(std::ostream &stream) const = 0;
			virtual std::istream& deserialize(std::istream &stream) = 0;

		protected:
			class ExpiringBundle
			{
			public:
				ExpiringBundle(const MetaBundle &b);
				virtual ~ExpiringBundle();

				bool operator!=(const ExpiringBundle& other) const;
				bool operator==(const ExpiringBundle& other) const;
				bool operator<(const ExpiringBundle& other) const;
				bool operator>(const ExpiringBundle& other) const;

				const dtn::data::MetaBundle &bundle;
			};
		};
	} /* namespace data */
} /* namespace dtn */
#endif /* BUNDLESETIMPL_H_ */
