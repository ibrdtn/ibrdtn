/*
 * SchedulingBundleIndex.cpp
 *
 *  Created on: 21.01.2013
 *      Author: morgenro
 */

#include "routing/SchedulingBundleIndex.h"

namespace dtn
{
	namespace routing
	{
		SchedulingBundleIndex::SchedulingBundleIndex()
		{
		}

		SchedulingBundleIndex::~SchedulingBundleIndex()
		{
		}

		void SchedulingBundleIndex::add(const dtn::data::MetaBundle &b)
		{
			ibrcommon::MutexLock l(_index_mutex);
			_priority_index.insert(b);
		}

		void SchedulingBundleIndex::remove(const dtn::data::BundleID &id)
		{
			ibrcommon::MutexLock l(_index_mutex);
			for (priority_index::const_iterator iter = _priority_index.begin(); iter != _priority_index.end(); ++iter)
			{
				const dtn::data::MetaBundle &b = (*iter);
				if (id == (const dtn::data::BundleID&)b) {
					_priority_index.erase(iter);
					return;
				}
			}
		}

		void SchedulingBundleIndex::get(const dtn::storage::BundleSelector &cb, dtn::storage::BundleResult &result) noexcept (false)
		{
			bool unlimited = (cb.limit() <= 0);
			size_t added = 0;

			ibrcommon::MutexLock l(_index_mutex);
			for (priority_index::const_iterator iter = _priority_index.begin(); iter != _priority_index.end(); ++iter)
			{
				const dtn::data::MetaBundle &b = (*iter);
				if (cb.addIfSelected(result, b)) {
					added++;
				}

				if (!unlimited && (added >= cb.limit())) break;
			}

			if (added == 0)
				throw dtn::storage::NoBundleFoundException();
		}

		const std::set<dtn::data::EID> SchedulingBundleIndex::getDistinctDestinations()
		{
			std::set<dtn::data::EID> ret;

			ibrcommon::MutexLock l(_index_mutex);
			for (priority_index::const_iterator iter = _priority_index.begin(); iter != _priority_index.end(); ++iter)
			{
				const dtn::data::MetaBundle &b = (*iter);
				ret.insert(b.destination);
			}

			return ret;
		}
	} /* namespace routing */
} /* namespace dtn */
