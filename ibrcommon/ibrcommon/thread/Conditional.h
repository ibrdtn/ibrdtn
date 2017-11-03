/*
 * Conditional.h
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

#ifndef IBRCOMMON_CONDITIONAL_H_
#define IBRCOMMON_CONDITIONAL_H_

#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include "ibrcommon/thread/Mutex.h"
#include "ibrcommon/Exceptions.h"

namespace ibrcommon
{
	class Conditional : public Mutex
	{
		public:
			class ConditionalAbortException : public ibrcommon::Exception
			{
			public:
				enum abort_t
				{
					COND_TIMEOUT = 0,
					COND_ABORT = 1,
					COND_ERROR = 2
				};

				ConditionalAbortException(abort_t abort, std::string what = "Conditional has been unblocked.") throw() : ibrcommon::Exception(what), reason(abort)
				{
				};

				const abort_t reason;
			};

			Conditional();
			virtual ~Conditional();

			void signal(bool broadcast = false) throw ();

			/*
			 * Wait until signal() is called or the timeout exceeds.
			 * @param timeout A timeout in milliseconds.
			 * @throw ConditionalAbortException If a timeout occur or the Conditional is aborted by abort() the ConditionalAbortException is thrown.
			 */
			void wait(size_t timeout = 0) throw (ConditionalAbortException);
			void wait(struct timespec *ts) throw (ConditionalAbortException);

			/**
			 * Abort all waits on this conditional.
			 */
			void abort() throw ();

			/**
			 * Removes the abort call off this conditional.
			 */
			void reset() throw ();

			/**
			 * Convert a millisecond timeout into use for high resolution
			 * conditional timers.
			 * @param timeout to convert.
			 * @param hires timespec representation to fill.
			 */
			static void gettimeout(size_t timeout, struct timespec *hires) throw ();

		private:
			bool isLocked();

			class attribute
			{
			public:
				pthread_condattr_t attr;
				attribute();
			};

			pthread_cond_t cond;

			static attribute attr;

			bool _abort;
	};

	template<class T, T block>
	class StatefulConditional : public Conditional
	{
	public:
		StatefulConditional(T state) : _state(state) {};
		virtual ~StatefulConditional() {};

		void setState(T state)
		{
			if (ifState(block)) return;
			_state = state;
			this->signal(true);
		}

		bool waitState(T state)
		{
			while (!ifState(state))
			{
				if (ifState(block)) return false;
				wait();
			}

			return true;
		}

		T getState()
		{
			return _state;
		}

		bool ifState(T state)
		{
			return (state == _state);
		}

	private:
		T _state;
	};
}

#endif /*CONDITIONAL_H_*/
