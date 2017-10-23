/*
 * Mutex.h
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

#ifndef IBRCOMMON_MUTEX_H_
#define IBRCOMMON_MUTEX_H_

#include <pthread.h>
#include "ibrcommon/Exceptions.h"

namespace ibrcommon
{
	class MutexException : public ibrcommon::Exception
	{
	public:
		MutexException(std::string what = "An error occured during mutex operation.") throw() : ibrcommon::Exception(what)
		{
		};
	};

	class MutexInterface
	{
	public:
		virtual ~MutexInterface() = 0;

		virtual void trylock() throw (MutexException) = 0;
		virtual void enter() throw (MutexException) = 0;
		virtual void leave() throw (MutexException) = 0;
	};

	class MutexMock : public MutexInterface
	{
	public:
		MutexMock() {};
		virtual ~MutexMock() {};

		virtual void trylock() throw (MutexException) {};
		virtual void enter() throw (MutexException) {};
		virtual void leave() throw (MutexException) {};
	};

	class Mutex : public MutexInterface
	{
		public:
			enum MUTEX_TYPE
			{
				MUTEX_NORMAL = PTHREAD_MUTEX_NORMAL,
				MUTEX_RECURSIVE = PTHREAD_MUTEX_RECURSIVE,
				MUTEX_ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK
			};

			Mutex(MUTEX_TYPE type = MUTEX_NORMAL);
			virtual ~Mutex();

			virtual void trylock() throw (MutexException);
			virtual void enter() throw (MutexException);
			virtual void leave() throw (MutexException);

			static MutexInterface& dummy();

		protected:
			pthread_mutex_t m_mutex;
			pthread_mutexattr_t m_attr;

		private:
			static MutexMock _dummy;
	};
}

#endif /*MUTEX_H_*/
