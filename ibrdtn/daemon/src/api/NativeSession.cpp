/*
 * NativeSession.cpp
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
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

#include "api/NativeSession.h"
#include "api/NativeSerializer.h"
#include "core/BundleCore.h"
#include "core/EventDispatcher.h"

#include <ibrdtn/data/PayloadBlock.h>
#include <ibrcommon/data/BLOB.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/RWLock.h>
#include <ibrcommon/thread/MutexLock.h>

namespace dtn
{
	namespace api
	{
		NativeSessionCallback::~NativeSessionCallback()
		{
		}

		const std::string NativeSession::TAG = "NativeSession";

		NativeSession::NativeSession(NativeSessionCallback *session_cb, NativeSerializerCallback *serializer_cb)
		 : _receiver(*this), _session_cb(session_cb), _serializer_cb(serializer_cb)
		{
			// set the local endpoint to the default
			_endpoint = _registration.getDefaultEID();

			// listen to QueueBundleEvents
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::add(&_receiver);

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 15) << "Session created" << IBRCOMMON_LOGGER_ENDL;
		}

		NativeSession::NativeSession(NativeSessionCallback *session_cb, NativeSerializerCallback *serializer_cb, const std::string &handle)
		 : _registration(handle), _receiver(*this), _session_cb(session_cb), _serializer_cb(serializer_cb)
		{
			// set the local endpoint to the default
			_endpoint = _registration.getDefaultEID();

			// listen to QueueBundleEvents
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::add(&_receiver);

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 15) << "Session created" << IBRCOMMON_LOGGER_ENDL;
		}

		NativeSession::~NativeSession()
		{
			// invalidate the callback pointer
			{
				ibrcommon::RWLock l(_cb_mutex);
				_session_cb = NULL;
				_serializer_cb = NULL;
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 15) << "Session destroyed" << IBRCOMMON_LOGGER_ENDL;
		}

		void NativeSession::destroy() noexcept
		{
			// un-listen from QueueBundleEvents
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::remove(&_receiver);

			_registration.abort();
		}

		const dtn::data::EID& NativeSession::getNodeEID() const noexcept
		{
			return dtn::core::BundleCore::local;
		}

		void NativeSession::fireNotificationBundle(const dtn::data::BundleID &id) noexcept
		{
			ibrcommon::MutexLock l(_cb_mutex);
			if (_session_cb == NULL) return;
			_session_cb->notifyBundle(id);
		}

		void NativeSession::fireNotificationStatusReport(const dtn::data::EID &source, const dtn::data::StatusReportBlock &report) noexcept
		{
			ibrcommon::MutexLock l(_cb_mutex);
			if (_session_cb == NULL) return;
			_session_cb->notifyStatusReport(source, report);
		}

		void NativeSession::fireNotificationCustodySignal(const dtn::data::EID &source, const dtn::data::CustodySignalBlock &custody) noexcept
		{
			ibrcommon::MutexLock l(_cb_mutex);
			if (_session_cb == NULL) return;
			_session_cb->notifyCustodySignal(source, custody);
		}

		void NativeSession::setEndpoint(const std::string &suffix) noexcept (false)
		{
			// error checking
			if (suffix.length() <= 0)
			{
				throw NativeSessionException("given endpoint is not acceptable");
			}
			else
			{
				/* unsubscribe from the old endpoint and subscribe to the new one */
				_registration.unsubscribe(_endpoint);

				// set new application part
				_endpoint.setApplication(suffix);

				// subscribe to new endpoint
				_registration.subscribe(_endpoint);
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "Endpoint set to " << _endpoint.getString() << IBRCOMMON_LOGGER_ENDL;
		}

		void NativeSession::resetEndpoint() noexcept
		{
			_registration.unsubscribe(_endpoint);
			_endpoint = _registration.getDefaultEID();
			_registration.subscribe(_endpoint);

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "Endpoint set to " << _endpoint.getString() << IBRCOMMON_LOGGER_ENDL;
		}

		void NativeSession::addEndpoint(const std::string &suffix) noexcept (false)
		{
			// error checking
			if (suffix.length() <= 0)
			{
				throw NativeSessionException("given endpoint is not acceptable");
			}
			else
			{
				dtn::data::EID new_endpoint = dtn::core::BundleCore::local;
				new_endpoint.setApplication( suffix );
				_registration.subscribe(new_endpoint);
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "Endpoint " << suffix << " added" << IBRCOMMON_LOGGER_ENDL;
		}

		void NativeSession::removeEndpoint(const std::string &suffix) noexcept (false)
		{
			// error checking
			if (suffix.length() <= 0)
			{
				throw NativeSessionException("given endpoint is not acceptable");
			}
			else
			{
				dtn::data::EID old_endpoint = dtn::core::BundleCore::local;
				old_endpoint.setApplication( suffix );
				_registration.unsubscribe(old_endpoint);
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "Endpoint " << suffix << " removed" << IBRCOMMON_LOGGER_ENDL;
		}

		void NativeSession::addRegistration(const dtn::data::EID &eid) noexcept (false)
		{
			// error checking
			if (eid == dtn::data::EID())
			{
				throw NativeSessionException("given endpoint is not acceptable");
			}
			else
			{
				_registration.subscribe(eid);
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "Registration " << eid.getString() << " added" << IBRCOMMON_LOGGER_ENDL;
		}

		void NativeSession::removeRegistration(const dtn::data::EID &eid) noexcept (false)
		{
			// error checking
			if (eid == dtn::data::EID())
			{
				throw NativeSessionException("given endpoint is not acceptable");
			}
			else
			{
				_registration.unsubscribe(eid);
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "Registration " << eid.getString() << " removed" << IBRCOMMON_LOGGER_ENDL;
		}

		void NativeSession::clearRegistration() noexcept
		{
			resetEndpoint();

			const std::set<dtn::data::EID> subs = _registration.getSubscriptions();
			for (std::set<dtn::data::EID>::const_iterator it = subs.begin(); it != subs.end(); ++it) {
				const dtn::data::EID &e = (*it);
				if (e != _endpoint) {
					_registration.unsubscribe(e);
				}
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "Registrations cleared" << IBRCOMMON_LOGGER_ENDL;
		}

		std::vector<std::string> NativeSession::getSubscriptions() noexcept
		{
			const std::set<dtn::data::EID> subs = _registration.getSubscriptions();
			std::vector<std::string> ret;
			for (std::set<dtn::data::EID>::const_iterator it = subs.begin(); it != subs.end(); ++it) {
				ret.push_back((*it).getString());
			}
			return ret;
		}

		void NativeSession::next(RegisterIndex ri) noexcept (false)
		{
			try {
				const dtn::data::BundleID id = _bundle_queue.take();

				IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "Next bundle in queue is " << id.toString() << IBRCOMMON_LOGGER_ENDL;

				load(ri, id);
			} catch (const ibrcommon::QueueUnblockedException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 15) << "No next bundle available" << IBRCOMMON_LOGGER_ENDL;
				throw BundleNotFoundException();
			}
		}

		void NativeSession::load(RegisterIndex ri, const dtn::data::BundleID &id) noexcept (false)
		{
			// load the bundle
			try {
				_bundle[ri] = dtn::core::BundleCore::getInstance().getStorage().get(id);

				try {
					// process the bundle block (security, compression, ...)
					dtn::core::BundleCore::processBlocks(_bundle[ri]);

					IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "Bundle " << id.toString() << " loaded" << IBRCOMMON_LOGGER_ENDL;
				} catch (const ibrcommon::Exception &e) {
					// clear the register
					_bundle[ri] = dtn::data::Bundle();
					IBRCOMMON_LOGGER_TAG(NativeSession::TAG, warning) << "Failed to process bundle " << id.toString() << ": " << e.what() << IBRCOMMON_LOGGER_ENDL;
				}
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 15) << "Failed to load bundle " << id.toString() << ", Exception: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				throw BundleNotFoundException();
			}
		}

		void NativeSession::get(RegisterIndex ri) noexcept
		{
			ibrcommon::MutexLock l(_cb_mutex);
			if (_serializer_cb == NULL) return;

			NativeSerializer serializer(*_serializer_cb, NativeSerializer::BUNDLE_FULL);
			try {
				serializer << _bundle[ri];
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(NativeSession::TAG, error) << "Get failed " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void NativeSession::getInfo(RegisterIndex ri) noexcept
		{
			ibrcommon::MutexLock l(_cb_mutex);
			if (_serializer_cb == NULL) return;

			NativeSerializer serializer(*_serializer_cb, NativeSerializer::BUNDLE_INFO);
			try {
				serializer << _bundle[ri];
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(NativeSession::TAG, error) << "Get failed " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void NativeSession::free(RegisterIndex ri) noexcept (false)
		{
			try {
				dtn::core::BundleCore::getInstance().getStorage().remove(_bundle[ri]);
				_bundle[ri] = dtn::data::Bundle();
			} catch (const ibrcommon::Exception&) {
				throw BundleNotFoundException();
			}
		}

		void NativeSession::clear(RegisterIndex ri) noexcept
		{
			_bundle[ri] = dtn::data::Bundle();
		}

		void NativeSession::delivered(const dtn::data::BundleID &id) const noexcept (false)
		{
			try {
				// announce this bundle as delivered
				const dtn::data::MetaBundle meta = dtn::core::BundleCore::getInstance().getStorage().info(id);
				_registration.delivered(meta);

				IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "Bundle " << id.toString() << " marked as delivered" << IBRCOMMON_LOGGER_ENDL;
			} catch (const ibrcommon::Exception&) {
				throw BundleNotFoundException();
			}
		}

		dtn::data::BundleID NativeSession::send(RegisterIndex ri) noexcept
		{
			// forward the bundle to the storage processing
			dtn::api::Registration::processIncomingBundle(_endpoint, _bundle[ri]);

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "Bundle " << _bundle[ri].toString() << " sent" << IBRCOMMON_LOGGER_ENDL;

			return _bundle[ri];
		}

		void NativeSession::put(RegisterIndex ri, const dtn::data::Bundle &b) noexcept
		{
			// Copy the given bundle into the local register
			_bundle[ri] = b;
		}

		void NativeSession::put(RegisterIndex ri, const dtn::data::PrimaryBlock &p) noexcept
		{
			// clear all blocks in the register
			_bundle[ri].clear();

			// Copy the given primary block into the local register
			((dtn::data::PrimaryBlock&)_bundle[ri]) = p;
		}

		void NativeSession::write(RegisterIndex ri, const char *buf, const size_t len, const size_t offset) noexcept
		{
			try {
				dtn::data::PayloadBlock &payload = _bundle[ri].find<dtn::data::PayloadBlock>();

				ibrcommon::BLOB::Reference ref = payload.getBLOB();
				ibrcommon::BLOB::iostream stream = ref.iostream();

				std::streamsize stream_size = stream.size();

				if ((offset > 0) || (stream_size < static_cast<std::streamsize>(offset))) {
					(*stream).seekp(0, std::ios_base::end);
				} else {
					(*stream).seekp(offset);
				}

				(*stream).write(buf, len);
				(*stream) << std::flush;
			} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) {
				dtn::data::PayloadBlock &payload = _bundle[ri].push_back<dtn::data::PayloadBlock>();

				ibrcommon::BLOB::Reference ref = payload.getBLOB();
				ibrcommon::BLOB::iostream stream = ref.iostream();

				std::streamsize stream_size = stream.size();

				if ((offset > 0) || (stream_size < static_cast<std::streamsize>(offset))) {
					(*stream).seekp(0, std::ios_base::end);
				} else {
					(*stream).seekp(offset);
				}
				(*stream).write(buf, len);
				(*stream) << std::flush;
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 25) << len << " bytes added to the payload" << IBRCOMMON_LOGGER_ENDL;
		}

		void NativeSession::read(RegisterIndex ri, char *buf, size_t &len, const size_t offset) noexcept
		{
			try {
				dtn::data::PayloadBlock &payload = _bundle[ri].find<dtn::data::PayloadBlock>();

				ibrcommon::BLOB::Reference ref = payload.getBLOB();
				ibrcommon::BLOB::iostream stream = ref.iostream();

				(*stream).seekg(offset);
				(*stream).read(buf, len);

				len = (*stream).gcount();
			} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) {
				IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 25) << "no payload block available" << IBRCOMMON_LOGGER_ENDL;
				len = 0;
			}
		}

		NativeSession::BundleReceiver::BundleReceiver(NativeSession &session)
		 : _session(session)
		{
		}

		NativeSession::BundleReceiver::~BundleReceiver()
		{
		}

		void NativeSession::BundleReceiver::raiseEvent(const dtn::routing::QueueBundleEvent &queued) noexcept
		{
			// ignore fragments - we can not deliver them directly to the client
			if (queued.bundle.isFragment()) return;

			if (_session._registration.hasSubscribed(queued.bundle.destination))
			{
				_session._registration.notify(Registration::NOTIFY_BUNDLE_AVAILABLE);
			}
		}

		void NativeSession::receive() noexcept (false)
		{
			Registration &reg = _registration;
			try {
				try {
					const dtn::data::MetaBundle id = reg.receiveMetaBundle();

					if (id.procflags & dtn::data::PrimaryBlock::APPDATA_IS_ADMRECORD) {
						// transform custody signals & status reports into notifies
						fireNotificationAdministrativeRecord(id);

						// announce the delivery of this bundle
						reg.delivered(id);
					} else {
						IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "fire notification for new bundle " << id.toString() << IBRCOMMON_LOGGER_ENDL;

						// put the bundle into the API queue
						_bundle_queue.push(id);

						// notify the client about the new bundle
						fireNotificationBundle(id);
					}
				} catch (const dtn::storage::NoBundleFoundException&) {
					IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 25) << "no more bundles found - wait until we are notified" << IBRCOMMON_LOGGER_ENDL;
					reg.wait_for_bundle();
				}
			} catch (const ibrcommon::QueueUnblockedException &ex) {
				throw NativeSessionException(std::string("loop aborted - ") + ex.what());
			} catch (const std::exception &ex) {
				throw NativeSessionException(std::string("loop aborted - ") + ex.what());
			}
		}

		void NativeSession::fireNotificationAdministrativeRecord(const dtn::data::MetaBundle &bundle)
		{
			// load the whole bundle
			const dtn::data::Bundle b = dtn::core::BundleCore::getInstance().getStorage().get(bundle);

			// get the payload block of the bundle
			const dtn::data::PayloadBlock &payload = b.find<dtn::data::PayloadBlock>();

			try {
				// try to decode as status report
				dtn::data::StatusReportBlock report;
				report.read(payload);

				IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "fire notification for status report" << IBRCOMMON_LOGGER_ENDL;

				// fire the status report notification
				fireNotificationStatusReport(b.source, report);
			} catch (const dtn::data::StatusReportBlock::WrongRecordException&) {
				// this is not a status report
			}

			try {
				// try to decode as custody signal
				dtn::data::CustodySignalBlock custody;
				custody.read(payload);

				IBRCOMMON_LOGGER_DEBUG_TAG(NativeSession::TAG, 20) << "fire notification for custody signal" << IBRCOMMON_LOGGER_ENDL;

				// fire the custody signal notification
				fireNotificationCustodySignal(b.source, custody);
			} catch (const dtn::data::CustodySignalBlock::WrongRecordException&) {
				// this is not a custody report
			}
		}

		const std::string& NativeSession::getHandle() const
		{
			return _registration.getHandle();
		}
	} /* namespace net */
} /* namespace dtn */
