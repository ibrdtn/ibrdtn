/*
 * DiscoveryBeacon.cpp
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

#include "config.h"
#include "net/DiscoveryBeacon.h"
#include <ibrdtn/data/Exceptions.h>
#include <ibrdtn/data/Number.h>
#include <ibrcommon/Logger.h>
#include <typeinfo>
#include <iostream>
#include <vector>

#ifdef __WIN32__
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

namespace dtn
{
	namespace net
	{
		DiscoveryBeacon::DiscoveryBeacon(const Protocol version, const dtn::data::EID &eid)
		 : _version(version), _flags(BEACON_NO_FLAGS), _canonical_eid(eid), _sn(0), _period(1)
		{
		}

		DiscoveryBeacon::~DiscoveryBeacon()
		{
		}

		bool DiscoveryBeacon::isShort() const
		{
			switch (_version)
			{
			case DISCO_VERSION_00:
				return (_flags & DiscoveryBeacon::BEACON_SHORT);

			case DISCO_VERSION_01:
				return !(_flags & DiscoveryBeacon::BEACON_CONTAINS_EID);
			};

			return false;
		}

		void DiscoveryBeacon::setEID(const dtn::data::EID &eid)
		{
			_canonical_eid = eid;
		}

		const dtn::data::EID& DiscoveryBeacon::getEID() const
		{
			return _canonical_eid;
		}

		DiscoveryBeacon::service_list& DiscoveryBeacon::getServices()
		{
			return _services;
		}

		const DiscoveryBeacon::service_list& DiscoveryBeacon::getServices() const
		{
			return _services;
		}

		void DiscoveryBeacon::clearServices()
		{
			_services.clear();
		}

		const DiscoveryService& DiscoveryBeacon::getService(const std::string &name) const
		{
			for (service_list::const_iterator iter = _services.begin(); iter != _services.end(); ++iter)
			{
				if ((*iter).getName() == name)
				{
					return (*iter);
				}
			}

			throw dtn::MissingObjectException("No service found with tag " + name);
		}

		DiscoveryService& DiscoveryBeacon::getService(const std::string &name)
		{
			for (service_list::iterator iter = _services.begin(); iter != _services.end(); ++iter)
			{
				if ((*iter).getName() == name)
				{
					return (*iter);
				}
			}

			throw dtn::MissingObjectException("No service found with tag " + name);
		}

		void DiscoveryBeacon::addService(const DiscoveryService &service)
		{
			_services.push_back(service);
		}

		void DiscoveryBeacon::setSequencenumber(uint16_t sequence)
		{
			_sn = sequence;
		}

		void DiscoveryBeacon::setPeriod(const dtn::data::Number &period)
		{
			_period = period;
		}

		const dtn::data::Number& DiscoveryBeacon::getPeriod() const
		{
			return _period;
		}

		bool DiscoveryBeacon::hasPeriod() const
		{
			return _period > 1;
		}

		std::ostream &operator<<(std::ostream &stream, const DiscoveryBeacon &announcement)
		{
			const dtn::net::DiscoveryBeacon::service_list &services = announcement._services;

			switch (announcement._version)
			{
				case DiscoveryBeacon::DISCO_VERSION_00:
				{
					if (services.empty())
					{
						const unsigned char flags = DiscoveryBeacon::BEACON_SHORT | announcement._flags;
						stream << (unsigned char)DiscoveryBeacon::DISCO_VERSION_00 << flags;
						return stream;
					}

					const dtn::data::BundleString eid(announcement._canonical_eid.getString());
					dtn::data::Number beacon_len;

					// determine the beacon length
					beacon_len += eid.getLength();

					// add service block length
					for (dtn::net::DiscoveryBeacon::service_list::const_iterator iter = services.begin(); iter != services.end(); ++iter)
					{
						beacon_len += (*iter).getLength();
					}

					stream << (unsigned char)DiscoveryBeacon::DISCO_VERSION_00 << announcement._flags << beacon_len << eid;

					for (dtn::net::DiscoveryBeacon::service_list::const_iterator iter = services.begin(); iter != services.end(); ++iter)
					{
						stream << (*iter);
					}

					break;
				}

				case DiscoveryBeacon::DISCO_VERSION_01:
				{
					unsigned char flags = 0;

					stream << (unsigned char)DiscoveryBeacon::DISCO_VERSION_01;

					if (announcement._canonical_eid != dtn::data::EID())
					{
						flags |= DiscoveryBeacon::BEACON_CONTAINS_EID;
					}

					if (!services.empty())
					{
						flags |= DiscoveryBeacon::BEACON_SERVICE_BLOCK;
					}

					if ( announcement._period > 1 )
					{
						flags |= DiscoveryBeacon::BEACON_CONTAINS_PERIOD;
					}

					stream << flags;

					// sequencenumber
					const uint16_t sn = htons(announcement._sn);
					stream.write( (const char*)&sn, 2 );

					if ( flags & DiscoveryBeacon::BEACON_CONTAINS_EID )
					{
						const dtn::data::BundleString eid(announcement._canonical_eid.getString());
						stream << eid;
					}

					if ( flags & DiscoveryBeacon::BEACON_SERVICE_BLOCK )
					{
						stream << dtn::data::Number(services.size());

						for (dtn::net::DiscoveryBeacon::service_list::const_iterator iter = services.begin(); iter != services.end(); ++iter)
						{
							stream << (*iter);
						}
					}

					// not standard conform in version 01!
					if ( flags & DiscoveryBeacon::BEACON_CONTAINS_PERIOD )
					{
						// append beacon period
						stream << announcement._period;
					}

					break;
				}

				case DiscoveryBeacon::DTND_IPDISCOVERY:
				{
					uint8_t cl_type = 1;
					char zero = '\0';
					uint8_t interval = 10;
					// uint32_t inet_addr;
					uint16_t inet_port = htons(4556);
					std::string eid = announcement._canonical_eid.getString();
					uint16_t eid_len = htons((uint16_t)eid.length());
					unsigned int add_zeros = (4 - (eid.length() % 4)) % 4;
					uint16_t length = htons(static_cast<uint16_t>(12 + eid.length() + add_zeros));


					stream << (unsigned char)cl_type;
					stream.write((char*)&interval, 1);
					stream.write((char*)&length, 2);

//					std::list<dtn::daemon::Configuration::NetConfig> interfaces = dtn::daemon::Configuration::getInstance().getInterfaces();
//					dtn::daemon::Configuration::NetConfig &i = interfaces.front();

//					struct sockaddr_in sock_address;
//
//					// set the local interface address
//					i.interface.getAddress(&sock_address.sin_addr);
//
//					stream.write((char*)&sock_address.sin_addr, 4);
					stream.write(&zero, 1);
					stream.write(&zero, 1);
					stream.write(&zero, 1);
					stream.write(&zero, 1);

					stream.write((char*)&inet_port, 2);
					stream.write((char*)&eid_len, 2);
					stream << eid;

					for (unsigned int i = 0; i < add_zeros; ++i)
					{
						stream.write((char*)&zero, 1);
					}

					break;
				}
			}

			return stream;
		}

		std::istream &operator>>(std::istream &stream, DiscoveryBeacon &announcement)
		{
			unsigned char version = 0;

			// do we running DTN2 compatibility mode?
			if (announcement._version == DiscoveryBeacon::DTND_IPDISCOVERY)
			{
				// set version to IPDiscovery (DTN2)
				version = DiscoveryBeacon::DTND_IPDISCOVERY;
			}
			else
			{
				// read IPND version of the frame
				version = (unsigned char)stream.get();
			}

			switch (version)
			{
			case DiscoveryBeacon::DISCO_VERSION_00:
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("DiscoveryBeacon", 60) << "beacon version 1 received" << IBRCOMMON_LOGGER_ENDL;

				dtn::data::Number beacon_len;
				dtn::data::Number eid_len;

				stream.get((char&)announcement._flags);

				// catch a short beacon
				if (DiscoveryBeacon::BEACON_SHORT == announcement._flags)
				{
					announcement._canonical_eid = dtn::data::EID();
					return stream;
				}

				stream >> beacon_len;
				int remain = beacon_len.get<int>();

				dtn::data::BundleString eid;
				stream >> eid;
				remain -= static_cast<int>(eid.getLength());

				announcement._canonical_eid = dtn::data::EID((const std::string&)eid);

				// get the services
				dtn::net::DiscoveryBeacon::service_list &services = announcement._services;

				// clear the services
				services.clear();

				while (remain > 0)
				{
					// decode the service blocks
					DiscoveryService service;
					stream >> service;
					services.push_back(service);
					remain -= static_cast<int>(service.getLength());
				}
				break;
			}

			case DiscoveryBeacon::DISCO_VERSION_01:
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("DiscoveryBeacon", 60) << "beacon version 2 received" << IBRCOMMON_LOGGER_ENDL;

				stream.get((char&)announcement._flags);

				IBRCOMMON_LOGGER_DEBUG_TAG("DiscoveryBeacon", 85) << "beacon flags: " << std::hex << (int)announcement._flags << IBRCOMMON_LOGGER_ENDL;

				uint16_t sn = 0;
				stream.read((char*)&sn, 2);

				// convert from network byte order
				uint16_t sequencenumber = ntohs(sn);

				IBRCOMMON_LOGGER_DEBUG_TAG("DiscoveryBeacon", 85) << "beacon sequence number: " << sequencenumber << IBRCOMMON_LOGGER_ENDL;

				if (announcement._flags & DiscoveryBeacon::BEACON_CONTAINS_EID)
				{
					dtn::data::BundleString eid;
					stream >> eid;

					announcement._canonical_eid = dtn::data::EID((const std::string&)eid);

					IBRCOMMON_LOGGER_DEBUG_TAG("DiscoveryBeacon", 85) << "beacon eid: " << (std::string)eid << IBRCOMMON_LOGGER_ENDL;
				}

				if (announcement._flags & DiscoveryBeacon::BEACON_SERVICE_BLOCK)
				{
					// get the services
					dtn::net::DiscoveryBeacon::service_list &services = announcement._services;

					// read the number of services
					dtn::data::Number num_services;
					stream >> num_services;

					IBRCOMMON_LOGGER_DEBUG_TAG("DiscoveryBeacon", 85) << "beacon services (" << num_services.toString() << "): " << IBRCOMMON_LOGGER_ENDL;

					// clear the services
					services.clear();

					for (unsigned int i = 0; num_services > i; ++i)
					{
						// decode the service blocks
						DiscoveryService service;
						stream >> service;
						services.push_back(service);

						IBRCOMMON_LOGGER_DEBUG_TAG("DiscoveryBeacon", 85) << "\t " << service.getName() << " [" << service.getParameters() << "]" << IBRCOMMON_LOGGER_ENDL;
					}
				}

				if (announcement._flags & DiscoveryBeacon::BEACON_BLOOMFILTER)
				{
					// read the bloom-filter
					stream >> announcement._bloomfilter;
				}

				// not standard conform in version 01!
				if (announcement._flags & DiscoveryBeacon::BEACON_CONTAINS_PERIOD)
				{
					// read appended beacon period
					stream >> announcement._period;
				}

				break;
			}

			case DiscoveryBeacon::DTND_IPDISCOVERY:
			{
				uint8_t cl_type;
				uint8_t interval;
				uint16_t length;
				uint32_t inet_addr;
				uint16_t inet_port;
				uint16_t eid_len;

				IBRCOMMON_LOGGER_DEBUG_TAG("DiscoveryBeacon", 60) << "beacon IPDiscovery (DTN2) frame received" << IBRCOMMON_LOGGER_ENDL;

				stream.read((char*)&cl_type, 1);
				stream.read((char*)&interval, 1);
				stream.read((char*)&length, 2);
				stream.read((char*)&inet_addr, 4);
				stream.read((char*)&inet_port, 2);
				stream.read((char*)&eid_len, 2);

				std::vector<char> eid(eid_len);
				stream.read(&eid[0], eid.size());

				announcement._version = DiscoveryBeacon::DTND_IPDISCOVERY;
				announcement._canonical_eid = dtn::data::EID(std::string(eid.begin(), eid.end()));

				break;
			}

			default:
				IBRCOMMON_LOGGER_DEBUG_TAG("DiscoveryBeacon", 60) << "unknown beacon received" << IBRCOMMON_LOGGER_ENDL;

				// Error, throw Exception!
				throw InvalidProtocolException("The received data does not match the discovery protocol.");

				break;
			}

			return stream;
		}

		std::string DiscoveryBeacon::toString() const
		{
			std::stringstream ss;
			ss << "ANNOUNCE: " << _canonical_eid.getString();
			return ss.str();
		}
	}
}
