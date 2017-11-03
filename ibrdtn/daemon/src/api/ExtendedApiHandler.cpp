/*
 * ExtendedApiHandler.cpp
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 * Written-by: Stephen Roettger <roettger@ibr.cs.tu-bs.de>
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
#include "api/ExtendedApiHandler.h"
#include "core/BundleEvent.h"
#include <ibrcommon/Logger.h>
#include <ibrdtn/data/BundleBuilder.h>
#include <ibrdtn/api/PlainSerializer.h>
#include <ibrdtn/data/AgeBlock.h>
#include <ibrdtn/utils/Utils.h>
#include <ibrcommon/data/Base64Reader.h>
#include <ibrcommon/data/Base64Stream.h>
#include "core/BundleCore.h"
#include <ibrdtn/utils/Random.h>

#include <ibrdtn/ibrdtn.h>
#ifdef IBRDTN_SUPPORT_BSP
#include "security/SecurityManager.h"
#endif

#include <algorithm>

namespace dtn
{
	namespace api
	{
		ExtendedApiHandler::ExtendedApiHandler(ClientHandler &client, ibrcommon::socketstream &stream)
		 : ProtocolHandler(client, stream), _sender(new Sender(*this)),
		   _endpoint(_client.getRegistration().getDefaultEID()), _encoding(dtn::api::PlainSerializer::BASE64)
		{
			_client.getRegistration().subscribe(_endpoint);
		}

		ExtendedApiHandler::~ExtendedApiHandler()
		{
			_client.getRegistration().abort();
			_sender->join();
			delete _sender;
		}

		bool ExtendedApiHandler::good() const{
			return _stream.good();
		}

		void ExtendedApiHandler::__cancellation() throw ()
		{
			// close the stream
			_stream.close();
		}

		void ExtendedApiHandler::finally()
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("ExtendedApiHandler", 60) << "ExtendedApiConnection down" << IBRCOMMON_LOGGER_ENDL;

			_client.getRegistration().abort();

			// close the stream
			_stream.close();

			try {
				// shutdown the sender thread
				_sender->stop();
			} catch (const std::exception&) { };
		}

		void ExtendedApiHandler::run()
		{
			_sender->start();

			std::string buffer;
			_stream << ClientHandler::API_STATUS_OK << " SWITCHED TO EXTENDED" << std::endl;

			while (_stream.good())
			{
				getline(_stream, buffer);

				std::string::reverse_iterator iter = buffer.rbegin();
				if ( (*iter) == '\r' ) buffer = buffer.substr(0, buffer.length() - 1);

				std::vector<std::string> cmd = dtn::utils::Utils::tokenize(" ", buffer);
				if (cmd.empty()) continue;

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
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " INVALID ENDPOINT" << std::endl;
							} else {
								Registration& reg = _client.getRegistration();

								// un-subscribe previous registration
								reg.unsubscribe(_endpoint);

								// set new application endpoint
								_endpoint.setApplication(cmd[2]);

								// subscribe to new endpoint
								reg.subscribe(_endpoint);

								// send accepted notification
								_stream << ClientHandler::API_STATUS_OK << " OK" << std::endl;
							}
						}
						else if (cmd[1] == "encoding")
						{
							if (cmd.size() < 3) throw ibrcommon::Exception("not enough parameters");

							// parse encoding
							dtn::api::PlainSerializer::Encoding enc = dtn::api::PlainSerializer::parseEncoding(cmd[2]);

							if (enc != dtn::api::PlainSerializer::INVALID) {
								// set the new encoding as default
								_encoding = enc;

								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_OK << " OK" << std::endl;
							} else {
								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " INVALID ENCODING" << std::endl;
							}
						}
						else
						{
							ibrcommon::MutexLock l(_write_lock);
							_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
						}
					}
					else if (cmd[0] == "endpoint")
					{
						if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

						if (cmd[1] == "add")
						{
							if (cmd.size() < 3) throw ibrcommon::Exception("not enough parameters");

							ibrcommon::MutexLock l(_write_lock);

							// error checking
							if (cmd[2].length() <= 0)
							{
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " INVALID ENDPOINT" << std::endl;
							}
							else
							{
								dtn::data::EID new_endpoint = dtn::core::BundleCore::local;
								new_endpoint.setApplication(cmd[2]);

								_client.getRegistration().subscribe(new_endpoint);
								_stream << ClientHandler::API_STATUS_OK << " OK" << std::endl;
							}
						}
						else if (cmd[1] == "del")
						{
							if (cmd.size() < 3) throw ibrcommon::Exception("not enough parameters");

							ibrcommon::MutexLock l(_write_lock);

							// error checking
							if (cmd[2].length() <= 0)
							{
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " INVALID ENDPOINT" << std::endl;
							}
							else
							{
								dtn::data::EID del_endpoint = dtn::core::BundleCore::local;
								del_endpoint.setApplication( cmd[2] );

								_client.getRegistration().unsubscribe(del_endpoint);

								// restore default endpoint if the standard endpoint has been removed
								if(_endpoint == del_endpoint)
								{
									_endpoint = _client.getRegistration().getDefaultEID();
									_client.getRegistration().subscribe(_endpoint);
								}

								_stream << ClientHandler::API_STATUS_OK << " OK" << std::endl;
							}
						}
						else if (cmd[1] == "get")
						{
							ibrcommon::MutexLock l(_write_lock);
							_stream << ClientHandler::API_STATUS_OK << " ENDPOINT GET " << _endpoint.getString() << std::endl;
						}
						else
						{
							ibrcommon::MutexLock l(_write_lock);
							_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
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
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " INVALID EID" << std::endl;
							}
							else
							{
								_client.getRegistration().subscribe(endpoint);
								_stream << ClientHandler::API_STATUS_OK << " OK" << std::endl;
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
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " INVALID EID" << std::endl;
							}
							else
							{
								_client.getRegistration().unsubscribe(endpoint);
								if(_endpoint == endpoint)
								{
									_endpoint = _client.getRegistration().getDefaultEID();
									_client.getRegistration().subscribe(_endpoint);
								}

								_stream << ClientHandler::API_STATUS_OK << " OK" << std::endl;
							}
						}
						else if (cmd[1] == "list")
						{
							ibrcommon::MutexLock l(_write_lock);
							const std::set<dtn::data::EID> list = _client.getRegistration().getSubscriptions();

							_stream << ClientHandler::API_STATUS_OK << " REGISTRATION LIST" << std::endl;
							for (std::set<dtn::data::EID>::const_iterator iter = list.begin(); iter != list.end(); ++iter)
							{
								_stream << (*iter).getString() << std::endl;
							}
							_stream << std::endl;
						}
						else if (cmd[1] == "save")
						{
							if (cmd.size() < 3) throw ibrcommon::Exception("not enough parameters");

							ibrcommon::Timer::time_t lifetime = 0;
							std::stringstream ss(cmd[2]);

							ss >> lifetime;
							if(ss.fail()) throw ibrcommon::Exception("malformed command");

							/* make the registration persistent for a given lifetime */
							_client.getRegistration().setPersistent(lifetime);

							ibrcommon::MutexLock l(_write_lock);
							_stream << ClientHandler::API_STATUS_OK << " REGISTRATION SAVE " << _client.getRegistration().getHandle() << std::endl;
						}
						else if (cmd[1] == "load")
						{
							if (cmd.size() < 3) throw ibrcommon::Exception("not enough parameters");

							const std::string handle = cmd[2];

							try
							{
								Registration& reg = _client.getAPIServer().getRegistration(handle);

								/* stop the sender */
								_client.getRegistration().abort();
								_sender->join();

								/* switch the registration */
								_client.switchRegistration(reg);

								/* and switch the sender */
								Sender *old_sender = _sender;
								try{
									_sender = new Sender(*this);
								}
								catch (const std::bad_alloc &ex)
								{
									_sender = old_sender;
									throw ex;
								}
								delete old_sender;
								_sender->start();

								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_OK << " REGISTRATION LOAD" << std::endl;
							}
							catch (const Registration::AlreadyAttachedException& ex)
							{
								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_SERVICE_UNAVAILABLE << " REGISTRATION BUSY" << std::endl;
							}
							catch (const Registration::NotFoundException& ex)
							{
								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_SERVICE_UNAVAILABLE << " REGISTRATION NOT FOUND" << std::endl;
							}
						}
						else
						{
							ibrcommon::MutexLock l(_write_lock);
							_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
						}
					}
					else if (cmd[0] == "neighbor")
					{
						if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

						if (cmd[1] == "list")
						{
							ibrcommon::MutexLock l(_write_lock);
							const std::set<dtn::core::Node> nlist = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();

							_stream << ClientHandler::API_STATUS_OK << " NEIGHBOR LIST" << std::endl;
							for (std::set<dtn::core::Node>::const_iterator iter = nlist.begin(); iter != nlist.end(); ++iter)
							{
								_stream << (*iter).getEID().getString() << std::endl;
							}
							_stream << std::endl;
						}
						else if(cmd[1]=="protocols")
						{
							ibrcommon::MutexLock l(_write_lock);
							const std::set<dtn::core::Node> nlist = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();

							_stream << ClientHandler::API_STATUS_OK << " NEIGHBOR PROTOCOLS" << std::endl;
							for (std::set<dtn::core::Node>::const_iterator iter = nlist.begin(); iter != nlist.end(); ++iter)
							{
								_stream << (*iter).getEID().getString();
								_stream << ":";
								std::list<Node::URI> urilist = (*iter).getAll();
								// Store found protocols in list
								std::set<std::string> foundProtocols;
								for(std::list<Node::URI>::const_iterator uriIter = urilist.begin(); uriIter != urilist.end(); ++uriIter)
								{
									// Get protocol name
									std::string protocol = Node::toString((*uriIter).protocol);
									// Check if protocol is already known
									if(foundProtocols.find(protocol) == foundProtocols.end())
									{
										_stream << " " << protocol;
										foundProtocols.insert(protocol);
									}
								}
								_stream << std::endl;
							}
							_stream << std::endl;
						}
						else if(cmd[1]=="connections")
						{
							ibrcommon::MutexLock l(_write_lock);
							const std::set<dtn::core::Node> nlist = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();

							_stream << ClientHandler::API_STATUS_OK << " NEIGHBOR CONNECTIONS" << std::endl;
							for (std::set<dtn::core::Node>::const_iterator iter = nlist.begin(); iter != nlist.end(); ++iter)
							{
								_stream << (*iter).getEID().getString() << ":";
								std::list<Node::URI> urilist = (*iter).getAll();
								for(std::list<Node::URI>::const_iterator uriIter = urilist.begin(); uriIter != urilist.end(); ++uriIter)
								{
									_stream << " " << (*uriIter);
								}
								_stream << std::endl;
							}
							_stream << std::endl;
						}
						else
						{
							ibrcommon::MutexLock l(_write_lock);
							_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
						}
					}
					else if (cmd[0] == "bundle")
					{
						if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

						if (cmd[1] == "get")
						{
							// transfer bundle data
							ibrcommon::MutexLock l(_write_lock);

							if (cmd.size() == 2)
							{
								_stream << ClientHandler::API_STATUS_OK << " BUNDLE GET "; sayBundleID(_stream, _bundle_reg); _stream << std::endl;
								PlainSerializer(_stream, _encoding) << _bundle_reg;
							}
							else if (cmd[2] == "binary")
							{
								_stream << ClientHandler::API_STATUS_OK << " BUNDLE GET BINARY "; sayBundleID(_stream, _bundle_reg); _stream << std::endl;
								dtn::data::DefaultSerializer(_stream) << _bundle_reg; _stream << std::flush;
							}
							else if (cmd[2] == "plain")
							{
								_stream << ClientHandler::API_STATUS_OK << " BUNDLE GET PLAIN "; sayBundleID(_stream, _bundle_reg); _stream << std::endl;
								PlainSerializer(_stream, _encoding) << _bundle_reg;
							}
							else if (cmd[2] == "xml")
							{
								_stream << ClientHandler::API_STATUS_NOT_IMPLEMENTED << " FORMAT NOT IMPLEMENTED" << std::endl;
							}
							else
							{
								_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN FORMAT" << std::endl;
							}
						}
						else if (cmd[1] == "put")
						{
							// lock the stream during reception of bundle data
							ibrcommon::MutexLock l(_write_lock);

							if (cmd.size() < 3)
							{
								_stream << ClientHandler::API_STATUS_BAD_REQUEST << " PLEASE DEFINE THE FORMAT" << std::endl;
							}
							else if (cmd[2] == "plain")
							{
								_stream << ClientHandler::API_STATUS_CONTINUE << " PUT BUNDLE PLAIN" << std::endl;

								try {
									PlainDeserializer(_stream) >> _bundle_reg;
									_stream << ClientHandler::API_STATUS_OK << " BUNDLE IN REGISTER" << std::endl;
								} catch (const std::exception &ex) {
									IBRCOMMON_LOGGER_DEBUG_TAG("ExtendedApiHandler", 20) << "put failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
									_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " PUT FAILED" << std::endl;

								}
							}
							else if (cmd[2] == "binary")
							{
								_stream << ClientHandler::API_STATUS_CONTINUE << " PUT BUNDLE BINARY" << std::endl;

								try {
									dtn::data::DefaultDeserializer(_stream) >> _bundle_reg;
									_stream << ClientHandler::API_STATUS_OK << " BUNDLE IN REGISTER" << std::endl;
								} catch (const std::exception &ex) {
									IBRCOMMON_LOGGER_DEBUG_TAG("ExtendedApiHandler", 20) << "put failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
									_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " PUT FAILED" << std::endl;
								}
							}
							else
							{
								_stream << ClientHandler::API_STATUS_BAD_REQUEST << " PLEASE DEFINE THE FORMAT" << std::endl;
							}
						}
						else if (cmd[1] == "load")
						{
							if (cmd.size() < 3) throw ibrcommon::Exception("not enough parameters");

							dtn::data::BundleID id;

							if (cmd[2] == "queue")
							{
								id = _bundle_queue.take();
							}
							else
							{
								// construct bundle id
								id = readBundleID(cmd, 2);
							}

							// load the bundle
							try {
								_bundle_reg = dtn::core::BundleCore::getInstance().getStorage().get(id);

								try {
									// process the bundle block (security, compression, ...)
									dtn::core::BundleCore::processBlocks(_bundle_reg);

									ibrcommon::MutexLock l(_write_lock);
									_stream << ClientHandler::API_STATUS_OK << " BUNDLE LOADED "; sayBundleID(_stream, id); _stream << std::endl;
								} catch (const ibrcommon::Exception &e) {
									// clear the register
									_bundle_reg = dtn::data::Bundle();

									ibrcommon::MutexLock l(_write_lock);
									_stream << ClientHandler::API_STATUS_INTERNAL_ERROR << " BUNDLE NOT LOADED" << std::endl;
								}
							} catch (const ibrcommon::Exception&) {
								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_NOT_FOUND << " BUNDLE NOT FOUND" << std::endl;
							}
						}
						else if (cmd[1] == "clear")
						{
							_bundle_reg = dtn::data::Bundle();

							ibrcommon::MutexLock l(_write_lock);
							_stream << ClientHandler::API_STATUS_OK << " BUNDLE CLEARED" << std::endl;
						}
						else if (cmd[1] == "free")
						{
							try {
								dtn::core::BundleCore::getInstance().getStorage().remove(_bundle_reg);
								_bundle_reg = dtn::data::Bundle();
								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_OK << " BUNDLE FREE SUCCESSFUL" << std::endl;
							} catch (const ibrcommon::Exception&) {
								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_NOT_FOUND << " BUNDLE NOT FOUND" << std::endl;
							}
						}
						else if (cmd[1] == "delivered")
						{
							if (cmd.size() < 5) throw ibrcommon::Exception("not enough parameters");

							try {
								// construct bundle id
								dtn::data::BundleID id = readBundleID(cmd, 2);

								// announce this bundle as delivered
								dtn::data::MetaBundle meta = dtn::data::MetaBundle::create(dtn::core::BundleCore::getInstance().getStorage().get(id));
								_client.getRegistration().delivered(meta);

								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_OK << " BUNDLE DELIVERED ACCEPTED" << std::endl;
							} catch (const ibrcommon::Exception&) {
								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_NOT_FOUND << " BUNDLE NOT FOUND" << std::endl;
							}
						}
						else if (cmd[1] == "store")
						{
							// store the bundle in the storage
							try {
								dtn::core::BundleCore::getInstance().getStorage().store(_bundle_reg);
								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_OK << " BUNDLE STORE SUCCESSFUL" << std::endl;
							} catch (const ibrcommon::Exception&) {
								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_INTERNAL_ERROR << " BUNDLE STORE FAILED" << std::endl;
							}
						}
						else if (cmd[1] == "send")
						{
							// forward the bundle to the storage processing
							dtn::api::Registration::processIncomingBundle(_endpoint, _bundle_reg);

							ibrcommon::MutexLock l(_write_lock);
							_stream << ClientHandler::API_STATUS_OK << " BUNDLE SENT "; sayBundleID(_stream, _bundle_reg); _stream << std::endl;
						}
						else if (cmd[1] == "info")
						{
							// transfer bundle data
							ibrcommon::MutexLock l(_write_lock);

							_stream << ClientHandler::API_STATUS_OK << " BUNDLE INFO "; sayBundleID(_stream, _bundle_reg); _stream << std::endl;
							PlainSerializer(_stream, PlainSerializer::SKIP_PAYLOAD) << _bundle_reg;
						}
						else if (cmd[1] == "block")
						{
							if (cmd.size() < 3) throw ibrcommon::Exception("not enough parameters");

							if (cmd[2] == "add")
							{
								dtn::data::BundleBuilder inserter(_bundle_reg, dtn::data::BundleBuilder::END);

								/* parse an optional offset, where to insert the block */
								if (cmd.size() > 3)
								{
									int offset;
									std::istringstream ss(cmd[3]);

									ss >> offset;
									if (ss.fail()) throw ibrcommon::Exception("malformed command");

									if (static_cast<dtn::data::Size>(offset) >= _bundle_reg.size())
									{
										inserter = dtn::data::BundleBuilder(_bundle_reg, dtn::data::BundleBuilder::END);
									}
									else if(offset == 0)
									{
										inserter = dtn::data::BundleBuilder(_bundle_reg, dtn::data::BundleBuilder::FRONT);
									}
									else
									{
										inserter = dtn::data::BundleBuilder(_bundle_reg, dtn::data::BundleBuilder::MIDDLE, offset);
									}
								}

								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_CONTINUE << " BUNDLE BLOCK ADD" << std::endl;

								try
								{
									dtn::data::Block &block = PlainDeserializer(_stream).readBlock(inserter);
									if (inserter.getAlignment() == dtn::data::BundleBuilder::END)
									{
										block.set(dtn::data::Block::LAST_BLOCK, true);
									}
									else
									{
										block.set(dtn::data::Block::LAST_BLOCK, false);
									}
									_stream << ClientHandler::API_STATUS_OK << " BUNDLE BLOCK ADD SUCCESSFUL" << std::endl;
								}
								catch (const BundleBuilder::DiscardBlockException &ex) {
									_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " BUNDLE BLOCK DISCARDED" << std::endl;
								}
								catch (const PlainDeserializer::PlainDeserializerException &ex){
									_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " BUNDLE BLOCK ADD FAILED" << std::endl;
								}
							}
							else if (cmd[2] == "del")
							{
								if (cmd.size() < 4) throw ibrcommon::Exception("not enough parameters");

								int offset;
								std::istringstream ss(cmd[3]);

								ss >> offset;
								if (ss.fail()) throw ibrcommon::Exception("malformed command");

								dtn::data::Bundle::iterator it = _bundle_reg.begin();
								std::advance(it, offset);
								_bundle_reg.erase(it);

								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_OK << " BUNDLE BLOCK DEL SUCCESSFUL" << std::endl;
							}
							else
							{
								ibrcommon::MutexLock l(_write_lock);
								_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
							}
						}
						else
						{
							ibrcommon::MutexLock l(_write_lock);
							_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
						}
					}
					else if (cmd[0] == "payload")
					{
						// check if there are more commands/parameters
						if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

						// check if the command is valid
						// [block-offset] get [[data-offset] [length]]

						dtn::data::Bundle::iterator block_it = _bundle_reg.begin();

						size_t cmd_index = 1;

						// check if a block offset is present
						std::stringstream ss(cmd[1]);
						size_t block_offset;
						ss >> block_offset;

						if (!ss.fail()) {
							// block offset present
							// move forward to the selected block
							std::advance(block_it, block_offset);

							// increment command index
							++cmd_index;
						} else {
							// search for the payload block
							block_it = _bundle_reg.find(dtn::data::PayloadBlock::BLOCK_TYPE);
						}

						// check if a valid block was selected
						if (block_it == _bundle_reg.end()) {
							throw ibrcommon::Exception("invalid offset or no payload block found");
						}

						// get the selected block
						dtn::data::Block &block = dynamic_cast<dtn::data::Block&>(**block_it);

						size_t cmd_remaining = cmd.size() - (cmd_index + 1);
						if (cmd[cmd_index] == "get")
						{
							// lock the API stream
							ibrcommon::MutexLock l(_write_lock);

							try {
								dtn::api::PlainSerializer ps(_stream, _encoding);

								if (cmd_remaining > 0)
								{
									size_t payload_offset = 0;
									size_t length = 0;

									/* read the payload offset */
									ss.clear(); ss.str(cmd[cmd_index+1]); ss >> payload_offset;

									if (cmd_remaining > 1)
									{
										ss.clear(); ss.str(cmd[cmd_index+2]); ss >> length;
									}

									// abort here if the stream is no payload block
									try {
										dtn::data::PayloadBlock &pb = dynamic_cast<dtn::data::PayloadBlock&>(block);

										// open the payload BLOB
										ibrcommon::BLOB::Reference ref = pb.getBLOB();
										ibrcommon::BLOB::iostream stream = ref.iostream();

										if (static_cast<std::streamsize>(payload_offset) >= stream.size())
											throw ibrcommon::Exception("offset out of range");

										size_t remaining = stream.size() - payload_offset;

										if ((length > 0) && (remaining > length)) {
											remaining = length;
										}

										// ignore all bytes leading the offset
										(*stream).ignore(payload_offset);

										_stream << ClientHandler::API_STATUS_OK << " PAYLOAD GET" << std::endl;

										// write the payload
										ps.writeData((*stream), remaining);

										// final line break (mark the end)
										_stream << std::endl;
									} catch (const std::bad_cast&) {
										_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " PAYLOAD GET FAILED INVALID BLOCK TYPE" << std::endl;
									}
								}
								else
								{
									_stream << ClientHandler::API_STATUS_OK << " PAYLOAD GET" << std::endl;

									// write the payload
									ps.writeData(block);

									// final line break (mark the end)
									_stream << std::endl;
								}
							} catch (const std::exception &ex) {
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " PAYLOAD GET FAILED " << ex.what() << std::endl;
							}
						}
						else if (cmd[cmd_index] == "put")
						{
							ibrcommon::MutexLock l(_write_lock);

							// abort there if the stream is no payload block
							try {
								dtn::data::PayloadBlock &pb = dynamic_cast<dtn::data::PayloadBlock&>(block);

								// write continue request to API
								_stream << ClientHandler::API_STATUS_CONTINUE << " PAYLOAD PUT" << std::endl;

								size_t payload_offset = 0;
								if (cmd_remaining > 0)
								{
									/* read the payload offset */
									ss.clear(); ss.str(cmd[cmd_index+1]); ss >> payload_offset;
								}

								// open the payload BLOB
								ibrcommon::BLOB::Reference ref = pb.getBLOB();
								ibrcommon::BLOB::iostream stream = ref.iostream();

								// if the offset is valid
								if (static_cast<std::streamsize>(payload_offset) < stream.size()) {
									// move the streams put pointer to the given offset
									(*stream).seekp(payload_offset, std::ios_base::beg);
								} else if (payload_offset > 0) {
									// move put-pointer to the end
									(*stream).seekp(0, std::ios_base::end);
								}

								/* write the new data into the blob */
								PlainDeserializer(_stream).readData(*stream);

								_stream << ClientHandler::API_STATUS_OK << " PAYLOAD PUT SUCCESSFUL" << std::endl;
							} catch (std::bad_cast&) {
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " PAYLOAD PUT FAILED INVALID BLOCK TYPE" << std::endl;
							} catch (const std::exception&) {
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " PAYLOAD PUT FAILED" << std::endl;
							}
						}
						else if (cmd[cmd_index] == "append")
						{
							ibrcommon::MutexLock l(_write_lock);

							// abort there if the stream is no payload block
							try {
								dtn::data::PayloadBlock &pb = dynamic_cast<dtn::data::PayloadBlock&>(block);

								// write continue request to API
								_stream << ClientHandler::API_STATUS_CONTINUE << " PAYLOAD APPEND" << std::endl;

								// open the payload BLOB
								ibrcommon::BLOB::Reference ref = pb.getBLOB();
								ibrcommon::BLOB::iostream stream = ref.iostream();

								// move put-pointer to the end
								(*stream).seekp(0, std::ios_base::end);

								/* write the new data into the blob */
								PlainDeserializer(_stream).readData(*stream);

								_stream << ClientHandler::API_STATUS_OK << " PAYLOAD APPEND SUCCESSFUL" << std::endl;
							} catch (std::bad_cast&) {
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " PAYLOAD APPEND FAILED INVALID BLOCK TYPE" << std::endl;
							} catch (const std::exception&) {
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " PAYLOAD APPEND FAILED" << std::endl;
							}
						}
						else if (cmd[cmd_index] == "clear")
						{
							ibrcommon::MutexLock l(_write_lock);
							// abort there if the stream is no payload block
							try {
								dtn::data::PayloadBlock &pb = dynamic_cast<dtn::data::PayloadBlock&>(block);

								// open the payload BLOB
								ibrcommon::BLOB::Reference ref = pb.getBLOB();
								ibrcommon::BLOB::iostream stream = ref.iostream();

								// clear the payload
								stream.clear();

								_stream << ClientHandler::API_STATUS_OK << " PAYLOAD CLEAR SUCCESSFUL" << std::endl;
							} catch (std::bad_cast&) {
								_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " PAYLOAD CLEAR FAILED INVALID BLOCK TYPE" << std::endl;
							}
						}
						else if (cmd[cmd_index] == "length")
						{
							ibrcommon::MutexLock l(_write_lock);
							_stream << ClientHandler::API_STATUS_OK << " PAYLOAD LENGTH" << std::endl;
							_stream << "Length: " << block.getLength() << std::endl;
						}
					}
					else if (cmd[0] == "nodename")
					{
						ibrcommon::MutexLock l(_write_lock);
						_stream << ClientHandler::API_STATUS_OK << " NODENAME " << dtn::core::BundleCore::local.getString() << std::endl;
					}
					else
					{
						ibrcommon::MutexLock l(_write_lock);
						_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
					}
				} catch (const std::exception&) {
					ibrcommon::MutexLock l(_write_lock);
					_stream << ClientHandler::API_STATUS_BAD_REQUEST << " ERROR" << std::endl;
				}
			}
		}

		ExtendedApiHandler::Sender::Sender(ExtendedApiHandler &conn)
		 : _handler(conn)
		{
		}

		ExtendedApiHandler::Sender::~Sender()
		{
			ibrcommon::JoinableThread::join();
		}

		void ExtendedApiHandler::Sender::__cancellation() throw ()
		{
			// abort all blocking calls on the registration object
			_handler._client.getRegistration().abort();
		}

		void ExtendedApiHandler::Sender::finally() throw ()
		{
		}

		void ExtendedApiHandler::Sender::run() throw ()
		{
			Registration &reg = _handler._client.getRegistration();
			try{
				while(_handler.good()){
					try{
						dtn::data::MetaBundle id = reg.receiveMetaBundle();

						if (id.procflags & dtn::data::PrimaryBlock::APPDATA_IS_ADMRECORD) {
							// transform custody signals & status reports into notifies
							_handler.notifyAdministrativeRecord(id);

							// announce the delivery of this bundle
							_handler._client.getRegistration().delivered(id);
						} else {
							// notify the client about the new bundle
							_handler.notifyBundle(id);
						}
					} catch (const dtn::storage::NoBundleFoundException&) {
						reg.wait_for_bundle();
					}

					yield();
				}
			} catch (const ibrcommon::QueueUnblockedException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("ExtendedApiHandler", 40) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				return;
			} catch (const ibrcommon::IOException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("ExtendedApiHandler", 10) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			} catch (const dtn::InvalidDataException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("ExtendedApiHandler", 10) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			} catch (const std::exception &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("ExtendedApiHandler", 10) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}

			try {
				//FIXME
//				_handler.stop();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("ExtendedApiHandler", 50) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void ExtendedApiHandler::notifyBundle(dtn::data::MetaBundle &bundle)
		{
			// put the bundle into the API queue
			_bundle_queue.push(bundle);

			// lock the API channel
			ibrcommon::MutexLock l(_write_lock);

			// write notification header to API channel
			_stream << API_STATUS_NOTIFY_BUNDLE << " NOTIFY BUNDLE ";

			// format the bundle ID and write it to the stream
			sayBundleID(_stream, bundle);

			// finalize statement with a line-break
			_stream << std::endl;
		}

		void ExtendedApiHandler::notifyAdministrativeRecord(dtn::data::MetaBundle &bundle)
		{
			// load the whole bundle
			const dtn::data::Bundle b = dtn::core::BundleCore::getInstance().getStorage().get(bundle);

			// get the payload block of the bundle
			const dtn::data::PayloadBlock &payload = b.find<dtn::data::PayloadBlock>();

			try {
				// try to decode as status report
				dtn::data::StatusReportBlock report;
				report.read(payload);

				// lock the API channel
				ibrcommon::MutexLock l(_write_lock);

				// write notification header to API channel
				_stream << API_STATUS_NOTIFY_REPORT << " NOTIFY REPORT ";

				// write sender EID
				_stream << b.source.getString() << " ";

				// format the bundle ID and write it to the stream
				_stream << report.bundleid.timestamp.toString() << "." << report.bundleid.sequencenumber.toString();

				if (report.bundleid.isFragment()) {
					_stream << "." << report.bundleid.fragmentoffset.toString() << ":" << report.bundleid.getPayloadLength() << " ";
				} else {
					_stream << " ";
				}

				// origin source
				_stream << report.bundleid.source.getString() << " ";

				// reason code
				_stream << (int)report.reasoncode << " ";

				if (report.status & dtn::data::StatusReportBlock::RECEIPT_OF_BUNDLE)
					_stream << "RECEIPT[" << report.timeof_receipt.getTimestamp().toString() << "."
						<< report.timeof_receipt.getNanoseconds().toString() << "] ";

				if (report.status & dtn::data::StatusReportBlock::CUSTODY_ACCEPTANCE_OF_BUNDLE)
					_stream << "CUSTODY-ACCEPTANCE[" << report.timeof_custodyaccept.getTimestamp().toString() << "."
						<< report.timeof_custodyaccept.getNanoseconds().toString() << "] ";

				if (report.status & dtn::data::StatusReportBlock::FORWARDING_OF_BUNDLE)
					_stream << "FORWARDING[" << report.timeof_forwarding.getTimestamp().toString() << "."
						<< report.timeof_forwarding.getNanoseconds().toString() << "] ";

				if (report.status & dtn::data::StatusReportBlock::DELIVERY_OF_BUNDLE)
					_stream << "DELIVERY[" << report.timeof_delivery.getTimestamp().toString() << "."
						<< report.timeof_delivery.getNanoseconds().toString() << "] ";

				if (report.status & dtn::data::StatusReportBlock::DELETION_OF_BUNDLE)
					_stream << "DELETION[" << report.timeof_deletion.getTimestamp().toString() << "."
						<< report.timeof_deletion.getNanoseconds().toString() << "] ";

				// finalize statement with a line-break
				_stream << std::endl;

				return;
			} catch (const dtn::data::StatusReportBlock::WrongRecordException&) {
				// this is not a status report
			}

			try {
				// try to decode as custody signal
				dtn::data::CustodySignalBlock custody;
				custody.read(payload);

				// lock the API channel
				ibrcommon::MutexLock l(_write_lock);

				// write notification header to API channel
				_stream << API_STATUS_NOTIFY_CUSTODY << " NOTIFY CUSTODY ";

				// write sender EID
				_stream << b.source.getString() << " ";

				// format the bundle ID and write it to the stream
				_stream << custody.bundleid.timestamp.toString() << "." << custody.bundleid.sequencenumber.toString();

				if (custody.bundleid.isFragment()) {
					_stream << "." << custody.bundleid.fragmentoffset.toString() << ":" << custody.bundleid.getPayloadLength() << " ";
				} else {
					_stream << " ";
				}

				// origin source
				_stream << custody.bundleid.source.getString() << " ";

				if (custody.custody_accepted) {
					_stream << "ACCEPTED ";
				} else {
					_stream << "REJECTED(" << (int)custody.reason << ") ";
				}

				// add time of signal to the message
				_stream << custody.timeofsignal.getTimestamp().toString() << "." << custody.timeofsignal.getNanoseconds().toString();

				// finalize statement with a line-break
				_stream << std::endl;

				return;
			} catch (const dtn::data::CustodySignalBlock::WrongRecordException&) {
				// this is not a custody report
			}
		}

		void ExtendedApiHandler::sayBundleID(std::ostream &stream, const dtn::data::BundleID &id)
		{
			stream << id.timestamp.toString() << " " << id.sequencenumber.toString() << " ";

			if (id.isFragment())
			{
				stream << id.fragmentoffset.toString() << " ";
				stream << id.getPayloadLength() << " ";
			}

			stream << id.source.getString();
		}

		dtn::data::BundleID ExtendedApiHandler::readBundleID(const std::vector<std::string> &data, const size_t start)
		{
			// load bundle id
			std::stringstream ss;
			dtn::data::BundleID id;

			if ((data.size() - start) < 3)
			{
				throw ibrcommon::Exception("not enough parameters");
			}

			// read timestamp
			ss.clear(); ss.str(data[start]);
			id.timestamp.read(ss);

			if(ss.fail())
			{
			throw ibrcommon::Exception("malformed parameters");
			}

			// read sequence number
			ss.clear(); ss.str(data[start+1]);
			id.sequencenumber.read(ss);

			if(ss.fail())
			{
			throw ibrcommon::Exception("malformed parameters");
			}

			// read fragment offset
			if ((data.size() - start) > 3)
			{
				id.setFragment(true);

				// read sequence number
				ss.clear(); ss.str(data[start+2]);
				id.fragmentoffset.read(ss);

				// read sequence number
				ss.clear(); ss.str(data[start+3]);
				dtn::data::Length len = 0;
				ss >> len;
				id.setPayloadLength(len);

				if(ss.fail())
				{
					throw ibrcommon::Exception("malformed parameters");
				}
			}

			// read EID
			id.source = dtn::data::EID(data[data.size() - 1]);

			// return bundle id
			return id;
		}
	}
}
