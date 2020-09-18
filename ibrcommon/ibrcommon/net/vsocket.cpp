/*
 * vsocket.cpp
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
#include "ibrcommon/net/vsocket.h"
#include "ibrcommon/thread/MutexLock.h"
#include "ibrcommon/Logger.h"

#ifndef HAVE_FEATURES_H
#include "ibrcommon/TimeMeasurement.h"
#endif

#ifdef __WIN32__
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <arpa/inet.h>
#endif

#include <algorithm>
#include <sys/types.h>
#include <errno.h>
#include <sstream>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace ibrcommon
{
#ifdef __WIN32__
	static int win32_pipe( int handles[2] )
	{
			SOCKET s;
			struct sockaddr_in serv_addr;
			int len = sizeof( serv_addr );

			handles[0] = handles[1] = INVALID_SOCKET;

			if ( ( s = socket( AF_INET, SOCK_STREAM, 0 ) ) == INVALID_SOCKET )
			{
	/*              ereport(LOG, (errmsg_internal("pgpipe failed to create socket: %ui", WSAGetLastError()))); */
					return -1;
			}

			memset( &serv_addr, 0, sizeof( serv_addr ) );
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = htons(0);
			serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			if (bind(s, (SOCKADDR *) & serv_addr, len) == SOCKET_ERROR)
			{
	/*              ereport(LOG, (errmsg_internal("pgpipe failed to bind: %ui", WSAGetLastError()))); */
					closesocket(s);
					return -1;
			}
			if (listen(s, 1) == SOCKET_ERROR)
			{
	/*              ereport(LOG, (errmsg_internal("pgpipe failed to listen: %ui", WSAGetLastError()))); */
					closesocket(s);
					return -1;
			}
			if (getsockname(s, (SOCKADDR *) & serv_addr, &len) == SOCKET_ERROR)
			{
	/*              ereport(LOG, (errmsg_internal("pgpipe failed to getsockname: %ui", WSAGetLastError()))); */
					closesocket(s);
					return -1;
			}
			if ((handles[1] = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
			{
	/*              ereport(LOG, (errmsg_internal("pgpipe failed to create socket 2: %ui", WSAGetLastError()))); */
					closesocket(s);
					return -1;
			}

			if (connect(handles[1], (SOCKADDR *) & serv_addr, len) == SOCKET_ERROR)
			{
	/*              ereport(LOG, (errmsg_internal("pgpipe failed to connect socket: %ui", WSAGetLastError()))); */
					closesocket(s);
					return -1;
			}
			if ((handles[0] = accept(s, (SOCKADDR *) & serv_addr, &len)) == INVALID_SOCKET)
			{
	/*              ereport(LOG, (errmsg_internal("pgpipe failed to accept socket: %ui", WSAGetLastError()))); */
					closesocket(handles[1]);
					handles[1] = INVALID_SOCKET;
					closesocket(s);
					return -1;
			}
			closesocket(s);
			return 0;
	}

	static int piperead( int s, char *buf, int len )
	{
			int ret = recv(s, buf, len, 0);

			if (ret < 0 && WSAGetLastError() == WSAECONNRESET)
					/* EOF on the pipe! (win32 socket based implementation) */
					ret = 0;
			return ret;
	}

#define __compat_pipe(a) win32_pipe(a)
#define pipewrite(a,b,c) send(a,b,c,0)

#else
#define __compat_pipe(a) ::pipe(a)
#define piperead(a,b,c) ::read(a,b,c)
#define pipewrite(a,b,c) ::write(a,b,c)
#endif

#ifdef HAVE_FEATURES_H
#define __compat_select ::select
#else
	int __compat_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
	{
		if (timeout == NULL)
		{
			return ::select(nfds, readfds, writefds, exceptfds, NULL);
		}

		TimeMeasurement tm;

		struct timeval to_copy;
		::memcpy(&to_copy, timeout, sizeof to_copy);

		tm.start();
		int ret = ::select(nfds, readfds, writefds, exceptfds, &to_copy);

		// on timeout set the timeout value to zero
		if (ret == 0)
		{
			timeout->tv_sec = 0;
			timeout->tv_usec = 0;
		}
		else
		{
			tm.stop();

			struct timespec time_spend;
			tm.getTime(time_spend);

			timeout->tv_sec -= time_spend.tv_sec;
			timeout->tv_usec -= time_spend.tv_nsec / 1000;
			if (timeout->tv_usec < 0) {
				--timeout->tv_sec;
				timeout->tv_usec += 1000000L;
			}

			// adjust timeout value if that falls below zero
			if (timeout->tv_sec < 0)
			{
				timeout->tv_sec = 0;
				timeout->tv_usec = 0;
			}
		}

		return ret;
	}
#endif

	vsocket::pipesocket::pipesocket()
	 : _output_fd(-1)
	{ }

	vsocket::pipesocket::~pipesocket()
	{
	}

	int vsocket::pipesocket::getOutput() const noexcept (false)
	{
		if (_state == SOCKET_DOWN) throw socket_exception("output fd not available");
		return _output_fd;
	}

	void vsocket::pipesocket::up() noexcept (false)
	{
		if (_state != SOCKET_DOWN)
			throw socket_exception("socket is already up");

		int pipe_fds[2];

		// create a pipe for interruption
		if (__compat_pipe(pipe_fds) < 0)
		{
			IBRCOMMON_LOGGER_TAG("pipesocket", error) << "Error " << errno << " creating pipe" << IBRCOMMON_LOGGER_ENDL;
			throw socket_exception("failed to create pipe");
		}

		_fd = pipe_fds[0];
		_output_fd = pipe_fds[1];

		this->set_blocking_mode(false);
		this->set_blocking_mode(false, _output_fd);

		_state = SOCKET_UP;
	}

	void vsocket::pipesocket::down() noexcept (false)
	{
		if (_state != SOCKET_UP)
			throw socket_exception("socket is not up");

		this->close();
		::close(_output_fd);

		_state = SOCKET_DOWN;
	}

	void vsocket::pipesocket::read(char *buf, size_t len) noexcept (false)
	{
		ssize_t ret = piperead(this->fd(), buf, len);
		if (ret == -1)
			throw socket_exception("read error");
		if (ret == 0)
			throw socket_exception("end of file");
	}

	void vsocket::pipesocket::write(const char *buf, size_t len) noexcept (false)
	{
		ssize_t ret = pipewrite(_output_fd, buf, len);
		if (ret == -1)
			throw socket_exception("write error");
	}

	vsocket::SocketState::SocketState(STATE initial)
	 : _state(initial)
	{
	}

	vsocket::SocketState::~SocketState()
	{
	}

	vsocket::SocketState::STATE vsocket::SocketState::get() const
	{
		return _state;
	}

	void vsocket::SocketState::set(STATE s) noexcept (false)
	{
		try {
			// try to get into state "s" and wait until this is possible
			switch (s) {
			case NONE:
				throw state_exception("illegal state requested");
			case SAFE_DOWN:
				// throw exception if not (DOWN or PENDING DOWN)
				if ((_state != DOWN) && (_state != PENDING_UP))
					throw state_exception("can not change to " + __getname(s) + ", state is " + __getname(_state) + " instead of DOWN");
				__change(s);
				break;

			case DOWN:
				// throw exception if not (PENDING_DOWN or SAFE_DOWN)
				if ((_state != PENDING_DOWN) && (_state != SAFE_DOWN) && (_state != PENDING_UP))
					throw state_exception("can not change to " + __getname(s) + ", state is " + __getname(_state) + " instead of PENDING_DOWN, PENDING_UP or SAFE_DOWN");
				__change(s);
				break;

			case PENDING_UP:
				// throw exception if not (DOWN)
				if (_state != DOWN)
					throw state_exception("can not change to " + __getname(s) + ", state is " + __getname(_state) + " instead of DOWN");
				__change(s);
				break;

			case PENDING_DOWN:
				// throw exception if not (IDLE, DOWN_REQUEST)
				if ((_state != IDLE) && (_state != DOWN_REQUEST))
					throw state_exception("can not change to " + __getname(s) + ", state is " + __getname(_state) + " instead of IDLE or DOWN_REQUEST");
				__change(s);
				break;

			case IDLE:
				// throw exception if not (PENDING_UP, SAFE, SELECT)
				if ((_state != PENDING_UP) && (_state != SAFE) && (_state != SELECT))
					throw state_exception("can not change to " + __getname(s) + ", state is " + __getname(_state) + " instead of PENDING_UP, SAFE or SELECT");
				__change(s);
				break;

			case SELECT:
				// throw exception if not (IDLE, SELECT)
				if ((_state != IDLE) && (_state != SELECT))
					throw state_exception("can not change to " + __getname(s) + ", state is " + __getname(_state) + " instead of IDLE or SELECT");
				__change(s);
				break;

			case DOWN_REQUEST:
			case SAFE_REQUEST:
				// throw exception if not (SELECT, DOWN_REQUEST)
				if (_state != SELECT)
					throw state_exception("can not change to " + __getname(s) + ", state is " + __getname(_state) + " instead of SELECT or DOWN_REQUEST");
				__change(s);
				break;

			case SAFE:
				// throw exception if not (SAFE_REQUEST, IDLE)
				if ((_state != SAFE_REQUEST) && (_state != IDLE))
					throw state_exception("can not change to " + __getname(s) + ", state is " + __getname(_state) + " instead of SAFE_REQUEST or IDLE");
				__change(s);
				break;
			}
		} catch (const Conditional::ConditionalAbortException &e) {
			throw state_exception(e.what());
		}
	}

	void vsocket::SocketState::setwait(STATE s, STATE abortstate) noexcept (false)
	{
		try {
			// try to get into state "s" and wait until this is possible
			switch (s) {
			case NONE:
				throw state_exception("illegal state requested");
			case SAFE_DOWN:
				// throw exception if not (DOWN or PENDING_UP)
				while ((_state != DOWN) && (_state != PENDING_UP)) {
					if (_state == abortstate)
						throw state_exception("abort state " + __getname(abortstate) + " reached");
					ibrcommon::Conditional::wait();
				}
				__change(s);
				break;

			case DOWN:
				// throw exception if not (PENDING_DOWN or SAFE_DOWN)
				while ((_state != PENDING_DOWN) && (_state != SAFE_DOWN) && (_state != DOWN)) {
					if (_state == abortstate)
						throw state_exception("abort state " + __getname(abortstate) + " reached");
					ibrcommon::Conditional::wait();
				}
				__change(s);
				break;

			case PENDING_UP:
				// throw exception if not (DOWN)
				while (_state != DOWN) {
					if (_state == abortstate)
						throw state_exception("abort state " + __getname(abortstate) + " reached");
					ibrcommon::Conditional::wait();
				}
				__change(s);
				break;

			case PENDING_DOWN:
				// throw exception if not (IDLE, DOWN_REQUEST)
				while ((_state != IDLE) && (_state != DOWN_REQUEST)) {
					if (_state == abortstate)
						throw state_exception("abort state " + __getname(abortstate) + " reached");
					ibrcommon::Conditional::wait();
				}
				__change(s);
				break;

			case IDLE:
				// throw exception if not (PENDING_UP, SAFE, SELECT)
				while ((_state != PENDING_UP) && (_state != SAFE) && (_state != SELECT)) {
					if (_state == abortstate)
						throw state_exception("abort state " + __getname(abortstate) + " reached");
					ibrcommon::Conditional::wait();
				}
				__change(s);
				break;

			case SELECT:
				// throw exception if not (IDLE, SELECT)
				while ((_state != IDLE) && (_state != SELECT)) {
					if (_state == abortstate)
						throw state_exception("abort state " + __getname(abortstate) + " reached");
					ibrcommon::Conditional::wait();
				}
				__change(s);
				break;

			case DOWN_REQUEST:
			case SAFE_REQUEST:
				// throw exception if not (SELECT)
				while (_state != SELECT) {
					if (_state == abortstate)
						throw state_exception("abort state " + __getname(abortstate) + " reached");
					ibrcommon::Conditional::wait();
				}
				__change(s);
				break;

			case SAFE:
				// throw exception if not (SAFE_REQUEST, IDLE)
				while ((_state != SAFE_REQUEST) && (_state != IDLE)) {
					if (_state == abortstate)
						throw state_exception("abort state " + __getname(abortstate) + " reached");
					ibrcommon::Conditional::wait();
				}
				__change(s);
				break;
			}
		} catch (const Conditional::ConditionalAbortException &e) {
			throw state_exception(e.what());
		}
	}

	void vsocket::SocketState::wait(STATE s, STATE abortstate) noexcept (false)
	{
		try {
			while (_state != s) {
				if (_state == abortstate)
					throw state_exception("abort state " + __getname(abortstate) + " reached");
				ibrcommon::Conditional::wait();
			}
		} catch (const Conditional::ConditionalAbortException &e) {
			throw state_exception(e.what());
		}
	}

	void vsocket::SocketState::__change(STATE s)
	{
		IBRCOMMON_LOGGER_DEBUG_TAG("SocketState", 90) << "SocketState transition: " << __getname(_state) << " -> " << __getname(s) << IBRCOMMON_LOGGER_ENDL;
		_state = s;
		ibrcommon::Conditional::signal(true);
	}

	std::string vsocket::SocketState::__getname(STATE s) const
	{
		switch (s) {
		case NONE:
			return "NONE";
		case SAFE_DOWN:
			return "SAFE_DOWN";
		case DOWN:
			return "DOWN";
		case PENDING_UP:
			return "PENDING_UP";
		case PENDING_DOWN:
			return "PENDING_DOWN";
		case IDLE:
			return "IDLE";
		case SELECT:
			return "SELECT";
		case SAFE_REQUEST:
			return "SAFE_REQUEST";
		case DOWN_REQUEST:
			return "DOWN_REQUEST";
		case SAFE:
			return "SAFE";
		}

		return "<unkown>";
	}

	vsocket::SafeLock::SafeLock(SocketState &state, vsocket &sock)
	 : _state(state)
	{
		// request safe-state
		ibrcommon::MutexLock l(_state);

		while ( (_state.get() != SocketState::DOWN) && (_state.get() != SocketState::IDLE) && (_state.get() != SocketState::SELECT) ) {
			((ibrcommon::Conditional&)_state).wait();
		}

		if (_state.get() == SocketState::SELECT) {
			_state.set(SocketState::SAFE_REQUEST);
			// send interrupt
			sock.interrupt();
			_state.wait(SocketState::SAFE);
		} else if (_state.get() == SocketState::DOWN) {
			_state.set(SocketState::SAFE_DOWN);
		} else {
			_state.set(SocketState::SAFE);
		}
	}

	vsocket::SafeLock::~SafeLock()
	{
		// release safe-state
		ibrcommon::MutexLock l(_state);
		if (_state.get() == SocketState::SAFE)
		{
			_state.set(SocketState::IDLE);
		}
		else if (_state.get() == SocketState::SAFE_DOWN)
		{
			_state.set(SocketState::DOWN);
		}
		else
		{
			throw SocketState::state_exception("socket not in safe state");
		}
	}

	vsocket::SelectGuard::SelectGuard(SocketState &state, int &counter, ibrcommon::vsocket &sock)
	 : _state(state), _counter(counter), _sock(sock)
	{
		// set the current state to SELECT
		try {
			ibrcommon::MutexLock l(_state);
			_state.setwait(SocketState::SELECT, SocketState::DOWN);
			_counter++;
			IBRCOMMON_LOGGER_DEBUG_TAG("SelectGuard", 90) << "SelectGuard counter set to " << _counter << IBRCOMMON_LOGGER_ENDL;
		} catch (const SocketState::state_exception&) {
			throw vsocket_interrupt("select interrupted while waiting for IDLE socket");
		}
	}

	vsocket::SelectGuard::~SelectGuard()
	{
		// set the current state to SELECT
		try {
			ibrcommon::MutexLock l(_state);
			_counter--;
			IBRCOMMON_LOGGER_DEBUG_TAG("SelectGuard", 90) << "SelectGuard counter set to " << _counter << IBRCOMMON_LOGGER_ENDL;

			if (_counter == 0) {
				if (_state.get() == SocketState::SAFE_REQUEST) {
					_state.set(SocketState::SAFE);
				} else if (_state.get() == SocketState::DOWN_REQUEST) {
					_state.set(SocketState::PENDING_DOWN);
				} else {
					_state.set(SocketState::IDLE);
				}
			}
			else
			{
				// call interrupt once more if necessary
				if (_state.get() == SocketState::SAFE_REQUEST) {
					_sock.interrupt();
				} else if (_state.get() == SocketState::DOWN_REQUEST) {
					_sock.interrupt();
				}
			}
		} catch (const ibrcommon::Conditional::ConditionalAbortException&) {
			throw vsocket_interrupt("select interrupted while checking SAFE_REQUEST state");
		}
	}

	vsocket::vsocket()
	 : _state(SocketState::DOWN), _select_count(0)
	{
		_pipe.up();
	}

	vsocket::~vsocket()
	{
		try {
			_pipe.down();
		} catch (const socket_exception &ex) {
		}
	}

	void vsocket::add(basesocket *socket)
	{
		SafeLock l(_state, *this);
		_sockets.insert(socket);
	}

	void vsocket::add(basesocket *socket, const vinterface &iface)
	{
		SafeLock l(_state, *this);
		_sockets.insert(socket);
		_socket_map[iface].insert(socket);
	}

	void vsocket::remove(basesocket *socket)
	{
		SafeLock l(_state, *this);
		_sockets.erase(socket);

		// search for the same socket in the map
		for (std::map<vinterface, socketset>::iterator iter = _socket_map.begin(); iter != _socket_map.end(); ++iter)
		{
			socketset &set = (*iter).second;
			set.erase(socket);
		}
	}

	size_t vsocket::size() const
	{
		return _sockets.size();
	}

	void vsocket::clear()
	{
		SafeLock l(_state, *this);
		_sockets.clear();
		_socket_map.clear();
	}

	void vsocket::destroy()
	{
		down();

		for (socketset::iterator iter = _sockets.begin(); iter != _sockets.end(); ++iter)
		{
			basesocket *sock = (*iter);
			delete sock;
		}
		_sockets.clear();
		_socket_map.clear();
	}

	socketset vsocket::getAll() const
	{
		return _sockets;
	}

	socketset vsocket::get(const vinterface &iface) const
	{
		std::map<vinterface, socketset>::const_iterator iter = _socket_map.find(iface);

		if (iter == _socket_map.end()) {
			socketset empty;
			return empty;
		}

		return (*iter).second;
	}

	void vsocket::up() noexcept (false)
	{
		{
			ibrcommon::MutexLock l(_state);
			// wait until we can get into PENDING_UP state
			_state.setwait(SocketState::PENDING_UP, SocketState::IDLE);
		}
		
		for (socketset::iterator iter = _sockets.begin(); iter != _sockets.end(); ++iter) {
			try {
				if (!(*iter)->ready()) (*iter)->up();
			} catch (const socket_exception&) {
				// rewind all previously up'ped sockets
				for (socketset::iterator riter = _sockets.begin(); riter != iter; ++riter) {
					(*riter)->down();
				}

				ibrcommon::MutexLock l(_state);
				_state.set(SocketState::DOWN);
				throw;
			}
		}

		// set state to IDLE
		ibrcommon::MutexLock l(_state);
		_state.set(SocketState::IDLE);
	}

	void vsocket::down() noexcept
	{
		try {
			ibrcommon::MutexLock l(_state);
			if (_state.get() == SocketState::DOWN) {
				return;
			}
			else if (_state.get() == SocketState::PENDING_DOWN || _state.get() == SocketState::DOWN_REQUEST) {
				// prevent double down
				_state.wait(SocketState::DOWN);
				return;
			}
			else if (_state.get() == SocketState::SELECT) {
				_state.setwait(SocketState::DOWN_REQUEST, SocketState::DOWN);
				interrupt();
			} else {
				// enter PENDING_DOWN state
				_state.setwait(SocketState::PENDING_DOWN, SocketState::DOWN);
			}
		} catch (SocketState::state_exception&) {
			return;
		}

		{
			// shut-down all the sockets
			ibrcommon::MutexLock l(_socket_lock);
			for (socketset::iterator iter = _sockets.begin(); iter != _sockets.end(); ++iter) {
				try {
					if ((*iter)->ready()) (*iter)->down();
				} catch (const socket_exception&) { }
			}
		}

		ibrcommon::MutexLock sl(_state);
		_state.setwait(SocketState::DOWN);
	}

	void vsocket::interrupt()
	{
		_pipe.write("i", 1);
	}

	void vsocket::select(socketset *readset, socketset *writeset, socketset *errorset, struct timeval *tv) noexcept (false)
	{
		fd_set fds_read;
		fd_set fds_write;
		fd_set fds_error;

		int high_fd = 0;

		while (true)
		{
			SelectGuard guard(_state, _select_count, *this);

			FD_ZERO(&fds_read);
			FD_ZERO(&fds_write);
			FD_ZERO(&fds_error);

			// add the self-pipe-trick interrupt fd
			FD_SET(_pipe.fd(), &fds_read);
			high_fd = _pipe.fd();

			{
				ibrcommon::MutexLock l(_socket_lock);
				for (socketset::iterator iter = _sockets.begin();
						iter != _sockets.end(); ++iter)
				{
					basesocket &sock = (**iter);
					if (!sock.ready()) continue;

					if (readset != NULL) {
						FD_SET(sock.fd(), &fds_read);
						if (high_fd < sock.fd()) high_fd = sock.fd();
					}

					if (writeset != NULL) {
						FD_SET(sock.fd(), &fds_write);
						if (high_fd < sock.fd()) high_fd = sock.fd();
					}

					if (errorset != NULL) {
						FD_SET(sock.fd(), &fds_error);
						if (high_fd < sock.fd()) high_fd = sock.fd();
					}
				}
			}

			// call the linux-like select with given timeout
			int res = __compat_select(high_fd + 1, &fds_read, &fds_write, &fds_error, tv);

#ifdef __WIN32__
			int errcode = WSAGetLastError();
#else
			int errcode = errno;
#endif

			if (res < 0) {
				if (errcode == EINTR) {
					// signal has been caught - handle it as interruption
					continue;
				}
				else if (errcode == 0) {
					throw vsocket_interrupt("select call has been interrupted");
				}
				else if (errcode == EBADF) {
					throw socket_error(ERROR_CLOSED, "socket was closed");
				}
				throw socket_raw_error(errcode, "unknown select error");
			}


			if (res == 0)
				throw vsocket_timeout("select timeout");

			if (FD_ISSET(_pipe.fd(), &fds_read))
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("vsocket::select", 90) << "unblocked by self-pipe-trick" << IBRCOMMON_LOGGER_ENDL;

				// this was an interrupt with the self-pipe-trick
				ibrcommon::MutexLock l(_socket_lock);
				char buf[2];
				_pipe.read(buf, 2);

				// start over with the select call
				continue;
			}

			ibrcommon::MutexLock l(_socket_lock);
			for (socketset::iterator iter = _sockets.begin();
					iter != _sockets.end(); ++iter)
			{
				basesocket *sock = (*iter);

				if (readset != NULL) {
					if (FD_ISSET(sock->fd(), &fds_read))
					{
						readset->insert(sock);
					}
				}

				if (writeset != NULL) {
					if (FD_ISSET(sock->fd(), &fds_write))
					{
						writeset->insert(sock);
					}
				}

				if (errorset != NULL) {
					if (FD_ISSET(sock->fd(), &fds_error))
					{
						errorset->insert(sock);
					}
				}
			}

			break;
		}
	}
}
