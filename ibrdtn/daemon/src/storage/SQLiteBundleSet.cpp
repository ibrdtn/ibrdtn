/*
 * SQLiteBundleSet.cpp
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

#include "storage/SQLiteBundleSet.h"
#include <ibrdtn/utils/Random.h>
#include <ibrcommon/Logger.h>

namespace dtn
{
	namespace storage
	{
		ibrcommon::Mutex SQLiteBundleSet::Factory::_create_lock;

		SQLiteBundleSet::Factory::Factory(SQLiteDatabase& db)
		 : _sqldb(db)
		{
		}

		SQLiteBundleSet::Factory::~Factory()
		{
		}

		dtn::data::BundleSetImpl* SQLiteBundleSet::Factory::create(dtn::data::BundleSet::Listener* listener, dtn::data::Size bf_size)
		{
			try {
				return new SQLiteBundleSet(create(_sqldb), false, listener, bf_size, _sqldb);
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				return NULL;
			}
		}

		dtn::data::BundleSetImpl* SQLiteBundleSet::Factory::create(const std::string &name, dtn::data::BundleSet::Listener* listener, dtn::data::Size bf_size)
		{
			try {
				return new SQLiteBundleSet(create(_sqldb, name), true, listener, bf_size, _sqldb);
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				return NULL;
			}
		}

		size_t SQLiteBundleSet::Factory::create(SQLiteDatabase &db) noexcept (false)
		{
			ibrcommon::MutexLock l(_create_lock);

			std::string name;
			do {
				name = dtn::utils::Random::gen_chars(32);
			} while (__exists(db, name, false));

			return __create(db, name, false);
		}

		size_t SQLiteBundleSet::Factory::create(SQLiteDatabase &db, const std::string &name) noexcept (false)
		{
			ibrcommon::MutexLock l(_create_lock);
			return __create(db, name, true);
		}

		size_t SQLiteBundleSet::Factory::__create(SQLiteDatabase &db, const std::string &name, bool persistent) noexcept (false)
		{
			// create a new name (fails, if the name already exists)
			SQLiteDatabase::Statement st1(db._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_NAME_ADD]);
			sqlite3_bind_text(*st1, 1, name.c_str(), static_cast<int>(name.length()), SQLITE_TRANSIENT);
			sqlite3_bind_int(*st1, 2, persistent ? 1 : 0);
			st1.step();

			// get the ID of the name
			SQLiteDatabase::Statement st2(db._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_NAME_GET_ID]);
			sqlite3_bind_text(*st2, 1, name.c_str(), static_cast<int>(name.length()), SQLITE_TRANSIENT);
			sqlite3_bind_int(*st2, 2, persistent ? 1 : 0);

			if (st2.step() == SQLITE_ROW) {
				return sqlite3_column_int64(*st2, 0);
			}

			throw SQLiteDatabase::SQLiteQueryException("could not create the bundle-set name");
		}

		bool SQLiteBundleSet::Factory::__exists(SQLiteDatabase &db, const std::string &name, bool persistent) noexcept (false)
		{
			SQLiteDatabase::Statement st(db._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_NAME_GET_ID]);
			sqlite3_bind_text(*st, 1, name.c_str(), static_cast<int>(name.length()), SQLITE_TRANSIENT);
			sqlite3_bind_int(*st, 2, persistent ? 1 : 0);

			return ( st.step() == SQLITE_ROW);
		}

		SQLiteBundleSet::SQLiteBundleSet(const size_t id, bool persistant, dtn::data::BundleSet::Listener *listener, dtn::data::Size bf_size, dtn::storage::SQLiteDatabase& database)
		 : _set_id(id), _bf_size(bf_size), _bf(bf_size * 8), _listener(listener), _consistent(true),_sqldb(database), _persistent(persistant)
		{
			// if this is a persitant bundle-set
			if (_persistent) {
				// rebuild the bloom filter
				rebuild_bloom_filter();

				// load the next expiration from the storage
				try {
					SQLiteDatabase::Statement st(_sqldb._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_EXPIRE_NEXT_TIMESTAMP]);
					sqlite3_bind_int64(*st, 1, _set_id);

					int err = st.step();

					if (err == SQLITE_ROW)
					{
						_next_expiration = sqlite3_column_int64(*st, 0);
					}
				} catch (const SQLiteDatabase::SQLiteQueryException&) {
					// error
				}
			}
		}

		SQLiteBundleSet::~SQLiteBundleSet()
		{
			// clear on deletion if this set is not persistent
			if (!_persistent) destroy();
		}

		void SQLiteBundleSet::destroy()
		{
			clear();

			try {
				SQLiteDatabase::Statement st(_sqldb._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_NAME_REMOVE]);
				sqlite3_bind_int64(*st, 1, _set_id);

				st.step();
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				// error
			}
		}

		refcnt_ptr<dtn::data::BundleSetImpl> SQLiteBundleSet::copy() const
		{
			// create a new bundle-set
			SQLiteBundleSet *set = new SQLiteBundleSet(Factory::create(_sqldb), false, NULL, _bf_size, _sqldb);

			// copy Bloom-filter
			set->_bf_size = _bf_size;
			set->_bf = _bf;
			set->_consistent = _consistent;
			set->_next_expiration = _next_expiration;

			// copy all entries
			try {
				SQLiteDatabase::Statement st(_sqldb._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_COPY]);
				sqlite3_bind_int64(*st, 1, set->_set_id);	// destination
				sqlite3_bind_int64(*st, 2, _set_id);	// source

				st.step();
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				// error
			}

			return refcnt_ptr<dtn::data::BundleSetImpl>(set);
		}

		void SQLiteBundleSet::assign(const refcnt_ptr<BundleSetImpl> &other)
		{
			// clear all bundles first
			clear();

			// cast the given set to a MemoryBundleSet
			try {
				const SQLiteBundleSet &set = dynamic_cast<const SQLiteBundleSet&>(*other);

				// copy Bloom-filter
				_bf_size = set._bf_size;
				_bf = set._bf;
				_consistent = set._consistent;
				_next_expiration = set._next_expiration;

				// copy all entries
				try {
					SQLiteDatabase::Statement st(_sqldb._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_COPY]);
					sqlite3_bind_int64(*st, 1, _set_id);	// destination
					sqlite3_bind_int64(*st, 2, set._set_id);	// source

					st.step();
				} catch (const SQLiteDatabase::SQLiteQueryException&) {
					// error
				}
			} catch (const std::bad_cast&) {
				// incompatible bundle-set implementation - abort here
			}
		}

		void SQLiteBundleSet::add(const dtn::data::MetaBundle &bundle) noexcept
		{
			try {
				// insert bundle id into database
				SQLiteDatabase::Statement st(_sqldb._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_ADD]);

				sqlite3_bind_int64(*st, 1, _set_id);
				sqlite3_bind_text(*st, 2, bundle.source.getString().c_str(), static_cast<int>(bundle.source.getString().length()), SQLITE_TRANSIENT);
				sqlite3_bind_int64(*st, 3, bundle.timestamp.get<uint64_t>());
				sqlite3_bind_int64(*st, 4, bundle.sequencenumber.get<uint64_t>());

				if (bundle.isFragment()) {
					sqlite3_bind_int64(*st, 5, bundle.fragmentoffset.get<uint64_t>());
					sqlite3_bind_int64(*st, 6, bundle.getPayloadLength());
				} else {
					sqlite3_bind_int64(*st, 5, -1);
					sqlite3_bind_int64(*st, 6, -1);
				}

				sqlite3_bind_int64(*st, 7, bundle.expiretime.get<uint64_t>());

				st.step();

				// update expiretime, if necessary
				new_expire_time(bundle.expiretime);

				// add bundle to the bloomfilter
				bundle.addTo(_bf);
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				// error
			}
		}

		void SQLiteBundleSet::clear() noexcept
		{
			try {
				SQLiteDatabase::Statement st(_sqldb._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_CLEAR]);
				sqlite3_bind_int64(*st, 1, _set_id);

				st.step();
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				// error
			}

			// clear the bloom-filter
			_bf.clear();
		}

		bool SQLiteBundleSet::has(const dtn::data::BundleID &id) const noexcept
		{
			// check bloom-filter first
			if (!id.isIn(_bf)) return false;

			// Return true if the bloom-filter is not consistent with
			// the bundles set. This happen if the MemoryBundleSet gets deserialized.
			if (!_consistent) return true;

			try {
				SQLiteDatabase::Statement st( const_cast<sqlite3*>(_sqldb._database), SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_GET]);

				sqlite3_bind_int64(*st, 1, _set_id);
				sqlite3_bind_text(*st, 2, id.source.getString().c_str(), static_cast<int>(id.source.getString().length()), SQLITE_TRANSIENT);
				sqlite3_bind_int64(*st, 3, id.timestamp.get<uint64_t>());
				sqlite3_bind_int64(*st, 4, id.sequencenumber.get<uint64_t>());

				if (id.isFragment()) {
					sqlite3_bind_int64(*st, 5, id.fragmentoffset.get<uint64_t>());
					sqlite3_bind_int64(*st, 6, id.getPayloadLength());
				} else {
					sqlite3_bind_int64(*st, 5, -1);
					sqlite3_bind_int64(*st, 6, -1);
				}

				if (st.step() == SQLITE_ROW)
					return true;
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				// error
			}

			return false;
		}

		void SQLiteBundleSet::expire(const dtn::data::Timestamp timestamp) noexcept
		{
			// we can not expire bundles if we have no idea of time
			if (timestamp == 0) return;

			// do not expire if its not the time
			if (_next_expiration > timestamp) return;

			// look for expired bundles and announce them in the listener
			if (_listener != NULL) {
				try {
					SQLiteDatabase::Statement st(_sqldb._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_GET_EXPIRED]);
					sqlite3_bind_int64(*st, 1, _set_id);

					// set expiration timestamp
					sqlite3_bind_int64(*st, 2, timestamp.get<uint64_t>());

					while (st.step() == SQLITE_ROW)
					{
						dtn::data::BundleID id;
						get_bundleid(st, id);

						const dtn::data::MetaBundle bundle = dtn::data::MetaBundle::create(id);

						// raise bundle expired event
						_listener->eventBundleExpired(bundle);
					}
				} catch (const SQLiteDatabase::SQLiteQueryException &ex) {
					IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}
			}

			// delete expired bundles
			try {
				SQLiteDatabase::Statement st(_sqldb._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_EXPIRE]);
				sqlite3_bind_int64(*st, 1, _set_id);

				// set expiration timestamp
				sqlite3_bind_int64(*st, 2, timestamp.get<uint64_t>());

				st.step();
			} catch (const SQLiteDatabase::SQLiteQueryException &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}

			// rebuild the bloom filter
			rebuild_bloom_filter();
		}

		dtn::data::Size SQLiteBundleSet::size() const noexcept
		{
			int rows = 0;

			try {
				SQLiteDatabase::Statement st(_sqldb._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_COUNT]);
				sqlite3_bind_int64(*st, 1, _set_id);

				if (st.step() == SQLITE_ROW)
				{
					rows = sqlite3_column_int(*st, 0);
				}
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				// error
			}

			return rows;
		}

		dtn::data::Length SQLiteBundleSet::getLength() const noexcept
		{
			return dtn::data::Number(_bf.size()).getLength() + _bf.size();
		}

		const ibrcommon::BloomFilter& SQLiteBundleSet::getBloomFilter() const noexcept
		{
			return _bf;
		}

		std::set<dtn::data::MetaBundle> SQLiteBundleSet::getNotIn(const ibrcommon::BloomFilter &filter) const noexcept
		{
			std::set<dtn::data::MetaBundle> ret;

			try {
				SQLiteDatabase::Statement st(_sqldb._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_GET_ALL]);
				sqlite3_bind_int64(*st, 1, _set_id);

				std::set<dtn::data::MetaBundle> ret;
				dtn::data::BundleID id;

				while (st.step() == SQLITE_ROW)
				{
					// get the bundle id
					get_bundleid(st, id);

					if ( ! id.isIn(filter) )
					{
						const dtn::data::MetaBundle bundle = dtn::data::MetaBundle::create(id);
						ret.insert( bundle );
					}
				}
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				// error
			}

			return ret;
		}

		std::ostream& SQLiteBundleSet::serialize(std::ostream &stream) const
		{
			dtn::data::Number size(_bf.size());
			stream << size;

			const char *data = reinterpret_cast<const char*>(_bf.table());
			stream.write(data, _bf.size());

			return stream;
		}

		std::istream& SQLiteBundleSet::deserialize(std::istream &stream)
		{
			dtn::data::Number count;
			stream >> count;

			std::vector<char> buffer(count.get<size_t>());

			stream.read(&buffer[0], buffer.size());

			SQLiteBundleSet::clear();
			_bf.load((unsigned char*)&buffer[0], buffer.size());

			// set the set to in-consistent mode
			_consistent = false;

			return stream;
		}

		void SQLiteBundleSet::new_expire_time(const dtn::data::Timestamp &ttl) noexcept
		{
			if (_next_expiration == 0 || ttl < _next_expiration)
			{
				_next_expiration = ttl;
			}
		}

		void SQLiteBundleSet::rebuild_bloom_filter()
		{
			// rebuild the bloom-filter
			_bf.clear();

			try {
				SQLiteDatabase::Statement st(_sqldb._database, SQLiteDatabase::_sql_queries[SQLiteDatabase::BUNDLE_SET_GET_ALL]);
				sqlite3_bind_int64(*st, 1, _set_id);

				std::set<dtn::data::MetaBundle> ret;
				dtn::data::BundleID id;

				while (st.step() == SQLITE_ROW)
				{
					// get the bundle id
					get_bundleid(st, id);
					id.addTo(_bf);
				}

				_consistent = true;
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				// error
			}
		}

		void SQLiteBundleSet::get_bundleid(SQLiteDatabase::Statement &st, dtn::data::BundleID &id, int offset) const noexcept (false)
		{
			id.source = dtn::data::EID((const char*)sqlite3_column_text(*st, offset + 0));
			id.timestamp = sqlite3_column_int64(*st, offset + 1);
			id.sequencenumber = sqlite3_column_int64(*st, offset + 2);
			dtn::data::Number fragmentoffset = 0;
			id.setFragment(sqlite3_column_int64(*st, offset + 2) >= 0);

			if (id.isFragment()) {
				id.fragmentoffset = sqlite3_column_int64(*st, offset + 3);
				id.setPayloadLength(sqlite3_column_int64(*st, offset + 4));
			} else {
				id.fragmentoffset = 0;
				id.setPayloadLength(0);
			}
		}
	} /* namespace data */
} /* namespace dtn */
