/**
 * IPNDTest.h
 *
 * Copyright (C) 2014 IBR, TU Braunschweig
 *
 * Written-by: Roland Hieber <r.hieber@tu-bs.de>
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

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "net/DiscoveryBeacon.h"

#ifndef IPNDTEST_HH
#define IPNDTEST_HH

class IPNDTest : public CppUnit::TestFixture {
	dtn::net::DiscoveryServiceParam * param1,
		* param2,
		* param3,
		* param4,
		* param5,
		* param6,
		* param7;
	dtn::net::DiscoveryService serv1,
		serv2,
		serv3,
		serv4,
		serv5,
		serv6,
		serv7;

	public:
		IPNDTest();
		void testServiceInterface();
		void testSelfAcceptance(dtn::net::Discovery::Protocol version);
		void testDraft00SelfAcceptance();
		void testDraft01SelfAcceptance();
		void testDraft02SelfAcceptance();

		void setUp();
		void tearDown();

		CPPUNIT_TEST_SUITE(IPNDTest);
		CPPUNIT_TEST(testServiceInterface);
		CPPUNIT_TEST(testDraft00SelfAcceptance);
		CPPUNIT_TEST(testDraft01SelfAcceptance);
		CPPUNIT_TEST(testDraft02SelfAcceptance);
		CPPUNIT_TEST_SUITE_END();
};
#endif /* IPNDTEST_HH */

