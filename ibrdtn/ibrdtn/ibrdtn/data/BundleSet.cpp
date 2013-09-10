/*
 * BundleSet.cpp
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: David Goltzsche <goltzsch@ibr.cs.tu-bs.de>
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
 *  Recreated on: 04.04.2013  as interface
 */

#include "ibrdtn/data/BundleSet.h"
#include "ibrdtn/data/BundleSetFactory.h"

namespace dtn
{
	namespace data
	{
		BundleSet::Listener::~Listener()
		{ }

		BundleSet::BundleSet(BundleSet::Listener *listener, Size bf_size) : _set_impl(BundleSetFactory::create(listener,bf_size))
		{
		}
		BundleSet::BundleSet(std::string name,BundleSet::Listener *listener, Size bf_size)
			: _set_impl(BundleSetFactory::create(name,listener,bf_size))
		{
		}

		BundleSet::BundleSet(BundleSetImpl* ptr) : _set_impl(ptr)
		{
		}

		BundleSet::~BundleSet()
		{
		}

		void BundleSet::add(const dtn::data::MetaBundle &bundle) throw ()
		{
			_set_impl->add(bundle);
		}

		void BundleSet::clear() throw ()
		{
			_set_impl->clear();
		}

		bool BundleSet::has(const dtn::data::BundleID &bundle) const throw ()
		{
			return _set_impl->has(bundle);
		}

		Size BundleSet::size() const throw ()
		{
			return _set_impl->size();
		}

		void BundleSet::expire(const Timestamp timestamp) throw ()
		{
			_set_impl->expire(timestamp);
		}

		const ibrcommon::BloomFilter& BundleSet::getBloomFilter() const throw ()
		{
			return _set_impl->getBloomFilter();
		}

		std::set<dtn::data::MetaBundle> BundleSet::getNotIn(ibrcommon::BloomFilter &filter) const throw ()
		{
			return _set_impl->getNotIn(filter);
		}

		Length BundleSet::getLength() const throw ()
		{
			return _set_impl->getLength();
		}

		std::ostream& BundleSet::serialize(std::ostream &stream) const
		{
			return _set_impl->serialize(stream);
		}

		std::istream& BundleSet::deserialize(std::istream &stream)
		{
			return _set_impl->deserialize(stream);
		}

		std::ostream &operator<<(std::ostream &stream, const BundleSet &obj)
		{
			return obj.serialize(stream);
		}

		std::istream &operator>>(std::istream &stream, BundleSet &obj)
		{
			return obj.deserialize(stream);
		}

		std::string BundleSet::getType()
		{
			return _set_impl->getType();
		}

		bool BundleSet::isPersistent()
		{
			return _set_impl->isPersistent();
		}

		std::string BundleSet::getName()
		{
			return _set_impl->getName();
		}

	} /* namespace data */
} /* namespace dtn */
