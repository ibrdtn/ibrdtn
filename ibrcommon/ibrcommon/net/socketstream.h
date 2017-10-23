/*
 * socketstream.h
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

#ifndef SOCKETSTREAM_H_
#define SOCKETSTREAM_H_

#include "ibrcommon/net/socket.h"
#include "ibrcommon/net/vsocket.h"
#include <streambuf>
#include <iostream>
#include <vector>

namespace ibrcommon
{
	class stream_exception : public Exception
	{
	public:
		stream_exception(std::string error) : Exception(error)
		{};
	};

	class socketstream : public std::basic_streambuf<char, std::char_traits<char> >, public std::iostream
	{
	public:
		socketstream(clientsocket *sock, size_t buffer_size = 5120);
		virtual ~socketstream();

		void setTimeout(const timeval &val);
		void close();

		socket_error_code errmsg;

	protected:
		virtual int sync();
		virtual std::char_traits<char>::int_type overflow(std::char_traits<char>::int_type = std::char_traits<char>::eof());
		virtual std::char_traits<char>::int_type underflow();

	private:
		vsocket _socket;
		const size_t _bufsize;

		// Input buffer
		std::vector<char> in_buf_;
		// Output buffer
		std::vector<char> out_buf_;

		timeval _timeout;
	};
} /* namespace ibrcommon */
#endif /* SOCKETSTREAM_H_ */
