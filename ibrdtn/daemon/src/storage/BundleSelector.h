/*
 * BundleSelector.h
 *
 *  Created on: 22.01.2013
 *      Author: morgenro
 */

#ifndef BUNDLESELECTOR_H_
#define BUNDLESELECTOR_H_

#include "storage/BundleResult.h"
#include <ibrdtn/data/EID.h>
#include <ibrdtn/data/MetaBundle.h>
#include <ibrdtn/data/Number.h>
#include <ibrcommon/Exceptions.h>

namespace dtn
{
	namespace storage
	{
		class BundleSelectorException : public ibrcommon::Exception
		{
		public:
			BundleSelectorException(std::string what = "Bundle query aborted.") noexcept : ibrcommon::Exception(what)
			{
			}
		};

		class NoBundleFoundException : public dtn::MissingObjectException
		{
		public:
			NoBundleFoundException(std::string what = "No bundle match the specified criteria.") noexcept : dtn::MissingObjectException(what)
			{
			};
		};

		class BundleSelector
		{
		public:
			virtual ~BundleSelector() {};

			/**
			 * Limit the number of selected items.
			 * @return The limit as number of items.
			 */
			virtual dtn::data::Size limit() const noexcept { return 1; };

			/**
			 * This method is called by addIfSelected() to determine if one bundle
			 * should added to a set or not.
			 * @param id The bundle id of the bundle to select.
			 * @return True, if the bundle should be added to the set.
			 */
			virtual bool shouldAdd(const dtn::data::MetaBundle&) const noexcept (false) { return false; };

			/**
			 * This method is called by the storage to add a bundle to the result
			 * if it has been selected by the shouldAdd method
			 */
			virtual bool addIfSelected(BundleResult &result, const dtn::data::MetaBundle &bundle) const noexcept (false) {
				if (shouldAdd(bundle)) {
					result.put(bundle);
					return true;
				}
				return false;
			};
		};
	}
}

#endif /* BUNDLESELECTOR_H_ */
