/* $Id: templateengine.py 2241 2006-05-22 07:58:58Z fischer $ */

///
/// @file        BaseRouterTest.cpp
/// @brief       CPPUnit-Tests for class BaseRouter
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

#include "BaseRouterTest.hh"
#include "routing/RoutingExtension.h"
#include "routing/BaseRouter.h"
#include "storage/BundleStorage.h"
#include "core/Node.h"
#include "../tools/EventSwitchLoop.h"
#include "net/BundleReceivedEvent.h"
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/EID.h>
#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/Logger.h>


CPPUNIT_TEST_SUITE_REGISTRATION(BaseRouterTest);

/*========================== tests below ==========================*/

/*=== BEGIN tests for class 'BaseRouter' ===*/
/*=== BEGIN tests for class 'Extension' ===*/
void BaseRouterTest::testGetRouter()
{
	class ExtensionTest : public dtn::routing::RoutingExtension
	{
	public:
		ExtensionTest() {};
		~ExtensionTest() {};

		void componentUp() noexcept {};
		void componentDown() noexcept {};


		dtn::routing::BaseRouter& testGetRouter()
		{
			return **this;
		};
	};

	dtn::routing::BaseRouter router;
	ExtensionTest e;
	CPPUNIT_ASSERT_EQUAL(&router, &e.testGetRouter());
}

/*=== END   tests for class 'Extension' ===*/

void BaseRouterTest::testAddExtension()
{
	/* test signature (BaseRouter::Extension *extension) */
	class ExtensionTest : public dtn::routing::RoutingExtension
	{
	public:
		ExtensionTest() {};
		~ExtensionTest() {};

		void componentUp() noexcept {};
		void componentDown() noexcept {};

		dtn::routing::BaseRouter& testGetRouter()
		{
			return **this;
		};
	};

	dtn::routing::BaseRouter router;
	ExtensionTest *e = new ExtensionTest();
	router.add(e);
}

void BaseRouterTest::testTransferTo()
{
	/* test signature (BaseRouter::Extension *extension) */
	class ExtensionTest : public dtn::routing::RoutingExtension
	{
	public:
		ExtensionTest() {};
		~ExtensionTest() {};

		void componentUp() noexcept {};
		void componentDown() noexcept {};

		void testTransfer(const dtn::data::EID &destination, const dtn::data::MetaBundle &meta)
		{
			transferTo(destination, meta, dtn::core::Node::CONN_UNDEFINED);
		}
	};

	class ConvergenceLayerTest : public dtn::net::ConvergenceLayer
	{
	public:
		ConvergenceLayerTest() {};
		~ConvergenceLayerTest() {};

		dtn::core::Node::Protocol getDiscoveryProtocol() const
		{
			return dtn::core::Node::CONN_TCPIP;
		}

		void queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job)
		{
		}
	};

	/* test signature (const dtn::data::EID &destination, const dtn::data::BundleID &id) */
	dtn::data::Bundle b;
	dtn::routing::BaseRouter router;

	// create a convergence layer
	ConvergenceLayerTest cl;

	dtn::core::BundleCore::getInstance().getConnectionManager().add(&cl);

	// EID of the neighbor to test
	dtn::data::EID neighbor("dtn://no-neighbor");

	ExtensionTest *ex = new ExtensionTest();
	router.add(ex);
	router.initialize();

	dtn::core::Node n(neighbor);
	n.add(dtn::core::Node::URI(dtn::core::Node::NODE_CONNECTED, dtn::core::Node::CONN_TCPIP, ""));
	dtn::core::BundleCore::getInstance().getConnectionManager().add(n);

	// get neighbor database
	dtn::routing::NeighborDatabase &db = router.getNeighborDB();

	// create the neighbor in the neighbor database
	{
		ibrcommon::MutexLock l(db);
		db.create(neighbor);
	}

	try {
		ex->testTransfer(neighbor, dtn::data::MetaBundle::create(b));
		router.terminate();
	} catch (const ibrcommon::Exception &ex) {
		std::cout << ex.what() << std::endl;
		router.terminate();
		throw;
	}

	dtn::core::BundleCore::getInstance().getConnectionManager().remove(n);
	dtn::core::BundleCore::getInstance().getConnectionManager().remove(&cl);
}

void BaseRouterTest::testRaiseEvent()
{
	/* test signature (const dtn::core::Event *evt) */
	dtn::data::EID eid("dtn://no-neighbor");
	dtn::data::Bundle b;
	b.source = dtn::data::EID("dtn://testcase-one/foo");

	ibrtest::EventSwitchLoop esl; esl.start();
	dtn::routing::BaseRouter router;
	router.initialize();

	CPPUNIT_ASSERT_EQUAL(false, router.isKnown(b));

	// send a bundle
	dtn::net::BundleReceivedEvent::raise(eid, b, false);

	// stop the event switch
	esl.stop();

	// ... and wait until all events are processed
	esl.join();

	// this bundle has to be known in future
	CPPUNIT_ASSERT_EQUAL(true, router.isKnown(b));

	router.terminate();
}

void BaseRouterTest::testGetStorage()
{
	/* test signature () */
	dtn::routing::BaseRouter router;
	CPPUNIT_ASSERT_EQUAL((dtn::storage::BundleStorage*)&_storage, &router.getStorage());
}

void BaseRouterTest::testIsKnown()
{
	/* test signature (const dtn::data::BundleID &id) */
	dtn::routing::BaseRouter router;

	dtn::data::Bundle b;
	b.source = dtn::data::EID("dtn://testcase-one/foo");

	CPPUNIT_ASSERT_EQUAL(false, router.isKnown(b));

	router.setKnown(dtn::data::MetaBundle::create(b));

	CPPUNIT_ASSERT_EQUAL(true, router.isKnown(b));
}

void BaseRouterTest::testSetKnown()
{
	/* test signature (const dtn::data::MetaBundle &meta) */
	dtn::routing::BaseRouter router;

	dtn::data::Bundle b;
	b.source = dtn::data::EID("dtn://testcase-one/foo");

	router.setKnown(dtn::data::MetaBundle::create(b));
	CPPUNIT_ASSERT_EQUAL(true, router.isKnown(b));
}

void BaseRouterTest::testGetSummaryVector()
{
	/* test signature () */
	dtn::routing::BaseRouter router;

	dtn::data::Bundle b;
	b.source = dtn::data::EID("dtn://testcase-one/foo");

	CPPUNIT_ASSERT_EQUAL(false, router.isKnown(b));

	router.setKnown(dtn::data::MetaBundle::create(b));

	CPPUNIT_ASSERT_EQUAL(true, router.getKnownBundles().has(b));
}

/*=== END   tests for class 'BaseRouter' ===*/

void BaseRouterTest::setUp()
{
	_storage.clear();
	dtn::core::BundleCore::getInstance().setStorage(&_storage);
}

void BaseRouterTest::tearDown()
{
	dtn::core::BundleCore::getInstance().setStorage(NULL);
}

