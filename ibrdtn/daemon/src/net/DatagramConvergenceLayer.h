/*
 * DatagramConvergenceLayer.h
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

#ifndef DATAGRAMCONVERGENCELAYER_H_
#define DATAGRAMCONVERGENCELAYER_H_

#include "Component.h"
#include "net/DiscoveryBeaconHandler.h"
#include "net/DatagramService.h"
#include "net/DatagramConnection.h"
#include "net/ConvergenceLayer.h"
#include "core/NodeEvent.h"
#include "core/EventReceiver.h"

#include <list>

namespace dtn
{
	namespace net
	{
		class DatagramConvergenceLayer : public dtn::net::ConvergenceLayer, public dtn::daemon::IndependentComponent,
			public dtn::net::DatagramConnectionCallback, public DiscoveryBeaconHandler, public dtn::core::EventReceiver<dtn::core::NodeEvent>
		{
			static const std::string TAG;

		public:
			enum HEADER_FLAGS
			{
				HEADER_UNKOWN = 0,
				HEADER_BROADCAST = 1,
				HEADER_SEGMENT = 2,
				HEADER_ACK = 4,
				HEADER_NACK = 8
			};

			DatagramConvergenceLayer(DatagramService *ds);
			virtual ~DatagramConvergenceLayer();

			/**
			 * method to receive global events
			 */
			void raiseEvent(const dtn::core::NodeEvent &evt) throw ();

			/**
			 * Returns the protocol identifier
			 * @return
			 */
			dtn::core::Node::Protocol getDiscoveryProtocol() const;

			/**
			 * Queueing a job for a specific node. Starting point for the DTN core to submit
			 * bundles to nodes behind the convergence layer
			 * @param n Node reference
			 * @param job Job reference
			 */
			void queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			virtual void resetStats();

			virtual void getStats(ConvergenceLayer::stats_data &data) const;

			void onAdvertiseBeacon(const ibrcommon::vinterface &iface, const DiscoveryBeacon &beacon) throw ();

		protected:
			virtual void componentUp() throw ();
			virtual void componentRun() throw ();
			virtual void componentDown() throw ();
			void __cancellation() throw ();
			/**
			 * callback send for connections
			 * @param connection
			 * @param destination
			 * @param buf
			 * @param len
			 */
			void callback_send(DatagramConnection &connection, const char &flags, const unsigned int &seqno, const std::string &destination, const char *buf, const dtn::data::Length &len) throw (DatagramException);

			void callback_ack(DatagramConnection &connection, const unsigned int &seqno, const std::string &destination) throw (DatagramException);

			void callback_nack(DatagramConnection &connection, const unsigned int &seqno, const std::string &destination) throw (DatagramException);

			void connectionUp(const DatagramConnection *conn);
			void connectionDown(const DatagramConnection *conn);

			void reportSuccess(size_t retries, double rtt);
			void reportFailure();

			void receive() throw ();

		private:
			class ConnectionNotAvailableException : public ibrcommon::Exception {
			public:
				ConnectionNotAvailableException(const std::string what = "connection not available")
				 : ibrcommon::Exception(what) {
				}

				virtual ~ConnectionNotAvailableException() throw () {
				}
			};

			/**
			 * The receiver is a thread and receives data from the
			 * datagram service and generates actions to process.
			 */
			class Receiver : public ibrcommon::JoinableThread {
			public:
				Receiver(DatagramConvergenceLayer &cl);
				virtual ~Receiver();

				void init() throw ();

				void run() throw ();
				void __cancellation() throw ();

			private:
				DatagramConvergenceLayer &_cl;
			};

			class Action {
			public:
				Action() {};
				virtual ~Action() {};
			};

			class SegmentReceived : public Action {
			public:
				SegmentReceived(size_t maxlen) : seqno(0), flags(0), data(maxlen), len(0) {};
				virtual ~SegmentReceived() {};

				std::string address;
				unsigned int seqno;
				char flags;
				std::vector<char> data;
				size_t len;
			};

			class BeaconReceived : public Action {
			public:
				BeaconReceived() {};
				virtual ~BeaconReceived() {};

				std::string address;
				DiscoveryBeacon data;
			};

			class AckReceived : public Action {
			public:
				AckReceived() : seqno(0) {};
				virtual ~AckReceived() {};

				std::string address;
				unsigned int seqno;
			};

			class NackReceived : public Action {
			public:
				NackReceived() : seqno(0), temporary(false) {};
				virtual ~NackReceived() {};

				std::string address;
				unsigned int seqno;
				bool temporary;
			};

			class QueueBundle : public Action {
			public:
				QueueBundle(const BundleTransfer &bt) : job(bt) {};
				virtual ~QueueBundle() {};

				BundleTransfer job;
				std::string uri;
			};

			class ConnectionDown : public Action {
			public:
				ConnectionDown() {};
				virtual ~ConnectionDown() {};

				std::string id;
			};

			class NodeGone : public Action {
			public:
				NodeGone() {};
				virtual ~NodeGone() {};

				dtn::data::EID eid;
			};

			class Shutdown : public Action {
			public:
				Shutdown() {};
				virtual ~Shutdown() {};
			};

			/**
			 * Returns a connection matching the given identifier.
			 * To use this method securely a lock on _cond_connections is required.
			 *
			 * @param identifier The identifier of the connection.
			 * @param create If this parameter is set to true a new connection is created if it does not exists.
			 */
			DatagramConnection& getConnection(const std::string &identifier, bool create) throw (ConnectionNotAvailableException);

			// associated datagram service
			DatagramService *_service;

			// this thread receives data from the datagram service
			// and generates actions to process
			Receiver _receiver;

			// actions are queued here until they get processed
			ibrcommon::Queue<Action*> _action_queue;

			// on any send operation this mutex should be locked
			ibrcommon::Mutex _send_lock;

			// conditional to protect _active_conns
			ibrcommon::Conditional _cond_connections;

			typedef std::list<DatagramConnection*> connection_list;
			connection_list _connections;

			// false, if the main thread is cancelled
			bool _running;

			// stats variables
			size_t _stats_in;
			size_t _stats_out;
			double _stats_rtt;
			size_t _stats_retries;
			size_t _stats_failure;
		};
	} /* namespace data */
} /* namespace dtn */
#endif /* DATAGRAMCONVERGENCELAYER_H_ */
