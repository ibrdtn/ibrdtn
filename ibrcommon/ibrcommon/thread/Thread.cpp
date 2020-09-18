/*
 * Thread.cpp
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

#include "ibrcommon/config.h"
#include "ibrcommon/thread/Thread.h"
#include "ibrcommon/thread/MutexLock.h"
#include "ibrcommon/Logger.h"
#include <stdexcept>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>

#ifdef __WIN32__
#include <windows.h>
#include <unistd.h>
#elif MACOS
#include <sys/param.h>
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif

#ifdef __DEVELOPMENT_ASSERTIONS__
#include <cassert>
#endif

namespace ibrcommon
{
	size_t Thread::getNumberOfProcessors()
	{
#ifdef __WIN32__
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		return sysinfo.dwNumberOfProcessors;
#elif MACOS
		int nm[2];
		size_t len = 4;
		uint32_t count;

		nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
		sysctl(nm, 2, &count, &len, NULL, 0);

		if(count < 1) {
			nm[1] = HW_NCPU;
			sysctl(nm, 2, &count, &len, NULL, 0);
			if(count < 1) { count = 1; }
		}
		return count;
#else
		return sysconf(_SC_NPROCESSORS_ONLN);
#endif
	}

	void* Thread::__execute__(void *obj) noexcept
	{
#ifdef __DEVELOPMENT_ASSERTIONS__
		// the given object should never be null
		assert(obj != NULL);
#endif
		// cast the thread object
		Thread *th = static_cast<Thread *>(obj);

		// set the state to running
		th->_state = THREAD_RUNNING;

		// run threads run routine
		th->run();

		// set the state to finalizing, blocking all threads until this is done
		th->_state = THREAD_FINALIZING;

		// call the finally method
		th->finally();

		// delete the thread-object is requested
		if (th->_detached) {
			th->_state = THREAD_FINALIZED;
			delete th;
		} else {
			// set the state to JOINABLE
			th->_state = THREAD_JOINABLE;
		}

		// exit the thread
		return NULL;
	}

	Thread::Thread(size_t size)
#ifdef __WIN32__
	 : _state(THREAD_CREATED, THREAD_FINALIZED), tid(), stack(size), priority(0), _detached(false)
#else
	 : _state(THREAD_CREATED, THREAD_FINALIZED), tid(0), stack(size), priority(0), _detached(false)
#endif
	{
		pthread_attr_init(&attr);
	}

	void Thread::yield(void)
	{
#if defined(HAVE_PTHREAD_YIELD_NP)
		pthread_yield_np();
#elif defined(HAVE_PTHREAD_YIELD)
		pthread_yield();
#else
		sched_yield();
#endif
	}

	void Thread::concurrency(int level)
	{
	#if defined(HAVE_PTHREAD_SETCONCURRENCY)
		pthread_setconcurrency(level);
	#endif
	}

	void Thread::sleep(time_t timeout)
	{
	#if defined(HAVE_PTHREAD_DELAY)
		timespec ts;
		ts.tv_sec = timeout / 1000l;
		ts.tv_nsec = (timeout % 1000l) * 1000000l;
		pthread_delay(&ts);
	#elif defined(HAVE_PTHREAD_DELAY_NP)
		timespec ts;
		ts.tv_sec = timeout / 1000l;
		ts.tv_nsec = (timeout % 1000l) * 1000000l;
		pthread_delay_np(&ts);
	#else
		::usleep(static_cast<useconds_t>(timeout) * 1000);
	#endif
	}

	Thread::~Thread()
	{
		pthread_attr_destroy(&attr);
	}

	void Thread::reset() noexcept (false)
	{
		if ( _state != THREAD_FINALIZED )
			throw ThreadException(0, "invalid state for reset");

		pthread_attr_destroy(&attr);

		_state.reset(THREAD_CREATED);
#if __WIN32__
		tid = pthread_t();
#else
		tid = 0;
#endif
		_detached = false;

		pthread_attr_init(&attr);
	}

	int Thread::kill(int sig)
	{
		if (pthread_equal(tid, pthread_t())) return -1;

		return pthread_kill(tid, sig);
	}

	void Thread::cancel() noexcept
	{
		// block multiple cancel calls
		{
			ibrcommon::ThreadsafeState<THREAD_STATE>::Locked ls = _state.lock();

			// if the thread never has been started: exit and deny any further startup
			if (ls == THREAD_CREATED)
			{
				ls = THREAD_FINALIZED;
				return;
			}

			if ((ls == THREAD_CANCELLED) || (ls == THREAD_FINALIZED) || (ls == THREAD_JOINABLE) || (ls == THREAD_FINALIZING)) return;

			// wait until a state is reached where cancellation is possible
			ls.wait(THREAD_RUNNING | THREAD_JOINABLE | THREAD_FINALIZING | THREAD_FINALIZED);

			// exit if the thread is not running
			if (ls != THREAD_RUNNING) return;

			// set state to cancelled
			ls = THREAD_CANCELLED;
		}

		// run custom cancellation
		__cancellation();
	}

	bool Thread::equal(pthread_t t1, pthread_t t2)
	{
		return (pthread_equal(t1, t2) != 0);
	}

	JoinableThread::JoinableThread(size_t size)
	 : Thread(size)
	{
	}

	JoinableThread::~JoinableThread()
	{
#ifdef __DEVELOPMENT_ASSERTIONS__
		// every thread should be joined, when the destructor is called.
		if ( _state != THREAD_FINALIZED )
		{
			std::cerr << "FAILURE: Thread not finalized before! Current state:" << _state.get() << std::endl;
			assert( _state != THREAD_FINALIZED );
		}
#endif
		join();
	}

	void JoinableThread::start(int adj) noexcept (false)
	{
		int ret;

		// switch to STARTED only if this is a fresh thread
		{
			ibrcommon::ThreadsafeState<THREAD_STATE>::Locked ls = _state.lock();

			// only start once
			if (ls != THREAD_CREATED) return;

			// set the thread state to STARTED
			ls = THREAD_STARTED;
		}

		// set priority
		priority = adj;

		// call the setup method
		setup();

		// set the state to running
		_state = THREAD_INITIALIZED;

#ifndef	__PTH__
		// modify the threads attributes - set as joinable thread
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

#ifndef ANDROID
		// ignore scheduling policy and use the same as the parent
		pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
#endif
#endif
		// we typically use "stack 1" for min stack...
#ifdef	PTHREAD_STACK_MIN
		// if the given stack size is too small...
		if(stack && stack < PTHREAD_STACK_MIN)
		{
			// set it to the min stack size
			stack = PTHREAD_STACK_MIN;
		}
#else
		// if stack size if larger than zero and smaller than two...
		if (stack && stack < 2)
		{
			// set it to zero (we will not set the stack size)
			stack = 0;
		}
#endif

#ifdef	__PTH__
		// spawn a new thread
		tid = pth_spawn(PTH_ATTR_DEFAULT, &Thread::__execute__, this);
#else
		// if the stack size is specified (> 0)
		if (stack)
		{
			// set the stack size attribute
			pthread_attr_setstacksize(&attr, stack);
		}

		// spawn a new thread
		ret = pthread_create(&tid, &attr, &Thread::__execute__, this);

		switch (ret)
		{
		case EAGAIN:
			_state = THREAD_FINALIZED;
			throw ThreadException(ret, "The system lacked the necessary resources to create another thread, or the system-imposed limit on the total number of threads in a process PTHREAD_THREADS_MAX would be exceeded.");
		case EINVAL:
			_state = THREAD_FINALIZED;
			throw ThreadException(ret, "The value specified by attr is invalid.");
		case EPERM:
			_state = THREAD_FINALIZED;
			throw ThreadException(ret, "The caller does not have appropriate permission to set the required scheduling parameters or scheduling policy.");
		}
#endif
	}

	void JoinableThread::stop() noexcept
	{
		Thread::cancel();
	}

	void JoinableThread::join(void) noexcept (false)
	{
		ibrcommon::ThreadsafeState<THREAD_STATE>::Locked ls = _state.lock();

		// if the thread never has been started: exit and deny any further startup
		if (ls == THREAD_CREATED)
		{
			ls = THREAD_FINALIZED;
			return;
		}

		// wait until the finalized state is reached
		ls.wait(THREAD_JOINABLE | THREAD_FINALIZED);

		// if the thread has been joined already: exit
		if (ls == THREAD_FINALIZED) return;

		// get the thread-id of the calling thread
		pthread_t self = pthread_self();

#ifdef __DEVELOPMENT_ASSERTIONS__
			// never try to join our own thread, check this here
			assert( !equal(tid, self) );
#endif

		// if the thread has been started, do join
		int ret = 0;
		if ((ret = pthread_join(tid, NULL)) == 0)
		{
			// set the state to joined
			ls = THREAD_FINALIZED;
		}
		else
		{
			throw ThreadException(ret, "Join on a thread failed");
		}
	}

	DetachedThread::DetachedThread(size_t size) : Thread(size)
	{
	}

	DetachedThread::~DetachedThread()
	{
	}

	void DetachedThread::start(int adj) noexcept (false)
	{
		int ret = 0;

		// switch to STARTED only if this is a fresh thread
		{
			ibrcommon::ThreadsafeState<THREAD_STATE>::Locked ls = _state.lock();

			// only start once
			if (ls != THREAD_CREATED) return;

			// set the thread state to STARTED
			ls = THREAD_STARTED;
		}

		// set the priority
		priority = adj;

		// call the setup method
		setup();

		// set the state to running
		_state = THREAD_INITIALIZED;

#ifndef	__PTH__
		// modify the threads attributes - set as detached thread
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

#ifndef ANDROID
		// ignore scheduling policy and use the same as the parent
		pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
#endif
#endif
		// we typically use "stack 1" for min stack...
#ifdef	PTHREAD_STACK_MIN
		// if the given stack size is too small...
		if(stack && stack < PTHREAD_STACK_MIN)
		{
			// set it to the min stack size
			stack = PTHREAD_STACK_MIN;
		}
#else
		// if stack size if larger than zero and smaller than two...
		if (stack && stack < 2)
		{
			// set it to zero (we will not set the stack size)
			stack = 0;
		}
#endif

#ifdef	__PTH__
		// spawn a new thread
		tid = pth_spawn(PTH_ATTR_DEFAULT, &Thread::__execute__, this);
#else
		// if the stack size is specified (> 0)
		if (stack)
		{
			// set the stack size attribute
			pthread_attr_setstacksize(&attr, stack);
		}

		// set this thread as detached
		_detached = true;

		// spawn a new thread
		ret = pthread_create(&tid, &attr, &Thread::__execute__, this);

		// check for errors
		switch (ret)
		{
		case EAGAIN:
			_state = THREAD_FINALIZED;
			throw ThreadException(ret, "The system lacked the necessary resources to create another thread, or the system-imposed limit on the total number of threads in a process PTHREAD_THREADS_MAX would be exceeded.");
		case EINVAL:
			_state = THREAD_FINALIZED;
			throw ThreadException(ret, "The value specified by attr is invalid.");
		case EPERM:
			_state = THREAD_FINALIZED;
			throw ThreadException(ret, "The caller does not have appropriate permission to set the required scheduling parameters or scheduling policy.");
		}
#endif
	}

	void DetachedThread::stop() noexcept (false)
	{
		Thread::cancel();
	}
}
