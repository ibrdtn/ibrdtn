/*
 * QueueTest.h
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

#ifndef IBRCOMMON_QUEUETEST_H_
#define IBRCOMMON_QUEUETEST_H_

#include <ibrcommon/thread/Queue.h>
#include <ibrcommon/thread/Thread.h>

class QueueTest : public CPPUNIT_NS :: TestFixture
{
	CPPUNIT_TEST_SUITE (QueueTest);
	CPPUNIT_TEST (tsq_test01);
	CPPUNIT_TEST (tsq_test02);
	CPPUNIT_TEST (tsq_test03);
	CPPUNIT_TEST (tsq_test04);
	CPPUNIT_TEST (tsq_test05);
	CPPUNIT_TEST_SUITE_END();

public:
	void setUp (void);
	void tearDown (void);

	class TestThread : public ibrcommon::JoinableThread
	{
	public:
		TestThread(size_t time = 0, size_t max = 0);
		~TestThread();

		void run() noexcept;
		virtual void __cancellation() noexcept;

		void finally() noexcept;

		size_t _count;
		size_t _time;
		size_t _max;

		ibrcommon::Queue<std::string> _queue;
	};

protected:
	void tsq_test01();
	void tsq_test02();
	void tsq_test03();
	void tsq_test04();
	void tsq_test05();
};

#endif /* IBRCOMMON_QUEUETEST_H_ */
