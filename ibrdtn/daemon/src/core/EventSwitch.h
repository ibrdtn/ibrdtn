/*
 * EventSwitch.h
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

#ifndef EVENTSWITCH_H_
#define EVENTSWITCH_H_

#include "Component.h"
#include "core/Event.h"
#include <ibrcommon/Exceptions.h>
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/Conditional.h>
#include <ibrcommon/TimeMeasurement.h>

#include <list>
#include <queue>
#include <map>

namespace dtn
{
	namespace core
	{
		class EventSwitch : public dtn::daemon::IntegratedComponent
		{
		private:
			EventSwitch();
			virtual ~EventSwitch();

			bool _running;
			bool _shutdown;

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			class Task
			{
			public:
				Task(EventProcessor &proc, dtn::core::Event *evt);
				~Task();

				EventProcessor &processor;
				dtn::core::Event *event;
			};

			class Worker : public ibrcommon::JoinableThread
			{
			public:
				Worker(EventSwitch &sw, bool profiling);
				~Worker();

				bool isStalled();

			protected:
				void run() noexcept;
				virtual void __cancellation() noexcept;

			private:
				EventSwitch &_switch;
				bool _running;
				ibrcommon::TimeMeasurement _tm;
				bool _inprogress;
				bool _profiling;
			};

			class WatchDog : public ibrcommon::JoinableThread
			{
			public:
				WatchDog(EventSwitch &sw, std::list<Worker*> &workers);
				~WatchDog();

				void up();
				void down();

			protected:
				void run() noexcept;
				virtual void __cancellation() noexcept;

			private:
				EventSwitch &_switch;
				std::list<Worker*> &_workers;

				bool _running;
				ibrcommon::Conditional _cond;
			};

			ibrcommon::Conditional _queue_cond;
			std::queue<Task*> _queue;
			std::queue<Task*> _prio_queue;
			std::queue<Task*> _low_queue;

			WatchDog _wd;
			std::list<Worker*> _wlist;

			ibrcommon::TimeMeasurement _tm;
			bool _inprogress;

			void process(ibrcommon::TimeMeasurement &tm, bool &inprogress, bool profiling);

		protected:
			virtual void componentUp() noexcept;
			virtual void componentDown() noexcept;

			bool isStalled();

		public:
			static EventSwitch& getInstance();
			void loop(size_t threads, bool profiling = false);

			/**
			 * checks if all the queues are empty
			 */
			bool empty() const;

			/**
			 * Bring down the EventSwitch.
			 * Do not accept any further events, but deliver all events in the queue.
			 */
			void shutdown();

			/**
			 * Queue an event to one of the event queues of the
			 * event switch.
			 */
			static void queue(EventProcessor &proc, Event *evt);

			friend class Worker;
		};
	}
}

#endif /* EVENTSWITCH_H_ */
