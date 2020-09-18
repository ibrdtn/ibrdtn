/*
 * SQLiteBundleSet.h
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
 *  Created on: 19.04.2013
 */

#ifndef SQLITEBUNDLESET_H_
#define SQLITEBUNDLESET_H_

#include "ibrdtn/data/BundleSetImpl.h"
#include "ibrdtn/data/BundleSet.h"
#include "storage/SQLiteDatabase.h"
#include "ibrdtn/data/MetaBundle.h"
#include <ibrcommon/data/BloomFilter.h>
#include <ibrcommon/thread/Mutex.h>

namespace dtn
{
	namespace storage
	{
		class SQLiteBundleSet : public dtn::data::BundleSetImpl
		{
		public:
			class Factory : public dtn::data::BundleSet::Factory {
				public:
					Factory(SQLiteDatabase& db);
					~Factory();

					dtn::data::BundleSetImpl* create(dtn::data::BundleSet::Listener* listener, dtn::data::Size bf_size);
					dtn::data::BundleSetImpl* create(const std::string &name, dtn::data::BundleSet::Listener* listener, dtn::data::Size bf_size);

					/**
					 * creates an anonymous bundle-set and returns its ID
					 */
					static size_t create(SQLiteDatabase &db) noexcept (false);

					/**
					 * creates a named bundle-set or returns the ID of an existing bundle-set
					 */
					static size_t create(SQLiteDatabase &db, const std::string &name) noexcept (false);
					static size_t __create(SQLiteDatabase &db, const std::string &name, bool persistent) noexcept (false);

					/*
					 * returns true, if a specific bundle-set name exists
					 */
					static bool __exists(SQLiteDatabase &db, const std::string &name, bool persistent) noexcept (false);

				private:
					static ibrcommon::Mutex _create_lock;

					SQLiteDatabase& _sqldb;
			};

			/**
			 * @param bf_size Initial size fo the bloom-filter.
			 */
			SQLiteBundleSet(const size_t id, bool persistant, dtn::data::BundleSet::Listener *listener, dtn::data::Size bf_size, dtn::storage::SQLiteDatabase& database);

			virtual ~SQLiteBundleSet();

			/**
			 * copies the current bundle-set into a new temporary one
			 */
			virtual refcnt_ptr<BundleSetImpl> copy() const;

			/**
			 * clears the bundle-set and copy all entries from the given
			 * one into this bundle-set
			 */
			virtual void assign(const refcnt_ptr<BundleSetImpl>&);

			virtual void add(const dtn::data::MetaBundle &bundle) noexcept;

			virtual void clear() noexcept;

			virtual bool has(const dtn::data::BundleID &bundle) const noexcept;

			virtual void expire(const dtn::data::Timestamp timestamp) noexcept;

			/**
			 * Returns the number of elements in this set
			 */
			virtual dtn::data::Size size() const noexcept;

			/**
			 * Returns the data length of the serialized BundleSet
			 */
			dtn::data::Length getLength() const noexcept;

			const ibrcommon::BloomFilter& getBloomFilter() const noexcept;

			std::set<dtn::data::MetaBundle> getNotIn(const ibrcommon::BloomFilter &filter) const noexcept;

			virtual std::ostream &serialize(std::ostream &stream) const;
			virtual std::istream &deserialize(std::istream &stream);

		private:
			void get_bundleid(SQLiteDatabase::Statement &st, dtn::data::BundleID &id, int offset = 0) const noexcept (false);

			// remove this bundle-set from the database
			void destroy();

			const size_t _set_id;

			// The initial size of the bloom-filter
			dtn::data::Length _bf_size;

			// The bloom-filter with all the bundles
			ibrcommon::BloomFilter _bf;

			// The assigned listener
			dtn::data::BundleSet::Listener *_listener;

			// Mark the bloom-filter and set of bundles as consistent
			bool _consistent;

			dtn::storage::SQLiteDatabase& _sqldb;

			dtn::data::Timestamp _next_expiration;

			void new_expire_time(const dtn::data::Timestamp &ttl) noexcept;

			void rebuild_bloom_filter();

			const bool _persistent;
		};
	} /* namespace data */
} /* namespace dtn */
#endif /* MEMORYBUNDLESET_H_ */
