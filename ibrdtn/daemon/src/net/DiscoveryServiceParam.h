/**
 * DiscoveryServiceParam.h
 *
 * Copyright (C) 2014 IBR, TU Braunschweig
 *
 * Written-by: Roland Hieber <r.hieber@tu-bs.de>
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
#ifndef DISCOVERYSERVICEPARAM_H_
#define DISCOVERYSERVICEPARAM_H_

#include "net/Discovery.h"
#include "core/Node.h"
#include <ibrdtn/data/Number.h>
#include <iostream>
#include <string>
#include <arpa/inet.h>

namespace dtn
{
	namespace net
	{
		class DiscoveryServiceParam
		{
		public:
			virtual ~DiscoveryServiceParam() { }

			static DiscoveryServiceParam * fromKeyValueString(const std::string& parameters) throw (IllegalServiceException);

			/**
			 * Factory Method, Virtual Constructor Idiom.
			 * @return a copy of the current instance. The returned pointer is then
			 *  owned by the calling context.
			 */
			virtual DiscoveryServiceParam * clone() const = 0;
			virtual DiscoveryServiceParam& operator=(const DiscoveryServiceParam& o);
			virtual bool operator==(const DiscoveryServiceParam& o) const;
			virtual bool operator!=(const DiscoveryServiceParam& o) const { return !((*this) == o); }

			/**
			 * @return the number of bytes in the byte representation which would be
			 * returned by @c DiscoveryServiceParam::pack(version).
			 */
			virtual dtn::data::Length getLength(Discovery::Protocol version) const = 0;

			/** @return service tag for IPND Draft 02 TLV entities */
			virtual const uint8_t getIPNDServiceTag(dtn::core::Node::Protocol proto) const = 0;

			/**
			 * Generate a byte representation for the specified protocol version.
			 * @throws May throw a WrongVersionException if the specified protocol
			 *  version is not (yet) implemented, or an IllegalServiceException if the
			 *  contained parameters are invalid in the current context.
			 *
			 * For reference, a DiscoveryServiceParam contains the following:
			 * \li For @c Discovery::DISCO_VERSION_00, it contains the fields "Service
			 *     Param Len" and "Service Parameters" of a Service Block, as
			 *     described in IPND Draft 00, Section 2.3.1.
			 * \li For @c Discovery::DISCO_VERSION_01, it contains the fields "Service
			 *     Param Len" and "Service Parameters" of a service in the Service
			 *     Block, as described in IPND Draft 01, Section 2.6.1.
			 * \li For @c Discovery::DISCO_VERSION_02, it contains the TLV
			 *     representation of a service in the Service Block, as described in
			 *     IPND Draft 02, Section 2.6.3, Figure 5, excluding the leading Tag
			 *     and Length Fields.
			 *     (Example: An IPv4ServiceParam would contain an FIXED32 Tag byte,
			 *     followed by 4 octets containing an IPv4 address, followed by an
			 *     FIXED16 Tag byte, followed by 2 octets containing a port number,
			 *     with a total length of 8 bytes.)
			 */
			virtual std::string pack(Discovery::Protocol version) const throw (WrongVersionException, IllegalServiceException) = 0;
			/**
			 * Generate a concrete instance from the bytes in an std::istream. You can
			 * call @c getLength() on the returned instance to determine how many
			 * bytes were read from the stream.
			 *
			 * See the remark in DiscoveryServiceParam::pack() for a list of what a
			 * DiscoveryServiceParam subclass may contain.
			 *
			 * @return a pointer to a concrete instance. The returned pointer is owned
			 *  by the calling context. It is safe to assume that this function never
			 *  returns a null pointer.
			 * @param version the discovery protocol version
			 * @param stream the input stream to read from
			 * @throw The following exceptions may be thrown:
			 *  \li @c WrongVersionException: the specified @c version is not (yet)
			 *      implemented
			 *  \li @c ParseException: There were errors parsing the @c stream. Call
			 *      ParseException::getBytesRead() on the exception to obtain the
			 *      number of bytes which were read from the stream.
			 *  \li @c IllegalServiceException: The parameter(s) read from the stream
			 *      are not valid in the current context.
			 */
			template<class ParamType>
			static DiscoveryServiceParam * unpack(std::istream& stream, Discovery::Protocol version) throw (ParseException, WrongVersionException, IllegalServiceException)
			{
				return ParamType::unpack(stream, version);
			}
		};

/******************************************************************************/

		/** Service Parameter containing an IPv4/6 address and port number */
		struct IPServiceParam : public DiscoveryServiceParam
		{
			std::string _address;
			uint16_t _port;

			IPServiceParam();
			IPServiceParam(const std::string& address, const uint16_t port);
			virtual ~IPServiceParam() { }

			/** Build a IPND draft 00/01 parameter string */
			std::string buildParamString() const;

			/** Determine whether this contains an IPv4 address */
			inline bool isIPv4Address() const
			{
					return (_address.find(".") != std::string::npos &&
					        _address.size() <= INET_ADDRSTRLEN);
			}
			/** Determine whether this contains an IPv6 address */
			inline bool isIPv6Address() const
			{
				return (_address.find(":") != std::string::npos &&
			          _address.size() <= INET6_ADDRSTRLEN);
			}

			// virtual from DiscoveryServiceParam
			virtual DiscoveryServiceParam * clone() const;
			virtual DiscoveryServiceParam& operator=(const IPServiceParam& o);
			virtual bool operator==(const IPServiceParam& o) const;
			virtual const uint8_t getIPNDServiceTag(dtn::core::Node::Protocol proto) const throw (IllegalServiceException);
			virtual dtn::data::Length getLength(Discovery::Protocol version) const;
			virtual std::string pack(Discovery::Protocol version) const throw (WrongVersionException, IllegalServiceException);
			static DiscoveryServiceParam * unpack(std::istream& stream, Discovery::Protocol version, const uint8_t tag) throw (ParseException, IllegalServiceException, WrongVersionException);
		};

/******************************************************************************/

		/** Service Parameter with address and port for LoWPAN */
		struct LOWPANServiceParam : public DiscoveryServiceParam
		{
			uint16_t _address;
			uint16_t _port;

			LOWPANServiceParam();
			LOWPANServiceParam(const uint16_t addr, const uint16_t port);
			virtual ~LOWPANServiceParam() { }

			/** Build a IPND draft 00/01 parameter string */
			std::string buildParamString() const;

			// virtual from DiscoveryServiceParam
			virtual DiscoveryServiceParam * clone() const;
			virtual DiscoveryServiceParam& operator=(const LOWPANServiceParam& o);
			virtual bool operator==(const LOWPANServiceParam& o) const;
			virtual dtn::data::Length getLength(Discovery::Protocol version) const;
			virtual const uint8_t getIPNDServiceTag(dtn::core::Node::Protocol proto) const throw (IllegalServiceException);
			virtual std::string pack(Discovery::Protocol version) const throw (WrongVersionException);
			static DiscoveryServiceParam * unpack(std::istream& stream, Discovery::Protocol version) throw (ParseException, IllegalServiceException, WrongVersionException);
		};

/******************************************************************************/

		/** Service Parameter containing an address for Datagram Convergence Layer*/
		struct DatagramServiceParam : public DiscoveryServiceParam
		{
			std::string _address;

			DatagramServiceParam();
			DatagramServiceParam(const std::string& addr);
			virtual ~DatagramServiceParam() { }

			// virtual from DiscoveryServiceParam
			virtual DiscoveryServiceParam * clone() const;
			virtual DiscoveryServiceParam& operator=(const DatagramServiceParam& o);
			virtual bool operator==(const DatagramServiceParam& o) const;
			virtual dtn::data::Length getLength(Discovery::Protocol version) const;
			virtual const uint8_t getIPNDServiceTag(dtn::core::Node::Protocol proto) const throw (IllegalServiceException);
			virtual std::string pack(Discovery::Protocol version) const throw (WrongVersionException);
			static DiscoveryServiceParam * unpack(std::istream& stream, Discovery::Protocol version) throw (ParseException, IllegalServiceException, WrongVersionException);
		};

/******************************************************************************/

		/** Service Parameter containing a mail address */
		struct EMailServiceParam : public DiscoveryServiceParam
		{
			std::string _address;

			EMailServiceParam();
			EMailServiceParam(const std::string& addr);
			virtual ~EMailServiceParam() { }

			/** Build a IPND draft 00/01 parameter string */
			std::string buildParamString() const;

			// virtual from DiscoveryServiceParam
			virtual DiscoveryServiceParam * clone() const;
			virtual DiscoveryServiceParam& operator=(const EMailServiceParam& o);
			virtual bool operator==(const EMailServiceParam& o) const;
			virtual dtn::data::Length getLength(Discovery::Protocol version) const;
			virtual const uint8_t getIPNDServiceTag(dtn::core::Node::Protocol proto) const throw (IllegalServiceException);
			virtual std::string pack(Discovery::Protocol version) const throw (WrongVersionException);
			static DiscoveryServiceParam * unpack(std::istream& stream, Discovery::Protocol version) throw (ParseException, IllegalServiceException, WrongVersionException);
		};

/******************************************************************************/

		/** Service Parameter containing information about DHT nodes */
		struct DHTServiceParam : public DiscoveryServiceParam
		{
			uint16_t _port;
			bool _proxy;

			DHTServiceParam();
			DHTServiceParam(const uint16_t port, const bool proxy);
			virtual ~DHTServiceParam() { }

			/** Build a IPND draft 00/01 parameter string */
			std::string buildParamString() const;

			// virtual from DiscoveryServiceParam
			virtual DiscoveryServiceParam * clone() const;
			virtual DiscoveryServiceParam& operator=(const DHTServiceParam& o);
			virtual bool operator==(const DHTServiceParam& o) const;
			virtual dtn::data::Length getLength(Discovery::Protocol version) const;
			virtual const uint8_t getIPNDServiceTag(dtn::core::Node::Protocol proto) const throw (IllegalServiceException);
			virtual std::string pack(Discovery::Protocol version) const throw (WrongVersionException);
			static DiscoveryServiceParam * unpack(std::istream& stream, Discovery::Protocol version) throw (ParseException, IllegalServiceException, WrongVersionException);
		};

/******************************************************************************/

		/** Service Parameter containing time sync information */
		struct DTNTPServiceParam : public DiscoveryServiceParam
		{
			unsigned int _version;
			double _quality;
			dtn::data::Timestamp _timestamp;

			DTNTPServiceParam();
			DTNTPServiceParam(const unsigned int& version, const double& quality, const dtn::data::Timestamp& timestamp);
			virtual ~DTNTPServiceParam() { }

			/** Build a IPND draft 00/01 parameter string */
			std::string buildParamString() const;

			// virtual from DiscoveryServiceParam
			virtual DiscoveryServiceParam * clone() const;
			virtual DiscoveryServiceParam& operator=(const DTNTPServiceParam& o);
			virtual bool operator==(const DTNTPServiceParam& o) const;
			virtual dtn::data::Length getLength(Discovery::Protocol version) const;
			virtual const uint8_t getIPNDServiceTag(dtn::core::Node::Protocol proto) const throw (IllegalServiceException);
			virtual std::string pack(Discovery::Protocol version) const throw (WrongVersionException);
			static DiscoveryServiceParam * unpack(std::istream& stream, Discovery::Protocol version) throw (ParseException, IllegalServiceException, WrongVersionException);
		};

	} // namespace net
} // namespace dtn

#endif // DISCOVERYSERVICEPARAM_H_
