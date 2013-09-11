/*
 * LinkMonitor.cpp
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: David Goltzsche <goltzsch@ibr.cs.tu-bs.de>
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
 *  Created on: Sep 9, 2013
 */

#include "ibrcommon/link/LinkMonitor.h"
#include "ibrcommon/link/LinkEvent.h"
#include "ibrcommon/thread/Conditional.h"
#include "ibrcommon/Logger.h"
#include <stdio.h>


#include <algorithm>

namespace ibrcommon {

	ibrcommon::Conditional cond;
	LinkMonitor::LinkMonitor(LinkManager &lm) : _lmgr(lm), _running(true)
	{
	}

	LinkMonitor::~LinkMonitor()
	{
		join();
	}

	void LinkMonitor::run() throw()
	{
		ibrcommon::MutexLock l(cond);
		while(_running)
		{

			std::set<vinterface> ifaces = _lmgr.getMonitoredInterfaces();
			for(std::set<vinterface>::iterator iface_iter = ifaces.begin(); iface_iter != ifaces.end(); iface_iter++)
			{
				//get current addresses from LinkManager
				std::list<vaddress> new_addresses = _lmgr.getAddressList(*iface_iter);
				//get old addresses from map
				std::set<vaddress> old_addresses;
				if(_iface_adr_map.size() > 0)
					old_addresses = _iface_adr_map.at(*iface_iter);



				new_addresses.sort();
				std::list<vaddress> addresses;

				LinkEvent::Action action = LinkEvent::ACTION_UNKOWN;

				//find deleted addresses by difference between old and new addresses
				std::set_difference(old_addresses.begin(),old_addresses.end(),new_addresses.begin(),new_addresses.end(), std::back_inserter(addresses));
				for(std::list<vaddress>::iterator adr_iter = addresses.begin(); adr_iter != addresses.end(); adr_iter++)
				{
						//remove deleted address from map
						_iface_adr_map[*(iface_iter)].erase(*adr_iter);

						//raise event
						action = LinkEvent::ACTION_ADDRESS_REMOVED;
						IBRCOMMON_LOGGER_DEBUG_TAG("LinkMonitor", 5) << "address REMOVED:" << (*adr_iter).address() << " on interface " << (*iface_iter).toString() <<  IBRCOMMON_LOGGER_ENDL;
						LinkEvent lme(action, *(iface_iter), *(adr_iter));;
						_lmgr.raiseEvent(lme);
				}

				addresses.clear();

				//find added addresses by difference between new and old addresses
				std::set_difference(new_addresses.begin(),new_addresses.end(),old_addresses.begin(),old_addresses.end(), std::back_inserter(addresses));
				for(std::list<vaddress>::iterator adr_iter = addresses.begin(); adr_iter != addresses.end(); adr_iter++)
				{
						//add new address to map
						_iface_adr_map[*(iface_iter)].insert(*adr_iter);

						//raise event
						action = LinkEvent::ACTION_ADDRESS_ADDED;
						IBRCOMMON_LOGGER_DEBUG_TAG("LinkMonitor", 5) << "address ADDED:" << (*adr_iter).address() << " on interface " << (*iface_iter).toString() <<  IBRCOMMON_LOGGER_ENDL;
						LinkEvent lme(action, *(iface_iter), *(adr_iter));;
						_lmgr.raiseEvent(lme);
				}
			}

			try {
				cond.wait(_lmgr.getLinkRequestInterval());
			} catch (const Conditional::ConditionalAbortException &e){
				//reached, if conditional timed out
			}
		}
	}

	void LinkMonitor::__cancellation() throw()
	{
			IBRCOMMON_LOGGER_DEBUG_TAG("LinkRequester", 1) << "LinkRequester CANCELLED" << IBRCOMMON_LOGGER_ENDL;
			_running = false;
			ibrcommon::MutexLock l(cond);
			cond.signal(true);
	}
}

