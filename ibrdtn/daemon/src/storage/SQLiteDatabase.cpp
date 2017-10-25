/*
 * SQLiteDatabase.cpp
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

#include "storage/SQLiteDatabase.h"
#include "storage/SQLiteConfigure.h"
#include <ibrdtn/data/ScopeControlHopLimitBlock.h>
#include <ibrdtn/data/SchedulingBlock.h>
#include <ibrdtn/utils/Clock.h>
#include <ibrcommon/Logger.h>
#include <stdint.h>

namespace dtn
{
	namespace storage
	{
		void sql_tracer(void*, const char * pQuery)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("SQLiteDatabase", 50) << "sqlite trace: " << pQuery << IBRCOMMON_LOGGER_ENDL;
		}

		const std::string SQLiteDatabase::TAG = "SQLiteDatabase";

		const std::string SQLiteDatabase::_select_names[] = {
				"source, destination, reportto, custodian, procflags, timestamp, sequencenumber, lifetime, expiretime, fragmentoffset, appdatalength, hopcount, netpriority, payloadlength, bytes",
				"source, timestamp, sequencenumber, fragmentoffset, payloadlength, bytes",
				"`source`, `timestamp`, `sequencenumber`, `fragmentoffset`, `fragmentlength`, `expiretime`"
		};

		const std::string SQLiteDatabase::_where_filter[] = {
				"source = ? AND timestamp = ? AND sequencenumber = ? AND fragmentoffset = ? AND fragmentlength = ?",
				"a.source = b.source AND a.timestamp = b.timestamp AND a.sequencenumber = b.sequencenumber AND a.fragmentoffset = b.fragmentoffset AND a.fragmentlength = b.fragmentlength"
		};

		const std::string SQLiteDatabase::_tables[] =
				{ "bundles", "blocks", "routing", "routing_bundles", "routing_nodes", "properties", "bundle_set", "bundle_set_names" };

		// this is the version of a fresh created db scheme
		const int SQLiteDatabase::DBSCHEMA_FRESH_VERSION = 8;

		const int SQLiteDatabase::DBSCHEMA_VERSION = 8;

		const std::string SQLiteDatabase::QUERY_SCHEMAVERSION = "SELECT `value` FROM " + SQLiteDatabase::_tables[SQLiteDatabase::SQL_TABLE_PROPERTIES] + " WHERE `key` = 'version' LIMIT 0,1;";
		const std::string SQLiteDatabase::SET_SCHEMAVERSION = "INSERT INTO " + SQLiteDatabase::_tables[SQLiteDatabase::SQL_TABLE_PROPERTIES] + " (`key`, `value`) VALUES ('version', ?);";

		const std::string SQLiteDatabase::_sql_queries[SQL_QUERIES_END] =
		{
			"SELECT " + _select_names[0] + " FROM " + _tables[SQL_TABLE_BUNDLE],
			"SELECT " + _select_names[0] + " FROM " + _tables[SQL_TABLE_BUNDLE] + " ORDER BY priority DESC, timestamp, sequencenumber, fragmentoffset, fragmentlength LIMIT ?,?;",
			"SELECT " + _select_names[0] + " FROM "+ _tables[SQL_TABLE_BUNDLE] +" WHERE " + _where_filter[0] + " LIMIT 1;",
			"SELECT bytes FROM "+ _tables[SQL_TABLE_BUNDLE] +" WHERE " + _where_filter[0] + " LIMIT 1;",
			"SELECT DISTINCT destination FROM " + _tables[SQL_TABLE_BUNDLE],

			//EXPIRE_*
			"SELECT " + _select_names[1] + " FROM "+ _tables[SQL_TABLE_BUNDLE] +" WHERE expiretime <= ?;",
			"SELECT filename FROM "+ _tables[SQL_TABLE_BUNDLE] +" as a, "+ _tables[SQL_TABLE_BLOCK] +" as b WHERE " + _where_filter[1] + " AND a.expiretime <= ?;",
			"DELETE FROM "+ _tables[SQL_TABLE_BUNDLE] +" WHERE expiretime <= ?;",
			"SELECT expiretime FROM "+ _tables[SQL_TABLE_BUNDLE] +" ORDER BY expiretime ASC LIMIT 1;",

			"SELECT ROWID FROM "+ _tables[SQL_TABLE_BUNDLE] +" LIMIT 1;",
			"SELECT COUNT(ROWID) FROM "+ _tables[SQL_TABLE_BUNDLE] +";",

			"DELETE FROM "+ _tables[SQL_TABLE_BUNDLE] +" WHERE " + _where_filter[0] + ";",
			"DELETE FROM "+ _tables[SQL_TABLE_BUNDLE] +";",
			"INSERT INTO "+ _tables[SQL_TABLE_BUNDLE] +" (source, timestamp, sequencenumber, fragmentoffset, fragmentlength, destination, reportto, custodian, procflags, lifetime, appdatalength, expiretime, priority, hopcount, netpriority, payloadlength, bytes) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
			"UPDATE "+ _tables[SQL_TABLE_BUNDLE] +" SET custodian = ? WHERE " + _where_filter[0] + ";",

			"UPDATE "+ _tables[SQL_TABLE_BUNDLE] +" SET procflags = ? WHERE " + _where_filter[0] + ";",

			//BLOCK_*
			"SELECT filename, blocktype FROM "+ _tables[SQL_TABLE_BLOCK] +" WHERE " + _where_filter[0] + " ORDER BY ordernumber ASC;",
			"SELECT filename, blocktype FROM "+ _tables[SQL_TABLE_BLOCK] +" WHERE " + _where_filter[0] + " AND ordernumber = ?;",
			"DELETE FROM "+ _tables[SQL_TABLE_BLOCK] +";",
			"INSERT INTO "+ _tables[SQL_TABLE_BLOCK] +" (source, timestamp, sequencenumber, fragmentoffset, fragmentlength, blocktype, filename, ordernumber) VALUES (?,?,?,?,?,?,?,?);",

			//BUNDLE_SET_*
			"INSERT INTO " + _tables[SQL_TABLE_BUNDLE_SET] + " (set_id, source, timestamp, sequencenumber, fragmentoffset, fragmentlength, expiretime) VALUES (?,?,?,?,?,?,?);",
			"DELETE FROM " + _tables[SQL_TABLE_BUNDLE_SET] + " WHERE set_id = ?;",
			"SELECT " + _select_names[2] + " FROM " + _tables[SQL_TABLE_BUNDLE_SET] + " WHERE set_id = ? AND " + _where_filter[0] + " LIMIT 1;",
			"SELECT " + _select_names[2] + " FROM " + _tables[SQL_TABLE_BUNDLE_SET] + " WHERE set_id = ? AND expiretime <= ?;",
			"DELETE FROM " + _tables[SQL_TABLE_BUNDLE_SET] + " WHERE set_id = ? AND expiretime <= ?;",
			"SELECT " + _select_names[2] + " FROM " + _tables[SQL_TABLE_BUNDLE_SET] + " WHERE set_id = ?;",
			"SELECT COUNT(*) FROM " + _tables[SQL_TABLE_BUNDLE_SET] + " WHERE set_id = ?;",
			"SELECT expiretime FROM "+ _tables[SQL_TABLE_BUNDLE_SET] +" WHERE set_id = ? ORDER BY expiretime ASC LIMIT 1;",
			"INSERT INTO " + _tables[SQL_TABLE_BUNDLE_SET] + " SELECT " + _select_names[2] + ", ? FROM " + _tables[SQL_TABLE_BUNDLE_SET] + " WHERE set_id = ?;",

			//BUNDLE_SET_NAME_*
			"INSERT INTO " + _tables[SQL_TABLE_BUNDLE_SET_NAME] + " (name, persistent) VALUES (?, ?);",
			"SELECT id FROM " + _tables[SQL_TABLE_BUNDLE_SET_NAME] + " WHERE name = ? AND persistent = ? LIMIT 0, 1;",
			"DELETE FROM " + _tables[SQL_TABLE_BUNDLE_SET_NAME] + " WHERE id = ? LIMIT 0, 1;",

			"VACUUM;"
		};

		const std::string SQLiteDatabase::_db_structure[SQLiteDatabase::DB_STRUCTURE_END] =
		{
			"CREATE TABLE IF NOT EXISTS `" + _tables[SQL_TABLE_BLOCK] + "` ( `key` INTEGER PRIMARY KEY ASC, `source` TEXT NOT NULL, `timestamp` INTEGER NOT NULL, `sequencenumber` INTEGER NOT NULL, `fragmentoffset` INTEGER NOT NULL DEFAULT 0, `fragmentlength` INTEGER NOT NULL DEFAULT 0, `blocktype` INTEGER NOT NULL, `filename` TEXT NOT NULL, `ordernumber` INTEGER NOT NULL);",
			"CREATE TABLE IF NOT EXISTS `" + _tables[SQL_TABLE_BUNDLE] + "` ( `key` INTEGER PRIMARY KEY ASC, `source` TEXT NOT NULL, `destination` TEXT NOT NULL, `reportto` TEXT NOT NULL, `custodian` TEXT NOT NULL, `procflags` INTEGER NOT NULL, `timestamp` INTEGER NOT NULL, `sequencenumber` INTEGER NOT NULL, `lifetime` INTEGER NOT NULL, `fragmentoffset` INTEGER NOT NULL DEFAULT 0, `appdatalength` INTEGER NOT NULL DEFAULT 0, `fragmentlength` INTEGER NOT NULL DEFAULT 0, `expiretime` INTEGER NOT NULL, `priority` INTEGER NOT NULL, `hopcount` INTEGER DEFAULT NULL, `netpriority` INTEGER NOT NULL DEFAULT 0, `payloadlength` INTEGER NOT NULL DEFAULT 0, `bytes` INTEGER NOT NULL DEFAULT 0);",
			"CREATE TABLE IF NOT EXISTS "+ _tables[SQL_TABLE_ROUTING] +" (INTEGER PRIMARY KEY ASC, KEY INT, Routing TEXT);",
			"CREATE TABLE IF NOT EXISTS "+ _tables[SQL_TABLE_BUNDLE_ROUTING_INFO] +" (INTEGER PRIMARY KEY ASC, BundleID TEXT, KEY INT, Routing TEXT);",
			"CREATE TABLE IF NOT EXISTS "+ _tables[SQL_TABLE_NODE_ROUTING_INFO] +" (INTEGER PRIMARY KEY ASC, EID text, KEY INT, Routing TEXT);",
			"CREATE TRIGGER IF NOT EXISTS blocks_autodelete AFTER DELETE ON " + _tables[SQL_TABLE_BUNDLE] + " FOR EACH ROW BEGIN DELETE FROM " + _tables[SQL_TABLE_BLOCK] + " WHERE " + _tables[SQL_TABLE_BLOCK] + ".source = OLD.source AND " + _tables[SQL_TABLE_BLOCK] + ".timestamp = OLD.timestamp AND " + _tables[SQL_TABLE_BLOCK] + ".sequencenumber = OLD.sequencenumber AND " + _tables[SQL_TABLE_BLOCK] + ".fragmentoffset = OLD.fragmentoffset AND " + _tables[SQL_TABLE_BLOCK] + ".fragmentlength = OLD.fragmentlength; END;",
			"CREATE INDEX IF NOT EXISTS blocks_bid ON " + _tables[SQL_TABLE_BLOCK] + " (source, timestamp, sequencenumber, fragmentoffset, fragmentlength);",
			"CREATE INDEX IF NOT EXISTS bundles_destination ON " + _tables[SQL_TABLE_BUNDLE] + " (destination);",
			"CREATE INDEX IF NOT EXISTS bundles_destination_priority ON " + _tables[SQL_TABLE_BUNDLE] + " (destination, priority);",
			"CREATE UNIQUE INDEX IF NOT EXISTS bundles_id ON " + _tables[SQL_TABLE_BUNDLE] + " (source, timestamp, sequencenumber, fragmentoffset, fragmentlength);"
			"CREATE INDEX IF NOT EXISTS bundles_expire ON " + _tables[SQL_TABLE_BUNDLE] + " (source, timestamp, sequencenumber, fragmentoffset, fragmentlength, expiretime);",
			"CREATE TABLE IF NOT EXISTS '" + _tables[SQL_TABLE_PROPERTIES] + "' ( `key` TEXT PRIMARY KEY ASC ON CONFLICT REPLACE, `value` TEXT NOT NULL);",
			"CREATE TABLE IF NOT EXISTS " + _tables[SQL_TABLE_BUNDLE_SET] + " (`source` TEXT NOT NULL, `timestamp` INTEGER NOT NULL, `sequencenumber` INTEGER NOT NULL, `fragmentoffset` INTEGER NOT NULL, `fragmentlength` INTEGER NOT NULL, `expiretime` INTEGER, `set_id` INTEGER, PRIMARY KEY(`set_id`, `source`, `timestamp`, `sequencenumber`, `fragmentoffset`, `fragmentlength`));",
			"CREATE TABLE IF NOT EXISTS " + _tables[SQL_TABLE_BUNDLE_SET_NAME] + " (`id` INTEGER PRIMARY KEY, `name` TEXT NOT NULL, `persistent` INTEGER NOT NULL);",
			"CREATE UNIQUE INDEX IF NOT EXISTS bundle_set_names_index ON " + _tables[SQL_TABLE_BUNDLE_SET_NAME] + " (`name`, `persistent`);"
		};

		SQLiteDatabase::SQLBundleQuery::SQLBundleQuery()
		{ }

		SQLiteDatabase::SQLBundleQuery::~SQLBundleQuery()
		{
		}

		SQLiteDatabase::Statement::Statement(sqlite3 *database, const std::string &query)
		 : _database(database), _st(NULL), _query(query)
		{
			prepare();
		}

		SQLiteDatabase::Statement::~Statement()
		{
			if (_st != NULL) {
				sqlite3_finalize(_st);
			}
		}

		sqlite3_stmt* SQLiteDatabase::Statement::operator*()
		{
			return _st;
		}

		void SQLiteDatabase::Statement::reset() throw ()
		{
			if (_st != NULL) {
				sqlite3_reset(_st);
				sqlite3_clear_bindings(_st);
			}
		}

		int SQLiteDatabase::Statement::step() throw (SQLiteDatabase::SQLiteQueryException)
		{
			if (_st == NULL)
				throw SQLiteQueryException("statement not prepared");

			int ret = sqlite3_step(_st);

			// check if the return value signals an error
			switch (ret)
			{
			case SQLITE_CORRUPT:
				throw SQLiteQueryException("Database is corrupt: " + std::string(sqlite3_errmsg(_database)));

			case SQLITE_INTERRUPT:
				throw SQLiteQueryException("Database interrupt: " + std::string(sqlite3_errmsg(_database)));

			case SQLITE_SCHEMA:
				throw SQLiteQueryException("Database schema error: " + std::string(sqlite3_errmsg(_database)));

			case SQLITE_ERROR:
				throw SQLiteQueryException("Database error: " + std::string(sqlite3_errmsg(_database)));

			default:
				return ret;
			}
		}

		void SQLiteDatabase::Statement::prepare() throw (SQLiteDatabase::SQLiteQueryException)
		{
			if (_st != NULL)
				throw SQLiteQueryException("already prepared");

			int err = sqlite3_prepare_v2(_database, _query.c_str(), static_cast<int>(_query.length()), &_st, 0);

			if ( err != SQLITE_OK )
				throw SQLiteQueryException("failed to prepare statement: " + _query);
		}

		SQLiteDatabase::DatabaseListener::~DatabaseListener() {}

		SQLiteDatabase::SQLiteDatabase(const ibrcommon::File &file, DatabaseListener &listener)
		 : _file(file), _database(NULL), _next_expiration(0), _listener(listener), _faulty(false)
		{
		}

		SQLiteDatabase::~SQLiteDatabase()
		{
		}

		int SQLiteDatabase::getVersion() throw (SQLiteDatabase::SQLiteQueryException)
		{
			// Check version of SQLiteDB
			int version = 0;

			// prepare the statement
			Statement st(_database, QUERY_SCHEMAVERSION);

			// execute statement for version query
			int err = st.step();

			// Query finished no table found
			if (err == SQLITE_ROW)
			{
				std::string dbversion = (const char*) sqlite3_column_text(*st, 0);
				std::stringstream ss(dbversion);
				ss >> version;
			}

			return version;
		}

		void SQLiteDatabase::setVersion(int version) throw (SQLiteDatabase::SQLiteQueryException)
		{
			std::stringstream ss; ss << version;
			Statement st(_database, SET_SCHEMAVERSION);

			// bind version text to the statement
			sqlite3_bind_text(*st, 1, ss.str().c_str(), static_cast<int>(ss.str().length()), SQLITE_TRANSIENT);

			int err = st.step();
			if(err != SQLITE_DONE)
			{
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << "failed to set version " << err << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SQLiteDatabase::doUpgrade(int oldVersion, int newVersion) throw (ibrcommon::Exception)
		{
			if (oldVersion > newVersion)
			{
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << "Downgrade from version " << oldVersion << " to version " << newVersion << " is not possible." << IBRCOMMON_LOGGER_ENDL;
				throw ibrcommon::Exception("Downgrade not possible.");
			}

			if ((oldVersion != 0) && (oldVersion < DBSCHEMA_FRESH_VERSION))
			{
				throw ibrcommon::Exception("Re-creation required.");
			}

			IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, info) << "Upgrade from version " << oldVersion << " to version " << newVersion << IBRCOMMON_LOGGER_ENDL;

			for (int j = oldVersion; j < newVersion; ++j)
			{
				switch (j)
				{
				// if there is no version field, drop all tables
				case 0:
					for (size_t i = 0; i < SQL_TABLE_END; ++i)
					{
						Statement st(_database, "DROP TABLE IF EXISTS " + _tables[i] + ";");
						int err = st.step();
						if(err != SQLITE_DONE)
						{
							IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << "drop table " << _tables[i] << " failed " << err << IBRCOMMON_LOGGER_ENDL;
						}
					}

					// create all tables
					for (size_t i = 0; i < (DB_STRUCTURE_END - 1); ++i)
					{
						Statement st(_database, _db_structure[i]);
						int err = st.step();
						if(err != SQLITE_DONE)
						{
							IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << "failed to create table " << _tables[i] << "; err: " << err << IBRCOMMON_LOGGER_ENDL;
						}
					}

					// set new database version
					setVersion(DBSCHEMA_FRESH_VERSION);
					j = DBSCHEMA_FRESH_VERSION;
					break;

				default:
					// NO UPGRADE PATH HERE
					if (DBSCHEMA_FRESH_VERSION > j)
						throw ibrcommon::Exception("Re-creation required.");
				}
			}
		}

		void SQLiteDatabase::open() throw (SQLiteDatabase::SQLiteQueryException)
		{
			//Configure SQLite Library
			SQLiteConfigure::configure();

			// check if SQLite is thread-safe
			if (sqlite3_threadsafe() == 0)
			{
				IBRCOMMON_LOGGER_TAG("SQLiteDatabase", critical) << "sqlite library has not compiled with threading support." << IBRCOMMON_LOGGER_ENDL;
				throw ibrcommon::Exception("need threading support in sqlite!");
			}

			//open the database
			if (sqlite3_open_v2(_file.getPath().c_str(), &_database,  SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL))
			{
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << "Can't open database: " << sqlite3_errmsg(_database) << IBRCOMMON_LOGGER_ENDL;
				sqlite3_close(_database);
				throw ibrcommon::Exception("Unable to open sqlite database");
			}
			try {
				// check database version and upgrade if necessary
				int version = getVersion();

				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, info) << "Database version " << version << " found." << IBRCOMMON_LOGGER_ENDL;

				if (version != DBSCHEMA_VERSION)
				{
					doUpgrade(version, DBSCHEMA_VERSION);
				}
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, warning) << "upgrade failed, start-over with a fresh database" << IBRCOMMON_LOGGER_ENDL;
				doUpgrade(0, DBSCHEMA_VERSION);
			}

			// disable synchronous mode
			sqlite3_exec(_database, "PRAGMA synchronous = OFF;", NULL, NULL, NULL);

			// enable sqlite tracing if debug level is higher than 50
			if (IBRCOMMON_LOGGER_LEVEL >= 50)
			{
				sqlite3_trace(_database, &sql_tracer, NULL);
			}

			// calculate next Bundleexpiredtime
			update_expire_time();
		}

		void SQLiteDatabase::close()
		{
			//close Databaseconnection
			if (sqlite3_close(_database) != SQLITE_OK)
			{
				IBRCOMMON_LOGGER_TAG("SQLiteDatabase", error) << "unable to close database" << IBRCOMMON_LOGGER_ENDL;
			}

			// shutdown sqlite library
			SQLiteConfigure::shutdown();
		}

		void SQLiteDatabase::get(const dtn::data::BundleID &id, dtn::data::MetaBundle &meta) const throw (SQLiteDatabase::SQLiteQueryException, NoBundleFoundException)
		{
			// lock the prepared statement
			Statement st(_database, _sql_queries[BUNDLE_GET_ID]);

			// bind bundle id to the statement
			set_bundleid(st, id);

			// execute the query and check for error
			if ((st.step() != SQLITE_ROW) || _faulty)
			{
				std::stringstream error;
				error << "No Bundle found with BundleID: " << id.toString();
				IBRCOMMON_LOGGER_DEBUG_TAG(SQLiteDatabase::TAG, 15) << error.str() << IBRCOMMON_LOGGER_ENDL;
				throw SQLiteQueryException(error.str());
			}

			// query bundle data
			get(st, meta);
		}

		void SQLiteDatabase::get(Statement &st, dtn::data::MetaBundle &bundle, int offset) const throw (SQLiteDatabase::SQLiteQueryException)
		{
			try {
				bundle.source = dtn::data::EID( (const char*) sqlite3_column_text(*st, offset) );
				bundle.destination = dtn::data::EID( (const char*) sqlite3_column_text(*st, offset + 1) );
				bundle.reportto = dtn::data::EID( (const char*) sqlite3_column_text(*st, offset + 2) );
				bundle.custodian = dtn::data::EID( (const char*) sqlite3_column_text(*st, offset + 3) );
			} catch (const dtn::InvalidDataException&) {
				IBRCOMMON_LOGGER_TAG(TAG, warning) << "unable to read EIDs from database" << IBRCOMMON_LOGGER_ENDL;
				throw SQLiteDatabase::SQLiteQueryException("unable to read EIDs from database");
			}

			bundle.procflags = sqlite3_column_int(*st, offset + 4);
			bundle.timestamp = sqlite3_column_int64(*st, offset + 5);
			bundle.sequencenumber = sqlite3_column_int64(*st, offset + 6);
			bundle.lifetime = sqlite3_column_int64(*st, offset + 7);
			bundle.expiretime = sqlite3_column_int64(*st, offset + 8);

			if (bundle.procflags & data::Bundle::FRAGMENT)
			{
				bundle.setFragment(true);
				bundle.fragmentoffset = sqlite3_column_int64(*st, offset + 9);
				bundle.appdatalength = sqlite3_column_int64(*st, offset + 10);
			}
			else
			{
				bundle.setFragment(false);
				bundle.fragmentoffset = 0;
				bundle.appdatalength = 0;
			}

			if (sqlite3_column_type(*st, offset + 11) != SQLITE_NULL)
			{
				bundle.hopcount = sqlite3_column_int64(*st, 11);
			}
			else
			{
				bundle.hopcount = dtn::data::Number::max();
			}

			// restore net priority
			bundle.net_priority = sqlite3_column_int(*st, 12);

			// set payload length
			bundle.setPayloadLength(sqlite3_column_int64(*st, offset + 13));
		}

		void SQLiteDatabase::get(Statement &st, dtn::data::Bundle &bundle, const int offset) const throw (SQLiteDatabase::SQLiteQueryException)
		{
			try {
				bundle.source = dtn::data::EID( (const char*) sqlite3_column_text(*st, offset + 0) );
				bundle.destination = dtn::data::EID( (const char*) sqlite3_column_text(*st, offset + 1) );
				bundle.reportto = dtn::data::EID( (const char*) sqlite3_column_text(*st, offset + 2) );
				bundle.custodian = dtn::data::EID( (const char*) sqlite3_column_text(*st, offset + 3) );
			} catch (const dtn::InvalidDataException&) {
				IBRCOMMON_LOGGER_TAG(TAG, warning) << "unable to read EIDs from database" << IBRCOMMON_LOGGER_ENDL;
				throw SQLiteDatabase::SQLiteQueryException("unable to read EIDs from database");
			}

			bundle.procflags = sqlite3_column_int(*st, offset + 4);
			bundle.timestamp = sqlite3_column_int64(*st, offset + 5);
			bundle.sequencenumber = sqlite3_column_int64(*st, offset + 6);
			bundle.lifetime = sqlite3_column_int64(*st, offset + 7);
			// offset = 8 -> expiretime

			if (bundle.procflags & data::Bundle::FRAGMENT)
			{
				bundle.fragmentoffset = sqlite3_column_int64(*st, offset + 9);
				bundle.appdatalength = sqlite3_column_int64(*st, offset + 10);
			}
		}

		void SQLiteDatabase::iterateAll() throw (SQLiteDatabase::SQLiteQueryException)
		{
			Statement st(_database, _sql_queries[BUNDLE_GET_ITERATOR]);
			// abort if enough bundles are found
			while (st.step() == SQLITE_ROW)
			{
				dtn::data::MetaBundle m;

				// extract the primary values and set them in the bundle object
				get(st, m, 0);

				// call iteration callback
				_listener.iterateDatabase(m, sqlite3_column_int(*st, 14));
			}

			st.reset();
		}

		void SQLiteDatabase::get(const BundleSelector &cb, BundleResult &ret) throw (NoBundleFoundException, BundleSelectorException)
		{
			size_t items_added = 0;

			const std::string base_query =
					"SELECT " + _select_names[0] + " FROM " + _tables[SQL_TABLE_BUNDLE];

			size_t offset = 0;
			const bool unlimited = (cb.limit() <= 0);
			const size_t query_limit = 50;

			try {
				try {
					const SQLBundleQuery &query = dynamic_cast<const SQLBundleQuery&>(cb);

					// custom query string
					const std::string query_string = base_query + " WHERE " + query.getWhere() + " ORDER BY priority DESC, timestamp, sequencenumber, fragmentoffset, fragmentlength LIMIT ?,?;";

					// create statement for custom query
					Statement st(_database, query_string);

					while (unlimited || (items_added < query_limit))
					{
						// bind the statement parameter
						int bind_offset = query.bind(*st, 1);

						// query the database
						__get(cb, st, ret, items_added, bind_offset, offset, query_limit);

						// increment the offset, because we might not have enough
						offset += query_limit;
					}
				} catch (const std::bad_cast&) {
					Statement st(_database, _sql_queries[BUNDLE_GET_FILTER]);

					while (unlimited || (items_added < query_limit))
					{
						// query the database
						__get(cb, st, ret, items_added, 1, offset, query_limit);

						// increment the offset, because we might not have enough
						offset += query_limit;
					}
				}
			} catch (const SQLiteDatabase::SQLiteQueryException &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			} catch (const dtn::storage::NoBundleFoundException&) { }

			if (items_added == 0) throw dtn::storage::NoBundleFoundException();
		}

		void SQLiteDatabase::__get(const BundleSelector &cb, Statement &st, BundleResult &ret, size_t &items_added, const int bind_offset, const size_t offset, const size_t query_limit) const throw (SQLiteDatabase::SQLiteQueryException, NoBundleFoundException, BundleSelectorException)
		{
			const bool unlimited = (cb.limit() <= 0);

			// limit result according to callback result
			sqlite3_bind_int64(*st, bind_offset, offset);
			sqlite3_bind_int64(*st, bind_offset + 1, query_limit);

			// returned no result
			if ((st.step() == SQLITE_DONE) || _faulty)
				throw dtn::storage::NoBundleFoundException();

			// abort if enough bundles are found
			while (unlimited || (items_added < query_limit))
			{
				dtn::data::MetaBundle m;

				// extract the primary values and set them in the bundle object
				get(st, m, 0);

				// check if the bundle is already expired
				if ( !dtn::utils::Clock::isExpired( m ) )
				{
					// ask the filter if this bundle should be added to the return list
					if (cb.addIfSelected(ret, m))
					{
						IBRCOMMON_LOGGER_DEBUG_TAG("SQLiteDatabase", 40) << "add bundle to query selection list: " << m.toString() << IBRCOMMON_LOGGER_ENDL;

						// bundle has been added - increment counter
						items_added++;
					}
				}

				if (st.step() != SQLITE_ROW) break;
			}

			st.reset();
		}

		void SQLiteDatabase::get(const dtn::data::BundleID &id, dtn::data::Bundle &bundle, blocklist &blocks) const throw (SQLiteDatabase::SQLiteQueryException, NoBundleFoundException)
		{
			int err = 0;

			IBRCOMMON_LOGGER_DEBUG_TAG("SQLiteDatabase", 25) << "get bundle from sqlite storage " << id.toString() << IBRCOMMON_LOGGER_ENDL;

			// do this while db is locked
			Statement st(_database, _sql_queries[BUNDLE_GET_ID]);

			// set the bundle key values
			set_bundleid(st, id);

			// execute the query and check for error
			if (((err = st.step()) != SQLITE_ROW) || _faulty)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG(SQLiteDatabase::TAG, 15) << "sql error: " << err << "; No bundle found with id: " << id.toString() << IBRCOMMON_LOGGER_ENDL;
				throw dtn::storage::NoBundleFoundException();
			}

			// read bundle data
			get(st, bundle);

			try {

				int err = 0;
				std::string file;

				Statement st(_database, _sql_queries[BLOCK_GET_ID]);

				// set the bundle key values
				set_bundleid(st, id);

				// query the database and step through all blocks
				while ((err = st.step()) == SQLITE_ROW)
				{
					const ibrcommon::File f( (const char*) sqlite3_column_text(*st, 0) );
					int blocktyp = sqlite3_column_int(*st, 1);

					blocks.push_back( blocklist_entry(blocktyp, f) );
				}

				if (err == SQLITE_DONE)
				{
					if (blocks.size() == 0)
					{
						IBRCOMMON_LOGGER_TAG("SQLiteDatabase", error) << "get_blocks: no blocks found for: " << id.toString() << IBRCOMMON_LOGGER_ENDL;
						throw SQLiteQueryException("no blocks found");
					}
				}
				else
				{
					IBRCOMMON_LOGGER_TAG("SQLiteDatabase", error) << "get_blocks() failure: "<< err << " " << sqlite3_errmsg(_database) << IBRCOMMON_LOGGER_ENDL;
					throw SQLiteQueryException("can not query for blocks");
				}

			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG("SQLiteDatabase", error) << "could not get bundle blocks: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				throw dtn::storage::NoBundleFoundException();
			}
		}

		void SQLiteDatabase::store(const dtn::data::Bundle &bundle, const dtn::data::Length &size) throw (SQLiteDatabase::SQLiteQueryException)
		{
			int err;

			Statement st(_database, _sql_queries[BUNDLE_STORE]);

			set_bundleid(st, bundle);

			sqlite3_bind_text(*st, 6, bundle.destination.getString().c_str(), static_cast<int>(bundle.destination.getString().length()), SQLITE_TRANSIENT);
			sqlite3_bind_text(*st, 7, bundle.reportto.getString().c_str(), static_cast<int>(bundle.reportto.getString().length()), SQLITE_TRANSIENT);
			sqlite3_bind_text(*st, 8, bundle.custodian.getString().c_str(), static_cast<int>(bundle.custodian.getString().length()), SQLITE_TRANSIENT);
			sqlite3_bind_int(*st, 9, bundle.procflags.get<uint32_t>());
			sqlite3_bind_int64(*st, 10, bundle.lifetime.get<uint64_t>());

			if (bundle.get(dtn::data::Bundle::FRAGMENT))
			{
				sqlite3_bind_int64(*st, 11, bundle.appdatalength.get<uint64_t>());
			}
			else
			{
				sqlite3_bind_int64(*st, 11, -1);
			}

			dtn::data::Timestamp expire_time = dtn::utils::Clock::getExpireTime(bundle);
			sqlite3_bind_int64(*st, 12, expire_time.get<uint64_t>());

			sqlite3_bind_int64(*st, 13, bundle.getPriority());

			try {
				const dtn::data::ScopeControlHopLimitBlock &schl = bundle.find<dtn::data::ScopeControlHopLimitBlock>();
				sqlite3_bind_int64(*st, 14, schl.getHopsToLive().get<uint64_t>() );
			} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) {
				sqlite3_bind_null(*st, 14 );
			}

			try {
				const dtn::data::SchedulingBlock &sched = bundle.find<dtn::data::SchedulingBlock>();
				sqlite3_bind_int(*st, 15, sched.getPriority().get<int>() );
			} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) {
				sqlite3_bind_int64(*st, 15, 0 );
			}

			// set payload length
			sqlite3_bind_int64(*st, 16, bundle.getPayloadLength());

			// set bundle size
			sqlite3_bind_int64(*st, 17, size);

			err = st.step();

			if (err == SQLITE_CONSTRAINT)
			{
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, warning) << "Bundle is already in the storage" << IBRCOMMON_LOGGER_ENDL;

				std::stringstream error;
				error << "store() failure: " << err << " " <<  sqlite3_errmsg(_database);
				throw SQLiteQueryException(error.str());
			}
			else if ((err != SQLITE_DONE) || _faulty)
			{
				std::stringstream error;
				error << "store() failure: " << err << " " <<  sqlite3_errmsg(_database);
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << error.str() << IBRCOMMON_LOGGER_ENDL;

				throw SQLiteQueryException(error.str());
			}

			// set new expire time
			new_expire_time(expire_time);
		}

		void SQLiteDatabase::store(const dtn::data::BundleID &id, int index, const dtn::data::Block &block, const ibrcommon::File &file) throw (SQLiteDatabase::SQLiteQueryException)
		{
			int blocktyp = (int)block.getType();

			// protect this query from concurrent access and enable the auto-reset feature
			Statement st(_database, _sql_queries[BLOCK_STORE]);

			// set bundle key data
			set_bundleid(st, id);

			// set the block type
			sqlite3_bind_int(*st, 6, blocktyp);

			// the filename of the block data
			sqlite3_bind_text(*st, 7, file.getPath().c_str(), static_cast<int>(file.getPath().size()), SQLITE_TRANSIENT);

			// the ordering number
			sqlite3_bind_int(*st, 8, index);

			// execute the query and store the block in the database
			if (st.step() != SQLITE_DONE)
			{
				throw SQLiteQueryException("can not store block of bundle");
			}
		}

		void SQLiteDatabase::transaction() throw (SQLiteDatabase::SQLiteQueryException)
		{
			char *zErrMsg = 0;

			// start a transaction
			int ret = sqlite3_exec(_database, "BEGIN TRANSACTION;", NULL, NULL, &zErrMsg);

			// check if the return value signals an error
			if ( ret != SQLITE_OK )
			{
				sqlite3_free( zErrMsg );
				throw SQLiteQueryException( zErrMsg );
			}
		}

		void SQLiteDatabase::rollback() throw (SQLiteDatabase::SQLiteQueryException)
		{
			char *zErrMsg = 0;

			// rollback the whole transaction
			int ret = sqlite3_exec(_database, "ROLLBACK TRANSACTION;", NULL, NULL, &zErrMsg);

			// check if the return value signals an error
			if ( ret != SQLITE_OK )
			{
				sqlite3_free( zErrMsg );
				throw SQLiteQueryException( zErrMsg );
			}
		}

		void SQLiteDatabase::commit() throw (SQLiteDatabase::SQLiteQueryException)
		{
			char *zErrMsg = 0;

			// commit the transaction
			int ret = sqlite3_exec(_database, "END TRANSACTION;", NULL, NULL, &zErrMsg);

			// check if the return value signals an error
			if ( ret != SQLITE_OK )
			{
				sqlite3_free( zErrMsg );
				throw SQLiteQueryException( zErrMsg );
			}
		}

		dtn::data::Length SQLiteDatabase::remove(const dtn::data::BundleID &id) throw (SQLiteDatabase::SQLiteQueryException)
		{
			// return value (size of the bundle in bytes)
			dtn::data::Length ret = 0;

			{
				// lock the database
				Statement st(_database, _sql_queries[BUNDLE_GET_LENGTH_ID]);

				// bind bundle id to the statement
				set_bundleid(st, id);

				// execute the query and check for error
				if (st.step() != SQLITE_ROW)
				{
					// no bundle found - stop here
					return ret;
				}
				else
				{
					ret = sqlite3_column_int(*st, 0);
				}
			}

			{
				// lock the database
				Statement st(_database, _sql_queries[BLOCK_GET_ID]);

				// set the bundle key values
				set_bundleid(st, id);

				// step through all blocks
				while (st.step() == SQLITE_ROW)
				{
					// delete each referenced block file
					ibrcommon::File blockfile( (const char*)sqlite3_column_text(*st, 0) );
					blockfile.remove();
				}
			}

			{
				// lock the database
				Statement st(_database, _sql_queries[BUNDLE_DELETE]);

				// then remove the bundle data
				set_bundleid(st, id);
				st.step();

				IBRCOMMON_LOGGER_DEBUG_TAG("SQLiteDatabase", 10) << "bundle " << id.toString() << " deleted" << IBRCOMMON_LOGGER_ENDL;
			}

			//update deprecated timer
			update_expire_time();

			// return the size of the removed bundle
			return ret;
		}

		void SQLiteDatabase::clear() throw (SQLiteDatabase::SQLiteQueryException)
		{
			Statement vacuum(_database, _sql_queries[VACUUM]);
			Statement bundle_clear(_database, _sql_queries[BUNDLE_CLEAR]);
			Statement block_clear(_database, _sql_queries[BLOCK_CLEAR]);

			bundle_clear.step();
			block_clear.step();

			if (SQLITE_DONE != vacuum.step())
			{
				throw SQLiteQueryException("SQLiteBundleStore: clear(): vacuum failed.");
			}

			reset_expire_time();
		}

		bool SQLiteDatabase::contains(const dtn::data::BundleID &id) throw (SQLiteDatabase::SQLiteQueryException)
		{
			// lock the prepared statement
			Statement st(_database, _sql_queries[BUNDLE_GET_ID]);

			// bind bundle id to the statement
			set_bundleid(st, id);

			// execute the query and check for error
			return !((st.step() != SQLITE_ROW) || _faulty);
		}

		bool SQLiteDatabase::empty() const throw (SQLiteDatabase::SQLiteQueryException)
		{
			Statement st(_database, _sql_queries[EMPTY_CHECK]);

			if (SQLITE_DONE == st.step())
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		size_t SQLiteDatabase::count() const throw (SQLiteDatabase::SQLiteQueryException)
		{
			size_t rows = 0;
			int err = 0;

			Statement st(_database, _sql_queries[COUNT_ENTRIES]);

			if ((err = st.step()) == SQLITE_ROW)
			{
				rows = sqlite3_column_int(*st, 0);
			}
			else
			{
				std::stringstream error;
				error << "count: failure " << err << " " << sqlite3_errmsg(_database);
				throw SQLiteQueryException(error.str());
			}

			return rows;
		}

		const std::set<dtn::data::EID> SQLiteDatabase::getDistinctDestinations() throw (SQLiteDatabase::SQLiteQueryException)
		{
			std::set<dtn::data::EID> ret;

			Statement st(_database, _sql_queries[GET_DISTINCT_DESTINATIONS]);

			// step through all blocks
			while (st.step() == SQLITE_ROW)
			{
				// delete each referenced block file
				const std::string destination( (const char*)sqlite3_column_text(*st, 0) );
				ret.insert(destination);
			}

			return ret;
		}

		void SQLiteDatabase::update_expire_time() throw (SQLiteDatabase::SQLiteQueryException)
		{
			Statement st(_database, _sql_queries[EXPIRE_NEXT_TIMESTAMP]);

			int err = st.step();

			if (err == SQLITE_ROW)
			{
				_next_expiration = sqlite3_column_int64(*st, 0);
			}
			else
			{
				_next_expiration = 0;
			}
		}

		void SQLiteDatabase::expire(const dtn::data::Timestamp &timestamp) throw ()
		{
			/*
			 * Only if the actual time is bigger or equal than the time when the next bundle expires, deleteexpired is called.
			 */
			dtn::data::Timestamp exp_time = get_expire_time();
			if ((timestamp < exp_time) || (exp_time == 0)) return;

			/*
			 * Performanceverbesserung: Damit die Abfragen nicht jede Sekunde ausgeführt werden müssen, speichert man den Zeitpunkt an dem
			 * das nächste Bündel gelöscht werden soll in eine Variable und führt deleteexpired erst dann aus wenn ein Bündel abgelaufen ist.
			 * Nach dem Löschen wird die DB durchsucht und der nächste Ablaufzeitpunkt wird in die Variable gesetzt.
			 */

			try {
				Statement st(_database, _sql_queries[EXPIRE_BUNDLE_FILENAMES]);

				// query for blocks of expired bundles
				sqlite3_bind_int64(*st, 1, timestamp.get<uint64_t>());
				while (st.step() == SQLITE_ROW)
				{
					ibrcommon::File block((const char*)sqlite3_column_text(*st,0));
					block.remove();
				}
			} catch (const SQLiteDatabase::SQLiteQueryException &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}

			try {
				Statement st(_database, _sql_queries[EXPIRE_BUNDLES]);

				dtn::data::BundleID id;

				// query expired bundles
				sqlite3_bind_int64(*st, 1, timestamp.get<uint64_t>());
				while (st.step() == SQLITE_ROW)
				{
					id.source = dtn::data::EID((const char*)sqlite3_column_text(*st, 0));
					id.timestamp = sqlite3_column_int64(*st, 1);
					id.sequencenumber = sqlite3_column_int64(*st, 2);

					id.setFragment(sqlite3_column_int64(*st, 3) >= 0);

					if (id.isFragment()) {
						id.fragmentoffset = sqlite3_column_int64(*st, 3);
					} else {
						id.fragmentoffset = 0;
					}

					id.setPayloadLength(sqlite3_column_int64(*st, 4));

					dtn::core::BundleExpiredEvent::raise(id);

					// raise bundle removed event
					_listener.eventBundleExpired(id, sqlite3_column_int(*st, 5));
				}
			} catch (const SQLiteDatabase::SQLiteQueryException &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}

			try {
				Statement st(_database, _sql_queries[EXPIRE_BUNDLE_DELETE]);

				// delete all expired db entries (bundles and blocks)
				sqlite3_bind_int64(*st, 1, timestamp.get<uint64_t>());
				st.step();
			} catch (const SQLiteDatabase::SQLiteQueryException &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}

			try {
				//update deprecated timer
				update_expire_time();
			} catch (const SQLiteDatabase::SQLiteQueryException &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteDatabase::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SQLiteDatabase::vacuum() throw (SQLiteDatabase::SQLiteQueryException)
		{
			Statement st(_database, _sql_queries[VACUUM]);
			st.step();
		}

		void SQLiteDatabase::update(UPDATE_VALUES mode, const dtn::data::BundleID &id, const dtn::data::EID &eid) throw (SQLiteDatabase::SQLiteQueryException)
		{
			if (mode == UPDATE_CUSTODIAN)
			{
				// select query with or without fragmentation extension
				Statement st(_database, _sql_queries[BUNDLE_UPDATE_CUSTODIAN]);

				sqlite3_bind_text(*st, 1, eid.getString().c_str(), static_cast<int>(eid.getString().length()), SQLITE_TRANSIENT);
				set_bundleid(st, id, 1);

				// update the custodian in the database
				int err = st.step();

				if (err != SQLITE_DONE)
				{
					std::stringstream error;
					error << "update_custodian() failure: " << err << " " <<  sqlite3_errmsg(_database);
					throw SQLiteDatabase::SQLiteQueryException(error.str());
				}
			}
		}

		void SQLiteDatabase::new_expire_time(const dtn::data::Timestamp &ttl) throw ()
		{
			if (_next_expiration == 0 || ttl < _next_expiration)
			{
				_next_expiration = ttl;
			}
		}

		void SQLiteDatabase::reset_expire_time() throw ()
		{
			_next_expiration = 0;
		}

		const dtn::data::Timestamp& SQLiteDatabase::get_expire_time() const throw ()
		{
			return _next_expiration;
		}

		void SQLiteDatabase::set_bundleid(Statement &st, const dtn::data::BundleID &id, int offset) const throw (SQLiteDatabase::SQLiteQueryException)
		{
			sqlite3_bind_text(*st, offset + 1, id.source.getString().c_str(), static_cast<int>(id.source.getString().length()), SQLITE_TRANSIENT);
			sqlite3_bind_int64(*st, offset + 2, id.timestamp.get<uint64_t>());
			sqlite3_bind_int64(*st, offset + 3, id.sequencenumber.get<uint64_t>());

			if (id.isFragment())
			{
				sqlite3_bind_int64(*st, offset + 4, id.fragmentoffset.get<uint64_t>());
				sqlite3_bind_int64(*st, offset + 5, id.getPayloadLength());
			}
			else
			{
				sqlite3_bind_int64(*st, offset + 4, -1);
				sqlite3_bind_int64(*st, offset + 5, -1);
			}
		}

		void SQLiteDatabase::setFaulty(bool mode)
		{
			_faulty = mode;
		}
	} /* namespace storage */
} /* namespace dtn */
