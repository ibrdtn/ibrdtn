/*
 * BundleStreamBuf.cpp
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

#include "BundleStreamBuf.h"
#include "core/BundleCore.h"
#include <ibrdtn/data/StreamBlock.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/TimeMeasurement.h>

namespace dtn
{
	namespace api
	{
		BundleStreamBuf::BundleStreamBuf(BundleStreamBufCallback &callback, const dtn::data::Length chunk_size, bool wait_seq_zero)
		 : _callback(callback), _in_buf(BUFF_SIZE), _out_buf(BUFF_SIZE),
		   _chunk_size(chunk_size), _chunk_payload(ibrcommon::BLOB::create()), _chunk_offset(0), _in_seq(0),
		   _out_seq(0), _streaming(wait_seq_zero), _first_chunk(true), _last_chunk_received(false), _timeout_receive(0)
		{
			// Initialize get pointer.  This should be zero so that underflow is called upon first read.
			setg(0, 0, 0);
			setp(&_in_buf[0], &_in_buf[BUFF_SIZE - 1]);
		}

		BundleStreamBuf::~BundleStreamBuf()
		{
		}

		int BundleStreamBuf::sync()
		{
			int ret = std::char_traits<char>::eq_int_type(this->overflow(
					std::char_traits<char>::eof()), std::char_traits<char>::eof()) ? -1
					: 0;

			// send the current chunk and clear it
			flushPayload(true);

			return ret;
		}

		std::char_traits<char>::int_type BundleStreamBuf::overflow(std::char_traits<char>::int_type c)
		{
			char *ibegin = &_in_buf[0];
			char *iend = pptr();

			// mark the buffer as free
			setp(&_in_buf[0], &_in_buf[0] + BUFF_SIZE - 1);

			if (!std::char_traits<char>::eq_int_type(c, std::char_traits<char>::eof()))
			{
				*iend++ = std::char_traits<char>::to_char_type(c);
			}

			// if there is nothing to send, just return
			if ((iend - ibegin) == 0)
			{
				return std::char_traits<char>::not_eof(c);
			}

			// copy data into the bundles payload
			BundleStreamBuf::append(_chunk_payload, &_in_buf[0], iend - ibegin);

			// if size exceeds chunk limit, send it
			if (_chunk_payload.size() > static_cast<std::streamsize>(_chunk_size))
			{
				flushPayload();
			}

			return std::char_traits<char>::not_eof(c);
		}

		void BundleStreamBuf::flushPayload(bool final)
		{
			// do not send a bundle if there are no bytes buffered
			// and no bundle has been sent before
			if ((_first_chunk) && (_chunk_payload.size() == 0))
			{
				return;
			}

			// create an empty bundle
			dtn::data::Bundle b;

			dtn::data::StreamBlock &block = b.push_front<dtn::data::StreamBlock>();
			block.setSequenceNumber(_out_seq);
			if (final) block.set(dtn::data::StreamBlock::STREAM_END, true);
			block.set(dtn::data::StreamBlock::STREAM_BEGIN, _first_chunk);
			if (_first_chunk) _first_chunk = false;

			// add tmp payload to the bundle
			b.push_back(_chunk_payload);

			// send the current chunk
			_callback.put(b);

			// and clear the payload
			_chunk_payload = ibrcommon::BLOB::create();

			// increment the sequence number
			_out_seq++;
		}

		void BundleStreamBuf::setChunkSize(const dtn::data::Length &size)
		{
			_chunk_size = size;
		}

		void BundleStreamBuf::setTimeout(const dtn::data::Timeout &timeout)
		{
			_timeout_receive = timeout;
		}

		void BundleStreamBuf::append(ibrcommon::BLOB::Reference &ref, const char* data, const dtn::data::Length &length)
		{
			ibrcommon::BLOB::iostream stream = ref.iostream();
			(*stream).seekp(0, std::ios::end);
			(*stream).write(data, length);
		}

		std::char_traits<char>::int_type BundleStreamBuf::underflow()
		{
			// return with EOF if the last chunk was received
			if (_last_chunk_received)
			{
				return std::char_traits<char>::eof();
			}

			// receive chunks until the next sequence number is received
			while (_chunks.empty())
			{
				// request the next bundle
				dtn::data::MetaBundle b = _callback.get();

				IBRCOMMON_LOGGER_DEBUG_TAG("BundleStreamBuf", 40) << "bundle received" << IBRCOMMON_LOGGER_ENDL;

				// create a chunk object
				Chunk c(b);

				if (c._seq >= _in_seq)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG("BundleStreamBuf", 40) << "bundle accepted, seq. no. " << c._seq << IBRCOMMON_LOGGER_ENDL;
					_chunks.insert(c);
				}
			}

			ibrcommon::TimeMeasurement tm;
			tm.start();

			// while not the right sequence number received -> wait
			while (_in_seq != (*_chunks.begin())._seq)
			{
				try {
					// request the next bundle
					dtn::data::MetaBundle b = _callback.get(_timeout_receive);
					IBRCOMMON_LOGGER_DEBUG_TAG("BundleStreamBuf", 40) << "bundle received" << IBRCOMMON_LOGGER_ENDL;

					// create a chunk object
					Chunk c(b);

					if (c._seq >= _in_seq)
					{
						IBRCOMMON_LOGGER_DEBUG_TAG("BundleStreamBuf", 40) << "bundle accepted, seq. no. " << c._seq << IBRCOMMON_LOGGER_ENDL;
						_chunks.insert(c);
					}
				} catch (std::exception&) {
					// timed out
				}

				tm.stop();
				if (((_timeout_receive > 0) && (tm.getSeconds() > _timeout_receive)) || !_streaming)
				{
					// skip the missing bundles and proceed with the last received one
					_in_seq = (*_chunks.begin())._seq;

					// set streaming to active
					_streaming = true;
				}
			}

			IBRCOMMON_LOGGER_DEBUG_TAG("BundleStreamBuf", 40) << "read the payload" << IBRCOMMON_LOGGER_ENDL;

			// get the first chunk in the buffer
			const Chunk &c = (*_chunks.begin());

			if (c._meta != _current_bundle)
			{
				// load the bundle from storage
				dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();
				_current_bundle = storage.get(c._meta);

				// process the bundle block (security, compression, ...)
				dtn::core::BundleCore::processBlocks(_current_bundle);
			}

			const dtn::data::PayloadBlock &payload = _current_bundle.find<dtn::data::PayloadBlock>();
			ibrcommon::BLOB::Reference r = payload.getBLOB();

			bool end_of_stream = false;
			std::streamsize bytes = 0;

			// lock the stream while reading from it
			{
				// get stream lock
				ibrcommon::BLOB::iostream stream = r.iostream();

				// jump to the offset position
				(*stream).seekg(_chunk_offset, std::ios::beg);

				// copy the data of the last received bundle into the buffer
				(*stream).read(&_out_buf[0], BUFF_SIZE);

				// get the read bytes
				bytes = (*stream).gcount();

				// check for end of stream
				end_of_stream = (*stream).eof();
			}

			if (end_of_stream)
			{
				// bundle consumed
		//		std::cerr << std::endl << "# " << c._seq << std::endl << std::flush;

				// check if this was the last chunk
				if (c._last)
				{
					_last_chunk_received = true;
				}

				// set bundle as delivered
				_callback.delivered(c._meta);

				// delete the last chunk
				_chunks.erase(c);

				// reset the chunk offset
				_chunk_offset = 0;

				// increment sequence number
				_in_seq++;

				// if no more bytes are read, get the next bundle -> call underflow() recursive
				if (bytes == 0)
				{
					return underflow();
				}
			}
			else
			{
				// increment the chunk offset
				_chunk_offset += bytes;
			}

			// Since the input buffer content is now valid (or is new)
			// the get pointer should be initialized (or reset).
			setg(&_out_buf[0], &_out_buf[0], &_out_buf[0] + bytes);

			return std::char_traits<char>::not_eof(_out_buf[0]);
		}

		BundleStreamBuf::Chunk::Chunk(const dtn::data::MetaBundle &m)
		 : _meta(m), _seq(0), _first(false), _last(false)
		{
			dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();
			dtn::data::Bundle bundle = storage.get(_meta);

			try {
				const dtn::data::StreamBlock &block = bundle.find<dtn::data::StreamBlock>();
				_seq = block.getSequenceNumber();
				_first = block.get(dtn::data::StreamBlock::STREAM_BEGIN);
				_last = block.get(dtn::data::StreamBlock::STREAM_END);
			} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) { }
		}

		BundleStreamBuf::Chunk::~Chunk()
		{
		}

		bool BundleStreamBuf::Chunk::operator==(const Chunk& other) const
		{
			return (_seq == other._seq);
		}

		bool BundleStreamBuf::Chunk::operator<(const Chunk& other) const
		{
			return (_seq < other._seq);
		}
	} /* namespace data */
} /* namespace dtn */
