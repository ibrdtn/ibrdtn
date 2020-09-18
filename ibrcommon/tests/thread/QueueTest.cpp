/*
 * QueueTest.cpp
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

#include "thread/QueueTest.h"
#include <stdlib.h>
#include <iostream>
#include <unistd.h>

CPPUNIT_TEST_SUITE_REGISTRATION (QueueTest);

void QueueTest::setUp()
{
}

void QueueTest::tearDown()
{
}

QueueTest::TestThread::TestThread(size_t time, size_t max)
 : _count(0), _time(time), _max(max)
{
}

QueueTest::TestThread::~TestThread()
{
	join();
}

void QueueTest::TestThread::run() noexcept
{
	try {
		while (true)
		{
			std::string data = _queue.poll();
			_count++;

			if (_time > 0) ibrcommon::Thread::sleep(_time);

			if ((_max > 0) && (_max <= _count)) return;
		}
	} catch (const std::exception&) {

	}
}

void QueueTest::TestThread::__cancellation() noexcept
{
	_queue.abort();
}

void QueueTest::TestThread::finally() noexcept
{
}

void QueueTest::tsq_test01()
{
	TestThread t;
	t.start();

	ibrcommon::Thread::sleep(100);

	t.stop();
	t.join();
}

void QueueTest::tsq_test02()
{
	TestThread t(0, 2);
	t.start();

	t._queue.push("hallo");
	t._queue.push("welt");

	t.join();

	CPPUNIT_ASSERT_EQUAL((size_t)2, t._count);
}

void QueueTest::tsq_test03()
{
	TestThread t;
	t.start();

	t._queue.push("hallo");
	t._queue.push("welt");

	ibrcommon::Thread::sleep(100);

	t._queue.abort();
	t.join();

	CPPUNIT_ASSERT_EQUAL((size_t)2, t._count);
}

void QueueTest::tsq_test04()
{
	TestThread t(200);
	t.start();

	t._queue.push("hallo");
	t._queue.push("welt");

	ibrcommon::Thread::sleep(100);

	t._queue.abort();
	t.join();

	CPPUNIT_ASSERT_EQUAL((size_t)2, t._count);
}

void QueueTest::tsq_test05()
{
	for (int i = 0; i < 100; ++i)
	{
		TestThread t(200 / (i+1));
		t.start();

		for (int j = 0; j < i; ++j)
		{
			t._queue.push("hallo");
			t._queue.push("welt");
		}

		ibrcommon::Thread::sleep(i);

		t._queue.abort();
		t.join();

		//CPPUNIT_ASSERT_EQUAL((size_t)2, t._count);
	}
}
