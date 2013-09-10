/*
 * SQLiteBundleSetFactory.h
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
 * Created on: May 7, 2013
 */

#ifndef SQLITEBUNDLESETFACTORY_H_
#define SQLITEBUNDLESETFACTORY_H_

#include "ibrdtn/data/BundleSetFactory.h"
#include "storage/SQLiteDatabase.h"

namespace dtn
{
	namespace storage
	{

		class SQLiteBundleSetFactory : public dtn::data::BundleSetFactory{
			public:
				SQLiteBundleSetFactory(SQLiteDatabase& db);
				~SQLiteBundleSetFactory();

			protected:
				dtn::data::BundleSetImpl* createBundleSet(dtn::data::BundleSet::Listener* listener, dtn::data::Size bf_size);
				dtn::data::BundleSetImpl* createBundleSet(const std::string &name, dtn::data::BundleSet::Listener* listener, dtn::data::Size bf_size);

			private:
				SQLiteDatabase& _database;
		};

	} /* namespace storage */
} /* namespace dtn */
#endif /* SQLITEBUNDLESETFACTORY_H_ */
