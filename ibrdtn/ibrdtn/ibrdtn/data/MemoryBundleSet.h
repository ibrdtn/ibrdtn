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
#include <set>

namespace dtn
{
	namespace data
	{
		class MemoryBundleSet : public BundleSetImpl
		{
		public:
			/**
			 * @param bf_size Initial size fo the bloom-filter.
			 */
			MemoryBundleSet(BundleSet::Listener *listener = NULL, Length bf_size = 1024);
			virtual ~MemoryBundleSet();

			virtual void add(const dtn::data::MetaBundle &bundle) throw ();
			virtual void clear() throw ();
			virtual bool has(const dtn::data::BundleID &bundle) const throw ();

			virtual void expire(const Timestamp timestamp) throw ();

			/**
			 * Returns the number of elements in this set
			 */
			virtual Size size() const throw ();

			/**
			 * Returns the data length of the serialized BundleSet
			 */
			Length getLength() const throw ();

			const ibrcommon::BloomFilter& getBloomFilter() const throw ();

			std::set<dtn::data::MetaBundle> getNotIn(ibrcommon::BloomFilter &filter) const throw ();

			virtual std::ostream &serialize(std::ostream &stream) const;
            virtual std::istream &deserialize(std::istream &stream);

			virtual std::string getType();
			virtual bool isPersistent();
			virtual std::string getName();


		private:

			std::set<dtn::data::MetaBundle> _bundles;
			std::set<BundleSetImpl::ExpiringBundle> _expire;

			ibrcommon::BloomFilter _bf;

			BundleSet::Listener *_listener;

			bool _consistent;
		};
	} /* namespace data */
} /* namespace dtn */
#endif /* MEMORYBUNDLESET_H_ */
