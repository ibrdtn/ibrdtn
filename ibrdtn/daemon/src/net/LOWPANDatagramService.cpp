/*
 * LOWPANDatagramService.cpp
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

#include "net/LOWPANDatagramService.h"
#include <ibrdtn/utils/Utils.h>

#include <ibrcommon/net/lowpansocket.h>
#include <ibrcommon/net/lowpanstream.h>

#include <ibrcommon/Logger.h>

#include <string.h>
#include <vector>

namespace dtn
{
	namespace net
	{
		const std::string LOWPANDatagramService::TAG = "LOWPANDatagramService";

		LOWPANDatagramService::LOWPANDatagramService(const ibrcommon::vinterface &iface, uint16_t panid)
		 : _panid(panid), _iface(iface)
		{
			// set connection parameters
			_params.max_msg_length = 115;
			_params.max_seq_numbers = 4;
			_params.flowcontrol = DatagramService::FLOW_STOPNWAIT;
			_params.initial_timeout = 2000;		// initial timeout 2 seconds
			_params.retry_limit = 5;

			// convert the panid into a string
			std::stringstream ss;
			ss << _panid;

			// assign the broadcast address
			_addr_broadcast = ibrcommon::vaddress("65535", ss.str(), AF_IEEE802154);
		}

		LOWPANDatagramService::~LOWPANDatagramService()
		{
			// destroy all socket objects
			_vsocket.destroy();
		}

		/**
		 * Bind to the local socket.
		 * @throw If the bind fails, an DatagramException is thrown.
		 */
		void LOWPANDatagramService::bind() throw (DatagramException)
		{
			try {
				_vsocket.destroy();
				_vsocket.add(new ibrcommon::lowpansocket(_panid, _iface));
				_vsocket.up();
			} catch (const std::bad_cast&) {
				throw DatagramException("bind failed");
			} catch (const ibrcommon::Exception&) {
				throw DatagramException("bind failed");
			}
		}

		/**
		 * Shutdown the socket. Unblock all calls on the socket (recv, send, etc.)
		 */
		void LOWPANDatagramService::shutdown()
		{
			// abort socket operations
			_vsocket.down();
		}

		/**
		 * Send the payload as datagram to a defined destination
		 * @param address The destination address encoded as string.
		 * @param buf The buffer to send.
		 * @param length The number of available bytes in the buffer.
		 */
		void LOWPANDatagramService::send(const char &type, const char &flags, const unsigned int &seqno, const std::string &identifier, const char *buf, size_t length) throw (DatagramException)
		{
			try {
				if (length > _params.max_msg_length)
				{
					IBRCOMMON_LOGGER_TAG(LOWPANDatagramService::TAG, error) << "send buffer to big to be transferred (" << length << ")."<< IBRCOMMON_LOGGER_ENDL;
					throw DatagramException("send failed - buffer to big to be transferred");
				}

				ibrcommon::socketset socks = _vsocket.getAll();
				if (socks.size() == 0) return;
				ibrcommon::lowpansocket &sock = dynamic_cast<ibrcommon::lowpansocket&>(**socks.begin());

				// decode destination address
				ibrcommon::vaddress destaddr;
				LOWPANDatagramService::decode(identifier, destaddr);

				// buffer for the datagram
				std::vector<char> tmp(length + 1);

				// encode the header (1 byte)
				tmp[0] = 0;

				// compat: 00

				// type: 01 = DATA, 10 = DISCO, 11 = ACK, 00 = NACK
				if (type == DatagramConvergenceLayer::HEADER_SEGMENT) tmp[0] |= 0x01 << 4;
				else if (type == DatagramConvergenceLayer::HEADER_BROADCAST) tmp[0] |= 0x02 << 4;
				else if (type == DatagramConvergenceLayer::HEADER_ACK) tmp[0] |= 0x03 << 4;
				else if (type == DatagramConvergenceLayer::HEADER_NACK) tmp[0] |= 0x00 << 4;

				// seq.no: xx (2 bit)
				tmp[0] |= static_cast<char>((0x03 & seqno) << 2);

				// flags: 10 = first, 00 = middle, 01 = last, 11 = both
				if (flags & DatagramService::SEGMENT_FIRST) tmp[0] |= 0x2;
				if (flags & DatagramService::SEGMENT_LAST) tmp[0] |= 0x1;

				if (length > 0) {
					// copy payload to the new buffer
					::memcpy(&tmp[1], buf, length);
				}

				IBRCOMMON_LOGGER_DEBUG_TAG(LOWPANDatagramService::TAG, 20) << "sendto() type: " << std::hex << (int)type << "; flags: " << std::hex << (int)flags << "; seqno: " << seqno << "; address: " << identifier << IBRCOMMON_LOGGER_ENDL;

				// send converted line
				sock.sendto(&tmp[0], length + 1, 0, destaddr);
			} catch (const ibrcommon::Exception&) {
				throw DatagramException("send failed");
			}
		}

		/**
		 * Send the payload as datagram to all neighbors (broadcast)
		 * @param buf The buffer to send.
		 * @param length The number of available bytes in the buffer.
		 */
		void LOWPANDatagramService::send(const char &type, const char &flags, const unsigned int &seqno, const char *buf, size_t length) throw (DatagramException)
		{
			try {
				if (length > _params.max_msg_length)
				{
					IBRCOMMON_LOGGER_TAG(LOWPANDatagramService::TAG, error) << "send buffer to big to be transferred (" << length << ")."<< IBRCOMMON_LOGGER_ENDL;
					throw DatagramException("send failed - buffer to big to be transferred");
				}

				ibrcommon::socketset socks = _vsocket.getAll();
				if (socks.size() == 0) return;
				ibrcommon::lowpansocket &sock = dynamic_cast<ibrcommon::lowpansocket&>(**socks.begin());

				// buffer for the datagram
				std::vector<char> tmp(length + 1);

				// encode the header (1 byte)
				tmp[0] = 0;

				// compat: 00

				// type: 01 = DATA, 10 = DISCO, 11 = ACK, 00 = NACK
				if (type == DatagramConvergenceLayer::HEADER_SEGMENT) tmp[0] |= 0x01 << 4;
				else if (type == DatagramConvergenceLayer::HEADER_BROADCAST) tmp[0] |= 0x02 << 4;
				else if (type == DatagramConvergenceLayer::HEADER_ACK) tmp[0] |= 0x03 << 4;
				else if (type == DatagramConvergenceLayer::HEADER_NACK) tmp[0] |= 0x00 << 4;

				// seq.no: xx (2 bit)
				tmp[0] |= static_cast<char>((0x03 & seqno) << 2);

				// flags: 10 = first, 00 = middle, 01 = last, 11 = both
				if (flags & DatagramService::SEGMENT_FIRST) tmp[0] |= 0x2;
				if (flags & DatagramService::SEGMENT_LAST) tmp[0] |= 0x1;

				if (length > 0) {
					// copy payload to the new buffer
					::memcpy(&tmp[1], buf, length);
				}

				// disable auto-ack feature for broadcast
				sock.setAutoAck(false);

				IBRCOMMON_LOGGER_DEBUG_TAG(LOWPANDatagramService::TAG, 20) << "sendto() type: " << std::hex << (int)type << "; flags: " << std::hex << (int)flags << "; seqno: " << seqno << "; address: broadcast" << IBRCOMMON_LOGGER_ENDL;

				// send converted line
				sock.sendto(&tmp[0], length + 1, 0, _addr_broadcast);

				// re-enable auto-ack feature
				sock.setAutoAck(true);
			} catch (const ibrcommon::Exception&) {
				throw DatagramException("send failed");
			}
		}

		/**
		 * Receive an incoming datagram.
		 * @param buf A buffer to catch the incoming data.
		 * @param length The length of the buffer.
		 * @param address A buffer for the address of the sender.
		 * @throw If the receive call failed for any reason, an DatagramException is thrown.
		 * @return The number of received bytes.
		 */
		size_t LOWPANDatagramService::recvfrom(char *buf, size_t length, char &type, char &flags, unsigned int &seqno, std::string &address) throw (DatagramException)
		{
			try {
				ibrcommon::socketset readfds;
				_vsocket.select(&readfds, NULL, NULL, NULL);

				for (ibrcommon::socketset::iterator iter = readfds.begin(); iter != readfds.end(); ++iter) {
					ibrcommon::lowpansocket &sock = dynamic_cast<ibrcommon::lowpansocket&>(**iter);

					std::vector<char> tmp(length + 1);

					// from address of the received frame
					ibrcommon::vaddress fromaddr;

					// Receive full frame from socket
					size_t ret = sock.recvfrom(&tmp[0], length + 1, 0, fromaddr);

					// decode the header (1 byte)
					// IGNORE: compat: 00

					// reset flags to zero
					flags = 0;

					// type: 01 = DATA, 10 = DISCO, 11 = ACK, 00 = NACK
					switch (tmp[0] & (0x03 << 4)) {
					case (0x01 << 4):
						type = DatagramConvergenceLayer::HEADER_SEGMENT;

						// flags: 10 = first, 00 = middle, 01 = last, 11 = both
						if (tmp[0] & 0x02) flags |= DatagramService::SEGMENT_FIRST;
						if (tmp[0] & 0x01) flags |= DatagramService::SEGMENT_LAST;

						break;
					case (0x02 << 4):
						type = DatagramConvergenceLayer::HEADER_BROADCAST;
						break;
					case (0x03 << 4):
						type = DatagramConvergenceLayer::HEADER_ACK;
						break;
					default:
						type = DatagramConvergenceLayer::HEADER_NACK;

						// flags: 10 = temporary
						if (tmp[0] & 0x02) flags |= DatagramService::NACK_TEMPORARY;

						break;
					}

					// seq.no: xx (2 bit)
					seqno = (tmp[0] & (0x03 << 2)) >> 2;

					if (ret > 1) {
						// copy payload into the buffer
						::memcpy(buf, &tmp[1], ret);
					}

					// encode into the right format
					address = LOWPANDatagramService::encode(fromaddr);

					IBRCOMMON_LOGGER_DEBUG_TAG(LOWPANDatagramService::TAG, 20) << "recvfrom() type: " << std::hex << (int)type << "; flags: " << std::hex << (int)flags << "; seqno: " << seqno << "; address: " << address << IBRCOMMON_LOGGER_ENDL;

					return ret - 1;
				}
			} catch (const ibrcommon::Exception&) {
				throw DatagramException("receive failed");
			}

			return 0;
		}

		/**
		 * Get the service description for this convergence layer. This
		 * data is used to contact this node.
		 * @return The service description as string.
		 */
		const std::string LOWPANDatagramService::getServiceDescription() const
		{
			std::stringstream ss_pan; ss_pan << _panid;

			// Get address via netlink
			ibrcommon::vaddress addr;
			ibrcommon::lowpansocket::getAddress(_iface, ss_pan.str(), addr);

			return LOWPANDatagramService::encode(addr);
		}

		/**
		 * The used interface as vinterface object.
		 * @return A vinterface object.
		 */
		const ibrcommon::vinterface& LOWPANDatagramService::getInterface() const
		{
			return _iface;
		}

		/**
		 * The protocol identifier for this type of service.
		 * @return
		 */
		dtn::core::Node::Protocol LOWPANDatagramService::getProtocol() const
		{
			return dtn::core::Node::CONN_DGRAM_LOWPAN;
		}

		const DatagramService::Parameter& LOWPANDatagramService::getParameter() const
		{
			return _params;
		}

		const std::string LOWPANDatagramService::encode(const ibrcommon::vaddress &addr)
		{
			std::stringstream ss;
			ss << "addr=" << addr.address() << ";pan=" << addr.service() << ";";
			return ss.str();
		}

		void LOWPANDatagramService::decode(const std::string &identifier, ibrcommon::vaddress &addr)
		{
			std::string address;
			std::string service;

			// parse parameters
			std::vector<std::string> parameters = dtn::utils::Utils::tokenize(";", identifier);
			std::vector<std::string>::const_iterator param_iter = parameters.begin();

			while (param_iter != parameters.end())
			{
				std::vector<std::string> p = dtn::utils::Utils::tokenize("=", (*param_iter));

				if (p[0].compare("addr") == 0)
				{
					address = p[1];
				}

				if (p[0].compare("pan") == 0)
				{
					service = p[1];
				}

				++param_iter;
			}

			addr = ibrcommon::vaddress(address, service, AF_IEEE802154);
		}
	} /* namespace net */
} /* namespace dtn */
