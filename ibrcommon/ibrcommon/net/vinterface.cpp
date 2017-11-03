/*
 * vinterface.cpp
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
#include "ibrcommon/net/vinterface.h"
#include "ibrcommon/net/vsocket.h"

#ifdef __WIN32__
#include <winsock2.h>
#include <windows.h>
#include <ddk/ntddndis.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <net/if.h>
#endif

namespace ibrcommon
{
	const std::string vinterface::LOOPBACK = "loopback";
	const std::string vinterface::ANY = "any";

	vinterface::vinterface()
	 : _name(ANY)
	{
	}

	vinterface::vinterface(const std::string &name)
	 : _name(name)
	{
	}

#ifdef __WIN32__
	vinterface::vinterface(const std::string &name, const std::wstring &friendlyName)
	 : _name(name), _friendlyName(friendlyName)
	{
	}
#endif

	vinterface::~vinterface()
	{
	}

	const std::string vinterface::toString() const
	{
		if (_name.length() == 0) return "<undefined>";
		if (isAny()) return "<any>";
		if (isLoopback()) return "<loopback>";
		return _name;
	}

#ifdef __WIN32__
	const std::wstring& vinterface::getFriendlyName() const
	{
		return _friendlyName;
	}
#endif

	bool vinterface::isLoopback() const
	{
		return (_name == LOOPBACK);
	}

	bool vinterface::isAny() const
	{
		return (_name == ANY);
	}

	uint32_t vinterface::getIndex() const
	{
#ifdef __WIN32__
		PULONG index = 0;
		GetAdapterIndex(LPWSTR(_name.c_str()), index);
		return (uint32_t)index;
#else
		return if_nametoindex(_name.c_str());
#endif
	}

	const std::list<vaddress> vinterface::getAddresses(const std::string &scope) const
	{
		if (empty()) throw interface_not_set();

		if (isAny()) {
			std::list<vaddress> ret;
			if (ibrcommon::basesocket::hasSupport(AF_INET6))
				ret.push_back(ibrcommon::vaddress(0, (sa_family_t)AF_INET6));
			ret.push_back(ibrcommon::vaddress(0, (sa_family_t)AF_INET));
			return ret;
		}

		return ibrcommon::LinkManager::getInstance().getAddressList(*this, scope);
	}

	bool vinterface::up() const {
		return !getAddresses().empty();
	}

	bool vinterface::operator<(const vinterface &obj) const
	{
		return (_name < obj._name);
	}

	bool vinterface::operator==(const vinterface &obj) const
	{
		return (obj._name == _name);
	}

	bool vinterface::operator!=(const vinterface &obj) const
	{
		return (obj._name != _name);
	}

	bool vinterface::empty() const
	{
		return (_name.length() == 0);
	}
}
