/*
 * TestStreamConnection.cpp
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

#include "net/TestStreamConnection.h"

#include <ibrcommon/net/socket.h>
#include <ibrcommon/net/vsocket.h>
#include <ibrcommon/net/socketstream.h>
#include <ibrdtn/streams/StreamConnection.h>
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/thread/Thread.h>
#include <ibrdtn/data/Serializer.h>
#include <ibrcommon/data/BLOB.h>

CPPUNIT_TEST_SUITE_REGISTRATION (TestStreamConnection);

void TestStreamConnection::setUp()
{
}

void TestStreamConnection::tearDown()
{
}

void TestStreamConnection::connectionUpDown()
{
	class testserver : public ibrcommon::JoinableThread, dtn::streams::StreamConnection::Callback
	{
	private:
		ibrcommon::vsocket _sockets;
		bool _running;
		bool _error;

	public:
		testserver(ibrcommon::serversocket *sock)
		: _running(true), _error(false), recv_bundles(0)
		{
			_sockets.add(sock);
			_sockets.up();
		}

		virtual ~testserver() {
			_sockets.down();
			join();
			_sockets.destroy();
		};

		void __cancellation() throw () {
			_running = false;
			_sockets.down();
		}

		void eventShutdown(dtn::streams::StreamConnection::ConnectionShutdownCases) throw () {};
		void eventTimeout() throw () {};
		void eventError() throw () {};
		void eventBundleRefused() throw () {};
		void eventBundleForwarded() throw () {};
		void eventBundleAck(const dtn::data::Length &ack) throw ()
		{
			std::cout << "server: ack received, value: " << ack << std::endl;
		};
		void eventConnectionUp(const dtn::streams::StreamContactHeader&) throw () {};
		void eventConnectionDown() throw () {};

		unsigned int recv_bundles;

	protected:
		void run() throw ()
		{
			ibrcommon::vaddress peeraddr;

			while (_running) {
				try {
					ibrcommon::socketset fds;
					_sockets.select(&fds, NULL, NULL, NULL);

					for (ibrcommon::socketset::iterator iter = fds.begin(); iter != fds.end(); ++iter)
					{
						ibrcommon::serversocket &servsock = dynamic_cast<ibrcommon::serversocket&>(**iter);

						try {
							ibrcommon::clientsocket *sock = servsock.accept(peeraddr);
							ibrcommon::socketstream conn(sock);
							dtn::streams::StreamConnection stream(*this, conn);

							// do the handshake
							stream.handshake(dtn::data::EID("dtn:server"), 0, dtn::streams::StreamContactHeader::REQUEST_ACKNOWLEDGMENTS);

							while (conn.good())
							{
								dtn::data::Bundle b;
								dtn::data::DefaultDeserializer(stream) >> b;
								// std::cout << "server: bundle received" << std::endl;
								recv_bundles++;
							}
						} catch (std::exception &e) {
							//CPPUNIT_FAIL(std::string("server error: ") + e.what());
							_error = true;
						}
					}
				} catch (const ibrcommon::vsocket_interrupt &e) {
					// excepted interruption
				} catch (const ibrcommon::socket_exception &e) {
					// unexpected socket error
					break;
				}
			}
		}
	};

	class testclient : public ibrcommon::JoinableThread, dtn::streams::StreamConnection::Callback
	{
	private:
		ibrcommon::socketstream &_client;
		dtn::streams::StreamConnection _stream;

	public:
		testclient(ibrcommon::socketstream &client)
		: _client(client), _stream(*this, _client)
		{ }

		virtual ~testclient() {
			join();
		};

		void __cancellation() throw () {
			_stream.shutdown(dtn::streams::StreamConnection::CONNECTION_SHUTDOWN_ERROR);
			_client.close();
		}

		void eventShutdown(dtn::streams::StreamConnection::ConnectionShutdownCases) throw () {};
		void eventTimeout() throw () {};
		void eventError() throw () {};
		void eventBundleRefused() throw () {};
		void eventBundleForwarded() throw () {};
		void eventBundleAck(const dtn::data::Length &ack) throw ()
		{
			// std::cout << "client: ack received, value: " << ack << std::endl;
		};

		void eventConnectionUp(const dtn::streams::StreamContactHeader&) throw () {};
		void eventConnectionDown() throw () {};

		void handshake()
		{
			// do the handshake
			_stream.handshake(dtn::data::EID("dtn:client"), 0, dtn::streams::StreamContactHeader::REQUEST_ACKNOWLEDGMENTS);
		}

		void send(int size = 2048)
		{
			dtn::data::Bundle b;
			ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();

			{
				ibrcommon::BLOB::iostream stream = ref.iostream();

				// create testing pattern, chunk-wise to conserve memory
				char pattern[2048];
				for (size_t i = 0; i < sizeof(pattern); ++i)
				{
					pattern[i] = '0';
					pattern[i] += i % 10;
				}
				std::string chunk = std::string(pattern,2048);

				while (size > 2048) {
					(*stream) << chunk;
					size-=2048;
				}
				(*stream) << chunk.substr(0,size);
			}

			b.push_back(ref);
			dtn::data::DefaultSerializer(_stream) << b;
			_stream << std::flush;
		}

		void close()
		{
			_stream.shutdown();
			stop();
		}

	protected:
		void run() throw ()
		{
			try {
				while (_client.good())
				{
					dtn::data::Bundle b;
					dtn::data::DefaultDeserializer(_stream) >> b;
					// std::cout << "client: bundle received" << std::endl;
				}
			} catch (dtn::InvalidProtocolException &e) {
				// allowed protocol exception on termination
			}
		}
	};

	//ibrcommon::File socket("/tmp/testsuite.sock");
	//testserver srv(new ibrcommon::fileserversocket(file));

	// create a new server bound to tcp port 1234
	testserver srv(new ibrcommon::tcpserversocket(1234));

	// start the server thread
	srv.start();

	ibrcommon::vaddress addr("127.0.0.1", 1234);
	ibrcommon::socketstream conn(new ibrcommon::tcpsocket(addr));
	testclient cl(conn);

	// do client-server handshake
	cl.handshake();

	// start client receiver
	cl.start();

	try {
		for (int i = 0; i < 2000; ++i)
		{
			cl.send(8192);
		}

		// close the client
		cl.close();
	} catch (const std::exception &e) {
		cl.stop();
		CPPUNIT_FAIL(std::string("client error: ") + e.what());
	}

	cl.join();

	srv.stop();
	srv.join();

	CPPUNIT_ASSERT_EQUAL((unsigned int) 2000, srv.recv_bundles);
}

