/*
 * SQLiteDatabase.h
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
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
 */

#ifndef SQLITEDATABASE_H_
#define SQLITEDATABASE_H_

#include "core/BundleExpiredEvent.h"
#include "storage/BundleSeeker.h"
#include "storage/BundleSelector.h"
#include <ibrdtn/data/EID.h>
#include <ibrdtn/data/MetaBundle.h>
#include <ibrcommon/data/File.h>
#include <map>
#include <set>
#include <list>
#include <sqlite3.h>

namespace dtn
{
	namespace storage
	{
		class SQLiteDatabase : public BundleSeeker
		{
			enum SQL_TABLES
			{
				SQL_TABLE_BUNDLE = 0,
				SQL_TABLE_BLOCK = 1,
				SQL_TABLE_ROUTING = 2,
				SQL_TABLE_BUNDLE_ROUTING_INFO = 3,
				SQL_TABLE_NODE_ROUTING_INFO = 4,
				SQL_TABLE_PROPERTIES = 5,
				SQL_TABLE_SEEN_BUNDLES = 6,
				SQL_TABLE_BUNDLENAME = 7,
				SQL_TABLE_END = 8
			};

			// enum of all possible statements
			enum STORAGE_STMT
			{
				BUNDLE_GET_ITERATOR,
				BUNDLE_GET_FILTER,
				BUNDLE_GET_ID,
				GET_DISTINCT_DESTINATIONS,

				EXPIRE_BUNDLES,
				EXPIRE_BUNDLE_FILENAMES,
				EXPIRE_BUNDLE_DELETE,
				EXPIRE_NEXT_TIMESTAMP,

				EMPTY_CHECK,
				COUNT_ENTRIES,

				BUNDLE_DELETE,
				BUNDLE_CLEAR,
				BUNDLE_STORE,
				BUNDLE_UPDATE_CUSTODIAN,

				PROCFLAGS_SET,

				BLOCK_GET_ID,
				BLOCK_GET,
				BLOCK_CLEAR,
				BLOCK_STORE,

				SEEN_BUNDLE_ADD,
				SEEN_BUNDLE_CLEAR,
				SEEN_BUNDLE_GET,
				SEEN_BUNDLE_EXPIRE,
				SEEN_BUNDLE_GETALL,
				SEEN_BUNDLE_COUNT,
				SEEN_BUNDLE_EXPIRE_NEXT_TIMESTAMP,

				BUNDLENAME_ADD,
				BUNDLENAME_COUNT,
				BUNDLENAME_GET_NAME_ID,

				VACUUM,
				SQL_QUERIES_END
			};

			static const int DBSCHEMA_FRESH_VERSION;
			static const int DBSCHEMA_VERSION;
			static const std::string QUERY_SCHEMAVERSION;
			static const std::string SET_SCHEMAVERSION;

			static const std::string _select_names[2];

			static const std::string _tables[SQL_TABLE_END];

			// array of sql queries
			static const std::string _sql_queries[SQL_QUERIES_END];

			// array of the db structure as sql
			static const int DB_STRUCTURE_END = 14;
			static const std::string _db_structure[DB_STRUCTURE_END];

			static const std::string TAG;

		public:
			enum UPDATE_VALUES
			{
				UPDATE_CUSTODIAN
			};

			class DatabaseListener
			{
			public:
				virtual ~DatabaseListener() = 0;
				virtual void eventBundleExpired(const dtn::data::BundleID&) throw () = 0;
				virtual void iterateDatabase(const dtn::data::MetaBundle&) = 0;
			};

			class SQLBundleQuery
			{
			public:
				SQLBundleQuery();
				virtual ~SQLBundleQuery() = 0;

				/**
				 * returns the user-defined sql query
				 * @return
				 */
				virtual const std::string getWhere() const throw () = 0;

				/**
				 * bind all custom values to the statement
				 * @param st
				 * @param offset
				 * @return
				 */
				virtual int bind(sqlite3_stmt*, int offset) const throw ()
				{
					return offset;
				}
			};

			class SQLiteQueryException : public ibrcommon::Exception
			{
			public:
				SQLiteQueryException(string what = "Unable to execute Querry.") throw() : Exception(what)
				{
				}
			};

			class Statement
			{
			public:
				Statement(sqlite3 *database, const std::string&);
				~Statement();

				sqlite3_stmt* operator*();
				void prepare() throw (SQLiteQueryException);
				void reset() throw ();
				int step() throw (SQLiteQueryException);

			private:
				sqlite3 *_database;
				sqlite3_stmt *_st;
				const std::string _query;
			};

			typedef std::list<std::pair<int, const ibrcommon::File> > blocklist;
			typedef std::pair<int, const ibrcommon::File> blocklist_entry;

			SQLiteDatabase(const ibrcommon::File &file, DatabaseListener &listener);
			virtual ~SQLiteDatabase();

			/**
			 * open the database
			 */
			void open() throw (SQLiteQueryException);

			/**
			 * close the database
			 * @return
			 */
			void close();

			/**
			 * Expire all bundles with a lifetime lower than the given timestamp.
			 * @param timestamp
			 */
			void expire(const dtn::data::Timestamp &timestamp) throw ();

			/**
			 * Shrink down the database.
			 */
			void vacuum() throw (SQLiteQueryException);

			/**
			 * Update
			 * @param id
			 * @param
			 */
			void update(UPDATE_VALUES, const dtn::data::BundleID &id, const dtn::data::EID&) throw (SQLiteQueryException);

			/**
			 * Delete an entry in the database.
			 * @param id
			 */
			void remove(const dtn::data::BundleID &id) throw (SQLiteQueryException);

			/**
			 * @see BundleSeeker::get(BundleSelector &cb, BundleResult &result)
			 */
			virtual void get(const BundleSelector &cb, BundleResult &result) throw (NoBundleFoundException, BundleSelectorException);

			/**
			 * Retrieve the meta data of a given bundle
			 * @param id
			 * @return
			 */
			void get(const dtn::data::BundleID &id, dtn::data::MetaBundle &meta) const throw (SQLiteQueryException, NoBundleFoundException);

			/**
			 * Retrieve the data of a given bundle
			 * @param id
			 * @return
			 */
			void get(const dtn::data::BundleID &id, dtn::data::Bundle &bundle, blocklist &blocks) const throw (SQLiteQueryException, NoBundleFoundException);

			/**
			 *
			 * @param bundle
			 */
			void store(const dtn::data::Bundle &bundle) throw (SQLiteQueryException);
			void store(const dtn::data::BundleID &id, int index, const dtn::data::Block &block, const ibrcommon::File &file) throw (SQLiteQueryException);
			void transaction() throw (SQLiteQueryException);
			void rollback() throw (SQLiteQueryException);
			void commit() throw (SQLiteQueryException);

			bool empty() const throw (SQLiteQueryException);

			dtn::data::Size count() const throw (SQLiteQueryException);

			void clear() throw (SQLiteQueryException);

			/**
			 * @see BundleSeeker::getDistinctDestinations()
			 */
			virtual const eid_set getDistinctDestinations() throw (SQLiteQueryException);

			/**
			 * iterate through all the bundles and call the iterateDatabase() on each bundle
			 */
			void iterateAll() throw (SQLiteQueryException);

			/**
			 * add a named seen bundle to the database
			 * @param name name, bundle bundle
			 */
			void add_seen_bundle(int name_id, const dtn::data::MetaBundle &bundle) throw (SQLiteQueryException);

			/**
			 * returns true, if database contains the bundle, false if not
			 */
			bool contains_seen_bundle(const dtn::data::BundleID &id) const throw (SQLiteQueryException);

			/*
			 * removes all bundles from a named bundleset
			 */
			void clear_seen_bundles(int name_id) throw (SQLiteQueryException);

			/*
			 * removes specific bundle from database
			 */
			void erase_seen_bundle(const dtn::data::BundleID &id) throw (SQLiteQueryException);

			/*
			 * returns a set of all bundles contained in the database
			 */
			std::set<dtn::data::MetaBundle> get_all_seen_bundles() throw (SQLiteQueryException);

			/*
			 * returns number of seen bundles
			 */
			dtn::data::Size count_seen_bundles(int name_id) const throw (SQLiteQueryException);

			/*
			 * adds a used bundlename
			 * @return name_id
			 */
			int add_used_bundlename(std::string name) const throw (SQLiteQueryException);

			/*
			 * returns true, if a specific bundlename is used
			 */

			bool is_used_bundlename(std::string name) const throw (SQLiteQueryException);

			/*** BEGIN: methods for unit-testing ***/

			/**
			 * Set the storage to faulty. If set to true, each try to store
			 * a bundle will fail.
			 */
			void setFaulty(bool mode);

			/*** END: methods for unit-testing ***/

		private:
			/**
			 * Retrieve meta data from the database and put them into a meta bundle structure.
			 * @param st
			 * @param bundle
			 * @param offset
			 */
			void get(Statement &st, dtn::data::MetaBundle &bundle, int offset = 0) const throw (SQLiteQueryException);

			/**
			 * Retrieve meta data from the database and put them into a bundle structure.
			 * @param st
			 * @param bundle
			 * @param offset
			 */
			void get(Statement &st, dtn::data::Bundle &bundle, const int offset = 0) const throw (SQLiteQueryException);

			/**
			 *
			 * @param st
			 * @param ret
			 * @param bind_offset
			 * @param limit
			 */
			void __get(const BundleSelector &cb, Statement &st, BundleResult &ret, size_t &items_added, const int bind_offset, const size_t offset, const size_t query_limit) const throw (SQLiteQueryException, NoBundleFoundException, BundleSelectorException);

			/**
			 * updates the nextExpiredTime. The calling function has to have the databaselock.
			 */
			void update_expire_time() throw (SQLiteQueryException);

			/**
			 * lower the next expire time if the ttl is lower than the current expire time
			 * @param ttl
			 */
			void new_expire_time(const dtn::data::Timestamp &ttl) throw ();
			void reset_expire_time() throw ();
			const dtn::data::Timestamp& get_expire_time() const throw ();

			void set_bundleid(Statement &st, const dtn::data::BundleID &id, int offset = 0) const throw (SQLiteQueryException);
			void get_bundleid(Statement &st, dtn::data::BundleID &id, int offset = 0) const throw (SQLiteQueryException);

			/**
			 * get database version
			 */
			int getVersion() throw (SQLiteQueryException);

			/**
			 * set database version
			 * @param version
			 */
			void setVersion(int version) throw (SQLiteQueryException);

			/**
			 * upgrade the database to new version
			 * @param oldVersion Current version of the database.
			 * @param newVersion Required version.
			 */
			void doUpgrade(int oldVersion, int newVersion) throw (ibrcommon::Exception);

			ibrcommon::File _file;

			// holds the database handle
			sqlite3 *_database;

			// next expiration
			dtn::data::Timestamp _next_expiration;

			// listener for events on the database
			DatabaseListener &_listener;

			bool _faulty;
		};
	} /* namespace storage */
} /* namespace dtn */
#endif /* SQLITEDATABASE_H_ */
