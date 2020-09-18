/*
 * BundleSetTest.h
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
 *  Created on: May 14, 2013
 */

#ifndef BUNDLESETTEST_H_
#define BUNDLESETTEST_H_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <ibrcommon/TimeMeasurement.h>

#include "config.h"
#include "storage/BundleStorage.h"
#include "../tools/EventSwitchLoop.h"
#include <vector>

class BundleSetTest : public CppUnit::TestFixture {
	private:

		ibrtest::EventSwitchLoop *esl;
		ibrcommon::TimeMeasurement tm;


		class ExpiredBundleCounter : public dtn::data::BundleSet::Listener
		{
		public:
			ExpiredBundleCounter();
			virtual ~ExpiredBundleCounter();

			void eventBundleExpired(const dtn::data::MetaBundle &b) noexcept;
			int counter;
		};
		void genbundles(dtn::data::BundleSet &l, int number, int offset, int max);
		dtn::data::MetaBundle genBundle(int offset, int range);

	public:
#define CPPUNIT_TEST_ALL_STORAGES(testMethod) \
		testCounter = 0; \
		for (size_t i = 0; i < _storage_names.size(); i++) { \
		  CPPUNIT_TEST_SUITE_ADD_TEST( \
		    ( new CPPUNIT_NS::TestCaller<TestFixtureType>( \
		    	context.getTestNameFor( std::string(#testMethod) + " (" + _storage_names[i] + ")" ), \
		    	&TestFixtureType::testMethod, \
		        context.makeFixture() ) ) ); \
		}

		void containTest();
		void orderTest();
		void namingTest();
		void performanceTest();
		void persistanceTest();
		void sizeTest();

		void setUp();
		void tearDown();

		CPPUNIT_TEST_SUITE(BundleSetTest);

		_storage_names.push_back("MemoryBundleStorage");

#ifdef HAVE_SQLITE
		_storage_names.push_back("SQLiteBundleStorage");
#endif

		CPPUNIT_TEST_ALL_STORAGES(containTest);
		CPPUNIT_TEST_ALL_STORAGES(orderTest);
		CPPUNIT_TEST_ALL_STORAGES(namingTest);
		CPPUNIT_TEST_ALL_STORAGES(performanceTest);
		CPPUNIT_TEST_ALL_STORAGES(persistanceTest);
		CPPUNIT_TEST_ALL_STORAGES(sizeTest);
		CPPUNIT_TEST_SUITE_END();

		static size_t testCounter;
		static dtn::storage::BundleStorage *_storage;
		static std::vector<std::string> _storage_names;
};
#endif /* BUNDLESETTEST_H_ */
