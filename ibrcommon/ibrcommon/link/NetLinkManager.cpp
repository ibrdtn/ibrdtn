/*
 * NetLinkManager.cpp
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

#include "ibrcommon/config.h"
#include "ibrcommon/link/NetLinkManager.h"
#include "ibrcommon/thread/MutexLock.h"
#include "ibrcommon/Logger.h"

#if defined HAVE_LIBNL2 || HAVE_LIBNL3
#include <netlink/version.h>
#endif

#ifdef HAVE_LIBNL3
#if (LIBNL_VER_NUM >= LIBNL_VER(3,2)) && (LIBNL_VER_MIC == 21)
#define LIBNL_ABOVE_3_2_21
#endif
#endif

#ifdef LIBNL_ABOVE_3_2_21
// netlink API header for >= 3.2.21
#include <netlink/object.h>
#include <netlink/cache.h>
// BEGIN: workaround for missing NL_ACT_* declaration in version 3.2.21
#if !(defined NL_ACT_NEW || defined NL_ACT_DEL)
#define NL_ACT_NEW 1
#define NL_ACT_DEL 2
#endif
// END: workaround
#else
// netlink API header for < 3.2.21
#include <netlink/object-api.h>
#include <netlink/cache-api.h>
#endif

#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <netlink/route/rtnl.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>

#ifndef IFF_UP
#include <net/if.h>
#endif

#include <iostream>
#include <sstream>
#include <typeinfo>

namespace ibrcommon
{
#ifndef HAVE_LIBNL3
	struct __nl_object {
			NLHDR_COMMON
	};

	int nl_object_get_msgtype(struct nl_object *obj) {
			return static_cast<__nl_object*>(nl_object_priv(obj))->ce_msgtype;
	}
#else
#if LIBNL_CURRENT < 206
	// The libnl method "nl_object_get_msgtype" is available since
	// version 206 of libnl-3.
	int nl_object_get_msgtype(struct nl_object *obj) {
			return static_cast<nl_object*>(nl_object_priv(obj))->ce_msgtype;
	}
#endif
#endif

	vaddress rtnl_addr_get_local_vaddress(struct rtnl_addr *obj) {
		struct nl_addr *naddr = rtnl_addr_get_local(obj);
		if (!naddr) return vaddress();

		int scope = rtnl_addr_get_scope((struct rtnl_addr *) obj);

		char scope_buf[256];
		rtnl_scope2str(scope, scope_buf, sizeof( scope_buf ));
		std::string scopename(scope_buf);

		struct sockaddr_storage sa_addr;
		socklen_t sa_len = sizeof(sockaddr_storage);
		::memset(&sa_addr, 0, sa_len);

		if (nl_addr_fill_sockaddr(naddr, (struct sockaddr*)&sa_addr, &sa_len) < 0) {
			return vaddress();
		}

		char addr_buf[256];
		if (::getnameinfo((struct sockaddr *) &sa_addr, sa_len, addr_buf, sizeof addr_buf, NULL, 0, NI_NUMERICHOST) != 0) {
			// error
			return vaddress();
		}

		std::string addrname(addr_buf);

		/*
		 * nl_addr_fill_sockaddr is does not copy the right scope id for ipv6 sockets
		 * which is necessary for link local-local addresses. Thus we add the interface suffix to
		 * all link-local IPv6 addresses.
		 */
		if ((sa_addr.ss_family == AF_INET6) && (scopename == vaddress::SCOPE_LINKLOCAL)) {
			int ifindex = rtnl_addr_get_ifindex((struct rtnl_addr *) obj);
			vinterface iface = LinkManager::getInstance().getInterface(ifindex);
			addrname += "%" + iface.toString();
		}

		// set loop-back address to scope local
		if (sa_addr.ss_family == AF_INET6) {
			// get ipv6 specific address
			sockaddr_in6 *addr6 = (sockaddr_in6*)&sa_addr;

			if (IN6_IS_ADDR_LOOPBACK(&(addr6->sin6_addr))) {
				// loop-back address
				scopename = ibrcommon::vaddress::SCOPE_LOCAL;
			}
		}
		else if (sa_addr.ss_family == AF_INET) {
			// get ipv6 specific address
			sockaddr_in *addr = (sockaddr_in*)&sa_addr;

			if ((addr->sin_addr.s_addr & htonl(0xff000000)) == htonl(0x7F000000)) {
				// loop-back address
				scopename = ibrcommon::vaddress::SCOPE_LOCAL;
			}
		}

		return vaddress(addrname, "", scopename, sa_addr.ss_family);
	}

#if defined HAVE_LIBNL2 || HAVE_LIBNL3
	static void nl_cache_callback(struct nl_cache*, struct nl_object *obj, int action, void*)
#else
	static void nl_cache_callback(struct nl_cache*, struct nl_object *obj, int action)
#endif
	{
		if (obj == NULL) return;

		switch (nl_object_get_msgtype(obj)) {
			case RTM_NEWLINK: {
				LinkEvent::Action evt_action = LinkEvent::ACTION_UNKOWN;

				struct rtnl_link *link = (struct rtnl_link *) obj;

				int ifindex = rtnl_link_get_ifindex(link);
				vinterface iface = LinkManager::getInstance().getInterface(ifindex);

				struct nl_addr *naddr = rtnl_link_get_addr(link);

				// default empty address
				vaddress addr;

				if (naddr) {
					char addr_buf[256];
					nl_addr2str( naddr, addr_buf, sizeof( addr_buf ));
					std::string addrname(addr_buf);

					addr = vaddress(addrname, "", static_cast<sa_family_t>(nl_addr_guess_family(naddr)));
				}

				unsigned int flags = rtnl_link_get_flags(link);

				if (flags & IFF_RUNNING) {
					evt_action = LinkEvent::ACTION_LINK_RUNNING;
				} else if (flags & IFF_UP) {
					evt_action = LinkEvent::ACTION_LINK_UP;
				} else {
					evt_action = LinkEvent::ACTION_LINK_DOWN;
				}

				LinkEvent lme(evt_action, iface, addr);;
				LinkManager::getInstance().raiseEvent(lme);
				break;
			}

			case RTM_NEWADDR: {
#if defined HAVE_LIBNL2 || HAVE_LIBNL3
				LinkEvent::Action evt_action = LinkEvent::ACTION_UNKOWN;

				if (action == NL_ACT_NEW)
					evt_action = LinkEvent::ACTION_ADDRESS_ADDED;
				else if (action == NL_ACT_DEL)
					evt_action = LinkEvent::ACTION_ADDRESS_REMOVED;

				int ifindex = rtnl_addr_get_ifindex((struct rtnl_addr *) obj);
				vinterface iface = LinkManager::getInstance().getInterface(ifindex);

				vaddress addr = rtnl_addr_get_local_vaddress((struct rtnl_addr *) obj);

				LinkEvent lme(evt_action, iface, addr);;
				LinkManager::getInstance().raiseEvent(lme);
				break;
#endif
			}

			default:
				if (IBRCOMMON_LOGGER_LEVEL > 90) {
				struct nl_dump_params dp;
				memset(&dp, 0, sizeof(struct nl_dump_params));
#if defined HAVE_LIBNL2 || HAVE_LIBNL3
				dp.dp_type = NL_DUMP_LINE;
#else
				dp.dp_type = NL_DUMP_BRIEF;
#endif
				dp.dp_fd = stdout;
				nl_object_dump(obj, &dp);
				}
				break;
		}
	}

	void add_addr_to_list(struct nl_object *obj, void *data)
	{
		std::list<vaddress> *list = static_cast<std::list<vaddress>*>(data);
		vaddress addr = rtnl_addr_get_local_vaddress((struct rtnl_addr *) obj);

		try {
			addr.address();
			list->push_back( addr );
		} catch (const vaddress::address_not_set&) {
		}
	}

	NetLinkManager::netlinkcache::netlinkcache(int protocol)
	 : _protocol(protocol), _nl_handle(NULL), _mngr(NULL)
	{
	}

	NetLinkManager::netlinkcache::~netlinkcache()
	{
		if (_nl_handle != NULL)
		{
			// delete the socket
			nl_socket_free((struct nl_sock*)_nl_handle);
			_nl_handle = NULL;
		}
	}

	void NetLinkManager::netlinkcache::up() throw (socket_exception)
	{
		if (_state != SOCKET_DOWN)
			throw socket_exception("socket is already up");

		// create netlink handle
#if defined HAVE_LIBNL2 || HAVE_LIBNL3
		int ret = 0;
		_nl_handle = nl_socket_alloc();
#else
		_nl_handle = nl_handle_alloc();
#endif

		// allocate a cache manager for ROUTE
#if defined HAVE_LIBNL2 || HAVE_LIBNL3
		ret = nl_cache_mngr_alloc((struct nl_sock*)_nl_handle, _protocol, NL_AUTO_PROVIDE, &_mngr);

		if (ret != 0)
			throw socket_exception("can not allocate netlink cache manager");
#else
		_mngr = nl_cache_mngr_alloc((struct nl_handle*)_nl_handle, _protocol, NL_AUTO_PROVIDE);
#endif

		if (_mngr == NULL)
			throw socket_exception("can not allocate netlink cache manager");

		for (std::map<std::string, struct nl_cache*>::iterator iter = _caches.begin(); iter != _caches.end(); ++iter)
		{
			const std::string &cachename = (*iter).first;
#if defined HAVE_LIBNL2 || HAVE_LIBNL3
			struct nl_cache *c;
			ret = nl_cache_mngr_add(_mngr, cachename.c_str(), &nl_cache_callback, this, &c);

			if (ret != 0)
				throw socket_exception(std::string("can not allocate netlink cache ") + cachename);
#else
			struct nl_cache *c = nl_cache_mngr_add(_mngr, cachename.c_str(), &nl_cache_callback);
#endif

			if (c == NULL)
				throw socket_exception(std::string("can not allocate netlink cache ") + cachename);

			(*iter).second = c;
		}

		// mark this socket as up
		_state = SOCKET_UP;
	}

	void NetLinkManager::netlinkcache::down() throw (socket_exception)
	{
		if ((_state == SOCKET_DOWN) || (_state == SOCKET_DESTROYED))
			throw socket_exception("socket is not up");

		// delete the cache manager
		nl_cache_mngr_free(_mngr);

#if defined HAVE_LIBNL2 || HAVE_LIBNL3
		// delete the socket
		nl_socket_free((struct nl_sock*)_nl_handle);
		_nl_handle = NULL;
#endif

		// mark this socket as down
		if (_state == SOCKET_UNMANAGED)
			_state = SOCKET_DESTROYED;
		else
			_state = SOCKET_DOWN;
	}

	int NetLinkManager::netlinkcache::fd() const throw (socket_exception)
	{
		if (_state == SOCKET_DOWN) throw socket_exception("fd not available");
		return nl_cache_mngr_get_fd(_mngr);
	}

	void NetLinkManager::netlinkcache::receive() throw (socket_exception)
	{
		if (_state == SOCKET_DOWN) throw socket_exception("socket not connected");
		nl_cache_mngr_data_ready(_mngr);
#if 0
		int ret = nl_cache_mngr_data_ready(_mngr);

		/*
		 * Some implementations always return an error code, because they reached
		 * the last message. In that cases we should not throw an exception. Since
		 * error checking here is not so important we disable this feature.
		 */
		if (ret < 0)
			throw socket_exception("can not receive data from netlink manager");
#endif
	}

	void NetLinkManager::netlinkcache::add(const std::string &cachename) throw (socket_exception)
	{
		if (_state != SOCKET_DOWN)
			throw socket_exception("socket is already up; adding not possible");

		std::map<std::string, struct nl_cache*>::const_iterator iter = _caches.find(cachename);
		if (iter != _caches.end()) throw socket_exception("cache already added");

		// add placeholder to the map
		_caches[cachename] = NULL;
	}

	struct nl_cache* NetLinkManager::netlinkcache::get(const std::string &cachename) const throw (socket_exception)
	{
		if (_state == SOCKET_DOWN) throw socket_exception("socket not available");

		std::map<std::string, struct nl_cache*>::const_iterator iter = _caches.find(cachename);
		if (iter == _caches.end()) throw socket_exception("cache not available");

		return (*iter).second;
	}

	NetLinkManager::NetLinkManager()
	 : _route_cache(NETLINK_ROUTE), _running(true)
	{
		// add cache types to the route cache
		_route_cache.add("route/link");
		_route_cache.add("route/addr");

		try {
			// initialize the netlink cache
			_route_cache.up();
		} catch (const ibrcommon::socket_exception &e) {
			// join on errors
			join();

			// re-throw the original exception
			throw;
		}
	}

	NetLinkManager::~NetLinkManager()
	{
		stop();
		join();

		try {
			_route_cache.down();
		} catch (const socket_exception&) {
			// catch socket exception caused by double down()
			// __cancellation() + destructor
		}
	}

	void NetLinkManager::up() throw ()
	{
		// add netlink fd to vsocket
		_sock.add(&_route_cache);
		_sock.up();

		this->start();
	}

	void NetLinkManager::down() throw ()
	{
		_sock.down();
		_sock.clear();

		this->stop();
		this->join();
	}

	void NetLinkManager::__cancellation() throw ()
	{
		_running = false;
		_sock.down();
	}

	void NetLinkManager::run() throw ()
	{
		try {
			while (_running)
			{
				socketset socks;
				_sock.select(&socks, NULL, NULL, NULL);

				for (socketset::iterator iter = socks.begin(); iter != socks.end(); ++iter) {
					try {
						netlinkcache &cache = dynamic_cast<netlinkcache&>(**iter);
						cache.receive();
					} catch (const std::bad_cast&) { };
				}
			}
		} catch (const socket_exception&) {
			// stopped / interrupted
			IBRCOMMON_LOGGER_TAG("NetLinkManager", error) << "NetLink connection stopped" << IBRCOMMON_LOGGER_ENDL;
		}
	}

	const std::list<vaddress> NetLinkManager::getAddressList(const vinterface &iface, const std::string &scope)
	{
		ibrcommon::MutexLock l(_cache_mutex);

		std::list<vaddress> addresses;

		struct rtnl_addr *filter = rtnl_addr_alloc();
		const std::string i = iface.toString();
		int ifindex = rtnl_link_name2i(_route_cache.get("route/link"), i.c_str());

		rtnl_addr_set_ifindex(filter, ifindex);

		// set scope if requested
		if (scope.length() > 0) {
			rtnl_addr_set_scope(filter, rtnl_str2scope(scope.c_str()));
		}

		// query the netlink cache for all known addresses
		nl_cache_foreach_filter(_route_cache.get("route/addr"), (struct nl_object *) filter, add_addr_to_list, &addresses);

		// free the filter address
		rtnl_addr_put(filter);

		return addresses;
	}

	const vinterface NetLinkManager::getInterface(const int ifindex) const
	{
		char buf[256];
		rtnl_link_i2name(_route_cache.get("route/link"), ifindex, (char*)&buf, sizeof buf);
		return std::string((char*)&buf);
	}
}
