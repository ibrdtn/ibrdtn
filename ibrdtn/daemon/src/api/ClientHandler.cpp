/*
 * ClientHandler.cpp
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

#include "config.h"
#include "Configuration.h"
#include "api/ClientHandler.h"
#include "api/BinaryStreamClient.h"
#include "api/ManagementConnection.h"
#include "api/EventConnection.h"
#include "api/ExtendedApiHandler.h"
#include "api/OrderedStreamHandler.h"
#include "api/ApiP2PExtensionHandler.h"
#include "core/BundleCore.h"
#include <ibrcommon/Logger.h>
#include <ibrdtn/utils/Utils.h>
#include <ibrcommon/link/LinkManager.h>

namespace dtn
{
	namespace api
	{
		ProtocolHandler::ProtocolHandler(ClientHandler &client, ibrcommon::socketstream &stream)
		 : _client(client), _stream(stream)
		{
		}

		ProtocolHandler::~ProtocolHandler()
		{}

		ClientHandler::ClientHandler(ApiServerInterface &srv, Registration &registration, ibrcommon::socketstream *conn)
		 : _srv(srv), _registration(&registration), _stream(conn), _endpoint(dtn::core::BundleCore::local), _handler(NULL)
		{
		}

		ClientHandler::~ClientHandler()
		{
			delete _stream;
		}

		Registration& ClientHandler::getRegistration()
		{
			return *_registration;
		}

		ApiServerInterface& ClientHandler::getAPIServer()
		{
			return _srv;
		}

		void ClientHandler::switchRegistration(Registration &reg)
		{
			_registration->abort();

			_srv.freeRegistration(*_registration);

			_registration = &reg;
		}

		void ClientHandler::setup() noexcept
		{
		}

		void ClientHandler::run() noexcept
		{
			try {
				// signal the active connection to the server
				_srv.connectionUp(this);

				std::string buffer;

				while (_stream->good())
				{
					if (_handler != NULL)
					{
						_handler->setup();
						_handler->run();
						_handler->finally();
						delete _handler;
						_handler = NULL;

						// end this stream, return to the previous stage
						(*_stream) << ClientHandler::API_STATUS_OK << " SWITCHED TO LEVEL 0" << std::endl;

						continue;
					}

					getline(*_stream, buffer);

					// search for '\r\n' and remove the '\r'
					std::string::reverse_iterator iter = buffer.rbegin();
					if ( (*iter) == '\r' ) buffer = buffer.substr(0, buffer.length() - 1);

					std::vector<std::string> cmd = dtn::utils::Utils::tokenize(" ", buffer);
					if (cmd.empty()) continue;

					try {
						if (cmd[0] == "protocol")
						{
							if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

							if (cmd[1] == "tcpcl")
							{
								// switch to binary protocol (old style api)
								_handler = new BinaryStreamClient(*this, *_stream);
								continue;
							}
							else if (cmd[1] == "management")
							{
								// switch to the management protocol
								_handler = new ManagementConnection(*this, *_stream);
								continue;
							}
							else if (cmd[1] == "event")
							{
								// switch to the management protocol
								_handler = new EventConnection(*this, *_stream);
								continue;
							}
							else if (cmd[1] == "extended")
							{
								// switch to the extended api
								_handler = new ExtendedApiHandler(*this, *_stream);
								continue;
							}
							else if (cmd[1] == "streaming")
							{
								// switch to the streaming api
								_handler = new OrderedStreamHandler(*this, *_stream);
								continue;
							}
							else if (cmd[1] == "p2p_extension")
							{
								if (cmd.size() < 3) {
									error(API_STATUS_NOT_ACCEPTABLE, "P2P TYPE REQUIRED");
									continue;
								}

								if (cmd[2] == "wifi") {
									// switch to the streaming api
									_handler = new ApiP2PExtensionHandler(*this, *_stream, dtn::core::Node::CONN_P2P_WIFI);
									continue;
								} else if (cmd[2] == "bt") {
									// switch to the streaming api
									_handler = new ApiP2PExtensionHandler(*this, *_stream, dtn::core::Node::CONN_P2P_BT);
									continue;
								} else {
									error(API_STATUS_NOT_ACCEPTABLE, "P2P TYPE UNKNOWN");
									continue;
								}
							}
							else
							{
								error(API_STATUS_NOT_ACCEPTABLE, "UNKNOWN PROTOCOL");
							}
						}
						else
						{
							// forward to standard command set
							processCommand(cmd);
						}
					} catch (const std::exception&) {
						error(API_STATUS_BAD_REQUEST, "PROTOCOL ERROR");
					}
				}
			} catch (const ibrcommon::socket_exception &ex) {
				IBRCOMMON_LOGGER_TAG("ClientHandler", error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void ClientHandler::error(STATUS_CODES code, const std::string &msg)
		{
			ibrcommon::MutexLock l(_write_lock);
			(*_stream) << code << " " << msg << std::endl;
		}

		void ClientHandler::__cancellation() noexcept
		{
			// close the stream
			(*_stream).close();
		}

		void ClientHandler::finally() noexcept
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("ClientHandler", 60) << "ApiConnection down" << IBRCOMMON_LOGGER_ENDL;

			// remove the client from the list in ApiServer
			_srv.connectionDown(this);

			_registration->abort();
			_srv.freeRegistration(*_registration);

			// close the stream
			(*_stream).close();
		}

		void ClientHandler::processCommand(const std::vector<std::string> &cmd)
		{
			class BundleFilter : public dtn::storage::BundleSelector
			{
			public:
				BundleFilter()
				{};

				virtual ~BundleFilter() {};

				virtual dtn::data::Size limit() const noexcept { return 0; };

				virtual bool shouldAdd(const dtn::data::MetaBundle&) const noexcept (false)
				{
					return true;
				}
			};

			try {
				if (cmd[0] == "set")
				{
					if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

					if (cmd[1] == "endpoint")
					{
						if (cmd.size() < 3) throw ibrcommon::Exception("not enough parameters");

						ibrcommon::MutexLock l(_write_lock);
						if (cmd[2].length() <= 0) {
							// send error notification
							(*_stream) << API_STATUS_NOT_ACCEPTABLE << " INVALID ENDPOINT" << std::endl;
						} else {
							// un-subscribe previous registration
							_registration->unsubscribe(_endpoint);

							// set new application endpoint
							_endpoint.setApplication(cmd[2]);

							// subscribe to new endpoint
							_registration->subscribe(_endpoint);

							// send accepted notification
							(*_stream) << API_STATUS_ACCEPTED << " OK" << std::endl;
						}
					}
					else
					{
						ibrcommon::MutexLock l(_write_lock);
						(*_stream) << API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
					}
				}
				else if (cmd[0] == "registration")
				{
					if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

					if (cmd[1] == "add")
					{
						if (cmd.size() < 3) throw ibrcommon::Exception("not enough parameters");

						ibrcommon::MutexLock l(_write_lock);
						dtn::data::EID endpoint(cmd[2]);

						// error checking
						if (endpoint == dtn::data::EID())
						{
							(*_stream) << API_STATUS_NOT_ACCEPTABLE << " INVALID EID" << std::endl;
						}
						else
						{
							_registration->subscribe(endpoint);
							(*_stream) << API_STATUS_ACCEPTED << " OK" << std::endl;
						}
					}
					else if (cmd[1] == "del")
					{
						if (cmd.size() < 3) throw ibrcommon::Exception("not enough parameters");

						ibrcommon::MutexLock l(_write_lock);
						dtn::data::EID endpoint(cmd[2]);

						// error checking
						if (endpoint == dtn::data::EID())
						{
							(*_stream) << API_STATUS_NOT_ACCEPTABLE << " INVALID EID" << std::endl;
						}
						else
						{
							_registration->unsubscribe(endpoint);
							(*_stream) << API_STATUS_ACCEPTED << " OK" << std::endl;
						}
					}
					else if (cmd[1] == "list")
					{
						ibrcommon::MutexLock l(_write_lock);
						const std::set<dtn::data::EID> list = _registration->getSubscriptions();

						(*_stream) << API_STATUS_OK << " REGISTRATION LIST" << std::endl;
						for (std::set<dtn::data::EID>::const_iterator iter = list.begin(); iter != list.end(); ++iter)
						{
							(*_stream) << (*iter).getString() << std::endl;
						}
						(*_stream) << std::endl;
					}
					else
					{
						ibrcommon::MutexLock l(_write_lock);
						(*_stream) << API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
					}
				}
				else
				{
					ibrcommon::MutexLock l(_write_lock);
					(*_stream) << API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
				}
			} catch (const std::exception&) {
				ibrcommon::MutexLock l(_write_lock);
				(*_stream) << API_STATUS_BAD_REQUEST << " ERROR" << std::endl;
			}
		}
	}
}
