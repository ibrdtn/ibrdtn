/*
 * DiscoveryService.cpp
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

#include "net/DiscoveryService.h"
#include "ibrdtn/data/Number.h"
#include "ibrdtn/data/BundleString.h"
#include <string>

using namespace dtn::data;

namespace dtn
{
	namespace net
	{
		DiscoveryService::DiscoveryService()
		 : _service_protocol(dtn::core::Node::CONN_UNDEFINED), _param(0)
		{
		}

		/**
		 * @param param The DiscoveryService instance will not take ownership of the
		 *  pointer, you have to destroy it yourself.
		 */
		DiscoveryService::DiscoveryService(const dtn::core::Node::Protocol p, DiscoveryServiceParam * param)
		 : _service_protocol(p), _param(param->clone())
		{
		}

		DiscoveryService::DiscoveryService(const DiscoveryService& other)
		 : _service_protocol(other._service_protocol), _param(0)
		{
			if (other._param)
			{
				_param = other._param->clone();
			}
		}

		DiscoveryService& DiscoveryService::operator=(const DiscoveryService& other)
		{
			_service_protocol = other._service_protocol;

			DiscoveryServiceParam * p = 0;
			if (other._param)
			{
				p = other._param->clone();
			}
			delete _param;
			_param = p;

			return *this;
		}

		DiscoveryService::~DiscoveryService()
		{
			delete _param;
			_param = 0;
		}

		bool DiscoveryService::operator==(const DiscoveryService& o) const
		{
			return (_service_protocol == o._service_protocol) &&
				(_param == o._param ||
					(_param && o._param && (*_param) == (*(o._param)))
				);
		}

		dtn::data::Length DiscoveryService::getLength(Discovery::Protocol version) const throw (WrongVersionException)
		{
			dtn::data::Number length;
			switch (version)
			{
				case Discovery::DISCO_VERSION_02:
				{
					if (_param)
					{
						length = _param->getLength(Discovery::DISCO_VERSION_02);
						return 1               // tag
							+ length.getLength() // length
							+ length.get();      // value
					}
					else
					{
						return 0;
					}
				}
				case Discovery::DISCO_VERSION_01:
				case Discovery::DISCO_VERSION_00:
				{
					if (_param)
					{
						BundleString name(asTag(_service_protocol));
						return name.getLength() + _param->getLength(version);
					}
					else
					{
						return 0;
					}
				}
				default:
				{
					std::ostringstream err;
					err << std::hex << version;
					throw WrongVersionException(err.str());
				}
			}
		}

		dtn::core::Node::Protocol DiscoveryService::getProtocol() const
		{
			return _service_protocol;
		}

		/** @return a pointer to the parameter object. The pointer is owned by
		 * the DiscoveryService object. */
		DiscoveryServiceParam * DiscoveryService::getParam()
		{
			return _param;
		}

		/** @return a pointer to the parameter object. The pointer is owned by
		 * the DiscoveryService object. */
		const DiscoveryServiceParam * DiscoveryService::getParam() const
		{
			return _param;
		}

		/** @param param DiscoveryService will not take ownership of this pointer */
		void DiscoveryService::update(DiscoveryServiceParam * param)
		{
			delete _param;
			_param = param->clone();
		}

		dtn::core::Node::Protocol DiscoveryService::asProtocol(const std::string &tag)
		{
			if (tag == "undefined") {
				return dtn::core::Node::CONN_UNDEFINED;
			}
			else if (tag == "udpcl") {
				return dtn::core::Node::CONN_UDPIP;
			}
			else if (tag == "tcpcl") {
				return dtn::core::Node::CONN_TCPIP;
			}
			else if (tag == "lowpancl") {
				return dtn::core::Node::CONN_LOWPAN;
			}
			else if (tag == "bt") {
				return dtn::core::Node::CONN_BLUETOOTH;
			}
			else if (tag == "http") {
				return dtn::core::Node::CONN_HTTP;
			}
			else if (tag == "file") {
				return dtn::core::Node::CONN_FILE;
			}
			else if (tag == "dgram:udp") {
				return dtn::core::Node::CONN_DGRAM_UDP;
			}
			else if (tag == "dgram:eth") {
				return dtn::core::Node::CONN_DGRAM_ETHERNET;
			}
			else if (tag == "dgram:lowpan") {
				return dtn::core::Node::CONN_DGRAM_LOWPAN;
			}
			else if (tag == "p2p:wifi") {
				return dtn::core::Node::CONN_P2P_WIFI;
			}
			else if (tag == "p2p:bt") {
				return dtn::core::Node::CONN_P2P_BT;
			}
			else if (tag == "email") {
				return dtn::core::Node::CONN_EMAIL;
			}
			else if (tag == "dhtns") {
				return dtn::core::Node::CONN_DHT;
			}
			else if (tag == "dtntp") {
				return dtn::core::Node::CONN_DTNTP;
			}

			return dtn::core::Node::CONN_UNSUPPORTED;
		}

		std::string DiscoveryService::asTag(const dtn::core::Node::Protocol proto)
		{
			switch (proto)
			{
			case dtn::core::Node::CONN_UNSUPPORTED:
				return "unsupported";

			case dtn::core::Node::CONN_UNDEFINED:
				return "undefined";

			case dtn::core::Node::CONN_UDPIP:
				return "udpcl";

			case dtn::core::Node::CONN_TCPIP:
				return "tcpcl";

			case dtn::core::Node::CONN_LOWPAN:
				return "lowpancl";

			case dtn::core::Node::CONN_BLUETOOTH:
				return "bt";

			case dtn::core::Node::CONN_HTTP:
				return "http";

			case dtn::core::Node::CONN_FILE:
				return "file";

			case dtn::core::Node::CONN_DGRAM_UDP:
				return "dgram:udp";

			case dtn::core::Node::CONN_DGRAM_ETHERNET:
				return "dgram:eth";

			case dtn::core::Node::CONN_DGRAM_LOWPAN:
				return "dgram:lowpan";

			case dtn::core::Node::CONN_P2P_WIFI:
				return "p2p:wifi";

			case dtn::core::Node::CONN_P2P_BT:
				return "p2p:bt";

			case dtn::core::Node::CONN_EMAIL:
				return "email";

			case dtn::core::Node::CONN_DHT:
				return "dhtns";

			case dtn::core::Node::CONN_DTNTP:
				return "dtntp";
			}

			return "unknown";
		}

		/**
		 * Generate a byte representation for the specified protocol version.
		 * @throws a @c WrongVersionException if the specified protocol
		 *  version is not (yet) implemented, or an @c IllegalServiceException if
		 *  the contained parameters are invalid in the current context.
		 */
		std::string DiscoveryService::pack(Discovery::Protocol version) const throw (WrongVersionException, IllegalServiceException)
		{
			std::ostringstream ss;

			switch (version)
			{
				case Discovery::DISCO_VERSION_02:
				{
					if (!_param)
					{
						throw IllegalServiceException("cannot announce a service without "
								"parameters");
					}

					try
					{
						ss << static_cast<char>(_param->getIPNDServiceTag(_service_protocol)); // tag
						ss << dtn::data::Number(_param->getLength(version)); // length
						ss << _param->pack(version);  // value
					}
					catch (WrongVersionException& e)
					{
						std::stringstream err;
						err << "Unknown protocol " << _service_protocol;
						throw IllegalServiceException(err.str());
					}

					break;
				}

				case Discovery::DISCO_VERSION_01:
				case Discovery::DISCO_VERSION_00:
				{
					std::string param_bytes = _param->pack(version);
					if (param_bytes.size() < 1)
					{
						throw IllegalServiceException("empty parameter string");
					}
					ss << dtn::data::BundleString(DiscoveryService::asTag(_service_protocol));
					ss << param_bytes;

					break;
				}

				default:
				{
					std::ostringstream err;
					err << std::hex << version;
					throw WrongVersionException(err.str());
				}
			}

			return ss.str();
		}

		/**
		 * Generate a concrete instance from the bytes in an std::istream. You can
		 * call @c getLength() on the returned instance to determine how many
		 * bytes were read from the stream.
		 *
		 * @return a pointer to the unpacked instance
		 * @param version the discovery protocol version
		 * @param stream the input stream to read from
		 * @throw The following exceptions may be thrown:
		 *  \li @c WrongVersionException: the specified @c version is not (yet)
		 *      implemented
		 *  \li @c ParseException: There were errors parsing the @c stream. Call
		 *      ParseException::getBytesRead() on the thrown object to obtain the
		 *      number of bytes which were read from the stream.
		 *  \li @c IllegalServiceException: The parameter(s) read from the stream
		 *      are not valid in the current context.
		 */
		DiscoveryService& DiscoveryService::unpack(std::istream& stream, Discovery::Protocol version) throw (ParseException, IllegalServiceException, WrongVersionException)
		{
			size_t bytes_read = 0;
			DiscoveryServiceParam * param = 0;

			switch (version)
			{
				case Discovery::DISCO_VERSION_02:
				{
					uint8_t tag;
					stream.read((char *) &tag, 1);
					bytes_read += 1;
					if (!stream.good())
					{
						throw ParseException("could not read IPND draft 02 service tag",
								bytes_read);
					}

					dtn::data::Number length;
					stream >> length;
					bytes_read += length.getLength();
					if (!stream.good())
					{
						throw ParseException("could not read IPND draft 02 service length",
								bytes_read);
					}

					// set service protocol
					switch (tag)
					{
						case ipnd::CLA_TCP_v4::Tag:
						case ipnd::CLA_TCP_v6::Tag:
						{
							_service_protocol = dtn::core::Node::CONN_TCPIP;
							break;
						}
						case ipnd::CLA_UDP_v6::Tag:
						case ipnd::CLA_UDP_v4::Tag:
						{
							_service_protocol = dtn::core::Node::CONN_UDPIP;
							break;
						}
						case ipnd::CLA_DGRAM_ETHERNET::Tag:
						{
							_service_protocol = dtn::core::Node::CONN_DGRAM_ETHERNET;
							break;
						}
						case ipnd::CLA_DGRAM_UDP::Tag:
						{
							_service_protocol = dtn::core::Node::CONN_DGRAM_UDP;
							break;
						}
						case ipnd::CLA_DGRAM_LOWPAN::Tag:
						{
							_service_protocol = dtn::core::Node::CONN_DGRAM_LOWPAN;
							break;
						}
						case ipnd::CLA_LOWPAN::Tag:
						{
							_service_protocol = dtn::core::Node::CONN_LOWPAN;
							break;
						}
						case ipnd::CLA_EMAIL::Tag:
						{
							_service_protocol = dtn::core::Node::CONN_EMAIL;
							break;
						}
						case ipnd::CLA_DHT::Tag:
						{
							_service_protocol = dtn::core::Node::CONN_DHT;
							break;
						}
						case ipnd::CLA_DTNTP::Tag:
						{
							_service_protocol = dtn::core::Node::CONN_DTNTP;
							break;
						}
						default:
						{
							std::ostringstream err;
							err << "ignoring unknown IPND draft 02 service tag " << (int) tag;
							stream.seekg(length.get(), std::ios_base::cur);
							bytes_read += length.get();
							throw ParseException(err.str(), bytes_read);
						}
					}

					// set param
					try
					{
						switch (tag)
						{
							case ipnd::CLA_TCP_v4::Tag:
							case ipnd::CLA_UDP_v4::Tag:
							case ipnd::CLA_TCP_v6::Tag:
							case ipnd::CLA_UDP_v6::Tag:
							{
								if (length != 2 + ipnd::FIXED32::Length + ipnd::FIXED16::Length &&
								    length != 2 + 1 + 16 + ipnd::FIXED16::Length)
								{
									std::ostringstream err;
									err << "expected length " << ipnd::CLA_TCP_v4::Length
										<< " or " << ipnd::CLA_TCP_v6::Length << ", got " << length
										<< " for IPND draft 02 tag " << (int) tag;
									throw ParseException(err.str(), bytes_read);
								}

								param = IPServiceParam::unpack(stream, version, tag);
								break;
							}

							case ipnd::CLA_DGRAM_UDP::Tag:
							case ipnd::CLA_DGRAM_ETHERNET::Tag:
							case ipnd::CLA_DGRAM_LOWPAN::Tag:
							{
								param = DatagramServiceParam::unpack(stream, version);
								break;
							}

							case ipnd::CLA_LOWPAN::Tag:
							{
								param = LOWPANServiceParam::unpack(stream, version);
								break;
							}

							case ipnd::CLA_EMAIL::Tag:
							{
								param = EMailServiceParam::unpack(stream, version);
								break;
							}

							case ipnd::CLA_DHT::Tag:
							{
								param = DHTServiceParam::unpack(stream, version);
								break;
							}

							case ipnd::CLA_DTNTP::Tag:
							{
								param = DTNTPServiceParam::unpack(stream, version);
								break;
							}

							// default: has already thrown above, unless we forgot something
						} // end switch (tag)
					}
					catch (ibrcommon::Exception&)
					{
						throw;
					}

					delete _param;
					_param = param; // only here so we can catch exceptions
					bytes_read += _param->getLength(version);

					return *this;
				}

				case Discovery::DISCO_VERSION_01:
				case Discovery::DISCO_VERSION_00:
				{
					BundleString name;
					stream >> name;
					bytes_read += name.getLength();
					if (stream.fail())
					{
						std::ostringstream err;
						err << "could not read service name for version " << version;
						throw ParseException(err.str(), bytes_read);
					}

					_service_protocol = DiscoveryService::asProtocol((const std::string&) name);

					try
					{
						switch (_service_protocol)
						{
							case dtn::core::Node::CONN_TCPIP:
							case dtn::core::Node::CONN_UDPIP:
							case dtn::core::Node::CONN_LOWPAN:
							case dtn::core::Node::CONN_EMAIL:
							case dtn::core::Node::CONN_DTNTP:
							{
								BundleString params;
								stream >> params;
								bytes_read += params.getLength();
								if (stream.fail())
								{
									std::ostringstream err;
									err << "could not read service parameters for version "
										<< version;
									throw ParseException(err.str(), bytes_read);
								}

								param = DiscoveryServiceParam::fromKeyValueString((const std::string&) params);
								break;
							}

							case dtn::core::Node::CONN_DGRAM_ETHERNET:
							case dtn::core::Node::CONN_DGRAM_UDP:
							case dtn::core::Node::CONN_DGRAM_LOWPAN:
							{
								param = DatagramServiceParam::unpack(stream, version);
								bytes_read += param->getLength(version);
								break;
							}

							case dtn::core::Node::CONN_DHT:
							{
								param = DHTServiceParam::unpack(stream, version);
								bytes_read += param->getLength(version);
								break;
							}

							default:
							{
								std::ostringstream err;
								err << "ignoring unknown service protocol " <<
									(const std::string&) name << " for version " << version;
								throw ParseException(err.str(), bytes_read);
							}
						} // switch (_service_protocol)
					}
					catch (const ibrcommon::Exception& e)
					{
						throw;
					}

					// if no exception was thrown, param contains the new parameter
					delete _param;
					_param = param;
					return *this;
				} // case Discovery::DISCO_VERSION_00 | Discovery::DISCO_VERSION_01

				default:
				{
					std::ostringstream err;
					err << std::hex << version;
					throw WrongVersionException(err.str());
				}
			} // switch (version)
		}
	}
}
