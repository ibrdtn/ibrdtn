/*
 * SQLiteBundleSetFactory.cpp
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
 *  Created on: May 7, 2013
 */

#include "storage/SQLiteBundleSetFactory.h"
#include "storage/SQLiteBundleSet.h"

namespace dtn {
	namespace storage {
		SQLiteBundleSetFactory::SQLiteBundleSetFactory(SQLiteDatabase& db) : _database(db)
		{
		}
		SQLiteBundleSetFactory::~SQLiteBundleSetFactory()
		{
		}

		dtn::data::BundleSetImpl* SQLiteBundleSetFactory::createBundleSet(dtn::data::BundleSet::Listener* listener, dtn::data::Size bf_size)
		{
			return new SQLiteBundleSet(listener,bf_size,_database);
		}
		dtn::data::BundleSetImpl* SQLiteBundleSetFactory::createBundleSet(const std::string &name, dtn::data::BundleSet::Listener* listener, dtn::data::Size bf_size)
		{
			return new SQLiteBundleSet(name,listener,bf_size,_database);
		}

	} /* namespace storage */
} /* namespace dtn */
