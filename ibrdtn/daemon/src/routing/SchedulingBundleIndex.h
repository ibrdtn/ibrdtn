/*
 * SchedulingBundleIndex.h
 *
 *  Created on: 21.01.2013
 *      Author: morgenro
 */

#ifndef SCHEDULINGBUNDLEINDEX_H_
#define SCHEDULINGBUNDLEINDEX_H_

#include "storage/BundleIndex.h"
#include <ibrdtn/data/Number.h>
#include <ibrcommon/thread/Mutex.h>

namespace dtn
{
	namespace routing
	{
		class SchedulingBundleIndex : public dtn::storage::BundleIndex
		{
		public:
			SchedulingBundleIndex();
			virtual ~SchedulingBundleIndex();

			virtual void add(const dtn::data::MetaBundle &b);
			virtual void remove(const dtn::data::BundleID &id);

			/**
			 * Query the database for a number of bundles. The bundles are selected with the BundleSelector
			 * class which is to implement by the user of this method.
			 * @param cb The instance of the callback filter class.
			 * @return A list of bundles.
			 */
			virtual void get(const dtn::storage::BundleSelector &cb, dtn::storage::BundleResult &result) noexcept (false);

			/**
			 * Return a set of distinct destinations for all bundles in the storage.
			 * @return
			 */
			virtual const std::set<dtn::data::EID> getDistinctDestinations();

		private:
			struct CMP_BUNDLE_PRIORITY
			{
				bool operator() (const dtn::data::MetaBundle& lhs, const dtn::data::MetaBundle& rhs) const
				{
					if (lhs.net_priority > rhs.net_priority)
						return true;
					if (lhs.net_priority != rhs.net_priority)
						return false;

					if (lhs.getPriority() > rhs.getPriority())
						return true;
					if (lhs.getPriority() != rhs.getPriority())
						return false;

					if (lhs.expiretime < rhs.expiretime)
						return true;
					if (lhs.expiretime != rhs.expiretime)
						return false;


					// use payloadlength for not fragmented bundles and
					// appdatalength for fragments
					dtn::data::Length complete_payloadlength_lhs, complete_payloadlength_rhs;

					if (lhs.isFragment())
						complete_payloadlength_lhs = lhs.appdatalength.get<dtn::data::Length>();
					else
						complete_payloadlength_lhs = lhs.getPayloadLength();

					if (rhs.isFragment())
						complete_payloadlength_rhs = rhs.appdatalength.get<dtn::data::Length>();
					else
						complete_payloadlength_rhs = rhs.getPayloadLength();


					if (complete_payloadlength_lhs < complete_payloadlength_rhs)
						return true;
					if (complete_payloadlength_lhs != complete_payloadlength_rhs)
						return false;

					return lhs < rhs;
				}
			};

			typedef std::set<dtn::data::MetaBundle, CMP_BUNDLE_PRIORITY> priority_index;
			priority_index _priority_index;
			ibrcommon::Mutex _index_mutex;
		};
	} /* namespace routing */
} /* namespace dtn */
#endif /* SCHEDULINGBUNDLEINDEX_H_ */
