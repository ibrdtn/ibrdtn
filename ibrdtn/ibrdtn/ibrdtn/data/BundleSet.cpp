/*
 * BundleSet.cpp
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
 *  Created on: 18.12.2012
 *  Recreated on: 04.04.2013  as interface
 */

#include "ibrdtn/data/BundleSet.h"
#include "ibrdtn/data/MemoryBundleSet.h"

namespace dtn
{
	namespace data
	{
		BundleSet::Factory* BundleSet::__factory__ = NULL;

		void BundleSet::setFactory(dtn::data::BundleSet::Factory *f)
		{
			if (__factory__ != NULL) delete __factory__;
			__factory__ = f;
		}

		BundleSet::Listener::~Listener()
		{
		}

		BundleSet::Factory::~Factory()
		{
		}

		BundleSetImpl* BundleSet::__create(Listener* listener, Size bf_size)
		{
			if (BundleSet::__factory__ != NULL) {
				BundleSetImpl *set = BundleSet::__factory__->create(listener,bf_size);
				if (set != NULL) return set;
			}

			// by default, return a memory bundle-set
			return new MemoryBundleSet(listener, bf_size);
		}

		BundleSetImpl* BundleSet::__create(const std::string &name, Listener* listener, Size bf_size)
		{
			if (BundleSet::__factory__ != NULL) {
				BundleSetImpl *set = BundleSet::__factory__->create(name, listener, bf_size);
				if (set != NULL) return set;
			}

			// by default, return a memory bundle-set
			return new MemoryBundleSet(name, listener, bf_size);
		}

		BundleSet::BundleSet(BundleSet::Listener *listener, Size bf_size)
		 : _set_impl(BundleSet::__create(listener, bf_size))
		{
		}

		BundleSet::BundleSet(const std::string &name, BundleSet::Listener *listener, Size bf_size)
		 : _set_impl(BundleSet::__create(name, listener, bf_size))
		{
		}

		BundleSet::BundleSet(const BundleSet &other)
		 : _set_impl(other._set_impl->copy())
		{
		}

		BundleSet::~BundleSet()
		{
		}

		BundleSet& BundleSet::operator=(const BundleSet &other)
		{
			_set_impl->assign(other._set_impl);
			return (*this);
		}

		void BundleSet::add(const dtn::data::MetaBundle &bundle) noexcept
		{
			_set_impl->add(bundle);
		}

		void BundleSet::clear() noexcept
		{
			_set_impl->clear();
		}

		bool BundleSet::has(const dtn::data::BundleID &bundle) const noexcept
		{
			return _set_impl->has(bundle);
		}

		Size BundleSet::size() const noexcept
		{
			return _set_impl->size();
		}

		void BundleSet::expire(const Timestamp timestamp) noexcept
		{
			_set_impl->expire(timestamp);
		}

		const ibrcommon::BloomFilter& BundleSet::getBloomFilter() const noexcept
		{
			return _set_impl->getBloomFilter();
		}

		std::set<dtn::data::MetaBundle> BundleSet::getNotIn(const ibrcommon::BloomFilter &filter) const noexcept
		{
			return _set_impl->getNotIn(filter);
		}

		void BundleSet::sync() noexcept
		{
			return _set_impl->sync();
		}

		Length BundleSet::getLength() const noexcept
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
	} /* namespace data */
} /* namespace dtn */
