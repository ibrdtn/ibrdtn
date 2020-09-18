/*
 * DatagramConnection.cpp
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

#include "Configuration.h"
#include "net/DatagramConnection.h"
#include "net/BundleReceivedEvent.h"
#include "core/BundleEvent.h"
#include "net/TransferAbortedEvent.h"
#include "core/BundleCore.h"

#include <ibrdtn/utils/Utils.h>
#include <ibrdtn/data/Serializer.h>

#include <ibrcommon/TimeMeasurement.h>
#include <ibrcommon/Logger.h>
#include <string.h>

#include <iomanip>

#define AVG_RTT_WEIGHT 0.875

namespace dtn
{
	namespace net
	{
		const std::string DatagramConnection::TAG = "DatagramConnection";

		DatagramConnection::DatagramConnection(const std::string &identifier, const DatagramService::Parameter &params, DatagramConnectionCallback &callback)
		 : _send_state(SEND_IDLE), _recv_state(RECV_IDLE), _callback(callback), _identifier(identifier), _stream(*this, params.max_msg_length), _sender(*this, _stream),
		   _last_ack(0), _next_seqno(0), _head_buf(params.max_msg_length), _head_len(0), _params(params), _avg_rtt(static_cast<double>(params.initial_timeout))
		{
		}

		DatagramConnection::~DatagramConnection()
		{
			// do not destroy this instance as long as
			// the sender thread is running
			_sender.join();

			// join ourself
			join();
		}

		void DatagramConnection::shutdown()
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 40) << "shutdown()" << IBRCOMMON_LOGGER_ENDL;

			// close the stream
			__cancellation();
		}

		void DatagramConnection::__cancellation() noexcept
		{
			// close the stream
			try {
				_stream.close();
			} catch (const ibrcommon::Exception&) { };
		}

		void DatagramConnection::run() noexcept
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 40) << "run()" << IBRCOMMON_LOGGER_ENDL;

			// create a filter context
			dtn::core::FilterContext context;
			context.setPeer(_peer_eid);
			context.setProtocol(_callback.getDiscoveryProtocol());

			// create a deserializer for the stream
			dtn::data::DefaultDeserializer deserializer(_stream, dtn::core::BundleCore::getInstance());

			try {
				while(_stream.good())
				{
					try {
						dtn::data::Bundle bundle;

						// read the bundle out of the stream
						deserializer >> bundle;

						// push bundle through the filter routines
						context.setBundle(bundle);
						BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::INPUT, context, bundle);

						switch (ret) {
							case BundleFilter::ACCEPT:
								// raise default bundle received event
								dtn::net::BundleReceivedEvent::raise(_peer_eid, bundle, false);
								break;

							case BundleFilter::REJECT:
								throw dtn::data::Validator::RejectedException("rejected by input filter");
								break;

							case BundleFilter::DROP:
								break;
						}
					} catch (const dtn::data::Validator::RejectedException &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 25) << "Bundle rejected: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

						// TODO: send NACK
						_stream.reject();
					} catch (const dtn::InvalidDataException &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 25) << "Received an invalid bundle: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

						// TODO: send NACK
						_stream.reject();
					}
				}
			} catch (std::exception &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 25) << "Main-thread died: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void DatagramConnection::setup() noexcept
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 40) << "setup()" << IBRCOMMON_LOGGER_ENDL;

			_callback.connectionUp(this);
			_sender.start();
		}

		void DatagramConnection::finally() noexcept
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 40) << "finally()" << IBRCOMMON_LOGGER_ENDL;

			try {
				ibrcommon::MutexLock l(_ack_cond);
				_ack_cond.abort();
			} catch (const std::exception&) { };

			try {
				// shutdown the sender thread
				_sender.stop();

				// wait until all operations are stopped
				_sender.join();
			} catch (const std::exception&) { };

			try {
				// remove this connection from the connection list
				_callback.connectionDown(this);
			} catch (const ibrcommon::MutexException&) { };
		}

		const std::string& DatagramConnection::getIdentifier() const
		{
			return _identifier;
		}

		/**
		 * Queue job for delivery to another node
		 * @param job
		 */
		void DatagramConnection::queue(const dtn::net::BundleTransfer &job)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 15) << "queue bundle " << job.getBundle().toString() << " to " << job.getNeighbor().getString() << IBRCOMMON_LOGGER_ENDL;
			_sender.queue.push(job);
		}

		/**
		 * queue data for delivery to the stream
		 * @param buf
		 * @param len
		 */
		void DatagramConnection::queue(const char &flags, const unsigned int &seqno, const char *buf, const dtn::data::Length &len)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 25) << "frame received, flags: " << (int)flags << ", seqno: " << seqno << ", len: " << len << IBRCOMMON_LOGGER_ENDL;

			try {
				// we will accept every sequence number on first segments
				// if this is not the first segment
				if (!(flags & DatagramService::SEGMENT_FIRST))
				{
					// if the sequence number is not expected
					if (_next_seqno != seqno)
						// then drop it and send an ack
						throw WrongSeqNoException(_next_seqno);
				}

				// if this is the last segment then...
				if ((flags & DatagramService::SEGMENT_FIRST) && (flags & DatagramService::SEGMENT_LAST))
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 45) << "full segment received" << IBRCOMMON_LOGGER_ENDL;

					// forward the last segment to the stream
					_stream.queue(buf, len, true);

					// switch to IDLE state
					_recv_state = RECV_IDLE;
				}
				else if (flags & DatagramService::SEGMENT_FIRST)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 45) << "first segment received" << IBRCOMMON_LOGGER_ENDL;

					// the first segment is only allowed on IDLE state or on
					// retransmissions due to lost ACKs
					if (_recv_state == RECV_IDLE)
					{
						// first segment received
						// store the segment in a buffer
						::memcpy(&_head_buf[0], buf, len);
						_head_len = len;

						// enter the HEAD state
						_recv_state = RECV_HEAD;
					}
					else if (_recv_state == RECV_HEAD)
					{
						// last ACK seams to be lost or the peer has been restarted after
						// sending the first segment
						// overwrite the buffer with the new segment
						::memcpy(&_head_buf[0], buf, len);
						_head_len = len;
					}
					else
					{
						// failure - abort the stream
						throw DatagramException("stream went inconsistent");
					}
				}
				else
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 45) << ((flags & DatagramService::SEGMENT_LAST) ? "last" : "middle") << " segment received" << IBRCOMMON_LOGGER_ENDL;

					// this is one segment after the HEAD flush the buffers
					if (_recv_state == RECV_HEAD)
					{
						// forward HEAD buffer to the stream
						_stream.queue(&_head_buf[0], _head_len, true);
						_head_len = 0;

						// switch to TRANSMISSION state
						_recv_state = RECV_TRANSMISSION;
					}

					// forward the current segment to the stream
					_stream.queue(buf, len, false);

					if (flags & DatagramService::SEGMENT_LAST)
					{
						// switch to IDLE state
						_recv_state = RECV_IDLE;
					}
				}

				// increment next sequence number
				_next_seqno = (seqno + 1) % _params.max_seq_numbers;
			} catch (const WrongSeqNoException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 15) << "sequence number received " << seqno << ", expected " << ex.expected_seqno << IBRCOMMON_LOGGER_ENDL;
			}

			if (_params.flowcontrol != DatagramService::FLOW_NONE)
			{
				// send ack for this message
				_callback.callback_ack(*this, _next_seqno, getIdentifier());
			}
		}

		void DatagramConnection::stream_send(const char *buf, const dtn::data::Length &len, bool last) noexcept (false)
		{
			// build the right flags
			char flags = 0;

			// if this is the first segment, then set the FIRST bit
			if (_send_state == SEND_IDLE) flags |= DatagramService::SEGMENT_FIRST;

			// if this is the last segment, then set the LAST bit
			if (last) flags |= DatagramService::SEGMENT_LAST;

			// set the seqno for this segment
			unsigned int seqno = _last_ack;

			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 25) << "frame to send, flags: " << (int)flags << ", seqno: " << seqno << ", len: " << len << IBRCOMMON_LOGGER_ENDL;

			if (_params.flowcontrol == DatagramService::FLOW_STOPNWAIT)
			{
				// measure the time until the ack is received
				ibrcommon::TimeMeasurement tm;

				// start time measurement
				tm.start();

				// max. 5 retries
				for (size_t i = 0; i < _params.retry_limit; ++i)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 30) << "transmit frame seqno: " << seqno << IBRCOMMON_LOGGER_ENDL;

					// send the datagram
					_callback.callback_send(*this, flags, seqno, getIdentifier(), buf, len);

					// enter the wait state
					_send_state = SEND_WAIT_ACK;

					// set timeout to twice the average round-trip-time
					struct timespec ts;
					ibrcommon::Conditional::gettimeout(static_cast<size_t>(_avg_rtt * 2) + 1, &ts);

					try {
						ibrcommon::MutexLock l(_ack_cond);

						// wait here for an ACK
						while (_last_ack != ((seqno + 1) % _params.max_seq_numbers))
						{
							_ack_cond.wait(&ts);
						}

						// stop the measurement
						tm.stop();

						// success!
						_send_state = last ? SEND_IDLE : SEND_NEXT;

						// adjust the average rtt
						adjust_rtt(tm.getMilliseconds());

						// report result
						_callback.reportSuccess(i, tm.getMilliseconds());

						return;
					} catch (const ibrcommon::Conditional::ConditionalAbortException &e) {
						if (e.reason == ibrcommon::Conditional::ConditionalAbortException::COND_TIMEOUT)
						{
							IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 20) << "ack timeout for seqno " << seqno << IBRCOMMON_LOGGER_ENDL;

							// fail -> increment the future timeout
							adjust_rtt(static_cast<double>(_avg_rtt) * 2);

							// retransmit the frame
							continue;
						}
						else
						{
							// aborted
							break;
						}
					}
				}

				// maximum number of retransmissions hit
				_send_state = SEND_ERROR;

				// report failure
				_callback.reportFailure();

				// transmission failed - abort the stream
				throw DatagramException("transmission failed - abort the stream");
			}
			else if (_params.flowcontrol == DatagramService::FLOW_SLIDING_WINDOW)
			{
				try {
					// lock the ACK variables and frame window
					ibrcommon::MutexLock l(_ack_cond);

					// timeout value
					struct timespec ts;

					// set timeout to twice the average round-trip-time
					ibrcommon::Conditional::gettimeout(static_cast<size_t>(_avg_rtt * 2) + 1, &ts);

					// wait until window has at least one free slot
					while (sw_frames_full()) _ack_cond.wait(&ts);

					// add new frame to the window
					_sw_frames.push_back(window_frame());

					window_frame &new_frame = _sw_frames.back();

					new_frame.flags = flags;
					new_frame.seqno = seqno;
					new_frame.buf.assign(buf, buf+len);
					new_frame.retry = 0;

					// start RTT measurement
					new_frame.tm.start();

					// send the datagram
					_callback.callback_send(*this, new_frame.flags, new_frame.seqno, getIdentifier(), &new_frame.buf[0], new_frame.buf.size());

					// increment next sequence number
					_last_ack = (seqno + 1) % _params.max_seq_numbers;

					// enter the wait state
					_send_state = SEND_WAIT_ACK;

					// set timeout to twice the average round-trip-time
					ibrcommon::Conditional::gettimeout(static_cast<size_t>(_avg_rtt * 2) + 1, &ts);

					// wait until one more slot is available
					// or no more frames are to ACK (if this was the last frame)
					while ((last && !_sw_frames.empty()) || (!last && sw_frames_full()))
					{
						_ack_cond.wait(&ts);
					}
				} catch (const ibrcommon::Conditional::ConditionalAbortException &e) {
					if (e.reason == ibrcommon::Conditional::ConditionalAbortException::COND_TIMEOUT)
					{
						// timeout - retransmit the whole window
						sw_timeout(last);
					}
					else
					{
						// maximum number of retransmissions hit
						_send_state = SEND_ERROR;

						// report failure
						_callback.reportFailure();

						// transmission failed - abort the stream
						throw DatagramException("transmission failed - abort the stream");
					}
				}

				// if this is the last segment switch directly to IDLE
				_send_state = last ? SEND_IDLE : SEND_NEXT;
			}
			else
			{
				IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 30) << "transmit frame seqno: " << seqno << IBRCOMMON_LOGGER_ENDL;

				// send the datagram
				_callback.callback_send(*this, flags, seqno, getIdentifier(), buf, len);

				// if this is the last segment switch directly to IDLE
				_send_state = last ? SEND_IDLE : SEND_NEXT;

				// increment next sequence number
				ibrcommon::MutexLock l(_ack_cond);
				_last_ack = (seqno + 1) % _params.max_seq_numbers;
			}
		}

		bool DatagramConnection::sw_frames_full()
		{
			return _sw_frames.size() >= (_params.max_seq_numbers / 2);
		}

		void DatagramConnection::sw_timeout(bool last)
		{
			// timeout value
			struct timespec ts;

			while (true) {
				try {
					ibrcommon::MutexLock l(_ack_cond);

					// fail -> increment the future timeout
					adjust_rtt(static_cast<double>(_avg_rtt) * 2);

					if (_sw_frames.size() > 0)
					{
						window_frame &front_frame = _sw_frames.front();

						IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 20) << "ack timeout for seqno " << front_frame.seqno << IBRCOMMON_LOGGER_ENDL;

						if (front_frame.retry > _params.retry_limit) {
							// maximum number of retransmissions hit
							_send_state = SEND_ERROR;

							// report failure
							_callback.reportFailure();

							// transmission failed - abort the stream
							throw DatagramException("transmission failed - abort the stream");
						}
					}

					// retransmit the window
					for (std::list<window_frame>::iterator it = _sw_frames.begin(); it != _sw_frames.end(); ++it)
					{
						window_frame &retry_frame = (*it);

						// send the datagram
						_callback.callback_send(*this, retry_frame.flags, retry_frame.seqno, getIdentifier(), &retry_frame.buf[0], retry_frame.buf.size());

						// increment retry counter
						retry_frame.retry++;
					}

					// enter the wait state
					_send_state = SEND_WAIT_ACK;

					// set timeout to twice the average round-trip-time
					ibrcommon::Conditional::gettimeout(static_cast<size_t>(_avg_rtt * 2) + 1, &ts);

					// wait until one more slot is available
					// or no more frames are to ACK (if this was the last frame)
					while ((last && !_sw_frames.empty()) || (!last && sw_frames_full()))
					{
						_ack_cond.wait(&ts);
					}
				} catch (const ibrcommon::Conditional::ConditionalAbortException &e) {
					if (e.reason == ibrcommon::Conditional::ConditionalAbortException::COND_TIMEOUT)
					{
						// timeout again - repeat at while loop
						continue;
					}
				}

				// done
				return;
			}
		}

		void DatagramConnection::nack(const unsigned int &seqno, const bool temporary)
		{
			// if the NACK is temporary skip ignore it
			// and repeat the frame after the timeout
			if (temporary) return;

			// skip the currently transmitted bundle
			_sender.skip();

			// handle the NACK as an ACK to move on with the next frame
			ack(seqno);
		}

		void DatagramConnection::ack(const unsigned int &seqno)
		{
			ibrcommon::MutexLock l(_ack_cond);

			switch (_params.flowcontrol) {
				case DatagramService::FLOW_SLIDING_WINDOW:
					if (_sw_frames.size() > 0) {
						window_frame &f = _sw_frames.front();

						if (seqno == ((f.seqno + 1) % _params.max_seq_numbers)) {
							// stop the measurement
							f.tm.stop();

							// adjust the average rtt
							adjust_rtt(f.tm.getMilliseconds());

							// report result
							_callback.reportSuccess(f.retry, f.tm.getMilliseconds());

							// remove front element
							_sw_frames.pop_front();
						}
					}
					break;

				default:
					_last_ack = seqno;
					break;
			}

			_ack_cond.signal(true);
		}

		void DatagramConnection::setPeerEID(const dtn::data::EID &peer)
		{
			_peer_eid = peer;
		}

		const dtn::data::EID& DatagramConnection::getPeerEID()
		{
			return _peer_eid;
		}

		void DatagramConnection::adjust_rtt(double value)
		{
			// convert current avg to float
			double new_rtt = _avg_rtt;

			// calculate average
			new_rtt = (new_rtt * AVG_RTT_WEIGHT) + ((1 - AVG_RTT_WEIGHT) * value);

			// assign the new value
			_avg_rtt = new_rtt;

			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 40) << "RTT adjusted, measured value: " << std::setprecision(4) << value << ", new avg. RTT: " << std::setprecision(4) << _avg_rtt << IBRCOMMON_LOGGER_ENDL;
		}

		DatagramConnection::Stream::Stream(DatagramConnection &conn, const dtn::data::Length &maxmsglen)
		 : std::iostream(this), _buf_size(maxmsglen), _first_segment(true), _last_segment(false),
		   _queue_buf(_buf_size), _queue_buf_len(0), _queue_buf_head(false),
		   _out_buf(_buf_size), _in_buf(_buf_size),
		   _abort(false), _skip(false), _reject(false), _callback(conn)
		{
			// Initialize get pointer. This should be zero so that underflow
			// is called upon first read.
			setg(0, 0, 0);

			// mark the buffer for outgoing data as free
			// the +1 sparse the first byte in the buffer and leave room
			// for the processing flags of the segment
			setp(&_out_buf[0], &_out_buf[0] + _buf_size - 1);
		}

		DatagramConnection::Stream::~Stream()
		{
		}

		void DatagramConnection::Stream::queue(const char *buf, const dtn::data::Length &len, bool isFirst) noexcept (false)
		{
			try {
				ibrcommon::MutexLock l(_queue_buf_cond);
				if (_abort) throw DatagramException("stream aborted");

				// wait until the buffer is free
				while (_queue_buf_len > 0)
				{
					if (_abort) throw DatagramException("stream aborted");
					_queue_buf_cond.wait();
				}

				// copy the new data into the buffer, but leave out the first byte (header)
				::memcpy(&_queue_buf[0], buf, len);

				// store the buffer length
				_queue_buf_len = len;
				_queue_buf_head = isFirst;

				// notify waiting threads
				_queue_buf_cond.signal();
			} catch (ibrcommon::Conditional::ConditionalAbortException &ex) {
				throw DatagramException("stream aborted");
			}
		}

		void DatagramConnection::Stream::skip()
		{
			ibrcommon::MutexLock l(_queue_buf_cond);
			_skip = true;
			_queue_buf_cond.signal(true);
		}

		void DatagramConnection::Stream::reject()
		{
			ibrcommon::MutexLock l(_queue_buf_cond);

			// set reject flag for futher frames
			_reject = true;
			_queue_buf_cond.signal(true);
		}

		void DatagramConnection::Stream::close()
		{
			ibrcommon::MutexLock l(_queue_buf_cond);
			_abort = true;
			_queue_buf_cond.abort();
		}

		int DatagramConnection::Stream::sync()
		{
			// We process the last segment in the set. Set this variable, so
			// that this information is available for the overflow method.
			_last_segment = true;

			int ret = std::char_traits<char>::eq_int_type(this->overflow(
					std::char_traits<char>::eof()), std::char_traits<char>::eof()) ? -1
					: 0;

			return ret;
		}

		std::char_traits<char>::int_type DatagramConnection::Stream::overflow(std::char_traits<char>::int_type c)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 40) << "Stream::overflow()" << IBRCOMMON_LOGGER_ENDL;

			if (_abort) throw DatagramException("stream aborted");

			char *ibegin = &_out_buf[0];
			char *iend = pptr();

			// mark the buffer for outgoing data as free
			// the +1 sparse the first byte in the buffer and leave room
			// for the processing flags of the segment
			setp(&_out_buf[0], &_out_buf[0] + _buf_size - 1);

			if (!std::char_traits<char>::eq_int_type(c, std::char_traits<char>::eof()))
			{
				*iend++ = std::char_traits<char>::to_char_type(c);
			}

			// bytes to send
			const dtn::data::Length bytes = (iend - ibegin);

			// if there is nothing to send, just return
			if (bytes == 0)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 35) << "Stream::overflow() nothing to sent" << IBRCOMMON_LOGGER_ENDL;
				return std::char_traits<char>::not_eof(c);
			}

			try {
				// disable skipping if this is the first segment
				if (_first_segment) _skip = false;

				// send segment to CL, use callback interface
				if (!_skip) _callback.stream_send(&_out_buf[0], bytes, _last_segment);

				// set the flags for the next segment
				_first_segment = _last_segment;
				_last_segment = false;
			} catch (const DatagramException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 35) << "Stream::overflow() exception: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

				// close this stream
				close();

				// re-throw the DatagramException
				throw;
			}

			return std::char_traits<char>::not_eof(c);
		}

		std::char_traits<char>::int_type DatagramConnection::Stream::underflow()
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 40) << "Stream::underflow()" << IBRCOMMON_LOGGER_ENDL;

			try {
				ibrcommon::MutexLock l(_queue_buf_cond);
				if (_abort) throw ibrcommon::Exception("stream aborted");

				// ignore this frame if this frame set is rejected
				while ((_queue_buf_len == 0) || (_reject && !_queue_buf_head))
				{
					// clear the buffer
					_queue_buf_len = 0;
					_queue_buf_cond.signal(true);

					if (_abort) throw ibrcommon::Exception("stream aborted");
					_queue_buf_cond.wait();
				}

				// reset reject
				_reject = false;

				// copy the queue buffer to an internal buffer
				::memcpy(&_in_buf[0], &_queue_buf[0], _queue_buf_len);

				// Since the input buffer content is now valid (or is new)
				// the get pointer should be initialized (or reset).
				setg(&_in_buf[0], &_in_buf[0], &_in_buf[0] + _queue_buf_len);

				// mark the queue buffer as free
				_queue_buf_len = 0;
				_queue_buf_cond.signal();

				return std::char_traits<char>::not_eof(_in_buf[0]);
			} catch (ibrcommon::Conditional::ConditionalAbortException &ex) {
				throw DatagramException("stream aborted");
			}
		}

		DatagramConnection::Sender::Sender(DatagramConnection &conn, Stream &stream)
		 : _stream(stream), _connection(conn), _skip(false)
		{
		}

		DatagramConnection::Sender::~Sender()
		{
		}

		void DatagramConnection::Sender::skip() noexcept
		{
			// skip all data of the current transmission
			_skip = true;
			_stream.skip();
		}

		void DatagramConnection::Sender::run() noexcept
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 40) << "Sender::run()"<< IBRCOMMON_LOGGER_ENDL;

			try {
				// get reference to the storage
				dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();

				// create a filter context
				dtn::core::FilterContext context;
				context.setProtocol(_connection._callback.getDiscoveryProtocol());

				// create a standard serializer
				dtn::data::DefaultSerializer serializer(_stream);

				// as long as the stream is marked as good ...
				while(_stream.good())
				{
					// get the next job
					dtn::net::BundleTransfer job = queue.poll();

					try {
						// read the bundle out of the storage
						dtn::data::Bundle bundle = storage.get(job.getBundle());

						// push bundle through the filter routines
						context.setBundle(bundle);
						context.setPeer(job.getNeighbor());
						BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::OUTPUT, context, bundle);

						if (ret != BundleFilter::ACCEPT) {
							job.abort(dtn::net::TransferAbortedEvent::REASON_REFUSED_BY_FILTER);
							continue;
						}

						// reset skip flag
						_skip = false;

						// write the bundle into the stream
						serializer << bundle; _stream.flush();

						// check if the stream is still marked as good
						if (_stream.good())
						{
							// check if last transmission was refused
							if (_skip) {
								// send transfer aborted event
								job.abort(dtn::net::TransferAbortedEvent::REASON_REFUSED);
							} else {
								// bundle send completely - raise bundle event
								job.complete();
							}
						}
					} catch (const dtn::storage::NoBundleFoundException&) {
						// could not load the bundle, abort the job
						job.abort(dtn::net::TransferAbortedEvent::REASON_BUNDLE_DELETED);
					}
				}

				IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 25) << "Sender::run() stream destroyed"<< IBRCOMMON_LOGGER_ENDL;
			} catch (const ibrcommon::QueueUnblockedException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 25) << "Sender::run() exception: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				return;
			} catch (std::exception &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(DatagramConnection::TAG, 25) << "Sender::run() exception: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void DatagramConnection::Sender::finally() noexcept
		{
		}

		void DatagramConnection::Sender::__cancellation() noexcept
		{
			// abort all blocking operations on the stream
			_stream.close();

			// abort blocking calls on the queue
			queue.abort();
		}
	} /* namespace data */
} /* namespace dtn */
