/*
 * DTNTPWorker.h
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

#ifndef DTNTPWORKER_H_
#define DTNTPWORKER_H_

#include "core/AbstractWorker.h"
#include "core/EventReceiver.h"
#include "core/TimeEvent.h"
#include "net/DiscoveryBeaconHandler.h"
#include "Configuration.h"
#include <time.h>

namespace dtn
{
	namespace daemon
	{
		class DTNTPWorker : public dtn::core::AbstractWorker, public dtn::core::EventReceiver<dtn::core::TimeEvent>, public dtn::net::DiscoveryBeaconHandler
		{
		public:
			/**
			 * Constructor
			 */
			DTNTPWorker();

			/**
			 * Destructor
			 */
			virtual ~DTNTPWorker();

			/**
			 * This method is called every time a bundles is received for this endpoint.
			 * @param b
			 */
			void callbackBundleReceived(const Bundle &b);

			/**
			 * This method is called by the EventSwitch.
			 * @param evt
			 */
			void raiseEvent(const dtn::core::TimeEvent &evt) noexcept;

			/**
			 * This message is called by the discovery module.
			 * @param iface
			 * @param name
			 * @param data
			 */
			void onUpdateBeacon(const ibrcommon::vinterface &iface, DiscoveryBeacon &announcement)
				throw(NoServiceHereException);

			/**
			 * TimeSyncMessage
			 * This class represent a sync message which is used to exchange data about
			 * the current clock state. During a short request-response contact the offset
			 * between two clock can be determined.
			 */
			class TimeSyncMessage
			{
			public:
				enum MSG_TYPE
				{
					TIMESYNC_REQUEST = 1,
					TIMESYNC_RESPONSE = 2
				};

				TimeSyncMessage();
				~TimeSyncMessage();

				MSG_TYPE type;

				timeval origin_timestamp;
				double origin_rating;

				timeval peer_timestamp;
				double peer_rating;

				friend std::ostream &operator<<(std::ostream &stream, const DTNTPWorker::TimeSyncMessage &obj);
				friend std::istream &operator>>(std::istream &stream, DTNTPWorker::TimeSyncMessage &obj);
			};

			class TimeSyncState {
			public:
				TimeSyncState();
				virtual ~TimeSyncState();

				// sync threshold
				float sync_threshold;

				// the base rating used to determine the current clock rating
				double base_rating;

				// the local rating is at least decremented by this value between each synchronization
				double psi;

				// current value for sigma
				double sigma;

				// timestamp of the last synchronization with another (better) clock
				struct timespec last_sync_time;

				// defines if the last_sync_time is set
				bool last_sync_set;

				/**
				 * Get the time of the last sync as double
				 */
				static double toDouble(const timespec &val);
			};

			/**
			 * Get the global time sync state
			 */
			static const TimeSyncState& getState();

		private:
			class SyncPeer {
			public:
				SyncPeer();
				virtual ~SyncPeer();

				enum State {
					STATE_IDLE = 0,
					STATE_PREPARE = 1,
					STATE_REQUEST = 2,
					STATE_SYNC = 3
				};

				/**
				 * Touch the entry to prevent expiration
				 */
				void touch();

				/**
				 * Returns true if the peer entry is expired
				 */
				bool isExpired() const;

				State state;
				struct timespec request_monotonic_time;
				timeval request_timestamp;

			private:
				dtn::data::Timestamp _touched;
			};

			static const unsigned int PROTO_VERSION;
			static const std::string TAG;

			/**
			 * check if we should sync with another node
			 * @param node
			 */
			float getPeerRating(const dtn::core::Node &node) const;

			/**
			 * Start sync'ing with another node
			 * @param node
			 */
			void syncWith(const dtn::data::EID &peer);

			/**
			 * Determine if this node is configured as reference or not
			 * @return
			 */
			bool hasReference() const;

			/**
			 * this method decode neighbor data
			 * @param attr
			 * @param version
			 * @param timestamp
			 * @param quality
			 */
			void decode(const dtn::core::Node::Attribute &attr, unsigned int &version, dtn::data::Timestamp &timestamp, float &quality) const;

			/**
			 * Synchronize this clock with another one
			 * @param msg
			 * @param tv
			 */
			void sync(const TimeSyncMessage &msg, const struct timeval &tv, const struct timeval &local, const struct timeval &remote);

			/**
			 * global synchronization state
			 */
			static TimeSyncState _sync_state;

			// send discovery announcements with the local clock rating
			bool _announce_rating;

			// synchronize with other nodes
			bool _sync;

			// Mutex to lock the synchronization process
			ibrcommon::Mutex _sync_lock;

			// manage a list of recently sync'd nodes
			ibrcommon::Mutex _peer_lock;
			typedef std::map<EID, SyncPeer> peer_map;
			peer_map _peers;
		};
	}
}

#endif /* DTNTPWORKER_H_ */
