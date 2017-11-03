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

#include "streaming/BundleStreamBuf.h"
#include <ibrdtn/data/StreamBlock.h>
#include <ibrcommon/TimeMeasurement.h>

BundleStreamBuf::BundleStreamBuf(dtn::api::Client &client, StreamBundle &chunk, size_t min_buffer, size_t max_buffer, bool wait_seq_zero)
 : _in_buf(min_buffer), _out_buf(min_buffer), _client(client), _chunk(chunk),
   _min_buf_size(min_buffer), _max_buf_size(max_buffer), _chunk_offset(0), _in_seq(0),
   _streaming(wait_seq_zero), _request_ack(false), _flush_request(false), _receive_timeout(0)
{
	// Initialize get pointer.  This should be zero so that underflow is called upon first read.
	setg(0, 0, 0);
	setp(&_in_buf[0], &_in_buf[__get_next_buffer_size() - 1]);
}

BundleStreamBuf::~BundleStreamBuf()
{
}

/**
 * Flush the buffer and send out the buffered bundle
 */
void BundleStreamBuf::flush()
{
	// lock the data structures
	ibrcommon::MutexLock l(_chunks_cond);

	if (_chunk.size() == 0) {
		_flush_request = true;
		return;
	}

	__flush();
}

void BundleStreamBuf::setReceiveTimeout(dtn::data::Timeout timeout)
{
	_receive_timeout = timeout;
}

void BundleStreamBuf::__flush()
{
	// request delivery acks
	if (_request_ack) {
		_chunk.set(dtn::data::PrimaryBlock::REQUEST_REPORT_OF_BUNDLE_DELIVERY, true);
		_chunk.reportto = dtn::data::EID("api:me");
	}

	// send the current chunk and clear it
	_client << _chunk; _client.flush();
	_chunk.clear();

	_flush_request = false;
}

size_t BundleStreamBuf::__get_next_buffer_size() const
{
	if ((_chunk.size() + _min_buf_size) <= _max_buf_size)
	{
		return _min_buf_size;
	}
	else
	{
		return _max_buf_size - _chunk.size();
	}
}

int BundleStreamBuf::sync()
{
	int ret = std::char_traits<char>::eq_int_type(this->overflow(
			std::char_traits<char>::eof()), std::char_traits<char>::eof()) ? -1
			: 0;

	flush();

	return ret;
}

void BundleStreamBuf::setRequestAck(bool val)
{
	_request_ack = val;
	_flush_request = val;
}

std::char_traits<char>::int_type BundleStreamBuf::overflow(std::char_traits<char>::int_type c)
{
	char *ibegin = &_in_buf[0];
	char *iend = pptr();

	if (!std::char_traits<char>::eq_int_type(c, std::char_traits<char>::eof()))
	{
		*iend++ = std::char_traits<char>::to_char_type(c);
	}

	// if there is nothing to send, just return
	if ((iend - ibegin) == 0)
	{
		return std::char_traits<char>::not_eof(c);
	}

	// lock the data structures
	ibrcommon::MutexLock l(_chunks_cond);

	// copy data into the bundles payload
	_chunk.append(&_in_buf[0], iend - ibegin);

	// mark the buffer as free
	setp(&_in_buf[0], &_in_buf[__get_next_buffer_size() - 1]);

	// if size exceeds chunk limit, send it
	if ((_chunk.size() >= _max_buf_size) || _flush_request)
	{
		__flush();
	}

	return std::char_traits<char>::not_eof(c);
}

void BundleStreamBuf::received(const dtn::data::Bundle &b)
{
	try {
		// get the stream block of the bundle - drop bundles without it
		const StreamBlock &block = b.find<StreamBlock>();

		// lock the data structures
		ibrcommon::MutexLock l(_chunks_cond);

		// check if the sequencenumber is already received
		if (_in_seq < block.getSequenceNumber()) return;

		// insert the received chunk into the chunk set
		_chunks.insert(Chunk(b));

		// unblock reading processes
		_chunks_cond.signal(true);
	} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) { }
}

std::char_traits<char>::int_type BundleStreamBuf::underflow()
{
	ibrcommon::MutexLock l(_chunks_cond);

	return __underflow();
}

std::char_traits<char>::int_type BundleStreamBuf::__underflow()
{
	// receive chunks until the next sequence number is received
	while (_chunks.empty())
	{
		// wait for the next bundle
		_chunks_cond.wait();
	}

	ibrcommon::TimeMeasurement tm;
	tm.start();

	// while not the right sequence number received -> wait
	while ((_in_seq != (*_chunks.begin())._seq))
	{
		try {
			// wait for the next bundle
			_chunks_cond.wait(1000);
		} catch (const ibrcommon::Conditional::ConditionalAbortException&) { };

		tm.stop();
		if (((_receive_timeout > 0) && (tm.getSeconds() > _receive_timeout)) || !_streaming)
		{
			// skip the missing bundles and proceed with the last received one
			_in_seq = (*_chunks.begin())._seq;

			// set streaming to active
			_streaming = true;
		}
	}

	// get the first chunk in the buffer
	const Chunk &c = (*_chunks.begin());

	dtn::data::Bundle b = c._bundle;
	ibrcommon::BLOB::Reference r = b.find<dtn::data::PayloadBlock>().getBLOB();

	// get stream lock
	ibrcommon::BLOB::iostream stream = r.iostream();

	// jump to the offset position
	(*stream).seekg(_chunk_offset, std::ios::beg);

	// copy the data of the last received bundle into the buffer
	(*stream).read(&_out_buf[0], _out_buf.size());

	// get the read bytes
	size_t bytes = (*stream).gcount();

	if ((*stream).eof())
	{
		// delete the last chunk
		_chunks.erase(c);

		// reset the chunk offset
		_chunk_offset = 0;

		// increment sequence number
		_in_seq++;

		// if no more bytes are read, get the next bundle -> call underflow() recursive
		if (bytes == 0)
		{
			return __underflow();
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

BundleStreamBuf::Chunk::Chunk(const dtn::data::Bundle &b)
 : _bundle(b), _seq(0)
{
	// get the stream block of the bundle - drop bundles without it
	const StreamBlock &block = b.find<StreamBlock>();
	_seq = block.getSequenceNumber();
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
