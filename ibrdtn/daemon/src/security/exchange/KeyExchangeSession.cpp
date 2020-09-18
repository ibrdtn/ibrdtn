/*
 * KeyExchangeSession.cpp
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

#include "security/exchange/KeyExchangeSession.h"
#include "security/SecurityKeyManager.h"
#include <ibrdtn/utils/Clock.h>
#include <ibrcommon/Logger.h>

namespace dtn
{
	namespace security
	{
		const std::string KeyExchangeSession::TAG = "KeyExchangeSession";

		KeyExchangeSession::SessionState::~SessionState()
		{
		}

		KeyExchangeSession::KeyExchangeSession(int protocol, const dtn::data::EID &peer, unsigned int uniqueId, SessionState *state)
		: _protocol(protocol), _unique_id(uniqueId), _peer(peer.getNode()), _state(state), _expiration(0)
		{
			// generate session key
			_session_key = getSessionKey(_peer, _unique_id);

			// set a expiration time (10 minutes)
			_expiration = dtn::utils::Clock::getMonotonicTimestamp() + 600;
		}

		KeyExchangeSession::~KeyExchangeSession()
		{
			// clear session state
			if (_state != NULL) delete _state;

			// delete all session files
			clearKeys();
		}

		dtn::data::Timestamp KeyExchangeSession::getExpiration() const
		{
			return _expiration;
		}

		void KeyExchangeSession::touch()
		{
			// set new expiration time
			_expiration = dtn::utils::Clock::getMonotonicTimestamp() + 600;
		}

		std::string KeyExchangeSession::getSessionKey(const dtn::data::EID &peer, unsigned int uniqueId)
		{
			// generate session key
			std::stringstream sstm;
			sstm << uniqueId << "." << peer.getString();

			return sstm.str();
		}

		const dtn::data::EID& KeyExchangeSession::getPeer() const
		{
			return _peer;
		}

		int KeyExchangeSession::getProtocol() const
		{
			return _protocol;
		}

		unsigned int KeyExchangeSession::getUniqueId() const
		{
			return _unique_id;
		}

		const std::string& KeyExchangeSession::getSessionKey() const
		{
			return _session_key;
		}

		dtn::security::SecurityKey KeyExchangeSession::getKey(const dtn::security::SecurityKey::KeyType type) const noexcept (false)
		{
			unsigned int id = getUniqueId();
			std::string prefix((char*)&id, sizeof id);

			return SecurityKeyManager::getInstance().get(prefix, _peer, type);
		}

		void KeyExchangeSession::putKey(const std::string &data, const dtn::security::SecurityKey::KeyType type, const dtn::security::SecurityKey::TrustLevel trust) const
		{
			unsigned int id = getUniqueId();
			std::string prefix((char*)&id, sizeof id);

			dtn::security::SecurityKey keydata;

			// assign key type
			keydata.type = type;

			// assign reference
			keydata.reference = _peer;

			// assign trust level
			keydata.trustlevel = trust;

			// set protocol flags
			keydata.flags = (1 << getProtocol());

			// store security key
			SecurityKeyManager::getInstance().store(prefix, keydata, data);
		}

		void KeyExchangeSession::clearKeys() const
		{
			std::list<dtn::security::SecurityKey::KeyType> types;
			types.push_back(dtn::security::SecurityKey::KEY_SHARED);
			types.push_back(dtn::security::SecurityKey::KEY_PUBLIC);
			types.push_back(dtn::security::SecurityKey::KEY_PRIVATE);

			for (std::list<dtn::security::SecurityKey::KeyType>::const_iterator it = types.begin(); it != types.end(); ++it)
			{
				try {
					SecurityKeyManager::getInstance().remove( getKey(*it) );
				} catch (const SecurityKey::KeyNotFoundException &e) {
					// key not found
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 25) << e.what() << IBRCOMMON_LOGGER_ENDL;
				}
			}
		}
	} /* namespace security */
} /* namespace dtn */
