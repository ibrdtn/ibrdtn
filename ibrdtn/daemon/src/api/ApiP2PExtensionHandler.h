/*
 * ApiP2PExtensionHandler.h
 *
 *  Created on: 25.02.2013
 *      Author: morgenro
 */

#include "api/Registration.h"
#include "api/ClientHandler.h"
#include "core/Node.h"
#include "net/P2PDialupExtension.h"
#include <ibrcommon/thread/Mutex.h>

#ifndef APIP2PEXTENSIONHANDLER_H_
#define APIP2PEXTENSIONHANDLER_H_

namespace dtn
{
	namespace api
	{
		class ApiP2PExtensionHandler : public ProtocolHandler, public dtn::net::P2PDialupExtension {
		public:
			enum COMMAND {
				CMD_NOOP = 100,
				CMD_CONNECT = 101,
				CMD_DISCONNECT = 102
			};

			ApiP2PExtensionHandler(ClientHandler &client, ibrcommon::socketstream &stream, dtn::core::Node::Protocol proto);
			virtual ~ApiP2PExtensionHandler();

			virtual void run();
			virtual void finally();
			virtual void setup();
			virtual void __cancellation() noexcept;

			/**
			 * Provides the extension tag used in the node URIs.
			 * E.g. p2p:wifi or p2p:bt
			 */
			virtual dtn::core::Node::Protocol getProtocol() const;

			/**
			 * Try to initiate a connection to a remote peer
			 */
			virtual void connect(const dtn::core::Node::URI &uri);

			/**
			 * Close an open connection to a peer
			 */
			virtual void disconnect(const dtn::core::Node::URI &uri);

		private:
			void processCommand(const std::vector<std::string> &cmd);

			ibrcommon::Mutex _write_lock;
			const dtn::core::Node::Protocol _proto;
		};
	} /* namespace api */
} /* namespace dtn */
#endif /* APIP2PEXTENSIONHANDLER_H_ */
