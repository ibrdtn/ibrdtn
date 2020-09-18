/*
 * Serializer.cpp
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

#include "ibrdtn/data/Serializer.h"
#include "ibrdtn/data/BundleBuilder.h"
#include "ibrdtn/data/Bundle.h"
#include "ibrdtn/data/Block.h"
#include "ibrdtn/data/BundleString.h"
#include "ibrdtn/data/ExtensionBlock.h"
#include "ibrdtn/data/PayloadBlock.h"
#include "ibrdtn/data/MetaBundle.h"
#include "ibrdtn/data/DTNTime.h"
#include "ibrdtn/utils/Clock.h"
#include <ibrcommon/refcnt_ptr.h>
#include <ibrcommon/Logger.h>
#include <list>
#include <limits>

#ifdef __DEVELOPMENT_ASSERTIONS__
#include <cassert>
#endif

namespace dtn
{
	namespace data
	{
		DefaultSerializer::DefaultSerializer(std::ostream& stream)
		 : _stream(stream), _compressable(false)
		{
		}

		DefaultSerializer::DefaultSerializer(std::ostream& stream, const Dictionary &d)
		 : _stream(stream), _dictionary(d), _compressable(false)
		{
		}

		void DefaultSerializer::rebuildDictionary(const dtn::data::Bundle &obj)
		{
			// clear the dictionary
			_dictionary.clear();

			// rebuild the dictionary
			_dictionary.add(obj);

			// check if the bundle header could be compressed
			_compressable = isCompressable(obj);
		}

		Serializer& DefaultSerializer::operator <<(const dtn::data::Bundle& obj)
		{
			// rebuild the dictionary
			rebuildDictionary(obj);

			// serialize the primary block
			(*this) << (PrimaryBlock&)obj;

			// serialize all secondary blocks
			for (Bundle::const_iterator iter = obj.begin(); iter != obj.end(); ++iter)
			{
				const Block &b = (**iter);
				(*this) << b;
			}

			return (*this);
		}

		Serializer& DefaultSerializer::operator<<(const dtn::data::BundleFragment &obj)
		{
			// rebuild the dictionary
			rebuildDictionary(obj._bundle);

			PrimaryBlock prim = obj._bundle;
			prim.set(dtn::data::PrimaryBlock::FRAGMENT, true);

			// set the application length according to the payload block size
			dtn::data::Bundle::const_iterator it = obj._bundle.find(dtn::data::PayloadBlock::BLOCK_TYPE);

			if (obj._bundle.get(dtn::data::PrimaryBlock::FRAGMENT)) {
				// keep appdatalength of the fragment
			} else if (it != obj._bundle.end()) {
				const dtn::data::PayloadBlock &payload = dynamic_cast<const dtn::data::PayloadBlock&>(**it);
				prim.appdatalength = payload.getLength();
			} else {
				prim.appdatalength = 0;
			}

			// set the fragmentation offset
			prim.fragmentoffset += obj._offset;

			// serialize the primary block
			(*this) << prim;

			// serialize all secondary blocks
			bool post_payload = false;

			for (Bundle::const_iterator iter = obj._bundle.begin(); iter != obj._bundle.end(); ++iter)
			{
				const Block &b = (**iter);

				try {
					// test if this is the payload block
					const dtn::data::PayloadBlock &payload = dynamic_cast<const dtn::data::PayloadBlock&>(b);

					// serialize the clipped block
					serialize(payload, obj._offset, obj._length);

					// we had serialized the payload block
					post_payload = true;
				} catch (const std::bad_cast&) {
					// serialize this block if
					// ... this block is before the payload block and marked as replicated in every fragment
					// ... this block is after the payload block and all remaining bytes of the payload block are included
					if (post_payload || b.get(dtn::data::Block::REPLICATE_IN_EVERY_FRAGMENT))
					{
						(*this) << b;
					}
				}
			}

			return (*this);
		}

		bool DefaultSerializer::isCompressable(const dtn::data::Bundle &obj) const
		{
			// check if all EID are compressable
			bool compressable = ( obj.source.isCompressable() &&
					obj.destination.isCompressable() &&
					obj.reportto.isCompressable() &&
					obj.custodian.isCompressable() );

			if (compressable)
			{
				// add EID of all secondary blocks
				for (Bundle::const_iterator iter = obj.begin(); iter != obj.end(); ++iter)
				{
					const Block &b = (**iter);
					const std::list<dtn::data::EID> eids = b.getEIDList();

					for (std::list<dtn::data::EID>::const_iterator eit = eids.begin(); eit != eids.end(); ++eit)
					{
						const dtn::data::EID &eid = (*eit);
						if (!eid.isCompressable())
						{
							return false;
						}
					}
				}
			}

			return compressable;
		}

		Serializer& DefaultSerializer::operator <<(const dtn::data::PrimaryBlock& obj)
		{
			_stream << dtn::data::BUNDLE_VERSION;		// bundle version
			_stream << obj.procflags;	// processing flags

			// predict the block length
			Number len = 0;
			dtn::data::Number primaryheader[14];

			primaryheader[8] = obj.timestamp;		// timestamp
			primaryheader[9] = obj.sequencenumber;	// sequence number
			primaryheader[10] = obj.lifetime;		// lifetime

			dtn::data::Dictionary::Reference ref;

			if (_compressable)
			{
				// destination reference
				ref = obj.destination.getCompressed();
				primaryheader[0] = ref.first;
				primaryheader[1] = ref.second;

				// source reference
				ref = obj.source.getCompressed();
				primaryheader[2] = ref.first;
				primaryheader[3] = ref.second;

				// reportto reference
				ref = obj.reportto.getCompressed();
				primaryheader[4] = ref.first;
				primaryheader[5] = ref.second;

				// custodian reference
				ref = obj.custodian.getCompressed();
				primaryheader[6] = ref.first;
				primaryheader[7] = ref.second;

				// dictionary size is zero in a compressed bundle header
				primaryheader[11] = 0;
			}
			else
			{
				// destination reference
				ref = _dictionary.getRef(obj.destination);
				primaryheader[0] = ref.first;
				primaryheader[1] = ref.second;

				// source reference
				ref = _dictionary.getRef(obj.source);
				primaryheader[2] = ref.first;
				primaryheader[3] = ref.second;

				// reportto reference
				ref = _dictionary.getRef(obj.reportto);
				primaryheader[4] = ref.first;
				primaryheader[5] = ref.second;

				// custodian reference
				ref = _dictionary.getRef(obj.custodian);
				primaryheader[6] = ref.first;
				primaryheader[7] = ref.second;

				// dictionary size
				primaryheader[11] = Number(_dictionary.getSize());
				len += _dictionary.getSize();
			}

			for (int i = 0; i < 12; ++i)
			{
				len += primaryheader[i].getLength();
			}

			if (obj.get(dtn::data::Bundle::FRAGMENT))
			{
				primaryheader[12] = obj.fragmentoffset;
				primaryheader[13] = obj.appdatalength;

				len += primaryheader[12].getLength();
				len += primaryheader[13].getLength();
			}

			// write the block length
			_stream << len;

			/*
			 * write the ref block of the dictionary
			 * this includes scheme and ssp for destination, source, reportto and custodian.
			 */
			for (int i = 0; i < 11; ++i)
			{
				_stream << primaryheader[i];
			}

			if (_compressable)
			{
				// write the size of the dictionary (always zero here)
				_stream << primaryheader[11];
			}
			else
			{
				// write size of dictionary + bytearray
				_stream << _dictionary;
			}

			if (obj.get(dtn::data::Bundle::FRAGMENT))
			{
				_stream << primaryheader[12]; // FRAGMENTATION_OFFSET
				_stream << primaryheader[13]; // APPLICATION_DATA_LENGTH
			}
			
			return (*this);
		}

		Serializer& DefaultSerializer::operator <<(const dtn::data::Block& obj)
		{
			_stream.put((char&)obj.getType());
			_stream << obj.getProcessingFlags();

			const Block::eid_list &eids = obj.getEIDList();

#ifdef __DEVELOPMENT_ASSERTIONS__
			// test: BLOCK_CONTAINS_EIDS => (eids.size() > 0)
			assert(!obj.get(Block::BLOCK_CONTAINS_EIDS) || (eids.size() > 0));
#endif

			if (obj.get(Block::BLOCK_CONTAINS_EIDS))
			{
				_stream << Number(eids.size());
				for (Block::eid_list::const_iterator it = eids.begin(); it != eids.end(); ++it)
				{
					dtn::data::Dictionary::Reference offsets;

					if (_compressable)
					{
						offsets = (*it).getCompressed();
					}
					else
					{
						offsets = _dictionary.getRef(*it);
					}

					_stream << offsets.first;
					_stream << offsets.second;
				}
			}

			// write size of the payload in the block
			_stream << Number(obj.getLength());

			// write the payload of the block
			Length slength = 0;
			obj.serialize(_stream, slength);

			return (*this);
		}

		Serializer& DefaultSerializer::serialize(const dtn::data::PayloadBlock& obj, const Length &clip_offset, const Length &clip_length)
		{
			_stream.put((char&)obj.getType());
			_stream << obj.getProcessingFlags();

			const Block::eid_list &eids = obj.getEIDList();

#ifdef __DEVELOPMENT_ASSERTIONS__
			// test: BLOCK_CONTAINS_EIDS => (eids.size() > 0)
			assert(!obj.get(Block::BLOCK_CONTAINS_EIDS) || (eids.size() > 0));
#endif

			if (obj.get(Block::BLOCK_CONTAINS_EIDS))
			{
				_stream << Number(eids.size());
				for (Block::eid_list::const_iterator it = eids.begin(); it != eids.end(); ++it)
				{
					dtn::data::Dictionary::Reference offsets;

					if (_compressable)
					{
						offsets = (*it).getCompressed();
					}
					else
					{
						offsets = _dictionary.getRef(*it);
					}

					_stream << offsets.first;
					_stream << offsets.second;
				}
			}

			// get the remaining payload size
			Length payload_size = obj.getLength();

			// check if the remaining data length is >= clip_length
			Length frag_len = (clip_offset < payload_size) ? payload_size - clip_offset : 0;

			// limit the fragment length to the clip length
			if (frag_len > clip_length) frag_len = clip_length;

			// set the real predicted payload length
			// write size of the payload in the block
			_stream << Number(frag_len);

			if (frag_len > 0)
			{
				// now skip the <offset>-bytes and all bytes after <offset + length>
				obj.serialize( _stream, clip_offset, frag_len );
			}

			return (*this);
		}

		Length DefaultSerializer::getLength(const dtn::data::Bundle &obj)
		{
			// rebuild the dictionary
			rebuildDictionary(obj);

			Length len = 0;
			len += getLength( (PrimaryBlock&)obj );
			
			// add size of all blocks
			for (Bundle::const_iterator iter = obj.begin(); iter != obj.end(); ++iter)
			{
				const Block &b = (**iter);
				len += getLength( b );
			}

			return len;
		}

		Length DefaultSerializer::getLength(const dtn::data::PrimaryBlock& obj) const
		{
			Length len = 0;

			len += sizeof(dtn::data::BUNDLE_VERSION);		// bundle version
			len += obj.procflags.getLength();	// processing flags

			// primary header
			dtn::data::Number primaryheader[14];
			dtn::data::Dictionary::Reference ref;

			if (_compressable)
			{
				// destination reference
				ref = obj.destination.getCompressed();
				primaryheader[0] = ref.first;
				primaryheader[1] = ref.second;

				// source reference
				ref = obj.source.getCompressed();
				primaryheader[2] = ref.first;
				primaryheader[3] = ref.second;;

				// reportto reference
				ref = obj.reportto.getCompressed();
				primaryheader[4] = ref.first;
				primaryheader[5] = ref.second;;

				// custodian reference
				ref = obj.custodian.getCompressed();
				primaryheader[6] = ref.first;
				primaryheader[7] = ref.second;;

				// dictionary size
				primaryheader[11] = Number(0);
			}
			else
			{
				// destination reference
				ref = _dictionary.getRef(obj.destination);
				primaryheader[0] = ref.first;
				primaryheader[1] = ref.second;;

				// source reference
				ref = _dictionary.getRef(obj.source);
				primaryheader[2] = ref.first;
				primaryheader[3] = ref.second;;

				// reportto reference
				ref = _dictionary.getRef(obj.reportto);
				primaryheader[4] = ref.first;
				primaryheader[5] = ref.second;;

				// custodian reference
				ref = _dictionary.getRef(obj.custodian);
				primaryheader[6] = ref.first;
				primaryheader[7] = ref.second;;

				// dictionary size
				primaryheader[11] = Number(_dictionary.getSize());
			}

			// timestamp
			primaryheader[8] = obj.timestamp;

			// sequence number
			primaryheader[9] = obj.sequencenumber;

			// lifetime
			primaryheader[10] = obj.lifetime;

			for (int i = 0; i < 11; ++i)
			{
				len += primaryheader[i].getLength();
			}

			if (_compressable)
			{
				// write the size of the dictionary (always zero here)
				len += primaryheader[11].getLength();
			}
			else
			{
				// write size of dictionary + bytearray
				len += primaryheader[11].getLength();
				len += _dictionary.getSize();
			}

			if (obj.get(dtn::data::Bundle::FRAGMENT))
			{
				primaryheader[12] = obj.fragmentoffset;
				primaryheader[13] = obj.appdatalength;

				len += primaryheader[12].getLength();
				len += primaryheader[13].getLength();
			}

			len += Number(len).getLength();

			return len;
		}

		Length DefaultSerializer::getLength(const dtn::data::Block &obj) const
		{
			Length len = 0;

			len += sizeof(obj.getType());
			len += obj.getProcessingFlags().getLength();

			const Block::eid_list &eids = obj.getEIDList();

#ifdef __DEVELOPMENT_ASSERTIONS__
			// test: BLOCK_CONTAINS_EIDS => (eids.size() > 0)
			assert(!obj.get(Block::BLOCK_CONTAINS_EIDS) || (eids.size() > 0));
#endif

			if (obj.get(Block::BLOCK_CONTAINS_EIDS))
			{
				len += dtn::data::Number(eids.size()).getLength();
				for (Block::eid_list::const_iterator it = eids.begin(); it != eids.end(); ++it)
				{
					dtn::data::Dictionary::Reference offsets = _dictionary.getRef(*it);
					len += offsets.first.getLength();
					len += offsets.second.getLength();
				}
			}

			// size of the payload in the block
			Length payload_size = obj.getLength();
			len += payload_size;

			// size of the payload size
			len += Number(payload_size).getLength();

			return len;
		}

		DefaultDeserializer::DefaultDeserializer(std::istream& stream)
		 : _stream(stream), _validator(_default_validator), _compressed(false), _fragmentation(false)
		{
		}

		DefaultDeserializer::DefaultDeserializer(std::istream &stream, Validator &v)
		 : _stream(stream), _validator(v), _compressed(false), _fragmentation(false)
		{
		}

		void DefaultDeserializer::setFragmentationSupport(bool val)
		{
			_fragmentation = val;
		}

		Deserializer& DefaultDeserializer::operator >>(dtn::data::Bundle& obj)
		{
			// clear all blocks
			obj.clear();

			// read the primary block
			(*this) >> (PrimaryBlock&)obj;

			// read until the last block
			bool lastblock = false;

			block_t block_type;
			dtn::data::Bitset<Block::ProcFlags> procflags;

			// create a bundle builder
			dtn::data::BundleBuilder builder(obj);

			// read all BLOCKs
			while (!_stream.eof() && !lastblock)
			{
				// BLOCK_TYPE
				_stream.get((char&)block_type);

				// read processing flags
				_stream >> procflags;

				try {
					// create a block object
					dtn::data::Block &block = builder.insert(block_type, procflags);

					try {
						// read block content
						(*this).read(obj, block);
					} catch (dtn::PayloadReceptionInterrupted &ex) {
						// some debugging
						IBRCOMMON_LOGGER_DEBUG_TAG("DefaultDeserializer", 15) << "Reception of bundle payload failed." << IBRCOMMON_LOGGER_ENDL;

						// interrupted transmission
						if (!obj.get(dtn::data::PrimaryBlock::DONT_FRAGMENT) && (block.getLength() > 0) && _fragmentation)
						{
							IBRCOMMON_LOGGER_DEBUG_TAG("DefaultDeserializer", 25) << "Create a fragment." << IBRCOMMON_LOGGER_ENDL;

							if ( !obj.get(dtn::data::PrimaryBlock::FRAGMENT) )
							{
								obj.set(dtn::data::PrimaryBlock::FRAGMENT, true);
								obj.appdatalength = ex.length;
								obj.fragmentoffset = 0;
							}
						}
						else
						{
							throw;
						}
					}
				} catch (BundleBuilder::DiscardBlockException &ex) {
					// skip EIDs
					if ( procflags.getBit(dtn::data::Block::BLOCK_CONTAINS_EIDS) )
					{
						Number eidcount;
						_stream >> eidcount;

						for (unsigned int i = 0; eidcount > i; ++i)
						{
							Number scheme, ssp;
							_stream >> scheme;
							_stream >> ssp;
						}
					}

					// read the size of the payload in the block
					Number block_size;
					_stream >> block_size;

					// skip payload
					_stream.ignore(block_size.get<std::streamsize>());
				}

				lastblock = procflags.getBit(Block::LAST_BLOCK);
			}

			// validate this bundle
			_validator.validate(obj);

			return (*this);
		}

		Deserializer& DefaultDeserializer::operator>>(dtn::data::MetaBundle &obj)
		{
			dtn::data::PrimaryBlock pb;
			(*this) >> pb;

			obj.appdatalength = pb.appdatalength;
			obj.custodian = pb.custodian;
			obj.destination = pb.destination;
			obj.expiretime = dtn::utils::Clock::getExpireTime(pb.timestamp, pb.lifetime);
			obj.hopcount = 0;
			obj.lifetime = pb.lifetime;
			obj.fragmentoffset = pb.fragmentoffset;
			obj.procflags = pb.procflags;
			obj.reportto = pb.reportto;
			obj.sequencenumber = pb.sequencenumber;
			obj.source = pb.source;
			obj.timestamp = pb.timestamp;

			return (*this);
		}

		Deserializer& DefaultDeserializer::operator >>(dtn::data::PrimaryBlock& obj)
		{
			char version = 0;
			Number blocklength;

			// check for the right version
			_stream.get(version);
			if (version != dtn::data::BUNDLE_VERSION) throw dtn::InvalidProtocolException("Bundle version differ from ours.");

			// PROCFLAGS
			_stream >> obj.procflags;	// processing flags

			// BLOCK LENGTH
			_stream >> blocklength;

			// EID References
			dtn::data::Dictionary::Reference ref[4];
			for (int i = 0; i < 4; ++i)
			{
				_stream >> ref[i].first;
				_stream >> ref[i].second;
			}

			// timestamp
			_stream >> obj.timestamp;

			// sequence number
			_stream >> obj.sequencenumber;

			// lifetime
			_stream >> obj.lifetime;

			try {
				// dictionary
				_stream >> _dictionary;

				// decode EIDs
				obj.destination = _dictionary.get(ref[0].first, ref[0].second);
				obj.source = _dictionary.get(ref[1].first, ref[1].second);
				obj.reportto = _dictionary.get(ref[2].first, ref[2].second);
				obj.custodian = _dictionary.get(ref[3].first, ref[3].second);
				_compressed = false;
			} catch (const dtn::InvalidDataException&) {
				// error while reading the dictionary. We assume that this is a compressed bundle header.
				obj.destination = dtn::data::EID(ref[0].first, ref[0].second);
				obj.source = dtn::data::EID(ref[1].first, ref[1].second);
				obj.reportto = dtn::data::EID(ref[2].first, ref[2].second);
				obj.custodian = dtn::data::EID(ref[3].first, ref[3].second);
				_compressed = true;
			}

			// fragmentation?
			if (obj.get(dtn::data::Bundle::FRAGMENT))
			{
				_stream >> obj.fragmentoffset;
				_stream >> obj.appdatalength;
			}
			
			// validate this primary block
			_validator.validate(obj);

			return (*this);
		}

		Deserializer& DefaultDeserializer::operator >>(dtn::data::Block& obj)
		{
			// read EIDs
			if ( obj.get(dtn::data::Block::BLOCK_CONTAINS_EIDS))
			{
				Number eidcount;
				_stream >> eidcount;

				for (unsigned int i = 0; eidcount > i; ++i)
				{
					Number scheme, ssp;
					_stream >> scheme;
					_stream >> ssp;

					if (_compressed)
					{
						obj.addEID( dtn::data::EID(scheme, ssp) );
					}
					else
					{
						obj.addEID( _dictionary.get(scheme, ssp) );
					}
				}
			}

			// read the size of the payload in the block
			Number block_size;
			_stream >> block_size;

			// validate this block
			_validator.validate(obj, block_size);

			// read the payload of the block
			obj.deserialize(_stream, block_size.get<dtn::data::Length>());

			return (*this);
		}

		Deserializer& DefaultDeserializer::read(const dtn::data::PrimaryBlock &bundle, dtn::data::Block &obj)
		{
			// read EIDs
			if ( obj.get(dtn::data::Block::BLOCK_CONTAINS_EIDS))
			{
				Number eidcount;
				_stream >> eidcount;

				for (unsigned int i = 0; eidcount > i; ++i)
				{
					Number scheme, ssp;
					_stream >> scheme;
					_stream >> ssp;

					if (_compressed)
					{
						obj.addEID( dtn::data::EID(scheme, ssp) );
					}
					else
					{
						obj.addEID( _dictionary.get(scheme, ssp) );
					}
				}
			}

			// read the size of the payload in the block
			Number block_size;
			_stream >> block_size;

			// validate this block
			_validator.validate(bundle, obj, block_size);

			// read the payload of the block
			obj.deserialize(_stream, block_size.get<Length>());

			return (*this);
		}

		AcceptValidator::AcceptValidator()
		{
		}

		AcceptValidator::~AcceptValidator()
		{
		}

		void AcceptValidator::validate(const dtn::data::PrimaryBlock&) const noexcept (false)
		{
		}

		void AcceptValidator::validate(const dtn::data::Block&, const dtn::data::Number&) const noexcept (false)
		{

		}

		void AcceptValidator::validate(const dtn::data::PrimaryBlock&, const dtn::data::Block&, const dtn::data::Number&) const noexcept (false)
		{

		}

		void AcceptValidator::validate(const dtn::data::Bundle&) const noexcept (false)
		{

		}

		SeparateSerializer::SeparateSerializer(std::ostream& stream)
		 : DefaultSerializer(stream)
		{
		}

		SeparateSerializer::~SeparateSerializer()
		{
		}

		Serializer& SeparateSerializer::operator <<(const dtn::data::Block& obj)
		{
			_stream.put((char&)obj.getType());
			_stream << obj.getProcessingFlags();

			const Block::eid_list &eids = obj.getEIDList();

#ifdef __DEVELOPMENT_ASSERTIONS__
			// test: BLOCK_CONTAINS_EIDS => (_ids.size() > 0)
			assert(!obj.get(Block::BLOCK_CONTAINS_EIDS) || (eids.size() > 0));
#endif

			if (obj.get(Block::BLOCK_CONTAINS_EIDS))
			{
				_stream << Number(eids.size());
				for (Block::eid_list::const_iterator it = eids.begin(); it != eids.end(); ++it)
				{
					dtn::data::BundleString str((*it).getString());
					_stream << str;
				}
			}

			// write size of the payload in the block
			_stream << Number(obj.getLength());

			// write the payload of the block
			Length slength = 0;
			obj.serialize(_stream, slength);

			return (*this);
		}

		Length SeparateSerializer::getLength(const dtn::data::Block &obj) const
		{
			Length len = 0;

			len += sizeof(obj.getType());
			len += obj.getProcessingFlags().getLength();

			const Block::eid_list &eids = obj.getEIDList();

#ifdef __DEVELOPMENT_ASSERTIONS__
			// test: BLOCK_CONTAINS_EIDS => (eids.size() > 0)
			assert(!obj.get(Block::BLOCK_CONTAINS_EIDS) || (eids.size() > 0));
#endif

			if (obj.get(Block::BLOCK_CONTAINS_EIDS))
			{
				len += dtn::data::Number(eids.size()).getLength();
				for (Block::eid_list::const_iterator it = eids.begin(); it != eids.end(); ++it)
				{
					dtn::data::BundleString str((*it).getString());
					len += str.getLength();
				}
			}

			// size of the payload in the block
			len += obj.getLength();

			return len;
		}

		SeparateDeserializer::SeparateDeserializer(std::istream& stream, Bundle &b)
		 : DefaultDeserializer(stream), _bundle(b)
		{
		}

		SeparateDeserializer::~SeparateDeserializer()
		{
		}

		dtn::data::Block& SeparateDeserializer::readBlock()
		{
			BundleBuilder builder(_bundle);

			block_t block_type;
			Bitset<Block::ProcFlags> procflags;

			// BLOCK_TYPE
			_stream.get((char&)block_type);

			// read processing flags
			_stream >> procflags;

			// create a block object
			dtn::data::Block &obj = builder.insert(block_type, procflags);

			// read EIDs
			if ( obj.get(dtn::data::Block::BLOCK_CONTAINS_EIDS))
			{
				Number eidcount;
				_stream >> eidcount;

				for (unsigned int i = 0; eidcount > i; ++i)
				{
					dtn::data::BundleString str;
					_stream >> str;
					obj.addEID(dtn::data::EID(str));
				}
			}

			// read the size of the payload in the block
			Number block_size;
			_stream >> block_size;

			// validate this block
			_validator.validate(obj, block_size);

			// read the payload of the block
			obj.deserialize(_stream, block_size.get<Length>());

			return obj;
		}
	}
}
