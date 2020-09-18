/*
 * UDPDatagramService.h
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

#ifndef UDPDATAGRAMSERVICE_H_
#define UDPDATAGRAMSERVICE_H_

#include "net/DatagramConvergenceLayer.h"
#include "net/DatagramService.h"
#include <ibrcommon/net/vsocket.h>
#include <ibrcommon/net/vinterface.h>

namespace dtn
{
	namespace net
	{
		class UDPDatagramService : public dtn::net::DatagramService
		{
		public:
			UDPDatagramService(const ibrcommon::vinterface &iface, int port, size_t mtu = 1280);
			virtual ~UDPDatagramService();

			/**
			 * Bind to the local socket.
			 * @throw If the bind fails, an DatagramException is thrown.
			 */
			virtual void bind() noexcept (false);

			/**
			 * Shutdown the socket. Unblock all calls on the socket (recv, send, etc.)
			 */
			virtual void shutdown();

			/**
			 * Send the payload as datagram to a defined destination
			 * @param address The destination address encoded as string.
			 * @param buf The buffer to send.
			 * @param length The number of available bytes in the buffer.
			 */
			virtual void send(const char &type, const char &flags, const unsigned int &seqno, const std::string &address, const char *buf, size_t length) noexcept (false);

			/**
			 * Send the payload as datagram to all neighbors (broadcast)
			 * @param buf The buffer to send.
			 * @param length The number of available bytes in the buffer.
			 */
			virtual void send(const char &type, const char &flags, const unsigned int &seqno, const char *buf, size_t length) noexcept (false);

			/**
			 * Receive an incoming datagram.
			 * @param buf A buffer to catch the incoming data.
			 * @param length The length of the buffer.
			 * @param address A buffer for the address of the sender.
			 * @throw If the receive call failed for any reason, an DatagramException is thrown.
			 * @return The number of received bytes.
			 */
			virtual size_t recvfrom(char *buf, size_t length, char &type, char &flags, unsigned int &seqno, std::string &address) noexcept (false);

			/**
			 * Get the service description for this convergence layer. This
			 * data is used to contact this node.
			 * @return The service description as string.
			 */
			virtual const std::string getServiceDescription() const;

			/**
			 * The used interface as vinterface object.
			 * @return A vinterface object.
			 */
			virtual const ibrcommon::vinterface& getInterface() const;

			/**
			 * The protocol identifier for this type of service.
			 * @return
			 */
			virtual dtn::core::Node::Protocol getProtocol() const;

			/**
			 * Returns the parameter for the connection.
			 * @return
			 */
			virtual const DatagramService::Parameter& getParameter() const;

		private:
			void send(const char &type, const char &flags, const unsigned int &seqno, const ibrcommon::vaddress &destination, const char *buf, size_t length) noexcept (false);

			static const std::string encode(const ibrcommon::vaddress &address, const int port = 0);
			static void decode(const std::string &identifier, ibrcommon::vaddress &address);

			ibrcommon::vsocket _vsocket;

			const static int BROADCAST_PORT = 5551;
			const static ibrcommon::vaddress BROADCAST_ADDR;

			ibrcommon::multicastsocket *_msock;

			const ibrcommon::vinterface _iface;
			const int _bind_port;

			DatagramService::Parameter _params;
		};

	} /* namespace net */
} /* namespace dtn */
#endif /* UDPDATAGRAMSERVICE_H_ */
