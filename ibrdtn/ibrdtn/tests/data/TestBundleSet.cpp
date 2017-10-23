/*
 * TestBundleSet.cpp
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

#include "data/TestBundleSet.h"
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/MetaBundle.h>
#include <ibrdtn/data/EID.h>
#include <ibrdtn/utils/Clock.h>
#include <iostream>
#include <cstdlib>

CPPUNIT_TEST_SUITE_REGISTRATION (TestBundleSet);

void TestBundleSet::setUp()
{
}

void TestBundleSet::tearDown()
{
}

TestBundleSet::ExpiredBundleCounter::ExpiredBundleCounter()
 : counter(0)
{

}

TestBundleSet::ExpiredBundleCounter::~ExpiredBundleCounter()
{
}

void TestBundleSet::ExpiredBundleCounter::eventBundleExpired(const dtn::data::MetaBundle&) throw ()
{
	//std::cout << "Bundle expired " << b.bundle.toString() << std::endl;
	counter++;
}

void TestBundleSet::genbundles(dtn::data::BundleSet &l, int number, int offset, int max)
{
	int range = max - offset;
	dtn::data::Bundle b;

	for (int i = 0; i < number; ++i)
	{
		int random_integer = offset + (rand() % range);

		b.lifetime = random_integer;
		b.timestamp = 1;
		b.sequencenumber = random_integer;

		std::stringstream ss; ss << rand();

		b.source = dtn::data::EID("dtn://node" + ss.str() + "/application");

		l.add(dtn::data::MetaBundle::create(b));
	}
}

void TestBundleSet::containTest(void)
{
	ExpiredBundleCounter ebc;
	dtn::data::BundleSet l(&ebc);

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

void TestBundleSet::orderTest(void)
{
	ExpiredBundleCounter ebc;
	dtn::data::BundleSet l(&ebc);

	CPPUNIT_ASSERT(ebc.counter == 0);

	genbundles(l, 1000, 0, 500);
	genbundles(l, 1000, 600, 1000);

	for (int i = 0; i < 550; ++i)
	{
		l.expire(i);
	}

	CPPUNIT_ASSERT(ebc.counter == 1000);

	for (int i = 0; i < 1050; ++i)
	{
		l.expire(i);
	}

	CPPUNIT_ASSERT(ebc.counter == 2000);
}

void TestBundleSet::copyTest(void)
{
	dtn::data::BundleSet l;

	dtn::data::Bundle b;
	b.lifetime = 3600;
	b.timestamp = 449917232;
	b.sequencenumber = 0;

	dtn::data::MetaBundle meta = dtn::data::MetaBundle::create(b);
	l.add(meta);

	std::stringstream ss;
	ss << l;
	ss.clear();
	ss.seekg(0, std::stringstream::beg);

	dtn::data::BundleSet h;
	ss >> h;

	dtn::data::BundleSet r;
	r = h;

	CPPUNIT_ASSERT(meta.isIn(r.getBloomFilter()));
}
