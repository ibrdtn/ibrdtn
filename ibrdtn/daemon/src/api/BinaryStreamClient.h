/*
 * BinaryStreamClient.h
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

#ifndef BINARYSTREAMCLIENT_H_
#define BINARYSTREAMCLIENT_H_

#include "api/ClientHandler.h"
#include <ibrdtn/streams/StreamConnection.h>
#include <ibrdtn/streams/StreamContactHeader.h>

namespace dtn
{
	namespace api
	{
		class BinaryStreamClient : public dtn::streams::StreamConnection::Callback, public ProtocolHandler
		{
			public:
				BinaryStreamClient(ClientHandler &client, ibrcommon::socketstream &stream);
				virtual ~BinaryStreamClient();

				virtual void eventShutdown(dtn::streams::StreamConnection::ConnectionShutdownCases csc) noexcept;
				virtual void eventTimeout() noexcept;
				virtual void eventError() noexcept;
				virtual void eventConnectionDown() noexcept;
				virtual void eventConnectionUp(const dtn::streams::StreamContactHeader &header) noexcept;

				virtual void eventBundleRefused() noexcept;
				virtual void eventBundleForwarded() noexcept;
				virtual void eventBundleAck(const dtn::data::Length &ack) noexcept;

				const dtn::data::EID& getPeer() const;

				void queue(const dtn::data::Bundle &bundle);

				void received(const dtn::streams::StreamContactHeader &h);
				void run();
				void finally();
				void __cancellation() noexcept;
				bool good() const;

			private:
				class Sender : public ibrcommon::JoinableThread, public ibrcommon::Queue<dtn::data::Bundle>
				{
				public:
					Sender(BinaryStreamClient &client);
					virtual ~Sender();

				protected:
					void run() noexcept;
					void __cancellation() noexcept;

					void received(const dtn::streams::StreamContactHeader &h);
					bool good() const;

				private:
					BinaryStreamClient &_client;
				};

				friend class Sender;

				BinaryStreamClient::Sender _sender;

				dtn::streams::StreamConnection _connection;
				dtn::streams::StreamContactHeader _contact;

				ibrcommon::Queue<dtn::data::Bundle> _sentqueue;
				uint64_t _lastack;

				dtn::data::EID _eid;
		};
	}
}

#endif /* BINARYSTREAMCLIENT_H_ */
