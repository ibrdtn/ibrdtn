/*
 * dtnstream.cpp
 *
 * The application dtnstream can transfer a data stream to another instance of dtnstream.
 * It uses an extension block to mark the sequence of the stream.
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
#include "streaming/BundleStream.h"
#include <ibrdtn/api/Client.h>
#include <ibrdtn/data/EID.h>
#include <ibrcommon/net/socket.h>
#include <ibrcommon/data/File.h>
#include <iostream>
#include <unistd.h>

void print_help()
{
	std::cout << "-- dtnstream (IBR-DTN) --" << std::endl;
	std::cout << "Syntax: dtnstream [options]"  << std::endl;
	std::cout << "" << std::endl;
	std::cout << "* optional parameters *" << std::endl;
	std::cout << " -h               Display this text" << std::endl;
	std::cout << " -U <socket>      Connect to UNIX domain socket API" << std::endl;
	std::cout << " -s <identifier>  Set the source identifier (e.g. stream)" << std::endl;
	std::cout << "" << std::endl;
	std::cout << "* send options *" << std::endl;
	std::cout << " -d <destination> Set the destination eid (e.g. dtn://node/stream)" << std::endl;
	std::cout << " -G               Destination is a group" << std::endl;
	std::cout << " -C <bytes>       Set the max. chunk size (max. size of each bundle)" << std::endl;
	std::cout << " -c <bytes>       Set the min. chunk size (min. size of each bundle)" << std::endl;
	std::cout << " -p <0..2>        Set the bundle priority (0 = low, 1 = normal, 2 = high)" << std::endl;
	std::cout << " -l <seconds>     Set the lifetime of stream chunks default: 30" << std::endl;
	std::cout << " -E               Request encryption on the bundle layer" << std::endl;
	std::cout << " -S               Request signature on the bundle layer" << std::endl;
	std::cout << " -f               Enable flow-control using Status Reports" << std::endl;
	std::cout << "" << std::endl;
	std::cout << "* receive options *" << std::endl;
	std::cout << " -g <group>       Join a destination group" << std::endl;
	std::cout << " -t <seconds>     Set the timeout of the buffer" << std::endl;
	std::cout << " -w               Wait for the bundle with seqno zero" << std::endl;
	std::cout << "" << std::endl;
}

int main(int argc, char *argv[])
{
	int opt = 0;
	dtn::data::EID _destination;
	std::string _source = "stream";
	int _priority = 1;
	unsigned int _lifetime = 30;
	unsigned int _receive_timeout = 0;

	size_t _min_chunk_size = 4096;
	size_t _max_chunk_size = 512000;

	dtn::data::EID _group;
	bool _bundle_encryption = false;
	bool _bundle_signed = false;
	bool _bundle_group = false;
	bool _wait_seq_zero = false;
	bool _flow_control = false;
	ibrcommon::File _unixdomain;

	while((opt = getopt(argc, argv, "hg:Gd:t:s:c:C:p:l:ESU:wf")) != -1)
	{
		switch (opt)
		{
		case 'h':
			print_help();
			return 0;

		case 'd':
			_destination = std::string(optarg);
			break;

		case 'g':
			_group = std::string(optarg);
			break;

		case 'G':
			_bundle_group = true;
			break;

		case 's':
			_source = optarg;
			break;

		case 'c':
			_min_chunk_size = atoi(optarg);
			break;

		case 'C':
			_max_chunk_size = atoi(optarg);
			break;

		case 't':
			_receive_timeout = atoi(optarg);
			break;

		case 'p':
			_priority = atoi(optarg);
			break;

		case 'l':
			_lifetime = atoi(optarg);
			break;

		case 'E':
			_bundle_encryption = true;
			break;

		case 'S':
			_bundle_signed = true;
			break;

		case 'U':
			_unixdomain = ibrcommon::File(optarg);
			break;

		case 'w':
			_wait_seq_zero = true;
			break;

		case 'f':
			_flow_control = true;
			break;

		default:
			std::cout << "unknown command" << std::endl;
			return -1;
		}
	}

	try {
		// Create a stream to the server using TCP.
		ibrcommon::clientsocket *sock = NULL;

		// check if the unixdomain socket exists
		if (_unixdomain.exists())
		{
			// connect to the unix domain socket
			sock = new ibrcommon::filesocket(_unixdomain);
		}
		else
		{
			// connect to the standard local api port
			ibrcommon::vaddress addr("localhost", 4550);
			sock = new ibrcommon::tcpsocket(addr);
		}

		ibrcommon::socketstream conn(sock);

		// Initiate a derivated client
		BundleStream bs(conn, _min_chunk_size, _max_chunk_size, _source, _group, _wait_seq_zero);

		// set flow-control as requested
		bs.setAutoFlush(_flow_control);

		// set the receive timeout
		bs.setReceiveTimeout(_receive_timeout);

		// Connect to the server. Actually, this function initiate the
		// stream protocol by starting the thread and sending the contact header.
		bs.connect();

		// transmitter mode
		if (_destination != dtn::data::EID())
		{
			bs.base().destination = _destination;
			bs.base().setPriority(dtn::data::PrimaryBlock::PRIORITY(_priority));
			bs.base().lifetime = _lifetime;
			if (_bundle_encryption) bs.base().set(dtn::data::PrimaryBlock::DTNSEC_REQUEST_ENCRYPT, true);
			if (_bundle_signed) bs.base().set(dtn::data::PrimaryBlock::DTNSEC_REQUEST_SIGN, true);
			if (_bundle_group) bs.base().set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, false);
			std::ostream stream(&bs.rdbuf());
			stream << std::cin.rdbuf() << std::flush;
		}
		// receiver mode
		else
		{
			std::istream stream(&bs.rdbuf());
			std::cout << stream.rdbuf() << std::flush;
		}

		// Shutdown the client connection.
		bs.close();
		conn.close();
	} catch (const ibrcommon::socket_exception&) {
		std::cerr << "Can not connect to the daemon. Does it run?" << std::endl;
		return -1;
	} catch (const std::exception&) {

	}
}
