/*
 * SQLiteBundleSet.h
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
 *  Created on: 19.04.2013
 */

#ifndef SQLITEBUNDLESET_H_
#define SQLITEBUNDLESET_H_

#include "ibrdtn/data/BundleSetImpl.h"
#include "ibrdtn/data/BundleSet.h"
#include "storage/SQLiteDatabase.h"
#include "ibrdtn/data/MetaBundle.h"
#include <ibrcommon/data/BloomFilter.h>

namespace dtn
{
	namespace storage
	{
		class SQLiteBundleSet : public dtn::data::BundleSetImpl
		{
		public:
			/**
			 * @param bf_size Initial size fo the bloom-filter.
			 */
			SQLiteBundleSet(dtn::data::BundleSet::Listener *listener, dtn::data::Size bf_size, dtn::storage::SQLiteDatabase& database);
			SQLiteBundleSet(std::string name, dtn::data::BundleSet::Listener *listener, dtn::data::Size bf_size, dtn::storage::SQLiteDatabase& database);

			virtual ~SQLiteBundleSet();

			virtual void add(const dtn::data::MetaBundle &bundle) throw ();

			virtual void clear() throw ();

			virtual bool has(const dtn::data::BundleID &bundle) const throw ();

			virtual void expire(const dtn::data::Timestamp timestamp) throw ();

			/**
			 * Returns the number of elements in this set
			 */
			virtual dtn::data::Size size() const throw();

			/**
			 * Returns the data length of the serialized BundleSet
			 */
			dtn::data::Length getLength() const throw();

			const ibrcommon::BloomFilter& getBloomFilter() const throw();

			std::set<dtn::data::MetaBundle> getNotIn(ibrcommon::BloomFilter &filter) const throw();

			virtual std::ostream &serialize(std::ostream &stream) const;
			virtual std::istream &deserialize(std::istream &stream);

			virtual std::string getType();
			virtual bool isPersistent();
			virtual std::string getName();
		private:

			std::string _name;

			int _name_id;

			ibrcommon::BloomFilter _bf;

			dtn::data::BundleSet::Listener *_listener;

			bool _consistent;

			dtn::storage::SQLiteDatabase& _database;

			dtn::data::Timestamp _next_expiration;

			void new_expire_time(const dtn::data::Timestamp &ttl) throw();

			void rebuild_bloom_filter();

			bool _isPersistent;
		};
	} /* namespace data */
} /* namespace dtn */
#endif /* MEMORYBUNDLESET_H_ */
