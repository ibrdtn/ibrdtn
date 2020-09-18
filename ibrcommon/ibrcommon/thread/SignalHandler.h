/*
 * SignalHandler.h
 *
 * Copyright (C) 2014 IBR, TU Braunschweig
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

#include "ibrcommon/thread/Thread.h"
#include "ibrcommon/thread/Queue.h"
#include <csignal>

#ifndef SIGNALHANDLER_H_
#define SIGNALHANDLER_H_

namespace ibrcommon
{
	class SignalHandler : private ibrcommon::JoinableThread
	{
	public:
		SignalHandler(void (*handler)(int));
		virtual ~SignalHandler();

		/**
		 * initializes the signal handler process
		 */
		void initialize();

		/**
		 * register to a given process signal
		 */
		void handle(int signal);

		/**
		 * Abstract interface for thread context run method.
		 */
		virtual void run(void) noexcept;

	protected:
		/**
		 * This method is call when the thread is stopped.
		 */
		virtual void __cancellation() noexcept;

	private:
		static void _proxy_handler(int signal);

		void (*_handler)(int);
		static ibrcommon::Queue<int> _signal_queue;
	};
} /* namespace ibrcommon */

#endif /* SIGNALHANDLER_H_ */
