/*
 * PosixLinkManager.cpp
 *
 *  Created on: 11.10.2012
 *      Author: morgenro
 */

#include "ibrcommon/config.h"
#include "ibrcommon/link/PosixLinkManager.h"

#include <string>
#include <typeinfo>

#include <sys/types.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>

namespace ibrcommon
{
	PosixLinkManager::PosixLinkManager() : _lm(LinkMonitor(*this))
	{
	}

	PosixLinkManager::~PosixLinkManager()
	{
		down();
	}

	void PosixLinkManager::up() noexcept
	{
		_lm.start();
	}

	void PosixLinkManager::down() noexcept
	{
		_lm.stop();
		_lm.join();
	}

	const vinterface PosixLinkManager::getInterface(int index) const
	{
		struct ifaddrs *ifap = NULL;
		int status = getifaddrs(&ifap);
		int i = 0;
		std::string ret;

		if ((status != 0) || (ifap == NULL))
		{
				// error, return with default address
				throw ibrcommon::Exception("can not iterate through interfaces");
		}

		for (struct ifaddrs *iter = ifap; iter != NULL; iter = iter->ifa_next, i++)
		{
			if (index == i)
			{
				ret = iter->ifa_name;
				break;
			}
		}

		freeifaddrs(ifap);

		return ret;
	}

	const std::list<vaddress> PosixLinkManager::getAddressList(const vinterface &iface, const std::string &scope)
	{
		std::list<vaddress> ret;

		struct ifaddrs *ifap = NULL;
		int status = getifaddrs(&ifap);
		int i = 0;

		if ((status != 0) || (ifap == NULL))
		{
			// error, return with default address
			throw ibrcommon::Exception("can not iterate through interfaces");
		}

		for (struct ifaddrs *iter = ifap; iter != NULL; iter = iter->ifa_next, i++)
		{
			if (iter->ifa_addr == NULL) continue;
			if ((iter->ifa_flags & IFF_UP) == 0) continue;

			// get the interface name
			vinterface interface(iter->ifa_name);

			// skip all non-matching interfaces
			if (interface != iface) continue;

			// cast to a sockaddr
			sockaddr *ifaceaddr = iter->ifa_addr;

			if (scope.length() > 0) {
				// scope filter only available with IPv6
				if (ifaceaddr->sa_family == AF_INET6) {
					// get ipv6 specific address
					sockaddr_in6 *addr6 = (sockaddr_in6*)ifaceaddr;

					if (IN6_IS_ADDR_LINKLOCAL(&(addr6->sin6_addr))) {
						if (scope != vaddress::SCOPE_LINKLOCAL) continue;
					}
					else if (IN6_IS_ADDR_LOOPBACK(&(addr6->sin6_addr))) {
						if (scope != vaddress::SCOPE_LOCAL) continue;
					}
					else {
						if (scope != vaddress::SCOPE_GLOBAL) continue;
					}
				}
				else if (ifaceaddr->sa_family == AF_INET) {
					// get ipv6 specific address
					sockaddr_in *addr = (sockaddr_in*)ifaceaddr;

					if ((addr->sin_addr.s_addr & htonl(0xffff0000)) == htonl(0xA9FE0000)) {
						// link-local address
						if (scope != vaddress::SCOPE_LINKLOCAL) continue;
					}
					else if ((addr->sin_addr.s_addr & htonl(0xff000000)) == htonl(0x7F000000)) {
						// loop-back address
						if (scope != vaddress::SCOPE_LOCAL) continue;
					}
					else {
						// global address
						if (scope != vaddress::SCOPE_GLOBAL) continue;
					}
				}
			}

			char address[256];
			if (::getnameinfo((struct sockaddr *) ifaceaddr, sizeof (struct sockaddr_storage), address, sizeof address, 0, 0, NI_NUMERICHOST) == 0) {
				std::string addr_scope = ibrcommon::vaddress::SCOPE_LINKLOCAL;

				if (ifaceaddr->sa_family == AF_INET6) {
					// get ipv6 specific address
					sockaddr_in6 *addr6 = (sockaddr_in6*)ifaceaddr;

					if (IN6_IS_ADDR_LINKLOCAL(&(addr6->sin6_addr))) {
						// link-local address
					}
					else if (IN6_IS_ADDR_LOOPBACK(&(addr6->sin6_addr))) {
						// loop-back address
						addr_scope = ibrcommon::vaddress::SCOPE_LOCAL;
					}
					else {
						addr_scope = ibrcommon::vaddress::SCOPE_GLOBAL;
					}
				}
				else if (ifaceaddr->sa_family == AF_INET) {
					// get ipv6 specific address
					sockaddr_in *addr = (sockaddr_in*)ifaceaddr;

					if ((addr->sin_addr.s_addr & htonl(0xffff0000)) == htonl(0xA9FE0000)) {
						// link-local address
					}
					else if ((addr->sin_addr.s_addr & htonl(0xff000000)) == htonl(0x7F000000)) {
						// loop-back address
						addr_scope = ibrcommon::vaddress::SCOPE_LOCAL;
					}
					else {
						addr_scope = ibrcommon::vaddress::SCOPE_GLOBAL;
					}
				}

				ret.push_back( vaddress(std::string(address), "", addr_scope, ifaceaddr->sa_family) );
			}
		}
		freeifaddrs(ifap);

		return ret;
	}

	void PosixLinkManager::addEventListener(const vinterface &iface, LinkManager::EventCallback *cb) noexcept
	{
		// initialize LinkManager with new listened interface
		_lm.add(iface);

		// call super-method
		LinkManager::addEventListener(iface, cb);
	}

	void PosixLinkManager::removeEventListener(const vinterface &iface, LinkManager::EventCallback *cb) noexcept
	{
		// call super-method
		LinkManager::removeEventListener(iface, cb);

		// remove no longer monitored interfaces
		_lm.remove();
	}

	void PosixLinkManager::removeEventListener(LinkManager::EventCallback *cb) noexcept
	{
		// call super-method
		LinkManager::removeEventListener(cb);

		// remove no longer monitored interfaces
		_lm.remove();
	}
} /* namespace ibrcommon */
