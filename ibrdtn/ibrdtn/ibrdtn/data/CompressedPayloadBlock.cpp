/*
 * CompressedPayloadBlock.cpp
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
#include "ibrdtn/data/CompressedPayloadBlock.h"
#include "ibrdtn/data/PayloadBlock.h"
#include <ibrcommon/data/BLOB.h>

#include <cassert>
#include <vector>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

namespace dtn
{
	namespace data
	{
		const dtn::data::block_t CompressedPayloadBlock::BLOCK_TYPE = 202;

		dtn::data::Block* CompressedPayloadBlock::Factory::create()
		{
			return new CompressedPayloadBlock();
		}

		CompressedPayloadBlock::CompressedPayloadBlock()
		 : dtn::data::Block(CompressedPayloadBlock::BLOCK_TYPE), _algorithm(COMPRESSION_UNKNOWN), _origin_size(0)
		{
		}

		CompressedPayloadBlock::~CompressedPayloadBlock()
		{
		}

		Length CompressedPayloadBlock::getLength() const
		{
			return _algorithm.getLength() + _origin_size.getLength();
		}

		std::ostream& CompressedPayloadBlock::serialize(std::ostream &stream, Length &len) const
		{
			stream << _algorithm << _origin_size;
			len -= _algorithm.getLength() + _origin_size.getLength();
			return stream;
		}

		std::istream& CompressedPayloadBlock::deserialize(std::istream &stream, const Length&)
		{
			stream >> _algorithm;
			stream >> _origin_size;
			return stream;
		}

		void CompressedPayloadBlock::setAlgorithm(CompressedPayloadBlock::COMPRESS_ALGS alg)
		{
			_algorithm = alg;
		}

		CompressedPayloadBlock::COMPRESS_ALGS CompressedPayloadBlock::getAlgorithm() const
		{
			return CompressedPayloadBlock::COMPRESS_ALGS(_algorithm.get<size_t>());
		}

		void CompressedPayloadBlock::setOriginSize(const Number &s)
		{
			_origin_size = s;
		}

		const Number& CompressedPayloadBlock::getOriginSize() const
		{
			return _origin_size;
		}

		void CompressedPayloadBlock::compress(dtn::data::Bundle &b, CompressedPayloadBlock::COMPRESS_ALGS alg)
		{
			Bundle::iterator p_it = b.find(dtn::data::PayloadBlock::BLOCK_TYPE);
			if (p_it == b.end()) throw ibrcommon::Exception("Payload block missing.");
			dtn::data::PayloadBlock &p = dynamic_cast<dtn::data::PayloadBlock&>(**p_it);

			// get a data container for the compressed payload
			ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();

			// lock the BLOBs while we are compress the payload
			{
				// get streams of both containers
				ibrcommon::BLOB::iostream is = p.getBLOB().iostream();
				ibrcommon::BLOB::iostream os = ref.iostream();

				// compress the payload
				CompressedPayloadBlock::compress(alg, *is, *os);
			}

			// add a compressed payload block in front of the old payload block
			dtn::data::CompressedPayloadBlock &cpb = b.push_front<CompressedPayloadBlock>();

			// set cpb values
			cpb.setAlgorithm(alg);
			cpb.setOriginSize(p.getLength());

			// add the new payload block to the bundle
			b.insert(p_it, ref);

			// delete the old payload block
			b.erase(p_it);
		}

		void CompressedPayloadBlock::extract(dtn::data::Bundle &b)
		{
			// get the CPB first
			dtn::data::CompressedPayloadBlock &cpb = b.find<CompressedPayloadBlock>();

			// get the payload block
			Bundle::iterator p_it = b.find(dtn::data::PayloadBlock::BLOCK_TYPE);
			if (p_it == b.end()) throw ibrcommon::Exception("Payload block missing.");
			dtn::data::PayloadBlock &p = dynamic_cast<dtn::data::PayloadBlock&>(**p_it);

			// get a data container for the uncompressed payload
			ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();

			// lock the BLOBs while we are uncompress the payload
			{
				// get streams of both containers
				ibrcommon::BLOB::iostream is = p.getBLOB().iostream();
				ibrcommon::BLOB::iostream os = ref.iostream();

				// compress the payload
				CompressedPayloadBlock::extract(cpb.getAlgorithm(), *is, *os);
			}

			// add the new payload block to the bundle
			b.insert(p_it, ref);

			// delete the old payload block
			b.erase(p_it);

			// delete the CPB
			b.remove(cpb);
		}

		void CompressedPayloadBlock::compress(CompressedPayloadBlock::COMPRESS_ALGS alg, std::istream &is, std::ostream &os)
		{
			switch (alg)
			{
				case COMPRESSION_ZLIB:
				{
#ifdef HAVE_ZLIB
					const uInt CHUNK_SIZE = 16384;

					int ret, flush;
					uInt have;
					std::vector<unsigned char> in(CHUNK_SIZE);
					std::vector<unsigned char> out(CHUNK_SIZE);
					z_stream strm;

					/* allocate deflate state */
					strm.zalloc = Z_NULL;
					strm.zfree = Z_NULL;
					strm.opaque = Z_NULL;
					ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);

					// exit if something is wrong
					if (ret != Z_OK) throw ibrcommon::Exception("initialization of zlib failed");

					do {
						is.read((char*)&in[0], CHUNK_SIZE);
						strm.avail_in = static_cast<uInt>(is.gcount());

						flush = is.eof() ? Z_FINISH : Z_NO_FLUSH;
						strm.next_in = &in[0];

						do {
							strm.avail_out = CHUNK_SIZE;
							strm.next_out = &out[0];

							ret = deflate(&strm, flush);    /* no bad return value */
							assert(ret != Z_STREAM_ERROR);  /* state not clobbered */

							// determine how many bytes are available
							have = CHUNK_SIZE - strm.avail_out;

							// write the buffer to the output stream
							os.write((char*)&out[0], have);

							if (!os.good())
							{
								(void)deflateEnd(&strm);
								throw ibrcommon::Exception("decompression failed. output stream went wrong.");
							}

						} while (strm.avail_out == 0);
						assert(strm.avail_in == 0);     /* all input will be used */
					} while (flush != Z_FINISH);
					assert(ret == Z_STREAM_END);        /* stream will be complete */

					(void)deflateEnd(&strm);
#else
					throw ibrcommon::Exception("zlib is not supported");
#endif
					break;
				}

				default:
					throw ibrcommon::Exception("compression mode is not supported");
			}
		}

		void CompressedPayloadBlock::extract(CompressedPayloadBlock::COMPRESS_ALGS alg, std::istream &is, std::ostream &os)
		{
			switch (alg)
			{
				case COMPRESSION_ZLIB:
				{
#ifdef HAVE_ZLIB
					const uInt CHUNK_SIZE = 16384;

					int ret;
					uInt have;
					std::vector<unsigned char> in(CHUNK_SIZE);
					std::vector<unsigned char> out(CHUNK_SIZE);
					z_stream strm;

					strm.zalloc = Z_NULL;
					strm.zfree = Z_NULL;
					strm.opaque = Z_NULL;
					strm.avail_in = 0;
					strm.next_in = Z_NULL;
					ret = inflateInit(&strm);

					// exit if something is wrong
					if (ret != Z_OK) throw ibrcommon::Exception("initialization of zlib failed");

					do {
						is.read((char*)&in[0], CHUNK_SIZE);
						strm.avail_in = static_cast<uInt>(is.gcount());

						// we're done if there is no more input
						if ((strm.avail_in == 0) && (ret != Z_STREAM_END))
						{
							(void)inflateEnd(&strm);
							throw ibrcommon::Exception("decompression failed. no enough data available.");
						}
						strm.next_in = &in[0];

						do {
							strm.avail_out = CHUNK_SIZE;
							strm.next_out = &out[0];

							ret = inflate(&strm, Z_NO_FLUSH);
							assert(ret != Z_STREAM_ERROR);  /* state not clobbered */

							switch (ret)
							{
								case Z_NEED_DICT:
									ret = Z_DATA_ERROR;     /* and fall through */
								case Z_DATA_ERROR:
								case Z_MEM_ERROR:
									(void)inflateEnd(&strm);
									throw ibrcommon::Exception("decompression failed. memory error.");
							}

							// determine how many bytes are available
							have = CHUNK_SIZE - strm.avail_out;

							// write the buffer to the output stream
							os.write((char*)&out[0], have);


							if (!os.good())
							{
								(void)inflateEnd(&strm);
								throw ibrcommon::Exception("decompression failed. output stream went wrong.");
							}

						} while (strm.avail_out == 0);
						assert(strm.avail_in == 0);     /* all input will be used */
					} while (ret != Z_STREAM_END);

					(void)inflateEnd(&strm);
#else
					throw ibrcommon::Exception("zlib is not supported");
#endif
					break;
				}

				default:
					throw ibrcommon::Exception("compression mode is not supported");
			}
		}
	}
}
