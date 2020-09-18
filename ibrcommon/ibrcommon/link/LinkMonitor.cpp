/*
 * LinkMonitor.cpp
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: David Goltzsche <goltzsch@ibr.cs.tu-bs.de>
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
 *  Created on: Sep 9, 2013
 */

#include "ibrcommon/link/LinkMonitor.h"
#include "ibrcommon/link/LinkEvent.h"
#include "ibrcommon/Logger.h"

#include <algorithm>

namespace ibrcommon
{
	const std::string LinkMonitor::TAG = "LinkMonitor";

	LinkMonitor::LinkMonitor(LinkManager &lm)
	 : _lmgr(lm), _running(true)
	{
	}

	LinkMonitor::~LinkMonitor()
	{
		// wait until the run-loop is finished
		join();
	}

	void LinkMonitor::add(const ibrcommon::vinterface &iface) noexcept
	{
		// check if there exists an address set for this interface
		if (_addr_map.find(iface) != _addr_map.end()) return;

		// get all addresses on this interface
		std::list<vaddress> addr = _lmgr.getAddressList(iface);

		// add all addresses of the interface to the known addresses list
		_addr_map[iface].insert(addr.begin(), addr.end());
	}

	void LinkMonitor::remove() noexcept
	{
		// get all monitored interfaces
		const std::set<vinterface> ifaces = _lmgr.getMonitoredInterfaces();

		// remove no longer monitored interfaces
		for (iface_map::iterator it = _addr_map.begin(); it != _addr_map.end();) {
			const ibrcommon::vinterface &iface = (*it).first;

			if (ifaces.find(iface) == ifaces.end()) {
				// remove the entry
				_addr_map.erase(it++);
			} else {
				++it;
			}
		}
	}

	void LinkMonitor::run() noexcept
	{
		ibrcommon::MutexLock l(_cond);

		while(_running)
		{
			// get all monitored interfaces
			const std::set<vinterface> ifaces = _lmgr.getMonitoredInterfaces();

			// check each interface for changed addresses
			for(std::set<vinterface>::const_iterator iface_iter = ifaces.begin(); iface_iter != ifaces.end(); iface_iter++)
			{
				const ibrcommon::vinterface &iface = (*iface_iter);

				// get current addresses from LinkManager
				std::list<vaddress> addr_new = _lmgr.getAddressList(iface);

				// sort all new addresses
				addr_new.sort();

				// check if there exists an address set for this interface
				if (_addr_map.find(iface) == _addr_map.end()) {
					// create a new address set
					_addr_map[iface] = addr_set();
				}

				// get the address set of last known addresses
				addr_set &addr_old = _addr_map[iface];

				// create a set for address differences
				std::list<vaddress> addr_diff;

				// find deleted addresses by difference between old and new addresses
				std::set_difference(addr_old.begin(), addr_old.end(), addr_new.begin(), addr_new.end(), std::back_inserter(addr_diff));

				// announce each removed address with a LinkEvent
				for(std::list<vaddress>::const_iterator addr_iter = addr_diff.begin(); addr_iter != addr_diff.end(); addr_iter++)
				{
					const ibrcommon::vaddress &addr = (*addr_iter);

					// remove deleted address from map
					addr_old.erase(addr);

					// debug
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 60) << "address " << addr.address() << " removed from interface " << iface.toString() <<  IBRCOMMON_LOGGER_ENDL;

					// raise event
					LinkEvent lme(LinkEvent::ACTION_ADDRESS_REMOVED, iface, addr);
					_lmgr.raiseEvent(lme);
				}

				// clear the result set
				addr_diff.clear();

				// find added addresses by difference between new and old addresses
				std::set_difference(addr_new.begin(), addr_new.end(), addr_old.begin(), addr_old.end(), std::back_inserter(addr_diff));

				// announce each added address with a LinkEvent
				for(std::list<vaddress>::const_iterator addr_iter = addr_diff.begin(); addr_iter != addr_diff.end(); addr_iter++)
				{
					const ibrcommon::vaddress &addr = (*addr_iter);

					// add new address to map
					addr_old.insert(addr);

					// debug
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 60) << "address " << addr.address() << " added to interface " << iface.toString() <<  IBRCOMMON_LOGGER_ENDL;

					// raise event
					LinkEvent lme(LinkEvent::ACTION_ADDRESS_ADDED, iface, addr);
					_lmgr.raiseEvent(lme);
				}
			}

			// take breath
			yield();

			try {
				// wait until the next check
				_cond.wait(_lmgr.getLinkRequestInterval());
			} catch (const Conditional::ConditionalAbortException &e){
				// reached, if conditional timed out
				if (e.reason == Conditional::ConditionalAbortException::COND_TIMEOUT) continue;

				// exit on abort
				return;
			}
		}
	}

	void LinkMonitor::__cancellation() noexcept
	{
		// debug
		IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 60) << "cancelled" << IBRCOMMON_LOGGER_ENDL;

		// shutdown the run-loop
		ibrcommon::MutexLock l(_cond);
		_running = false;
		_cond.signal(true);
	}
}

