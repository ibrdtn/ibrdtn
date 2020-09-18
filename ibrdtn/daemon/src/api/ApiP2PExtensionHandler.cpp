/*
 * ApiP2PExtensionHandler.cpp
 *
 *  Created on: 25.02.2013
 *      Author: morgenro
 */

#include "api/ApiP2PExtensionHandler.h"
#include "core/BundleCore.h"
#include <ibrdtn/utils/Utils.h>
#include <ibrcommon/net/vinterface.h>

namespace dtn
{
	namespace api
	{
		ApiP2PExtensionHandler::ApiP2PExtensionHandler(ClientHandler &client, ibrcommon::socketstream &stream, dtn::core::Node::Protocol proto)
		 : ProtocolHandler(client, stream), _proto(proto)
		{
		}

		ApiP2PExtensionHandler::~ApiP2PExtensionHandler()
		{
		}

		void ApiP2PExtensionHandler::run()
		{
			std::string buffer = "";
			_stream << ClientHandler::API_STATUS_OK << " SWITCHED TO P2P_EXTENSION" << std::endl;

			// run as long the stream is ok
			while (_stream.good())
			{
				getline(_stream, buffer);

				if (buffer.length() == 0) continue;

				// search for '\r\n' and remove the '\r'
				std::string::reverse_iterator iter = buffer.rbegin();
				if ( (*iter) == '\r' ) buffer = buffer.substr(0, buffer.length() - 1);

				std::vector<std::string> cmd = dtn::utils::Utils::tokenize(" ", buffer);
				if (cmd.empty()) continue;

				if (cmd[0] == "exit")
				{
					// return to previous level
					break;
				}
				else
				{
					// forward to standard command set
					processCommand(cmd);
				}
			}
		}

		void ApiP2PExtensionHandler::finally()
		{
			dtn::core::BundleCore::getInstance().getConnectionManager().remove(this);
		}

		void ApiP2PExtensionHandler::setup()
		{
			dtn::core::BundleCore::getInstance().getConnectionManager().add(this);
		}

		void ApiP2PExtensionHandler::__cancellation() noexcept
		{
		}

		dtn::core::Node::Protocol ApiP2PExtensionHandler::getProtocol() const
		{
			return _proto;
		}

		void ApiP2PExtensionHandler::connect(const dtn::core::Node::URI &uri)
		{
			ibrcommon::MutexLock l(_write_lock);
			_stream << CMD_CONNECT << " CONNECT " << uri.value << std::endl;
		}

		void ApiP2PExtensionHandler::disconnect(const dtn::core::Node::URI &uri)
		{
			ibrcommon::MutexLock l(_write_lock);
			_stream << CMD_DISCONNECT << " DISCONNECT " << uri.value << std::endl;
		}

		void ApiP2PExtensionHandler::processCommand(const std::vector<std::string> &cmd)
		{
			try {
				if (cmd[0] == "connected") {
					if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

					const dtn::data::EID eid(cmd[1]);
					const dtn::core::Node::URI uri(dtn::core::Node::NODE_CONNECTED, this->getProtocol(), cmd[2], 120, -40);
					fireConnected(eid, uri);

					ibrcommon::MutexLock l(_write_lock);
					_stream << ClientHandler::API_STATUS_OK << " NODE CONNECTED" << std::endl;
				}
				else if (cmd[0] == "disconnected") {
					if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

					const dtn::data::EID eid(cmd[1]);
					const dtn::core::Node::URI uri(dtn::core::Node::NODE_CONNECTED, this->getProtocol(), cmd[2], 0, 0);
					fireDisconnected(eid, uri);

					ibrcommon::MutexLock l(_write_lock);
					_stream << ClientHandler::API_STATUS_OK << " NODE DISCONNECTED" << std::endl;
				}
				else if (cmd[0] == "discovered") {
					if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

					const dtn::data::EID eid(cmd[1]);
					const dtn::core::Node::URI uri(dtn::core::Node::NODE_P2P_DIALUP, this->getProtocol(), cmd[2], 120, -50);
					fireDiscovered(eid, uri);

					ibrcommon::MutexLock l(_write_lock);
					_stream << ClientHandler::API_STATUS_OK << " NODE DISCOVERED" << std::endl;
				}
				else if (cmd[0] == "interface") {
					if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

					if (cmd[1] == "up") {
						const ibrcommon::vinterface iface(cmd[2]);
						fireInterfaceUp(iface);

						ibrcommon::MutexLock l(_write_lock);
						_stream << ClientHandler::API_STATUS_OK << " INTERFACE UP" << std::endl;
					}
					else if (cmd[1] == "down") {
						const ibrcommon::vinterface iface(cmd[2]);
						fireInterfaceDown(iface);

						ibrcommon::MutexLock l(_write_lock);
						_stream << ClientHandler::API_STATUS_OK << " INTERFACE DOWN" << std::endl;
					}
					else {
						ibrcommon::MutexLock l(_write_lock);
						_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN INTERFACE STATE" << std::endl;
					}
				}
				else {
					ibrcommon::MutexLock l(_write_lock);
					_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN ACTION" << std::endl;
				}
			} catch (const std::exception&) {
				ibrcommon::MutexLock l(_write_lock);
				_stream << ClientHandler::API_STATUS_BAD_REQUEST << " ERROR" << std::endl;
			}
		}
	} /* namespace api */
} /* namespace dtn */
