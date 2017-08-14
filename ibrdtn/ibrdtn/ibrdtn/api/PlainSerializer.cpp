/*
 * PlainSerializer.cpp
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

#include "ibrdtn/api/PlainSerializer.h"
#include "ibrdtn/data/BundleBuilder.h"
#include "ibrdtn/data/Bundle.h"
#include "ibrdtn/utils/Utils.h"
#include <ibrcommon/refcnt_ptr.h>
#include <ibrcommon/data/Base64Stream.h>
#include <ibrcommon/data/Base64Reader.h>
#include <ibrcommon/Logger.h>
#include <list>

namespace dtn
{
	namespace api
	{
		PlainSerializer::Encoding PlainSerializer::parseEncoding(const std::string &data)
		{
			if (data == "raw") return PlainSerializer::RAW;
			if (data == "base64") return PlainSerializer::BASE64;
			if (data == "skip") return PlainSerializer::SKIP_PAYLOAD;
			return PlainSerializer::INVALID;
		}

		std::string PlainSerializer::printEncoding(const PlainSerializer::Encoding &enc)
		{
			switch (enc) {
			case PlainSerializer::RAW:
				return "raw";
			case PlainSerializer::BASE64:
				return "base64";
			case PlainSerializer::SKIP_PAYLOAD:
				return "skip";
			default:
				return "invalid";
			}
		}

		PlainSerializer::PlainSerializer(std::ostream &stream, Encoding enc)
		 : _stream(stream), _encoding(enc)
		{
		}

		PlainSerializer::~PlainSerializer()
		{
		}

		dtn::data::Serializer& PlainSerializer::operator<<(const dtn::data::Bundle &obj)
		{
			// serialize the primary block
			(*this) << (dtn::data::PrimaryBlock&)obj;

			// block count
			_stream << "Blocks: " << obj.size() << std::endl;

			// serialize all secondary blocks
			for (dtn::data::Bundle::const_iterator iter = obj.begin(); iter != obj.end(); ++iter)
			{
				const dtn::data::Block &b = (**iter);
				_stream << std::endl;
				(*this) << b;
			}

			_stream << std::endl;

			return (*this);
		}

		dtn::data::Serializer& PlainSerializer::operator<<(const dtn::data::PrimaryBlock &obj)
		{
			_stream << "Processing flags: " << obj.procflags.toString() << std::endl;
			_stream << "Timestamp: " << obj.timestamp.toString() << std::endl;
			_stream << "Sequencenumber: " << obj.sequencenumber.toString() << std::endl;
			_stream << "Source: " << obj.source.getString() << std::endl;
			_stream << "Destination: " << obj.destination.getString() << std::endl;
			_stream << "Reportto: " << obj.reportto.getString() << std::endl;
			_stream << "Custodian: " << obj.custodian.getString() << std::endl;
			_stream << "Lifetime: " << obj.lifetime.toString() << std::endl;

			if (obj.procflags & dtn::data::PrimaryBlock::FRAGMENT)
			{
				_stream << "Fragment offset: " << obj.fragmentoffset.toString() << std::endl;
				_stream << "Application data length: " << obj.appdatalength.toString() << std::endl;
			}

			return (*this);
		}

		dtn::data::Serializer& PlainSerializer::operator<<(const dtn::data::Block &obj)
		{
			_stream << "Block: " << (int)((unsigned char)obj.getType()) << std::endl;

			std::stringstream flags;

			if (obj.get(dtn::data::Block::LAST_BLOCK))
			{
				flags << " LAST_BLOCK";
			}

			if (obj.get(dtn::data::Block::REPLICATE_IN_EVERY_FRAGMENT))
			{
				flags << " REPLICATE_IN_EVERY_FRAGMENT";
			}

			if (obj.get(dtn::data::Block::TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED))
			{
				flags << " TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED";
			}

			if (obj.get(dtn::data::Block::DELETE_BUNDLE_IF_NOT_PROCESSED))
			{
				flags << " DELETE_BUNDLE_IF_NOT_PROCESSED";
			}

			if (obj.get(dtn::data::Block::DISCARD_IF_NOT_PROCESSED))
			{
				flags << " DISCARD_IF_NOT_PROCESSED";
			}

			if (obj.get(dtn::data::Block::FORWARDED_WITHOUT_PROCESSED))
			{
				flags << " FORWARDED_WITHOUT_PROCESSED";
			}

			if (flags.str().length() > 0)
			{
				_stream << "Flags:" << flags.str() << std::endl;
			}

			if (obj.get(dtn::data::Block::BLOCK_CONTAINS_EIDS))
			{
				std::list<dtn::data::EID> eid_list = obj.getEIDList();

				for (std::list<dtn::data::EID>::const_iterator iter = eid_list.begin(); iter != eid_list.end(); ++iter)
				{
					_stream << "EID: " << (*iter).getString() << std::endl;
				}
			}

			_stream << "Length: " << obj.getLength() << std::endl;
			_stream << "Encoding: " << PlainSerializer::printEncoding(_encoding) << std::endl;

			if (_encoding != SKIP_PAYLOAD)
			{
				try {
					_stream << std::endl;

					// put data here
					dtn::data::Length slength = 0;
					switch(_encoding)
					{
						case BASE64:
						{
							ibrcommon::Base64Stream b64(_stream, false, 80);
							obj.serialize(b64, slength);
							b64 << std::flush;
							break;
						}

						case RAW:
							obj.serialize(_stream, slength);
							_stream << std::flush;
							break;

						default:
							break;
					}

				} catch (const std::exception &ex) {
					std::cerr << ex.what() << std::endl;
				}

				_stream << std::endl;
			}

			return (*this);
		}

		dtn::data::Length PlainSerializer::getLength(const dtn::data::Bundle&)
		{
			return 0;
		}

		dtn::data::Length PlainSerializer::getLength(const dtn::data::PrimaryBlock&) const
		{
			return 0;
		}

		dtn::data::Length PlainSerializer::getLength(const dtn::data::Block&) const
		{
			return 0;
		}

		PlainDeserializer::PlainDeserializer(std::istream &stream)
		 : _stream(stream), _lastblock(false)
		{
		}

		PlainDeserializer::~PlainDeserializer()
		{
		}

		dtn::data::Deserializer& PlainDeserializer::operator>>(dtn::data::Bundle &obj)
		{
			dtn::data::BundleBuilder builder(obj);

			// clear all blocks
			obj.clear();

			// read the primary block
			(*this) >> (dtn::data::PrimaryBlock&)obj;

			// read until the last block
			_lastblock = false;

			// read all BLOCKs
			while (!_stream.eof() && !_lastblock)
			{
				try {
					readBlock(builder);
				} catch (const dtn::data::BundleBuilder::DiscardBlockException &ex) {
					IBRCOMMON_LOGGER_DEBUG_TAG("PlainDeserializer", 5) << "unknown block has been discarded " << obj.toString() << " has been removed" << IBRCOMMON_LOGGER_ENDL;
				} catch (const UnknownBlockException &ex) {
					IBRCOMMON_LOGGER_DEBUG_TAG("PlainDeserializer", 5) << "unknown administrative block in bundle " << obj.toString() << " has been removed" << IBRCOMMON_LOGGER_ENDL;
				} catch (const BlockNotProcessableException &ex) {
					IBRCOMMON_LOGGER_DEBUG_TAG("PlainDeserializer", 5) << "unprocessable block in bundle " << obj.toString() << " has been removed" << IBRCOMMON_LOGGER_ENDL;
				}
			}

			return (*this);
		}

		dtn::data::Deserializer& PlainDeserializer::operator>>(dtn::data::PrimaryBlock &obj)
		{
			std::string data;

			// read until the first empty line appears
			while (_stream.good())
			{
				getline(_stream, data);

				std::string::reverse_iterator iter = data.rbegin();
				if ( (*iter) == '\r' ) data = data.substr(0, data.length() - 1);

//				// strip off the last char
//				data.erase(data.size() - 1);

				// abort after the first empty line
				if (data.size() == 0) break;

				// split header value
				std::vector<std::string> values = dtn::utils::Utils::tokenize(":", data, 1);

				// if there are not enough parameter abort with an error
				if (values.size() < 1) throw ibrcommon::Exception("parsing error");

				// assign header value
				if (values[0] == "Processing flags")
				{
					obj.procflags.fromString(values[1]);
				}
				else if (values[0] == "Timestamp")
				{
					obj.timestamp.fromString(values[1]);
				}
				else if (values[0] == "Sequencenumber")
				{
					obj.sequencenumber.fromString(values[1]);
				}
				else if (values[0] == "Source")
				{
					obj.source = values[1];
				}
				else if (values[0] == "Destination")
				{
					obj.destination = values[1];
				}
				else if (values[0] == "Reportto")
				{
					obj.reportto = values[1];
				}
				else if (values[0] == "Custodian")
				{
					obj.custodian = values[1];
				}
				else if (values[0] == "Lifetime")
				{
					obj.lifetime.fromString(values[1]);
				}
				else if (values[0] == "Fragment offset")
				{
					obj.fragmentoffset.fromString(values[1]);
				}
				else if (values[0] == "Application data length")
				{
					obj.appdatalength.fromString(values[1]);
				}
			}

			return (*this);
		}

		dtn::data::Deserializer& PlainDeserializer::operator>>(dtn::data::Block &obj)
		{
			std::string data;
			dtn::data::Length blocksize = 0;
			PlainSerializer::Encoding enc = PlainSerializer::BASE64;

			// read until the first empty line appears
			while (_stream.good())
			{
				getline(_stream, data);

				std::string::reverse_iterator iter = data.rbegin();
				if ( (*iter) == '\r' ) data = data.substr(0, data.length() - 1);

//				// strip off the last char
//				data.erase(data.size() - 1);

				// abort after the first empty line
				if (data.size() == 0) break;

				// split header value
				std::vector<std::string> values = dtn::utils::Utils::tokenize(":", data, 1);

				// skip invalid lines
				if(values.size() < 2)
					continue;

				// assign header value
				if (values[0] == "Flags")
				{
					std::vector<std::string> flags = dtn::utils::Utils::tokenize(" ", values[1]);

					for (std::vector<std::string>::const_iterator iter = flags.begin(); iter != flags.end(); ++iter)
					{
						const std::string &value = (*iter);
						if (value == "LAST_BLOCK")
						{
							obj.set(dtn::data::Block::LAST_BLOCK, true);
							_lastblock = true;
						}
						else if (value == "FORWARDED_WITHOUT_PROCESSED")
						{
							obj.set(dtn::data::Block::FORWARDED_WITHOUT_PROCESSED, true);
						}
						else if (value == "DISCARD_IF_NOT_PROCESSED")
						{
							obj.set(dtn::data::Block::DISCARD_IF_NOT_PROCESSED, true);
						}
						else if (value == "DELETE_BUNDLE_IF_NOT_PROCESSED")
						{
							obj.set(dtn::data::Block::DELETE_BUNDLE_IF_NOT_PROCESSED, true);
						}
						else if (value == "TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED")
						{
							obj.set(dtn::data::Block::TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED, true);
						}
						else if (value == "REPLICATE_IN_EVERY_FRAGMENT")
						{
							obj.set(dtn::data::Block::REPLICATE_IN_EVERY_FRAGMENT, true);
						}
					}
				}
				else if (values[0] == "EID")
				{
					obj.addEID(values[1]);
					obj.set(dtn::data::Block::BLOCK_CONTAINS_EIDS, true);
				}
				else if (values[0] == "Length")
				{
					std::stringstream ss; ss.str(values[1]);
					ss >> blocksize;
				}
				else if (values[0] == "Encoding")
				{
					std::stringstream ss; ss << values[1]; ss.clear(); ss >> values[1];
					enc = PlainSerializer::parseEncoding(values[1]);
				}
			}

			// then read the payload
			IBRCOMMON_LOGGER_DEBUG_TAG("PlainDeserializer", 20) << "API expecting payload size of " << blocksize << IBRCOMMON_LOGGER_ENDL;

			switch(enc)
			{
				case PlainSerializer::BASE64:
					{
						ibrcommon::Base64Reader base64_decoder(_stream, blocksize);
						obj.deserialize(base64_decoder, blocksize);
						break;
					}
				case PlainSerializer::RAW:
					obj.deserialize(_stream, blocksize);
					break;

				default:
					break;
			}

			std::string buffer;

			// read the appended newline character
			getline(_stream, buffer);
			std::string::reverse_iterator iter = buffer.rbegin();
			if ( (*iter) == '\r' ) buffer = buffer.substr(0, buffer.length() - 1);
			if (buffer.size() != 0) throw dtn::InvalidDataException("last line not empty");

			// read the final empty line
			getline(_stream, buffer);
			iter = buffer.rbegin();
			if ( (*iter) == '\r' ) buffer = buffer.substr(0, buffer.length() - 1);
			if (buffer.size() != 0) throw dtn::InvalidDataException("last line not empty");

			return (*this);
		}

		dtn::data::Deserializer& PlainDeserializer::operator>>(ibrcommon::BLOB::iostream &obj)
		{
			std::string data;
			dtn::data::Length blocksize = 0;
			PlainSerializer::Encoding enc = PlainSerializer::BASE64;

			// read until the first empty line appears
			while (_stream.good())
			{
				getline(_stream, data);

				std::string::reverse_iterator iter = data.rbegin();
				if ( (*iter) == '\r' ) data = data.substr(0, data.length() - 1);

				// abort after the first empty line
				if (data.size() == 0) break;

				// split header value
				std::vector<std::string> values = dtn::utils::Utils::tokenize(":", data, 1);

				// skip invalid lines
				if(values.size() < 1)
					continue;

				// assign header value
				if (values[0] == "Length")
				{
					std::stringstream ss; ss.str(values[1]);
					ss >> blocksize;
				}
				else if (values[0] == "Encoding")
				{
					std::stringstream ss; ss << values[1]; ss.clear(); ss >> values[1];
					enc = PlainSerializer::parseEncoding(values[1]);
				}
			}

			// then read the payload
			switch (enc)
			{
				case PlainSerializer::BASE64:
					{
						ibrcommon::Base64Reader base64_decoder(_stream, blocksize);
						ibrcommon::BLOB::copy(*obj, base64_decoder, blocksize);
						break;
					}
				case PlainSerializer::RAW:
					ibrcommon::BLOB::copy(*obj, _stream, blocksize);
					break;

				default:
					break;
			}

			std::string buffer;

			// read the appended newline character
			getline(_stream, buffer);
			std::string::reverse_iterator iter = buffer.rbegin();
			if ( (*iter) == '\r' ) buffer = buffer.substr(0, buffer.length() - 1);
			if (buffer.size() != 0) throw dtn::InvalidDataException("last line not empty");

			// read the final empty line
			getline(_stream, buffer);
			iter = buffer.rbegin();
			if ( (*iter) == '\r' ) buffer = buffer.substr(0, buffer.length() - 1);
			if (buffer.size() != 0) throw dtn::InvalidDataException("last line not empty");

			return (*this);
		}

		void PlainSerializer::writeData(const dtn::data::Block &block)
		{
			_stream << "Length: " << block.getLength() << std::endl;
			_stream << "Encoding: " << PlainSerializer::printEncoding(_encoding) << std::endl;
			_stream << std::endl;

			size_t slength = 0;

			switch(_encoding)
			{
				case BASE64:
				{
					// put data here
					ibrcommon::Base64Stream b64(_stream, false, 80);
					block.serialize(b64, slength);
					b64 << std::flush;
					break;
				}

				case RAW:
					block.serialize(_stream, slength);
					break;

				default:
					break;
			}
			_stream << std::endl;
		}

		void PlainSerializer::writeData(std::istream &stream, const dtn::data::Length &len)
		{
			_stream << "Length: " << len << std::endl;
			_stream << "Encoding: " << PlainSerializer::printEncoding(_encoding) << std::endl;
			_stream << std::endl;

			switch(_encoding)
			{
				case BASE64:
				{
					ibrcommon::Base64Stream b64(_stream, false, 80);
					ibrcommon::BLOB::copy(b64, stream, len);
					b64 << std::flush;
					break;
				}

				case RAW:
					ibrcommon::BLOB::copy(_stream, stream, len);
					_stream << std::flush;
					break;

				default:
					break;
			}
			_stream << std::endl;
		}

		void PlainDeserializer::readData(std::ostream &stream)
		{
			std::string data;
			dtn::data::Length len = 0;
			PlainSerializer::Encoding enc = PlainSerializer::BASE64;

			// read headers until an empty line
			while (true)
			{
				// read the block type (first line)
				getline(_stream, data);

				std::string::reverse_iterator iter = data.rbegin();
				if ( (*iter) == '\r' ) data = data.substr(0, data.length() - 1);

				// abort if the line data is empty
				if (data.size() == 0) break;

				// split header value
				std::vector<std::string> values = dtn::utils::Utils::tokenize(":", data, 1);

				if (values[0] == "Length")
				{
					std::stringstream ss; ss.str(values[1]);
					ss >> len;
				}
				else if (values[0] == "Encoding")
				{
					std::stringstream ss; ss << values[1]; ss.clear(); ss >> values[1];
					enc = PlainSerializer::parseEncoding(values[1]);
				}
				else
				{
					/* API documentation states "while header fields except the length field may be omitted" */
					//throw dtn::InvalidDataException("unknown block header");
				}
			}

			if (enc == PlainSerializer::BASE64)
			{
				ibrcommon::Base64Stream b64(stream, true);

				while (b64.good())
				{
					getline(_stream, data);

					std::string::reverse_iterator iter = data.rbegin();
					if ( (*iter) == '\r' ) data = data.substr(0, data.length() - 1);

					// abort after the first empty line
					if (data.size() == 0) break;

					// put the line into the stream decoder
					b64 << data;
				}

				b64 << std::flush;
			} else if (enc == PlainSerializer::RAW) {
				ibrcommon::BLOB::copy(stream, _stream, len);
			}
		}

		dtn::data::Block& PlainDeserializer::readBlock(dtn::data::BundleBuilder &builder)
		{
			std::string buffer;
			dtn::data::block_t block_type;

			// read the block type (first line)
			getline(_stream, buffer);

			std::string::reverse_iterator iter = buffer.rbegin();
			if ( (*iter) == '\r' ) buffer = buffer.substr(0, buffer.length() - 1);

			// abort if the line data is empty
			if (buffer.size() == 0) throw dtn::InvalidDataException("block header is missing");

			// split header value
			std::vector<std::string> values = dtn::utils::Utils::tokenize(":", buffer, 1);

			if (values[0] == "Block")
			{
				std::stringstream ss; ss.str(values[1]);
				int type;
				ss >> type;
				block_type = static_cast<dtn::data::block_t>(type);
			}
			else
			{
				throw dtn::InvalidDataException("need block type as first header");
			}

			try {
				const dtn::data::Bitset<dtn::data::Block::ProcFlags> procflags;
				dtn::data::Block &block = builder.insert(block_type, procflags);
				(*this) >> block;
				return block;
			} catch (dtn::data::BundleBuilder::DiscardBlockException &ex) {
				// TODO: skip block
				throw;
			}
		}
	}
}
