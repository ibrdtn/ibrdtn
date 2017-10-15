/**
 * IPNDTest.cpp
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

#include "IPNDTest.h"
#include "net/DiscoveryBeacon.h"
#include "net/DiscoveryService.h"
#include "core/Node.h"
#include <sstream>
#include <arpa/inet.h>

// debugging
//#include <ibrcommon/data/Base64Stream.h>
//#include <ibrcommon/Logger.h>

CPPUNIT_TEST_SUITE_REGISTRATION(IPNDTest);
IPNDTest::IPNDTest()
	: param1(0), param2(0), param3(0), param4(0)
{
	// debugging
	//ibrcommon::Logger::enableBuffer(200);
	//ibrcommon::Logger::enableAsync(); // enable asynchronous logging feature (thread-safe)
	//ibrcommon::Logger::setVerbosity(40);
	//ibrcommon::Logger::addStream(std::cout, ibrcommon::Logger::LOGGER_DEBUG,
		//ibrcommon::Logger::LOG_DATETIME | ibrcommon::Logger::LOG_LEVEL |
		//ibrcommon::Logger::LOG_TAG);
}

void IPNDTest::setUp() {
	param1 = new dtn::net::IPServiceParam("198.51.100.23", 225); // c6 33 64 17, e1
	serv1 = dtn::net::DiscoveryService(dtn::core::Node::CONN_TCPIP, param1);

	param2 = new dtn::net::IPServiceParam("2001:DB8::255:A5", 1834); // 07 2a
	serv2 = dtn::net::DiscoveryService(dtn::core::Node::CONN_UDPIP, param2);
	
	param3 = new dtn::net::DatagramServiceParam("198.51.100.23");
	serv3 = dtn::net::DiscoveryService(dtn::core::Node::CONN_DGRAM_UDP, param3);

	param4 = new dtn::net::LOWPANServiceParam(0xeA5, 1337); // 0e a5, 05 39
	serv4 = dtn::net::DiscoveryService(dtn::core::Node::CONN_LOWPAN, param4);

	param5 = new dtn::net::EMailServiceParam("username@example.org");
	serv5 = dtn::net::DiscoveryService(dtn::core::Node::CONN_EMAIL, param5);

	param6 = new dtn::net::DHTServiceParam(2553, false); // 09 f9
	serv6 = dtn::net::DiscoveryService(dtn::core::Node::CONN_DHT, param6);

	param7 = new dtn::net::DTNTPServiceParam(1, 15.63, 1410492227);
	serv7 = dtn::net::DiscoveryService(dtn::core::Node::CONN_DTNTP, param7);
}

void IPNDTest::tearDown() {
	delete param1;
	delete param2;
	delete param3;
	delete param4;
	delete param5;
	delete param6;
}

void IPNDTest::testServiceInterface()
{
	dtn::net::IPServiceParam * p1 =
		dynamic_cast<dtn::net::IPServiceParam *>(param1);
	CPPUNIT_ASSERT(p1->_address == "198.51.100.23");
	CPPUNIT_ASSERT(p1->_port == 225);

	CPPUNIT_ASSERT(*param1 == *param1);
	CPPUNIT_ASSERT(*param2 == *param2);
	CPPUNIT_ASSERT(*param3 == *param3);
	CPPUNIT_ASSERT(*param4 == *param4);
	CPPUNIT_ASSERT(*param5 == *param5);
	CPPUNIT_ASSERT(*param6 == *param6);
	CPPUNIT_ASSERT(*param7 == *param7);

	(*param1) = (*param2);
	CPPUNIT_ASSERT(*param1 == *param2);

	dtn::net::DiscoveryService s(dtn::core::Node::CONN_TCPIP, param1);
	CPPUNIT_ASSERT(*(s.getParam()) == *param1);
	CPPUNIT_ASSERT(*(s.getParam()) == *param2);

	dtn::net::IPServiceParam p("192.168.12.42", 153);
	s.update(&p);
	CPPUNIT_ASSERT(*(s.getParam()) == p);
}

/** Test IPND service descriptions */
void IPNDTest::testSelfAcceptance(dtn::net::Discovery::Protocol version) {
	using namespace dtn::net;

	DiscoveryBeacon beacon(version, dtn::data::EID("dtn://test.dtn"));

	beacon.setSequencenumber(0xfffe);
	beacon.addService(serv1);
	beacon.addService(serv2);
	beacon.addService(serv3);
	beacon.addService(serv4);
	beacon.addService(serv5);
	beacon.addService(serv6);
	beacon.addService(serv7);

	// wrap bacon into a packet
	std::stringstream packet;
	packet << beacon;

	// for debugging: print byte representation
	//std::cout << std::endl << std::endl;
	//ibrcommon::Base64Stream b64(std::cout, false, 72);
	//b64 << packet.str();
	//b64.flush();
	//std::cout << std::endl << std::endl;

	// unwrap packet
	DiscoveryBeacon new_beacon;
	packet >> new_beacon;

	// check bacons before wrapping and after unwrapping
	DiscoveryBeacon::service_list& services = beacon.getServices();
	DiscoveryBeacon::service_list& read_services = new_beacon.getServices();

	CPPUNIT_ASSERT_EQUAL(beacon.getEID().getString(),
		new_beacon.getEID().getString());

	CPPUNIT_ASSERT_EQUAL(services.size(), read_services.size());

	DiscoveryBeacon::service_list::iterator it, it_new;
	for (it = services.begin(), it_new = read_services.begin();
	   !(it == services.end() || it_new == read_services.end());
	     it++, it_new++)
	{
		CPPUNIT_ASSERT(*it == *it_new);
	}
}

void IPNDTest::testDraft00SelfAcceptance()
{
	testSelfAcceptance(dtn::net::Discovery::DISCO_VERSION_00);
}

void IPNDTest::testDraft01SelfAcceptance()
{
	testSelfAcceptance(dtn::net::Discovery::DISCO_VERSION_01);
}

void IPNDTest::testDraft02SelfAcceptance()
{
	testSelfAcceptance(dtn::net::Discovery::DISCO_VERSION_02);
}
