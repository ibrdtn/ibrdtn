/*
 * LinkManager.h
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

#ifndef LINKMANAGER_H_
#define LINKMANAGER_H_

#include "ibrcommon/link/LinkEvent.h"
#include "ibrcommon/net/vinterface.h"
#include "ibrcommon/net/vaddress.h"
#include "ibrcommon/thread/Mutex.h"
#include <string>
#include <map>
#include <set>

namespace ibrcommon
{
	class LinkManager
	{
	public:
		class EventCallback
		{
		public:
			virtual ~EventCallback() {};
			virtual void eventNotify(const LinkEvent&) = 0;
		};

		virtual ~LinkManager() {};

		virtual void up() throw () { };
		virtual void down() throw () { };

		virtual const ibrcommon::vinterface getInterface(int index) const = 0;
		virtual const std::list<vaddress> getAddressList(const vinterface &iface, const std::string &scope = "") = 0;

		virtual void addEventListener(const vinterface&, LinkManager::EventCallback*) throw ();
		virtual void removeEventListener(const vinterface&, LinkManager::EventCallback*) throw ();
		virtual void removeEventListener(LinkManager::EventCallback*) throw ();

		void raiseEvent(const LinkEvent &lme);

		static LinkManager& getInstance();
		static void initialize();
		static void setLinkRequestInterval(size_t interval);
		static size_t getLinkRequestInterval();
		/*
		 * returns a set of interfaces, containing all interfaces with an eventListener
		 */
		std::set<vinterface> getMonitoredInterfaces();

	protected:
		ibrcommon::Mutex _listener_mutex;
		std::map<ibrcommon::vinterface, std::set<LinkManager::EventCallback* > > _listener;

		static size_t _link_request_interval;
	};
}

#endif /* LINKMANAGER_H_ */
