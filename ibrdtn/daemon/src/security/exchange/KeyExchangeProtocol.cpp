/*
 * KeyExchangeProtocol.cpp
 *
 * Copyright (C) 2014 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *             Thomas Schrader <schrader.thomas@gmail.com>
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

#include "security/exchange/KeyExchangeProtocol.h"

#include <openssl/sha.h>
#include <iomanip>

namespace dtn
{
	namespace security
	{
		KeyExchangeManager::~KeyExchangeManager()
		{
		}

		KeyExchangeProtocol::KeyExchangeProtocol(KeyExchangeManager &m, int protocol_id)
		 : manager(m), _protocol_id(protocol_id)
		{
		}

		KeyExchangeProtocol::~KeyExchangeProtocol()
		{
		}

		void KeyExchangeProtocol::add(std::map<int, KeyExchangeProtocol*> &list)
		{
			list[getProtocol()] = this;
		}

		int KeyExchangeProtocol::getProtocol() const
		{
			return _protocol_id;
		}

		KeyExchangeSession* KeyExchangeProtocol::createSession(const dtn::data::EID &peer, unsigned int uniqueId)
		{
			return new KeyExchangeSession(getProtocol(), peer, uniqueId, NULL);
		}

		void KeyExchangeProtocol::sha256(std::ostream &stream, const std::string &data)
		{
		    unsigned char hash[SHA256_DIGEST_LENGTH];
		    SHA256((const unsigned char*) data.c_str(), data.size(), hash);

			for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
			{
				stream << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
			}
		}

		std::string KeyExchangeProtocol::toHex(const std::string &data)
		{
			std::stringstream stream;

			for (std::string::size_type i = 0; i < data.size(); ++i)
			{
				unsigned char c = data[i];
				stream << std::hex << std::setw(2) << std::setfill('0') << (int)c;
			}

			return stream.str();
		}
	}
}
