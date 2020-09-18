/*
 * AbstractWorker.cpp
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
#include "core/EventDispatcher.h"
#include "core/AbstractWorker.h"
#include "core/BundleCore.h"
#include "core/BundleEvent.h"
#include "core/BundlePurgeEvent.h"
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/Logger.h>
#include <typeinfo>

namespace dtn
{
	namespace core
	{
		AbstractWorker::AbstractWorkerAsync::AbstractWorkerAsync(AbstractWorker &worker)
		 : _worker(worker), _running(true)
		{
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::add(this);
		}

		AbstractWorker::AbstractWorkerAsync::~AbstractWorkerAsync()
		{
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::remove(this);
			shutdown();
		}

		void AbstractWorker::AbstractWorkerAsync::raiseEvent(const dtn::routing::QueueBundleEvent &queued) noexcept
		{
			// ignore fragments - we can not deliver them directly to the client
			if (queued.bundle.isFragment()) return;

			// check for bundle destination
			if (queued.bundle.destination == _worker._eid)
			{
				_receive_bundles.push(queued.bundle);
				return;
			}

			// if the bundle is a singleton, stop here
			if (queued.bundle.procflags & dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON) return;

			// check for subscribed groups
			if (_worker._groups.find(queued.bundle.destination) != _worker._groups.end())
			{
				_receive_bundles.push(queued.bundle);
				return;
			}
		}

		void AbstractWorker::AbstractWorkerAsync::initialize()
		{
			// reset thread if necessary
			if (JoinableThread::isFinalized())
			{
				JoinableThread::reset();
				_running = true;
				_receive_bundles.reset();
			}

			JoinableThread::start();
		}

		void AbstractWorker::AbstractWorkerAsync::shutdown()
		{
			_running = false;
			_receive_bundles.abort();

			join();
		}

		void AbstractWorker::AbstractWorkerAsync::run() noexcept
		{
			dtn::storage::BundleStorage &storage = BundleCore::getInstance().getStorage();

			try {
				while (_running)
				{
					dtn::data::BundleID id = _receive_bundles.poll();

					try {
						dtn::data::Bundle b = storage.get( id );

						try {
							// process the bundle block (security, compression, ...)
							dtn::core::BundleCore::processBlocks(b);
						} catch (const ibrcommon::Exception &ex) {
							// delete the bundle from the storage
							storage.remove(id);
							continue;
						}

						// forward the bundle to the application
						_worker.callbackBundleReceived( b );

						// create meta bundle for further processing
						const dtn::data::MetaBundle meta = dtn::data::MetaBundle::create(b);

						// raise bundle event
						dtn::core::BundleEvent::raise(meta, BUNDLE_DELIVERED);

						if (b.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
						{
							// remove the bundle from the storage
							dtn::core::BundlePurgeEvent::raise(meta);
						}
					} catch (const ibrcommon::Exception &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG("AbstractWorker", 15) << ex.what() << IBRCOMMON_LOGGER_ENDL;
					};

					yield();
				}
			} catch (const ibrcommon::QueueUnblockedException&) {
				// queue was aborted by another call
			}
		}

		void AbstractWorker::AbstractWorkerAsync::__cancellation() noexcept
		{
			// cancel the main thread in here
			_receive_bundles.abort();
		}

		AbstractWorker::AbstractWorker() : _eid(BundleCore::local), _thread(*this)
		{
		}

		void AbstractWorker::subscribe(const dtn::data::EID &endpoint)
		{
			_groups.insert(endpoint);
		}

		void AbstractWorker::unsubscribe(const dtn::data::EID &endpoint)
		{
			_groups.erase(endpoint);
		}

		void AbstractWorker::initialize(const std::string &uri)
		{
			_eid.setApplication(uri);

			try {
				_thread.initialize();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG("AbstractWorker", error) << "initialize failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		AbstractWorker::~AbstractWorker()
		{
			shutdown();
		}

		void AbstractWorker::shutdown()
		{
			// wait for the async thread
			_thread.shutdown();
		}

		const EID AbstractWorker::getWorkerURI() const
		{
			return _eid;
		}

		void AbstractWorker::transmit(dtn::data::Bundle &bundle)
		{
			dtn::core::BundleCore::inject(dtn::core::BundleCore::local, bundle);
		}
	}
}
