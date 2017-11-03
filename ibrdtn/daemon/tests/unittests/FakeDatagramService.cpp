/*
 * FakeDatagramService.cpp
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

#include "FakeDatagramService.h"
#include "net/DiscoveryBeacon.h"

#include <algorithm>
#include <string.h>
#include <unistd.h>

FakeDatagramService::FakeDatagramService() : _iface("fake0"), _discovery_sn(0), _fake_peer("dtn://fake-peer") {
	// set connection parameters
	_params.max_msg_length = 114;
	_params.max_seq_numbers = 4;
	_params.flowcontrol = DatagramService::FLOW_STOPNWAIT;
	_params.initial_timeout = 2000;		// initial timeout 2 seconds
	_params.retry_limit = 5;
}

FakeDatagramService::~FakeDatagramService() {
}

void FakeDatagramService::fakeDiscovery() {
	dtn::net::DiscoveryBeacon announcement(dtn::net::DiscoveryBeacon::DISCO_VERSION_01, _fake_peer);

	// set sequencenumber
	announcement.setSequencenumber(_discovery_sn);
	_discovery_sn++;

	// clear all services
	announcement.clearServices();

	// serialize announcement
	std::stringstream ss;
	ss << announcement;

	Message msg;
	msg.type = dtn::net::DatagramConvergenceLayer::HEADER_BROADCAST;
	msg.flags = 0;
	msg.seqno = 0;
	msg.address = "fakeaddr";

	std::istream_iterator<char> eos;
	std::istream_iterator<char> iit(ss);
	std::copy(iit, eos, std::back_inserter(msg.data));

	_recv_queue.push(msg);
}

void FakeDatagramService::genAck(const unsigned int seqno, const std::string &address) {
	Message msg;
	msg.type = dtn::net::DatagramConvergenceLayer::HEADER_ACK;
	msg.flags = 0;
	msg.seqno = seqno;
	msg.address = address;
	_recv_queue.push(msg);
}

void FakeDatagramService::bind() throw (dtn::net::DatagramException) {
	_recv_queue.reset();
}

void FakeDatagramService::shutdown() {
	_recv_queue.abort();
}

void FakeDatagramService::send(const char &type, const char &flags, const unsigned int &seqno, const std::string &address, const char *buf, size_t length) throw (dtn::net::DatagramException) {
	if (type == dtn::net::DatagramConvergenceLayer::HEADER_SEGMENT) {
		// wait 50ms and queue an ack
		ibrcommon::Thread::sleep(50);

		genAck((seqno + 1) % 4, address);
	}
}

void FakeDatagramService::send(const char &type, const char &flags, const unsigned int &seqno, const char *buf, size_t length) throw (dtn::net::DatagramException) {
	// no ack here!
}

size_t FakeDatagramService::recvfrom(char *buf, size_t length, char &type, char &flags, unsigned int &seqno, std::string &address) throw (dtn::net::DatagramException) {
	size_t ret = 0;

	msg_queue::Locked lq = _recv_queue.exclusive();
	try  {
		lq.wait(msg_queue::QUEUE_NOT_EMPTY);
		Message &msg = lq.front();
		type = msg.type;
		flags = msg.flags;
		seqno = msg.seqno;
		address = msg.address;
		ret = msg.data.size();
		if (ret > 0) ::memcpy(buf, &msg.data[0], std::min(ret, length));
		lq.pop();
	} catch (const ibrcommon::QueueUnblockedException&) {
		throw dtn::net::DatagramException("unblocked");
	}

	return ret;
}

const std::string FakeDatagramService::getServiceDescription() const {
	return "addr=42;tsap=1";
}

const ibrcommon::vinterface& FakeDatagramService::getInterface() const {
	return _iface;
}

dtn::core::Node::Protocol FakeDatagramService::getProtocol() const {
	return dtn::core::Node::CONN_BLUETOOTH;
}

const dtn::net::DatagramService::Parameter& FakeDatagramService::getParameter() const {
	return _params;
}
