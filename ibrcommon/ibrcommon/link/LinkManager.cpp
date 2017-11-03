/*
 * LinkManager.cpp
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
#include "ibrcommon/link/LinkManager.h"
#include "ibrcommon/link/CompatLinkManager.h"
#include "ibrcommon/link/LinkEvent.h"
#include "ibrcommon/thread/MutexLock.h"
#include "ibrcommon/Logger.h"
#include <list>
#include <string>
#include <typeinfo>
#include <unistd.h>

namespace ibrcommon
{
	// default value for LinkMonitor checks
	size_t LinkManager::_link_request_interval = 5000;

	LinkManager& LinkManager::getInstance()
	{
		static CompatLinkManager lm;
		return lm;
	}

	void LinkManager::initialize()
	{
		static bool initialized = false;
		if (!initialized) {
			getInstance().up();
			initialized = true;
		}
	}

	void LinkManager::addEventListener(const vinterface &iface, LinkManager::EventCallback *cb) throw ()
	{
		if (cb == NULL) return;
		ibrcommon::MutexLock l(_listener_mutex);

		callback_set &ss = _listener[iface];
		ss.insert(cb);
	}

	void LinkManager::removeEventListener(const vinterface &iface, LinkManager::EventCallback *cb) throw ()
	{
		if (cb == NULL) return;
		ibrcommon::MutexLock l(_listener_mutex);

		callback_set &ss = _listener[iface];

		ss.erase(cb);

		if (ss.empty())
		{
			_listener.erase(iface);
		}
	}

	void LinkManager::removeEventListener(LinkManager::EventCallback *cb) throw ()
	{
		if (cb == NULL) return;

		try {
			ibrcommon::MutexLock l(_listener_mutex);

			for (listener_map::iterator iter = _listener.begin(); iter != _listener.end(); ++iter)
			{
				callback_set &ss = iter->second;
				ss.erase(cb);
			}
		} catch (const ibrcommon::MutexException&) {
			// this happens if this method is called after destroying the object
			// and is normal at shutdown
		}
	}

	void LinkManager::raiseEvent(const LinkEvent &lme)
	{
		IBRCOMMON_LOGGER_DEBUG_TAG("LinkManager", 65) << "event raised " << lme.toString() << IBRCOMMON_LOGGER_ENDL;

		// wait some time until the event is reported to the subscribers
		// this avoids bind issues if an address is not really ready
		if (lme.getAction() == LinkEvent::ACTION_ADDRESS_ADDED) ibrcommon::Thread::sleep(1000);

		// get the corresponding interface
		const vinterface &iface = lme.getInterface();

		// search for event subscriptions
		ibrcommon::MutexLock l(_listener_mutex);
		callback_set &ss = _listener[iface];

		for (callback_set::iterator iter = ss.begin(); iter != ss.end(); ++iter)
		{
			try {
				(*iter)->eventNotify((LinkEvent&)lme);
			} catch (const std::exception&) { };
		}
	}

	void LinkManager::setLinkRequestInterval(size_t interval)
	{
		_link_request_interval = interval;
	}

	size_t LinkManager::getLinkRequestInterval()
	{
		return _link_request_interval;
	}

	std::set<vinterface> LinkManager::getMonitoredInterfaces()
	{
		ibrcommon::MutexLock l(_listener_mutex);

		std::set<vinterface> interfaces;

		for(listener_map::const_iterator it = _listener.begin(); it != _listener.end(); ++it)
		{
			interfaces.insert((*it).first);
		}

		return interfaces;
	}
}
