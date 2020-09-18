/*
 * SignalHandler.cpp
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

#include "ibrcommon/thread/SignalHandler.h"

namespace ibrcommon
{
	ibrcommon::Queue<int> SignalHandler::_signal_queue;

	void SignalHandler::_proxy_handler(int signal)
	{
		_signal_queue.push(signal);
	}

	SignalHandler::SignalHandler(void (*handler)(int))
	 : _handler(handler)
	{
	}

	SignalHandler::~SignalHandler()
	{
		stop();
		join();
	}

	void SignalHandler::initialize()
	{
		start();
	}

	void SignalHandler::handle(int signal)
	{
		// register signal at system
		::signal(signal, _proxy_handler);
	}

	void SignalHandler::run(void) noexcept
	{
		try {
			int signal = _signal_queue.poll();
			_handler(signal);
		} catch (const ibrcommon::QueueUnblockedException &e) {
			// unblocked
		}
	}

	void SignalHandler::__cancellation() noexcept
	{
		_signal_queue.abort();
	}
} /* namespace ibrcommon */
