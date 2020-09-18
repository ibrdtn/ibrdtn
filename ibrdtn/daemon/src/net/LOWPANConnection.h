/*
 * LOWPANConnection.h
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

#ifndef LOWPANCONNECTION_H_
#define LOWPANCONNECTION_H_

#include "net/LOWPANConvergenceLayer.h"

#include <ibrcommon/thread/Queue.h>
#include <ibrcommon/net/lowpanstream.h>

using namespace dtn::data;

namespace dtn
{
	namespace net
	{
		class LOWPANConnectionSender : public ibrcommon::JoinableThread
		{
			public:
				LOWPANConnectionSender(ibrcommon::lowpanstream &_stream);
				virtual ~LOWPANConnectionSender();

				/**
				 * Queueing a job, originally coming from the DTN core, to work its way down to the CL through the lowpanstream
				 * @param job Reference to the job conatining EID and bundle
				 */
				void queue(const dtn::net::BundleTransfer &job);
				void run() noexcept;
				void __cancellation() noexcept;

			private:
				ibrcommon::lowpanstream &_stream;
				ibrcommon::Queue<dtn::net::BundleTransfer> _queue;
		};

		class LOWPANConvergenceLayer;
		class LOWPANConnection : public ibrcommon::DetachedThread
		{
		public:
			/**
			 * LOWPANConnection constructor
			 * @param _address IEEE 802.15.4 short address to identfy the connection
			 * @param LOWPANConvergenceLayer reference
			 */
			LOWPANConnection(const ibrcommon::vaddress &_address, LOWPANConvergenceLayer &cl);

			virtual ~LOWPANConnection();

			/**
			 * IEEE 802.15.4 short address of the node this connection handles data for.
			 */
			const ibrcommon::vaddress _address;

			/**
			 * Getting the lowpanstream connected with the LOWPANConnection
			 * @return lowpanstream reference
			 */
			ibrcommon::lowpanstream& getStream();

			void run() noexcept;
			void setup() noexcept;
			void finally() noexcept;
			void __cancellation() noexcept;

			/**
			 * Instance of the LOWPANConnectionSender
			 */
			LOWPANConnectionSender _sender;

		private:
			ibrcommon::lowpanstream _stream;
			LOWPANConvergenceLayer &_cl;
		};
	}
}
#endif /*LOWPANCONNECTION_H_*/
