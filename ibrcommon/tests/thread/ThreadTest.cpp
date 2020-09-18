/*
 * ThreadTest.cpp
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

#include "thread/ThreadTest.h"
#include <stdlib.h>
#include <iostream>
#include <unistd.h>

CPPUNIT_TEST_SUITE_REGISTRATION (ThreadTest);

void ThreadTest::setUp()
{
}

void ThreadTest::tearDown()
{
}

ThreadTest::DetachedTestThread::DetachedTestThread(ibrcommon::Conditional &cond, bool &run, size_t time)
 : _cond(cond), _run(run), _time(time)
{

}

ThreadTest::DetachedTestThread::~DetachedTestThread()
{

}

void ThreadTest::DetachedTestThread::run() noexcept
{
	// set run to true
	{
		ibrcommon::MutexLock l(_cond);
		_run = true;
		_cond.signal(true);
	}

	while (_run) {
		ibrcommon::Thread::sleep(_time);
	}
}

void ThreadTest::DetachedTestThread::finally() noexcept
{
}

void ThreadTest::DetachedTestThread::__cancellation() noexcept
{
	ibrcommon::MutexLock l(_cond);
	_run = false;
	_cond.signal(true);
}

ThreadTest::TestThread::TestThread(size_t time)
 : _running(true), _finally(0), _time(time)
{
}

ThreadTest::TestThread::~TestThread()
{
	join();
}

void ThreadTest::TestThread::__cancellation() noexcept
{
	_running = false;
}

void ThreadTest::TestThread::run() noexcept
{
	if (_time == 0)
	{
		while (_running) ibrcommon::Thread::sleep(100);
	}
	else
	{
		for (size_t i = 0; i < _time; ++i)
			ibrcommon::Thread::sleep(100);
	}
}

void ThreadTest::TestThread::finally() noexcept
{
	_finally++;
}

void ThreadTest::thread_test01()
{
	TestThread t;
	t.start();

	ibrcommon::Thread::sleep(100);

	t.stop();
	t.join();

	CPPUNIT_ASSERT_EQUAL(1, (int)t._finally);
}

void ThreadTest::thread_test02()
{
	TestThread t(2);
	t.start();

	ibrcommon::Thread::sleep(100);

	t.join();

	CPPUNIT_ASSERT_EQUAL(1, (int)t._finally);
}

void ThreadTest::thread_test03()
{
	bool run = false;
	ibrcommon::Conditional cond;

	DetachedTestThread *t = new DetachedTestThread(cond, run, 100);
	t->start();

	// wait until the run variable is true
	{
		ibrcommon::MutexLock l(cond);
		while (!run) { cond.wait(10); }
	}

	// stop the thread
	t->stop();

	// wait until the run variable is false
	{
		ibrcommon::MutexLock l(cond);
		while (run) { cond.wait(10); }
	}

	CPPUNIT_ASSERT_EQUAL(false, run);
}
