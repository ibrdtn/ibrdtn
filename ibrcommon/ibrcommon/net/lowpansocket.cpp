/*
 * lowpansocket.cpp
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

#include "ibrcommon/config.h"
#include "ibrcommon/net/lowpansocket.h"
#include "ibrcommon/Logger.h"
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <iomanip>

#if defined HAVE_LIBNL || HAVE_LIBNL3
#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#endif

extern "C" {
#include "ibrcommon/net/ieee802154.h"
#include "ibrcommon/link/nl802154.h"
extern struct nla_policy ieee802154_policy[IEEE802154_ATTR_MAX + 1];
}

namespace ibrcommon
{
	lowpansocket::lowpansocket(const uint16_t &panid, const vinterface &iface)
	 : _panid(panid), _iface(iface)
	{
	}

	lowpansocket::~lowpansocket()
	{

	}

	void lowpansocket::up() noexcept (false)
	{
		if (_state != SOCKET_DOWN)
			throw socket_exception("socket is already up");

		// Create socket for listening for client connection requests.
		init_socket(PF_IEEE802154, SOCK_DGRAM, 0);

		struct sockaddr_ieee802154 sockaddr;
		bzero(&sockaddr, sizeof(sockaddr));

		sockaddr.family = AF_IEEE802154;
		sockaddr.addr.addr_type = IEEE802154_ADDR_SHORT;
		sockaddr.addr.pan_id = _panid;

		// Get address via netlink
		getAddress(&sockaddr.addr, _iface);

		if ( ::bind(_fd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0 )
		{
			throw socket_exception("cannot bind socket");
		}

		_state = SOCKET_UP;
	}

	void lowpansocket::down() noexcept (false)
	{
		if ((_state == SOCKET_DOWN) || (_state == SOCKET_DESTROYED))
			throw socket_exception("socket is not up");

		this->close();
	}

	void lowpansocket::setAutoAck(bool enable) noexcept (false)
	{
		int optval = (enable ? 1 : 0);
		if (::setsockopt(_fd, SOL_IEEE802154, WPAN_WANTACK, &optval, sizeof(optval)) < 0) {
			throw ibrcommon::socket_exception("can not set auto-ack feature");
		}
	}

	ssize_t lowpansocket::recvfrom(char *buf, size_t buflen, int flags, ibrcommon::vaddress &addr) noexcept (false)
	{
		struct sockaddr_storage clientAddress;
		socklen_t clientAddressLength = sizeof(clientAddress);
		::memset(&clientAddress, 0, clientAddressLength);

		// data waiting
		ssize_t ret = ::recvfrom(_fd, buf, buflen, flags, (struct sockaddr *) &clientAddress, &clientAddressLength);

		if (ret == -1) {
			throw socket_exception("recvfrom error");
		}

		struct sockaddr_ieee802154 *addr154 = (struct sockaddr_ieee802154*)&clientAddress;

		std::stringstream ss_pan; ss_pan << addr154->addr.pan_id;
		std::stringstream ss_short; ss_short << addr154->addr.short_addr;

		addr = ibrcommon::vaddress(ss_short.str(), ss_pan.str(), clientAddress.ss_family);

		return ret;
	}

	void lowpansocket::sendto(const char *buf, size_t buflen, int flags, const ibrcommon::vaddress &addr) noexcept (false)
	{
		ssize_t ret = 0;
		struct sockaddr_ieee802154 sockaddr;
		bzero(&sockaddr, sizeof(sockaddr));

		sockaddr.family = AF_IEEE802154;
		sockaddr.addr.addr_type = IEEE802154_ADDR_SHORT;

		std::stringstream ss_pan(addr.service());
		ss_pan >> sockaddr.addr.pan_id;

		std::stringstream ss_short(addr.address());
		ss_short >> sockaddr.addr.short_addr;

		::connect(_fd, (struct sockaddr *) &sockaddr, sizeof(sockaddr));
		//printf("lowpan send() address %04x, PAN %04x\n", _destaddress.addr.short_addr, _destaddress.addr.pan_id);
		//return ::sendto(_socket._socket, data, length, 0, (struct sockaddr *) &_destaddress, sizeof(_destaddress));
		ret = ::send(_fd, buf, buflen, flags);

		if (ret == -1) {
			throw socket_raw_error(errno);
		}
	}

	void lowpansocket::getAddress(const vinterface &iface, const std::string &panid, ibrcommon::vaddress &addr)
	{
		struct sockaddr_ieee802154 address;
		address.addr.addr_type = IEEE802154_ADDR_SHORT;

		lowpansocket::getAddress(&address.addr, iface);

		std::stringstream ss_pan; ss_pan << address.addr.pan_id;
		std::stringstream ss_addr; ss_addr << address.addr.short_addr;

		addr = ibrcommon::vaddress(ss_addr.str(), panid, address.family);
	}

	void lowpansocket::getAddress(struct ieee802154_addr *ret, const vinterface &iface)
	{
#if defined HAVE_LIBNL || HAVE_LIBNL2 || HAVE_LIBNL3
#if defined HAVE_LIBNL2 || HAVE_LIBNL3
		struct nl_sock *nl = nl_socket_alloc();
#else
		struct nl_handle *nl = nl_handle_alloc();
#endif
		unsigned char *buf = NULL;
		struct sockaddr_nl nla;
		struct nlattr *attrs[IEEE802154_ATTR_MAX+1];
		//struct genlmsghdr *ghdr;
		struct nlmsghdr *nlh;
		struct nl_msg *msg;
		int family;

		if (!nl)
			return;

		genl_connect(nl);

		/* Build and send message */
		msg = nlmsg_alloc();
		family = genl_ctrl_resolve(nl, "802.15.4 MAC");
		genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family, 0, NLM_F_ECHO, IEEE802154_LIST_IFACE, 1);
		nla_put_string(msg, IEEE802154_ATTR_DEV_NAME, iface.toString().c_str());
		nl_send_auto_complete(nl, msg);
		nlmsg_free(msg);

		/* Receive and parse answer */
		nl_recv(nl, &nla, &buf, NULL);
		nlh = (struct nlmsghdr*)buf;
		genlmsg_parse(nlh, 0, attrs, IEEE802154_ATTR_MAX, ieee802154_policy);
		//ghdr = (genlmsghdr*)nlmsg_data(nlh);
		if (!attrs[IEEE802154_ATTR_SHORT_ADDR] || !attrs[IEEE802154_ATTR_SHORT_ADDR])
			return;

		// We only handle short addresses right now
		ret->addr_type = IEEE802154_ADDR_SHORT;
		ret->pan_id = nla_get_u16(attrs[IEEE802154_ATTR_PAN_ID]);
		ret->short_addr = nla_get_u16(attrs[IEEE802154_ATTR_SHORT_ADDR]);

		free(buf);
		nl_close(nl);

#if defined HAVE_LIBNL2 || HAVE_LIBNL3
		nl_socket_free(nl);
#else
		nl_handle_destroy(nl);
#endif
#endif
	}
}
