/*
 * DatagramClTest.cpp
 *
 *  Created on: 03.05.2013
 *      Author: morgenro
 */

#include "DatagramClTest.h"
#include "../tools/TestEventListener.h"
#include "storage/MemoryBundleStorage.h"
#include "core/NodeEvent.h"
#include "net/TransferCompletedEvent.h"

#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/EID.h>
#include <ibrcommon/thread/Thread.h>
#include <ibrdtn/utils/Clock.h>
#include "core/BundleCore.h"
#include <ibrcommon/data/File.h>
#include <ibrcommon/data/BLOB.h>
#include <ibrcommon/thread/MutexLock.h>
#include <ibrdtn/data/PayloadBlock.h>
#include <ibrdtn/data/AgeBlock.h>
#include "Component.h"

#include <unistd.h>

CPPUNIT_TEST_SUITE_REGISTRATION(DatagramClTest);

dtn::storage::BundleStorage* DatagramClTest::_storage = NULL;

void DatagramClTest::setUp() {
	// create a new event switch
	_esl = new ibrtest::EventSwitchLoop();

	// enable blob path
	ibrcommon::File blob_path("/tmp/blobs");

	// check if the BLOB path exists
	if (!blob_path.exists()) {
		// try to create the BLOB path
		ibrcommon::File::createDirectory(blob_path);
	}

	// enable the blob provider
	ibrcommon::BLOB::changeProvider(new ibrcommon::FileBLOBProvider(blob_path), true);

	// add standard memory base storage
	_storage = new dtn::storage::MemoryBundleStorage();

	// make storage globally available
	dtn::core::BundleCore::getInstance().setStorage(_storage);
	dtn::core::BundleCore::getInstance().setSeeker(_storage);

	// create fake datagram service
	_fake_service = new FakeDatagramService();
	_fake_cl = new DatagramConvergenceLayer( _fake_service );

	// add convergence layer to bundle core
	dtn::core::BundleCore::getInstance().getConnectionManager().add(_fake_cl);

	// initialize BundleCore
	dtn::core::BundleCore::getInstance().initialize();

	// start-up event switch
	_esl->start();

	_fake_cl->initialize();

	try {
		dtn::daemon::Component &c = dynamic_cast<dtn::daemon::Component&>(*_storage);
		c.initialize();
	} catch (const std::bad_cast&) {
	}

	// startup BundleCore
	dtn::core::BundleCore::getInstance().startup();

	_fake_cl->startup();

	try {
		dtn::daemon::Component &c = dynamic_cast<dtn::daemon::Component&>(*_storage);
		c.startup();
	} catch (const std::bad_cast&) {
	}
}

void DatagramClTest::tearDown() {
	_esl->stop();

	_fake_cl->terminate();

	try {
		dtn::daemon::Component &c = dynamic_cast<dtn::daemon::Component&>(*_storage);
		c.terminate();
	} catch (const std::bad_cast&) {
	}

	// shutdown BundleCore
	dtn::core::BundleCore::getInstance().terminate();

	// add convergence layer to bundle core
	dtn::core::BundleCore::getInstance().getConnectionManager().remove(_fake_cl);

	delete _fake_cl;
	_fake_cl = NULL;

	_esl->join();
	delete _esl;
	_esl = NULL;

	// delete storage
	delete _storage;
}

void DatagramClTest::discoveryTest() {
	TestEventListener<dtn::core::NodeEvent> evtl;

	const std::set<dtn::core::Node> pre_disco_nodes = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();
	CPPUNIT_ASSERT_EQUAL((size_t)0, pre_disco_nodes.size());

	// send fake discovery beacon
	_fake_service->fakeDiscovery();

	// wait until the beacon has been processes
	ibrcommon::MutexLock l(evtl.event_cond);
	while (evtl.event_counter == 0) evtl.event_cond.wait();

	const std::set<dtn::core::Node> post_disco_nodes = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();
	CPPUNIT_ASSERT_EQUAL((size_t)1, post_disco_nodes.size());
}

void DatagramClTest::queueTest() {
	// create a new bundle
	dtn::data::Bundle b;

	// set standard variable.sourceurce = dtn::data::EID("dtn://node-one/test");
	b.lifetime = 1;
	b.destination = dtn::data::EID("dtn://node-two/test");

	// add some payload
	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
	b.push_back(ref);

	for (int i = 0; i < 1000; ++i)
		(*ref.iostream()) << "Hallo Welt" << std::endl;

	{
		dtn::data::AgeBlock &agebl = b.push_back<dtn::data::AgeBlock>();
		agebl.setSeconds(42);
	}

	// store the bundle
	_storage->store(b);

	// special case for caching storages (SimpleBundleStorage)
	// wait until the bundle is written
	_storage->wait();

	const dtn::data::MetaBundle id = dtn::data::MetaBundle::create(b);

	TestEventListener<dtn::core::NodeEvent> node_evtl;
	TestEventListener<dtn::net::TransferCompletedEvent> completed_evtl;

	// send fake discovery beacon
	_fake_service->fakeDiscovery();

	// wait until the beacon has been processes
	try {
		ibrcommon::MutexLock l(node_evtl.event_cond);
		while (node_evtl.event_counter == 0) node_evtl.event_cond.wait(20000);
	} catch (const ibrcommon::Conditional::ConditionalAbortException&) {
		CPPUNIT_FAIL("discovery - timeout reached");
	}

	const std::set<dtn::core::Node> nodes = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();

	// check the number of nodes before accessing the first one
	CPPUNIT_ASSERT_EQUAL((size_t)1, nodes.size());

	const dtn::core::Node &n = (*nodes.begin());

	// create BundleTransfer in a separate scope because the
	// TransferCompletedEvent is only raised after all objects
	// are destroyed
	{
		// create a job
		const dtn::net::BundleTransfer job(n.getEID(), id, dtn::core::Node::CONN_UNDEFINED);

		// send fake discovery beacon
		_fake_cl->queue(n, job);
	}

	// wait until the bundle has been transmitted
	try {
		ibrcommon::MutexLock l(completed_evtl.event_cond);
		while (completed_evtl.event_counter == 0) completed_evtl.event_cond.wait(20000);
	} catch (const ibrcommon::Conditional::ConditionalAbortException&) {
		CPPUNIT_FAIL("completed - timeout reached");
	}

	CPPUNIT_ASSERT_EQUAL((unsigned int)1, completed_evtl.event_counter);
}
