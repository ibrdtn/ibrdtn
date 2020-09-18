/*
 * RWMutex.cpp
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

#include "ibrcommon/thread/RWMutex.h"
#include <errno.h>

namespace ibrcommon
{
	RWMutex::RWMutex()
	{
		pthread_rwlock_init(&_rwlock, NULL);
	}

	RWMutex::~RWMutex()
	{
		pthread_rwlock_destroy(&_rwlock);
	}

	void RWMutex::trylock() noexcept (false)
	{
		switch (pthread_rwlock_tryrdlock(&_rwlock))
		{
		case 0:
			break;

		case EBUSY:
			throw MutexException("The mutex could not be acquired because it was already locked.");
			break;
		case EDEADLK:
			throw MutexException("The mutex could not be acquired because the current thread already locked it.");
			break;
		default:
			throw MutexException("The mutex could not be acquired. Reason unknown.");
		}
	}

	void RWMutex::enter() noexcept (false)
	{
		pthread_rwlock_rdlock(&_rwlock);
	}

	void RWMutex::leave() noexcept (false)
	{
		pthread_rwlock_unlock(&_rwlock);
	}

	void RWMutex::trylock_wr() noexcept (false)
	{
		switch (pthread_rwlock_trywrlock(&_rwlock))
		{
		case 0:
			break;

		case EBUSY:
			throw MutexException("The mutex could not be acquired because it was already locked.");
			break;
		case EDEADLK:
			throw MutexException("The mutex could not be acquired because the current thread already locked it.");
			break;
		default:
			throw MutexException("The mutex could not be acquired. Reason unknown.");
		}
	}

	void RWMutex::enter_wr() noexcept (false)
	{
		pthread_rwlock_wrlock(&_rwlock);
	}
} /* namespace ibrcommon */
