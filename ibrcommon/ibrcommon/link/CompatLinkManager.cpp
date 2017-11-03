/*
 * NetLinkManager.cpp
 *
 * Copyright (C) 2017 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <jm@m-network.de>
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
#include "CompatLinkManager.h"

#if defined __WIN32__
#include "ibrcommon/link/Win32LinkManager.h"
#else
#if defined HAVE_LIBNL || HAVE_LIBNL2 || HAVE_LIBNL3
#include "ibrcommon/link/NetLinkManager.h"
#endif
#include "ibrcommon/link/PosixLinkManager.h"
#endif

namespace ibrcommon
{
	CompatLinkManager::CompatLinkManager()
	 : _manager(NULL)
	{
#if defined __WIN32__
		_manager = new Win32LinkManager();
#else
#if defined HAVE_LIBNL || HAVE_LIBNL2 || HAVE_LIBNL3
		try {
			_manager = new NetLinkManager();
		} catch (const ibrcommon::Exception &e) {
			_manager = new PosixLinkManager();
		}
#else
		_manager = new PosixLinkManager();
#endif
#endif

	}

	CompatLinkManager::~CompatLinkManager()
	{
		delete _manager;
	}

	void CompatLinkManager::up() throw ()
	{
		_manager->up();
	}

	void CompatLinkManager::down() throw ()
	{
		_manager->down();
	}

	const vinterface CompatLinkManager::getInterface(int index) const
	{
		return _manager->getInterface(index);
	}

	const std::list<vaddress> CompatLinkManager::getAddressList(const vinterface &iface, const std::string &scope)
	{
		return _manager->getAddressList(iface, scope);
	}

} /* namespace ibrcommon */
