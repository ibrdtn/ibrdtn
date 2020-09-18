/*
 * AcknowledgementSet.h
 *
 *  Created on: 11.01.2013
 *      Author: morgenro
 */

#ifndef ACKNOWLEDGEMENTSET_H_
#define ACKNOWLEDGEMENTSET_H_

#include "routing/NodeHandshake.h"
#include <ibrdtn/data/BundleList.h>
#include <ibrcommon/thread/Mutex.h>
#include <iostream>
#include <set>

namespace dtn
{
	namespace routing
	{
		/*!
		 * \brief Set of Acknowledgements, that can be serialized in node handshakes.
		 */
		class AcknowledgementSet : public NodeHandshakeItem, public ibrcommon::Mutex
		{
		public:
			AcknowledgementSet();
			AcknowledgementSet(const AcknowledgementSet&);

			/**
			 * Add a bundle to the set
			 */
			void add(const dtn::data::MetaBundle &bundle) noexcept;

			/**
			 * Expire outdated entries
			 */
			void expire(const dtn::data::Timestamp &timestamp) noexcept;

			/**
			 * merge the set with a second AcknowledgementSet
			 */
			void merge(const AcknowledgementSet&) noexcept;

			/**
			 * Returns true if the given bundleId is in the bundle list
			 */
			bool has(const dtn::data::BundleID &id) const noexcept;

			/* virtual methods from NodeHandshakeItem */
			virtual const dtn::data::Number& getIdentifier() const; ///< \see NodeHandshakeItem::getIdentifier
			virtual dtn::data::Length getLength() const; ///< \see NodeHandshakeItem::getLength
			virtual std::ostream& serialize(std::ostream& stream) const; ///< \see NodeHandshakeItem::serialize
			virtual std::istream& deserialize(std::istream& stream); ///< \see NodeHandshakeItem::deserialize
			static const dtn::data::Number identifier;

			friend std::ostream& operator<<(std::ostream&, const AcknowledgementSet&);
			friend std::istream& operator>>(std::istream&, AcknowledgementSet&);

			typedef dtn::data::BundleList::const_iterator const_iterator;
			const_iterator begin() const { return _bundles.begin(); };
			const_iterator end() const { return _bundles.end(); };

		private:
			dtn::data::BundleList _bundles;
		};
	} /* namespace routing */
} /* namespace dtn */
#endif /* ACKNOWLEDGEMENTSET_H_ */
