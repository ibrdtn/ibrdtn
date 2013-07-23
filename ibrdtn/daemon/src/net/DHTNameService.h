/*
 * DHTNameService.h
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Till Lorentzen <lorenzte@ibr.cs.tu-bs.de>
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

#ifndef DHTNAMESERVICE_H_
#define DHTNAMESERVICE_H_

#define DHT_RESULTS_EXPIRE_TIMEOUT 600
#define DHT_PATH_EXPIRE_TIMEOUT 60
#define DHT_DISCOVERED_NODE_PRIORITY 3

#include "Component.h"
#include "Configuration.h"
#include "core/Event.h"
#include "core/EventReceiver.h"
#include "net/DiscoveryServiceProvider.h"

#include <ibrdtn/data/EID.h>

#include <ibrcommon/thread/Mutex.h>

#include <string>
#include <list>

extern "C" {
#include <dtndht/dtndht.h>
}

namespace dtn {
namespace dht {
/**
 * This Service provides a Lookup service for node and group EIDs in a bittorrent DHT.
 * It is fully event driven. It provides the following mechanisms:
 * Publishes the own EID to the DHT, also as specific neighbour EIDs.
 * Registers for Routing Events to look up automatically for the embedded EIDs in the DHT
 * Adds the found EID with its Convergence Layers as a Node.
 * Adds routes to the neighbour EIDs of the found EID.
 * Uses IPND to announce the DHT name service to local neighbour nodes.
 */
class DHTNameService: public dtn::daemon::IndependentComponent,
		public dtn::core::EventReceiver,
		public dtn::net::DiscoveryServiceProvider {
private:
	// All DHT configured sockets and needed setting are saved this structure
	struct dtn_dht_context _context;
	// Receiving buffer for DHT UDP packets
	unsigned char _buf[4096];
	// Helping pipe to interrupt the main loop select
	int _interrupt_pipe[2];
	// A little extra variable to signal main loop to exit on interrupt pipe
	bool _exiting;
	// true if the dht is correctly set up and ready for work
	bool _initialized;
	// true, if the dht was ready and I announced myself
	bool _announced;
	// A Mutex for the access to the dht lib
	ibrcommon::Mutex _libmutex;
	// Number of good and dubious DHT Nodes
	int _foundNodes;
	// A list of all lookups done before DHT is stable
	std::list<dtn::data::EID> cachedLookups;
	// Timestamp of the last bootstrapping call
	time_t _bootstrapped;
	// Configuration for the DHT
	const daemon::Configuration::DHT& _config;
	/**
	 * Looks up for a given EID in the DHT
	 */
	void lookup(const dtn::data::EID &eid);
	/**
	 * Announces the a Node on the DHT
	 */
	void announce(const dtn::core::Node &n, enum dtn_dht_lookup_type type);
	/**
	 * Deannounces the a Node from the DHT
	 */
	void deannounce(const dtn::core::Node &n);

	bool isNeighbourAnnouncable(const dtn::core::Node &n);
	/**
	 * Helper function to build set a given pipe to non blocking mode
	 */
	bool setNonBlockingInterruptPipe(void);
	/**
	 * Helper function to get the right name of a given Convergence Layer
	 * It is necessary to publish the correct key to the DHT
	 */
	std::string getConvergenceLayerName(
			const dtn::daemon::Configuration::NetConfig &net);
	/**
	 * Sends a DHT Ping and adds the answering node to the DHT
	 */
	void pingNode(const dtn::core::Node &n);
	/**
	 * Extract all known IP Addresses out of the given Node
	 */
	std::list<std::string> getAllIPAddresses(const dtn::core::Node &n);

	/**
	 * Bootstraps the DHT by the configured method
	 */
	void bootstrapping();

	/**
	 * Bootstrapping from nodes from last session, saved in a file
	 */
	void bootstrappingFile();

	/**
	 * Bootstrapping from the given domains by requesting dns
	 */
	void bootstrappingDNS();

	/**
	 * Bootstrapps from the given IP addresses in the config file
	 */
	void bootstrappingIPs();

public:
	DHTNameService();
	virtual ~DHTNameService();
	/**
	 * Returns the name of this Service: "DHT Naming Service"
	 */
	const std::string getName() const;
	/**
	 * Handles all incomming Events, for which the Service is registered
	 * It is registered for this types of events:
	 * 	dtn::routing::QueueBundleEvent
	 * 	dtn::core::NodeEvent
	 */
	void raiseEvent(const dtn::core::Event *evt) throw ();
	/**
	 * this method updates the given values for discovery service
	 * It publishes the port number of the DHT instance
	 */
	void update(const ibrcommon::vinterface &iface, DiscoveryAnnouncement &announcement)
			throw (dtn::net::DiscoveryServiceProvider::NoServiceHereException);


protected:
	/**
	 * returns true
	 */
	void __cancellation() throw ();
	/**
	 * Reads the configured settings for the DHT and
	 * initializes all needed sockets(IPv4&IPv6) for the DHT
	 */
	void componentUp() throw ();
	/**
	 * Runs the bootstrapping method and starts the DHT.
	 * Executes the main loop with dtn_dht_periodic.
	 * After exiting the main loop, the DHT is shut down.
	 */
	void componentRun() throw ();
	/**
	 * Calls the interrupt pipe to exit the main loop.
	 * And so ends up the main loop indirectly
	 */
	void componentDown() throw ();
};
}
}

#endif /* DHTNAMESERVICE_H_ */
