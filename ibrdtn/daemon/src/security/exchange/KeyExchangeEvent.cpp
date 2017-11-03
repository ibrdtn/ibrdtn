/*
 * KeyExchangeEvent.cpp
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

#include "security/exchange/KeyExchangeEvent.h"
#include "core/EventDispatcher.h"

#include <sstream>

namespace dtn
{
	namespace security
	{
		KeyExchangeEvent::KeyExchangeEvent(const dtn::data::EID &eid, const dtn::security::KeyExchangeData &data)
		 : _eid(eid), _data(data)
		{
			switch (_data.getAction())
			{
			case dtn::security::KeyExchangeData::START:
			case dtn::security::KeyExchangeData::REQUEST:
			case dtn::security::KeyExchangeData::RESPONSE:
				setLoggable(false);
				break;

			default:
				break;
			}
		}

		KeyExchangeEvent::~KeyExchangeEvent()
		{}

		const dtn::data::EID& KeyExchangeEvent::getEID() const
		{
			return _eid;
		}

		const dtn::security::KeyExchangeData& KeyExchangeEvent::getData() const
		{
			return _data;
		}

		const std::string KeyExchangeEvent::getName() const
		{
			return KeyExchangeEvent::className;
		}

		std::string KeyExchangeEvent::getMessage() const
		{
			switch (_data.getAction())
			{
				case dtn::security::KeyExchangeData::PASSWORD_REQUEST:
					return "Please enter the password for " + _eid.getString() + " (" + _data.toString() + ")";

				case dtn::security::KeyExchangeData::HASH_COMMIT:
					return "Please compare the hash values for " + _eid.getString() + " (" + _data.toString() + "): " + _data.str();

				case dtn::security::KeyExchangeData::NEWKEY_FOUND:
					return "A new key was found. Please select a key for " + _eid.getString() + " (" + _data.toString() + ")";

				case dtn::security::KeyExchangeData::COMPLETE:
					return "Key-exchange completed for " + _eid.getString() + " (" + _data.toString() + ")";

				case dtn::security::KeyExchangeData::ERROR:
					return "Key-exchange failed for " + _eid.getString() + " (" + _data.toString() + ")";

				case dtn::security::KeyExchangeData::WRONG_PASSWORD:
					return "Entered password does not match the peer; Session " + _eid.getString() + " (" + _data.toString() + ")";

				default: {
					std::stringstream sstm;
					sstm << "Unknown event for " << _eid.getString() << " (" << _data.toString() + ")";
					return sstm.str();
				}
			}
		}

		void KeyExchangeEvent::raise(const dtn::data::EID &eid, const dtn::security::KeyExchangeData &data)
		{
			// raise the new event
			dtn::core::EventDispatcher<KeyExchangeEvent>::queue( new KeyExchangeEvent(eid.getNode(), data) );
		}

		const std::string KeyExchangeEvent::className = "KeyExchangeEvent";
	} /* namespace security */
} /* namespace dtn */
