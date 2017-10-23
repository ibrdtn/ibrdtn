/*
 * ExtendedApiHandler.h
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 * Written-by: Stephen Roettger <roettger@ibr.cs.tu-bs.de>
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

#ifndef EXTENDEDAPIHANDLER_H_
#define EXTENDEDAPIHANDLER_H_

#include "api/Registration.h"
#include "core/Node.h"
#include "api/ClientHandler.h"

#include <ibrdtn/api/PlainSerializer.h>
#include <ibrdtn/data/Bundle.h>
#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/thread/Queue.h>
#include <ibrcommon/net/socketstream.h>


namespace dtn
{
	namespace api
	{
		class ExtendedApiHandler : public ProtocolHandler
		{
		public:
			enum STATUS_CODES
			{
				API_STATUS_NOTIFY_COMMON = 600,
				API_STATUS_NOTIFY_NEIGHBOR = 601,
				API_STATUS_NOTIFY_BUNDLE = 602,
				API_STATUS_NOTIFY_REPORT = 603,
				API_STATUS_NOTIFY_CUSTODY = 604
			};

			ExtendedApiHandler(ClientHandler &client, ibrcommon::socketstream &stream);
			virtual ~ExtendedApiHandler();

			virtual void run();
			virtual void finally();
			virtual void __cancellation() throw ();

			bool good() const;

		private:
			class Sender : public ibrcommon::JoinableThread
			{
			public:
				Sender(ExtendedApiHandler &conn);
				virtual ~Sender();

			protected:
				void run() throw ();
				void finally() throw ();
				void __cancellation() throw ();

			private:
				ExtendedApiHandler &_handler;
			} *_sender;

			static void sayBundleID(std::ostream &stream, const dtn::data::BundleID &id);
			static dtn::data::BundleID readBundleID(const std::vector<std::string>&, const size_t start);

			/**
			 * Announce a new bundle in the queue
			 */
			void notifyBundle(dtn::data::MetaBundle &bundle);

			/**
			 * Transform a administrative record into a notification
			 */
			void notifyAdministrativeRecord(dtn::data::MetaBundle &bundle);

			ibrcommon::Mutex _write_lock;

			dtn::data::Bundle _bundle_reg;

			dtn::data::EID _endpoint;
			ibrcommon::Queue<dtn::data::BundleID> _bundle_queue;

			dtn::api::PlainSerializer::Encoding _encoding;
		};
	}
}

#endif /* EXTENDEDAPICONNECTION_H_ */
