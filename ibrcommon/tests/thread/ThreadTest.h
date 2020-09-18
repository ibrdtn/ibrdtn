/*
 * ThreadTest.h
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

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#ifndef THREADTEST_H_
#define THREADTEST_H_

#include <ibrcommon/thread/Thread.h>

class ThreadTest : public CPPUNIT_NS :: TestFixture
{
	CPPUNIT_TEST_SUITE (ThreadTest);
	CPPUNIT_TEST (thread_test01);
	CPPUNIT_TEST (thread_test02);
	CPPUNIT_TEST (thread_test03);
	CPPUNIT_TEST_SUITE_END();

public:
	void setUp (void);
	void tearDown (void);

	class TestThread : public ibrcommon::JoinableThread
	{
	public:
		TestThread(size_t time = 0);
		~TestThread();

		void run() noexcept;
		void __cancellation() noexcept;
		void finally() noexcept;

		bool _running;
		size_t _finally;
		size_t _time;
	};

	class DetachedTestThread : public ibrcommon::DetachedThread
	{
	public:
		DetachedTestThread(ibrcommon::Conditional &cond, bool &run, size_t time = 0);
		~DetachedTestThread();

		void run() noexcept;
		void __cancellation() noexcept;
		void finally() noexcept;

	private:
		ibrcommon::Conditional &_cond;
		bool &_run;
		size_t _time;
	};

protected:
	void thread_test01();
	void thread_test02();
	void thread_test03();
};

#endif /* THREADTEST_H_ */
