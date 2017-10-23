/*
 * tcpstreamtest.cpp
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

#include "net/tcpstreamtest.h"
#include "ibrcommon/thread/MutexLock.h"
#include "ibrcommon/net/socketstream.h"

CPPUNIT_TEST_SUITE_REGISTRATION (tcpstreamtest);

void tcpstreamtest :: setUp (void)
{
}

void tcpstreamtest :: tearDown (void)
{
}

void tcpstreamtest :: baseTest (void)
{
	StreamChecker _checker(4343, 10);

	// start the streamchecker
	_checker.start();

	for (int i = 0; i < 10; ++i)
	{
		runTest();
	}

	_checker.stop();
	_checker.join();

	CPPUNIT_ASSERT(!_checker._error);
}

tcpstreamtest::StreamChecker::StreamChecker(int port, int chars)
 : _error(false), _running(true), _chars(chars)
{
	_sock.add(new ibrcommon::tcpserversocket(port));
	_sock.up();
}

tcpstreamtest::StreamChecker::~StreamChecker()
{
	join();
	_sock.destroy();
}

void tcpstreamtest::runTest()
{
	char values[10] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };

	try {
		ibrcommon::vaddress addr("127.0.0.1", 4343);
		ibrcommon::socketstream client(new ibrcommon::tcpsocket(addr));

		// send some data
		for (size_t j = 0; j < 20; ++j)
		{
			for (size_t i = 0; i < 100000; ++i)
			{
				for (int k = 0; k < 10; ++k)
				{
					client.put(values[k]);
				}
			}
		}

		client.flush();
		client.close();
	} catch (const ibrcommon::vsocket_interrupt &e) {
		// select interrupted
	} catch (const ibrcommon::socket_exception &e) {
		CPPUNIT_FAIL(std::string("client error: ") + e.what());
	};
}

void tcpstreamtest::StreamChecker::__cancellation() throw ()
{
	_running = false;
	_sock.down();
}

void tcpstreamtest::StreamChecker::setup() throw () {
	_running = true;
}

void tcpstreamtest::StreamChecker::run() throw ()
{
	try {
		char values[10] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
		ibrcommon::clientsocket *socket = NULL;

		while (_running) {
			try {
				ibrcommon::socketset fds;
				_sock.select(&fds, NULL, NULL, NULL);

				for (ibrcommon::socketset::iterator iter = fds.begin(); iter != fds.end(); ++iter)
				{
					ibrcommon::serversocket &srv = dynamic_cast<ibrcommon::serversocket&>(**iter);
					ibrcommon::vaddress source;
					socket = srv.accept(source);

//					std::cout << "connection accepted from " << source.toString() << std::endl;

					CPPUNIT_ASSERT(socket != NULL);

					int byte = 0;

					// create a new stream
					ibrcommon::socketstream stream(socket);

					while (stream.good())
					{
						char value;

						// read one char
						stream.get(value);

						if ((value != values[byte % _chars]) && stream.good())
						{
							std::cout << "error in byte " << byte << ", " << value << " != " << values[byte % _chars] << std::endl;
							break;
						}

						byte++;
					}

					// connection should be closed
					CPPUNIT_ASSERT(stream.errmsg == ibrcommon::ERROR_CLOSED);

					stream.close();

					CPPUNIT_ASSERT(byte == 20000001);
				}
			} catch (const ibrcommon::vsocket_interrupt &e) {
				// regular interrupt
				return;
			} catch (const ibrcommon::Exception &e) {
				CPPUNIT_FAIL(std::string("server error: ") + e.what());
			};
		}
	} catch (const CppUnit::Exception &e) {
		_error = true;
	}
}
