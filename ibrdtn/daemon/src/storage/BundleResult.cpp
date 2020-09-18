/*
 * BundleResult.cpp
 *
 *  Created on: 07.12.2012
 *      Author: morgenro
 */

#include "storage/BundleResult.h"

namespace dtn
{
	namespace storage
	{
		BundleResult::BundleResult() {
		}

		BundleResult::~BundleResult() {
		}

		BundleResultList::BundleResultList() {
		}

		BundleResultList::~BundleResultList() {
		}

		void BundleResultList::put(const dtn::data::MetaBundle &bundle) noexcept {
			this->push_back(bundle);
		}

		BundleResultQueue::BundleResultQueue() {
		}

		BundleResultQueue::~BundleResultQueue() {
		}

		void BundleResultQueue::put(const dtn::data::MetaBundle &bundle) noexcept {
			this->push(bundle);
		}
	} /* namespace storage */
} /* namespace dtn */
