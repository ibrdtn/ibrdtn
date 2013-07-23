/*
 * Node.h
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

#ifndef IBRDTN_DAEMON_NODE_H_
#define IBRDTN_DAEMON_NODE_H_

#include <string>
#include <ibrdtn/data/EID.h>
#include <set>
#include <list>
#include <iostream>

namespace dtn
{
	namespace net
	{
		class ConvergenceLayer;
	}

	namespace core
	{
		/**
		 * A Node instance holds all informations of a neighboring node.
		 */
		class Node
		{
		public:
			enum Protocol
			{
				CONN_UNSUPPORTED = -1,
				CONN_UNDEFINED = 0,
				CONN_TCPIP = 1,
				CONN_UDPIP = 2,
				CONN_LOWPAN = 3,
				CONN_BLUETOOTH = 4,
				CONN_HTTP = 5,
				CONN_FILE = 6,
				CONN_DGRAM_UDP = 7,
				CONN_DGRAM_LOWPAN = 8,
				CONN_DGRAM_ETHERNET = 9,
				CONN_P2P_WIFI = 10,
				CONN_P2P_BT = 11,
				CONN_EMAIL = 12
			};

			/**
			 * Specify a node type.
			 * FLOATING is a node, if it's not statically reachable.
			 * STATIC is used for static nodes with are permanently reachable.
			 * DHT_DISCOVERED is used for a node, learned by a dht lookup
			 */
			enum Type
			{
				NODE_UNAVAILABLE = 0,
				NODE_CONNECTED = 1,
				NODE_DISCOVERED = 2,
				NODE_STATIC_GLOBAL = 3,
				NODE_STATIC_LOCAL = 4,
				NODE_DHT_DISCOVERED = 5,
				NODE_P2P_DIALUP = 6
			};

			class URI
			{
			public:
				URI(const Type t, const Protocol proto, const std::string &uri, const dtn::data::Number &timeout = 0, const int priority = 0);
				~URI();

				const Type type;
				const Protocol protocol;
				const std::string value;
				const dtn::data::Timestamp expire;
				const int priority;

				void decode(std::string &address, unsigned int &port) const;

				bool operator<(const URI &other) const;
				bool operator==(const URI &other) const;

				bool operator==(const Node::Protocol &p) const;
				bool operator==(const Node::Type &t) const;

				friend std::ostream& operator<<(std::ostream&, const Node::URI&);
			};

			class Attribute
			{
			public:
				Attribute(const Type t, const std::string &name, const std::string &value, const dtn::data::Number &timeout = 0, const int priority = 0);
				~Attribute();

				const Type type;
				const std::string name;
				const std::string value;
				const dtn::data::Timestamp expire;
				const int priority;

				bool operator<(const Attribute &other) const;
				bool operator==(const Attribute &other) const;

				bool operator==(const std::string &name) const;

				friend std::ostream& operator<<(std::ostream&, const Node::Attribute&);
			};

			static std::string toString(const Node::Type type);
			static std::string toString(const Node::Protocol proto);
			static Node::Protocol fromProtocolString(const std::string &protocol);

			/**
			 * constructor
			 * @param type set the node type
			 * @sa NoteType
			 */
			Node(const dtn::data::EID &id);
			Node();

			/**
			 * destructor
			 */
			virtual ~Node();

			/**
			 * Check if the protocol is available for this node.
			 * @param proto
			 * @return
			 */
			bool has(Node::Protocol proto) const;
			bool has(const std::string &name) const;

			/**
			 * Add a URI to this node.
			 * @param u
			 */
			void add(const URI &u);
			void add(const Attribute &attr);

			/**
			 * Remove a given URI of the node.
			 * @param proto
			 */
			void remove(const URI &u);
			void remove(const Attribute &attr);

			/**
			 * Clear all URIs & Attributes contained in this node.
			 */
			void clear();

			/**
			 * Get the number of entries (URI + Attributes)
			 */
			dtn::data::Size size() const;

			/**
			 * Returns a list of URIs matching the given protocol
			 * @param proto
			 * @return
			 */
			std::list<URI> get(Node::Protocol proto) const;
			std::list<URI> get(Node::Type type) const;
			std::list<URI> get(Node::Type type, Node::Protocol proto) const;

			/**
			 * Returns a ordered list of all available URIs
			 */
			std::list<Node::URI> getAll() const;

			/**
			 * Returns a set of all available types
			 */
			std::set<Node::Type> getTypes() const;

			/**
			 * get a list of attributes match the given name
			 * @param name
			 * @return
			 */
			std::list<Attribute> get(const std::string &name) const;

			/**
			 * Return the EID of this node.
			 * @return The EID of this node.
			 */
			const dtn::data::EID& getEID() const;

			/**
			 * remove expired service descriptors
			 */
			bool expire();

			/**
			 * Compare this node to another one. Two nodes are equal if the
			 * uri and address of both nodes are equal.
			 * @param node A other node to compare.
			 * @return true, if the given node is equal to this node.
			 */
			bool operator==(const Node &other) const;
			bool operator<(const Node &other) const;

			bool operator==(const dtn::data::EID &other) const;

			const Node& operator+=(const Node &other);
			const Node& operator-=(const Node &other);

			std::string toString() const;

			bool doConnectImmediately() const;
			void setConnectImmediately(bool val);

			/**
			 * @return true, if there is at least one dial-up connection
			 */
			bool hasDialup() const;

			/**
			 * @return true, if at least one connection is available
			 */
			bool isAvailable() const;

			/**
			 * @return true, if this node has been announced before
			 */
			bool isAnnounced() const;

			/**
			 * Mark this node as announced or not
			 */
			void setAnnounced(bool val);

			friend std::ostream& operator<<(std::ostream&, const Node&);

		private:
			bool isAvailable(const Node::URI &uri) const;

			bool _connect_immediately;
			dtn::data::EID _id;

			std::set<URI> _uri_list;
			std::set<Attribute> _attr_list;

			bool _announced_mark;
		};
	}
}

#endif /*IBRDTN_DAEMON_NODE_H_*/
