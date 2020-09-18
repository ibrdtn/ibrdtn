/*
 * EventSwitch.cpp
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

#include "core/EventSwitch.h"
#include "core/EventReceiver.h"
#include "core/EventDispatcher.h"

#include <ibrcommon/thread/MutexLock.h>
#include "core/GlobalEvent.h"
#include <ibrcommon/Logger.h>
#include <stdexcept>
#include <iostream>
#include <typeinfo>
#include <signal.h>

namespace dtn
{
	namespace core
	{
		EventSwitch::EventSwitch()
		 : _running(true), _shutdown(false), _wd(*this, _wlist), _inprogress(false)
		{
		}

		EventSwitch::~EventSwitch()
		{
			componentDown();
		}

		void EventSwitch::componentUp() noexcept
		{
			// routine checked for noexcept on 15.02.2013

			// clear all queues
			_queue = std::queue<Task*>();
			_prio_queue = std::queue<Task*>();
			_low_queue = std::queue<Task*>();

			// reset component state
			_running = true;
			_shutdown = false;

			// reset aborted conditional
			_queue_cond.reset();
		}

		void EventSwitch::componentDown() noexcept
		{
			try {
				ibrcommon::MutexLock l(_queue_cond);

				// stop receiving events
				_shutdown = true;

				// wait until all queues are empty
				while (!this->empty())
				{
					_queue_cond.wait();
				}

				_queue_cond.abort();
			} catch (const ibrcommon::Conditional::ConditionalAbortException&) {};
		}

		bool EventSwitch::empty() const
		{
			return (_low_queue.empty() && _queue.empty() && _prio_queue.empty());
		}

		void EventSwitch::process(ibrcommon::TimeMeasurement &tm, bool &inprogress, bool profiling)
		{
			EventSwitch::Task *t = NULL;

			// just look for an event to process
			{
				ibrcommon::MutexLock l(_queue_cond);
				if (!_running) return;

				while (this->empty() && !_shutdown)
				{
					_queue_cond.wait();
				}

				if (!_prio_queue.empty())
				{
					t = _prio_queue.front();
					_prio_queue.pop();
				}
				else if (!_queue.empty())
				{
					t = _queue.front();
					_queue.pop();
				}
				else if (!_low_queue.empty())
				{
					t = _low_queue.front();
					_low_queue.pop();
				}
				else if (_shutdown)
				{
					// if all queues are empty and shutdown is requested
					// set running mode to false
					_running = false;

					// abort the conditional to release all blocking threads
					_queue_cond.abort();

					return;
				}

				_queue_cond.signal(true);
			}

			if (t != NULL)
			{
				if (profiling) {
					inprogress = true;
					tm.start();
				}
				// execute the event
				t->processor.process(t->event);

				if (profiling) {
					tm.stop();
					inprogress = false;
				}

				// log the event
				if (t->event->isLoggable())
				{
					if (profiling) {
						IBRCOMMON_LOGGER_TAG(t->event->getName(), notice) << t->event->getMessage() << " (" << tm.getMilliseconds() << " ms)" << IBRCOMMON_LOGGER_ENDL;
					} else {
						IBRCOMMON_LOGGER_TAG(t->event->getName(), notice) << t->event->getMessage() << IBRCOMMON_LOGGER_ENDL;
					}
				}

				// delete the Task
				delete t;
			}
		}

		bool EventSwitch::isStalled()
		{
			if (!_inprogress) return false;
			_tm.stop();
			return (_tm.getMilliseconds() > 5000);
		}

		void EventSwitch::loop(size_t threads, bool profiling)
		{
			if (profiling) {
				IBRCOMMON_LOGGER_TAG("EventSwitch", warning) << "Profiling and stalled event detection enabled" << IBRCOMMON_LOGGER_ENDL;
			}

			for (size_t i = 0; i < threads; ++i)
			{
				Worker *w = new Worker(*this, profiling);
				w->start();
				_wlist.push_back(w);
			}

			// bring up watchdog
			if (profiling) _wd.up();

			try {
				while (_running)
				{
					process(_tm, _inprogress, profiling);
				}
			} catch (const ibrcommon::Conditional::ConditionalAbortException&) { };

			// shut down watchdog
			if (profiling) _wd.down();
			_wd.join();

			for (std::list<Worker*>::iterator iter = _wlist.begin(); iter != _wlist.end(); ++iter)
			{
				Worker *w = (*iter);
				w->stop();
				w->join();
				delete w;
			}

			_wlist.clear();
		}

		void EventSwitch::queue(EventProcessor &proc, Event *evt)
		{
			EventSwitch &s = EventSwitch::getInstance();

			ibrcommon::MutexLock l(s._queue_cond);

			// do not process any event if the system is going down
			if (s._shutdown)
			{
				delete evt;
				return;
			}

			EventSwitch::Task *t = new EventSwitch::Task(proc, evt);

			if (evt->prio > 0)
			{
				s._prio_queue.push(t);
			}
			else if (evt->prio < 0)
			{
				s._low_queue.push(t);
			}
			else
			{
				s._queue.push(t);
			}
			s._queue_cond.signal();
		}

		void EventSwitch::shutdown()
		{
			try {
				ibrcommon::MutexLock l(_queue_cond);

				// stop receiving events
				_shutdown = true;

				// signal all blocking thread to check _shutdown variable
				_queue_cond.signal(true);
			} catch (const ibrcommon::Conditional::ConditionalAbortException&) {};
		}

		EventSwitch& EventSwitch::getInstance()
		{
			static EventSwitch instance;
			return instance;
		}

		const std::string EventSwitch::getName() const
		{
			return "EventSwitch";
		}

		EventSwitch::Task::Task(EventProcessor &proc, dtn::core::Event *evt)
		 : processor(proc), event(evt)
		{
		}

		EventSwitch::Task::~Task()
		{
			if (event != NULL)
			{
				delete event;
			}
		}

		EventSwitch::Worker::Worker(EventSwitch &sw, bool profiling)
		 : _switch(sw), _running(true), _inprogress(false), _profiling(profiling)
		{}

		EventSwitch::Worker::~Worker()
		{}

		bool EventSwitch::Worker::isStalled()
		{
			if (!_inprogress) return false;
			_tm.stop();
			return (_tm.getMilliseconds() > 5000);
		}

		void EventSwitch::Worker::run() noexcept
		{
			try {
				while (_running)
					_switch.process(_tm, _inprogress, _profiling);
			} catch (const ibrcommon::Conditional::ConditionalAbortException&) { };
		}

		void EventSwitch::Worker::__cancellation() noexcept
		{
			_running = false;
		}

		EventSwitch::WatchDog::WatchDog(EventSwitch &sw, std::list<Worker*> &workers)
		 : _switch(sw), _workers(workers), _running(true)
		{}

		EventSwitch::WatchDog::~WatchDog()
		{}

		void EventSwitch::WatchDog::up()
		{
			// reset thread if necessary
			if (JoinableThread::isFinalized())
			{
				JoinableThread::reset();
				_running = true;
			}

			JoinableThread::start();
		}

		void EventSwitch::WatchDog::down()
		{
			JoinableThread::stop();
		}

		void EventSwitch::WatchDog::run() noexcept
		{
			try {
				ibrcommon::MutexLock l(_cond);
				while (_running) {
					try {
						// wait a period
						_cond.wait(10000);
					} catch (const ibrcommon::Conditional::ConditionalAbortException &ex) {
						if (ex.reason == ibrcommon::Conditional::ConditionalAbortException::COND_TIMEOUT) {
							if (_switch.isStalled()) {
								IBRCOMMON_LOGGER_TAG("EventSwitch", critical) << "stalled event detected" << IBRCOMMON_LOGGER_ENDL;
								raise (SIGABRT);
							}

							for (std::list<Worker*>::iterator iter = _workers.begin(); iter != _workers.end(); ++iter)
							{
								Worker *w = (*iter);

								if (w->isStalled()) {
									IBRCOMMON_LOGGER_TAG("EventSwitch", critical) << "stalled event detected" << IBRCOMMON_LOGGER_ENDL;
									raise (SIGABRT);
								}
							}

						} else throw;
					}
				}
			} catch (const ibrcommon::Conditional::ConditionalAbortException&) { };
		}

		void EventSwitch::WatchDog::__cancellation() noexcept
		{
			ibrcommon::MutexLock l(_cond);
			_running = false;
			_cond.abort();
		}
	}
}

