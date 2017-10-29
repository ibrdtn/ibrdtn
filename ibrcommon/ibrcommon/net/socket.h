/*
 * socket.h
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

#ifndef IBRCOMMON_SOCKET_H_
#define IBRCOMMON_SOCKET_H_

#include <ibrcommon/Exceptions.h>
#include <ibrcommon/data/File.h>
#include <ibrcommon/net/vaddress.h>
#include <ibrcommon/net/vinterface.h>
#include <sstream>
#include <string.h>
#include <sys/time.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

namespace ibrcommon {
	enum socket_error_code
	{
		ERROR_NONE = 0,
		ERROR_EPIPE = 1,
		ERROR_CLOSED = 2,
		ERROR_WRITE = 3,
		ERROR_READ = 4,
		ERROR_RESET = 5,
		ERROR_AGAIN = 6
	};

	class socket_exception : public Exception
	{
	public:
		socket_exception() : Exception()
		{};

		socket_exception(std::string error) : Exception(error)
		{};
	};

	class socket_error : public socket_exception
	{
	public:
		socket_error(socket_error_code code, std::string error) : socket_exception(error), _code(code)
		{};

		socket_error_code code() const { return _code; }

	private:
		socket_error_code _code;
	};

	class socket_raw_error : public socket_exception
	{
	public:
		socket_raw_error(int error, std::string description) : socket_exception(description), _errno(error)
		{
			std::stringstream ss;
			ss << error << ": " << socket_exception::what();
			__what = ss.str();
		};

		socket_raw_error(int error) : socket_exception(), _errno(error)
		{
			std::stringstream ss;
			ss << error << ": " << ::strerror(error);
			__what = ss.str();
		};

		virtual ~socket_raw_error() throw()
		{};

		virtual const char* what() const throw()
		{
			return __what.c_str();
		}

		int error() const { return _errno; }

	private:
		int _errno;
		std::string __what;
	};

	void initialize_socket();

	/**
	 * The basesocket is an interface for all kinds of sockets. The
	 * methods allows to bring the socket up or down.
	 */
	class basesocket {
	public:
		static int DEFAULT_SOCKET_FAMILY;
		static int DEFAULT_SOCKET_FAMILY_ALTERNATIVE;

		virtual ~basesocket() = 0;

		/**
		 * Create the file descriptor for this socket
		 * and bind to the interface if necessary.
		 * @throw socket_exception if the action has failed
		 */
		virtual void up() throw (socket_exception) = 0;

		/**
		 * Close and destroy the file descriptor of this
		 * socket assignment.
		 * @throw socket_exception if the action has failed
		 */
		virtual void down() throw (socket_exception) = 0;

		/**
		 * Return the file descriptor for this socket.
		 * @throw socket_exception if no file descriptor is available
		 */
		virtual int fd() const throw (socket_exception);

		/**
		 * Return the file descriptor and bring the socket into the down state
		 * to keep the file descriptor alive even if the socket object gets
		 * destroyed.
		 * @return The file descriptor of this socket.
		 */
		virtual int release() throw (socket_exception);

		/**
		 * Standard socket calls
		 */
		void close() throw (socket_exception);
		void shutdown(int how) throw (socket_exception);

		/**
		 * @return True, if this socket is up and ready.
		 */
		bool ready() const;

		/**
		 * return the family of a socket
		 * @return
		 */
		sa_family_t get_family() const throw (socket_exception);
		static sa_family_t get_family(int fd) throw (socket_exception);

		/**
		 * Checks if the given network protocol is supported
		 */
		static bool hasSupport(const sa_family_t family, const int type = SOCK_DGRAM, const int protocol = 0) throw ();

	protected:
		/**
		 * The socket state determine if the socket file descriptor
		 * is usable or not.
		 */
		enum socketstate {
			SOCKET_DOWN,     //!< SOCKET_DOWN
			SOCKET_UP,       //!< SOCKET_UP
			SOCKET_UNMANAGED,//!< SOCKET_UNMANAGED
			SOCKET_DESTROYED //!< SOCKET_DESTROYED
		};

		/**
		 * This is a protected constructor to prevent any
		 * direct instantiation.
		 * @param fd An existing file descriptor to use.
		 */
		basesocket();
		basesocket(int fd);

		/**
		 * Error check methods
		 */
		void check_socket_error(const int err) const throw (socket_exception);
		void check_bind_error(const int err, const std::string &msg = "") const throw (socket_exception);

		void set_blocking_mode(bool val, int fd = -1) const throw (socket_exception);
		void set_keepalive(bool val, int fd = -1) const throw (socket_exception);
		void set_linger(bool val, int l = 1, int fd = -1) const throw (socket_exception);
		void set_reuseaddr(bool val, int fd = -1) const throw (socket_exception);
		void set_nodelay(bool val, int fd = -1) const throw (socket_exception);
		void set_interface(const vinterface &iface, int fd = -1) const throw (socket_exception);

		void init_socket(const vaddress &addr, int type, int protocol) throw (socket_exception);
		void init_socket(int domain, int type, int protocol) throw (socket_exception);

		void bind(int fd, struct sockaddr *addr, socklen_t len) throw (socket_exception);

		// contains the current socket state
		socketstate _state;

		// contains the file descriptor if one is available
		int _fd;

		// stores the socket family
		sa_family_t _family;
	};

	/**
	 * A clientsocket is used if a incoming connection has been accepted
	 * by the serversocket.
	 */
	class clientsocket : public basesocket {
	public:
		enum CLIENT_OPTION {
			NO_DELAY = 0,
			BLOCKING = 1
		};

		virtual ~clientsocket() = 0;
		virtual void up() throw (socket_exception);
		virtual void down() throw (socket_exception);

		ssize_t send(const char *data, size_t len, int flags = 0) throw (socket_exception);
		ssize_t recv(char *data, size_t len, int flags = 0) throw (socket_exception);

		void set(CLIENT_OPTION opt, bool val) throw (socket_exception);

	protected:
		clientsocket();
		clientsocket(int fd);
	};

	class serversocket : public basesocket {
	public:
		enum SERVER_OPTION {
			BLOCKING = 0
		};

		virtual ~serversocket() = 0;
		virtual void up() throw (socket_exception) = 0;
		virtual void down() throw (socket_exception) = 0;

		void listen(int connections) throw (socket_exception);
		virtual clientsocket* accept(ibrcommon::vaddress &addr) throw (socket_exception) = 0;

		void set(SERVER_OPTION opt, bool val);

	protected:
		serversocket();
		serversocket(int fd);

		int _accept_fd(ibrcommon::vaddress &addr) throw (socket_exception);
	};

	class datagramsocket : public basesocket {
	public:
		virtual ~datagramsocket() = 0;
		virtual void up() throw (socket_exception) = 0;
		virtual void down() throw (socket_exception) = 0;

		virtual ssize_t recvfrom(char *buf, size_t buflen, int flags, ibrcommon::vaddress &addr) throw (socket_exception);
		virtual void sendto(const char *buf, size_t buflen, int flags, const ibrcommon::vaddress &addr) throw (socket_exception);

	protected:
		datagramsocket();
		datagramsocket(int fd);
	};

	/**
	 * A file socket opens a named socket for communication.
	 */
	class filesocket : public clientsocket {
	public:
		filesocket(int fd);
		filesocket(const File &file);
		virtual ~filesocket();
		virtual void up() throw (socket_exception);
		virtual void down() throw (socket_exception);

	private:
		const File _filename;
	};

	/**
	 * A fileserversocket is bound to a specific socket
	 * file waiting for incoming connections.
	 */
	class fileserversocket : public serversocket {
	public:
		fileserversocket(const File &file, int listen = 0);
		virtual ~fileserversocket();
		virtual void up() throw (socket_exception);
		virtual void down() throw (socket_exception);

		virtual clientsocket* accept(ibrcommon::vaddress &addr) throw (socket_exception);

	private:
		void bind(const File &file) throw (socket_exception);

	private:
		const File _filename;
		const int _listen;
	};

	/**
	 * A tcpsocket is used to connect to a TCP server.
	 */
	class tcpsocket : public clientsocket {
	public:
		tcpsocket(int fd);
		tcpsocket(const vaddress &destination, const timeval *timeout = NULL);
		virtual ~tcpsocket();
		virtual void up() throw (socket_exception);
		virtual void down() throw (socket_exception);

	private:
		const vaddress _address;
		timeval _timeout;
	};

	/**
	 * A tcpserversocket is bound to a specific port and
	 * listen for incoming connections. It binds on the ANY
	 * or a specific address.
	 */
	class tcpserversocket : public serversocket {
	public:
		tcpserversocket(const int port, int listen = 2);
		tcpserversocket(const vaddress &address, int listen = 2);
		virtual ~tcpserversocket();
		virtual void up() throw (socket_exception);
		virtual void down() throw (socket_exception);

		const vaddress& get_address() const;

		virtual clientsocket* accept(ibrcommon::vaddress &addr) throw (socket_exception);

	protected:
		void bind(const vaddress &addr) throw (socket_exception);

	private:
		const vaddress _address;
		const int _listen;
	};

	/**
	 * A udpsocket allows to send and receive UDP datagrams
	 * with a bound or non-bound file descriptor.
	 */
	class udpsocket : public datagramsocket {
	public:
		udpsocket();
		udpsocket(const vaddress &address);
		udpsocket(const vinterface &iface, const vaddress &address);
		virtual ~udpsocket();
		virtual void up() throw (socket_exception);
		virtual void down() throw (socket_exception);

		const vinterface& get_interface() const;
		const vaddress& get_address() const;

	protected:
		void bind(const vaddress &addr) throw (socket_exception);

		const vinterface _iface;
		const vaddress _address;
	};

	class multicastsocket : public udpsocket {
	public:
		multicastsocket(const vaddress &address);
		multicastsocket(const vinterface &iface, const vaddress &address);
		virtual ~multicastsocket();
		virtual void up() throw (socket_exception);
		virtual void down() throw (socket_exception);

		void join(const vaddress &group, const vinterface &iface) throw (socket_exception);
		void leave(const vaddress &group, const vinterface &iface) throw (socket_exception);

	private:
		void mcast_op(int optname, const vaddress &group, const vinterface &iface) throw (socket_exception);
	};
}

#endif /* IBRCOMMON_SOCKET_H_ */
