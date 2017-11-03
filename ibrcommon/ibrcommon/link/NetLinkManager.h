/*
 * NetLinkManager.h
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

#ifndef NETLINKMANAGER_H_
#define NETLINKMANAGER_H_

#include "ibrcommon/link/LinkManager.h"
#include "ibrcommon/net/socket.h"
#include "ibrcommon/net/vsocket.h"
#include "ibrcommon/net/vinterface.h"
#include "ibrcommon/thread/Mutex.h"
#include "ibrcommon/thread/Conditional.h"
#include "ibrcommon/thread/Thread.h"

#include <netlink/cache.h>
#include <map>

namespace ibrcommon
{
	class NetLinkManager : public ibrcommon::LinkManager, public ibrcommon::JoinableThread
	{
	public:
		NetLinkManager();
		virtual ~NetLinkManager();

		void up() throw ();
		void down() throw ();

		const vinterface getInterface(int index) const;
		const std::list<vaddress> getAddressList(const vinterface &iface, const std::string &scope = "");

		class parse_exception : public Exception
		{
		public:
			parse_exception(std::string error) : Exception(error)
			{};
		};

	protected:
		void run() throw ();
		void __cancellation() throw ();

	private:
		class netlinkcache : public basesocket
		{
		public:
			netlinkcache(int protocol);
			virtual ~netlinkcache();

			virtual void up() throw (socket_exception);
			virtual void down() throw (socket_exception);

			virtual int fd() const throw (socket_exception);

			void add(const std::string &cachename) throw (socket_exception);
			struct nl_cache* get(const std::string &cachename) const throw (socket_exception);

			void receive() throw (socket_exception);

		private:
			const int _protocol;

			void *_nl_handle;
			struct nl_cache_mngr *_mngr;

			std::map<std::string, struct nl_cache*> _caches;
		};

		ibrcommon::Mutex _cache_mutex;
		netlinkcache _route_cache;

		bool _running;

		ibrcommon::vsocket _sock;
	};
}

#endif /* NETLINKMANAGER_H_ */
