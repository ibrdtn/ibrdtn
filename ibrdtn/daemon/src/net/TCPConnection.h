/*
 * TCPConnection.h
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

#ifndef TCPCONNECTION_H_
#define TCPCONNECTION_H_

#include "core/NodeEvent.h"
#include "net/BundleTransfer.h"

#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/EID.h>
#include <ibrdtn/data/MetaBundle.h>
#include <ibrdtn/data/Serializer.h>

#include <ibrdtn/streams/StreamConnection.h>
#include <ibrdtn/streams/StreamContactHeader.h>

#include <ibrcommon/net/socket.h>
#include <ibrcommon/net/socketstream.h>
#include <ibrcommon/thread/Queue.h>
#include <ibrcommon/thread/SharedReference.h>

#include <memory>

namespace dtn
{
	namespace net
	{
		class TCPConvergenceLayer;

		class TCPConnection : public dtn::streams::StreamConnection::Callback, public ibrcommon::DetachedThread
		{
			const static std::string TAG;
		public:
			/**
			 * Constructor for a new TCPConnection object.
			 * @param tcpsrv
			 * @param stream TCP stream to talk to the other peer.
			 * @param name
			 * @param timeout
			 * @return
			 */
			TCPConnection(TCPConvergenceLayer &tcpsrv, const dtn::core::Node &node, ibrcommon::clientsocket *sock, const size_t timeout = 10);

			/**
			 * Destructor
			 * @return
			 */
			virtual ~TCPConnection();

			/**
			 * This method is called after accept()
			 */
			virtual void initialize() throw ();

			/**
			 * shutdown the whole tcp connection
			 */
			void shutdown() throw ();

			/**
			 * Get the header of this connection
			 * @return
			 */
			const dtn::streams::StreamContactHeader& getHeader() const;

			/**
			 * Get the associated Node object
			 * @return
			 */
			const dtn::core::Node& getNode() const;

			/**
			 * callback methods for tcpstream
			 */
			virtual void eventShutdown(dtn::streams::StreamConnection::ConnectionShutdownCases csc) throw ();
			virtual void eventTimeout() throw ();
			virtual void eventError() throw ();
			virtual void eventConnectionUp(const dtn::streams::StreamContactHeader &header) throw ();
			virtual void eventConnectionDown() throw ();

			virtual void eventBundleRefused() throw ();
			virtual void eventBundleForwarded() throw ();
			virtual void eventBundleAck(const dtn::data::Length &ack) throw ();

			virtual void addTrafficIn(size_t) throw ();
			virtual void addTrafficOut(size_t) throw ();

			dtn::core::Node::Protocol getDiscoveryProtocol() const;

			/**
			 * queue a bundle for this connection
			 * @param bundle
			 */
			void queue(const dtn::net::BundleTransfer &job);

			bool match(const dtn::core::Node &n) const;
			bool match(const dtn::data::EID &destination) const;
			bool match(const dtn::core::NodeEvent &evt) const;

#ifdef WITH_TLS
			/*!
			 * \brief enables TLS for this connection
			 */
			void enableTLS();
#endif

		protected:
			void rejectTransmission();

			void setup() throw ();
			void connect();
			void run() throw ();
			void finally() throw ();
			void __cancellation() throw ();

			void clearQueue();

			void keepalive();
			bool good() const;

			void initiateExtendedHandshake() throw (ibrcommon::Exception);

		private:
			class KeepaliveSender : public ibrcommon::JoinableThread
			{
			public:
				KeepaliveSender(TCPConnection &connection, size_t &keepalive_timeout);
				~KeepaliveSender();

				/**
				 * run method of the thread
				 */
				void run() throw ();

				/**
				 * soft-cancellation function
				 */
				void __cancellation() throw ();

			private:
				ibrcommon::Conditional _wait;
				TCPConnection &_connection;
				size_t &_keepalive_timeout;
			};

			class Sender : public ibrcommon::JoinableThread, public ibrcommon::Queue<dtn::net::BundleTransfer>
			{
			public:
				Sender(TCPConnection &connection);
				virtual ~Sender();

			protected:
				void run() throw ();
				void finally() throw ();
				void __cancellation() throw ();

			private:
				TCPConnection &_connection;
			};

			void __setup_socket(ibrcommon::clientsocket *sock, bool server);

			// lock object for the procotol stream
			typedef ibrcommon::SharedReference<dtn::streams::StreamConnection> safe_streamconnection;

			/**
			 * Return a thread-safe reference to the protocol stream
			 * This method will throw an exception if the stream is NULL
			 */
			safe_streamconnection getProtocolStream() throw (ibrcommon::Exception);

			dtn::streams::StreamContactHeader _peer;
			dtn::core::Node _node;

			ibrcommon::clientsocket *_socket;

			ibrcommon::socketstream *_socket_stream;

			// optional security layer between socketstream and bundle protocol layer
			std::iostream *_sec_stream;

			// mutex for the protocol stream
			ibrcommon::RWMutex _protocol_stream_mutex;

			// this connection stream implements the bundle protocol
			dtn::streams::StreamConnection *_protocol_stream;

			// This thread gets awaiting bundles of the queue
			// and transmit them to the peer.
			Sender _sender;

			// Keepalive sender
			KeepaliveSender _keepalive_sender;

			// handshake variables
			size_t _timeout;

			ibrcommon::Queue<dtn::net::BundleTransfer> _sentqueue;
			dtn::data::Length _lastack;
			dtn::data::Length _resume_offset;
			size_t _keepalive_timeout;

			TCPConvergenceLayer &_callback;

			/* flags to be used in this nodes StreamContactHeader */
			dtn::data::Bitset<dtn::streams::StreamContactHeader::HEADER_BITS> _flags;

			/* with this boolean the connection is marked as aborted */
			bool _aborted;
		};
	}
}

#endif /* TCPCONNECTION_H_ */
