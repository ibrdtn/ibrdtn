/* $Id: templateengine.py 2241 2006-05-22 07:58:58Z fischer $ */

///
/// @file        ConfigurationTest.cpp
/// @brief       CPPUnit-Tests for class Configuration
/// @author      Author Name (email@mail.address)
/// @date        Created at 2010-11-01
/// 
/// @version     $Revision: 2241 $
/// @note        Last modification: $Date: 2006-05-22 09:58:58 +0200 (Mon, 22 May 2006) $
///              by $Author: fischer $
///

/*
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

#include "ConfigurationTest.hh"
#include <fstream>


CPPUNIT_TEST_SUITE_REGISTRATION(ConfigurationTest);

/*========================== tests below ==========================*/

/*=== BEGIN tests for class 'Configuration' ===*/

void ConfigurationTest::testParams()
{
	/* test signature (int argc, char *argv[]) */

	char name[5] = "test";
	char param[3] = "-d";
	char value[3] = "99";

	char *argv[3];
	argv[0] = (char*)&name;
	argv[1] = (char*)param;
	argv[2] = (char*)value;

	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	conf.params(3, argv);
}

void ConfigurationTest::testGetNodename()
{
	/* test signature () */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(std::string("dtn://node.dtn"), conf.getNodename());
}

void ConfigurationTest::testGetPath()
{
	/* test signature (string name) */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(ibrcommon::File("/tmp").getPath(), conf.getPath("blob").getPath());
}

void ConfigurationTest::testDoAPI()
{
	/* test signature () */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(conf.doAPI(), true);
}

void ConfigurationTest::testGetAPIInterface()
{
	/* test signature () */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(conf.getAPIInterface().name, std::string("api"));
}

void ConfigurationTest::testGetAPISocket()
{
	/* test signature () */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_THROW(conf.getAPISocket(), dtn::daemon::Configuration::ParameterNotSetException);
}

void ConfigurationTest::testVersion()
{
	/* test signature () */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT(conf.version() != "0.0.0");
}

void ConfigurationTest::testGetStorage()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(std::string("default"), conf.getStorage());
}

void ConfigurationTest::testGetLimit()
{
	/* test signature (std::string) */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL((dtn::data::Length)0, conf.getLimit("test"));
	CPPUNIT_ASSERT_EQUAL((dtn::data::Length)20000000, conf.getLimit("storage"));
}

/*=== BEGIN tests for class 'Discovery' ===*/

void ConfigurationTest::testDiscoveryEnabled()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(true, conf.getDiscovery().enabled());
}

void ConfigurationTest::testDiscoveryAnnounce()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(true, conf.getDiscovery().announce());
}

void ConfigurationTest::testDiscoveryShortbeacon()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(false, conf.getDiscovery().shortbeacon());
}

void ConfigurationTest::testDiscoveryVersion()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(2, conf.getDiscovery().version());
}

void ConfigurationTest::testDiscoveryAddress()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(std::string("[224.0.0.1]:4551"), conf.getDiscovery().address().begin()->toString());
}

void ConfigurationTest::testDiscoveryPort()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(4551, conf.getDiscovery().port());
}

void ConfigurationTest::testDiscoveryInterval()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL((unsigned int)5, conf.getDiscovery().interval());
}

/*=== END   tests for class 'Discovery' ===*/

/*=== BEGIN tests for class 'Debug' ===*/

void ConfigurationTest::testDebugLevel()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(0, conf.getDebug().level());
}

void ConfigurationTest::testDebugEnabled()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(false, conf.getDebug().enabled());
}

void ConfigurationTest::testDebugQuiet()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(false, conf.getDebug().quiet());
}

/*=== END   tests for class 'Debug' ===*/

/*=== BEGIN tests for class 'Logger' ===*/

void ConfigurationTest::testQuiet()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(false, conf.getLogger().quiet());
}

void ConfigurationTest::testOptions()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL((unsigned int)0, conf.getLogger().options());
}

void ConfigurationTest::testOutput()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(&std::cout, &conf.getLogger().output());
}

/*=== END   tests for class 'Logger' ===*/

/*=== BEGIN tests for class 'Network' ===*/

void ConfigurationTest::testGetInterfaces()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL((size_t)1, conf.getNetwork().getInterfaces().size());
}

void ConfigurationTest::testGetStaticNodes()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL((size_t)2, conf.getNetwork().getStaticNodes().size());
}

void ConfigurationTest::testGetStaticRoutes()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL((size_t)1, conf.getNetwork().getStaticRoutes().size());
}

void ConfigurationTest::testGetRoutingExtension()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(dtn::daemon::Configuration::DEFAULT_ROUTING, conf.getNetwork().getRoutingExtension());
}

void ConfigurationTest::testDoForwarding()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(true, conf.getNetwork().doForwarding());
}

void ConfigurationTest::testDoAcceptNonsingleton()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(true, conf.getNetwork().doAcceptNonSingleton());
}

void ConfigurationTest::testGetTCPOptionNoDelay()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(true, conf.getNetwork().getTCPOptionNoDelay());
}

void ConfigurationTest::testGetTCPChunkSize()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL((dtn::data::Length)1024, conf.getNetwork().getTCPChunkSize());
}

/*=== END   tests for class 'Network' ===*/

void ConfigurationTest::testGetDiscovery()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(true, conf.getDiscovery().enabled());
}

void ConfigurationTest::testGetDebug()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(false, conf.getDebug().enabled());
}

void ConfigurationTest::testGetLogger()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(false, conf.getLogger().quiet());
}

void ConfigurationTest::testGetNetwork()
{
	/* test signature () const */
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
	CPPUNIT_ASSERT_EQUAL(true, conf.getNetwork().doForwarding());
}

/*=== END   tests for class 'Configuration' ===*/

void ConfigurationTest::setUp()
{
	ibrcommon::File _tmp("/tmp/dtnd.config");

	std::fstream stream(_tmp.getPath().c_str(), std::ios::out);
	stream << "local_uri = dtn://node.dtn" << std::endl
		<< "blob_path = /tmp" << std::endl
		<< "storage_path = /var/spool/ibrdtn/bundles" << std::endl
		<< "limit_blocksize = 1.3G" << std::endl
		<< "limit_storage = 20M" << std::endl
		<< "" << std::endl
		<< "statistic_type = stdout" << std::endl
		<< "statistic_interval = 2" << std::endl
		<< "statistic_file = /tmp/ibrdtn.stats" << std::endl
		<< "statistic_address = 127.0.0.1" << std::endl
		<< "statistic_port = 1234" << std::endl
		<< "net_interfaces = lan0" << std::endl
		<< "net_lan0_type = tcp					# we want to use TCP as protocol" << std::endl
		<< "net_lan0_interface = eth0			# listen on interface eth0" << std::endl
		<< "net_lan0_port = 4556				# with port 4556 (default)" << std::endl
		<< "" << std::endl
		<< "net_lan1_type = udp				# we want to use UDP as protocol" << std::endl
		<< "net_lan1_interface = eth0			# listen on interface eth0 " << std::endl
		<< "net_lan1_port = 4556				# with port 4556 (default)" << std::endl
		<< "discovery_address = 224.0.0.1" << std::endl
		<< "discovery_timeout = 5" << std::endl
		<< "tcp_nodelay = yes" << std::endl
		<< "tcp_chunksize = 1024" << std::endl
		<< "" << std::endl
		<< "routing = default" << std::endl
		<< "routing_forwarding = yes" << std::endl
		<< "" << std::endl
		<< "route1 = dtn://* dtn://router.dtn" << std::endl
		<< "" << std::endl
		<< "static1_address = 10.0.0.5" << std::endl
		<< "static1_port = 4556" << std::endl
		<< "static1_uri = dtn://node-five.dtn" << std::endl
		<< "static1_proto = tcp" << std::endl
		<< "static2_address = 192.168.0.10" << std::endl
		<< "static2_port = 4556" << std::endl
		<< "static2_uri = dtn://node-ten.dtn" << std::endl
		<< "static2_proto = udp" << std::endl;
	stream.close();

	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance(true);
	conf.load(_tmp.getPath());
}

void ConfigurationTest::tearDown()
{
	ibrcommon::File("/tmp/dtnd.config").remove();
}
