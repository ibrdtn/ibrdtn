/*
 * FakeDatagramService.h
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

#ifndef FAKEDATAGRAMSERVICE_H_
#define FAKEDATAGRAMSERVICE_H_

#include "net/DatagramConvergenceLayer.h"
#include "net/DatagramService.h"
#include <ibrcommon/thread/Queue.h>
#include <ibrdtn/data/EID.h>
#include <vector>

class FakeDatagramService : public dtn::net::DatagramService {
public:
	struct Message {
		char type;
		char flags;
		unsigned int seqno;
		std::string address;
		std::vector<char> data;
	};

	FakeDatagramService();
	virtual ~FakeDatagramService();

	void bind() noexcept (false);

	void shutdown();

	void send(const char &type, const char &flags, const unsigned int &seqno, const std::string &address, const char *buf, size_t length) noexcept (false);

	void send(const char &type, const char &flags, const unsigned int &seqno, const char *buf, size_t length) noexcept (false);

	size_t recvfrom(char *buf, size_t length, char &type, char &flags, unsigned int &seqno, std::string &address) noexcept (false);

	const std::string getServiceDescription() const;

	const ibrcommon::vinterface& getInterface() const;

	dtn::core::Node::Protocol getProtocol() const;

	const dtn::net::DatagramService::Parameter& getParameter() const;

	void fakeDiscovery();

private:
	void genAck(const unsigned int seqno, const std::string &address);

	DatagramService::Parameter _params;
	typedef ibrcommon::Queue<Message> msg_queue;
	msg_queue _recv_queue;
	const ibrcommon::vinterface _iface;
	uint16_t _discovery_sn;
	dtn::data::EID _fake_peer;
};

#endif /* FAKEDATAGRAMSERVICE_H_ */
