/*
 * EMailConvergenceLayer.h
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: Björn Gernert <mail@bjoern-gernert.de>
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

#ifndef EMAILCONVERGENCELAYER_H_
#define EMAILCONVERGENCELAYER_H_

#include "Component.h"
#include "Configuration.h"

#include "core/EventReceiver.h"
#include "core/TimeEvent.h"
#include "net/ConvergenceLayer.h"
#include "net/DiscoveryBeaconHandler.h"
#include "net/EMailSmtpService.h"
#include "net/EMailImapService.h"

namespace dtn
{
	namespace net
	{
		class EMailConvergenceLayer : public dtn::net::ConvergenceLayer,
			public dtn::core::EventReceiver<dtn::core::TimeEvent>,
			public dtn::net::DiscoveryBeaconHandler,
			public dtn::daemon::IntegratedComponent
		{
		public:
			/**
			 * Creates a new instance of the email convergence layer
			 */
			EMailConvergenceLayer();

			/**
			 * Default destructor
			 */
			virtual ~EMailConvergenceLayer();

			/**
			 * This function will handle all incoming events.
			 * The EMail Convergence Layer is registered to the
			 * following events:
			 *   - dtn::core::TimeEvent
			 */
			void raiseEvent(const dtn::core::TimeEvent &event) noexcept;

			/**
			 * This method updates the given values for discovery service.
			 * It publishes the email address of the EMail Convergence Layer
			 */
			void onUpdateBeacon(const ibrcommon::vinterface &iface, DiscoveryBeacon &beacon)
					noexcept (false);

			/**
			 * Returns the discovery protocol type:
			 *   - "dtn::core::Node::CONN_EMAIL"
			 *
			 * @return Returns the protocol type
			 */
			dtn::core::Node::Protocol getDiscoveryProtocol() const;

			/**
			 * Function for sending a bundle to the given node. If submitting in
			 * intervals is enabled, the job will be stored in a queue and
			 * will be processed later. Otherwise the bundle will be send
			 * without delay. After a successful transmission the job will be
			 * stored in a second queue which will be used to map returning
			 * mails to the right job.
			 *
			 * @param node The receiving node
			 * @param job The needed information for processing the job
			 */
			void queue(const dtn::core::Node &node,
					const dtn::net::BundleTransfer &job);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

		protected:
			void __cancellation() noexcept;

			void componentUp() noexcept;
			void componentDown() noexcept;

		private:
			/**
			 * Stores the configuration of the EMail Convergence Layer
			 */
			const dtn::daemon::Configuration::EMail& _config;

			/**
			 * Stores the reference of the SMTP service
			 */
			EMailSmtpService& _smtp;

			/**
			 * Stores the reference of the IMAP service
			 */
			EMailImapService& _imap;

			/**
			 * Timestamp of the last SMTP submit call
			 */
			size_t _lastSmtpTaskTime;

			/**
			 * Timestamp of the last IMAP query call
			 */
			size_t _lastImapTaskTime;
		};
	}
}

#endif /* EMAILCONVERGENCELAYER_H_ */
