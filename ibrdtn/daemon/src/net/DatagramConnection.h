/*
 * DatagramConnection.h
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

#ifndef DATAGRAMCONNECTION_H_
#define DATAGRAMCONNECTION_H_

#include "net/ConvergenceLayer.h"
#include "net/DatagramService.h"
#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/thread/Queue.h>
#include <ibrcommon/thread/Conditional.h>
#include <ibrcommon/TimeMeasurement.h>
#include <streambuf>
#include <iostream>
#include <vector>
#include <stdint.h>

namespace dtn
{
	namespace net
	{
		class DatagramConnection;

		class DatagramConnectionCallback
		{
		public:
			virtual ~DatagramConnectionCallback() {};
			virtual void callback_send(DatagramConnection &connection, const char &flags, const unsigned int &seqno, const std::string &destination, const char *buf, const dtn::data::Length &len) noexcept (false) = 0;
			virtual void callback_ack(DatagramConnection &connection, const unsigned int &seqno, const std::string &destination) noexcept (false) = 0;
			virtual void callback_nack(DatagramConnection &connection, const unsigned int &seqno, const std::string &destination) noexcept (false) = 0;

			virtual void connectionUp(const DatagramConnection *conn) = 0;
			virtual void connectionDown(const DatagramConnection *conn) = 0;

			virtual void reportSuccess(size_t retries, double rtt) { };
			virtual void reportFailure() { };

			virtual dtn::core::Node::Protocol getDiscoveryProtocol() const = 0;
		};

		class DatagramConnection : public ibrcommon::JoinableThread
		{
			static const std::string TAG;

		public:
			DatagramConnection(const std::string &identifier, const DatagramService::Parameter &params, DatagramConnectionCallback &callback);
			virtual ~DatagramConnection();

			void run() noexcept;
			void setup() noexcept;
			void finally() noexcept;

			virtual void __cancellation() noexcept;

			void shutdown();

			const std::string& getIdentifier() const;

			/**
			 * Queue job for delivery to another node
			 * @param job
			 */
			void queue(const dtn::net::BundleTransfer &job);

			/**
			 * queue data for delivery to the stream
			 * @param buf
			 * @param len
			 */
			void queue(const char &flags, const unsigned int &seqno, const char *buf, const dtn::data::Length &len);

			/**
			 * This method is called by the DatagramCL, if an ACK is received.
			 * @param seq
			 */
			void ack(const unsigned int &seqno);

			/**
			 * This method is called by the DatagramCL, if an permanent NACK is received.
			 */
			void nack(const unsigned int &seqno, const bool temporary);

			/**
			 * Assign a peer EID to this connection
			 * @param peer The EID of this peer.
			 */
			void setPeerEID(const dtn::data::EID &peer);

			/**
			 * Returns the peer EID of this connection
			 */
			const dtn::data::EID& getPeerEID();

		private:
			enum SEND_FLOW {
				SEND_IDLE,
				SEND_WAIT_ACK,
				SEND_NEXT,
				SEND_ERROR
			} _send_state;

			enum RECV_FLOW {
				RECV_IDLE,
				RECV_HEAD,
				RECV_TRANSMISSION,
				RECV_ERROR
			} _recv_state;

			class Stream : public std::basic_streambuf<char, std::char_traits<char> >, public std::iostream
			{
			public:
				Stream(DatagramConnection &conn, const dtn::data::Length &maxmsglen);
				virtual ~Stream();

				/**
				 * Queueing data received from the CL worker thread for the LOWPANConnection
				 * @param buf Buffer with received data
				 * @param len Length of the buffer
				 */
				void queue(const char *buf, const dtn::data::Length &len, bool isFirst) noexcept (false);

				/**
				 * Close the stream to terminate all blocking
				 * calls on it.
				 */
				void close();

				/**
				 * Skip the outgoing frame set
				 */
				void skip();

				/**
				 * Reject the current incoming frame set
				 */
				void reject();

			protected:
				virtual int sync();
				virtual std::char_traits<char>::int_type overflow(std::char_traits<char>::int_type = std::char_traits<char>::eof());
				virtual std::char_traits<char>::int_type underflow();

			private:
				// buffer size and maximum message size
				const dtn::data::Length _buf_size;

				// true, if the next segment if the first of the bundle
				bool _first_segment;

				// will be set to true if the next segment is the last
				// of the bundle
				bool _last_segment;

				// buffer for incoming data to queue
				// the underflow method will block until
				// this buffer contains any data
				std::vector<char> _queue_buf;

				// the number of bytes available in the queue buffer
				dtn::data::Length _queue_buf_len;

				// true if the frame in the queue is the head of the frame-set
				bool _queue_buf_head;

				// conditional to lock the queue buffer and the
				// corresponding length variable
				ibrcommon::Conditional _queue_buf_cond;

				// outgoing data from the upper layer is stored
				// here first and processed by the overflow() method
				std::vector<char> _out_buf;

				// incoming data to deliver data to the upper layer
				// is stored in this buffer by the underflow() method
				std::vector<char> _in_buf;

				// this variable is set to true to shutdown
				// this stream
				bool _abort;

				// this variable is set to true if the outgoing
				// frame set should be skipped
				bool _skip;

				// this variable is set to true if the incoming
				// frame set should skipped
				bool _reject;

				// callback to the corresponding connection object
				DatagramConnection &_callback;
			};

			class Sender : public ibrcommon::JoinableThread
			{
			public:
				Sender(DatagramConnection &conn, Stream &stream);
				~Sender();

				/**
				 * skip the current bundle
				 */
				void skip() noexcept;

				void run() noexcept;
				void finally() noexcept;
				void __cancellation() noexcept;

				ibrcommon::Queue<dtn::net::BundleTransfer> queue;

			private:
				DatagramConnection::Stream &_stream;

				// callback to the corresponding connection object
				DatagramConnection &_connection;

				bool _skip;
			};

			/**
			 * Send a new frame
			 */
			void stream_send(const char *buf, const dtn::data::Length &len, bool last) noexcept (false);

			/**
			 * Adjust the average RTT by the new measured value
			 */
			void adjust_rtt(double value);

			/**
			 * True, if the sliding window buffer is exhausted
			 */
			bool sw_frames_full();

			/**
			 * Retransmit the whole sliding window buffer on a timeout
			 */
			void sw_timeout(bool last);

			DatagramConnectionCallback &_callback;
			const std::string _identifier;
			DatagramConnection::Stream _stream;
			DatagramConnection::Sender _sender;

			ibrcommon::Conditional _ack_cond;
			unsigned int _last_ack;
			unsigned int _next_seqno;

			// stores the head of each connection
			// the head is hold back until at least a second
			// or the last segment was received
			std::vector<char> _head_buf;
			dtn::data::Length _head_len;

			const DatagramService::Parameter _params;

			double _avg_rtt;

			dtn::data::EID _peer_eid;

			// buffer for sliding window approach
			class window_frame {
			public:
				// default constructor
				window_frame()
				: flags(0), seqno(0), retry(0) { }

				// destructor
				virtual ~window_frame() { }

				char flags;
				unsigned int seqno;
				std::vector<char> buf;
				unsigned int retry;
				ibrcommon::TimeMeasurement tm;
			};
			std::list<window_frame> _sw_frames;
		};
	} /* namespace data */
} /* namespace dtn */
#endif /* DATAGRAMCONNECTION_H_ */
