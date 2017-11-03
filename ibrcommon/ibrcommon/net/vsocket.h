/*
 * vsocket.h
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

#ifndef VSOCKET_H_
#define VSOCKET_H_

#include <ibrcommon/data/File.h>
#include "ibrcommon/link/LinkManager.h"
#include <ibrcommon/net/socket.h>
#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/net/vaddress.h>
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/Conditional.h>
#include <string>
#include <list>
#include <set>
#include <map>

namespace ibrcommon
{
	/**
	 * socketset is used for socket select calls and managing the socket instances
	 */
	typedef std::set<basesocket*> socketset;

	class vsocket_timeout : public socket_exception
	{
	public:
		vsocket_timeout(std::string error) : socket_exception(error)
		{};
	};

	class vsocket_interrupt : public socket_exception
	{
	public:
		vsocket_interrupt(std::string error) : socket_exception(error)
		{};
	};

	class vsocket
	{
	public:
		/**
		 * Constructor
		 */
		vsocket();

		/**
		 * Destructor
		 */
		virtual ~vsocket();

		/**
		 * Add a socket to the list monitored sockets. This socket instance
		 * will be adapted and destroyed on the destruction of the vsocket instance.
		 * @param socket Socket object to add
		 */
		void add(basesocket *socket);

		/**
		 * Add a socket to the list monitored sockets. This socket instance
		 * will be adapted and destroyed on the destruction of the vsocket instance.
		 * The socket will be shutdown if the state of the given interface changes.
		 * @param socket Socket object to add
		 * @param iface The interface to connect this socket to
		 */
		void add(basesocket *socket, const vinterface &iface);

		/**
		 * Remove the socket from the socket list.
		 * @param socket Pointer to the socket in the list
		 */
		void remove(basesocket *socket);

		/**
		 * get the number of added sockets
		 * @return the size of the internal socketset
		 */
		size_t size() const;

		/**
		 * @return a copy of the socketset
		 */
		socketset getAll() const;

		/**
		 * Get all sockets bound to a specific interface
		 * @param iface The selected interface
		 * @return A socketset with all sockets bound to a specific interface
		 */
		socketset get(const vinterface &iface) const;

		/**
		 * delete all sockets in the socketset
		 */
		void destroy();

		/**
		 * clear the socketset
		 */
		void clear();

		/**
		 * Enable all sockets and turn into the up state.
		 */
		void up() throw (socket_exception);

		/**
		 * Disable all sockets and turn into the down state.
		 */
		void down() throw ();

		/**
		 * Execute a select on all associated sockets.
		 * @param callback
		 * @param tv
		 */
		void select(socketset *readset, socketset *writeset, socketset *errorset, struct timeval *tv = NULL) throw (socket_exception);

	private:
		class pipesocket : public basesocket
		{
		public:
			pipesocket();
			virtual ~pipesocket();
			virtual void up() throw (socket_exception);
			virtual void down() throw (socket_exception);

			void read(char *buf, size_t len) throw (socket_exception);
			void write(const char *buf, size_t len) throw (socket_exception);

			int getOutput() const throw (socket_exception);

		private:
			int _output_fd;
		};

		class SocketState : public ibrcommon::Conditional {
		public:
			class state_exception : public Exception
			{
			public:
				state_exception(std::string error) : Exception(error)
				{};
			};

			enum STATE {
				NONE,
				SAFE_DOWN,
				DOWN,
				DOWN_REQUEST,
				PENDING_DOWN,
				PENDING_UP,
				IDLE,
				SELECT,
				SAFE_REQUEST,
				SAFE
			};

			SocketState(STATE initial);
			virtual ~SocketState();

			STATE get() const;

			void set(STATE s) throw (state_exception);
			void wait(STATE s, STATE abortstate = NONE) throw (state_exception);
			void setwait(STATE s, STATE abortstate = NONE) throw (state_exception);

		private:
			void __change(STATE s);
			std::string __getname(STATE s) const;

			STATE _state;
		};

		class SafeLock
		{
		public:
			SafeLock(SocketState &state, vsocket &sock);
			virtual ~SafeLock();
		private:
			SocketState &_state;
		};

		class SelectGuard
		{
		public:
			SelectGuard(SocketState &state, int &counter, ibrcommon::vsocket &sock);
			virtual ~SelectGuard();
		private:
			SocketState &_state;
			int &_counter;
			ibrcommon::vsocket &_sock;
		};

		void interrupt();

		ibrcommon::Mutex _socket_lock;
		socketset _sockets;
		std::map<vinterface, socketset> _socket_map;

		pipesocket _pipe;

		SocketState _state;
		int _select_count;
	};
}

#endif /* VSOCKET_H_ */
