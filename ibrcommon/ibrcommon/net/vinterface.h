/*
 * vinterface.h
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

#ifndef VINTERFACE_H_
#define VINTERFACE_H_

#include "ibrcommon/net/vaddress.h"

#include <list>
#include <string>
#include <stdint.h>

namespace ibrcommon
{
	class LinkManagerEvent;

	class vinterface
	{
	public:
		class interface_not_set : public Exception
		{
		public:
			interface_not_set(std::string error = "interface is not specified") : Exception(error)
			{};
		};

		const static std::string LOOPBACK;
		const static std::string ANY;

		vinterface();
		vinterface(const std::string &name);
#ifdef __WIN32__
		vinterface(const std::string &name, const std::wstring &friendlyName);
#endif
		virtual ~vinterface();

		bool isLoopback() const;
		bool isAny() const;

		uint32_t getIndex() const;
		const std::list<vaddress> getAddresses(const std::string &scope = "") const;
		const std::string toString() const;
		bool empty() const;
		bool up() const;

#ifdef __WIN32__
		const std::wstring& getFriendlyName() const;
#endif

		bool operator<(const vinterface &obj) const;
		bool operator==(const vinterface &obj) const;
		bool operator!=(const vinterface &obj) const;

		void eventNotify(const LinkManagerEvent &evt);

	private:
		std::string _name;
#ifdef __WIN32__
		std::wstring _friendlyName;
#endif
	};
}

#endif /* VINTERFACE_H_ */
