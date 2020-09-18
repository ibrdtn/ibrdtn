/*
 * AcknowledgementSet.cpp
 *
 *  Created on: 11.01.2013
 *      Author: morgenro
 */

#include "routing/prophet/AcknowledgementSet.h"
#include "core/BundleCore.h"
#include <ibrdtn/utils/Clock.h>

namespace dtn
{
	namespace routing
	{
		const dtn::data::Number AcknowledgementSet::identifier = NodeHandshakeItem::PROPHET_ACKNOWLEDGEMENT_SET;

		AcknowledgementSet::AcknowledgementSet()
		{
		}

		AcknowledgementSet::AcknowledgementSet(const AcknowledgementSet &other)
		 : ibrcommon::Mutex(), _bundles(other._bundles)
		{
		}

		void AcknowledgementSet::add(const dtn::data::MetaBundle &bundle) noexcept
		{
			try {
				// check if the bundle is valid
				dtn::core::BundleCore::getInstance().validate(bundle);

				// add the bundle to the set
				_bundles.add(bundle);
			} catch (dtn::data::Validator::RejectedException &ex) {
				// bundle rejected
			}
		}

		void AcknowledgementSet::expire(const dtn::data::Timestamp &timestamp) noexcept
		{
			_bundles.expire(timestamp);
		}

		void AcknowledgementSet::merge(const AcknowledgementSet &other) noexcept
		{
			for(dtn::data::BundleList::const_iterator it = other._bundles.begin(); it != other._bundles.end(); ++it)
			{
				const dtn::data::MetaBundle &ack = (*it);
				this->add(ack);
			}
		}

		bool AcknowledgementSet::has(const dtn::data::BundleID &id) const noexcept
		{
			dtn::data::BundleList::const_iterator iter = _bundles.find(dtn::data::MetaBundle::create(id));
			return !(iter == _bundles.end());
		}

		const dtn::data::Number& AcknowledgementSet::getIdentifier() const
		{
			return identifier;
		}

		dtn::data::Length AcknowledgementSet::getLength() const
		{
			std::stringstream ss;
			serialize(ss);
			return ss.str().length();
		}

		std::ostream& AcknowledgementSet::serialize(std::ostream& stream) const
		{
			stream << (*this);
			return stream;
		}

		std::istream& AcknowledgementSet::deserialize(std::istream& stream)
		{
			stream >> (*this);
			return stream;
		}

		std::ostream& operator<<(std::ostream& stream, const AcknowledgementSet& ack_set)
		{
			dtn::data::Number ackset_size(ack_set._bundles.size());
			ackset_size.encode(stream);
			for (dtn::data::BundleList::const_iterator it = ack_set._bundles.begin(); it != ack_set._bundles.end(); ++it)
			{
				const dtn::data::MetaBundle &ack = (*it);
				stream << (const dtn::data::BundleID&)ack;
				ack.expiretime.encode(stream);
				ack.lifetime.encode(stream);
			}

			return stream;
		}

		std::istream& operator>>(std::istream &stream, AcknowledgementSet &ack_set)
		{
			// clear the ack set first
			ack_set._bundles.clear();

			dtn::data::Number size;
			size.decode(stream);

			for(size_t i = 0; size > i; ++i)
			{
				dtn::data::MetaBundle ack;
				stream >> (dtn::data::BundleID&)ack;
				ack.expiretime.decode(stream);
				ack.lifetime.decode(stream);

				ack_set.add(ack);
			}
			return stream;
		}
	} /* namespace routing */
} /* namespace dtn */
