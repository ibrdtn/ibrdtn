/*
 * TCPConnection.cpp
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

#include "config.h"
#include "Configuration.h"
#include "core/BundleCore.h"
#include "core/BundleEvent.h"
#include "storage/BundleStorage.h"
#include "core/FragmentManager.h"

#include "net/TCPConvergenceLayer.h"
#include "net/ConnectionEvent.h"
#include "net/TransferAbortedEvent.h"

#include <ibrcommon/net/socket.h>
#include <ibrcommon/TimeMeasurement.h>
#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/thread/Conditional.h>
#include <ibrcommon/thread/RWLock.h>
#include <ibrcommon/Logger.h>

#include <iostream>
#include <iomanip>
#include <memory>

#ifdef WITH_TLS
#include "security/SecurityCertificateManager.h"
#include <openssl/x509.h>
#include <ibrcommon/TLSExceptions.h>
#include <ibrcommon/ssl/TLSStream.h>
#endif

namespace dtn
{
	namespace net
	{
		const std::string TCPConnection::TAG = "TCPConnection";

		/*
		 * class TCPConnection
		 */
		TCPConnection::TCPConnection(TCPConvergenceLayer &tcpsrv, const dtn::core::Node &node, ibrcommon::clientsocket *sock, const size_t timeout)
		 : _peer(), _node(node), _socket(sock), _socket_stream(NULL), _sec_stream(NULL), _protocol_stream(NULL), _sender(*this),
		   _keepalive_sender(*this, _keepalive_timeout), _timeout(timeout), _lastack(0), _resume_offset(0), _keepalive_timeout(0),
		   _callback(tcpsrv), _flags(0), _aborted(false)
		{
		}

		TCPConnection::~TCPConnection()
		{
			// join the keepalive sender thread
			_keepalive_sender.join();

			// wait until the sender thread is finished
			_sender.join();

			// clean-up
			{
				ibrcommon::RWLock l(_protocol_stream_mutex);
				delete _protocol_stream;
				_protocol_stream = NULL;
			}

			if (_sec_stream != NULL) {
				delete _sec_stream;
			}

			if ((_socket != NULL) && (_socket_stream == NULL)) {
				delete _socket;
			} else if (_socket_stream != NULL) {
				delete _socket_stream;
			}
		}

		void TCPConnection::queue(const dtn::net::BundleTransfer &job)
		{
			_sender.push(job);
		}

		const dtn::streams::StreamContactHeader& TCPConnection::getHeader() const
		{
			return _peer;
		}

		const dtn::core::Node& TCPConnection::getNode() const
		{
			return _node;
		}

		void TCPConnection::rejectTransmission()
		{
			(*getProtocolStream()).reject();
		}

		void TCPConnection::eventShutdown(dtn::streams::StreamConnection::ConnectionShutdownCases) throw ()
		{
		}

		void TCPConnection::eventTimeout() throw ()
		{
			// event
			ConnectionEvent::raise(ConnectionEvent::CONNECTION_TIMEOUT, _node);

			try {
				// stop the receiver thread
				this->stop();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void TCPConnection::eventError() throw ()
		{
		}

		void TCPConnection::initiateExtendedHandshake() throw (ibrcommon::Exception)
		{
#ifdef WITH_TLS
			/* if both nodes support TLS, activate it */
			if (_peer._flags.getBit(dtn::streams::StreamContactHeader::REQUEST_TLS)
					&& _flags.getBit(dtn::streams::StreamContactHeader::REQUEST_TLS))
			{
				try{
					ibrcommon::TLSStream &tls = dynamic_cast<ibrcommon::TLSStream&>(*_sec_stream);
					X509 *peer_cert = tls.activate();

					// check the full EID first
					const std::string cn = _peer.getEID().getString();

					try {
						try {
							dtn::security::SecurityCertificateManager::validateSubject(peer_cert, cn);
						} catch (const dtn::security::SecurityCertificateException &ex) {
							// check using the hostname
							std::string weak_cn = _peer.getEID().getHost();

							// strip leading "//"
							if (weak_cn.find_first_of("//") == 0) {
								weak_cn = weak_cn.substr(2, weak_cn.length() - 2);
							}

							try {
								dtn::security::SecurityCertificateManager::validateSubject(peer_cert, weak_cn);
							} catch (const dtn::security::SecurityCertificateException &ex_weak) {
								throw ex;
							}
						}
					} catch (const dtn::security::SecurityCertificateException &ex) {
						IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, warning) << ex.what() << IBRCOMMON_LOGGER_ENDL;
						throw ibrcommon::TLSCertificateVerificationException(ex.what());
					}
				} catch (const std::exception&) {
					if (dtn::daemon::Configuration::getInstance().getSecurity().TLSRequired()){
						/* close the connection */
						IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 20) << "TLS failed, closing the connection." << IBRCOMMON_LOGGER_ENDL;
						throw;
					} else {
						IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 20) << "TLS failed, continuing unauthenticated." << IBRCOMMON_LOGGER_ENDL;
					}
				}
			} else {
				/* TLS not supported by both Nodes, check if its required */
				if (dtn::daemon::Configuration::getInstance().getSecurity().TLSRequired()){
					/* close the connection */
					throw ibrcommon::TLSException("TLS not supported by peer.");
				} else if(_flags & dtn::streams::StreamContactHeader::REQUEST_TLS){
					IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, notice) << "TLS not supported by peer. Continuing without TLS." << IBRCOMMON_LOGGER_ENDL;
				}
				/* else: this node does not support TLS, should have already printed a warning */
			}
#endif
		}

		void TCPConnection::eventConnectionUp(const dtn::streams::StreamContactHeader &header) throw ()
		{
			_peer = header;

			// copy old attributes and urls to the new node object
			Node n_old = _node;
			_node = Node(header._localeid);
			_node += n_old;

			// check if the peer has the same EID
			if (_node.getEID() == dtn::core::BundleCore::getInstance().local)
			{
				// abort the connection
				shutdown();

				IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, warning) << "connection to local endpoint rejected" << IBRCOMMON_LOGGER_ENDL;
				return;
			}

			_keepalive_timeout = header._keepalive * 1000;

			try {
				// initiate extended handshake (e.g. TLS)
				initiateExtendedHandshake();
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, warning) << ex.what() << IBRCOMMON_LOGGER_ENDL;

				// abort the connection
				shutdown();
				return;
			}

			// set the timer
			timeval timeout;
			timerclear(&timeout);

			// set the incoming timer if set (> 0)
			if (_peer._keepalive > 0)
			{
				timeout.tv_sec = header._keepalive * 2;
			}

			// change time-out
			_socket_stream->setTimeout(timeout);

			try {
				// enable idle timeout
				size_t _idle_timeout = dtn::daemon::Configuration::getInstance().getNetwork().getTCPIdleTimeout();
				if (_idle_timeout > 0)
				{
					(*getProtocolStream()).enableIdleTimeout(_idle_timeout);
				}
			} catch (const ibrcommon::Exception&) {};

			// raise up event
			ConnectionEvent::raise(ConnectionEvent::CONNECTION_UP, _node);
		}

		void TCPConnection::eventConnectionDown() throw ()
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 40) << "eventConnectionDown()" << IBRCOMMON_LOGGER_ENDL;

			try {
				// shutdown the keepalive sender thread
				_keepalive_sender.stop();

				// stop the sender
				_sender.stop();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}

			if (_peer._localeid != dtn::data::EID())
			{
				// event
				ConnectionEvent::raise(ConnectionEvent::CONNECTION_DOWN, _node);
			}
		}

		void TCPConnection::eventBundleRefused() throw ()
		{
			ibrcommon::Queue<dtn::net::BundleTransfer>::Locked l = _sentqueue.exclusive();

			// stop here if the queue is already empty
			if (l.empty()) {
				IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, error) << "transfer refused without a bundle in queue" << IBRCOMMON_LOGGER_ENDL;
				return;
			}

			// get the job on top of the sent queue
			dtn::net::BundleTransfer &job = l.front();

			// abort the transmission
			job.abort(dtn::net::TransferAbortedEvent::REASON_REFUSED);

			// set ACK to zero
			_lastack = 0;

			// release the job
			l.pop();
		}

		void TCPConnection::eventBundleForwarded() throw ()
		{
			ibrcommon::Queue<dtn::net::BundleTransfer>::Locked l = _sentqueue.exclusive();

			// stop here if the queue is already empty
			if (l.empty()) {
				IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, error) << "transfer completed without a bundle in queue" << IBRCOMMON_LOGGER_ENDL;
				return;
			}

			// get the job on top of the sent queue
			dtn::net::BundleTransfer &job = l.front();

			// mark job as complete
			job.complete();

			// set ACK to zero
			_lastack = 0;

			// release the job
			l.pop();
		}

		void TCPConnection::eventBundleAck(const dtn::data::Length &ack) throw ()
		{
			_lastack = ack;
		}

		void TCPConnection::addTrafficIn(size_t amount) throw ()
		{
			_callback.addTrafficIn(amount);
		}

		void TCPConnection::addTrafficOut(size_t amount) throw ()
		{
			_callback.addTrafficOut(amount);
		}

		void TCPConnection::initialize() throw ()
		{
			// start the receiver for incoming bundles + handshake
			try {
				start();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void TCPConnection::shutdown() throw ()
		{
			try {
				// shutdown
				(*getProtocolStream()).shutdown(dtn::streams::StreamConnection::CONNECTION_SHUTDOWN_ERROR);
			} catch (const ibrcommon::Exception&) {};

			try {
				// abort the connection thread
				ibrcommon::DetachedThread::stop();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, error) << "shutdown failed (" << ex.what() << ")" << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void TCPConnection::__cancellation() throw ()
		{
			// mark the connection as aborted
			_aborted = true;

			// close the stream
			if (_socket_stream != NULL) _socket_stream->close();
		}

		void TCPConnection::finally() throw ()
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 60) << "TCPConnection down" << IBRCOMMON_LOGGER_ENDL;

			try {
				// shutdown the keepalive sender thread
				_keepalive_sender.stop();

				// shutdown the sender thread
				_sender.stop();
			} catch (const std::exception&) { };

			// close the tcpstream
			if (_socket_stream != NULL) _socket_stream->close();

			try {
				_callback.connectionDown(this);
			} catch (const ibrcommon::MutexException&) { };

			// clear the queue
			clearQueue();
		}

		void TCPConnection::setup() throw ()
		{
			_flags |= dtn::streams::StreamContactHeader::REQUEST_ACKNOWLEDGMENTS;
			_flags |= dtn::streams::StreamContactHeader::REQUEST_NEGATIVE_ACKNOWLEDGMENTS;

			if (dtn::daemon::Configuration::getInstance().getNetwork().doFragmentation())
			{
				_flags |= dtn::streams::StreamContactHeader::REQUEST_FRAGMENTATION;
			}
		}

		void TCPConnection::__setup_socket(ibrcommon::clientsocket *sock, bool server)
		{
			if ( dtn::daemon::Configuration::getInstance().getNetwork().getTCPOptionNoDelay() )
			{
				sock->set(ibrcommon::clientsocket::NO_DELAY, true);
			}

			_socket_stream = new ibrcommon::socketstream(sock);

			// set an initial time-out
			timeval timeout;
			timerclear(&timeout);
			timeout.tv_sec = _timeout;
			_socket_stream->setTimeout(timeout);

#ifdef WITH_TLS
			// initialize security layer if available
			_sec_stream = new ibrcommon::TLSStream(_socket_stream);
			if (server) dynamic_cast<ibrcommon::TLSStream&>(*_sec_stream).setServer(true);
			else dynamic_cast<ibrcommon::TLSStream&>(*_sec_stream).setServer(false);
#endif

			// create a new stream connection
			dtn::data::Length chunksize = dtn::daemon::Configuration::getInstance().getNetwork().getTCPChunkSize();

			ibrcommon::RWLock l(_protocol_stream_mutex);
			if (_protocol_stream != NULL) delete _protocol_stream;
			_protocol_stream = new dtn::streams::StreamConnection(*this, (_sec_stream == NULL) ? *_socket_stream : *_sec_stream, chunksize);
			_protocol_stream->exceptions(std::ios::badbit | std::ios::eofbit);
		}

		void TCPConnection::connect()
		{
			// do not connect to other hosts if we are in server
			if (_socket != NULL) return;

			// do not connect to anyone if we are already connected
			if (_socket_stream != NULL) return;

			// variables for address and port
			std::string address = "0.0.0.0";
			unsigned int port = 0;

			// try to connect to the other side
			try {
				const std::list<dtn::core::Node::URI> uri_list = _node.get(dtn::core::Node::CONN_TCPIP);

				for (std::list<dtn::core::Node::URI>::const_iterator iter = uri_list.begin(); iter != uri_list.end(); ++iter)
				{
					// break-out if the connection has been aborted
					if (_aborted) throw ibrcommon::socket_exception("connection has been aborted");

					try {
						// decode address and port
						const dtn::core::Node::URI &uri = (*iter);
						uri.decode(address, port);

						// create a virtual address to connect to
						ibrcommon::vaddress addr(address, port);

						IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 15) << "Initiate TCP connection to " << address << ":" << port << IBRCOMMON_LOGGER_ENDL;

						// create a new tcpsocket
						timeval tv;
						timerclear(&tv);
						tv.tv_sec = _timeout;

						// create a new tcp connection via the tcpsocket object
						ibrcommon::tcpsocket *client = new ibrcommon::tcpsocket(addr, &tv);

						try {
							// connect to the node
							client->up();

							// setup a new tcp connection
							__setup_socket(client, false);

							// add TCP connection descriptor to the node object
							_node.clear();
							_node.add( dtn::core::Node::URI(Node::NODE_CONNECTED, Node::CONN_TCPIP, uri.value, 0, 10) );

							// connection successful
							return;
						} catch (const ibrcommon::socket_exception&) {
							delete client;
						}
					} catch (const ibrcommon::socket_exception&) { };
				}

				// no connection has been established
				throw ibrcommon::socket_exception("no address available to connect");

			} catch (const ibrcommon::socket_exception&) {
				// error on open, requeue all bundles in the queue
				IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, warning) << "connection to " << _node.toString() << " failed" << IBRCOMMON_LOGGER_ENDL;
				try {
					(*getProtocolStream()).shutdown(dtn::streams::StreamConnection::CONNECTION_SHUTDOWN_ERROR);
				} catch (const ibrcommon::Exception&) {};
				throw;
			} catch (const std::bad_cast&) { };
		}

		void TCPConnection::run() throw ()
		{
			try {
				if (_socket == NULL) {
					// connect to the peer
					connect();
				} else {
					// accept remote connection as server
					__setup_socket(_socket, true);
				}

				TCPConnection::safe_streamconnection sc = getProtocolStream();
				std::iostream &stream = (*sc);

				// do the handshake
				(*sc).handshake(dtn::core::BundleCore::local, _timeout, _flags);

				// start the sender
				_sender.start();

				// start keepalive sender
				_keepalive_sender.start();

				// create a filter context
				dtn::core::FilterContext context;
				context.setPeer(_peer._localeid);
				context.setProtocol(_callback.getDiscoveryProtocol());

				// create a deserializer for next bundle
				dtn::data::DefaultDeserializer deserializer(stream, dtn::core::BundleCore::getInstance());

				while (!(*sc).eof())
				{
					try {
						// create a new empty bundle
						dtn::data::Bundle bundle;

						// check if the stream is still good
						if (!stream.good()) throw ibrcommon::IOException("stream went bad");

						// enable/disable fragmentation support according to the contact header.
						deserializer.setFragmentationSupport(_peer._flags.getBit(dtn::streams::StreamContactHeader::REQUEST_FRAGMENTATION));

						// read the bundle (or the fragment if fragmentation is enabled)
						deserializer >> bundle;

						// check the bundle
						if ( ( bundle.destination == EID() ) || ( bundle.source == EID() ) )
						{
							// invalid bundle!
							throw dtn::data::Validator::RejectedException("destination or source EID is null");
						}

						// push bundle through the filter routines
						context.setBundle(bundle);
						BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::INPUT, context, bundle);

						switch (ret) {
							case BundleFilter::ACCEPT:
								// inject bundle into core
								dtn::core::BundleCore::getInstance().inject(_peer._localeid, bundle, false);
								break;

							case BundleFilter::REJECT:
								throw dtn::data::Validator::RejectedException("rejected by input filter");
								break;

							case BundleFilter::DROP:
								break;
						}
					}
					catch (const dtn::data::Validator::RejectedException &ex)
					{
						// bundle rejected
						rejectTransmission();

						// display the rejection
						IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 2) << "bundle has been rejected: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}
					catch (const dtn::InvalidDataException &ex) {
						// bundle rejected
						rejectTransmission();

						// display the rejection
						IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 2) << "invalid bundle-data received: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}

					yield();
				}
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG(TCPConnection::TAG, error) << "failed to start thread in TCPConnection\n" << ex.what() << IBRCOMMON_LOGGER_ENDL;
				try {
					(*getProtocolStream()).shutdown(dtn::streams::StreamConnection::CONNECTION_SHUTDOWN_ERROR);
				} catch (const ibrcommon::Exception&) {};
			} catch (const std::exception &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 10) << "run(): std::exception (" << ex.what() << ")" << IBRCOMMON_LOGGER_ENDL;
				try {
					(*getProtocolStream()).shutdown(dtn::streams::StreamConnection::CONNECTION_SHUTDOWN_ERROR);
				} catch (const ibrcommon::Exception&) {};
			}
		}

		TCPConnection::safe_streamconnection TCPConnection::getProtocolStream() throw (ibrcommon::Exception)
		{
			return safe_streamconnection(_protocol_stream, _protocol_stream_mutex);
		}

		TCPConnection::KeepaliveSender::KeepaliveSender(TCPConnection &connection, size_t &keepalive_timeout)
		 : _connection(connection), _keepalive_timeout(keepalive_timeout)
		{

		}

		TCPConnection::KeepaliveSender::~KeepaliveSender()
		{
		}

		void TCPConnection::KeepaliveSender::run() throw ()
		{
			try {
				ibrcommon::MutexLock l(_wait);
				while (true)
				{
					try {
						_wait.wait(_keepalive_timeout);
					} catch (const ibrcommon::Conditional::ConditionalAbortException &ex) {
						if (ex.reason == ibrcommon::Conditional::ConditionalAbortException::COND_TIMEOUT)
						{
							// send a keepalive
							_connection.keepalive();
						}
						else
						{
							throw;
						}
					}
				}
			} catch (const std::exception&) { };
		}

		void TCPConnection::KeepaliveSender::__cancellation() throw ()
		{
			ibrcommon::MutexLock l(_wait);
			_wait.abort();
		}

		TCPConnection::Sender::Sender(TCPConnection &connection)
		 : _connection(connection)
		{
		}

		TCPConnection::Sender::~Sender()
		{
		}

		void TCPConnection::Sender::__cancellation() throw ()
		{
			// cancel the main thread in here
			ibrcommon::Queue<dtn::net::BundleTransfer>::abort();
		}

		void TCPConnection::Sender::run() throw ()
		{
			try {
				dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();

				TCPConnection::safe_streamconnection sc = _connection.getProtocolStream();
				std::iostream &stream = (*sc);

				// create a filter context
				dtn::core::FilterContext context;
				context.setPeer(_connection._peer._localeid);
				context.setProtocol(_connection._callback.getDiscoveryProtocol());

				// create a serializer
				dtn::data::DefaultSerializer serializer(stream);

				while (stream.good())
				{
					dtn::net::BundleTransfer transfer = ibrcommon::Queue<dtn::net::BundleTransfer>::poll();

					// check if the transfer is directed to the connected neighbor
					if (transfer.getNeighbor() != _connection.getNode().getEID()) continue;

					try {
						// read the bundle out of the storage
						dtn::data::Bundle bundle = storage.get(transfer.getBundle());

						// push bundle through the filter routines
						context.setBundle(bundle);
						BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::OUTPUT, context, bundle);

						switch (ret) {
							case BundleFilter::ACCEPT:
								break;
							case BundleFilter::REJECT:
							case BundleFilter::DROP:
								transfer.abort(dtn::net::TransferAbortedEvent::REASON_REFUSED_BY_FILTER);
								continue;
						}

						// send bundle
						// get the offset, if this bundle has been reactively fragmented before
						if (dtn::daemon::Configuration::getInstance().getNetwork().doFragmentation()
								&& !bundle.get(dtn::data::PrimaryBlock::DONT_FRAGMENT))
						{
							_connection._resume_offset = dtn::core::FragmentManager::getOffset(_connection.getNode().getEID(), bundle);
						}
						else
						{
							_connection._resume_offset = 0;
						}

						// put the bundle into the sentqueue
						_connection._sentqueue.push(transfer);

						try {
							// activate exceptions for this method
							if (!stream.good()) throw ibrcommon::IOException("stream went bad");

							if (_connection._resume_offset > 0)
							{
								IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 4) << "Resume transfer of bundle " << bundle.toString() << " to " << _connection.getNode().getEID().getString() << ", offset: " << _connection._resume_offset << IBRCOMMON_LOGGER_ENDL;

								// transmit the fragment
								serializer << dtn::data::BundleFragment(bundle, _connection._resume_offset, -1);
							}
							else
							{
								// transmit the bundle
								serializer << bundle;
							}

							// flush the stream
							stream << std::flush;
						} catch (const ibrcommon::Exception &ex) {
							// the connection not available
							IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 10) << "connection error: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

							// forward exception
							throw;
						}
					} catch (const dtn::storage::NoBundleFoundException&) {
						// send transfer aborted event
						transfer.abort(dtn::net::TransferAbortedEvent::REASON_BUNDLE_DELETED);
					}

					// idle a little bit
					yield();
				}
			} catch (const ibrcommon::QueueUnblockedException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 50) << "Sender::run(): aborted" << IBRCOMMON_LOGGER_ENDL;
				return;
			} catch (const std::exception &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(TCPConnection::TAG, 10) << "Sender terminated by exception: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}

			_connection.stop();
		}

		void TCPConnection::clearQueue()
		{
			// requeue all bundles still in transit
			ibrcommon::Queue<dtn::net::BundleTransfer>::Locked l = _sentqueue.exclusive();

			while (!l.empty())
			{
				// get the job on top of the sent queue
				const dtn::net::BundleTransfer &job = l.front();

				if ((_lastack > 0) && (_peer._flags.getBit(dtn::streams::StreamContactHeader::REQUEST_FRAGMENTATION)))
				{
					// some data are already acknowledged
					// store this information in the fragment manager
					dtn::core::FragmentManager::setOffset(_peer.getEID(), job.getBundle(), _lastack, _resume_offset);
				}

				// set last ack to zero
				_lastack = 0;

				// release the job
				l.pop();
			}
		}

#ifdef WITH_TLS
		void dtn::net::TCPConnection::enableTLS()
		{
			_flags.setBit(dtn::streams::StreamContactHeader::REQUEST_TLS, true);
		}
#endif

		void TCPConnection::keepalive()
		{
			(*getProtocolStream()).keepalive();
		}

		bool TCPConnection::good() const
		{
			return _socket_stream->good();
		}

		void TCPConnection::Sender::finally() throw ()
		{
		}

		bool TCPConnection::match(const dtn::core::Node &n) const
		{
			return (_node == n);
		}

		bool TCPConnection::match(const dtn::data::EID &destination) const
		{
			return _node.getEID().sameHost(destination);
		}

		bool TCPConnection::match(const NodeEvent &evt) const
		{
			const dtn::core::Node &n = evt.getNode();
			return match(n);
		}
	}
}
