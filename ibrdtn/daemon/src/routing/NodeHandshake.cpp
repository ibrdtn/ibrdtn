/*
 * NodeHandshake.cpp
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

#include "routing/NodeHandshake.h"

namespace dtn
{
	namespace routing
	{
		NodeHandshake::NodeHandshake()
		 : _type((size_t)HANDSHAKE_INVALID), _lifetime(0)
		{
		}

		NodeHandshake::NodeHandshake(MESSAGE_TYPE type, const dtn::data::Number &lifetime)
		 : _type((size_t)type), _lifetime(lifetime)
		{
		}

		NodeHandshake::~NodeHandshake()
		{
			clear();
		}

		void NodeHandshake::clear()
		{
			for (item_set::const_iterator iter = _items.begin(); iter != _items.end(); ++iter)
			{
				delete (*iter);
			}

			// clear raw items too
			_raw_items.clear();
		}

		void NodeHandshake::addRequest(const dtn::data::Number &identifier)
		{
			_requests.insert(identifier);
		}

		bool NodeHandshake::hasRequest(const dtn::data::Number &identifier) const
		{
			return (_requests.find(identifier) != _requests.end());
		}

		const NodeHandshake::request_set& NodeHandshake::getRequests() const
		{
			return _requests;
		}

		const NodeHandshake::item_set& NodeHandshake::getItems() const
		{
			return _items;
		}

		void NodeHandshake::addItem(NodeHandshakeItem *item)
		{
			_items.push_back(item);
		}

		bool NodeHandshake::hasItem(const dtn::data::Number &identifier) const
		{
			for (item_set::const_iterator iter = _items.begin(); iter != _items.end(); ++iter)
			{
				const NodeHandshakeItem &item = (**iter);
				if (item.getIdentifier() == identifier) return true;
			}

			return false;
		}

		NodeHandshakeItem* NodeHandshake::getItem(const dtn::data::Number &identifier) const
		{
			for (item_set::const_iterator iter = _items.begin(); iter != _items.end(); ++iter)
			{
				NodeHandshakeItem *item = (*iter);
				if (item->getIdentifier() == identifier) return item;
			}

			return NULL;
		}

		NodeHandshake::MESSAGE_TYPE NodeHandshake::getType() const
		{
			return NodeHandshake::MESSAGE_TYPE(_type.get<size_t>());
		}

		const dtn::data::Number& NodeHandshake::getLifetime() const
		{
			return _lifetime;
		}

		const std::string NodeHandshake::toString() const
		{
			std::stringstream ss;

			switch (getType()) {
				case HANDSHAKE_INVALID:
					ss << "HANDSHAKE_INVALID";
					break;

				case HANDSHAKE_REQUEST:
					ss << "HANDSHAKE_REQUEST";
					break;

				case HANDSHAKE_RESPONSE:
					ss << "HANDSHAKE_RESPONSE";
					break;

				case HANDSHAKE_NOTIFICATION:
					ss << "HANDSHAKE_NOTIFICATION";
					break;

				default:
					ss << "HANDSHAKE";
					break;
			}

			if (getType() == NodeHandshake::HANDSHAKE_REQUEST || getType() == NodeHandshake::HANDSHAKE_NOTIFICATION)
			{
				ss << "[";

				for (request_set::const_iterator iter = _requests.begin(); iter != _requests.end(); ++iter)
				{
					const dtn::data::Number &item = (*iter);
					ss << " " << item.toString();
				}
			}
			else if (getType() == NodeHandshake::HANDSHAKE_RESPONSE)
			{
				ss << "[ ttl: " << getLifetime().toString() << ",";

				for (item_set::const_iterator iter = _items.begin(); iter != _items.end(); ++iter)
				{
					const NodeHandshakeItem &item (**iter);
					ss << " " << item.getIdentifier().toString();
				}
			}

			ss << " ]";

			return ss.str();
		}

		std::ostream& operator<<(std::ostream &stream, const NodeHandshake &hs)
		{
			// first the type as SDNV
			stream << hs._type;

			if (hs.getType() == NodeHandshake::HANDSHAKE_REQUEST || hs.getType() == NodeHandshake::HANDSHAKE_NOTIFICATION)
			{
				// then the number of request items
				dtn::data::Number number_of_items(hs._requests.size());
				stream << number_of_items;

				for (NodeHandshake::request_set::const_iterator iter = hs._requests.begin(); iter != hs._requests.end(); ++iter)
				{
					dtn::data::Number req(*iter);
					stream << req;
				}
			}
			else if (hs.getType() == NodeHandshake::HANDSHAKE_RESPONSE)
			{
				// then the lifetime of this data
				stream << hs._lifetime;

				// then the number of request items
				dtn::data::Number number_of_items(hs._items.size());
				stream << number_of_items;

				for (NodeHandshake::item_set::const_iterator iter = hs._items.begin(); iter != hs._items.end(); ++iter)
				{
					const NodeHandshakeItem &item = (**iter);

					// first the identifier of the item
					dtn::data::Number id(item.getIdentifier());
					stream << id;

					// then the length of the payload
					dtn::data::Number len(item.getLength());
					stream << len;

					item.serialize(stream);
				}
			}

			return stream;
		}

		std::istream& operator>>(std::istream &stream, NodeHandshake &hs)
		{
			// clear the node handshake object
			hs.clear();

			// first the type as SDNV
			stream >> hs._type;

			if (hs.getType() == NodeHandshake::HANDSHAKE_REQUEST || hs.getType() == NodeHandshake::HANDSHAKE_NOTIFICATION)
			{
				// then the number of request items
				dtn::data::Number number_of_items;
				stream >> number_of_items;

				for (size_t i = 0; number_of_items > i; ++i)
				{
					dtn::data::Number req;
					stream >> req;
					hs._requests.insert(req);
				}
			}
			else if (hs.getType() == NodeHandshake::HANDSHAKE_RESPONSE)
			{
				// then the lifetime of this data
				stream >> hs._lifetime;

				// then the number of request items
				dtn::data::Number number_of_items;
				stream >> number_of_items;

				for (size_t i = 0; number_of_items > i; ++i)
				{
					// first the identifier of the item
					dtn::data::Number id;
					stream >> id;

					// then the length of the payload
					dtn::data::Number len;
					stream >> len;

					// add to raw map and create data container
					//std::stringstream *data = new std::stringstream();
					std::stringstream &data = hs._raw_items.get(id);

					// copy data to stringstream
					ibrcommon::BLOB::copy(data, stream, len.get<std::streamsize>());
				}
			}

			return stream;
		}

		NodeHandshake::StreamMap::StreamMap()
		{
		}

		NodeHandshake::StreamMap::~StreamMap()
		{
			clear();
		}

		bool NodeHandshake::StreamMap::has(const dtn::data::Number &identifier)
		{
			stream_map::iterator i = _map.find(identifier);
			return (i != _map.end());
		}

		std::stringstream& NodeHandshake::StreamMap::get(const dtn::data::Number &identifier)
		{
			stream_map::iterator i = _map.find(identifier);

			if (i == _map.end())
			{
				std::pair<stream_map::iterator, bool> p =
						_map.insert(std::pair<dtn::data::Number, std::stringstream* >(identifier, new std::stringstream()));
				i = p.first;
			}

			return (*(*i).second);
		}

		void NodeHandshake::StreamMap::remove(const dtn::data::Number &identifier)
		{
			stream_map::iterator i = _map.find(identifier);

			if (i != _map.end())
			{
				delete (*i).second;
				_map.erase(identifier);
			}
		}

		void NodeHandshake::StreamMap::clear()
		{
			for (stream_map::iterator iter = _map.begin(); iter != _map.end(); ++iter)
			{
				delete (*iter).second;
			}
			_map.clear();
		}

		BloomFilterSummaryVector::BloomFilterSummaryVector(const dtn::data::BundleSet &vector)
		 : _vector(vector)
		{
		}

		BloomFilterSummaryVector::BloomFilterSummaryVector()
		{
		}

		BloomFilterSummaryVector::~BloomFilterSummaryVector()
		{
		}

		const dtn::data::Number& BloomFilterSummaryVector::getIdentifier() const
		{
			return identifier;
		}

		dtn::data::Length BloomFilterSummaryVector::getLength() const
		{
			return _vector.getLength();
		}

		const dtn::data::BundleSet& BloomFilterSummaryVector::getVector() const
		{
			return _vector;
		}

		std::ostream& BloomFilterSummaryVector::serialize(std::ostream &stream) const
		{
			stream << _vector;
			return stream;
		}

		std::istream& BloomFilterSummaryVector::deserialize(std::istream &stream)
		{
			stream >> _vector;
			return stream;
		}

		const dtn::data::Number BloomFilterSummaryVector::identifier = NodeHandshakeItem::BLOOM_FILTER_SUMMARY_VECTOR;

		BloomFilterPurgeVector::BloomFilterPurgeVector(const dtn::data::BundleSet &vector)
		 : _vector(vector)
		{
		}

		BloomFilterPurgeVector::BloomFilterPurgeVector()
		{
		}

		BloomFilterPurgeVector::~BloomFilterPurgeVector()
		{
		}

		const dtn::data::Number& BloomFilterPurgeVector::getIdentifier() const
		{
			return identifier;
		}

		dtn::data::Length BloomFilterPurgeVector::getLength() const
		{
			return _vector.getLength();
		}

		const dtn::data::BundleSet& BloomFilterPurgeVector::getVector() const
		{
			return _vector;
		}

		std::ostream& BloomFilterPurgeVector::serialize(std::ostream &stream) const
		{
			stream << _vector;
			return stream;
		}

		std::istream& BloomFilterPurgeVector::deserialize(std::istream &stream)
		{
			stream >> _vector;
			return stream;
		}

		const dtn::data::Number BloomFilterPurgeVector::identifier = NodeHandshakeItem::BLOOM_FILTER_PURGE_VECTOR;

		RoutingLimitations::RoutingLimitations()
		 : NeighborDataSetImpl(RoutingLimitations::identifier)
		{
		}

		RoutingLimitations::~RoutingLimitations()
		{
		}

		const dtn::data::Number& RoutingLimitations::getIdentifier() const
		{
			return identifier;
		}

		dtn::data::Length RoutingLimitations::getLength() const
		{
			dtn::data::Length ret = 0;

			// number of elements
			ret += dtn::data::Number(_limits.size()).getLength();

			for (std::map<size_t, ssize_t>::const_iterator it = _limits.begin(); it != _limits.end(); ++it)
			{
				ret += dtn::data::Number((*it).first).getLength();
				ret += dtn::data::SDNV<ssize_t>((*it).second).getLength();
			}

			return ret;
		}

		void RoutingLimitations::setLimit(LimitIndex index, ssize_t value)
		{
			_limits[index] = value;
		}

		ssize_t RoutingLimitations::getLimit(LimitIndex index) const
		{
			std::map<size_t, ssize_t>::const_iterator it = _limits.find(index);
			if (it == _limits.end()) return 0;
			return (*it).second;
		}

		std::ostream& RoutingLimitations::serialize(std::ostream &stream) const
		{
			// number of elements
			stream << dtn::data::Number(_limits.size());

			for (std::map<size_t, ssize_t>::const_iterator it = _limits.begin(); it != _limits.end(); ++it)
			{
				stream << dtn::data::Number((*it).first);
				stream << dtn::data::SDNV<ssize_t>((*it).second);
			}

			return stream;
		}

		std::istream& RoutingLimitations::deserialize(std::istream &stream)
		{
			dtn::data::Number elements, first;
			dtn::data::SDNV<ssize_t> second;

			// clear all elements
			_limits.clear();

			// get number of elements
			stream >> elements;

			for (size_t i = 0; i < elements.get<size_t>(); ++i)
			{
				stream >> first;
				stream >> second;
				_limits[first.get<size_t>()] = second.get<ssize_t>();
			}

			return stream;
		}

		const dtn::data::Number RoutingLimitations::identifier = NodeHandshakeItem::ROUTING_LIMITATIONS;

	} /* namespace routing */
} /* namespace dtn */
