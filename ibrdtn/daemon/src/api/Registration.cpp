/*
 * Registration.cpp
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
#include "api/Registration.h"
#include "storage/BundleStorage.h"
#include "core/BundleCore.h"
#include "core/BundleEvent.h"
#include "core/BundlePurgeEvent.h"
#include "core/FragmentManager.h"
#include "net/BundleReceivedEvent.h"

#ifdef HAVE_SQLITE
#include "storage/SQLiteBundleStorage.h"
#endif

#include <ibrdtn/data/TrackingBlock.h>
#include <ibrdtn/data/AgeBlock.h>

#include <ibrdtn/utils/Clock.h>
#include <ibrdtn/utils/Random.h>
#include <ibrcommon/Logger.h>

#include <limits.h>
#include <stdint.h>

namespace dtn
{
	namespace api
	{
		const std::string Registration::TAG = "Registration";
		ibrcommon::Mutex Registration::_handle_lock;
		std::set<std::string> Registration::_handles;

		const std::string Registration::gen_handle()
		{
			std::string new_handle = dtn::utils::Random::gen_chars(16);

			// if the local host is configured with an IPN address
			if (dtn::core::BundleCore::local.isCompressable())
			{
				// .. then use 32-bit numbers only
				uint32_t *int_handle = (uint32_t*)new_handle.c_str();
				std::stringstream ss;
				ss << *int_handle;
				new_handle = ss.str();
			}

			return new_handle;
		}

		const std::string& Registration::alloc_handle(const std::string &handle)
		{
			ibrcommon::MutexLock l(_handle_lock);
			std::pair<std::set<std::string>::iterator, bool> ret = _handles.insert(handle);

			while (!ret.second) {
				ret = _handles.insert(gen_handle());
			}

			return (*ret.first);
		}

		const std::string& Registration::alloc_handle()
		{
			return alloc_handle(gen_handle());
		}

		void Registration::free_handle(const std::string &handle)
		{
			ibrcommon::MutexLock l(_handle_lock);
			_handles.erase(handle);
		}

		Registration::Registration(const std::string &handle)
		 : _handle(alloc_handle(handle)),
		   _default_eid(core::BundleCore::local), _no_more_bundles(false),
		   _persistent(false), _detached(false), _expiry(0), _filter_fragments(true)
		{
			_default_eid.setApplication(_handle);
		}

		Registration::Registration()
		 : _handle(alloc_handle()),
		   _default_eid(core::BundleCore::local), _no_more_bundles(false),
		   _persistent(false), _detached(false), _expiry(0), _filter_fragments(true)
		{
			_default_eid.setApplication(_handle);
		}

		Registration::~Registration()
		{
			free_handle(_handle);
		}

		void Registration::notify(const NOTIFY_CALL call)
		{
			ibrcommon::MutexLock l(_wait_for_cond);
			if (call == NOTIFY_BUNDLE_AVAILABLE)
			{
				_no_more_bundles = false;
				_wait_for_cond.signal(true);
			}
			else
			{
				_notify_queue.push(call);
			}
		}

		void Registration::wait_for_bundle(size_t timeout)
		{
			ibrcommon::MutexLock l(_wait_for_cond);

			while (_no_more_bundles)
			{
				if (timeout > 0)
				{
					_wait_for_cond.wait(timeout);
				}
				else
				{
					_wait_for_cond.wait();
				}
			}
		}

		Registration::NOTIFY_CALL Registration::wait()
		{
			return _notify_queue.poll();
		}

		bool Registration::hasSubscribed(const dtn::data::EID &endpoint)
		{
			ibrcommon::MutexLock l(_endpoints_lock);
			return (_endpoints.find(endpoint) != _endpoints.end());
		}

		const std::set<dtn::data::EID> Registration::getSubscriptions()
		{
			ibrcommon::MutexLock l(_endpoints_lock);
			return _endpoints;
		}

		void Registration::delivered(const dtn::data::MetaBundle &m) const
		{
			// raise bundle event
			dtn::core::BundleEvent::raise(m, dtn::core::BUNDLE_DELIVERED);

			if (m.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
			{
				dtn::core::BundlePurgeEvent::raise(m);
			}
		}

		dtn::data::Bundle Registration::receive() noexcept (false)
		{
			// get the global storage
			dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();

			// get the next bundles as MetaBundle
			dtn::data::MetaBundle b = receiveMetaBundle();

			// load the bundle
			return storage.get(b);
		}

		dtn::data::MetaBundle Registration::receiveMetaBundle() noexcept (false)
		{
			ibrcommon::MutexLock l(_receive_lock);

			while(true)
			{
				try {
					// get the first bundle in the queue
					dtn::data::MetaBundle b = _queue.pop();
					return b;
				} catch (const ibrcommon::QueueUnblockedException &e) {
					if (e.reason == ibrcommon::QueueUnblockedException::QUEUE_ABORT)
					{
						IBRCOMMON_LOGGER_DEBUG_TAG(Registration::TAG, 25) << "search for more bundles" << IBRCOMMON_LOGGER_ENDL;

						// query for new bundles
						underflow();
					}
				}
				catch(const dtn::storage::NoBundleFoundException & ){
				}
			}

			throw dtn::storage::NoBundleFoundException();
		}

		void Registration::underflow()
		{
			bool fragment_conf = dtn::daemon::Configuration::getInstance().getNetwork().doFragmentation();

			// expire outdated bundles in the list
			_queue.expire(dtn::utils::Clock::getTime());

			/**
			 * search for bundles in the storage
			 */
#ifdef HAVE_SQLITE
			class BundleFilter : public dtn::storage::BundleSelector, public dtn::storage::SQLiteDatabase::SQLBundleQuery
#else
			class BundleFilter : public dtn::storage::BundleSelector
#endif
			{
			public:
				BundleFilter(const std::set<dtn::data::EID> endpoints, const RegistrationQueue &queue, bool loopback, bool fragment_filter)
				 : _endpoints(endpoints), _queue(queue), _loopback(loopback), _fragment_filter(fragment_filter)
				{};

				virtual ~BundleFilter() {};

				virtual dtn::data::Size limit() const noexcept { return dtn::core::BundleCore::max_bundles_in_transit; };

				virtual bool shouldAdd(const dtn::data::MetaBundle &meta) const noexcept (false)
				{
					// filter fragments if requested
					if (meta.isFragment() && _fragment_filter)
					{
						return false;
					}

					if (_endpoints.find(meta.destination) == _endpoints.end())
					{
						return false;
					}

					// filter own bundles
					if (!_loopback)
					{
						if (_endpoints.find(meta.source) != _endpoints.end())
						{
							return false;
						}
					}

					IBRCOMMON_LOGGER_DEBUG_TAG(Registration::TAG, 30) << "search bundle in the list of delivered bundles: " << meta.toString() << IBRCOMMON_LOGGER_ENDL;

					if (_queue.has(meta))
					{
						return false;
					}

					return true;
				};

#ifdef HAVE_SQLITE
				const std::string getWhere() const noexcept
				{
					if (_endpoints.size() > 1)
					{
						std::string where = "(";

						for (size_t i = _endpoints.size() - 1; i > 0; i--)
						{
							where += "destination = ? OR ";
						}

						return where + "destination = ?)";
					}
					else if (_endpoints.size() == 1)
					{
						return "destination = ?";
					}
					else
					{
						return "destination = null";
					}
				};

				int bind(sqlite3_stmt *st, int offset) const noexcept
				{
					int o = offset;

					for (std::set<dtn::data::EID>::const_iterator iter = _endpoints.begin(); iter != _endpoints.end(); ++iter)
					{
						const std::string data = (*iter).getString();

						sqlite3_bind_text(st, o, data.c_str(), static_cast<int>(data.size()), SQLITE_TRANSIENT);
						o++;
					}

					return o;
				}
#endif

			private:
				const std::set<dtn::data::EID> _endpoints;
				const RegistrationQueue &_queue;
				const bool _loopback;
				const bool _fragment_filter;
			} filter(_endpoints, _queue, false, fragment_conf && _filter_fragments);

			// query the database for more bundles
			ibrcommon::MutexLock l(_endpoints_lock);

			try {
				dtn::core::BundleCore::getInstance().getSeeker().get( filter, _queue );
			} catch (const dtn::storage::NoBundleFoundException&) {
				_no_more_bundles = true;
				throw;
			}
		}

		Registration::RegistrationQueue::RegistrationQueue()
		{
		}

		Registration::RegistrationQueue::~RegistrationQueue()
		{
		}

		void Registration::RegistrationQueue::put(const dtn::data::MetaBundle &bundle) noexcept
		{
			try {
				_queue.push(bundle);

				ibrcommon::MutexLock l(_lock);
				_recv_bundles.add(bundle);

				IBRCOMMON_LOGGER_DEBUG_TAG(Registration::TAG, 10) << "[RegistrationQueue] add bundle to list of delivered bundles: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;
			} catch (const ibrcommon::Exception&) { }
		}

		dtn::data::MetaBundle Registration::RegistrationQueue::pop() noexcept (false)
		{
			return _queue.take();
		}

		bool Registration::RegistrationQueue::has(const dtn::data::BundleID &bundle) const noexcept
		{
			ibrcommon::MutexLock l(const_cast<ibrcommon::Mutex&>(_lock));
			return _recv_bundles.has(bundle);
		}

		void Registration::RegistrationQueue::expire(const dtn::data::Timestamp &timestamp) noexcept
		{
			ibrcommon::MutexLock l(_lock);
			_recv_bundles.expire(timestamp);
		}

		void Registration::RegistrationQueue::abort() noexcept
		{
			_queue.abort();
		}

		void Registration::RegistrationQueue::reset() noexcept
		{
			_queue.reset();
		}

		void Registration::subscribe(const dtn::data::EID &endpoint)
		{
			{
				ibrcommon::MutexLock l(_endpoints_lock);

				// add endpoint to the local set
				_endpoints.insert(endpoint);
			}

			// trigger the search for new bundles
			notify(NOTIFY_BUNDLE_AVAILABLE);
		}

		void Registration::unsubscribe(const dtn::data::EID &endpoint)
		{
			ibrcommon::MutexLock l(_endpoints_lock);
			_endpoints.erase(endpoint);
		}

		/**
		 * compares the local handle with the given one
		 */
		bool Registration::operator==(const std::string &other) const
		{
			return (_handle == other);
		}

		/**
		 * compares another registration with this one
		 */
		bool Registration::operator==(const Registration &other) const
		{
			return (_handle == other._handle);
		}

		/**
		 * compares and order a registration (using the handle)
		 */
		bool Registration::operator<(const Registration &other) const
		{
			return (_handle < other._handle);
		}

		void Registration::abort()
		{
			_queue.abort();
			_notify_queue.abort();

			ibrcommon::MutexLock l(_wait_for_cond);
			_wait_for_cond.abort();
		}

		const dtn::data::EID& Registration::getDefaultEID() const
		{
			return _default_eid;
		}

		const std::string& Registration::getHandle() const
		{
			return _handle;
		}

		void Registration::setPersistent(ibrcommon::Timer::time_t lifetime)
		{
			_expiry = lifetime + ibrcommon::Timer::get_current_time();
			_persistent = true;
		}

		void Registration::unsetPersistent()
		{
			_persistent = false;
		}

		bool Registration::isPersistent()
		{
			if(_expiry <= ibrcommon::Timer::get_current_time())
			{
				_persistent = false;
			}

			return _persistent;
		}

		bool Registration::isPersistent() const
		{
			if(_expiry <= ibrcommon::Timer::get_current_time())
			{
				return false;
			}

			return _persistent;
		}

		void Registration::setFilterFragments(bool val)
		{
			_filter_fragments = val;
		}

		ibrcommon::Timer::time_t Registration::getExpireTime() const
		{
			if(!isPersistent()) throw NotPersistentException("Registration is not persistent.");

			return _expiry;

		}

		void Registration::attach()
		{
			ibrcommon::MutexLock l(_attach_lock);
			if(!_detached) throw AlreadyAttachedException("Registration is already attached to a client.");

			_detached = false;
		}

		void Registration::detach()
		{
			ibrcommon::MutexLock l1(_wait_for_cond);
			ibrcommon::MutexLock l2(_attach_lock);

			_detached = true;

			_queue.reset();
			_notify_queue.reset();

			_wait_for_cond.reset();
		}

		void Registration::processIncomingBundle(const dtn::data::EID &source, dtn::data::Bundle &bundle)
		{
			// check address fields for "api:me", this has to be replaced
			static const dtn::data::EID clienteid("api:me");

			// create a new sequence number
			bundle.relabel();

			// if the relabeling results in a zero timestamp, add an ageblock
			if (bundle.timestamp == 0)
			{
				// check for ageblock
				try {
					bundle.find<dtn::data::AgeBlock>();
				} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) {
					// add a new ageblock
					bundle.push_front<dtn::data::AgeBlock>();
				}
			}

			// set the source address to the sending EID
			bundle.source = source;

			if (bundle.destination == clienteid) bundle.destination = source;
			if (bundle.reportto == clienteid) bundle.reportto = source;
			if (bundle.custodian == clienteid) bundle.custodian = source;

			// inject the bundle
			dtn::core::BundleCore::inject(source, bundle);
		}
	}
}
