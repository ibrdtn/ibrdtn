/*
 * BundleSetTest.cpp
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

#include "BundleSetTest.hh"
#include "Component.h"

#include "storage/MemoryBundleStorage.h"
#include "ibrdtn/data/MemoryBundleSet.h"

#ifdef HAVE_SQLITE
#include "storage/SQLiteBundleStorage.h"
#endif

size_t BundleSetTest::testCounter;
dtn::storage::BundleStorage* BundleSetTest::_storage = NULL;
std::vector<std::string> BundleSetTest::_storage_names;


CPPUNIT_TEST_SUITE_REGISTRATION(BundleSetTest);
void BundleSetTest::setUp()
{
	srand ( time(NULL) );
	// create a new event switch
	esl = new ibrtest::EventSwitchLoop();


	switch(testCounter++){
	//MemoryBundleSet
	case 0: {
		_storage = new dtn::storage::MemoryBundleStorage();

		ibrcommon::File path("/tmp/memory-bundleset-test");
		if (path.exists()) path.remove(true);

		MemoryBundleSet::setPath(path);
		break;
	}

	//SQLiteBundleSet
#ifdef HAVE_SQLITE
	case 1: {
			ibrcommon::File path("/tmp/sqlite-bundleset-test");
			if (path.exists()) path.remove(true);
			ibrcommon::File::createDirectory(path);

			_storage = new dtn::storage::SQLiteBundleStorage(path, 0, true);

			break;

	}
#endif
	}


	if (testCounter >= _storage_names.size()) testCounter = 0;

	esl->start();
	try {
		dtn::daemon::Component &c = dynamic_cast<dtn::daemon::Component&>(*_storage);
		c.initialize();
		c.startup();
	} catch (const std::bad_cast&) {
	}
}

void BundleSetTest::tearDown()
{
	esl->stop();

	try {
		dtn::daemon::Component &c = dynamic_cast<dtn::daemon::Component&>(*_storage);
		c.terminate();
	} catch (const std::bad_cast&) {
	}

	esl->join();
	delete esl;
	esl = NULL;

	delete _storage;

}

/*========================== tests below ==========================*/

void BundleSetTest::containTest(){
	ExpiredBundleCounter ebc;
	dtn::data::BundleSet l(&ebc,1024);

	dtn::data::Bundle b1;
	b1.source = dtn::data::EID("dtn:test");
	b1.timestamp = 1;
	b1.sequencenumber = 1;

	dtn::data::Bundle b2;
	b2.source = dtn::data::EID("dtn:test");
	b2.timestamp = 2;
	b2.sequencenumber = 3;

	CPPUNIT_ASSERT(l.has(b1) == false);
	CPPUNIT_ASSERT(l.has(b2) == false);

	l.add(dtn::data::MetaBundle::create(b1));

	CPPUNIT_ASSERT(l.has(b1) == true);
	CPPUNIT_ASSERT(l.has(b2) == false);

	l.add(dtn::data::MetaBundle::create(b2));

	CPPUNIT_ASSERT(l.has(b1) == true);
	CPPUNIT_ASSERT(l.has(b2) == true);

	l.clear();

	CPPUNIT_ASSERT(l.has(b1) == false);
	CPPUNIT_ASSERT(l.has(b2) == false);

	l.add(dtn::data::MetaBundle::create(b1));

	CPPUNIT_ASSERT(l.has(b1) == true);
	CPPUNIT_ASSERT(l.has(b2) == false);

	l.add(dtn::data::MetaBundle::create(b2));

	CPPUNIT_ASSERT(l.has(b1) == true);
	CPPUNIT_ASSERT(l.has(b2) == true);
}

void BundleSetTest::orderTest(){

	dtn::data::BundleSet l;

	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, l.size());

	genbundles(l, 500, 0, 500);
	genbundles(l, 500, 600, 1000);

	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)1000, l.size());


	for (int i = 0; i < 550; ++i)
	{
		l.expire(i);
	}


	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)500, l.size());

	for (int i = 0; i < 1050; ++i)
	{
		l.expire(i);
	}

	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, l.size());
}

void BundleSetTest::namingTest()
{
	// do not test naming without sqlite bundle storage
	// named bundle-set without sqlite is currently not supported
#ifdef HAVE_SQLITE
	if (dynamic_cast<dtn::storage::SQLiteBundleStorage*>(_storage) == NULL)
		return;
#else
	// do not run the test without sqlite support
	return;
#endif

	const std::string name1 = "test1BundleSet1";
	const std::string name2 = "test2BundleSet2";

	dtn::data::BundleSet a; //automatically named BundleSet
	dtn::data::BundleSet b(name1);
	dtn::data::BundleSet c;
	dtn::data::BundleSet d(name2);
	dtn::data::BundleSet e(name2);

	// create bundles in each set, check if size is correct
	genbundles(a, 500, 1, 0);
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)500, a.size());
	genbundles(b, 500, 1, 0);
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)500, b.size());
	genbundles(c, 500, 1, 0);
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)500, c.size());
	genbundles(d, 500, 1, 0);
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)500, d.size());
	genbundles(e, 500, 1, 0);

	// bundle with same should have double as many bundles
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)1000, d.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)1000, e.size());

	// clear each set individually, check if size is correct
	a.clear();
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, a.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)500, b.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)500, c.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)1000, d.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)1000, e.size());

	b.clear();
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, a.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, b.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)500, c.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)1000, d.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)1000, e.size());

	c.clear();
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, a.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, b.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, c.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)1000, d.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)1000, e.size());

	d.clear();
	e.clear();
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, a.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, b.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, c.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, d.size());
	CPPUNIT_ASSERT_EQUAL((dtn::data::Size)0, e.size());
}

void BundleSetTest::performanceTest()
{
	BundleSet set;

	// start measurement
	tm.start();

	for (int i = 0; i < 1000; ++i) {
		if (i%2 == 0) {
			genbundles(set,100,10,15);
		} else {
			set.clear();
		}
	}

	// stop measurement
	tm.stop();

	std::cout << std::endl << "completed after " << tm ;
}


void BundleSetTest::persistanceTest()
{
	size_t num_bundles = 1000;
	size_t size_before = 0;
	size_t size_after = 0;

	std::stringstream ss; ss << rand();
	{
		BundleSet set(ss.str());
		genbundles(set,num_bundles,10,15);
		size_before = set.size();
	}
	CPPUNIT_ASSERT_EQUAL(num_bundles,size_before);
	{
		BundleSet set(ss.str());
		size_after = set.size();
	}
	CPPUNIT_ASSERT_EQUAL(num_bundles,size_after);
}

void BundleSetTest::sizeTest()
{
	BundleSet src(NULL, 12000);
	genbundles(src,100,10,15);

	BundleSet dst;
	genbundles(dst,100,10,15);

	CPPUNIT_ASSERT(src.getBloomFilter().size() != dst.getBloomFilter().size());

	std::stringstream ss;
	ss << src;
	ss.clear();

	ss >> dst;

	CPPUNIT_ASSERT_EQUAL(src.getBloomFilter().size(), dst.getBloomFilter().size());
}

void BundleSetTest::genbundles(dtn::data::BundleSet &l, int number, int offset, int max)
{
	int range = max - offset;

	for (int i = 0; i < number; ++i)
	{
		l.add(genBundle(offset,range));
	}
}

dtn::data::MetaBundle BundleSetTest::genBundle(int offset, int range)
{
	dtn::data::MetaBundle b;
	int random_integer = offset + (rand() % range);

	b.lifetime = random_integer;
	b.expiretime = random_integer;

	b.timestamp = 1;
	b.sequencenumber = random_integer;

	std::stringstream ss; ss << rand();

	b.source = dtn::data::EID("dtn://node" + ss.str() + "/application");
	return b;
}

BundleSetTest::ExpiredBundleCounter::ExpiredBundleCounter()
 : counter(0)
{

}

BundleSetTest::ExpiredBundleCounter::~ExpiredBundleCounter()
{
}

void BundleSetTest::ExpiredBundleCounter::eventBundleExpired(const dtn::data::MetaBundle&) throw ()
{
	counter++;
}
