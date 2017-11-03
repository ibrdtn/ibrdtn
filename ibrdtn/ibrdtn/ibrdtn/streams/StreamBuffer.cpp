/*
 * bpstreambuf.cpp
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

#include "ibrdtn/streams/StreamConnection.h"
#include <ibrcommon/Logger.h>
#include <ibrcommon/TimeMeasurement.h>
#include <vector>

namespace dtn
{
	namespace streams
	{
		StreamConnection::StreamBuffer::StreamBuffer(StreamConnection &conn, std::iostream &stream, const dtn::data::Length buffer_size)
			: _buffer_size(buffer_size), _statebits(STREAM_SOB), _conn(conn), in_buf_(buffer_size), out_buf_(buffer_size), _stream(stream),
			  _recv_size(0), _underflow_data_remain(0), _underflow_state(IDLE), _idle_timer(*this, 0)
		{
			// Initialize get pointer.  This should be zero so that underflow is called upon first read.
			setg(0, 0, 0);
			setp(&out_buf_[0], &out_buf_[0] + _buffer_size - 1);
		}

		StreamConnection::StreamBuffer::~StreamBuffer()
		{
			// stop the idle timer
			_idle_timer.stop();
		}

		bool StreamConnection::StreamBuffer::get(const StateBits bit) const
		{
			return (_statebits & bit);
		}

		void StreamConnection::StreamBuffer::set(const StateBits bit)
		{
			ibrcommon::MutexLock l(_statelock);
			_statebits |= bit;
		}

		void StreamConnection::StreamBuffer::unset(const StateBits bit)
		{
			ibrcommon::MutexLock l(_statelock);
			_statebits &= ~(bit);
		}

		void StreamConnection::StreamBuffer::__error() const
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "StreamBuffer Debugging" << IBRCOMMON_LOGGER_ENDL;
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "---------------------------------------" << IBRCOMMON_LOGGER_ENDL;
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "Buffer size: " << _buffer_size << IBRCOMMON_LOGGER_ENDL;
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "State bits: " << _statebits << IBRCOMMON_LOGGER_ENDL;
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "Recv size: " << _recv_size.toString() << IBRCOMMON_LOGGER_ENDL;
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "Segments: " << _segments.size() << IBRCOMMON_LOGGER_ENDL;
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "Reject segments: " << _rejected_segments.size() << IBRCOMMON_LOGGER_ENDL;
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "Underflow remaining: " << _underflow_data_remain << IBRCOMMON_LOGGER_ENDL;
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "Underflow state: " << _underflow_state << IBRCOMMON_LOGGER_ENDL;
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "---------------------------------------" << IBRCOMMON_LOGGER_ENDL;

			if (_statebits & STREAM_FAILED)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "stream went bad: STREAM_FAILED is set" << IBRCOMMON_LOGGER_ENDL;
			}

			if (_statebits & STREAM_BAD)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "stream went bad: STREAM_BAD is set" << IBRCOMMON_LOGGER_ENDL;
			}

			if (_statebits & STREAM_EOF)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "stream went bad: STREAM_EOF is set" << IBRCOMMON_LOGGER_ENDL;
			}

			if (_statebits & STREAM_SHUTDOWN)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "stream went bad: STREAM_SHUTDOWN is set" << IBRCOMMON_LOGGER_ENDL;
			}

			if (_statebits & STREAM_CLOSED)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "stream went bad: STREAM_CLOSED is set" << IBRCOMMON_LOGGER_ENDL;
			}

			if (!_stream.good())
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 80) << "stream went bad: good() returned false" << IBRCOMMON_LOGGER_ENDL;
			}
		}

		bool StreamConnection::StreamBuffer::__good() const
		{
			int badbits = STREAM_FAILED | STREAM_BAD | STREAM_EOF | STREAM_SHUTDOWN | STREAM_CLOSED;
			return !(badbits & _statebits);
		}

		/**
		 * This method do a handshake with the peer including send and receive
		 * the contact header and set the IN/OUT timer. If the handshake was not
		 * successful a SHUTDOWN message is sent, the eventShutdown() is called and
		 * a exception is thrown.
		 * @param header
		 * @return
		 */
		const StreamContactHeader StreamConnection::StreamBuffer::handshake(const StreamContactHeader &header)
		{
			StreamContactHeader peer;

			try {
				// make the send-call atomic
				{
					ibrcommon::MutexLock l(_sendlock);

					// transfer the local header
					_stream << header << std::flush;
				}

				// receive the remote header
				_stream >> peer;

				// enable/disable ACK/NACK support
				if (peer._flags.getBit(StreamContactHeader::REQUEST_ACKNOWLEDGMENTS)) set(STREAM_ACK_SUPPORT);
				if (peer._flags.getBit(StreamContactHeader::REQUEST_NEGATIVE_ACKNOWLEDGMENTS)) set(STREAM_NACK_SUPPORT);

				// set the incoming timer if set (> 0)
				if (peer._keepalive > 0)
				{
					// mark timer support
					set(STREAM_TIMER_SUPPORT);
				}

				// set handshake completed bit
				set(STREAM_HANDSHAKE);

			} catch (const std::exception&) {
				// set failed bit
				set(STREAM_FAILED);

				// shutdown the stream
				shutdown(StreamDataSegment::MSG_SHUTDOWN_VERSION_MISSMATCH);

				// call the shutdown event
				_conn.shutdown(CONNECTION_SHUTDOWN_ERROR);

				// forward the catched exception
				throw StreamErrorException("handshake not completed");
			}

			// return the received header
			return peer;
		}

		/**
		 * This method is called by the super class and shutdown the hole
		 * connection. While we have no access to the connection itself this
		 * method send a SHUTDOWN message and set this connection as closed.
		 */
		void StreamConnection::StreamBuffer::shutdown(const StreamDataSegment::ShutdownReason reason)
		{
			try {
				ibrcommon::MutexLock l(_sendlock);
				// send a SHUTDOWN message
				_stream << StreamDataSegment(reason) << std::flush;
			} catch (const std::exception&) {
				// set failed bit
				set(STREAM_FAILED);

				throw StreamErrorException("can not send shutdown message");
			}
		}

		void StreamConnection::StreamBuffer::keepalive()
		{
			try {
				try {
					ibrcommon::MutexTryLock l(_sendlock);
					_stream << StreamDataSegment() << std::flush;
					IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 15) << "KEEPALIVE sent" << IBRCOMMON_LOGGER_ENDL;
				} catch (const ibrcommon::MutexException&) {
					// could not grab the lock - another process is sending something
					// then we do nothing since a data frame do the same as a keepalive frame.
				};
			} catch (const std::exception&) {
				// set failed bit
				set(STREAM_FAILED);
			}
		}

		void StreamConnection::StreamBuffer::close()
		{
			// set shutdown bit
			set(STREAM_SHUTDOWN);
		}

		void StreamConnection::StreamBuffer::reject()
		{
			// we have to reject the current transmission
			// so we have to discard all all data until the next segment with a start bit
			set(STREAM_REJECT);

			// set the current in buffer to zero
			// this should result in a underflow call on the next read
			setg(0, 0, 0);
		}

		void StreamConnection::StreamBuffer::abort()
		{
			_segments.abort();
		}

		void StreamConnection::StreamBuffer::wait()
		{
			// TODO: get max time to wait out of the timeout values
			dtn::data::Timeout timeout = 0;

			try {
				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 15) << "waitCompleted(): wait for completion of transmission, " << _segments.size() << " ACKs left" << IBRCOMMON_LOGGER_ENDL;
				_segments.wait(ibrcommon::Queue<StreamDataSegment>::QUEUE_EMPTY, timeout);
				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 15) << "waitCompleted(): transfer completed" << IBRCOMMON_LOGGER_ENDL;
			} catch (const ibrcommon::QueueUnblockedException&) {
				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 15) << "waitCompleted(): transfer aborted (timeout)" << IBRCOMMON_LOGGER_ENDL;
			}
		}

		// This function is called when the output buffer is filled.
		// In this function, the buffer should be written to wherever it should
		// be written to (in this case, the streambuf object that this is controlling).
		std::char_traits<char>::int_type StreamConnection::StreamBuffer::overflow(std::char_traits<char>::int_type c)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 90) << "overflow() called" << IBRCOMMON_LOGGER_ENDL;

			try {
				char *ibegin = &out_buf_[0];
				char *iend = pptr();

				// mark the buffer as free
				setp(&out_buf_[0], &out_buf_[0] + _buffer_size - 1);

				// append the last character
				if(!traits_type::eq_int_type(c, traits_type::eof())) {
					*iend++ = traits_type::to_char_type(c);
				}

				// if there is nothing to send, just return
				if ((iend - ibegin) == 0)
				{
					return traits_type::not_eof(c);
				}

				// wrap a segment around the data
				StreamDataSegment seg(StreamDataSegment::MSG_DATA_SEGMENT, (iend - ibegin));

				// set the start flag
				if (get(STREAM_SOB))
				{
					seg._flags |= StreamDataSegment::MSG_MARK_BEGINN;
					unset(STREAM_SKIP);
					unset(STREAM_SOB);
				}

				if (std::char_traits<char>::eq_int_type(c, std::char_traits<char>::eof()))
				{
					// set the end flag
					seg._flags |= StreamDataSegment::MSG_MARK_END;
					set(STREAM_SOB);
				}

				if (!get(STREAM_SKIP))
				{
					// put the segment into the queue
					if (get(STREAM_ACK_SUPPORT))
					{
						_segments.push(seg);
					}
					else if (seg._flags & StreamDataSegment::MSG_MARK_END)
					{
						// without ACK support we have to assume that a bundle is forwarded
						// when the last segment is sent.
						_conn.eventBundleForwarded();
					}

					ibrcommon::MutexLock l(_sendlock);
					if (!_stream.good()) throw StreamErrorException("stream went bad");

					// write the segment to the stream
					_stream << seg;
					_stream.write(&out_buf_[0], seg._value.get<size_t>());

					// record statistics
					_conn._callback.addTrafficOut(seg._value.get<size_t>());
				}

				return traits_type::not_eof(c);
			} catch (const StreamClosedException&) {
				// set failed bit
				set(STREAM_FAILED);

				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 10) << "StreamClosedException in overflow()" << IBRCOMMON_LOGGER_ENDL;

				throw;
			} catch (const StreamErrorException&) {
				// set failed bit
				set(STREAM_FAILED);

				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 10) << "StreamErrorException in overflow()" << IBRCOMMON_LOGGER_ENDL;

				throw;
			} catch (const ios_base::failure&) {
				// set failed bit
				set(STREAM_FAILED);

				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 10) << "ios_base::failure in overflow()" << IBRCOMMON_LOGGER_ENDL;

				throw;
			}

			return traits_type::eof();
		}

		// This is called to flush the buffer.
		// This is called when we're done with the file stream (or when .flush() is called).
		int StreamConnection::StreamBuffer::sync()
		{
			int ret = traits_type::eq_int_type(this->overflow(traits_type::eof()),
											traits_type::eof()) ? -1 : 0;

			try {
				ibrcommon::MutexLock l(_sendlock);

				// ... and flush.
				_stream.flush();
			} catch (const ios_base::failure&) {
				// set failed bit
				set(STREAM_BAD);

				_conn.shutdown(CONNECTION_SHUTDOWN_ERROR);
			}

			return ret;
		}

		void StreamConnection::StreamBuffer::skipData(dtn::data::Length &size)
		{
			// a temporary buffer
			std::vector<char> tmpbuf(_buffer_size);

			try {
				//  and read until the next segment
				while (size > 0 && _stream.good())
				{
					dtn::data::Length readsize = _buffer_size;
					if (size < _buffer_size) readsize = size;

					// to reject a bundle read all remaining data of this segment
					_stream.read(&tmpbuf[0], (std::streamsize)readsize);

					// record statistics
					_conn._callback.addTrafficIn(readsize);

					// reset idle timeout
					_idle_timer.reset();

					// adjust the remain counter
					size -= readsize;
				}
			} catch (const ios_base::failure &ex) {
				_underflow_state = IDLE;
				throw StreamErrorException("read error during data skip: " + std::string(ex.what()));
			}
		}

		// Fill the input buffer.  This reads out of the streambuf.
		std::char_traits<char>::int_type StreamConnection::StreamBuffer::underflow()
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 90) << "StreamBuffer::underflow() called" << IBRCOMMON_LOGGER_ENDL;

			try {
				if (_underflow_state == DATA_TRANSFER)
				{
					// on bundle reject
					if (get(STREAM_REJECT))
					{
						// send NACK on bundle reject
						if (get(STREAM_NACK_SUPPORT))
						{
							ibrcommon::MutexLock l(_sendlock);
							if (!_stream.good()) throw StreamErrorException("stream went bad");

							// send a REFUSE message
							_stream << StreamDataSegment(StreamDataSegment::MSG_REFUSE_BUNDLE) << std::flush;
						}

						// skip data in this segment
						skipData(_underflow_data_remain);

						// return to idle state
						_underflow_state = IDLE;
					}
					// send ACK if the data segment is received completely
					else if (_underflow_data_remain == 0)
					{
						// New data segment received. Send an ACK.
						if (get(STREAM_ACK_SUPPORT))
						{
							ibrcommon::MutexLock l(_sendlock);
							if (!_stream.good()) throw StreamErrorException("stream went bad");
							_stream << StreamDataSegment(StreamDataSegment::MSG_ACK_SEGMENT, _recv_size) << std::flush;
						}

						// return to idle state
						_underflow_state = IDLE;
					}
				}

				// read segments until DATA is AVAILABLE
				while (_underflow_state == IDLE)
				{
					// container for segment data
					dtn::streams::StreamDataSegment seg;

					try {
						// read the segment
						if (!_stream.good()) throw StreamErrorException("stream went bad");

						_stream >> seg;
					} catch (const ios_base::failure &ex) {
						throw StreamErrorException("read error: " + std::string(ex.what()));
					}

					if (seg._type != StreamDataSegment::MSG_KEEPALIVE)
					{
						// reset idle timeout
						_idle_timer.reset();
					}

					switch (seg._type)
					{
						case StreamDataSegment::MSG_DATA_SEGMENT:
						{
							IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 70) << "MSG_DATA_SEGMENT received, size: " << seg._value.toString() << IBRCOMMON_LOGGER_ENDL;

							if (seg._flags & StreamDataSegment::MSG_MARK_BEGINN)
							{
								_recv_size = seg._value;
								unset(STREAM_REJECT);
							}
							else
							{
								_recv_size += seg._value;
							}

							// set the new data length
							_underflow_data_remain = seg._value.get<Length>();

							if (get(STREAM_REJECT))
							{
								// send NACK on bundle reject
								if (get(STREAM_NACK_SUPPORT))
								{
									// lock for sending
									ibrcommon::MutexLock l(_sendlock);
									if (!_stream.good()) throw StreamErrorException("stream went bad");

									// send a NACK message
									_stream << StreamDataSegment(StreamDataSegment::MSG_REFUSE_BUNDLE, 0) << std::flush;
								}

								// skip data in this segment
								skipData(_underflow_data_remain);
							}
							else
							{
								// announce the new data block
								_underflow_state = DATA_TRANSFER;
							}
							break;
						}

						case StreamDataSegment::MSG_ACK_SEGMENT:
						{
							IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 70) << "MSG_ACK_SEGMENT received, size: " << seg._value.toString() << IBRCOMMON_LOGGER_ENDL;

							// remove the segment in the queue
							if (get(STREAM_ACK_SUPPORT))
							{
								ibrcommon::Queue<StreamDataSegment>::Locked q = _segments.exclusive();
								if (q.empty())
								{
									IBRCOMMON_LOGGER_TAG("StreamBuffer", error) << "got an unexpected ACK with size of " << seg._value.toString() << IBRCOMMON_LOGGER_ENDL;
								}
								else
								{
									StreamDataSegment &qs = q.front();

									IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 60) << q.size() << " elements to ACK" << IBRCOMMON_LOGGER_ENDL;

									_conn.eventBundleAck(seg._value.get<Length>());

									if (qs._flags & StreamDataSegment::MSG_MARK_END)
									{
										_conn.eventBundleForwarded();
									}

									q.pop();
								}
							}
							break;
						}

						case StreamDataSegment::MSG_KEEPALIVE:
							IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 70) << "MSG_KEEPALIVE received, size: " << seg._value.toString() << IBRCOMMON_LOGGER_ENDL;
							break;

						case StreamDataSegment::MSG_REFUSE_BUNDLE:
						{
							IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 70) << "MSG_REFUSE_BUNDLE received, flags: " << (int)seg._flags << IBRCOMMON_LOGGER_ENDL;

							// TODO: Test bundle rejection!

							// remove the segment in the queue
							if (get(STREAM_ACK_SUPPORT) && get(STREAM_NACK_SUPPORT))
							{
								// skip segments
								if (!_rejected_segments.empty())
								{
									_rejected_segments.pop();

									// we received a NACK
									IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 30) << "NACK received, still " << _rejected_segments.size() << " segments to NACK" << IBRCOMMON_LOGGER_ENDL;
								}
								else try
								{
									StreamDataSegment qs = _segments.take();

									// we received a NACK
									IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 20) << "NACK received!" << IBRCOMMON_LOGGER_ENDL;

									// get all segment ACKs in the queue for this transmission
									while (!_segments.empty())
									{
										StreamDataSegment &seg = _segments.front();
										if (seg._flags & StreamDataSegment::MSG_MARK_BEGINN)
										{
											break;
										}

										// move the segments to another queue
										_rejected_segments.push(seg);
										_segments.pop();
									}

									// call event reject
									_conn.eventBundleRefused();

									// we received a NACK
									IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 30) << _rejected_segments.size() << " segments to NACK" << IBRCOMMON_LOGGER_ENDL;

									// the queue is empty, then skip the current transfer
									if (_segments.empty())
									{
										set(STREAM_SKIP);

										// we received a NACK
										IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 25) << "skip the current transfer" << IBRCOMMON_LOGGER_ENDL;
									}

								} catch (const ibrcommon::QueueUnblockedException&) {
									IBRCOMMON_LOGGER_TAG("StreamBuffer", error) << "got an unexpected NACK" << IBRCOMMON_LOGGER_ENDL;
								}

							}
							else
							{
								IBRCOMMON_LOGGER_TAG("StreamBuffer", error) << "got an unexpected NACK" << IBRCOMMON_LOGGER_ENDL;
							}

							break;
						}

						case StreamDataSegment::MSG_SHUTDOWN:
						{
							IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 70) << "MSG_SHUTDOWN received" << IBRCOMMON_LOGGER_ENDL;
							throw StreamShutdownException();
						}
					}
				}

				// currently transferring data
				dtn::data::Length readsize = _buffer_size;
				if (_underflow_data_remain < _buffer_size) readsize = _underflow_data_remain;

				try {
					if (!_stream.good()) throw StreamErrorException("stream went bad");

					// here receive the data
					_stream.read(&in_buf_[0], (std::streamsize)readsize);

					// record statistics
					_conn._callback.addTrafficIn(readsize);

					// reset idle timeout
					_idle_timer.reset();
				} catch (const ios_base::failure &ex) {
					_underflow_state = IDLE;
					throw StreamErrorException("read error: " + std::string(ex.what()));
				}

				// adjust the remain counter
				_underflow_data_remain -= readsize;

				// Since the input buffer content is now valid (or is new)
				// the get pointer should be initialized (or reset).
				setg(&in_buf_[0], &in_buf_[0], &in_buf_[0] + readsize);

				return traits_type::not_eof(in_buf_[0]);

			} catch (const StreamClosedException&) {
				// set failed bit
				set(STREAM_FAILED);

				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 10) << "StreamClosedException in underflow()" << IBRCOMMON_LOGGER_ENDL;

			} catch (const StreamErrorException &ex) {
				// set failed bit
				set(STREAM_FAILED);

				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 10) << "StreamErrorException in underflow(): " << ex.what() << IBRCOMMON_LOGGER_ENDL;

				throw;
			} catch (const StreamShutdownException&) {
				// set failed bit
				set(STREAM_FAILED);

				IBRCOMMON_LOGGER_DEBUG_TAG("StreamBuffer", 10) << "StreamShutdownException in underflow()" << IBRCOMMON_LOGGER_ENDL;
			}

			return traits_type::eof();
		}

		size_t StreamConnection::StreamBuffer::timeout(ibrcommon::Timer*)
		{
			if (__good())
			{
				shutdown(StreamDataSegment::MSG_SHUTDOWN_IDLE_TIMEOUT);
			}
			throw ibrcommon::Timer::StopTimerException();
		}

		void StreamConnection::StreamBuffer::enableIdleTimeout(const dtn::data::Timeout &seconds)
		{
			_idle_timer.set(seconds);
			_idle_timer.start();
		}
	}
}
