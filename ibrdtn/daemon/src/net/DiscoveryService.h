/*
 * DiscoveryService.h
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

#ifndef DISCOVERYSERVICE_H_
#define DISCOVERYSERVICE_H_

#include "core/Node.h"
#include "net/Discovery.h"
#include "net/DiscoveryServiceParam.h"
#include <ibrdtn/data/Number.h>
#include <iostream>

namespace dtn
{
	namespace net
	{
		class DiscoveryService
		{
		public:
			DiscoveryService();
			DiscoveryService(const dtn::core::Node::Protocol p, DiscoveryServiceParam * param);
			DiscoveryService(const DiscoveryService& other);
			virtual ~DiscoveryService();

			DiscoveryService& operator=(const DiscoveryService& other);
			bool operator==(const DiscoveryService& other) const;

			dtn::data::Length getLength(Discovery::Protocol version) const throw (WrongVersionException);

			dtn::core::Node::Protocol getProtocol() const;
			DiscoveryServiceParam * getParam();
			const DiscoveryServiceParam * getParam() const;

			/** get protocol name as string, or empty string if protocol not set */
			std::string getName() const
			{
				return asTag(_service_protocol);
			}
			/** get parameters as key-value string, or empty if parameters not set */
			std::string getParameters() const
			{
				return (_param ? _param->pack(Discovery::DISCO_VERSION_01) : "");
			}

			/** Update the parameters of this service */
			void update(DiscoveryServiceParam * param);

			/** Get byte representation */
			virtual std::string pack(Discovery::Protocol version) const throw (WrongVersionException, IllegalServiceException);
			/** Generate instance from byte representation */
			virtual DiscoveryService& unpack(std::istream& stream, Discovery::Protocol version) throw (ParseException, IllegalServiceException, WrongVersionException);

			/**
			 * convert a protocol identifier to a tag
			 */
			static std::string asTag(const dtn::core::Node::Protocol proto);
			static dtn::core::Node::Protocol asProtocol(const std::string &tag);

		protected:
			typedef dtn::core::Node::Protocol ServiceProtocol;

		private:
			ServiceProtocol _service_protocol;
			DiscoveryServiceParam * _param;
		};
	}
}

#endif /* DISCOVERYSERVICE_H_ */
