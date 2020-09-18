/*
 * BundleQueryInterface.h
 *
 *  Created on: 21.01.2013
 *      Author: morgenro
 */

#ifndef BUNDLESEEKER_H_
#define BUNDLESEEKER_H_

#include "storage/BundleSelector.h"
#include "storage/BundleResult.h"
#include <ibrdtn/data/EID.h>
#include <ibrdtn/data/MetaBundle.h>
#include <ibrcommon/Exceptions.h>
#include <set>

namespace dtn
{
	namespace storage
	{
		class BundleSeeker
		{
		public:
			virtual ~BundleSeeker() { };

			/**
			 * Query the database for a number of bundles. The bundles are selected with the BundleSeeker
			 * class which is to implement by the user of this method.
			 * @param cb The instance of the BundleSelector class.
			 * @return A list of bundles.
			 */
			virtual void get(const BundleSelector &cb, BundleResult &result) noexcept (false) = 0;

			/**
			 * Return a set of distinct destinations for all bundles in the storage.
			 * @return
			 */
			typedef std::set<dtn::data::EID> eid_set;
			virtual const eid_set getDistinctDestinations() = 0;
		};
	}
}


#endif /* BUNDLESEEKER_H_ */
