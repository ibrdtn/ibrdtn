/*
 * lowpansocket.h
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Stefan Schmidt <stefan@datenfreihafen.org>
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

#ifndef LOWPANSOCKET_H_
#define LOWPANSOCKET_H_

#include "ibrcommon/net/vinterface.h"
#include "ibrcommon/net/vsocket.h"
#include "ibrcommon/net/vaddress.h"
#include "ibrcommon/Exceptions.h"

/* Move to a better place */
#include "ibrcommon/net/ieee802154.h"

namespace ibrcommon
{
	class lowpansocket : public datagramsocket
	{
	public:
		lowpansocket(const uint16_t &panid, const vinterface &iface);
		~lowpansocket();
		void up() noexcept (false);
		void down() noexcept (false);

		/**
		 * If set to true, the auto-ack request is set on each message
		 */
		void setAutoAck(bool enable) noexcept (false);

		virtual ssize_t recvfrom(char *buf, size_t buflen, int flags, ibrcommon::vaddress &addr) noexcept (false);
		virtual void sendto(const char *buf, size_t buflen, int flags, const ibrcommon::vaddress &addr) noexcept (false);

		static void getAddress(const vinterface &iface, const std::string &panid, ibrcommon::vaddress &addr);
		static void getAddress(struct ieee802154_addr *ret, const vinterface &iface);

	private:
		const uint16_t _panid;
		const vinterface _iface;
	};
}
#endif /* LOWPANSOCKET_H_ */
