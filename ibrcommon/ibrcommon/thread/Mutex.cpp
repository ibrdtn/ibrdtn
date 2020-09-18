/*
 * Mutex.cpp
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
#include "ibrcommon/thread/Mutex.h"
#include <errno.h>

namespace ibrcommon
{
	MutexInterface::~MutexInterface()
	{
	}

	Mutex::Mutex(MUTEX_TYPE type)
	{
		pthread_mutexattr_init(&m_attr);
		pthread_mutexattr_settype(&m_attr, type);
		pthread_mutex_init(&m_mutex, &m_attr);
	}

	Mutex::~Mutex()
	{
		pthread_mutex_destroy( &m_mutex );
		pthread_mutexattr_destroy(&m_attr);
	}

	void Mutex::trylock() noexcept (false)
	{
		int ret = pthread_mutex_trylock( &m_mutex );

		switch (ret)
		{
		case 0:
			break;

		case EBUSY:
			throw MutexException("The mutex could not be acquired because it was already locked.");
			break;
		}
	}

	void Mutex::enter() noexcept (false)
	{
		switch (pthread_mutex_lock( &m_mutex ))
		{
		case 0:
			return;

//		case EINVAL:
//			throw MutexException("The mutex was created with the protocol attribute having the value PTHREAD_PRIO_PROTECT and the calling thread's priority is higher than the mutex's current priority ceiling.");

		// The pthread_mutex_trylock() function will fail if:

		case EBUSY:
			throw MutexException("The mutex could not be acquired because it was already locked.");

		// The pthread_mutex_lock(), pthread_mutex_trylock() and pthread_mutex_unlock() functions may fail if:

		case EINVAL:
			throw MutexException("The value specified by mutex does not refer to an initialised mutex object.");

		case EAGAIN:
			throw MutexException("The mutex could not be acquired because the maximum number of recursive locks for mutex has been exceeded.");

		// The pthread_mutex_lock() function may fail if:

		case EDEADLK:
			throw MutexException("The current thread already owns the mutex.");

		// The pthread_mutex_unlock() function may fail if:

		case EPERM:
			throw MutexException("The current thread does not own the mutex.");

		default:
			throw MutexException("can not lock the mutex");
		}
	}

	void Mutex::leave() noexcept (false)
	{
		if (0 != pthread_mutex_unlock( &m_mutex ))
		{
			throw MutexException("can not unlock mutex");
		}
	}

	MutexMock Mutex::_dummy;

	MutexInterface& Mutex::dummy()
	{
		return _dummy;
	}
}
