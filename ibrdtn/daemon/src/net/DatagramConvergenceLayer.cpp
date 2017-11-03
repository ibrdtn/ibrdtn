/*
 * DatagramConvergenceLayer.cpp
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

#include "net/DatagramConvergenceLayer.h"
#include "net/DatagramConnection.h"

#include "core/BundleCore.h"
#include "core/NodeEvent.h"
#include "core/EventDispatcher.h"

#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/thread/RWLock.h>

#include <string.h>
#include <vector>

namespace dtn
{
	namespace net
	{
		const std::string DatagramConvergenceLayer::TAG = "DatagramConvergenceLayer";

		DatagramConvergenceLayer::DatagramConvergenceLayer(DatagramService *ds)
		 : _service(ds), _receiver(*this), _running(false),
		   _stats_in(0), _stats_out(0), _stats_rtt(0.0), _stats_retries(0), _stats_failure(0)
		{
		}

		DatagramConvergenceLayer::~DatagramConvergenceLayer()
		{
			// wait until the component thread is terminated
			join();

			// wait until all connections are down
			{
				ibrcommon::MutexLock l(_cond_connections);
				while (_connections.size() > 0) _cond_connections.wait();
			}

			// delete the associated service
			delete _service;
		}

		void DatagramConvergenceLayer::raiseEvent(const dtn::core::NodeEvent &event) throw ()
		{
			if (event.getAction() == NODE_UNAVAILABLE)
			{
				NodeGone *gone = new NodeGone();
				gone->eid = event.getNode().getEID();
				_action_queue.push(gone);
			}
		}

		void DatagramConvergenceLayer::resetStats()
		{
			_stats_in = 0;
			_stats_out = 0;
			_stats_rtt = 0.0;
			_stats_retries = 0;
			_stats_failure = 0;
		}

		void DatagramConvergenceLayer::getStats(ConvergenceLayer::stats_data &data) const
		{
			std::stringstream ss_format;

			static const std::string IN_TAG = dtn::core::Node::toString(getDiscoveryProtocol()) + "|in";
			static const std::string OUT_TAG = dtn::core::Node::toString(getDiscoveryProtocol()) + "|out";

			static const std::string RTT_TAG = dtn::core::Node::toString(getDiscoveryProtocol()) + "|rtt";
			static const std::string RETRIES_TAG = dtn::core::Node::toString(getDiscoveryProtocol()) + "|retries";
			static const std::string FAIL_TAG = dtn::core::Node::toString(getDiscoveryProtocol()) + "|fail";

			ss_format << _stats_in;
			data[IN_TAG] = ss_format.str();
			ss_format.str("");

			ss_format << _stats_out;
			data[OUT_TAG] = ss_format.str();
			ss_format.str("");

			ss_format << _stats_rtt;
			data[RTT_TAG] = ss_format.str();
			ss_format.str("");

			ss_format << _stats_retries;
			data[RETRIES_TAG] = ss_format.str();
			ss_format.str("");

			ss_format << _stats_failure;
			data[FAIL_TAG] = ss_format.str();
		}

		dtn::core::Node::Protocol DatagramConvergenceLayer::getDiscoveryProtocol() const
		{
			return _service->getProtocol();
		}

		void DatagramConvergenceLayer::callback_send(DatagramConnection&, const char &flags, const unsigned int &seqno, const std::string &destination, const char *buf, const dtn::data::Length &len) throw (DatagramException)
		{
			// only on sender at once
			ibrcommon::MutexLock l(_send_lock);

			// forward the send request to DatagramService
			_service->send(HEADER_SEGMENT, flags, seqno, destination, buf, len);

			// traffic monitoring
			_stats_out += len;
		}

		void DatagramConvergenceLayer::callback_ack(DatagramConnection&, const unsigned int &seqno, const std::string &destination) throw (DatagramException)
		{
			// only on sender at once
			ibrcommon::MutexLock l(_send_lock);

			// forward the send request to DatagramService
			_service->send(HEADER_ACK, 0, seqno, destination, NULL, 0);
		}

		void DatagramConvergenceLayer::callback_nack(DatagramConnection&, const unsigned int &seqno, const std::string &destination) throw (DatagramException)
		{
			// only on sender at once
			ibrcommon::MutexLock l(_send_lock);

			// forward the send request to DatagramService
			_service->send(HEADER_NACK, 0, seqno, destination, NULL, 0);
		}

		void DatagramConvergenceLayer::queue(const dtn::core::Node &node, const dtn::net::BundleTransfer &job)
		{
			// do not queue any new jobs if the convergence layer goes down
			if (!_running) return;

			const std::list<dtn::core::Node::URI> uri_list = node.get(_service->getProtocol());
			if (uri_list.empty()) return;

			// get the first element of the result
			const dtn::core::Node::URI &uri = uri_list.front();

			// some debugging
			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConvergenceLayer::TAG, 10) << "job queued for " << node.getEID().getString() << IBRCOMMON_LOGGER_ENDL;

			QueueBundle *queue = new QueueBundle(job);
			queue->uri = uri.value;

			_action_queue.push( queue );
		}

		DatagramConnection& DatagramConvergenceLayer::getConnection(const std::string &identifier, bool create) throw (ConnectionNotAvailableException)
		{
			DatagramConnection *connection = NULL;

			// Test if connection for this address already exist
			for(connection_list::const_iterator i = _connections.begin(); i != _connections.end(); ++i)
			{
				if ((*i)->getIdentifier() == identifier)
					return *(*i);
			}

			// throw exception if we should not create new connections
			if (!create) throw ConnectionNotAvailableException();

			// Connection does not exist, create one and put it into the list
			connection = new DatagramConnection(identifier, _service->getParameter(), (*this));

			// increment the number of active connections
			{
				ibrcommon::MutexLock l(_cond_connections);

				// add a new connection to the list of connections
				_connections.push_back(connection);

				// signal the modified connection list
				_cond_connections.signal(true);
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConvergenceLayer::TAG, 10) << "Selected identifier: " << connection->getIdentifier() << IBRCOMMON_LOGGER_ENDL;
			connection->start();
			return *connection;
		}

		void DatagramConvergenceLayer::reportSuccess(size_t retries, double rtt)
		{
			_stats_rtt = rtt;
			_stats_retries += retries;
		}

		void DatagramConvergenceLayer::reportFailure()
		{
			_stats_failure++;
		}

		void DatagramConvergenceLayer::connectionUp(const DatagramConnection *conn)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConvergenceLayer::TAG, 10) << "Up: " << conn->getIdentifier() << IBRCOMMON_LOGGER_ENDL;
		}

		void DatagramConvergenceLayer::connectionDown(const DatagramConnection *conn)
		{
			ConnectionDown *cd = new ConnectionDown();
			cd->id = conn->getIdentifier();
			_action_queue.push(cd);
		}

		void DatagramConvergenceLayer::componentUp() throw ()
		{
			// routine checked for throw() on 15.02.2013
			try {
				_service->bind();
			} catch (const std::exception &e) {
				IBRCOMMON_LOGGER_TAG(DatagramConvergenceLayer::TAG, error) << "bind to " << _service->getInterface().toString() << " failed (" << e.what() << ")" << IBRCOMMON_LOGGER_ENDL;
			}

			// register for NodeEvent objects
			dtn::core::EventDispatcher<dtn::core::NodeEvent>::add(this);

			// register for discovery beacon handling
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(_service->getInterface(), this);

			// set the running variable
			_running = true;
		}

		void DatagramConvergenceLayer::componentDown() throw ()
		{
			// un-register for discovery beacon handling
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(_service->getInterface(), this);

			// un-register for NodeEvent objects
			dtn::core::EventDispatcher<dtn::core::NodeEvent>::remove(this);

			_action_queue.push(new Shutdown());
		}

		void DatagramConvergenceLayer::onAdvertiseBeacon(const ibrcommon::vinterface &iface, const DiscoveryBeacon &beacon) throw ()
		{
			// only handler beacons for this interface
			if (iface != _service->getInterface()) return;

			// serialize announcement
			std::stringstream ss;
			ss << beacon;

			std::streamsize len = ss.str().size();

			try {
				// only on sender at once
				ibrcommon::MutexLock l(_send_lock);

				// forward the send request to DatagramService
				_service->send(HEADER_BROADCAST, 0, 0, ss.str().c_str(), static_cast<dtn::data::Length>(len));
			} catch (const DatagramException&) {
				// ignore any send failure
			};
		}

		void DatagramConvergenceLayer::receive() throw ()
		{
			size_t maxlen = _service->getParameter().max_msg_length;
			std::string address;
			unsigned int seqno = 0;
			char flags = 0;
			char type = 0;
			std::vector<char> data(maxlen);
			size_t len = 0;

			// get the reference to the discovery agent
			dtn::net::DiscoveryAgent &agent = dtn::core::BundleCore::getInstance().getDiscoveryAgent();

			while (_running)
			{
				try {
					// Receive full frame from socket
					len = _service->recvfrom(&data[0], maxlen, type, flags, seqno, address);

					// traffic monitoring
					_stats_in += len;
				} catch (const DatagramException &ex) {
					if (_running) {
						IBRCOMMON_LOGGER_TAG(DatagramConvergenceLayer::TAG, error) << "recvfrom() failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}
					_running = false;
					break;
				}

				IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConvergenceLayer::TAG, 10) << "receive() Address: " << address << IBRCOMMON_LOGGER_ENDL;

				// Check for extended header and retrieve if available
				if (type == HEADER_BROADCAST)
				{
					try {
						IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConvergenceLayer::TAG, 10) << "receive() Announcement received" << IBRCOMMON_LOGGER_ENDL;

						DiscoveryBeacon beacon = agent.obtainBeacon();

						std::stringstream ss;
						ss.write(&data[0], len);
						ss >> beacon;

						// ignore own beacons
						if (beacon.getEID() == dtn::core::BundleCore::local) continue;

						if (beacon.isShort())
						{
							// can not generate node name from short beacons
							continue;
						}

						// add discovered service entry
						beacon.addService(dtn::net::DiscoveryService(_service->getProtocol(), address));

						BeaconReceived *bc = new BeaconReceived();
						bc->address = address;
						bc->data = beacon;
						_action_queue.push(bc);
					} catch (const ibrcommon::Exception&) {
						// catch wrong formats
					}

					continue;
				}
				else if ( type == HEADER_SEGMENT )
				{
					SegmentReceived *seg = new SegmentReceived(maxlen);
					seg->address = address;
					seg->seqno = seqno;
					seg->flags = flags;
					seg->data = data;
					seg->len = len;
					_action_queue.push(seg);
				}
				else if ( type == HEADER_ACK )
				{
					AckReceived *ack = new AckReceived();
					ack->address = address;
					ack->seqno = seqno;
					_action_queue.push(ack);
				}
				else if ( type == HEADER_NACK )
				{
					NackReceived *nack = new NackReceived();
					nack->address = address;
					nack->seqno = seqno;
					nack->temporary = flags & DatagramService::NACK_TEMPORARY;
					_action_queue.push(nack);
				}
			}
		}

		void DatagramConvergenceLayer::componentRun() throw ()
		{
			// start receiver
			_receiver.init();
			_receiver.start();

			// get the reference to the discovery agent
			dtn::net::DiscoveryAgent &agent = dtn::core::BundleCore::getInstance().getDiscoveryAgent();

			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConvergenceLayer::TAG, 10) << "componentRun() entered" << IBRCOMMON_LOGGER_ENDL;

			try {
				while (_running || (_connections.size() > 0))
				{
					Action *action = _action_queue.poll();

					IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConvergenceLayer::TAG, 10) << "processing task" << IBRCOMMON_LOGGER_ENDL;

					try {
						AckReceived &ack = dynamic_cast<AckReceived&>(*action);

						try {
							// Connection instance for this address
							DatagramConnection& connection = getConnection(ack.address, false);

							IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 20) << "ack received for seqno " << ack.seqno << IBRCOMMON_LOGGER_ENDL;

							// Decide in which queue to write based on the src address
							connection.ack(ack.seqno);
						} catch (const ConnectionNotAvailableException &ex) {
							// connection does not exists - ignore the ACK
						}
					} catch (const std::bad_cast&) { };

					try {
						NackReceived &nack = dynamic_cast<NackReceived&>(*action);

						// the peer refused the current bundle
						try {
							// Connection instance for this address
							DatagramConnection& connection = getConnection(nack.address, false);

							IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 20) << "nack received for seqno " << nack.seqno << IBRCOMMON_LOGGER_ENDL;

							// Decide in which queue to write based on the src address
							connection.nack(nack.seqno, nack.temporary);
						} catch (const ConnectionNotAvailableException &ex) {
							// connection does not exists - ignore the NACK
						}
					} catch (const std::bad_cast&) { };

					try {
						SegmentReceived &segment = dynamic_cast<SegmentReceived&>(*action);

						// Connection instance for this address
						DatagramConnection& connection = getConnection(segment.address, true);

						try {
							// Decide in which queue to write based on the src address
							connection.queue(segment.flags, segment.seqno, &segment.data[0], segment.len);
						} catch (const ibrcommon::Exception &ex) {
							IBRCOMMON_LOGGER_TAG(DatagramConvergenceLayer::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
							connection.shutdown();
						};
					} catch (const std::bad_cast&) { };

					try {
						BeaconReceived &beacon = dynamic_cast<BeaconReceived&>(*action);

						// Connection instance for this address
						DatagramConnection& connection = getConnection(beacon.address, true);
						connection.setPeerEID(beacon.data.getEID());

						// announce the received beacon
						agent.onBeaconReceived(beacon.data);
					} catch (const std::bad_cast&) { };

					try {
						ConnectionDown &cd = dynamic_cast<ConnectionDown&>(*action);

						ibrcommon::MutexLock l(_cond_connections);
						for (connection_list::iterator i = _connections.begin(); i != _connections.end(); ++i)
						{
							if ((*i)->getIdentifier() == cd.id)
							{
								IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConvergenceLayer::TAG, 10) << "Down: " << cd.id << IBRCOMMON_LOGGER_ENDL;

								// delete the connection
								delete (*i);
								_connections.erase(i);

								// signal the modified connection list
								_cond_connections.signal(true);
								break;
							}
						}
					} catch (const std::bad_cast&) { };

					try {
						NodeGone &gone = dynamic_cast<NodeGone&>(*action);

						for (connection_list::iterator i = _connections.begin(); i != _connections.end(); ++i)
						{
							if ((*i)->getPeerEID() == gone.eid)
							{
								// shutdown the connection
								(*i)->shutdown();
								break;
							}
						}
					} catch (const std::bad_cast&) { };

					try {
						QueueBundle &queue = dynamic_cast<QueueBundle&>(*action);

						// get a new or the existing connection for this address
						DatagramConnection &conn = getConnection( queue.uri, true );

						// queue the job to the connection
						conn.queue(queue.job);
					} catch (const std::bad_cast&) { };

					try {
						dynamic_cast<Shutdown&>(*action);

						// shutdown all connections
						for(connection_list::const_iterator i = _connections.begin(); i != _connections.end(); ++i)
						{
							(*i)->shutdown();
						}

						_running = false;
					} catch (const std::bad_cast&) { };

					// delete task object
					delete action;

					yield();
				}
			} catch (const ibrcommon::QueueUnblockedException &ex) {
				// unblocked
			}

			_receiver.join();
		}

		void DatagramConvergenceLayer::__cancellation() throw ()
		{
			_service->shutdown();
		}

		const std::string DatagramConvergenceLayer::getName() const
		{
			return DatagramConvergenceLayer::TAG;
		}

		DatagramConvergenceLayer::Receiver::Receiver(DatagramConvergenceLayer &cl)
		 : _cl(cl)
		{
		}

		DatagramConvergenceLayer::Receiver::~Receiver()
		{
		}

		void DatagramConvergenceLayer::Receiver::init() throw ()
		{
			// reset receiver is necessary
			if (JoinableThread::isFinalized()) JoinableThread::reset();
		}

		void DatagramConvergenceLayer::Receiver::run() throw ()
		{
			_cl.receive();
		}

		void DatagramConvergenceLayer::Receiver::__cancellation() throw ()
		{
		}
	} /* namespace data */
} /* namespace dtn */
