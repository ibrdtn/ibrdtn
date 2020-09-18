/*
 * Timer.cpp
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
#include "ibrcommon/thread/Timer.h"
#include "ibrcommon/thread/MutexLock.h"

#include <iostream>
#include <queue>
#include <utility>
#include <sstream>

namespace ibrcommon
{
	Timer::time_t Timer::get_current_time()
	{
		::time_t rawtime = ::time(NULL);
		tm * ptm;

		ptm = ::gmtime ( &rawtime );

		return ::mktime(ptm);
	}

	Timer::Timer(TimerCallback &callback, size_t timeout)
	 : _state(TIMER_UNSET), _callback(callback), _timeout(timeout * 1000)
	{
	}

	Timer::~Timer()
	{
		stop();
		join();
	}

	void Timer::__cancellation() noexcept
	{
		MutexLock l(_state);
		_state.setState(TIMER_CANCELLED);
		_state.abort();
	}

	void Timer::set(size_t timeout)
	{
		MutexLock l(_state);
		_timeout = timeout * 1000;
		_state.setState(TIMER_RESET);
	}

	void Timer::reset()
	{
		MutexLock l(_state);
		_state.setState(TIMER_RESET);
	}

	void Timer::pause()
	{
		MutexLock l(_state);
		_state.setState(TIMER_STOPPED);
	}

	size_t Timer::getTimeout() const
	{
		return _timeout / 1000;
	}

	void Timer::run() noexcept
	{
		MutexLock l(_state);
		_state.setState(TIMER_RUNNING);

		while (_state.ifState(TIMER_RUNNING) || _state.ifState(TIMER_STOPPED))
		{
			try {
				if(_state.ifState(TIMER_RUNNING))
				{
					if(_timeout > 0)
					{
						_state.wait(_timeout);
					}
					else
					{
						throw ibrcommon::Conditional::ConditionalAbortException(ibrcommon::Conditional::ConditionalAbortException::COND_ABORT);
					}
				}
				else
				{
					_state.wait();
				}

				// got signal, check for reset request
				if (_state.ifState(TIMER_RESET))
				{
					_state.setState(TIMER_RUNNING);
				}
				else if(!_state.ifState(TIMER_STOPPED))
				{
					// stop the timer
					_state.setState(TIMER_CANCELLED);
				}
			}
			catch (const ibrcommon::Conditional::ConditionalAbortException &ex)
			{
				switch (ex.reason)
				{
					case ibrcommon::Conditional::ConditionalAbortException::COND_TIMEOUT:
					{
						try
						{
							// timeout exceeded, call callback method
							_timeout = _callback.timeout(this) * 1000;
							_state.setState(TIMER_RUNNING);
						}
						catch(const StopTimerException &ex)
						{
							// stop the timer
							_state.setState(TIMER_STOPPED);
						}
						break;
					}

					case ibrcommon::Conditional::ConditionalAbortException::COND_ABORT:
					case ibrcommon::Conditional::ConditionalAbortException::COND_ERROR:
					{
						_state.setState(TIMER_CANCELLED);
						return;
					}
				}
			}

			yield();
		}
	}
}
