/*
 * DTNTPWorker.cpp
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

#include "config.h"
#include "DTNTPWorker.h"
#include "core/EventDispatcher.h"
#include "core/BundleCore.h"
#include "core/TimeAdjustmentEvent.h"
#include <ibrdtn/utils/Clock.h>
#include <ibrdtn/utils/Utils.h>
#include <ibrdtn/data/AgeBlock.h>
#include <ibrdtn/data/PayloadBlock.h>
#include <ibrdtn/data/SDNV.h>
#include <ibrdtn/data/ScopeControlHopLimitBlock.h>
#include <ibrcommon/TimeMeasurement.h>
#include <ibrcommon/Logger.h>

#include <sys/time.h>

#ifdef __WIN32__
#define suseconds_t long
#endif

namespace dtn
{
	namespace daemon
	{
		const unsigned int DTNTPWorker::PROTO_VERSION = 1;
		const std::string DTNTPWorker::TAG = "DTNTPWorker";
		DTNTPWorker::TimeSyncState DTNTPWorker::_sync_state;

		DTNTPWorker::TimeSyncState::TimeSyncState()
		 : sync_threshold(0.15f), base_rating(0.0), psi(0.99), sigma(1.0), last_sync_set(false)
		{
			// initialize the last sync time to zero
			last_sync_time.tv_sec = 0;
			last_sync_time.tv_nsec = 0;
		}

		DTNTPWorker::TimeSyncState::~TimeSyncState()
		{
		}

		double DTNTPWorker::TimeSyncState::toDouble(const timespec &val) {
			return static_cast<double>(val.tv_sec) + (static_cast<double>(val.tv_nsec) / 1000000000.0);
		}

		DTNTPWorker::DTNTPWorker()
		 : _announce_rating(false), _sync(false)
		{
			AbstractWorker::initialize("dtntp");

			// get global configuration for time synchronization
			const dtn::daemon::Configuration::TimeSync &conf = dtn::daemon::Configuration::getInstance().getTimeSync();

			if (conf.hasReference())
			{
				// set clock rating to 1 since this node has a reference clock
				_sync_state.base_rating = 1.0;

				// evaluate the current local time
				if (dtn::utils::Clock::getTime() > 0) {
					dtn::utils::Clock::setRating(1.0);
				} else {
					dtn::utils::Clock::setRating(0.0);
					IBRCOMMON_LOGGER_TAG(DTNTPWorker::TAG, warning) << "Expiration limited due to wrong local clock." << IBRCOMMON_LOGGER_ENDL;
				}
			} else {
				dtn::utils::Clock::setRating(0.0);
				_sync_state.sigma = conf.getSigma();
				_sync_state.psi = conf.getPsi();
			}

			// check if we should announce our own rating via discovery
			_announce_rating = conf.sendDiscoveryBeacons();

			// store the sync threshold locally
			_sync_state.sync_threshold = conf.getSyncLevel();

			// synchronize with other nodes
			_sync = conf.doSync();

			if (_sync || _announce_rating) {
				IBRCOMMON_LOGGER_TAG(DTNTPWorker::TAG, info) << "Time-Synchronization enabled: " << (conf.hasReference() ? "master mode" : "slave mode") << IBRCOMMON_LOGGER_ENDL;
			}

			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);

			// register as discovery handler for all interfaces
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(this);
		}

		DTNTPWorker::~DTNTPWorker()
		{
			// unregister as discovery handler for all interfaces
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(this);

			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
		}

		DTNTPWorker::TimeSyncMessage::TimeSyncMessage()
		 : type(TIMESYNC_REQUEST), origin_rating(dtn::utils::Clock::getRating()), peer_rating(0.0)
		{
			timerclear(&origin_timestamp);
			timerclear(&peer_timestamp);

			dtn::utils::Clock::gettimeofday(&origin_timestamp);
		}

		DTNTPWorker::TimeSyncMessage::~TimeSyncMessage()
		{
		}

		std::ostream &operator<<(std::ostream &stream, const DTNTPWorker::TimeSyncMessage &obj)
		{
			std::stringstream ss;

			stream << (char)obj.type;

			ss.clear(); ss.str(""); ss << obj.origin_rating;
			stream << dtn::data::BundleString(ss.str());

			stream << dtn::data::Number(obj.origin_timestamp.tv_sec);
			stream << dtn::data::Number(obj.origin_timestamp.tv_usec);

			ss.clear(); ss.str(""); ss << obj.peer_rating;
			stream << dtn::data::BundleString(ss.str());

			stream << dtn::data::Number(obj.peer_timestamp.tv_sec);
			stream << dtn::data::Number(obj.peer_timestamp.tv_usec);

			return stream;
		}

		std::istream &operator>>(std::istream &stream, DTNTPWorker::TimeSyncMessage &obj)
		{
			char type = 0;
			std::stringstream ss;
			dtn::data::BundleString bs;
			dtn::data::Number sdnv;

			stream >> type;
			obj.type = DTNTPWorker::TimeSyncMessage::MSG_TYPE(type);

			stream >> bs;
			ss.clear();
			ss.str((const std::string&)bs);
			ss >> obj.origin_rating;

			stream >> sdnv;
			obj.origin_timestamp.tv_sec = sdnv.get<time_t>();

			stream >> sdnv;
			obj.origin_timestamp.tv_usec = sdnv.get<suseconds_t>();

			stream >> bs;
			ss.clear();
			ss.str((const std::string&)bs);
			ss >> obj.peer_rating;

			stream >> sdnv;
			obj.peer_timestamp.tv_sec = sdnv.get<time_t>();

			stream >> sdnv;
			obj.peer_timestamp.tv_usec = sdnv.get<suseconds_t>();

			return stream;
		}

		void DTNTPWorker::raiseEvent(const dtn::core::TimeEvent &t) throw ()
		{
			if (t.getAction() != dtn::core::TIME_SECOND_TICK) return;

			ibrcommon::MutexLock l(_sync_lock);

			// remove outdated blacklist entries
			{
				// get current monotonic timestamp
				const dtn::data::Timestamp mt = dtn::utils::Clock::getMonotonicTimestamp();

				ibrcommon::MutexLock l(_peer_lock);
				for (peer_map::iterator iter = _peers.begin(); iter != _peers.end();)
				{
					const SyncPeer &peer = (*iter).second;

					// do not query again if the blacklist entry is valid
					if (peer.isExpired()) {
						_peers.erase(iter++);
					} else {
						++iter;
					}
				}
			}

			// if we are a reference node, we have to watch on our clock
			// do some plausibility checks here
			if (hasReference())
			{
				/**
				 * evaluate the current local time
				 */
				if (dtn::utils::Clock::getRating() == 0)
				{
					if (t.getTimestamp() > 0)
					{
						dtn::utils::Clock::setRating(1.0);
						IBRCOMMON_LOGGER_TAG(DTNTPWorker::TAG, warning) << "The local clock seems to be okay again." << IBRCOMMON_LOGGER_ENDL;
					}
				}
			}
			// if we are not a reference then update the local rating if we're not a reference node
			else
			{
				// before we can age our rating we should have been synchronized at least one time
				if (_sync_state.last_sync_set)
				{
					struct timespec ts_now, ts_diff;
					ibrcommon::MonotonicClock::gettime(ts_now);
					ibrcommon::MonotonicClock::diff(_sync_state.last_sync_time, ts_now, ts_diff);

					double last_sync = TimeSyncState::toDouble(ts_diff);

					// the last sync must be in the past
					if (last_sync)
					{
						// calculate the new clock rating
						dtn::utils::Clock::setRating(_sync_state.base_rating * (1.0 / (::pow(_sync_state.sigma, last_sync))));
					}
				}
			}

			// if synchronization is enabled
			if (_sync)
			{
				float best_rating = 0.0;
				dtn::data::EID best_peer;

				// search for other nodes with better credentials
				const std::set<dtn::core::Node> nodes = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();

				// walk through all nodes and find the best peer to sync with
				for (std::set<dtn::core::Node>::const_iterator iter = nodes.begin(); iter != nodes.end(); ++iter)
				{
					float rating = getPeerRating(*iter);

					if (rating > best_rating) {
						best_peer = (*iter).getEID();
						best_rating = rating;
					}
				}

				// sync with best found peer
				if (!best_peer.isNone()) syncWith(best_peer);
			}
		}

		float DTNTPWorker::getPeerRating(const dtn::core::Node &node) const
		{
			// only query for time sync if the other node supports this
			if (!node.has("dtntp")) return 0.0;

			// get discovery attribute
			const std::list<dtn::core::Node::Attribute> attrs = node.get("dtntp");

			if (attrs.empty()) return 0.0;

			// decode attribute parameter
			unsigned int version = 0;
			dtn::data::Timestamp timestamp = 0;
			float rating = 0.0;
			decode(attrs.front(), version, timestamp, rating);

			// we do only support version = 1
			if (version != 1) return 0.0;

			// do not sync if the quality is worse than ours
			if ((rating * (1 - _sync_state.sync_threshold)) <= dtn::utils::Clock::getRating()) return false;

			return rating;
		}

		void DTNTPWorker::syncWith(const dtn::data::EID &peer)
		{
			// check sync peers
			{
				ibrcommon::MutexLock l(_peer_lock);
				const peer_map::const_iterator it = _peers.find(peer);
				if (it != _peers.end())
				{
					const SyncPeer &peer = (*it).second;

					// do not query again if the peer was already queried
					if (peer.state != SyncPeer::STATE_IDLE)
					{
						return;
					}
				}

				// create a new peer entry
				SyncPeer &p = _peers[peer];
				p.touch();
				p.state = SyncPeer::STATE_PREPARE;
			}

			// generate a time sync bundle with a zero timestamp (+age block)
			dtn::data::Bundle b(true);

			try {
				ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();

				// create the payload of the message
				{
					ibrcommon::BLOB::iostream stream = ref.iostream();

					// create a new timesync request
					TimeSyncMessage msg;

					ibrcommon::MutexLock l(_peer_lock);

					// add sync parameters to the peer entry
					SyncPeer &p = _peers[peer];
					p.request_timestamp = msg.origin_timestamp;
					ibrcommon::MonotonicClock::gettime(p.request_monotonic_time);
					p.state = SyncPeer::STATE_REQUEST;

					// write the message
					(*stream) << msg;
				}

				// add the payload to the message
				b.push_back(ref);

				// set the source
				b.source = dtn::core::BundleCore::local;

				// set source application
				b.source.setApplication("dtntp");

				// set the destination
				b.destination = peer;

				// set destination application
				b.destination.setApplication("dtntp");

				// set high priority
				b.set(dtn::data::PrimaryBlock::PRIORITY_BIT1, false);
				b.set(dtn::data::PrimaryBlock::PRIORITY_BIT2, true);

				// set the the destination as singleton receiver
				b.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, true);

				// set the lifetime of the bundle to 60 seconds
				b.lifetime = 60;

				// add a schl block
				dtn::data::ScopeControlHopLimitBlock &schl = b.push_front<dtn::data::ScopeControlHopLimitBlock>();
				schl.setLimit(1);

				transmit(b);
			} catch (const ibrcommon::IOException &ex) {
				IBRCOMMON_LOGGER_TAG(DTNTPWorker::TAG, error) << "error while synchronizing, Exception: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void DTNTPWorker::onUpdateBeacon(const ibrcommon::vinterface&, DiscoveryBeacon &announcement) throw (NoServiceHereException)
		{
			if (!_announce_rating) throw NoServiceHereException("Discovery of time sync mechanisms disabled.");

			std::stringstream ss;
			DTNTPServiceParam p(PROTO_VERSION, dtn::utils::Clock::getRating(), dtn::utils::Clock::getTime());
			announcement.addService(DiscoveryService(dtn::core::Node::CONN_DTNTP, &p));
		}

		void DTNTPWorker::decode(const dtn::core::Node::Attribute &attr, unsigned int &version, dtn::data::Timestamp &timestamp, float &quality) const
		{
			// parse parameters
			std::vector<std::string> parameters = dtn::utils::Utils::tokenize(";", attr.value);
			std::vector<std::string>::const_iterator param_iter = parameters.begin();

			while (param_iter != parameters.end())
			{
				std::vector<std::string> p = dtn::utils::Utils::tokenize("=", (*param_iter));

				if (p[0].compare("version") == 0)
				{
					std::stringstream ss(p[1]);
					ss >> version;
				}

				if (p[0].compare("timestamp") == 0)
				{
					timestamp.fromString(p[1]);
				}

				if (p[0].compare("quality") == 0)
				{
					std::stringstream ss(p[1]);
					ss >> quality;
				}

				++param_iter;
			}
		}

		bool DTNTPWorker::hasReference() const {
			return (_sync_state.sigma == 1.0);
		}

		void DTNTPWorker::sync(const TimeSyncMessage &msg, const struct timeval &tv_offset, const struct timeval &tv_local, const struct timeval &tv_remote)
		{
			// do not sync if we are a reference
			if (hasReference()) return;

			ibrcommon::MutexLock l(_sync_lock);

			// if the received quality of time is worse than ours, ignore it
			if ((msg.peer_rating * (1 - _sync_state.sync_threshold)) <= dtn::utils::Clock::getRating()) return;

			double local_time = dtn::utils::Clock::toDouble(tv_local);
			double remote_time = dtn::utils::Clock::toDouble(tv_remote);

			// adjust sigma if we sync'd at least twice
			if (_sync_state.last_sync_set)
			{
				struct timespec ts_now, ts_diff;
				ibrcommon::MonotonicClock::gettime(ts_now);
				ibrcommon::MonotonicClock::diff(_sync_state.last_sync_time, ts_now, ts_diff);

				// adjust sigma
				double t_stable = TimeSyncState::toDouble(ts_diff);

				if (t_stable > 0.0) {
					double sigma_base = (1 / ::pow(_sync_state.psi, 1/t_stable));
					double sigma_adjustment = ::fabs(remote_time - local_time) / t_stable * msg.peer_rating;
					_sync_state.sigma = sigma_base + sigma_adjustment;

					IBRCOMMON_LOGGER_DEBUG_TAG(DTNTPWorker::TAG, 25) << "new sigma: " << _sync_state.sigma << IBRCOMMON_LOGGER_ENDL;
				}
			}

			if (local_time > remote_time) {
				// determine the new base rating
				_sync_state.base_rating = msg.peer_rating * (remote_time / local_time);
			} else {
				// determine the new base rating
				_sync_state.base_rating = msg.peer_rating * (local_time / remote_time);
			}

			// set the local clock to the new timestamp
			dtn::utils::Clock::setOffset(tv_offset);

			// update the current local rating
			dtn::utils::Clock::setRating(_sync_state.base_rating * (1.0 / (::pow(_sync_state.sigma, 0.001))));

			// store the timestamp of the last synchronization
			ibrcommon::MonotonicClock::gettime(_sync_state.last_sync_time);
			_sync_state.last_sync_set = true;

			// trigger time adjustment event
			dtn::core::TimeAdjustmentEvent::raise(tv_offset, _sync_state.base_rating);
		}

		void DTNTPWorker::callbackBundleReceived(const Bundle &b)
		{
			// do not sync with ourselves
			if (b.source.sameHost(dtn::core::BundleCore::local)) return;

			try {
				// read payload block
				const dtn::data::PayloadBlock &p = b.find<dtn::data::PayloadBlock>();

				// read the type of the message
				char type = 0; (*p.getBLOB().iostream()).get(type);

				switch (type)
				{
					case TimeSyncMessage::TIMESYNC_REQUEST:
					{
						dtn::data::Bundle response = b;

						// relabel with a zero timestamp
						response.relabel(true);

						// set the lifetime of the bundle to 60 seconds
						response.lifetime = 60;

						// switch the source and destination
						response.source = b.destination;
						response.destination = b.source;
						
						// set high priority
						response.set(dtn::data::PrimaryBlock::PRIORITY_BIT1, false);
						response.set(dtn::data::PrimaryBlock::PRIORITY_BIT2, true);

						// set the the destination as singleton receiver
						response.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, true);

						// modify the payload - locked
						{
							ibrcommon::BLOB::Reference ref = p.getBLOB();
							ibrcommon::BLOB::iostream stream = ref.iostream();

							// read the timesync message
							TimeSyncMessage msg;
							(*stream) >> msg;

							// clear the payload
							stream.clear();

							// fill in the own values
							msg.type = TimeSyncMessage::TIMESYNC_RESPONSE;
							msg.peer_rating = dtn::utils::Clock::getRating();
							dtn::utils::Clock::gettimeofday(&msg.peer_timestamp);

							// write the response
							(*stream) << msg;
						}

						// add a second age block
						response.push_front<dtn::data::AgeBlock>();

						// modify the old schl block or add a new one
						try {
							dtn::data::ScopeControlHopLimitBlock &schl = response.find<dtn::data::ScopeControlHopLimitBlock>();
							schl.setLimit(1);
						} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) {
							dtn::data::ScopeControlHopLimitBlock &schl = response.push_front<dtn::data::ScopeControlHopLimitBlock>();
							schl.setLimit(1);
						};

						// send the response
						transmit(response);
						break;
					}

					case TimeSyncMessage::TIMESYNC_RESPONSE:
					{
						// read the ageblock of the bundle
						dtn::data::Bundle::const_find_iterator age_it(b.begin(), dtn::data::AgeBlock::BLOCK_TYPE);

						if (!age_it.next(b.end())) throw ibrcommon::Exception("first ageblock missing");
						const dtn::data::AgeBlock &peer_age = dynamic_cast<const dtn::data::AgeBlock&>(**age_it);

						if (!age_it.next(b.end())) throw ibrcommon::Exception("second ageblock missing");
						const dtn::data::AgeBlock &origin_age = dynamic_cast<const dtn::data::AgeBlock&>(**age_it);

						timeval tv_rtt_measured, tv_local_timestamp, tv_rtt, tv_prop_delay, tv_sync_delay, tv_peer_timestamp, tv_offset;

						timerclear(&tv_rtt_measured);
						tv_rtt_measured.tv_sec = origin_age.getSeconds().get<time_t>();
						tv_rtt_measured.tv_usec = origin_age.getMicroseconds().get<suseconds_t>() % 1000000;

						ibrcommon::BLOB::Reference ref = p.getBLOB();
						ibrcommon::BLOB::iostream stream = ref.iostream();

						// parse the received time sync message
						TimeSyncMessage msg; (*stream) >> msg;

						// do peer checks
						{
							ibrcommon::MutexLock l(_peer_lock);

							// check if the peer entry exists
							const peer_map::const_iterator it = _peers.find(b.source.getNode());
							if (it == _peers.end()) break;

							const SyncPeer &p = (*it).second;

							// check if the peer entry is in the right state
							if (p.state != SyncPeer::STATE_REQUEST)
								break;

							// check if response matches the request
							if ((p.request_timestamp.tv_sec != msg.origin_timestamp.tv_sec) ||
									(p.request_timestamp.tv_usec != msg.origin_timestamp.tv_usec))
								break;

							// determine the RTT of the message exchange
							struct timespec diff, now;
							ibrcommon::MonotonicClock::gettime(now);
							ibrcommon::MonotonicClock::diff(p.request_monotonic_time, now, diff);

							tv_rtt.tv_sec = diff.tv_sec;
							tv_rtt.tv_usec = diff.tv_nsec / 1000;
						}

						// store the current time in tv_local
						dtn::utils::Clock::gettimeofday(&tv_local_timestamp);

						// convert timeval RTT into double value
						double rtt = dtn::utils::Clock::toDouble(tv_rtt);

						// abort here if the rtt is negative or zero!
						if (rtt <= 0.0) {
							IBRCOMMON_LOGGER_TAG(DTNTPWorker::TAG, warning) << "RTT " << rtt << " is too small" << IBRCOMMON_LOGGER_ENDL;
							break;
						}

						double prop_delay = 0.0;

						// assume zero prop. delay if rtt is smaller than the
						// time measured by the age block
						double rtt_measured = dtn::utils::Clock::toDouble(tv_rtt_measured);
						if (rtt <= rtt_measured) {
							timerclear(&tv_prop_delay);
							IBRCOMMON_LOGGER_TAG(DTNTPWorker::TAG, warning) << "Prop. delay " << prop_delay << " is smaller than the tracked time (" << rtt_measured << ")" << IBRCOMMON_LOGGER_ENDL;
						} else {
							timersub(&tv_rtt, &tv_rtt_measured, &tv_prop_delay);
							prop_delay = dtn::utils::Clock::toDouble(tv_prop_delay);
						}

						// half the prop delay
						tv_prop_delay.tv_sec /= 2;
						tv_prop_delay.tv_usec /= 2;

						// copy time interval tracked with the ageblock of the peer
						timerclear(&tv_sync_delay);
						tv_sync_delay.tv_sec = peer_age.getSeconds().get<time_t>();
						tv_sync_delay.tv_usec = peer_age.getMicroseconds().get<suseconds_t>() % 1000000;

						// add sync delay to the peer timestamp
						timeradd(&msg.peer_timestamp, &tv_sync_delay, &tv_peer_timestamp);

						// add propagation delay to the peer timestamp
						timeradd(&msg.peer_timestamp, &tv_prop_delay, &tv_peer_timestamp);

						// calculate offset
						timersub(&tv_local_timestamp, &tv_peer_timestamp, &tv_offset);

						// print out offset to the local clock
						IBRCOMMON_LOGGER_TAG(DTNTPWorker::TAG, info) << "DT-NTP bundle received; rtt = " << rtt << "s; prop. delay = " << prop_delay << "s; clock of " << b.source.getNode().getString() << " has a offset of " << dtn::utils::Clock::toDouble(tv_offset) << "s" << IBRCOMMON_LOGGER_ENDL;

						// sync to this time message
						sync(msg, tv_offset, tv_local_timestamp, tv_peer_timestamp);

						// update the blacklist entry
						ibrcommon::MutexLock l(_peer_lock);
						SyncPeer &p = _peers[b.source.getNode()];
						p.state = SyncPeer::STATE_SYNC;
						p.touch();

						break;
					}
				}
			} catch (const ibrcommon::Exception&) { };
		}

		const DTNTPWorker::TimeSyncState& DTNTPWorker::getState()
		{
			return DTNTPWorker::_sync_state;
		}

		DTNTPWorker::SyncPeer::SyncPeer()
		 : state(STATE_IDLE)
		{
			request_monotonic_time.tv_sec = 0;
			request_monotonic_time.tv_nsec = 0;

			request_timestamp.tv_sec = 0;
			request_timestamp.tv_usec = 0;

			touch();
		}

		DTNTPWorker::SyncPeer::~SyncPeer()
		{
		}

		void DTNTPWorker::SyncPeer::touch()
		{
			_touched = dtn::utils::Clock::getMonotonicTimestamp();
		}

		bool DTNTPWorker::SyncPeer::isExpired() const
		{
			return ((_touched + 60) < dtn::utils::Clock::getMonotonicTimestamp());
		}
	}
}
