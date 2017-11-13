/*
 * tcpstreamtest.h
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

#ifndef TCPSTREAMTEST_H_
#define TCPSTREAMTEST_H_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "ibrcommon/thread/Thread.h"
#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/thread/Conditional.h>
#include <ibrcommon/net/vsocket.h>


class tcpstreamtest : public CPPUNIT_NS :: TestFixture
{
	class StreamChecker : public ibrcommon::JoinableThread
	{
	public:
		StreamChecker(const int port, const int chars = 10, const int conns = 10);
		~StreamChecker();

		virtual void run() throw ();
		virtual void __cancellation() throw ();

		bool _error;

	private:
		ibrcommon::vsocket _sockets;
		int _chars;
		int _conns;
	};

	CPPUNIT_TEST_SUITE (tcpstreamtest);
	CPPUNIT_TEST (baseTest);
	CPPUNIT_TEST_SUITE_END ();

	public:
		void setUp (void);
		void tearDown (void);

	protected:
		void baseTest (void);
		bool runTest (const int port, const int chars);
};

#endif /* TCPSTREAMTEST_H_ */
