/*
 * BundleSetTest.cpp
 *
 *  Created on: May 14, 2013
 *      Author: goltzsch
 */

#include "BundleSetTest.hh"
#include "Component.h"

#include "storage/MemoryBundleStorage.h"

#ifdef HAVE_SQLITE
#include "storage/SQLiteBundleStorage.h"
#endif

size_t BundleSetTest::testCounter;
dtn::storage::BundleStorage* BundleSetTest::_storage = NULL;
std::vector<std::string> BundleSetTest::_storage_names;


CPPUNIT_TEST_SUITE_REGISTRATION(BundleSetTest);
void BundleSetTest::setUp()
{
	// create a new event switch
	esl = new ibrtest::EventSwitchLoop();
	/*// enable blob path
		ibrcommon::File blob_path("/tmp/blobs");

		// check if the BLOB path exists
		if (!blob_path.exists()) {
			// try to create the BLOB path
			ibrcommon::File::createDirectory(blob_path);
		}

		// enable the blob provider
		ibrcommon::BLOB::changeProvider(new ibrcommon::FileBLOBProvider(blob_path), true);*/
	switch(testCounter++){
	//MemoryBundleSet
	case 0: {
		_storage = new dtn::storage::MemoryBundleStorage();
		break;
	}

	//SQLiteBundleSet
#ifdef HAVE_SQLITE
	case 1: {
			ibrcommon::File path("/tmp/sqlite-bundleset-test");
			if (path.exists()) path.remove(true);
			ibrcommon::File::createDirectory(path);

			_storage = new dtn::storage::SQLiteBundleStorage(path, 0);

			break;

	}
#endif
	}


	if (testCounter >= _storage_names.size()) testCounter = 0;
	// start-up event switch
	esl->start();

	try {
		dtn::daemon::Component &c = dynamic_cast<dtn::daemon::Component&>(*_storage);
		c.initialize();
		c.startup();
	} catch (const bad_cast&) {
	}
}

void BundleSetTest::tearDown()
{
	esl->stop();

	try {
		dtn::daemon::Component &c = dynamic_cast<dtn::daemon::Component&>(*_storage);
		c.terminate();
	} catch (const bad_cast&) {
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

	l.add(b1);

	CPPUNIT_ASSERT(l.has(b1) == true);
	CPPUNIT_ASSERT(l.has(b2) == false);

	l.add(b2);

	CPPUNIT_ASSERT(l.has(b1) == true);
	CPPUNIT_ASSERT(l.has(b2) == true);

	l.clear();

	CPPUNIT_ASSERT(l.has(b1) == false);
	CPPUNIT_ASSERT(l.has(b2) == false);

	l.add(b1);

	CPPUNIT_ASSERT(l.has(b1) == true);
	CPPUNIT_ASSERT(l.has(b2) == false);

	l.add(b2);

	CPPUNIT_ASSERT(l.has(b1) == true);
	CPPUNIT_ASSERT(l.has(b2) == true);
}
void BundleSetTest::orderTest(){

	dtn::data::BundleSet l;

	CPPUNIT_ASSERT(l.size() == 0);

	genbundles(l, 50, 0, 500);
	genbundles(l, 50, 600, 1000);

	CPPUNIT_ASSERT(l.size() == 100);


	for (int i = 0; i < 550; ++i)
	{
		l.expire(i);
	}


	CPPUNIT_ASSERT(l.size() == 50);

	for (int i = 0; i < 1050; ++i)
	{
		l.expire(i);
	}

	CPPUNIT_ASSERT(l.size() == 0);

}
void BundleSetTest::genbundles(dtn::data::BundleSet &l, int number, int offset, int max)
{
	int range = max - offset;
	dtn::data::MetaBundle b;

	for (int i = 0; i < number; ++i)
	{
		int random_integer = offset + (rand() % range);

		b.lifetime = random_integer;
		b.expiretime = random_integer;

		b.timestamp = 1;
		b.sequencenumber = random_integer;

		stringstream ss; ss << rand();

		b.source = dtn::data::EID("dtn://node" + ss.str() + "/application");


		l.add(b);
	}
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
