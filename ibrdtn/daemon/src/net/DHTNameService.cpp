/*
 * DHTNameService.cpp
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

#include "config.h"

#include "DHTNameService.h"
#include "ibrcommon/net/vsocket.h"
#include "core/EventDispatcher.h"
#include "core/BundleCore.h"

#include <ibrdtn/utils/Utils.h>

#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/MutexLock.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <set>
#include <cstdlib>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

dtn::dht::DHTNameService::DHTNameService() :
	_exiting(false), _initialized(false), _announced(false), _foundNodes(-1),
			_bootstrapped(0),
			_config(daemon::Configuration::getInstance().getDHT()) {
}

dtn::dht::DHTNameService::~DHTNameService() {
	join();
}

const std::string dtn::dht::DHTNameService::getName() const {
	return "DHT Naming Service";
}

void dtn::dht::DHTNameService::__cancellation() throw () {
}

bool dtn::dht::DHTNameService::setNonBlockingInterruptPipe() {
	for (int i = 0; i < 2; ++i) {
		int opts;
		opts = fcntl(_interrupt_pipe[i], F_GETFL);
		if (opts < 0) {
			return false;
		}
		opts |= O_NONBLOCK;
		if (fcntl(_interrupt_pipe[i], F_SETFL, opts) < 0) {
			return false;
		}
	}
	return true;
}

void dtn::dht::DHTNameService::componentUp() throw () {
	// register as discovery beacon handler
	dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(this);

	// creating interrupt pipe
	if (pipe(_interrupt_pipe) < 0) {
		IBRCOMMON_LOGGER_TAG("DHTNameService", error) << "Error " << errno << " creating pipe"
					<< IBRCOMMON_LOGGER_ENDL;
		return;
	}
	// set the pipe to non-blocking
	if (!this->setNonBlockingInterruptPipe()) {
		IBRCOMMON_LOGGER_TAG("DHTNameService", error) << "Error " << errno
					<< " setting pipe to non-blocking mode"
					<< IBRCOMMON_LOGGER_ENDL;
		return;
	}
	// init DHT
	int rc = dtn_dht_initstruct(&_context);
	if (rc < 0) {
		IBRCOMMON_LOGGER_TAG("DHTNameService", error) << "DHT Context creation failed: " << rc
					<< IBRCOMMON_LOGGER_ENDL;
		return;
	}
	if (_config.randomPort()) {
		srand( time(NULL));
		_context.port = rand() % 64000 + 1024;
	} else {
		_context.port = _config.getPort();
	}
	if (_config.getIPv4Binding().size() > 0) {
		_context.bind = _config.getIPv4Binding().c_str();
	}
	if (_config.getIPv6Binding().size() > 0) {
		_context.bind6 = _config.getIPv6Binding().c_str();
	}
	if (_config.isIPv4Enabled() && _config.isIPv6Enabled()) {
		_context.type = BINDBOTH;
	} else if (_config.isIPv4Enabled()) {
		_context.type = IPV4ONLY;
	} else if (_config.isIPv6Enabled()) {
		_context.type = IPV6ONLY;
	} else {
		_context.type = BINDNONE;
	}
	std::string myid = _config.getID();
	if (!_config.randomID()) {
		dtn_dht_build_id_from_str(_context.id, myid.c_str(), myid.size());
	}

	rc = dtn_dht_init_sockets(&_context);
	if (rc != 0) {
		if(_context.type == BINDBOTH) {
			if(rc == -2){
				IBRCOMMON_LOGGER_TAG("DHTNameService", warning) << "DHT IPv6 socket couldn't be initialized"
								<<IBRCOMMON_LOGGER_ENDL;
			} else {
				IBRCOMMON_LOGGER_TAG("DHTNameService", warning) << "DHT socket(s) couldn't be initialized"
												<<IBRCOMMON_LOGGER_ENDL;
			}
			dtn_dht_close_sockets(&_context);
			// Try only IPv4
			_context.type = IPV4ONLY;
			rc = dtn_dht_init_sockets(&_context);
			if(rc != 0){
				IBRCOMMON_LOGGER_TAG("DHTNameService", warning) << "DHT IPv4 socket couldn't be initialized"
							<<IBRCOMMON_LOGGER_ENDL;
				dtn_dht_close_sockets(&_context);
				// Try only IPv6
				_context.type = IPV6ONLY;
				rc = dtn_dht_init_sockets(&_context);
				if(rc!=0){
					IBRCOMMON_LOGGER_TAG("DHTNameService", warning) << "DHT IPv6 socket couldn't be initialized"
								<<IBRCOMMON_LOGGER_ENDL;
					IBRCOMMON_LOGGER_TAG("DHTNameService", error) << "DHT Sockets couldn't be initialized"
										<<IBRCOMMON_LOGGER_ENDL;
					dtn_dht_close_sockets(&_context);
					return;
				}
			}
		} else {
			IBRCOMMON_LOGGER_TAG("DHTNameService", error) << "DHT Sockets couldn't be initialized"
					<<IBRCOMMON_LOGGER_ENDL;
			dtn_dht_close_sockets(&_context);
			return;
		}
	}
	IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 50) << "Sockets for DHT initialized"
				<<IBRCOMMON_LOGGER_ENDL;
	if (!_config.isBlacklistEnabled()) {
		IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 50) << "DHT Blacklist disabled"
					<<IBRCOMMON_LOGGER_ENDL;
		dtn_dht_blacklist(0);
	} else {
		IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 50) << "DHT Blacklist enabled"
					<<IBRCOMMON_LOGGER_ENDL;
	}
	{
		ibrcommon::MutexLock l(this->_libmutex);
		rc = dtn_dht_init(&_context);
	}
	if (rc < 0) {
		IBRCOMMON_LOGGER_TAG("DHTNameService", error) << "DHT initialization failed"
					<< IBRCOMMON_LOGGER_ENDL;
		return;
	}
	IBRCOMMON_LOGGER_TAG("DHTNameService", info) << "DHT initialized on Port: " << _context.port
				<< " with ID: " << myid << IBRCOMMON_LOGGER_ENDL;
	this->_initialized = true;
	// Warning about possibly wrong configuration
	if(_config.isNeighbourAnnouncementEnabled() &&
			!daemon::Configuration::getInstance().getNetwork().doForwarding()) {
		IBRCOMMON_LOGGER_TAG("DHTNameService", warning) << "DHT will not announce neighbor nodes: "
				<<  "announcement enabled but routing forwarding is disabled"
				<< IBRCOMMON_LOGGER_ENDL;
	}
}

void dtn::dht::DHTNameService::componentRun() throw () {
	if (!this->_initialized) {
		IBRCOMMON_LOGGER_TAG("DHTNameService", error) << "DHT is not initialized"
					<<IBRCOMMON_LOGGER_ENDL;
		return;
	}
	std::string cltype_;
	std::string port_;
	const std::list<dtn::daemon::Configuration::NetConfig>
			&nets =
					dtn::daemon::Configuration::getInstance().getNetwork().getInterfaces();

	// for every available interface, build the correct struct
	for (std::list<dtn::daemon::Configuration::NetConfig>::const_iterator iter =
			nets.begin(); iter != nets.end(); ++iter) {
		try {
			cltype_ = getConvergenceLayerName((*iter));
			std::stringstream ss;
			ss << (*iter).port;
			port_ = ss.str();
			struct dtn_convergence_layer * clstruct =
					(struct dtn_convergence_layer*) malloc(
							sizeof(struct dtn_convergence_layer));
			clstruct->clname = (char*) malloc(cltype_.size());
			clstruct->clnamelen = cltype_.size();
			memcpy(clstruct->clname, cltype_.c_str(), cltype_.size());
			struct dtn_convergence_layer_arg * arg =
					(struct dtn_convergence_layer_arg*) malloc(
							sizeof(struct dtn_convergence_layer_arg));
			arg->key = (char*) malloc(5);
			arg->key[4] = '\n';

#ifdef HAVE_VMIME
			if(cltype_ == "email") {
				memcpy(arg->key, "email", 5);
				arg->keylen = 5;
				const std::string &email =
					dtn::daemon::Configuration::getInstance().getEMail().getOwnAddress();
				arg->value = (char*) malloc(email.size());
				memcpy(arg->value, email.c_str(), email.size());
				arg->valuelen = email.size();
			}else{
#endif

			memcpy(arg->key, "port", 4);
			arg->keylen = 4;
			arg->value = (char*) malloc(port_.size());
			memcpy(arg->value, port_.c_str(), port_.size());
			arg->valuelen = port_.size();

#ifdef HAVE_VMIME
			}
#endif

			arg->next = NULL;
			clstruct->args = arg;
			clstruct->next = _context.clayer;
			_context.clayer = clstruct;
		} catch (const std::exception&) {
			continue;
		}
	}

	// Bootstrapping the DHT
	int rc, numberOfRandomRequests = 0;
	dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::add(this);
	dtn::core::EventDispatcher<dtn::core::NodeEvent>::add(this);

	// DHT main loop
	time_t tosleep = 0;
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(sockaddr_storage);
	::memset(&from, 0, fromlen);

	while (!this->_exiting) {
		if (this->_foundNodes == 0) {
			bootstrapping();
		}
		//Announce myself
		if (!this->_announced && dtn_dht_ready_for_work(&_context) > 2) {
			this->_announced = true;
			if (!_config.isSelfAnnouncingEnabled())
				return;
			announce(dtn::core::BundleCore::local, SINGLETON);
			// Read all unknown EIDs and lookup them
			dtn::core::BundleCore &core = dtn::core::BundleCore::getInstance();
			dtn::storage::BundleStorage & storage = core.getStorage();
			std::set<dtn::data::EID> eids_ = storage.getDistinctDestinations();
			std::set<dtn::data::EID>::iterator iterator;
			for (iterator = eids_.begin(); iterator != eids_.end(); ++iterator) {
				lookup(*iterator);
			}
			std::set<dtn::core::Node> neighbours_ = core.getConnectionManager().getNeighbors();
			std::set<dtn::core::Node>::iterator neighbouriterator;
			for (neighbouriterator = neighbours_.begin(); neighbouriterator
					!= neighbours_.end(); ++neighbouriterator) {
				if (isNeighbourAnnouncable(*neighbouriterator))
					announce(*neighbouriterator, NEIGHBOUR);
			}
			while (!this->cachedLookups.empty()) {
				lookup(this->cachedLookups.front());
				this->cachedLookups.pop_front();
			}
		}
		// Faster Bootstrapping by searching for random Keys
		if (dtn_dht_ready_for_work(&_context) > 0 && dtn_dht_ready_for_work(
				&_context) <= 2 && numberOfRandomRequests < 40) {
			dtn_dht_start_random_lookup(&_context);
			numberOfRandomRequests++;
		}
		struct timeval tv;
		fd_set readfds;
		tv.tv_sec = tosleep;
		tv.tv_usec = random() % 1000000;
		int high_fd = _interrupt_pipe[0];
		FD_ZERO(&readfds);
		FD_SET(_interrupt_pipe[0], &readfds);
		if (_context.ipv4socket >= 0) {
			FD_SET(_context.ipv4socket, &readfds);
			high_fd = std::max(high_fd, _context.ipv4socket);
		}
		if (_context.ipv6socket >= 0) {
			FD_SET(_context.ipv6socket, &readfds);
			high_fd = std::max(high_fd, _context.ipv6socket);
		}
		rc = select(high_fd + 1, &readfds, NULL, NULL, &tv);
		if (rc < 0) {
			if (errno != EINTR) {
				IBRCOMMON_LOGGER_TAG("DHTNameService", error)
							<< "select of DHT Sockets failed with error: "
							<< errno << IBRCOMMON_LOGGER_ENDL;
				ibrcommon::Thread::sleep(1000);
			}
		}
		if (FD_ISSET(_interrupt_pipe[0], &readfds)) {
			IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 25) << "unblocked by self-pipe-trick"
						<< IBRCOMMON_LOGGER_ENDL;
			// this was an interrupt with the self-pipe-trick
			char buf[2];
			int read = ::read(_interrupt_pipe[0], buf, 2);
			if (read <= 2 || _exiting)
				break;
		}
		if (rc > 0) {
			fromlen = sizeof(from);
			if (_context.ipv4socket >= 0 && FD_ISSET(_context.ipv4socket,
					&readfds))
				rc = recvfrom(_context.ipv4socket, _buf, sizeof(_buf) - 1, 0,
						(struct sockaddr*) &from, &fromlen);
			else if (_context.ipv6socket >= 0 && FD_ISSET(_context.ipv6socket,
					&readfds))
				rc = recvfrom(_context.ipv6socket, _buf, sizeof(_buf) - 1, 0,
						(struct sockaddr*) &from, &fromlen);
		}
		if (rc > 0) {
			_buf[rc] = '\0';
			{
				ibrcommon::MutexLock l(this->_libmutex);
				rc = dtn_dht_periodic(&_context, _buf, rc,
						(struct sockaddr*) &from, fromlen, &tosleep);
			}
		} else {
			{
				ibrcommon::MutexLock l(this->_libmutex);
				rc = dtn_dht_periodic(&_context, NULL, 0, NULL, 0, &tosleep);
			}
		}
		int numberOfHosts = 0;
		int numberOfGoodHosts = 0;
		int numberOfGood6Hosts = 0;
		unsigned int numberOfBlocksHosts = 0;
		unsigned int numberOfBlocksHostsIPv4 = 0;
		unsigned int numberOfBlocksHostsIPv6 = 0;
		{
			ibrcommon::MutexLock l(this->_libmutex);
			if (_context.ipv4socket >= 0)
				numberOfHosts
						= dtn_dht_nodes(AF_INET, &numberOfGoodHosts, NULL);
			if (_context.ipv6socket >= 0)
				numberOfHosts += dtn_dht_nodes(AF_INET6, &numberOfGood6Hosts,
						NULL);
			numberOfBlocksHosts = dtn_dht_blacklisted_nodes(
					&numberOfBlocksHostsIPv4, &numberOfBlocksHostsIPv6);
		}
		if (this->_foundNodes != numberOfHosts) {
			if (_config.isBlacklistEnabled()) {
				IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 42) << "DHT Nodes available: "
							<< numberOfHosts << "(Good:" << numberOfGoodHosts
							<< "+" << numberOfGood6Hosts << ") Blocked: "
							<< numberOfBlocksHosts << "("
							<< numberOfBlocksHostsIPv4 << "+"
							<< numberOfBlocksHostsIPv6 << ")"
							<< IBRCOMMON_LOGGER_ENDL;

			} else {
				IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 42) << "DHT Nodes available: "
							<< numberOfHosts << "(Good:" << numberOfGoodHosts
							<< "+" << numberOfGood6Hosts << ")"
							<< IBRCOMMON_LOGGER_ENDL;

			}
			this->_foundNodes = numberOfHosts;
		}
		if (rc < 0) {
			if (errno == EINTR) {
				continue;
			} else {
				IBRCOMMON_LOGGER_TAG("DHTNameService", error) << "dtn_dht_periodic failed"
							<< IBRCOMMON_LOGGER_ENDL;
				if (rc == EINVAL || rc == EFAULT) {
					IBRCOMMON_LOGGER_TAG("DHTNameService", error) << "DHT failed -> stopping DHT"
								<< IBRCOMMON_LOGGER_ENDL;
					break;
				}
				tosleep = 1;
			}
		}
	}
	dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::remove(this);
	dtn::core::EventDispatcher<dtn::core::NodeEvent>::remove(this);
	this->_initialized = false;
	if (_config.getPathToNodeFiles().size() > 0) {
		ibrcommon::MutexLock l(this->_libmutex);
		int save = dtn_dht_save_conf(_config.getPathToNodeFiles().c_str());
		if (save != 0) {
			IBRCOMMON_LOGGER_TAG("DHTNameService", warning) << "DHT could not save nodes"
						<< IBRCOMMON_LOGGER_ENDL;
		} else {
			IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 25) << "DHT saved nodes"
						<< IBRCOMMON_LOGGER_ENDL;
		}
	}
	if (this->_announced && _config.isSelfAnnouncingEnabled())
		deannounce(dtn::core::BundleCore::local);
	dtn_dht_uninit();
	IBRCOMMON_LOGGER_TAG("DHTNameService", info) << "DHT shut down" << IBRCOMMON_LOGGER_ENDL;
	// Closes all sockets of the DHT
	dtn_dht_close_sockets(&_context);

	dtn_dht_free_convergence_layer_struct(_context.clayer);

	IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 25) << "DHT sockets are closed"
				<< IBRCOMMON_LOGGER_ENDL;
	::close(_interrupt_pipe[0]);
	::close(_interrupt_pipe[1]);
}

void dtn::dht::DHTNameService::componentDown() throw () {
	// un-register as discovery beacon handler
	dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(this);

	this->_exiting = true;
	IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 25) << "DHT will be shut down"
				<< IBRCOMMON_LOGGER_ENDL;
	ssize_t written = ::write(_interrupt_pipe[1], "i", 1);
	if (written < 1) {
		IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 25) << "DHT pipeline trick failed"
					<< IBRCOMMON_LOGGER_ENDL;
	}
}

void dtn::dht::DHTNameService::lookup(const dtn::data::EID &eid) {
	if (!dtn::core::BundleCore::local.sameHost(eid)) {
		if (this->_announced) {
			std::string eid_ = eid.getNode().getString();
			IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 30) << "DHT Lookup: " << eid_
						<< IBRCOMMON_LOGGER_ENDL;
			ibrcommon::MutexLock l(this->_libmutex);
			dtn_dht_lookup(&_context, eid_.c_str(), eid_.size());
		} else {
			this->cachedLookups.push_front(eid);
		}
	}
}

/**
 * Reads the own EID and publishes all the available Convergence Layer in the DHT
 */
void dtn::dht::DHTNameService::announce(const dtn::core::Node &n,
		enum dtn_dht_lookup_type type) {
	if (this->_announced) {
		std::string eid_ = n.getEID().getNode().getString();
		ibrcommon::MutexLock l(this->_libmutex);
		int rc = dtn_dht_announce(&_context, eid_.c_str(), eid_.size(), type);
		if (rc > 0) {
			IBRCOMMON_LOGGER_TAG("DHTNameService", info) << "DHT Announcing: " << eid_
						<< IBRCOMMON_LOGGER_ENDL;
		}
	}
}

bool dtn::dht::DHTNameService::isNeighbourAnnouncable(
		const dtn::core::Node &node) {
	if (!_config.isNeighbourAnnouncementEnabled())
		return false;
	// If forwarding of bundles is disabled, do not announce the neighbors to
	// prevent receiving bundles for neighbor EIDs, which would not be forwarded
	if (!daemon::Configuration::getInstance().getNetwork().doForwarding()) {
		return false;
	}

	try {
		// get the merged node object
		const dtn::core::Node n =
				dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbor(node.getEID());

		// Ignore all none discovered and none static nodes
		// They could only be discovered by the DHT,
		// so they are not really close neighbours and could be found directly in the DHT.
		// This prevents the node to be announced by all neighbours, how found this node
		std::set<dtn::core::Node::Type> types = n.getTypes();
		if (types.find(dtn::core::Node::NODE_DISCOVERED) == types.end()
				&& (types.find(dtn::core::Node::NODE_STATIC_GLOBAL) == types.end())) {
			return false;
		}

		// Proof, if the neighbour has told us, that he don't want to be published on the DHT
		std::list<dtn::core::Node::Attribute> services = n.get("dhtns");
		if (!services.empty()) {
			for (std::list<dtn::core::Node::Attribute>::const_iterator service =
					services.begin(); service != services.end(); ++service) {
				bool proxy = true;
				std::vector<std::string> parameters = dtn::utils::Utils::tokenize(
						";", (*service).value);
				std::vector<std::string>::const_iterator param_iter = parameters.begin();

				while (param_iter != parameters.end()) {
					std::vector<std::string> p = dtn::utils::Utils::tokenize("=",
							(*param_iter));
					if (p[0].compare("proxy") == 0) {
						std::stringstream proxy_stream;
						proxy_stream << p[1];
						proxy_stream >> proxy;
					}
					++param_iter;
				}
				if (!proxy)
					return false;
			}
		}
		return true;
	} catch (const NodeNotAvailableException&) {
		// do not announce the node if it is
		// not recognized as neighbor
		return false;
	}
}

void dtn::dht::DHTNameService::deannounce(const dtn::core::Node &n) {
	std::string eid_ = n.getEID().getNode().getString();
	ibrcommon::MutexLock l(this->_libmutex);
	dtn_dht_deannounce(eid_.c_str(), eid_.size());
	IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 25) << "DHT Deannounced: " << eid_
				<< IBRCOMMON_LOGGER_ENDL;
}

std::string dtn::dht::DHTNameService::getConvergenceLayerName(
		const dtn::daemon::Configuration::NetConfig &net) {
	std::string cltype_ = "";
	switch (net.type) {
	case dtn::daemon::Configuration::NetConfig::NETWORK_TCP:
		cltype_ = "tcp";
		break;
	case dtn::daemon::Configuration::NetConfig::NETWORK_UDP:
		cltype_ = "udp";
		break;
	case dtn::daemon::Configuration::NetConfig::NETWORK_EMAIL:
		cltype_ = "email";
		break;
		//	case dtn::daemon::Configuration::NetConfig::NETWORK_HTTP:
		//		cltype_ = "http";
		//		break;
	default:
		throw ibrcommon::Exception("type of convergence layer not supported");
	}
	return cltype_;
}

void dtn::dht::DHTNameService::raiseEvent(const dtn::routing::QueueBundleEvent &event) throw ()
{
	if (!event.bundle.destination.sameHost(dtn::core::BundleCore::local)
			&& !event.bundle.destination.isNone()) {
		lookup(event.bundle.destination);
	}
}

void dtn::dht::DHTNameService::raiseEvent(const dtn::core::NodeEvent &nodeevent) throw ()
{
	const dtn::core::Node &n = nodeevent.getNode();
	if (!n.getEID().sameHost(dtn::core::BundleCore::local)) {
		switch (nodeevent.getAction()) {
		case NODE_AVAILABLE:
			if (isNeighbourAnnouncable(n))
				announce(n, NEIGHBOUR);
			pingNode(n);
			break;
		case NODE_UNAVAILABLE:
			deannounce(n);
		default:
			break;
		}
	}
}

void dtn::dht::DHTNameService::pingNode(const dtn::core::Node &n) {
	int rc;
	std::list<dtn::core::Node::Attribute> services = n.get("dhtns");
	unsigned int port = 9999;
	if (!services.empty()) {
		for (std::list<dtn::core::Node::Attribute>::const_iterator service =
				services.begin(); service != services.end(); ++service) {
			std::vector<std::string> parameters = dtn::utils::Utils::tokenize(
					";", (*service).value);
			std::vector<std::string>::const_iterator param_iter = parameters.begin();
			bool portFound = false;
			while (param_iter != parameters.end()) {
				std::vector<std::string> p = dtn::utils::Utils::tokenize("=",
						(*param_iter));
				if (p[0].compare("port") == 0) {
					std::stringstream port_stream;
					port_stream << p[1];
					port_stream >> port;
					portFound = true;
				}
				++param_iter;
			}
			// Do not ping the node, if he doesn't tell us, which port he has
			if(!portFound){
				continue;
			}
			IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 25) << "trying to ping node "
						<< n.toString() << IBRCOMMON_LOGGER_ENDL;
			const std::list<std::string> ips = getAllIPAddresses(n);
			IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 25) << ips.size() << " IP Addresses found!"
						<< IBRCOMMON_LOGGER_ENDL;
			std::string lastip = "";
			for (std::list<std::string>::const_iterator iter = ips.begin(); iter
					!= ips.end(); ++iter) {
				const std::string ip = (*iter);
				// Ignoring double existence of ip's
				if (ip == lastip) {
					continue;
				}
				// decode address and port
				struct sockaddr_in sin;
				memset(&sin, 0, sizeof(sin));
				sin.sin_family = AF_INET;
				sin.sin_port = htons(port);
				IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 26) << " --- Using IP address: " << ip
							<<IBRCOMMON_LOGGER_ENDL;
				rc = inet_pton(AF_INET, ip.c_str(), &(sin.sin_addr));
				if (rc == 1) {
					IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 26) << "Pinging node "
								<< n.toString() << " on " << ip << ":" << port
								<< IBRCOMMON_LOGGER_ENDL;
					ibrcommon::MutexLock l(this->_libmutex);
					dtn_dht_ping_node((struct sockaddr*) &sin, sizeof(sin));
				} else {
					IBRCOMMON_LOGGER_TAG("DHTNameService", error) << " --- ERROR pton: " << rc
								<<IBRCOMMON_LOGGER_ENDL;
				}
				lastip = ip;
			}
		}
	}
}

std::list<std::string> dtn::dht::DHTNameService::getAllIPAddresses(
		const dtn::core::Node &n) {
	std::string address = "0.0.0.0";
	unsigned int port = 0;
	std::list < std::string > ret;
	const std::list<dtn::core::Node::URI> uri_list = n.get(
			dtn::core::Node::CONN_TCPIP);
	for (std::list<dtn::core::Node::URI>::const_iterator iter =
			uri_list.begin(); iter != uri_list.end(); ++iter) {
		const dtn::core::Node::URI &uri = (*iter);
		uri.decode(address, port);
		ret.push_front(address);
	}
	const std::list<dtn::core::Node::URI> udp_uri_list = n.get(
			dtn::core::Node::CONN_UDPIP);
	for (std::list<dtn::core::Node::URI>::const_iterator iter =
			udp_uri_list.begin(); iter != udp_uri_list.end(); ++iter) {
		const dtn::core::Node::URI &udp_uri = (*iter);
		udp_uri.decode(address, port);
		ret.push_front(address);
	}
	return ret;
}

void dtn::dht::DHTNameService::bootstrapping() {
	if (this->_bootstrapped > time(NULL) + 30) {
		return;
	}
	this->_bootstrapped = time(NULL);
	if (_config.getPathToNodeFiles().size() > 0) {
		bootstrappingFile();
	}
	if (_config.isDNSBootstrappingEnabled()) {
		bootstrappingDNS();
	}
	if (_config.isIPBootstrappingEnabled()) {
		bootstrappingIPs();
	}
}

void dtn::dht::DHTNameService::bootstrappingFile() {
	int rc;
	{
		ibrcommon::MutexLock l(this->_libmutex);
		rc = dtn_dht_load_prev_conf(_config.getPathToNodeFiles().c_str());
	}
	if (rc != 0) {
		IBRCOMMON_LOGGER_TAG("DHTNameService", warning) << "DHT loading of saved nodes failed"
					<< IBRCOMMON_LOGGER_ENDL;
	} else {
		IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 25) << "DHT loading of saved nodes done"
					<< IBRCOMMON_LOGGER_ENDL;
	}
}

void dtn::dht::DHTNameService::bootstrappingDNS() {
	int rc;
	std::vector<std::string> dns = _config.getDNSBootstrappingNames();
	if (!dns.empty()) {
		std::vector<std::string>::const_iterator dns_iter = dns.begin();
		while (dns_iter != dns.end()) {
			const std::string &dn = (*dns_iter);
			{
				ibrcommon::MutexLock l(this->_libmutex);
				rc = dtn_dht_dns_bootstrap(&_context, dn.c_str(), NULL);
			}
			if (rc != 0) {
				IBRCOMMON_LOGGER_TAG("DHTNameService", warning) << "bootstrapping from domain " << dn
							<< " failed with error: " << rc
							<< IBRCOMMON_LOGGER_ENDL;
			} else {
				IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 25)
							<< "DHT Bootstrapping done for domain" << dn
							<< IBRCOMMON_LOGGER_ENDL;
			}
			++dns_iter;
		}
	} else {
		{
			ibrcommon::MutexLock l(this->_libmutex);
			rc = dtn_dht_dns_bootstrap(&_context, NULL, NULL);
		}
		if (rc != 0) {
			IBRCOMMON_LOGGER_TAG("DHTNameService", warning)
						<< "bootstrapping from default domain failed with error: "
						<< rc << IBRCOMMON_LOGGER_ENDL;
		} else {
			IBRCOMMON_LOGGER_DEBUG_TAG("DHTNameService", 25)
						<< "DHT Bootstrapping done for default domain"
						<< IBRCOMMON_LOGGER_ENDL;
		}
	}

}

void dtn::dht::DHTNameService::bootstrappingIPs() {
	int rc;
	std::vector<std::string> ips = _config.getIPBootstrappingIPs();
	std::vector<std::string>::const_iterator ip_iter = ips.begin();
	while (ip_iter != ips.end()) {
		std::vector<std::string> ip
				= dtn::utils::Utils::tokenize(" ", (*ip_iter));
		int size, ipversion = AF_INET;
		struct sockaddr *wellknown_node;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
		memset(&sin, 0, sizeof(sin));
		int port = 9999;

		// read port
		if (ip.size() > 1) {
			port = atoi(ip[1].c_str());
		}

		// read address
		if (!ip.empty()) {
			rc = inet_pton(ipversion, ip[0].c_str(), &(sin.sin_addr));
			if (rc <= 0) {
				ipversion = AF_INET6;
				rc = inet_pton(ipversion, ip[0].c_str(), &(sin6.sin6_addr));
				if (rc <= 0) {
					break;
				} else {
					sin6.sin6_family = ipversion;
					sin6.sin6_port = htons(port);
					size = sizeof(sin6);
					wellknown_node = (struct sockaddr *) &sin6;
				}
			} else {
				sin.sin_family = ipversion;
				sin.sin_port = htons(port);
				size = sizeof(sin);
				wellknown_node = (struct sockaddr *) &sin;
			}
			if (rc == 1) {
				ibrcommon::MutexLock l(this->_libmutex);
				dtn_dht_ping_node(wellknown_node, size);
			}
		}

		++ip_iter;
	}
}

// TODO Nur für Interfaces zulassen, auf denen ich gebunden bin!

void dtn::dht::DHTNameService::onUpdateBeacon(const ibrcommon::vinterface&, DiscoveryBeacon &beacon)
		throw (dtn::net::DiscoveryBeaconHandler::NoServiceHereException) {
	if (this->_initialized) {
		std::stringstream service;
		service << "port=" << this->_context.port << ";";
		if (!this->_config.isNeighbourAllowedToAnnounceMe()) {
			service << "proxy=false;";
		}
		beacon.addService( DiscoveryService("dhtns", service.str()));
	} else {
		if(!this->_config.isNeighbourAllowedToAnnounceMe()) {
			beacon.addService( DiscoveryService("dhtns", "proxy=false;"));
		} else {
			throw dtn::net::DiscoveryBeaconHandler::NoServiceHereException();
		}
	}
}

void dtn_dht_handle_lookup_result(const struct dtn_dht_lookup_result *result) {
	if (result == NULL || result->eid == NULL || result->clayer == NULL) {
		return;
	}

	// Extracting the EID of the answering node
	std::string eid__;
	std::string clname__;
	struct dtn_eid * eid = result->eid;
	unsigned int i;
	for (i = 0; i < eid->eidlen; ++i) {
		eid__ += eid->eid[i];
	}

	// Extracting the convergence layer of the node
	std::stringstream ss;
	struct dtn_convergence_layer * cl = result->clayer;
	if (cl == NULL)
		return;
	dtn::core::Node node(eid__);
	ss << "Adding Node " << eid__ << ": ";
	// Iterating over the list of convergence layer
	bool hasCL = false;
	while (cl != NULL) {
		enum Node::Protocol proto__;
		std::stringstream service;
		clname__ = "";
		for (i = 0; i < cl->clnamelen; ++i) {
			clname__ += cl->clname[i];
		}
		if (clname__ == "tcp") {
			proto__ = Node::CONN_TCPIP;
		} else if (clname__ == "udp") {
			proto__ = Node::CONN_UDPIP;
		} else if (clname__ == "email") {
			proto__ = Node::CONN_EMAIL;
		} else {
			proto__ = Node::CONN_UNDEFINED;
			//TODO find the right string to be added to service string
		}
		// Extracting the arguments of the convergence layer
		struct dtn_convergence_layer_arg * arg = cl->args;
		std::string argstring__;
		while (arg != NULL) {
			if (arg->keylen <= 0 || arg->valuelen <= 0) {
				arg = arg->next;
				continue;
			}
			argstring__ = "";
			for (i = 0; i < arg->keylen; ++i) {
				argstring__ += arg->key[i];
			}
			service << argstring__ << "=";
			argstring__ = "";
			for (i = 0; i < arg->valuelen && arg->value[i] != '\0'; ++i) {
				argstring__ += arg->value[i];
			}
			service << argstring__ << ";";
			arg = arg->next;
		}
		ss << clname__ << "(" << service.str() << ") ";
		// Add the found convergence layer to the node object
		node.add(
				Node::URI(Node::NODE_DHT_DISCOVERED, proto__, service.str(),
						DHT_RESULTS_EXPIRE_TIMEOUT,
						DHT_DISCOVERED_NODE_PRIORITY));
		hasCL = true;
		cl = cl->next;
	}

	// Adding the new node to the BundleCore to make it available to the daemon
	dtn::core::BundleCore &core = dtn::core::BundleCore::getInstance();
	if (hasCL) {
		IBRCOMMON_LOGGER_TAG("DHTNameService", info) << ss.str() << IBRCOMMON_LOGGER_ENDL;
		core.getConnectionManager().add(node);
	}

	// Extracting all neighbours of the new node if usage is allowed
	if (!dtn::daemon::Configuration::getInstance().getDHT().ignoreDHTNeighbourInformations()) {
		struct dtn_eid * neighbour = result->neighbours;
		std::string neighbour__;
		while (neighbour) {
			neighbour__ = "";
			for (i = 0; i < neighbour->eidlen; ++i) {
				neighbour__ += neighbour->eid[i];
			}
			dtn::data::EID n(neighbour__);
			dtn::core::Node neighbourNode(n);
			if (!dtn::core::BundleCore::local.sameHost(n)
					&& !core.getConnectionManager().isNeighbor(n)) {
				dtn::core::BundleCore::getInstance().addRoute(n, node.getEID(),
						DHT_PATH_EXPIRE_TIMEOUT);
			}
			neighbour = neighbour->next;
		}
	}
	//TODO GROUP HANDLING SHOULD BE IMPLEMENTED HERE!!!
	struct dtn_eid * group = result->groups;
	while (group) {
		group = group->next;
	}
	return;
}

// This function is called, if the DHT has done a search or announcement.
// At this moment, it is just ignored
void dtn_dht_operation_done(const unsigned char *info_hash){
	return;
}
