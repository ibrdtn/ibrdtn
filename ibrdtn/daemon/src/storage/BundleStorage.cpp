/*
 * BundleStorage.cpp
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

#include "core/BundleCore.h"
#include "storage/BundleStorage.h"
#include "core/CustodyEvent.h"
#include <ibrdtn/data/PayloadBlock.h>
#include <ibrdtn/data/BundleID.h>
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/Logger.h>

namespace dtn
{
	namespace storage
	{
		BundleStorage::BundleStorage(const dtn::data::Length &maxsize)
		 : _faulty(false), _maxsize(maxsize), _currentsize(0)
		{
		}

		BundleStorage::~BundleStorage()
		{
		}

		void BundleStorage::remove(const dtn::data::Bundle &b)
		{
			remove(dtn::data::BundleID(b));
		}

		const dtn::data::EID BundleStorage::acceptCustody(const dtn::data::MetaBundle &meta)
		{
			if (!meta.get(Bundle::CUSTODY_REQUESTED))
				throw ibrcommon::Exception("custody transfer is not requested for this bundle.");

			if (meta.custodian == EID())
				throw ibrcommon::Exception("no previous custodian is set.");

			// create a new bundle
			Bundle custody_bundle;

			// set priority to HIGH
			custody_bundle.set(dtn::data::PrimaryBlock::PRIORITY_BIT1, false);
			custody_bundle.set(dtn::data::PrimaryBlock::PRIORITY_BIT2, true);

			// create a custody signal with accept flag
			CustodySignalBlock signal;

			// set the bundle to match
			signal.setMatch(meta);

			// set accepted
			signal.custody_accepted = true;

			// write the custody data to a payload block
			dtn::data::PayloadBlock &payload = custody_bundle.push_back<dtn::data::PayloadBlock>();
			signal.write(payload);

			custody_bundle.set(dtn::data::PrimaryBlock::APPDATA_IS_ADMRECORD, true);
			custody_bundle.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, true);
			custody_bundle.destination = meta.custodian;
			custody_bundle.source = dtn::core::BundleCore::local;

			// always sign custody signals
			custody_bundle.set(dtn::data::PrimaryBlock::DTNSEC_REQUEST_SIGN, true);

			// send the custody accepted bundle
			dtn::core::BundleCore::inject(dtn::core::BundleCore::local, custody_bundle);

			// raise the custody accepted event
			dtn::core::CustodyEvent::raise(meta, dtn::core::CUSTODY_ACCEPT);

			return dtn::core::BundleCore::local;
		}

		void BundleStorage::rejectCustody(const dtn::data::MetaBundle &meta, dtn::data::CustodySignalBlock::REASON_CODE reason)
		{
			if (!meta.get(Bundle::CUSTODY_REQUESTED))
				throw ibrcommon::Exception("custody transfer is not requested for this bundle.");

			if (meta.custodian == EID())
				throw ibrcommon::Exception("no previous custodian is set.");

			// create a new bundle
			Bundle b;

			// create a custody signal with reject flag
			CustodySignalBlock signal;

			// set the bundle to match
			signal.setMatch(meta);

			// set reason code
			signal.reason = reason;

			// write the custody data to a payload block
			dtn::data::PayloadBlock &payload = b.push_back<dtn::data::PayloadBlock>();
			signal.write(payload);

			b.set(dtn::data::PrimaryBlock::APPDATA_IS_ADMRECORD, true);
			b.destination = meta.custodian;
			b.source = dtn::core::BundleCore::local;

			// always sign custody signals
			b.set(dtn::data::PrimaryBlock::DTNSEC_REQUEST_SIGN, true);

			// send the custody rejected bundle
			dtn::core::BundleCore::inject(dtn::core::BundleCore::local, b);

			// raise the custody rejected event
			dtn::core::CustodyEvent::raise(dtn::data::MetaBundle::create(b), dtn::core::CUSTODY_REJECT);
		}

		dtn::data::Length BundleStorage::size() const
		{
			return _currentsize;
		}

		void BundleStorage::allocSpace(const dtn::data::Length &size) noexcept (false)
		{
			ibrcommon::MutexLock l(_sizelock);

			// check if this container is too big for us.
			if ((_maxsize > 0) && (_currentsize + size > _maxsize))
			{
				throw StorageSizeExeededException();
			}

			// increment the storage size
			_currentsize += size;
		}

		void BundleStorage::freeSpace(const dtn::data::Length &size) noexcept
		{
			ibrcommon::MutexLock l(_sizelock);
			if (size > _currentsize)
			{
				_currentsize = 0;
				IBRCOMMON_LOGGER_TAG("BundleStorage", critical) << "More space to free than allocated." << IBRCOMMON_LOGGER_ENDL;
			}
			else
			{
				_currentsize -= size;
			}
		}

		void BundleStorage::clearSpace() noexcept
		{
			ibrcommon::MutexLock l(_sizelock);
			_currentsize = 0;
		}

		void BundleStorage::eventBundleAdded(const dtn::data::MetaBundle &b) noexcept
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("BundleStorage", 2) << "add bundle to index: " << b.toString() << IBRCOMMON_LOGGER_ENDL;

			for (index_list::iterator it = _indexes.begin(); it != _indexes.end(); ++it) {
				BundleIndex &index = (**it);
				index.add(b);
			}
		}

		void BundleStorage::eventBundleRemoved(const dtn::data::BundleID &id) noexcept
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("BundleStorage", 2) << "remove bundle from index: " << id.toString() << IBRCOMMON_LOGGER_ENDL;

			for (index_list::iterator it = _indexes.begin(); it != _indexes.end(); ++it) {
				BundleIndex &index = (**it);
				index.remove(id);
			}
		}

		void BundleStorage::attach(dtn::storage::BundleIndex *index)
		{
			ibrcommon::MutexLock l(_index_lock);
			_indexes.insert(index);
		}

		void BundleStorage::detach(dtn::storage::BundleIndex *index)
		{
			ibrcommon::MutexLock l(_index_lock);
			_indexes.erase(index);
		}
	}
}
