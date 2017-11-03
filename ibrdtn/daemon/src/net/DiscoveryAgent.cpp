/*
 * DiscoveryAgent.cpp
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

#include "Configuration.h"
#include "net/DiscoveryAgent.h"
#include "net/DiscoveryService.h"
#include "net/DiscoveryBeacon.h"
#include "core/BundleCore.h"
#include "core/NodeEvent.h"
#include "core/EventDispatcher.h"
#include <ibrdtn/utils/Utils.h>
#include <ibrdtn/utils/Clock.h>
#include <ibrcommon/Logger.h>

using namespace dtn::core;

namespace dtn
{
	namespace net
	{
		DiscoveryAgent::DiscoveryAgent()
		 : _config(dtn::daemon::Configuration::getInstance().getDiscovery()),
		   _enabled(true), _sn(0), _adv_next(0), _beacon_period(_config.interval())
		{
		}

		DiscoveryAgent::~DiscoveryAgent()
		{
		}

		const std::string DiscoveryAgent::getName() const
		{
			return "DiscoveryAgent";
		}

		void DiscoveryAgent::raiseEvent(const dtn::core::TimeEvent&) throw ()
		{
			const dtn::data::Timestamp ts = dtn::utils::Clock::getMonotonicTimestamp();

			if (_config.announce() && (_adv_next <= ts)) {
				// advertise me
				onAdvertise();

				// set next advertisement period
				_adv_next = ts + _beacon_period;
			}
		}

		void DiscoveryAgent::raiseEvent(const dtn::core::GlobalEvent &global) throw ()
		{
			if (global.getAction() == dtn::core::GlobalEvent::GLOBAL_START_DISCOVERY) {
				// start sending discovery beacons
				_enabled = true;
			}
			else if (global.getAction() == dtn::core::GlobalEvent::GLOBAL_STOP_DISCOVERY) {
				// suspend discovery beacons
				_enabled = false;
			}
			else if (global.getAction() == dtn::core::GlobalEvent::GLOBAL_LOW_ENERGY) {
				// suspend mode - pro-long beaconing interval
				_beacon_period = _config.interval() * 10;
			}
			else if (global.getAction() == dtn::core::GlobalEvent::GLOBAL_NORMAL) {
				// suspend mode stopped - reset beaconing interval
				_beacon_period = _config.interval();
				_adv_next = 0;
			}
		}

		void DiscoveryAgent::componentUp() throw ()
		{
			// listen to global events (discovery start/stop)
			dtn::core::EventDispatcher<dtn::core::GlobalEvent>::add(this);

			// listen to time events
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);
		}

		void DiscoveryAgent::componentDown() throw ()
		{
			// un-listen to global events (discovery start/stop)
			dtn::core::EventDispatcher<dtn::core::GlobalEvent>::remove(this);

			// un-listen to time events
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
		}

		void DiscoveryAgent::registerService(const ibrcommon::vinterface &iface, dtn::net::DiscoveryBeaconHandler *handler)
		{
			ibrcommon::MutexLock l(_provider_lock);
			handler_list &list = _providers[iface];
			list.push_back(handler);
		}

		void DiscoveryAgent::registerService(dtn::net::DiscoveryBeaconHandler *handler)
		{
			ibrcommon::MutexLock l(_provider_lock);
			_default_providers.push_back(handler);
		}

		void DiscoveryAgent::unregisterService(const dtn::net::DiscoveryBeaconHandler *handler)
		{
			ibrcommon::MutexLock l(_provider_lock);

			// walk though all interfaces
			for (handler_map::iterator it_p = _providers.begin(); it_p != _providers.end();)
			{
				handler_list &list = (*it_p).second;

				for (handler_list::iterator it = list.begin(); it != list.end(); ++it) {
					if ((*it) == handler) {
						list.erase(it);
						break;
					}
				}

				if (list.empty())
					_providers.erase(it_p++);
				else
					++it_p;
			}

			// remove from default providers
			for (handler_list::iterator it = _default_providers.begin(); it != _default_providers.end(); ++it) {
				if ((*it) == handler) {
					_default_providers.erase(it);
					break;
				}
			}
		}

		void DiscoveryAgent::unregisterService(const ibrcommon::vinterface &iface, const dtn::net::DiscoveryBeaconHandler *handler)
		{
			ibrcommon::MutexLock l(_provider_lock);
			if (_providers.find(iface) == _providers.end()) return;

			handler_list &list = _providers[iface];

			for (handler_list::iterator it = list.begin(); it != list.end(); ++it) {
				if ((*it) == handler) {
					list.erase(it);
					if (list.empty()) _providers.erase(iface);
					return;
				}
			}
		}

		DiscoveryBeacon DiscoveryAgent::obtainBeacon() const
		{
			DiscoveryBeacon::Protocol version;

			switch (_config.version())
			{
			case 2:
				version = DiscoveryBeacon::DISCO_VERSION_01;
				break;

			default:
				version = DiscoveryBeacon::DISCO_VERSION_00;
				break;

			case 0:
				IBRCOMMON_LOGGER_TAG("DiscoveryAgent", info) << "DTN2 compatibility mode" << IBRCOMMON_LOGGER_ENDL;
				version = DiscoveryBeacon::DTND_IPDISCOVERY;
				break;
			};

			DiscoveryBeacon beacon(version, dtn::core::BundleCore::local);

			// set beaconing period
			beacon.setPeriod(_beacon_period);

			return beacon;
		}

		void DiscoveryAgent::onBeaconReceived(const DiscoveryBeacon &beacon)
		{
			// ignore own beacons
			if (beacon.getEID() == dtn::core::BundleCore::local) return;

			// convert the announcement into NodeEvents
			Node n(beacon.getEID());

			// if beaconing period is defined by beacon, set time-out to twice the period
			const dtn::data::Number to_value = beacon.hasPeriod() ? beacon.getPeriod() * 2 : _config.interval() * 2;

			const std::list<DiscoveryService> &services = beacon.getServices();

			for (std::list<DiscoveryService>::const_iterator iter = services.begin(); iter != services.end(); ++iter)
			{
				const DiscoveryService &s = (*iter);

				// get protocol from tag
				const dtn::core::Node::Protocol p = s.getProtocol();

				if (p == dtn::core::Node::CONN_EMAIL)
				{
					// Set timeout
					dtn::data::Number to_value_mailcl = to_value;
					size_t configTime = dtn::daemon::Configuration::getInstance().getEMail().getNodeAvailableTime();
					if(configTime > 0)
						to_value_mailcl = configTime;

					n.add(Node::URI(Node::NODE_DISCOVERED, Node::CONN_EMAIL, s.getParameters(), to_value_mailcl, -80));
				}
				else if ((p == dtn::core::Node::CONN_UNSUPPORTED) || (p == dtn::core::Node::CONN_UNDEFINED))
				{
					n.add(Node::Attribute(Node::NODE_DISCOVERED, s.getName(), s.getParameters(), to_value));
				}
				else
				{
					n.add(Node::URI(Node::NODE_DISCOVERED, p, s.getParameters(), to_value));
				}
			}

			// announce NodeInfo to ConnectionManager
			dtn::core::BundleCore::getInstance().getConnectionManager().updateNeighbor(n);

			// if continuous announcements are disabled, then reply to this message
			if (!_config.announce() && _enabled)
			{
				// first check if another announcement was sent during the same seconds
				const dtn::data::Timestamp ts = dtn::utils::Clock::getMonotonicTimestamp();

				if (_adv_next <= ts)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG("DiscoveryAgent", 55) << "reply with discovery beacon" << IBRCOMMON_LOGGER_ENDL;

					// reply with an own announcement
					onAdvertise();

					// set next advertisement period
					_adv_next = ts + _beacon_period;
				}
			}
		}

		void DiscoveryAgent::onAdvertise()
		{
			// check if announcements are enabled
			if (!_enabled) return;

			IBRCOMMON_LOGGER_DEBUG_TAG("DiscoveryAgent", 55) << "advertise discovery beacon" << IBRCOMMON_LOGGER_ENDL;

			DiscoveryBeacon beacon = obtainBeacon();

			// set sequencenumber
			beacon.setSequencenumber(_sn);

			ibrcommon::MutexLock l(_provider_lock);

			for (handler_map::const_iterator it_p = _providers.begin(); it_p != _providers.end(); ++it_p)
			{
				const ibrcommon::vinterface &iface = (*it_p).first;
				const handler_list &plist = (*it_p).second;

				// clear all services
				beacon.clearServices();

				// collect services from providers
				if (!_config.shortbeacon())
				{
					for (handler_list::const_iterator iter = plist.begin(); iter != plist.end(); ++iter)
					{
						DiscoveryBeaconHandler &handler = (**iter);

						try {
							// update service information
							handler.onUpdateBeacon(iface, beacon);
						} catch (const dtn::net::DiscoveryBeaconHandler::NoServiceHereException&) {

						}
					}
				}

				// broadcast announcement
				for (handler_list::const_iterator iter = plist.begin(); iter != plist.end(); ++iter)
				{
					DiscoveryBeaconHandler &handler = (**iter);

					// broadcast beacon
					handler.onAdvertiseBeacon(iface, beacon);
				}
			}

			if (!_config.shortbeacon())
			{
				const ibrcommon::vinterface any;
				for (handler_list::const_iterator iter = _default_providers.begin(); iter != _default_providers.end(); ++iter)
				{
					DiscoveryBeaconHandler &handler = (**iter);

					try {
						// update service information
						handler.onUpdateBeacon(any, beacon);
					} catch (const dtn::net::DiscoveryBeaconHandler::NoServiceHereException&) {

					}
				}
			}

			// increment sequencenumber
			_sn++;
		}
	}
}
