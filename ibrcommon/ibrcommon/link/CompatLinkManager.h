/*
 * NetLinkManager.h
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

#ifndef COMPATLINKMANAGER_H_
#define COMPATLINKMANAGER_H_

#include "ibrcommon/link/LinkManager.h"

namespace ibrcommon
{
	class CompatLinkManager : public ibrcommon::LinkManager
	{
	public:
		CompatLinkManager();
		virtual ~CompatLinkManager();

		void up() throw ();
		void down() throw ();

		const vinterface getInterface(int index) const;
		const std::list<vaddress> getAddressList(const vinterface &iface, const std::string &scope = "");

	private:
		LinkManager *_manager;
	};
}

#endif /* COMPATLINKMANAGER_H_ */
