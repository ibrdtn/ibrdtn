/*
 * Timer.h
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

#ifndef IBRCOMMON_TIMER_H_
#define IBRCOMMON_TIMER_H_

#include "ibrcommon/thread/Thread.h"
#include "ibrcommon/thread/Conditional.h"
#include <map>
#include <set>

#include <string>
#include <iostream>

namespace ibrcommon
{
	class Timer;
	class TimerCallback
	{
	public:
		virtual ~TimerCallback() {
		}

		/**
		 * This method will be called if the timer timed out.
		 * @param timer The reference to the timer which timed out.
		 * @exception Timer::StopTimerException if thrown will abort the timer
		 * @return The new timeout value.
		 */
		virtual size_t timeout(Timer *timer) = 0;
	};

	class Timer : public JoinableThread
	{
	public:
		typedef size_t time_t;
		static time_t get_current_time();

		/**
		 * Constructor for this timer
		 * @param timeout Timeout in seconds
		 */
		Timer(TimerCallback &callback, size_t timeout = 0);

		virtual ~Timer();

		void set(size_t timeout);

		/**
		 * This method resets the timer.
		 */
		void reset();

		/**
		 * This method stops the timer.
		 * Use reset() to start it again.
		 */
		void pause();

		/**
		 * This method returns the current timeout
		 */
		size_t getTimeout() const;

	protected:
		void run() noexcept;
		void __cancellation() noexcept;

	private:
		enum TIMER_STATE
		{
			TIMER_UNSET = 0,
			TIMER_RUNNING = 1,
			TIMER_RESET = 2,
			TIMER_STOPPED = 3,
			TIMER_CANCELLED = 4
		};

		StatefulConditional<Timer::TIMER_STATE, TIMER_STOPPED> _state;
		TimerCallback &_callback;
		size_t _timeout;
	public:
		class StopTimerException : public ibrcommon::Exception
		{
		public:
			StopTimerException(const std::string& what = "") : Exception(what)
			{
			}
		};
	};
}

#endif /* TIMER_H_ */
