/*
 * MutexTests.h
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

#ifndef MUTEXTESTS_H_
#define MUTEXTESTS_H_

#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/thread/Thread.h>

class MutexTests : public CPPUNIT_NS :: TestFixture
{
	CPPUNIT_TEST_SUITE (MutexTests);
	CPPUNIT_TEST (mutex_test01);
	CPPUNIT_TEST (mutex_trylock);
	CPPUNIT_TEST (rwmutex_test_readonly);
	CPPUNIT_TEST (rwmutex_test_readwrite);
	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp (void);
	void tearDown (void);

protected:
	class TestThread : public ibrcommon::JoinableThread
	{
	public:
		TestThread(ibrcommon::Mutex &m, bool &var, const bool value);
		~TestThread();

		void run() noexcept;
		void __cancellation() noexcept;

	private:
		ibrcommon::Mutex &_m;
		bool &_var;
		bool _value;
	};

	void mutex_test01();
	void mutex_trylock();
	void rwmutex_test_readonly();
	void rwmutex_test_readwrite();
};

#endif /* MUTEXTESTS_H_ */
