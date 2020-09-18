/*
 * EMailConvergenceLayer.cpp
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

#include "net/EMailConvergenceLayer.h"
#include "net/EMailSmtpService.h"
#include "net/TransferAbortedEvent.h"
#include "core/BundleCore.h"
#include "core/EventDispatcher.h"

#include <ibrdtn/utils/Utils.h>
#include <ibrcommon/Logger.h>

namespace dtn
{
	namespace net
	{
		EMailConvergenceLayer::EMailConvergenceLayer() :
				_config(daemon::Configuration::getInstance().getEMail()),
				_smtp(EMailSmtpService::getInstance()),
				_imap(EMailImapService::getInstance()),
				_lastSmtpTaskTime(0),
				_lastImapTaskTime(0)
		{
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);
		}

		EMailConvergenceLayer::~EMailConvergenceLayer()
		{
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
		}

		void EMailConvergenceLayer::raiseEvent(const dtn::core::TimeEvent &time) noexcept
		{
			if (time.getAction() == dtn::core::TIME_SECOND_TICK)
			{
				if(_lastSmtpTaskTime + _config.getSmtpSubmitInterval() < time.getTimestamp().get<size_t>()
						&& _config.getSmtpSubmitInterval() > 0)
				{
					_smtp.submitQueue();
					_lastSmtpTaskTime = time.getTimestamp().get<size_t>();
				}

				if(_lastImapTaskTime + _config.getImapLookupInterval() < time.getTimestamp().get<size_t>()
						&& _config.getImapLookupInterval() > 0)
				{
					_imap.fetchMails();
					_lastImapTaskTime = time.getTimestamp().get<size_t>();
				}
			}
		}

		void EMailConvergenceLayer::onUpdateBeacon(const ibrcommon::vinterface&, DiscoveryBeacon &beacon)
			noexcept (false)
		{
			beacon.addService(DiscoveryService(getDiscoveryProtocol(), "email=" + _config.getOwnAddress()));
		}

		dtn::core::Node::Protocol EMailConvergenceLayer::getDiscoveryProtocol() const
		{
			return dtn::core::Node::CONN_EMAIL;
		}

		void EMailConvergenceLayer::queue(const dtn::core::Node &node,
				const dtn::net::BundleTransfer &job)
		{
			// Check if node supports email convergence layer
			const std::list<dtn::core::Node::URI> uri_list = node.get(dtn::core::Node::CONN_EMAIL);
			if (uri_list.empty())
			{
				dtn::net::TransferAbortedEvent::raise(node.getEID(), job.getBundle(),
						dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
				return;
			}

			// Get recipient
			std::string recipient = dtn::utils::Utils::tokenize("//", node.getEID().getString())[1];

			// Create new Task
			EMailSmtpService::Task *t = new EMailSmtpService::Task(node, job, recipient);

			// Submit Task
			if(_config.getSmtpSubmitInterval() <= 0)
			{
				_smtp.submitNow(t);
			}else{
				_smtp.queueTask(t);
			}

			IBRCOMMON_LOGGER(info) << "EMail Convergence Layer: Bundle " << t->getJob().getBundle().toString() << " stored in submit queue" << IBRCOMMON_LOGGER_ENDL;

		}

		const std::string EMailConvergenceLayer::getName() const
		{
			return "EMailConvergenceLayer";
		}

		void EMailConvergenceLayer::__cancellation() noexcept {}

		void EMailConvergenceLayer::componentUp() noexcept
		{
			// register as discovery beacon handler
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(this);
		}

		void EMailConvergenceLayer::componentDown() noexcept
		{
			// un-register as discovery beacon handler
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(this);
		}
	}
}
