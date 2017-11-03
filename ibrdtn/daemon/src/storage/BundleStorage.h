/*
 * BundleStorage.h
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

#ifndef BUNDLESTORAGE_H_
#define BUNDLESTORAGE_H_

#include <storage/BundleSeeker.h>
#include <storage/BundleResult.h>
#include <storage/BundleIndex.h>
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/BundleSet.h>
#include <ibrdtn/data/BundleID.h>
#include <ibrdtn/data/MetaBundle.h>
#include <ibrdtn/data/CustodySignalBlock.h>
#include <ibrcommon/data/BloomFilter.h>
#include <ibrcommon/thread/Mutex.h>

#include <stdexcept>
#include <iterator>
#include <set>

namespace dtn
{
	namespace storage
	{
		class BundleStorage : public BundleSeeker
		{
		public:
			class BundleLoadException : public NoBundleFoundException
			{
			public:
				BundleLoadException(std::string what = "Error while loading bundle data.") throw() : NoBundleFoundException(what)
				{
				};
			};

			class StorageSizeExeededException : public ibrcommon::Exception
			{
			public:
				StorageSizeExeededException(std::string what = "No space left in the storage.") throw() : ibrcommon::Exception(what)
				{
				};
			};

			/**
			 * destructor
			 */
			virtual ~BundleStorage() = 0;

			/**
			 * Stores a bundle in the storage.
			 * @param bundle The bundle to store.
			 */
			virtual void store(const dtn::data::Bundle &bundle) = 0;

			/**
			 * This method returns true if the requested bundle is
			 * stored in the storage.
			 */
			virtual bool contains(const dtn::data::BundleID &id) = 0;

			/**
			 * Get meta data about a specific bundle ID
			 */
			virtual dtn::data::MetaBundle info(const dtn::data::BundleID &id) = 0;

			/**
			 * This method returns a specific bundle which is identified by its id.
			 * @param id The ID of the bundle to return.
			 * @return A bundle object.
			 */
			virtual dtn::data::Bundle get(const dtn::data::BundleID &id) = 0;

			/**
			 * @see BundleSeeker::get(BundleSelector &cb, BundleResult &result)
			 */
			virtual void get(const BundleSelector &cb, BundleResult &result) throw (NoBundleFoundException, BundleSelectorException) = 0;

			/**
			 * @see BundleSeeker::getDistinctDestinations()
			 */
			virtual const eid_set getDistinctDestinations() = 0;

			/**
			 * This method deletes a specific bundle in the storage.
			 * No reports will be generated here.
			 * @param id The ID of the bundle to remove.
			 */
			virtual void remove(const dtn::data::BundleID &id) = 0;

			/**
			 * This method deletes a specific bundle in the storage.
			 * No reports will be generated here.
			 * @param b The bundle to remove.
			 */
			void remove(const dtn::data::Bundle &b);

			/**
			 * Clears all bundles and fragments in the storage.
			 */
			virtual void clear() {};

			/**
			 * @return True, if no bundles in the storage.
			 */
			virtual bool empty() { return true; };

			/**
			 * @return the count of bundles in the storage
			 */
			virtual dtn::data::Size count() { return 0; };

			/**
			 * Get the current size
			 */
			dtn::data::Length size() const;

			/**
			 * This method is called if another node accepts custody for a
			 * bundle of us.
			 * @param bundle
			 */
			virtual void releaseCustody(const dtn::data::EID &custodian, const dtn::data::BundleID &id) = 0;

			/**
			 * Accept custody for a given bundle. The previous custodian gets notified
			 * with a custody accept message.
			 * @param bundle
			 * @return The new custodian.
			 */
			const dtn::data::EID acceptCustody(const dtn::data::MetaBundle &meta);

			/**
			 * Reject custody for a given bundle. The custodian of this bundle gets notified
			 * with a custody reject message.
			 * @param bundle
			 */
			void rejectCustody(const dtn::data::MetaBundle &meta, dtn::data::CustodySignalBlock::REASON_CODE reason = dtn::data::CustodySignalBlock::NO_ADDITIONAL_INFORMATION);

			/**
			 * attach an index to this storage
			 */
			void attach(dtn::storage::BundleIndex *index);

			/**
			 * detach a specific bundle index
			 */
			void detach(dtn::storage::BundleIndex *index);

			/*** BEGIN: methods for unit-testing ***/

			/**
			 * Wait until all the data has been stored and is not cached anymore
			 */
			virtual void wait() { };

			/**
			 * Set the storage to faulty. If set to true, each try to store
			 * or retrieve a bundle will fail.
			 */
			virtual void setFaulty(bool mode) { _faulty = mode; };

			/*** END: methods for unit-testing ***/

		protected:
			/**
			 * constructor
			 */
			BundleStorage(const dtn::data::Length &maxsize);

			void allocSpace(const dtn::data::Length &size) throw (StorageSizeExeededException);
			void freeSpace(const dtn::data::Length &size) throw ();
			void clearSpace() throw ();

			void eventBundleAdded(const dtn::data::MetaBundle &b) throw ();
			void eventBundleRemoved(const dtn::data::BundleID &id) throw ();

			bool _faulty;

		private:
			ibrcommon::Mutex _sizelock;
			const dtn::data::Length _maxsize;
			dtn::data::Length _currentsize;

			ibrcommon::Mutex _index_lock;
			typedef std::set<dtn::storage::BundleIndex*> index_list;
			index_list _indexes;
		};
	}
}

#endif /* BUNDLESTORAGE_H_ */
