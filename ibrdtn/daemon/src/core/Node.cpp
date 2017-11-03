/*
 * Node.cpp
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

#include "core/Node.h"
#include "core/BundleCore.h"
#include "net/ConvergenceLayer.h"
#include <ibrdtn/utils/Clock.h>
#include <ibrdtn/utils/Utils.h>
#include <ibrcommon/Logger.h>

#include <iostream>
#include <sstream>


namespace dtn
{
	namespace core
	{
		Node::URI::URI(const Node::Type t, const Node::Protocol p, const std::string &uri, const dtn::data::Number &to, const int prio)
		 : type(t), protocol(p), value(uri), expire((to == 0) ? 0 : dtn::utils::Clock::getTime() + to), priority(prio)
		{
		}

		Node::URI::~URI()
		{
		}

		void Node::URI::decode(std::string &address, unsigned int &port) const
		{
			// parse parameters
			std::vector<std::string> parameters = dtn::utils::Utils::tokenize(";", value);
			std::vector<std::string>::const_iterator param_iter = parameters.begin();

			while (param_iter != parameters.end())
			{
				std::vector<std::string> p = dtn::utils::Utils::tokenize("=", (*param_iter));

				if (p[0].compare("ip") == 0 || p[0].compare("email") == 0)
				{
					address = p[1];
				}

				if (p[0].compare("port") == 0)
				{
					std::stringstream port_stream;
					port_stream << p[1];
					port_stream >> port;
				}

				++param_iter;
			}
		}

		bool Node::URI::operator<(const URI &other) const
		{
			if (protocol < other.protocol) return true;
			if (protocol != other.protocol) return false;

			if (type < other.type) return true;
			if (type != other.type) return false;

			return (value < other.value);
		}

		bool Node::URI::operator==(const URI &other) const
		{
			return ((type == other.type) && (protocol == other.protocol) && (value == other.value));
		}

		bool Node::URI::operator==(const Node::Protocol &p) const
		{
			return (protocol == p);
		}

		bool Node::URI::operator==(const Node::Type &t) const
		{
			return (type == t);
		}

		std::ostream& operator<<(std::ostream &stream, const Node::URI &u)
		{
			stream << u.priority << "#" << Node::toString(u.type) << "#" << Node::toString(u.protocol) << "#" << u.value;
			return stream;
		}

		Node::Attribute::Attribute(const Type t, const std::string &n, const std::string &v, const dtn::data::Number &to, const int p)
		 : type(t), name(n), value(v), expire((to == 0) ? 0 : dtn::utils::Clock::getTime() + to), priority(p)
		{
		}

		Node::Attribute::~Attribute()
		{
		}

		bool Node::Attribute::operator<(const Attribute &other) const
		{
			if (name < other.name) return true;
			if (name != other.name) return false;

			return (type < other.type);
		}

		bool Node::Attribute::operator==(const Attribute &other) const
		{
			return ((type == other.type) && (name == other.name));
		}

		bool Node::Attribute::operator==(const std::string &n) const
		{
			return (name == n);
		}

		std::ostream& operator<<(std::ostream &stream, const Node::Attribute &a)
		{
			stream << a.priority << "#" << Node::toString(a.type) << "#" << a.name << "#" << a.value;
			return stream;
		}

		Node::Node()
		: _connect_immediately(false), _id(), _announced_mark(false)
		{
		}

		Node::Node(const dtn::data::EID &id)
		: _connect_immediately(false), _id(id), _announced_mark(false)
		{
		}

		Node::~Node()
		{
		}

		std::string Node::toString(const Node::Type type)
		{
			switch (type)
			{
			case Node::NODE_UNAVAILABLE:
				return "unavailable";

			case Node::NODE_DISCOVERED:
				return "discovered";

			case Node::NODE_STATIC_GLOBAL:
				return "static global";

			case Node::NODE_STATIC_LOCAL:
				return "static local";

			case Node::NODE_CONNECTED:
				return "connected";

			case Node::NODE_DHT_DISCOVERED:
				return "dht discovered";

			case Node::NODE_P2P_DIALUP:
				return "p2p dialup";
			}

			return "unknown";
		}

		std::string Node::toString(const Node::Protocol proto)
		{
			switch (proto)
			{
			case Node::CONN_UNSUPPORTED:
				return "unsupported";

			case Node::CONN_UNDEFINED:
				return "undefined";

			case Node::CONN_UDPIP:
				return "UDP";

			case Node::CONN_TCPIP:
				return "TCP";

			case Node::CONN_LOWPAN:
				return "LoWPAN";

			case Node::CONN_BLUETOOTH:
				return "Bluetooth";

			case Node::CONN_HTTP:
				return "HTTP";

			case Node::CONN_FILE:
				return "FILE";

			case Node::CONN_DGRAM_UDP:
				return "DGRAM:UDP";

			case Node::CONN_DGRAM_ETHERNET:
				return "DGRAM:ETHERNET";

			case Node::CONN_DGRAM_LOWPAN:
				return "DGRAM:LOWPAN";

			case Node::CONN_P2P_WIFI:
				return "P2P:WIFI";

			case Node::CONN_P2P_BT:
				return "P2P:BT";

			case Node::CONN_EMAIL:
				return "EMAIL";
			}

			return "unknown";
		}

		Node::Protocol Node::fromProtocolString(const std::string &protocol)
		{
			if (protocol == "UDP") {
				return Node::CONN_UDPIP;
			} else if (protocol == "TCP") {
				return Node::CONN_TCPIP;
			} else if (protocol == "LoWPAN") {
				return Node::CONN_LOWPAN;
			} else if (protocol == "Bluetooth") {
				return Node::CONN_BLUETOOTH;
			} else if (protocol == "HTTP") {
				return Node::CONN_HTTP;
			} else if (protocol == "FILE") {
				return Node::CONN_FILE;
			} else if (protocol == "DGRAM:UDP") {
				return Node::CONN_DGRAM_UDP;
			} else if (protocol == "DGRAM:ETHERNET") {
				return Node::CONN_DGRAM_ETHERNET;
			} else if (protocol == "DGRAM:LOWPAN") {
				return Node::CONN_DGRAM_LOWPAN;
			} else if (protocol == "P2P:WIFI") {
				return Node::CONN_P2P_WIFI;
			} else if (protocol == "P2P:BT") {
				return Node::CONN_P2P_BT;
			} else if (protocol == "unsupported") {
				return Node::CONN_UNSUPPORTED;
			}

			return Node::CONN_UNDEFINED;
		}

		bool Node::has(Node::Protocol proto) const
		{
			for (std::set<URI>::const_iterator iter = _uri_list.begin(); iter != _uri_list.end(); ++iter)
			{
				if ((*iter) == proto) return true;
			}
			return false;
		}

		bool Node::has(const std::string &name) const
		{
			for (std::set<Attribute>::const_iterator iter = _attr_list.begin(); iter != _attr_list.end(); ++iter)
			{
				if ((*iter) == name) return true;
			}
			return false;
		}

		void Node::add(const URI &u)
		{
			_uri_list.erase(u);
			_uri_list.insert(u);
		}

		void Node::add(const Attribute &attr)
		{
			_attr_list.erase(attr);
			_attr_list.insert(attr);
		}

		void Node::remove(const URI &u)
		{
			_uri_list.erase(u);
		}

		void Node::remove(const Attribute &attr)
		{
			_attr_list.erase(attr);
		}

		void Node::clear()
		{
			_uri_list.clear();
			_attr_list.clear();
		}

		dtn::data::Size Node::size() const throw ()
		{
			return _uri_list.size() + _attr_list.size();
		}

		// comparison of two URIs according to the priority
		bool compare_uri_priority (const Node::URI &first, const Node::URI &second)
		{
			return first.priority > second.priority;
		}

		// comparison of two Attributes according to the priority
		bool compare_attr_priority (const Node::Attribute &first, const Node::Attribute &second)
		{
			return first.priority > second.priority;
		}

		std::list<Node::URI> Node::get(Node::Protocol proto) const
		{
			std::list<URI> ret;
			for (std::set<URI>::const_iterator iter = _uri_list.begin(); iter != _uri_list.end(); ++iter)
			{
				const URI &uri = (*iter);

				if ((uri == proto) && isAvailable(uri))
				{
					ret.push_back(uri);
				}
			}

			ret.sort(compare_uri_priority);
			return ret;
		}

		std::list<Node::URI> Node::get(Node::Type type) const
		{
			std::list<URI> ret;
			for (std::set<URI>::const_iterator iter = _uri_list.begin(); iter != _uri_list.end(); ++iter)
			{
				const URI &uri = (*iter);

				if (uri == type)
				{
					ret.push_back(uri);
				}
			}

			ret.sort(compare_uri_priority);
			return ret;
		}

		std::list<Node::URI> Node::get(Node::Type type, Node::Protocol proto) const
		{
			std::list<URI> ret;
			for (std::set<URI>::const_iterator iter = _uri_list.begin(); iter != _uri_list.end(); ++iter)
			{
				const URI &uri = (*iter);

				if ((uri == proto) && (uri == type) && isAvailable(uri)) ret.push_back(uri);
			}
			ret.sort(compare_uri_priority);
			return ret;
		}

		std::list<Node::URI> Node::getAll() const
		{
			std::list<Node::URI> ret;
			for (std::set<URI>::const_iterator iter = _uri_list.begin(); iter != _uri_list.end(); ++iter)
			{
				const URI &uri = (*iter);

				if (isAvailable(uri)) ret.push_back(uri);
			}
			ret.sort(compare_uri_priority);
			return ret;
		}

		std::set<Node::Type> Node::getTypes() const
		{
			std::set<Type> ret;
			for (std::set<URI>::const_iterator iter = _uri_list.begin(); iter != _uri_list.end(); ++iter)
			{
				ret.insert((*iter).type);
			}
			return ret;
		}

		std::list<Node::Attribute> Node::get(const std::string &name) const
		{
			std::list<Attribute> ret;
			for (std::set<Attribute>::const_iterator iter = _attr_list.begin(); iter != _attr_list.end(); ++iter)
			{
				if ((*iter) == name) ret.push_back(*iter);
			}
			ret.sort(compare_attr_priority);
			return ret;
		}

		const dtn::data::EID& Node::getEID() const throw ()
		{
			return _id;
		}

		/**
		 *
		 * @return True, if all attributes are gone
		 */
		bool Node::expire()
		{
			// get the current timestamp
			dtn::data::Timestamp ct = dtn::utils::Clock::getTime();

			// walk though all Attribute elements and remove the expired ones
			{
				std::set<Attribute>::iterator iter = _attr_list.begin();
				while ( iter != _attr_list.end() )
				{
					const Attribute &attr = (*iter);
					if ((attr.expire > 0) && (attr.expire < ct))
					{
						// remove this attribute
						_attr_list.erase(iter++);
					}
					else
					{
						++iter;
					}
				}
			}

			// walk though all URI elements and remove the expired ones
			{
				std::set<URI>::iterator iter = _uri_list.begin();
				while ( iter != _uri_list.end() )
				{
					const URI &u = (*iter);
					if ((u.expire > 0) && (u.expire < ct))
					{
						// remove this attribute
						_uri_list.erase(iter++);
					}
					else
					{
						++iter;
					}
				}
			}

			return (_attr_list.empty() && _uri_list.empty());
		}

		const Node& Node::operator+=(const Node &other)
		{
			for (std::set<Attribute>::const_iterator iter = other._attr_list.begin(); iter != other._attr_list.end(); ++iter)
			{
				const Attribute &attr = (*iter);
				add(attr);
			}

			for (std::set<URI>::const_iterator iter = other._uri_list.begin(); iter != other._uri_list.end(); ++iter)
			{
				const URI &u = (*iter);
				add(u);
			}

			return (*this);
		}

		const Node& Node::operator-=(const Node &other)
		{
			for (std::set<Attribute>::const_iterator iter = other._attr_list.begin(); iter != other._attr_list.end(); ++iter)
			{
				const Attribute &attr = (*iter);
				remove(attr);
			}

			for (std::set<URI>::const_iterator iter = other._uri_list.begin(); iter != other._uri_list.end(); ++iter)
			{
				const URI &u = (*iter);
				remove(u);
			}

			return (*this);
		}

		bool Node::operator==(const dtn::data::EID &other) const
		{
			return (other == _id);
		}

		bool Node::operator==(const Node &other) const
		{
			return (other._id == _id);
		}

		bool Node::operator<(const Node &other) const
		{
			if (_id < other._id ) return true;

			return false;
		}

		std::string Node::toString() const
		{
			std::stringstream ss; ss << getEID().getString();
			return ss.str();
		}

		bool Node::doConnectImmediately() const
		{
			return _connect_immediately;
		}

		void Node::setConnectImmediately(bool val)
		{
			_connect_immediately = val;
		}

		bool Node::isAvailable() const throw ()
		{
			if (dtn::core::BundleCore::getInstance().isGloballyConnected()) {
				return !_uri_list.empty();
			} else {
				// filter for global addresses when internet is not available
				for (std::set<Node::URI>::const_iterator iter = _uri_list.begin(); iter != _uri_list.end(); ++iter)
				{
					const Node::URI &u = (*iter);

					switch (u.type) {
					case NODE_CONNECTED: return true;
					case NODE_DISCOVERED: return true;
					case NODE_STATIC_LOCAL: return true;
					case NODE_P2P_DIALUP: return true;
					default: break;
					}
				}
			}

			return false;
		}

		bool Node::isAvailable(const Node::URI &uri) const throw ()
		{
			if (dtn::core::BundleCore::getInstance().isGloballyConnected()) {
				return true;
			} else {
				switch (uri.type)
				{
				case NODE_CONNECTED:
					return true;

				case NODE_DISCOVERED:
					return true;

				case NODE_STATIC_LOCAL:
					return true;

				case NODE_P2P_DIALUP:
					return true;

				default:
					return false;
				}
			}
		}

		void Node::setAnnounced(bool val) throw ()
		{
			_announced_mark = val;
		}

		bool Node::isAnnounced() const throw ()
		{
			return _announced_mark;
		}

		std::ostream& operator<<(std::ostream &stream, const Node &node)
		{
			stream << "Node: " << node._id.getString() << " [ ";
			for (std::set<Node::Attribute>::const_iterator iter = node._attr_list.begin(); iter != node._attr_list.end(); ++iter)
			{
				const Node::Attribute &attr = (*iter);
				stream << attr << "#expire=" << attr.expire.toString() << "; ";
			}

			for (std::set<Node::URI>::const_iterator iter = node._uri_list.begin(); iter != node._uri_list.end(); ++iter)
			{
				const Node::URI &u = (*iter);
				stream << u << "#expire=" << u.expire.toString() << "; ";
			}
			stream << " ]";

			return stream;
		}
	}
}
