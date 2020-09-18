/*
 * Conditional.cpp
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
#include "ibrcommon/thread/Conditional.h"
#include "ibrcommon/thread/Thread.h"
#include <sys/time.h>
#include <unistd.h>

#ifdef __DEVELOPMENT_ASSERTIONS__
#include <cassert>
#endif

#include <pthread.h>

namespace ibrcommon
{
	Conditional::Conditional() : Mutex(MUTEX_NORMAL), _abort(false)
	{
		pthread_cond_init(&cond, &attr.attr);
	}

	Conditional::~Conditional()
	{
		pthread_cond_destroy(&cond);
	}

	bool Conditional::isLocked()
	{
		try {
			trylock();
			leave();
		} catch (const ibrcommon::MutexException&) {
			return true;
		}

		return false;
	}

	void Conditional::signal (bool broadcast) noexcept
	{
#ifdef __DEVELOPMENT_ASSERTIONS__
		// assert a locked Conditional
		assert(isLocked());
#endif

		if (broadcast)
			pthread_cond_broadcast( &cond );
		else
			pthread_cond_signal( &cond );
	}

	void Conditional::wait(size_t timeout) noexcept (false)
	{
#ifdef __DEVELOPMENT_ASSERTIONS__
		// assert a locked Conditional
		assert(isLocked());
#endif

		if (_abort) throw ConditionalAbortException(ConditionalAbortException::COND_ABORT);

		if (timeout == 0)
		{
			pthread_cond_wait( &cond, &m_mutex );
			if (_abort) throw ConditionalAbortException(ConditionalAbortException::COND_ABORT);
		}
		else
		{
			struct timespec ts;
			gettimeout(timeout, &ts);
			wait(&ts);
			if (_abort) throw ConditionalAbortException(ConditionalAbortException::COND_ABORT);
		}
	}

	void Conditional::wait(struct timespec *ts) noexcept (false)
	{
#ifdef __DEVELOPMENT_ASSERTIONS__
		// assert a locked Conditional
		assert(isLocked());
#endif

		if (_abort) throw ConditionalAbortException(ConditionalAbortException::COND_ABORT);

		int ret = pthread_cond_timedwait(&cond, &m_mutex, ts);

		if(ret == ETIMEDOUT)
		{
			throw ConditionalAbortException(ConditionalAbortException::COND_TIMEOUT, "conditional wait timed out");
		}
		else if (ret != 0)
		{
			throw ConditionalAbortException(ConditionalAbortException::COND_ERROR, "error on locking conditional mutex");
		}

		if (_abort) throw ConditionalAbortException(ConditionalAbortException::COND_ABORT);
	}

	Conditional::attribute Conditional::attr;

	Conditional::attribute::attribute()
	{
		pthread_condattr_init(&attr);

#if _POSIX_TIMERS > 0 && defined(HAVE_PTHREAD_CONDATTR_SETCLOCK)
	#if defined(_POSIX_MONOTONIC_CLOCK)
		pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	#else
		pthread_condattr_setclock(&attr, CLOCK_REALTIME);
	#endif
#endif
	}

	void Conditional::gettimeout(size_t msec, struct timespec *ts) noexcept
	{
#if _POSIX_TIMERS > 0 && defined(HAVE_PTHREAD_CONDATTR_SETCLOCK)
	#if defined(_POSIX_MONOTONIC_CLOCK)
		clock_gettime(CLOCK_MONOTONIC, ts);
	#else
		clock_gettime(CLOCK_REALTIME, ts);
	#endif
#else
		timeval tv;
		::gettimeofday(&tv, NULL);
		ts->tv_sec = tv.tv_sec;
		ts->tv_nsec = tv.tv_usec * 1000l;
#endif
		ts->tv_sec += msec / 1000;
		ts->tv_nsec += (msec % 1000) * 1000000l;
		while(ts->tv_nsec > 1000000000l) {
			++ts->tv_sec;
			ts->tv_nsec -= 1000000000l;
		}
	}

	void Conditional::abort() noexcept
	{
#ifdef __DEVELOPMENT_ASSERTIONS__
		// assert a locked Conditional
		assert(isLocked());
#endif // __DEVELOPMENT_ASSERTIONS__

		signal(true);
		_abort = true;
	}

	void Conditional::reset() noexcept
	{
		_abort = false;
	}
}

