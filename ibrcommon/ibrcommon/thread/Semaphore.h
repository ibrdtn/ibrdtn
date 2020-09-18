/*
 * Semaphore.h
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

#ifndef IBRCOMMON_SEMAPHORE_H_
#define IBRCOMMON_SEMAPHORE_H_

#include "ibrcommon/thread/Mutex.h"
#include <sys/types.h>

#ifdef HAVE_SYS_SEMAPHORE_H
#include <sys/semaphore.h>
#else
#include <semaphore.h>
#endif

namespace ibrcommon
{
	class Semaphore : public MutexInterface
	{
		public:
			Semaphore(unsigned int value = 0);
			virtual ~Semaphore();

			void wait();
			void post();

			void trylock() noexcept (false);
			void enter() noexcept (false);
			void leave() noexcept (false);
		private:
			sem_t count_sem;
	};
}
#endif /*SEMAPHORE_H_*/
