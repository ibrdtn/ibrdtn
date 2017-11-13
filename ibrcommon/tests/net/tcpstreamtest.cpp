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
	const int testPort = 4343;
	std::cout << "Using TCP port " << testPort << " for the connection test." << std::endl;

	const int chars = 10;
	const int cons = 10;

	StreamChecker _checker(testPort, chars, cons);
	_checker.start();

	bool result = true;
	for (int i = 0; i < cons; ++i)
	{
		result = result && runTest(testPort, chars);
	}

	_checker.join();

	CPPUNIT_ASSERT_MESSAGE("baseTest failed on the client side", result);
	CPPUNIT_ASSERT_MESSAGE("baseTest failed on the server side", !_checker._error);
}

tcpstreamtest::StreamChecker::StreamChecker(const int port, const int chars, const int conns)
 : _error(false), _chars(chars), _conns(conns)
{
//	if (ibrcommon::basesocket::hasSupport(AF_INET6)) {
//		ibrcommon::vaddress addr6(ibrcommon::vaddress::VADDR_LOCALHOST, port, AF_INET6);
//		_sockets.add(new ibrcommon::tcpserversocket(addr6, 5));
//	}

	ibrcommon::vaddress addr4(ibrcommon::vaddress::VADDR_LOCALHOST, port, AF_INET);
	_sockets.add(new ibrcommon::tcpserversocket(addr4, 5));

	_sockets.up();
}

tcpstreamtest::StreamChecker::~StreamChecker()
{
	join();
	_sockets.destroy();
}

bool tcpstreamtest::runTest(const int port, const int chars)
{
	char values[chars];
	char nextChar = '0';
	for (int i = 0; i < chars; i++) {
		values[i] = nextChar++;
	}

	try {
		ibrcommon::vaddress addr("localhost", port);
		ibrcommon::socketstream client(new ibrcommon::tcpsocket(addr));
		// send some data
		for (int j = 0; j < 20; ++j)
		{
			for (int i = 0; i < 100000; ++i)
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
		std::cerr << "client error: " << + e.what() << std::endl;
		return false;
	};
	return true;
}

void tcpstreamtest::StreamChecker::__cancellation() throw ()
{
	_sockets.down();
}

void tcpstreamtest::StreamChecker::run() throw ()
{
	char values[_chars];
	char nextChar = '0';
	for (int i = 0; i < _chars; i++) {
		values[i] = nextChar++;
    }

	int connection = 0;

	try {
		while (connection < _conns)
		{
			try {
				bool hasSocket = false;
				int byte = 0;

				ibrcommon::socketset fds;
				_sockets.select(&fds, NULL, NULL, NULL);

				// Let's check all that's ready.
				for (ibrcommon::socketset::iterator iter = fds.begin(); iter != fds.end(); ++iter)
				{
					ibrcommon::serversocket &srv = dynamic_cast<ibrcommon::serversocket&>(**iter);
					ibrcommon::vaddress source;
					ibrcommon::clientsocket *socket = srv.accept(source);

					std::cout << "Cycle " << connection << ", connection accepted from " << source.toString() << std::endl;

					CPPUNIT_ASSERT(socket != NULL);

					// create a new stream
					ibrcommon::socketstream stream(socket);

					while (stream.good())
					{
						char value;

						// read one char
						stream.get(value);

						if (!stream.good()) {
							continue;
						}

						if (value != values[byte % _chars])
						{
							std::cerr << "error in byte " << byte << ", " << value << " != " << values[byte % _chars];
							_error = true;
							break;
						}

						byte++;
						hasSocket = true;
					}

					// connection should be closed
					CPPUNIT_ASSERT(stream.errmsg == ibrcommon::ERROR_CLOSED);

					stream.close();
				}

				++connection;

				CPPUNIT_ASSERT(hasSocket);
				CPPUNIT_ASSERT(byte == 20000000);
			} catch (const ibrcommon::vsocket_interrupt &e) {
				// regular interrupt
				return;
			} catch (const ibrcommon::Exception &e) {
				CPPUNIT_FAIL(std::string("server error: ") + e.what());
			};
		}
	} catch (const CppUnit::Exception &e) {
		std::cerr << "Assertion failed: " << e.what() << std::endl;
		_error = true;
	}
}
