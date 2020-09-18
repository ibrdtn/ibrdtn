/*
 * FileConvergenceLayer.cpp
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
#include "net/FileConvergenceLayer.h"
#include "net/TransferAbortedEvent.h"
#include "net/BundleReceivedEvent.h"
#include "core/EventDispatcher.h"
#include "core/BundleEvent.h"
#include "core/BundleCore.h"
#include "routing/BaseRouter.h"
#include "routing/NodeHandshake.h"
#include <ibrdtn/data/BundleSet.h>
#include <ibrdtn/data/ScopeControlHopLimitBlock.h>
#include <ibrdtn/utils/Clock.h>
#include <ibrcommon/data/File.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/MutexLock.h>

namespace dtn
{
	namespace net
	{
		FileConvergenceLayer::Task::Task(FileConvergenceLayer::Task::Action a, const dtn::core::Node &n)
		 : action(a), node(n)
		{
		}

		FileConvergenceLayer::Task::~Task()
		{
		}

		FileConvergenceLayer::StoreBundleTask::StoreBundleTask(const dtn::core::Node &n, const dtn::net::BundleTransfer &j)
		 : FileConvergenceLayer::Task(TASK_STORE, n), job(j)
		{
		}

		FileConvergenceLayer::StoreBundleTask::~StoreBundleTask()
		{
		}

		FileConvergenceLayer::FileConvergenceLayer()
		{
		}

		FileConvergenceLayer::~FileConvergenceLayer()
		{
		}

		void FileConvergenceLayer::componentUp() noexcept
		{
			// routine checked for noexcept on 15.02.2013
			dtn::core::EventDispatcher<dtn::core::NodeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);
		}

		void FileConvergenceLayer::componentDown() noexcept
		{
			// routine checked for noexcept on 15.02.2013
			dtn::core::EventDispatcher<dtn::core::NodeEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
		}

		void FileConvergenceLayer::__cancellation() noexcept
		{
			_tasks.abort();
		}

		void FileConvergenceLayer::componentRun() noexcept
		{
			try {
				while (true)
				{
					Task *t = _tasks.poll();

					try {
						switch (t->action)
						{
							case Task::TASK_LOAD:
							{
								// load bundles (receive)
								load(t->node);
								break;
							}

							case Task::TASK_STORE:
							{
								try {
									StoreBundleTask &sbt = dynamic_cast<StoreBundleTask&>(*t);
									dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();

									// get the file path of the node
									ibrcommon::File path = getPath(sbt.node);

									// scan for bundles
									std::list<dtn::data::MetaBundle> bundles = scan(path);

									// create a filter context
									dtn::core::FilterContext context;
									context.setPeer(sbt.job.getNeighbor());
									context.setProtocol(getDiscoveryProtocol());

									try {
										// check if bundle is a routing bundle
										const dtn::data::EID &source = sbt.job.getBundle().source;

										if (source.isApplication("routing"))
										{
											// read the bundle out of the storage
											dtn::data::Bundle bundle = storage.get(sbt.job.getBundle());
											const dtn::data::MetaBundle meta = dtn::data::MetaBundle::create(bundle);

											if (bundle.destination.isApplication("routing"))
											{
												// push bundle through the filter routines
												context.setBundle(bundle);
												BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::OUTPUT, context, bundle);

												if (ret != BundleFilter::ACCEPT)
												{
													sbt.job.abort(dtn::net::TransferAbortedEvent::REASON_REFUSED_BY_FILTER);
													continue;
												}

												// add this bundle to the blacklist
												{
													ibrcommon::MutexLock l(_blacklist_mutex);
													if (_blacklist.find(meta) != _blacklist.end())
													{
														// send transfer aborted event
														sbt.job.abort(dtn::net::TransferAbortedEvent::REASON_REFUSED);
														continue;
													}
													_blacklist.add(meta);
												}

												// create ECM reply
												replyHandshake(bundle, bundles);

												// raise bundle event
												sbt.job.complete();
												continue;
											}
										}

										// check if bundle is already in the path
										for (std::list<dtn::data::MetaBundle>::const_iterator iter = bundles.begin(); iter != bundles.end(); ++iter)
										{
											if ((*iter) == sbt.job.getBundle())
											{
												// send transfer aborted event
												sbt.job.abort(dtn::net::TransferAbortedEvent::REASON_REFUSED);
												continue;
											}
										}

										ibrcommon::TemporaryFile filename(path, "bundle");

										try {
											// read the bundle out of the storage
											dtn::data::Bundle bundle = storage.get(sbt.job.getBundle());

											// push bundle through the filter routines
											context.setBundle(bundle);
											BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::OUTPUT, context, bundle);

											if (ret != BundleFilter::ACCEPT)
											{
												sbt.job.abort(dtn::net::TransferAbortedEvent::REASON_REFUSED_BY_FILTER);
												continue;
											}

											std::fstream fs(filename.getPath().c_str(), std::fstream::out);

											IBRCOMMON_LOGGER_TAG("FileConvergenceLayer", info) << "write bundle " << sbt.job.getBundle().toString() << " to file " << filename.getPath() << IBRCOMMON_LOGGER_ENDL;

											dtn::data::DefaultSerializer s(fs);

											// serialize the bundle
											s << bundle;

											// raise bundle event
											sbt.job.complete();
										} catch (const ibrcommon::Exception&) {
											filename.remove();
											throw;
										}
									} catch (const dtn::storage::NoBundleFoundException&) {
										// send transfer aborted event
										sbt.job.abort(dtn::net::TransferAbortedEvent::REASON_BUNDLE_DELETED);
									} catch (const ibrcommon::Exception&) {
										// something went wrong - requeue transfer for later
									}

								} catch (const std::bad_cast&) { }
								break;
							}
						}
					} catch (const std::exception &ex) {
						IBRCOMMON_LOGGER_TAG("FileConvergenceLayer", error) << "error while processing file convergencelayer task: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}
					delete t;
				}
			} catch (const ibrcommon::QueueUnblockedException &ex) { };
		}

		void FileConvergenceLayer::raiseEvent(const dtn::core::NodeEvent &node) noexcept
		{
			if (node.getAction() == dtn::core::NODE_AVAILABLE)
			{
				const dtn::core::Node &n = node.getNode();
				if ( n.has(dtn::core::Node::CONN_FILE) )
				{
					_tasks.push(new Task(Task::TASK_LOAD, n));
				}
			}
		}

		void FileConvergenceLayer::raiseEvent(const dtn::core::TimeEvent &time) noexcept
		{
			if (time.getAction() == dtn::core::TIME_SECOND_TICK)
			{
				ibrcommon::MutexLock l(_blacklist_mutex);
				_blacklist.expire(time.getTimestamp());
			}
		}

		const std::string FileConvergenceLayer::getName() const
		{
			return "FileConvergenceLayer";
		}

		dtn::core::Node::Protocol FileConvergenceLayer::getDiscoveryProtocol() const
		{
			return dtn::core::Node::CONN_FILE;
		}

		void FileConvergenceLayer::open(const dtn::core::Node&)
		{
		}

		void FileConvergenceLayer::load(const dtn::core::Node &n)
		{
			std::list<dtn::data::MetaBundle> ret;
			std::list<ibrcommon::File> files;

			// list all files in the folder
			getPath(n).getFiles(files);

			// get a reference to the router
			dtn::routing::BaseRouter &router = dtn::core::BundleCore::getInstance().getRouter();

			// create a filter context
			dtn::core::FilterContext context;
			context.setPeer(n.getEID());
			context.setProtocol(getDiscoveryProtocol());

			for (std::list<ibrcommon::File>::const_iterator iter = files.begin(); iter != files.end(); ++iter)
			{
				const ibrcommon::File &f = (*iter);

				// skip system files
				if (f.isSystem()) continue;

				try {
					// open the file
					std::fstream fs(f.getPath().c_str(), std::fstream::in);

					// get a deserializer
					dtn::data::DefaultDeserializer d(fs);

					dtn::data::MetaBundle bundle;

					// load meta data
					d >> bundle;

					// check the bundle
					if ( ( bundle.destination == EID() ) || ( bundle.source == EID() ) )
					{
						// invalid bundle!
						throw dtn::data::Validator::RejectedException("destination or source EID is null");
					}

					// ask if the bundle is already known
					if ( router.isKnown(bundle) ) continue;
				} catch (const std::exception&) {
					// bundle could not be read
					continue;
				}

				try {
					// open the file
					std::fstream fs(f.getPath().c_str(), std::fstream::in);

					// get a deserializer
					dtn::data::DefaultDeserializer d(fs, dtn::core::BundleCore::getInstance());

					dtn::data::Bundle bundle;

					// load meta data
					d >> bundle;

					// push bundle through the filter routines
					context.setBundle(bundle);
					BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::INPUT, context, bundle);

					if (ret == BundleFilter::ACCEPT)
					{
						// raise default bundle received event
						dtn::net::BundleReceivedEvent::raise(n.getEID(), bundle, false);
					}
				}
				catch (const dtn::data::Validator::RejectedException &ex)
				{
					// display the rejection
					IBRCOMMON_LOGGER_DEBUG_TAG("FileConvergenceLayer", 2) << "bundle has been rejected: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}
				catch (const dtn::InvalidDataException &ex) {
					// display the rejection
					IBRCOMMON_LOGGER_DEBUG_TAG("FileConvergenceLayer", 2) << "invalid bundle-data received: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}
			}
		}

		ibrcommon::File FileConvergenceLayer::getPath(const dtn::core::Node &n)
		{
			std::list<dtn::core::Node::URI> uris = n.get(dtn::core::Node::CONN_FILE);

			// abort the transfer, if no URI exists
			if (uris.empty()) throw ibrcommon::Exception("path not defined");

			// get the URI of the file path
			const std::string &uri = uris.front().value;

			if (uri.substr(0, 7) != "file://") throw ibrcommon::Exception("path invalid");

			return ibrcommon::File(uri.substr(7, uri.length() - 7));
		}

		std::list<dtn::data::MetaBundle> FileConvergenceLayer::scan(const ibrcommon::File &path)
		{
			std::list<dtn::data::MetaBundle> ret;
			std::list<ibrcommon::File> files;

			// list all files in the folder
			path.getFiles(files);

			for (std::list<ibrcommon::File>::const_iterator iter = files.begin(); iter != files.end(); ++iter)
			{
				const ibrcommon::File &f = (*iter);

				// skip system files
				if (f.isSystem()) continue;

				try {
					// open the file
					std::fstream fs(f.getPath().c_str(), std::fstream::in);

					// get a deserializer
					dtn::data::DefaultDeserializer d(fs);

					dtn::data::MetaBundle meta;

					// load meta data
					d >> meta;

					if (meta.expiretime < dtn::utils::Clock::getTime())
					{
						dtn::core::BundleEvent::raise(meta, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::LIFETIME_EXPIRED);
						throw ibrcommon::Exception("bundle is expired");
					}

					// put the meta bundle in the list
					ret.push_back(meta);
				} catch (const std::exception&) {
					IBRCOMMON_LOGGER_DEBUG_TAG("FileConvergenceLayer", 34) << "bundle in file " << f.getPath() << " invalid or expired" << IBRCOMMON_LOGGER_ENDL;

					// delete the file
					ibrcommon::File(f).remove();
				}
			}

			return ret;
		}

		void FileConvergenceLayer::queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job)
		{
			_tasks.push(new StoreBundleTask(n, job));
		}

		void FileConvergenceLayer::replyHandshake(const dtn::data::Bundle &bundle, std::list<dtn::data::MetaBundle> &bl)
		{
			// read the ecm
			const dtn::data::PayloadBlock &p = bundle.find<dtn::data::PayloadBlock>();
			ibrcommon::BLOB::Reference ref = p.getBLOB();
			dtn::routing::NodeHandshake request;

			// locked within this region
			{
				ibrcommon::BLOB::iostream s = ref.iostream();
				(*s) >> request;
			}

			// if this is a request answer with an summary vector
			if (request.getType() == dtn::routing::NodeHandshake::HANDSHAKE_REQUEST)
			{
				// create a new request for the summary vector of the neighbor
				dtn::routing::NodeHandshake response(dtn::routing::NodeHandshake::HANDSHAKE_RESPONSE);

				if (request.hasRequest(dtn::routing::BloomFilterSummaryVector::identifier))
				{
					// add own summary vector to the message
					dtn::data::BundleSet vec;

					// add bundles in the path
					for (std::list<dtn::data::MetaBundle>::const_iterator iter = bl.begin(); iter != bl.end(); ++iter)
					{
						vec.add(*iter);
					}

					// add bundles from the blacklist
					{
						ibrcommon::MutexLock l(_blacklist_mutex);
						for (std::set<dtn::data::MetaBundle>::const_iterator iter = _blacklist.begin(); iter != _blacklist.end(); ++iter)
						{
							vec.add(*iter);
						}
					}

					// create an item
					dtn::routing::BloomFilterSummaryVector *item = new dtn::routing::BloomFilterSummaryVector(vec);

					// add it to the handshake
					response.addItem(item);
				}

				// create a new bundle
				dtn::data::Bundle answer;

				// set the source of the bundle
				answer.source = bundle.destination;

				// set the destination of the bundle
				answer.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, true);
				answer.destination = bundle.source;

				// limit the lifetime to 60 seconds
				answer.lifetime = 60;

				// set high priority
				answer.set(dtn::data::PrimaryBlock::PRIORITY_BIT1, false);
				answer.set(dtn::data::PrimaryBlock::PRIORITY_BIT2, true);

				dtn::data::PayloadBlock &p = answer.push_back<PayloadBlock>();
				ibrcommon::BLOB::Reference ref = p.getBLOB();

				// serialize the request into the payload
				{
					ibrcommon::BLOB::iostream ios = ref.iostream();
					(*ios) << response;
				}

				// add a schl block
				dtn::data::ScopeControlHopLimitBlock &schl = answer.push_front<dtn::data::ScopeControlHopLimitBlock>();
				schl.setLimit(1);

				// create a filter context
				dtn::core::FilterContext context;
				context.setPeer(answer.source);
				context.setProtocol(getDiscoveryProtocol());

				// push bundle through the filter routines
				context.setBundle(answer);
				BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::INPUT, context, answer);

				if (ret == BundleFilter::ACCEPT)
				{
					// raise default bundle received event
					dtn::net::BundleReceivedEvent::raise(bundle.destination.getNode(), answer, false);
				}
			}
		}
	} /* namespace net */
} /* namespace dtn */
