/*
 * socket.cpp
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
#include "ibrcommon/net/socket.h"
#include "ibrcommon/net/vsocket.h"
#include "ibrcommon/Logger.h"

#ifdef __WIN32__
#include <windows.h>
#include <ws2tcpip.h>
#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0
#endif
#else
#include <arpa/inet.h>
#include <sys/select.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#include <sstream>

#include <cassert>

#ifndef HAVE_BZERO
#define bzero(s,n) (memset((s), '\0', (n)), (void) 0)
#endif

#ifdef __WIN32__
#define EINPROGRESS WSAEINPROGRESS
#define ECONNRESET WSAECONNRESET
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#define ENOBUFS WSAENOBUFS
#define EPROTONOSUPPORT WSAEPROTONOSUPPORT
#define ELOOP WSAELOOP
#endif

namespace ibrcommon
{
#ifdef __WIN32__
	/**
	 * wrapper to translate win32 method signature into linux/posix signature
	 */
	int __compat_setsockopt(int __fd, int __level, int __optname, __const void *__optval, socklen_t __optlen)
	{
		return ::setsockopt(__fd, __level, __optname, (char*)__optval, __optlen);
	}

	int __init_sockets()
	{
		static bool initialized = false;
		if (initialized) return 0;
		WSADATA wsa;
		return WSAStartup(MAKEWORD(2,2),&wsa);
	}

#define __close closesocket
#define __errno WSAGetLastError()
#else
#define __compat_setsockopt ::setsockopt
#define __init_sockets void
#define __close ::close
#define __errno errno
#endif

	int basesocket::DEFAULT_SOCKET_FAMILY = AF_INET6;
	int basesocket::DEFAULT_SOCKET_FAMILY_ALTERNATIVE = AF_INET;

	void initialize_socket() {
		__init_sockets();
	}

	basesocket::basesocket()
	 : _state(SOCKET_DOWN), _fd(-1), _family(PF_UNSPEC)
	{
		__init_sockets();
	}

	basesocket::basesocket(int fd)
	 : _state(SOCKET_UNMANAGED), _fd(fd), _family(PF_UNSPEC)
	{
		__init_sockets();
	}

	basesocket::~basesocket()
	{
#ifdef __DEVELOPMENT_ASSERTIONS__
		assert((_state == SOCKET_DOWN) || (_state == SOCKET_DESTROYED));
		assert(_fd == -1);
#endif
	}

	int basesocket::fd() const throw (socket_exception)
	{
		if ((_state == SOCKET_DOWN) || (_state == SOCKET_DESTROYED) || (_fd == -1)) throw socket_exception("fd not available");
		return _fd;
	}

	int basesocket::release() throw (socket_exception)
	{
		if ((_state == SOCKET_DOWN) || (_state == SOCKET_DESTROYED)) throw socket_exception("fd not available");
		int fd = _fd;
		_fd = -1;

		if (_state == SOCKET_UP)
			_state = SOCKET_DOWN;
		else
			_state = SOCKET_DESTROYED;

		return fd;
	}

	void basesocket::close() throw (socket_exception)
	{
		int ret = __close(this->fd());
		if (ret == -1)
			throw socket_exception("close error");

		_fd = -1;

		if (_state == SOCKET_UNMANAGED)
			_state = SOCKET_DESTROYED;
		else
			_state = SOCKET_DOWN;
	}

	void basesocket::shutdown(int how) throw (socket_exception)
	{
		int ret = ::shutdown(this->fd(), how);
		if (ret == -1)
			throw socket_exception("shutdown error");

		if (_state == SOCKET_UNMANAGED)
			_state = SOCKET_DESTROYED;
		else
			_state = SOCKET_DOWN;
	}

	bool basesocket::ready() const
	{
		return ((_state == SOCKET_UP) || (_state == SOCKET_UNMANAGED));
	}

	void basesocket::set_blocking_mode(bool val, int fd) const throw (socket_exception)
	{
#ifdef __WIN32__
		// set blocking mode - the win32 way
		unsigned long block_mode = (val) ? 1 : 0;
		ioctlsocket((fd == -1) ? _fd : fd, FIONBIO, &block_mode);
#else
		int opts;
		opts = fcntl((fd == -1) ? _fd : fd, F_GETFL);
		if (opts < 0) {
			throw socket_exception("cannot set non-blocking");
		}

		if (val)
			opts &= ~(O_NONBLOCK);
		else
			opts |= O_NONBLOCK;

		if (fcntl((fd == -1) ? _fd : fd, F_SETFL, opts) < 0) {
			throw socket_exception("cannot set non-blocking");
		}
#endif
	}

	void basesocket::set_keepalive(bool val, int fd) const throw (socket_exception)
	{
		/* Set the option active */
		int optval = (val ? 1 : 0);
		if (__compat_setsockopt((fd == -1) ? _fd : fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
			throw ibrcommon::socket_exception("can not activate keepalives");
		}
	}

	void basesocket::set_linger(bool val, int l, int fd) const throw (socket_exception)
	{
		// set linger option to the socket
		struct linger linger;

		linger.l_onoff = (val ? 1 : 0);
		linger.l_linger = l;
		if (__compat_setsockopt((fd == -1) ? _fd : fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) < 0) {
			throw ibrcommon::socket_exception("can not set linger option");
		}
	}

	void basesocket::set_reuseaddr(bool val, int fd) const throw (socket_exception)
	{
		int on = (val ? 1: 0);
		if (__compat_setsockopt((fd == -1) ? _fd : fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
		{
			throw socket_exception("setsockopt(SO_REUSEADDR) failed");
		}
	}

	void basesocket::set_nodelay(bool val, int fd) const throw (socket_exception)
	{
		int set = (val ? 1 : 0);
		if (__compat_setsockopt((fd == -1) ? _fd : fd, IPPROTO_TCP, TCP_NODELAY, &set, sizeof(set)) < 0) {
			throw socket_exception("set no delay option failed");
		}
	}

	sa_family_t basesocket::get_family() const throw (socket_exception)
	{
		return _family;
	}

	sa_family_t basesocket::get_family(int fd) throw (socket_exception)
	{
		struct sockaddr_storage bound_addr;
		socklen_t bound_len = sizeof(bound_addr);
		::memset(&bound_addr, 0, bound_len);

		// get the socket family
		int ret = ::getsockname(fd, (struct sockaddr*)&bound_addr, &bound_len);
		if (ret == -1) {
			throw socket_exception("socket is not bound");
		}

		return bound_addr.ss_family;
	}

	bool basesocket::hasSupport(const sa_family_t family, const int type, const int protocol) throw ()
	{
		int fd = 0;
		if ((fd = ::socket(family, type, protocol)) < 0) {
			return false;
		}
		__close(fd);
		return true;
	}

	void basesocket::init_socket(const vaddress &addr, int type, int protocol) throw (socket_exception)
	{
		try {
			_family = addr.family();
			if ((_fd = ::socket(_family, type, protocol)) < 0) {
				throw socket_raw_error(__errno, "cannot create socket");
			}
		} catch (const vaddress::address_exception&) {
			// if no address is set use DEFAULT_SOCKET_FAMILY
			if ((_fd = ::socket(DEFAULT_SOCKET_FAMILY, type, protocol)) > -1) {
				_family = static_cast<sa_family_t>(DEFAULT_SOCKET_FAMILY);
			}
			// if that fails switch to the alternative SOCKET_FAMILY
			else if ((_fd = ::socket(DEFAULT_SOCKET_FAMILY_ALTERNATIVE, type, protocol)) > -1) {
				_family = static_cast<sa_family_t>(DEFAULT_SOCKET_FAMILY_ALTERNATIVE);

				// set the alternative socket family as default
				DEFAULT_SOCKET_FAMILY = DEFAULT_SOCKET_FAMILY_ALTERNATIVE;
			}
			else
			{
				throw socket_raw_error(__errno, "cannot create socket");
			}
		}
	}

	void basesocket::init_socket(int domain, int type, int protocol) throw (socket_exception)
	{
		_family = static_cast<sa_family_t>(domain);
		if ((_fd = ::socket(domain, type, protocol)) < 0) {
			throw socket_raw_error(__errno, "cannot create socket");
		}
	}

	void basesocket::bind(int fd, struct sockaddr *addr, socklen_t len) throw (socket_exception)
	{
		int ret = ::bind(fd, addr, len);

		if (ret < 0) {
			// error
			int bind_err = __errno;

			char addr_str[256];
			char serv_str[256];
			::getnameinfo(addr, len, (char*)&addr_str, 256, (char*)&serv_str, 256, NI_NUMERICHOST | NI_NUMERICSERV);
			std::stringstream ss;
			vaddress vaddr(addr_str, serv_str, addr->sa_family);
			ss << "with address " << vaddr.toString();

			check_bind_error(bind_err, ss.str());
		}
	}

        int basesocket::get_port()
	{
	  struct   sockaddr_in sin;
	  socklen_t addrlen = sizeof(sin);
	  memset(&sin, 0, addrlen);
	  getsockname(_fd,(struct sockaddr*)&sin,&addrlen);  
	  int port = ntohs(sin.sin_port);
	  
	  return port;
	}

	clientsocket::clientsocket()
	{
	}

	clientsocket::clientsocket(int fd)
	 : basesocket(fd)
	{
	}

	clientsocket::~clientsocket()
	{
		try {
			down();
		} catch (const socket_exception&) { }
	}

	void clientsocket::up() throw (socket_exception)
	{
		if (_state != SOCKET_DOWN)
			throw socket_exception("socket is already up");

		_state = SOCKET_UP;
	}

	void clientsocket::down() throw (socket_exception)
	{
		if ((_state == SOCKET_DOWN) || (_state == SOCKET_DESTROYED))
			throw socket_exception("socket is not up");

		this->close();
	}

	ssize_t clientsocket::send(const char *data, size_t len, int flags) throw (socket_exception)
	{
		ssize_t ret = ::send(this->fd(), data, len, flags);
		if (ret == -1) {
			switch (__errno)
			{
			case EPIPE:
				// connection has been reset
				throw socket_error(ERROR_EPIPE, "connection has been reset");

			case ECONNRESET:
				// Connection reset by peer
				throw socket_error(ERROR_RESET, "Connection reset by peer");

			case EAGAIN:
				// sent failed but we should retry again
				throw socket_error(ERROR_AGAIN, "sent failed but we should retry again");

			default:
				throw socket_error(ERROR_WRITE, "send error");
			}
		}
		return ret;
	}

	ssize_t clientsocket::recv(char *data, size_t len, int flags) throw (socket_exception)
	{
		ssize_t ret = ::recv(this->fd(), data, len, flags);
		if (ret == -1) {
			switch (__errno)
			{
			case EPIPE:
				// connection has been reset
				throw socket_error(ERROR_EPIPE, "connection has been reset");

			default:
				throw socket_error(ERROR_READ, "read error");
			}
		}

		return ret;
	}

	void clientsocket::set(CLIENT_OPTION opt, bool val) throw (socket_exception)
	{
		switch (opt) {
		case NO_DELAY:
			set_nodelay(val);
			break;
		case BLOCKING:
			set_blocking_mode(val);
			break;
		default:
			break;
		}
	}

	serversocket::serversocket()
	{
	}

	serversocket::serversocket(int fd)
	 : basesocket(fd)
	{
	}

	serversocket::~serversocket()
	{
	}

	void serversocket::listen(int connections) throw (socket_exception)
	{
		int ret = ::listen(_fd, connections);
		if (ret == -1)
			throw socket_exception("listen failed");
	}

	int serversocket::_accept_fd(ibrcommon::vaddress &addr) throw (socket_exception)
	{
		struct sockaddr_storage cliaddr;
		socklen_t len = sizeof(cliaddr);
		::memset(&cliaddr, 0, len);

		int new_fd = ::accept(this->fd(), (struct sockaddr *) &cliaddr, &len);

		if (new_fd <= 0) {
			throw socket_exception("accept failed");
		}

		// set source to addr
		char address[256];
		char service[256];
		if (::getnameinfo((struct sockaddr *) &cliaddr, len, address, sizeof address, service, sizeof service, NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
			addr = ibrcommon::vaddress(std::string(address), std::string(service), cliaddr.ss_family);
		}

		return new_fd;
	}

	void serversocket::set(SERVER_OPTION opt, bool val)
	{
		switch (opt) {
		case BLOCKING:
			set_blocking_mode(val);
			break;
		}
	}

	datagramsocket::datagramsocket()
	{
	}

	datagramsocket::datagramsocket(int fd)
	 : basesocket(fd)
	{
	}

	datagramsocket::~datagramsocket()
	{
	}

	ssize_t datagramsocket::recvfrom(char *buf, size_t buflen, int flags, ibrcommon::vaddress &addr) throw (socket_exception)
	{
		struct sockaddr_storage clientAddress;
		socklen_t clientAddressLength = sizeof(clientAddress);
		::memset(&clientAddress, 0, clientAddressLength);

		// data waiting
		ssize_t ret = ::recvfrom(this->fd(), buf, buflen, flags, (struct sockaddr *) &clientAddress, &clientAddressLength);

		if (ret == -1) {
			throw socket_exception("recvfrom error");
		}

		char address[256];
		char service[256];
		if (::getnameinfo((struct sockaddr *) &clientAddress, clientAddressLength, address, sizeof address, service, sizeof service, NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
			addr = ibrcommon::vaddress(std::string(address), std::string(service), clientAddress.ss_family);
		}

		return ret;
	}

	void datagramsocket::sendto(const char *buf, size_t buflen, int flags, const ibrcommon::vaddress &addr) throw (socket_exception)
	{
		int ret = 0;
		struct addrinfo hints, *res;
		memset(&hints, 0, sizeof hints);

		hints.ai_family = _family;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_ADDRCONFIG;

		const char *address = NULL;
		const char *service = NULL;

		try {
			address = addr.address().c_str();
		} catch (const vaddress::address_not_set&) {
			throw socket_exception("need at least an address to send to");
		};

		try {
			service = addr.service().c_str();
		} catch (const vaddress::address_not_set&) { };

		if ((ret = ::getaddrinfo(address, service, &hints, &res)) != 0)
		{
			throw socket_exception("getaddrinfo(): " + std::string(gai_strerror(ret)));
		}

		ssize_t len = 0;
		len = ::sendto(this->fd(), buf, buflen, flags, res->ai_addr, res->ai_addrlen);

		// free the addrinfo struct
		freeaddrinfo(res);

		if (len == -1) {
			throw socket_raw_error(__errno);
		}
	}

	filesocket::filesocket(int fd)
	 : clientsocket(fd)
	{
	}

	filesocket::filesocket(const ibrcommon::File &file)
	 : _filename(file)
	{
	}

	filesocket::~filesocket()
	{
		try {
			down();
		} catch (const socket_exception&) { }
	}

	void filesocket::up() throw (socket_exception)
	{
		if (_state != SOCKET_DOWN)
			throw socket_exception("socket is already up");

#ifdef __WIN32__
		throw socket_exception("socket type not supported");
#else
		size_t len = 0;
		struct sockaddr_un saun;

		/*
		 * Get a socket to work with.  This socket will
		 * be in the UNIX domain, and will be a
		 * stream socket.
		 */
		init_socket(AF_UNIX, SOCK_STREAM, 0);

		/*
		 * Create the address we will be connecting to.
		 */
		saun.sun_family = AF_UNIX;
		::strcpy(saun.sun_path, _filename.getPath().c_str());

		/*
		 * Try to connect to the address.  For this to
		 * succeed, the server must already have bound
		 * this address, and must have issued a listen()
		 * request.
		 *
		 * The third argument indicates the "length" of
		 * the structure, not just the length of the
		 * socket name.
		 */
		len = sizeof(saun.sun_family) + strlen(saun.sun_path);

		if (::connect(_fd, (struct sockaddr *)&saun, static_cast<socklen_t>(len)) < 0) {
			this->close();
			throw socket_exception("Could not connect to the named socket.");
		}
#endif

		_state = SOCKET_UP;
	}

	void filesocket::down() throw (socket_exception)
	{
		if ((_state == SOCKET_DOWN) || (_state == SOCKET_DESTROYED))
			throw socket_exception("socket is not up");

		this->close();
	}

	fileserversocket::fileserversocket(const ibrcommon::File &file, int listen)
	 : _filename(file), _listen(listen)
	{
	}

	fileserversocket::~fileserversocket()
	{
		try {
			down();
		} catch (const socket_exception&) { }
	}

	void fileserversocket::up() throw (socket_exception)
	{
		if (_state != SOCKET_DOWN)
			throw socket_exception("socket is already up");

		/*
		 * Get a socket to work with.  This socket will
		 * be in the UNIX domain, and will be a
		 * stream socket.
		 */
		init_socket(AF_UNIX, SOCK_STREAM, 0);

		try {
			this->bind(_filename);
			this->listen(_listen);
		} catch (const socket_exception&) {
			// clean-up socket
			__close(_fd);
			_fd = -1;
			throw;
		};

		_state = SOCKET_UP;
	}

	void fileserversocket::down() throw (socket_exception)
	{
		if ((_state == SOCKET_DOWN) || (_state == SOCKET_DESTROYED))
			throw socket_exception("socket is not up");

		this->close();
	}

	clientsocket* fileserversocket::accept(ibrcommon::vaddress &addr) throw (socket_exception)
	{
		return new filesocket(_accept_fd(addr));
	}

	void fileserversocket::bind(const File &file) throw (socket_exception)
	{
#ifndef __WIN32__
		// remove old sockets
		unlink(file.getPath().c_str());

		struct sockaddr_un address;
		size_t address_length;

		address.sun_family = AF_UNIX;
		strcpy(address.sun_path, file.getPath().c_str());
		address_length = sizeof(address.sun_family) + strlen(address.sun_path);

		// bind to the socket
		basesocket::bind(_fd, (struct sockaddr *) &address, static_cast<socklen_t>(address_length));
#endif
	}

	tcpserversocket::tcpserversocket(const int port, int listen)
	 : _address(port), _listen(listen)
	{
	}

	tcpserversocket::tcpserversocket(const ibrcommon::vaddress &address, int listen)
	 : _address(address), _listen(listen)
	{
	}

	tcpserversocket::~tcpserversocket()
	{
		try {
			down();
		} catch (const socket_exception&) { }
	}

	void tcpserversocket::up() throw (socket_exception)
	{
		if (_state != SOCKET_DOWN)
			throw socket_exception("socket is already up");

		init_socket(_address, SOCK_STREAM, 0);

		// enable reuse to avoid delay on process restart
		this->set_reuseaddr(true);

		try {
			// try to bind on port and/or address
			this->bind(_address);
			this->listen(_listen);
		} catch (const socket_exception&) {
			// clean-up socket
			__close(_fd);
			_fd = -1;
			throw;
		};

		_state = SOCKET_UP;
	}

	void tcpserversocket::down() throw (socket_exception)
	{
		if ((_state == SOCKET_DOWN) || (_state == SOCKET_DESTROYED))
			throw socket_exception("socket is not up");

		this->close();
	}

	void tcpserversocket::bind(const vaddress &addr) throw (socket_exception)
	{
		struct addrinfo hints, *res;
		memset(&hints, 0, sizeof hints);

		hints.ai_family = _family;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;

		std::string address;
		std::string service;

		if (addr.isAny()) {
			hints.ai_flags |= AI_PASSIVE;
		} else if (addr.isLocal()) {
		} else {
			hints.ai_flags |= AI_PASSIVE;
			try {
				address = addr.address();
			} catch (const vaddress::address_not_set&) { };
		}

		try {
			service = addr.service();
		} catch (const vaddress::address_not_set&) { };

		if (0 != ::getaddrinfo(address.length() > 0 ? address.c_str() : NULL, service.length() > 0 ? service.c_str() : NULL, &hints, &res))
			throw socket_exception("failed to getaddrinfo with address: " + addr.toString());

		try {
			basesocket::bind(_fd, res->ai_addr, res->ai_addrlen);
			freeaddrinfo(res);
		} catch (const socket_exception&) {
			freeaddrinfo(res);
			throw;
		}
	}

	clientsocket* tcpserversocket::accept(ibrcommon::vaddress &addr) throw (socket_exception)
	{
		return new tcpsocket(_accept_fd(addr));
	}

	const vaddress& tcpserversocket::get_address() const
	{
		return _address;
	}

	tcpsocket::tcpsocket(int fd)
	 : clientsocket(fd)
	{
		timerclear(&_timeout);
	}

	tcpsocket::tcpsocket(const ibrcommon::vaddress &destination, const timeval *timeout)
	 : _address(destination)
	{
		if (timeout == NULL) {
			timerclear(&_timeout);
		} else {
			::memcpy(&_timeout, timeout, sizeof _timeout);
		}
	}

	tcpsocket::~tcpsocket()
	{
		try {
			down();
		} catch (const socket_exception&) { }
	}

	void tcpsocket::up() throw (socket_exception)
	{
		if (_state != SOCKET_DOWN)
			throw socket_exception("socket is already up");

		struct addrinfo hints;
		struct addrinfo *walk;
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;

		struct addrinfo *res = NULL;
		int ret = 0;

		const char *address = NULL;
		const char *service = NULL;

		try {
			address = _address.address().c_str();
		} catch (const vaddress::address_not_set&) {
			throw socket_exception("need at least an address to connect to");
		};

		try {
			service = _address.service().c_str();
		} catch (const vaddress::service_not_set&) { };

		if ((ret = ::getaddrinfo(address, service, &hints, &res)) != 0)
		{
			throw socket_exception("getaddrinfo(): " + std::string(gai_strerror(ret)));
		}

		if (res == NULL)
		{
			throw socket_exception("Could not connect to the server.");
		}

		// create a vsocket for concurrent connection setup
		ibrcommon::vsocket probesocket;

		try {
			// walk through all the returned addresses and try to connect to all of them
			for (walk = res; walk != NULL; walk = walk->ai_next) {
				// mark the socket as invalid first
				int fd = -1;

				// create a matching socket
				fd = ::socket(walk->ai_family, walk->ai_socktype, walk->ai_protocol);

				// if the socket is invalid we proceed with the next address
				if (fd < 0) {
					/* Hier kann eine Fehlermeldung hin, z.B. mit warn() */

					if ((walk->ai_next ==  NULL) && (probesocket.size() == 0))
					{
						throw socket_exception("Could not create a socket.");
					}
					continue;
				}

				// set the socket to non-blocking
				this->set_blocking_mode(false, fd);

				// connect to the current address using the created socket
				if (::connect(fd, walk->ai_addr, walk->ai_addrlen) != 0) {
					if (__errno != EINPROGRESS) {
						// the connect failed, so we close the socket immediately
						__close(fd);

						/* Hier kann eine Fehlermeldung hin, z.B. mit warn() */
						if ((walk->ai_next == NULL) && (probesocket.size() == 0))
						{
							throw socket_raw_error(__errno);
						}
						continue;
					}
				}

				// add the current socket to the probe-socket for later select-call
				probesocket.add( new tcpsocket(fd) );
			}

			// bring probesocket into UP state
			probesocket.up();

			// create a probe set
			socketset probeset;

			timeval timeout_value;
			::memcpy(&timeout_value, &_timeout, sizeof timeout_value);

			bool fastest_found = false;

			while (!fastest_found) {
				// clear the probe-set
				probeset.clear();

				if (timerisset(&_timeout)) {
					// check timeout value
					if (!timerisset(&timeout_value)) {
						// timeout reached abort this
						break;
					}

					// probe for the first open socket using a timer value
					probesocket.select(NULL, &probeset, NULL, &timeout_value);
				} else {
					// probe for the first open socket without any timer
					probesocket.select(NULL, &probeset, NULL, NULL);
				}

				// error checking for all returned sockets
				for (socketset::iterator iter = probeset.begin(); iter != probeset.end(); ++iter) {
					basesocket *current = (*iter);
					int err = 0;
					socklen_t len = sizeof(err);
#ifdef __WIN32__
					::getsockopt(current->fd(), SOL_SOCKET, SO_ERROR, (char*)&err, &len);
#else
					::getsockopt(current->fd(), SOL_SOCKET, SO_ERROR, &err, &len);
#endif

					switch (err) {
					case 0:
						// we got a winner!
						fastest_found = true;

						// assign the fasted fd
						_fd = current->release();

						// switch back to standard blocking mode
						this->set_blocking_mode(true);
						break;

					case EINPROGRESS:
						// wait another round
						break;

					default:
						// error, remove the socket out of the probeset
						probesocket.remove(current);
						current->close();
						delete current;

						// if this was the last socket then abort with an exception
						if (probesocket.size() == 0) {
							throw socket_raw_error(err);
						}
						break;
					}

					if (fastest_found) break;
				}
			}

			if (fastest_found == false) {
				throw socket_exception("connection setup timed out");
			}

			// free the address
			freeaddrinfo(res);

			// set the current state to UP
			_state = SOCKET_UP;
		} catch (const std::exception&) {
			freeaddrinfo(res);
			probesocket.destroy();
			throw;
		}

		// bring all other sockets down and clean-up
		probesocket.destroy();
	}

	void tcpsocket::down() throw (socket_exception)
	{
		if ((_state == SOCKET_DOWN) || (_state == SOCKET_DESTROYED))
			throw socket_exception("socket is not up");

		this->close();
	}

	udpsocket::udpsocket()
	{
	}

	udpsocket::udpsocket(const vaddress &address)
	 : _address(address)
	{
	}

	udpsocket::~udpsocket()
	{
		try {
			down();
		} catch (const socket_exception&) { }
	}

	const vaddress& udpsocket::get_address() const
	{
		return _address;
	}

	void udpsocket::up() throw (socket_exception)
	{
		if (_state != SOCKET_DOWN)
			throw socket_exception("socket is already up");

		init_socket(_address, SOCK_DGRAM, 0);

		try {
			// test if the service is defined
			_address.service();

			// service is defined, enable reuseaddr option
			this->set_reuseaddr(true);
		} catch (const vaddress::address_exception&) { }

		try {
			// try to bind on port and/or address
			this->bind(_address);
		} catch (const socket_exception&) {
			// clean-up socket
			__close(_fd);
			_fd = -1;
			throw;
		};

		_state = SOCKET_UP;
	}

	void udpsocket::down() throw (socket_exception)
	{
		if ((_state == SOCKET_DOWN) || (_state == SOCKET_DESTROYED))
			throw socket_exception("socket is not up");

		this->close();
	}

	void udpsocket::bind(const vaddress &addr) throw (socket_exception)
	{
		struct addrinfo hints, *res;
		memset(&hints, 0, sizeof hints);

		hints.ai_family = _family;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = 0;

		std::string address;
		std::string service;

		if (addr.isAny()) {
			hints.ai_flags |= AI_PASSIVE;
		} else if (addr.isLocal()) {
		} else {
			hints.ai_flags |= AI_PASSIVE;
			try {
				address = addr.address();
			} catch (const vaddress::address_not_set&) { };
		}

		try {
			service = addr.service();
		} catch (const vaddress::service_not_set&) { };

		if (0 != ::getaddrinfo(address.length() > 0 ? address.c_str() : NULL, service.length() > 0 ? service.c_str() : NULL, &hints, &res))
			throw socket_exception("failed to getaddrinfo with address: " + addr.toString());

		try {
			basesocket::bind(_fd, res->ai_addr, res->ai_addrlen);
			freeaddrinfo(res);
		} catch (const socket_exception&) {
			freeaddrinfo(res);
			throw;
		}
	}

	multicastsocket::multicastsocket(const vaddress &address)
	 : udpsocket(address)
	{
	}

	multicastsocket::~multicastsocket()
	{
	}

	void multicastsocket::up() throw (socket_exception)
	{
		udpsocket::up();

		try {
			switch (get_family()) {
			case AF_INET: {
#ifdef __linux__
				int val = 1;
				if ( __compat_setsockopt(_fd, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)&val, sizeof(val)) < 0 )
				{
					throw socket_exception("setsockopt(IP_MULTICAST_LOOP)");
				}

				unsigned char ttl = 7; // Multicast TTL
				if ( __compat_setsockopt(_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0 )
				{
					throw socket_exception("setsockopt(IP_MULTICAST_TTL)");
				}
#endif
//				unsigned char ittl = 255; // IP TTL
//				if ( __compat_setsockopt(this->fd(), IPPROTO_IP, IP_TTL, &ittl, sizeof(ittl)) < 0 )
//				{
//					throw socket_exception("setsockopt(IP_TTL)");
//				}
				break;
			}

			case AF_INET6: {
#ifdef __linux__
				int val = 1;
				if ( __compat_setsockopt(_fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (const char *)&val, sizeof(val)) < 0 )
				{
					throw socket_exception("setsockopt(IPV6_MULTICAST_LOOP)");
				}

//				unsigned char ttl = 255; // Multicast TTL
//				if ( __compat_setsockopt(this_fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl)) < 0 )
//				{
//					throw socket_exception("setsockopt(IPV6_MULTICAST_HOPS)");
//				}
#endif

//				unsigned char ittl = 255; // IP TTL
//				if ( __compat_setsockopt(_fd, IPPROTO_IPV6, IPV6_HOPLIMIT, &ittl, sizeof(ittl)) < 0 )
//				{
//					throw socket_exception("setsockopt(IPV6_HOPLIMIT)");
//				}
				break;
			}
			}
		} catch (const socket_exception&) {
			udpsocket::down();
			throw;
		}
	}

	void multicastsocket::down() throw (socket_exception)
	{
		switch (get_family()) {
		case AF_INET: {
#ifdef __linux__
			int val = 0;
			if ( __compat_setsockopt(_fd, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)&val, sizeof(val)) < 0 )
			{
				throw socket_exception("setsockopt(IP_MULTICAST_LOOP)");
			}
#endif
			break;
		}

		case AF_INET6: {
#ifdef __linux__
			int val = 0;
			if ( __compat_setsockopt(_fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (const char *)&val, sizeof(val)) < 0 )
			{
				throw socket_exception("setsockopt(IPV6_MULTICAST_LOOP)");
			}
#endif
			break;
		}
		}

		udpsocket::down();
	}

	void multicastsocket::join(const vaddress &group, const vinterface &iface) throw (socket_exception)
	{
#ifndef MCAST_JOIN_GROUP
		if (group.family() == AF_INET6) {
			mcast_op(IPV6_JOIN_GROUP, group, iface);
		} else {
			mcast_op(IP_ADD_MEMBERSHIP, group, iface);
		}
#else
		mcast_op(MCAST_JOIN_GROUP, group, iface);
#endif
	}

	void multicastsocket::leave(const vaddress &group, const vinterface &iface) throw (socket_exception)
	{
#ifndef MCAST_LEAVE_GROUP
		if (group.family() == AF_INET6) {
			mcast_op(IPV6_LEAVE_GROUP, group, iface);
		} else {
			mcast_op(IP_DROP_MEMBERSHIP, group, iface);
		}
#else
		mcast_op(MCAST_LEAVE_GROUP, group, iface);
#endif
	}

#ifndef MCAST_JOIN_GROUP
	void __copy_device_address(struct in_addr *inaddr, const vinterface &iface) {
		ssize_t ret = 0;
		struct addrinfo hints, *res;
		memset(&hints, 0, sizeof hints);

		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;

		// determine interface address
		struct sockaddr_storage iface_addr;
		::memset(&iface_addr, 0, sizeof(iface_addr));

		const std::list<vaddress> iface_addrs = iface.getAddresses();
		for (std::list<vaddress>::const_iterator iter = iface_addrs.begin(); iter != iface_addrs.end(); ++iter) {
			const vaddress &addr = (*iter);

			if ((ret = ::getaddrinfo(addr.address().c_str(), NULL, &hints, &res)) == 0) {
				// address found
				::memcpy(inaddr, &((struct sockaddr_in*)res->ai_addr)->sin_addr, sizeof(struct sockaddr_in));

				// free the addrinfo struct
				freeaddrinfo(res);

				break;
			}
		}
	}
#endif

	void multicastsocket::mcast_op(int optname, const vaddress &group, const vinterface &iface) throw (socket_exception)
	{
		struct sockaddr_storage mcast_addr;
		::memset(&mcast_addr, 0, sizeof(mcast_addr));

		int level = 0;
		int ret = 0;

		struct addrinfo hints, *res;
		memset(&hints, 0, sizeof hints);

		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = 0;

		if ((ret = ::getaddrinfo(group.address().c_str(), NULL, &hints, &res)) != 0) {
			throw socket_exception("getaddrinfo(): " + std::string(gai_strerror(ret)));
		}

		switch (res->ai_family) {
		case AF_INET:
			level = IPPROTO_IP;
			break;

		case AF_INET6:
			level = IPPROTO_IPV6;
			break;

		default:
			// free the addrinfo struct
			freeaddrinfo(res);

			throw socket_exception("address family not supported");
		}

#ifndef MCAST_JOIN_GROUP
		if (res->ai_family == AF_INET) {
			struct ip_mreq req;
			::memset(&req, 0, sizeof(req));

			// copy the address to the group request
			req.imr_multiaddr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;

			// free the addrinfo struct
			freeaddrinfo(res);

			// set the right interface
			__copy_device_address(&req.imr_interface, iface);

			if ( __compat_setsockopt(this->fd(), level, optname, &req, sizeof(req)) == -1 )
			{
				throw socket_raw_error(__errno, "setsockopt()");
			}
		} else {
			struct ipv6_mreq req;
			::memset(&req, 0, sizeof(req));

			// copy the address to the group request
			req.ipv6mr_multiaddr = ((struct sockaddr_in6*)res->ai_addr)->sin6_addr;

			// free the addrinfo struct
			freeaddrinfo(res);

			// set the right interface
			req.ipv6mr_interface = iface.getIndex();

			if ( __compat_setsockopt(this->fd(), level, optname, &req, sizeof(req)) == -1 )
			{
				throw socket_raw_error(__errno, "setsockopt()");
			}
		}
#else
		struct group_req req;
		::memset(&req, 0, sizeof(req));

		// copy the address to the group request
		::memcpy(&req.gr_group, res->ai_addr, res->ai_addrlen);

		// free the addrinfo struct
		freeaddrinfo(res);

		// set the right interface here
		req.gr_interface = iface.getIndex();

		if ( __compat_setsockopt(this->fd(), level, optname, &req, sizeof(req)) == -1 )
		{
			throw socket_raw_error(__errno, "setsockopt()");
		}
#endif

		// successful!
		IBRCOMMON_LOGGER_DEBUG_TAG("multicastsocket", 70) << "multicast operation (" << optname << ") successful with " << group.toString() << " on " << iface.toString() << IBRCOMMON_LOGGER_ENDL;
	}

	void basesocket::check_socket_error(const int err) const throw (socket_exception)
	{
		switch (err)
		{
		case EACCES:
			throw socket_exception("Permission to create a socket of the specified type and/or protocol is denied.");

		case EAFNOSUPPORT:
			throw socket_exception("The implementation does not support the specified address family.");

		case EINVAL:
			throw socket_exception("Unknown protocol, or protocol family not available.");

		case EMFILE:
			throw socket_exception("Process file table overflow.");

		case ENFILE:
			throw socket_exception("The system limit on the total number of open files has been reached.");

		case ENOBUFS:
		case ENOMEM:
			throw socket_exception("Insufficient memory is available. The socket cannot be created until sufficient resources are freed.");

		case EPROTONOSUPPORT:
			throw socket_exception("The protocol type or the specified protocol is not supported within this domain.");

		default:
			throw socket_exception("Cannot create a socket.");
		}
	}

	void basesocket::check_bind_error(const int err, const std::string &msg) const throw (socket_exception)
	{
		switch ( err )
		{
		case EBADF:
			throw socket_exception("sockfd is not a valid descriptor.");

		case EINVAL:
			throw socket_exception("The socket is already bound to an address.");

		case EROFS:
			throw socket_exception("The socket inode would reside on a read-only filesystem.");

		case EFAULT:
			throw socket_exception("Given address points outside the user's accessible address space.");

		case ENAMETOOLONG:
			throw socket_exception("Given address is too long.");

		case ENOENT:
			throw socket_exception("The file does not exist.");

		case ENOMEM:
			throw socket_exception("Insufficient kernel memory was available.");

		case ENOTDIR:
			throw socket_exception("A component of the path prefix is not a directory.");

		case EACCES:
			throw socket_exception("Search permission is denied on a component of the path prefix.");

		case ELOOP:
			throw socket_exception("Too many symbolic links were encountered in resolving the given address.");

		default:
			throw socket_exception("Cannot bind to socket " + msg);
		}
	}
}
