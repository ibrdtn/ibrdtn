/*
 * TestBundleList.cpp
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

#include "data/TestBundleList.h"
#include <ibrdtn/utils/Clock.h>
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/EID.h>
#include <iostream>
#include <cstdlib>

CPPUNIT_TEST_SUITE_REGISTRATION (TestBundleList);

void TestBundleList::setUp()
{
}

void TestBundleList::tearDown()
{
}

TestBundleList::ExpiredBundleCounter::ExpiredBundleCounter()
 : counter(0)
{

}

TestBundleList::ExpiredBundleCounter::~ExpiredBundleCounter()
{
}

void TestBundleList::ExpiredBundleCounter::eventBundleExpired(const dtn::data::MetaBundle&) throw ()
{
	//std::cout << "Bundle expired " << b.bundle.toString() << std::endl;
	counter++;
}

void TestBundleList::genbundles(dtn::data::BundleList &l, int number, int offset, int max)
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

void TestBundleList::orderTest(void)
{
	ExpiredBundleCounter ebc;
	dtn::data::BundleList l(&ebc);

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

