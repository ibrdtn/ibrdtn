/*
 * RetransmissionExtension.cpp
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

#include "routing/RetransmissionExtension.h"

#include "core/BundleCore.h"
#include "core/EventDispatcher.h"
#include "net/TransferCompletedEvent.h"

#include <ibrdtn/utils/Clock.h>
#include <ibrdtn/data/Exceptions.h>
#include <ibrcommon/thread/MutexLock.h>


namespace dtn
{
	namespace routing
	{
		RetransmissionExtension::RetransmissionExtension()
		{
		}

		RetransmissionExtension::~RetransmissionExtension()
		{
		}

		void RetransmissionExtension::componentUp() noexcept
		{
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::net::TransferAbortedEvent>::add(this);
			dtn::core::EventDispatcher<dtn::routing::RequeueBundleEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::BundleExpiredEvent>::add(this);
		}

		void RetransmissionExtension::componentDown() noexcept
		{
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::net::TransferAbortedEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::routing::RequeueBundleEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::core::BundleExpiredEvent>::remove(this);
		}

		void RetransmissionExtension::eventTransferCompleted(const dtn::data::EID &peer, const dtn::data::MetaBundle &meta) noexcept
		{
			// remove the bundleid in our list
			RetransmissionData data(meta, peer);
			_set.erase(data);
		}

		void RetransmissionExtension::raiseEvent(const dtn::core::TimeEvent &time) noexcept
		{
			ibrcommon::MutexLock l(_mutex);
			if (!_queue.empty())
			{
				const RetransmissionData &data = _queue.front();

				if ( data.getTimestamp() <= time.getTimestamp() )
				{
					try {
						const dtn::data::MetaBundle meta = dtn::core::BundleCore::getInstance().getStorage().info(data);

						try {
							// create a new bundle transfer
							dtn::net::BundleTransfer transfer(data.destination, meta, data.protocol);

							try {
								// re-queue the bundle
								dtn::core::BundleCore::getInstance().getConnectionManager().queue(transfer);
							} catch (const dtn::core::P2PDialupException&) {
								// abort transmission
								transfer.abort(dtn::net::TransferAbortedEvent::REASON_CONNECTION_DOWN);
							}
						} catch (const ibrcommon::Exception&) {
							// do nothing here
						}
					} catch (const ibrcommon::Exception&) {
						// bundle is not available, abort transmission
						dtn::net::TransferAbortedEvent::raise(data.destination, data, dtn::net::TransferAbortedEvent::REASON_BUNDLE_DELETED);
					}

					// remove the item off the queue
					_queue.pop();
				}
			}
		}

		void RetransmissionExtension::raiseEvent(const dtn::net::TransferAbortedEvent &aborted) noexcept
		{
			// remove the bundleid in our list
			RetransmissionData data(aborted.getBundleID(), aborted.getPeer());
			_set.erase(data);
		}

		void RetransmissionExtension::raiseEvent(const dtn::routing::RequeueBundleEvent &requeue) noexcept
		{
			const RetransmissionData data(requeue.getBundle(), requeue.getPeer(), requeue.getProtocol());

			ibrcommon::MutexLock l(_mutex);
			std::set<RetransmissionData>::const_iterator iter = _set.find(data);

			if (iter != _set.end())
			{
				// increment the retry counter
				RetransmissionData data2 = (*iter);
				data2++;

				// remove the item
				_set.erase(data);

				if (data2.getCount() <= 8)
				{
					// requeue the bundle
					_set.insert(data2);
					_queue.push(data2);
				}
				else
				{
					dtn::net::TransferAbortedEvent::raise(requeue.getPeer(), requeue.getBundle(), dtn::net::TransferAbortedEvent::REASON_RETRY_LIMIT_REACHED);
				}
			}
			else
			{
				// queue the bundle
				_set.insert(data);
				_queue.push(data);
			}
		}

		void RetransmissionExtension::raiseEvent(const dtn::core::BundleExpiredEvent &expired) noexcept
		{
			// delete all matching elements in the queue
			ibrcommon::MutexLock l(_mutex);

			dtn::data::Size elements = _queue.size();
			for (dtn::data::Size i = 0; i < elements; ++i)
			{
				const RetransmissionData &data = _queue.front();

				if ((dtn::data::BundleID&)data == expired.getBundle())
				{
					dtn::net::TransferAbortedEvent::raise(data.destination, data, dtn::net::TransferAbortedEvent::REASON_BUNDLE_DELETED);
				}
				else
				{
					_queue.push(data);
				}

				_queue.pop();
			}
		}

		bool RetransmissionExtension::RetransmissionData::operator!=(const RetransmissionData &obj)
		{
			const dtn::data::BundleID &id1 = dynamic_cast<const dtn::data::BundleID&>(obj);
			const dtn::data::BundleID &id2 = dynamic_cast<const dtn::data::BundleID&>(*this);

			if (id1 != id2) return true;
			if (obj.destination != destination) return true;

			return false;
		}

		bool RetransmissionExtension::RetransmissionData::operator==(const RetransmissionData &obj)
		{
			const dtn::data::BundleID &id1 = dynamic_cast<const dtn::data::BundleID&>(obj);
			const dtn::data::BundleID &id2 = dynamic_cast<const dtn::data::BundleID&>(*this);

			if (id1 != id2) return false;
			if (obj.destination != destination) return false;

			return true;
		}

		dtn::data::Size RetransmissionExtension::RetransmissionData::getCount() const
		{
			return _count;
		}

		const dtn::data::Timestamp& RetransmissionExtension::RetransmissionData::getTimestamp() const
		{
			return _timestamp;
		}

		RetransmissionExtension::RetransmissionData& RetransmissionExtension::RetransmissionData::operator++(int)
		{
			_count++;
			_timestamp = dtn::utils::Clock::getTime();
			float backoff = ::pow((float)retry, (int)_count -1);
			_timestamp += static_cast<dtn::data::Size>(backoff);

			return (*this);
		}

		RetransmissionExtension::RetransmissionData& RetransmissionExtension::RetransmissionData::operator++()
		{
			_count++;
			_timestamp = dtn::utils::Clock::getTime();
			float backoff = ::pow((float)retry, (int)_count -1);
			_timestamp += static_cast<dtn::data::Size>(backoff);

			return (*this);
		}

		RetransmissionExtension::RetransmissionData::RetransmissionData(const dtn::data::BundleID &id, const dtn::data::EID &d, dtn::core::Node::Protocol p, const dtn::data::Size r)
		 : dtn::data::BundleID(id), destination(d), protocol(p), _timestamp(0), _count(0), retry(r)
		{
			(*this)++;
		}

		RetransmissionExtension::RetransmissionData::~RetransmissionData()
		{
		}
	}
}
