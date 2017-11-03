/*
 * BundleID.cpp
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
#include "ibrdtn/data/BundleID.h"
#include "ibrdtn/data/Number.h"
#include "ibrdtn/data/BundleString.h"
#include <string.h>

// include code for platform-independent endianess conversion
#include "ibrdtn/data/Endianess.h"

namespace dtn
{
	namespace data
	{
		const unsigned int BundleID::RAW_LENGTH_MAX = 33 + 1024 + 1 + 1024;

		BundleID::BundleID()
		 : source(), timestamp(0), sequencenumber(0), fragmentoffset(0), _fragment(false), _payloadlength(0)
		{
		}

		BundleID::BundleID(const BundleID &id)
		 : source(id.source), timestamp(id.timestamp), sequencenumber(id.sequencenumber),
		   fragmentoffset(id.fragmentoffset), _fragment(id.isFragment()), _payloadlength(id.getPayloadLength())
		{
		}

		BundleID::~BundleID()
		{
		}

		BundleID& BundleID::operator=(const BundleID &id)
		{
			source = id.source;
			timestamp = id.timestamp;
			sequencenumber = id.sequencenumber;
			fragmentoffset = id.fragmentoffset;
			setFragment(id.isFragment());
			setPayloadLength(id.getPayloadLength());
			return (*this);
		}

		bool BundleID::operator<(const BundleID& other) const
		{
			if (source < other.source) return true;
			if (source != other.source) return false;

			if (timestamp < other.timestamp) return true;
			if (timestamp != other.timestamp) return false;

			if (sequencenumber < other.sequencenumber) return true;
			if (sequencenumber != other.sequencenumber) return false;

			if (other.isFragment())
			{
				if (!isFragment()) return true;
				if (fragmentoffset < other.fragmentoffset) return true;
				if (fragmentoffset != other.fragmentoffset) return false;

				return (getPayloadLength() < other.getPayloadLength());
			}

			return false;
		}

		bool BundleID::operator>(const BundleID& other) const
		{
			return !(((*this) < other) || ((*this) == other));
		}

		bool BundleID::operator!=(const BundleID& other) const
		{
			return !((*this) == other);
		}

		bool BundleID::operator==(const BundleID& other) const
		{
			if (other.timestamp != timestamp) return false;
			if (other.sequencenumber != sequencenumber) return false;
			if (other.source != source) return false;
			if (other.isFragment() != isFragment()) return false;

			if (isFragment())
			{
				if (other.fragmentoffset != fragmentoffset) return false;
				if (other.getPayloadLength() != getPayloadLength()) return false;
			}

			return true;
		}

		dtn::data::Length BundleID::getPayloadLength() const
		{
			return _payloadlength;
		}

		void BundleID::setPayloadLength(const dtn::data::Length &value)
		{
			_payloadlength = value;
		}

		bool BundleID::isFragment() const
		{
			return _fragment;
		}

		void BundleID::setFragment(bool val)
		{
			_fragment = val;
		}

		void BundleID::addTo(ibrcommon::BloomFilter &bf) const
		{
			unsigned char data[RAW_LENGTH_MAX];
			const size_t data_len = raw((unsigned char*)&data, RAW_LENGTH_MAX);
			bf.insert((unsigned char*)&data, data_len);
		}

		bool BundleID::isIn(const ibrcommon::BloomFilter &bf) const
		{
			unsigned char data[RAW_LENGTH_MAX];
			const size_t data_len = raw((unsigned char*)&data, RAW_LENGTH_MAX);
			return bf.contains((unsigned char*)&data, data_len);
		}

		size_t BundleID::raw(unsigned char *data, size_t len) const
		{
			uint64_t tmp = 0;

			// stop here if there is not enough space
			if (len < 33) return 0;

			// add timestamp data
			tmp = GUINT64_TO_BE(timestamp.get<uint64_t>());
			::memcpy(data, reinterpret_cast<unsigned char*>(&tmp), sizeof(tmp));
			data += sizeof(tmp);

			// add sequencenumber
			tmp = GUINT64_TO_BE(sequencenumber.get<uint64_t>());
			::memcpy(data, reinterpret_cast<unsigned char*>(&tmp), sizeof(tmp));
			data += sizeof(tmp);

			// add fragment yes/no
			(uint8_t&)(*data) = isFragment() ? 1 : 0;
			data += sizeof(uint8_t);

			// add fragment offset
			tmp = isFragment() ? GUINT64_TO_BE(fragmentoffset.get<uint64_t>()) : 0;
			::memcpy(data, reinterpret_cast<unsigned char*>(&tmp), sizeof(tmp));
			data += sizeof(tmp);

			// add fragment length
			tmp = isFragment() ? GUINT64_TO_BE(getPayloadLength()) : 0;
			::memcpy(data, reinterpret_cast<unsigned char*>(&tmp), sizeof(tmp));
			data += sizeof(tmp);

			// get source endpoint string
			const std::string s = source.getString();

			// copy source endpoint into data array
			::strncpy((char*)data, s.c_str(), len - 33);

			if ((len - 33) < s.length()) {
				return len;
			} else {
				return 33 + s.length();
			}
		}

		std::string BundleID::toString() const
		{
			std::stringstream ss;
			ss << "[" << timestamp.toString() << "." << sequencenumber.toString();

			if (isFragment())
			{
				ss << "." << fragmentoffset.toString();
				ss << "." << getPayloadLength();
			}

			ss << "] " << source.getString();

			return ss.str();
		}

		std::ostream &operator<<(std::ostream &stream, const BundleID &obj)
		{
			dtn::data::BundleString source(obj.source.getString());

			stream << obj.timestamp << obj.sequencenumber;

			if (obj.isFragment()) {
				stream.put(1);
				stream << obj.fragmentoffset;
				stream << dtn::data::Number(obj.getPayloadLength());
			} else {
				stream.put(0);
			}

			stream << source;

			return stream;
		}

		std::istream &operator>>(std::istream &stream, BundleID &obj)
		{
			dtn::data::BundleString source;

			stream >> obj.timestamp >> obj.sequencenumber;

			uint8_t frag = 0;
			stream.get((char&)frag);

			if (frag == 1) {
				stream >> obj.fragmentoffset;
				dtn::data::Number tmp;
				stream >> tmp;
				obj.setPayloadLength(tmp.get<dtn::data::Length>());
				obj.setFragment(true);
			} else {
				obj.setFragment(false);
			}

			stream >> source;
			obj.source = dtn::data::EID(source);

			return stream;
		}
	}
}
