/*
 * BundleResult.h
 *
 *  Created on: 07.12.2012
 *      Author: morgenro
 */

#ifndef BUNDLERESULT_H_
#define BUNDLERESULT_H_

#include <ibrdtn/data/MetaBundle.h>
#include <ibrcommon/thread/Queue.h>
#include <list>

namespace dtn
{
	namespace storage
	{
		class BundleResult {
		public:
			BundleResult();
			virtual ~BundleResult() = 0;
			virtual void put(const dtn::data::MetaBundle &bundle) noexcept = 0;
		};

		class BundleResultList : public BundleResult, public std::list<dtn::data::MetaBundle> {
		public:
			BundleResultList();
			virtual ~BundleResultList();

			virtual void put(const dtn::data::MetaBundle &bundle) noexcept;
		};

		class BundleResultQueue : public BundleResult, public ibrcommon::Queue<dtn::data::MetaBundle> {
		public:
			BundleResultQueue();
			virtual ~BundleResultQueue();

			virtual void put(const dtn::data::MetaBundle &bundle) noexcept;
		};
	} /* namespace storage */
} /* namespace dtn */
#endif /* BUNDLERESULT_H_ */
