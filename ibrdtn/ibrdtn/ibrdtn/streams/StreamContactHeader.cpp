/*
 * StreamContactHeader.cpp
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

#include "ibrdtn/config.h"
#include "ibrdtn/streams/StreamContactHeader.h"
#include "ibrdtn/data/Bundle.h"
#include "ibrdtn/data/Exceptions.h"
#include "ibrdtn/data/BundleString.h"
#include <typeinfo>

#ifdef __WIN32__
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

namespace dtn
{
	namespace streams
	{
		StreamContactHeader::StreamContactHeader()
		 : _flags(REQUEST_ACKNOWLEDGMENTS), _keepalive(0)
		{
		}

		StreamContactHeader::StreamContactHeader(EID localeid)
		 : _localeid(localeid), _flags(REQUEST_ACKNOWLEDGMENTS), _keepalive(0)
		{
		}

		StreamContactHeader::~StreamContactHeader()
		{
		}

		StreamContactHeader& StreamContactHeader::operator=(const StreamContactHeader &other)
		{
			_localeid = other._localeid;
			_flags = other._flags;
			_keepalive = other._keepalive;
			return *this;
		}

		const EID StreamContactHeader::getEID() const
		{
			return _localeid;
		}

		std::ostream &operator<<(std::ostream &stream, const StreamContactHeader &h)
		{
			//BundleStreamWriter writer(stream);
			stream << "dtn!" << TCPCL_VERSION << h._flags.get<char>();

			// convert to network byte order
			uint16_t ka = htons(h._keepalive);
			stream.write( (char*)&ka, 2 );

			dtn::data::BundleString eid(h._localeid.getString());
			stream << eid;

			return stream;
		}

		std::istream &operator>>(std::istream &stream, StreamContactHeader &h)
		{
			char magic[5];

			// wait for magic
			stream.read(magic, 4); magic[4] = '\0';
			std::string str_magic(magic);

			if (str_magic != "dtn!")
			{
				throw InvalidProtocolException("not talking dtn");
			}

			// version
			char version; stream.get(version);
			if (version != TCPCL_VERSION)
			{
				throw InvalidProtocolException("invalid bundle protocol version");
			}

			// flags
			char tmp;
			stream.get(tmp);
			h._flags = tmp;

			uint16_t ka = 0;
			stream.read((char*)&ka, 2);

			// convert from network byte order
			h._keepalive = ntohs(ka);

			dtn::data::BundleString eid;
			stream >> eid;
			h._localeid = EID(eid);

			return stream;
		}
	}
}
