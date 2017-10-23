/*
 * dtnping.cpp
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
#include "ibrdtn/api/Client.h"
#include "ibrcommon/net/socket.h"
#include "ibrcommon/thread/Mutex.h"
#include "ibrcommon/thread/MutexLock.h"
#include <ibrcommon/thread/SignalHandler.h>
#include "ibrcommon/TimeMeasurement.h"

#include <iostream>
#include <stdint.h>

#define CREATE_CHUNK_SIZE 2048

class EchoClient : public dtn::api::Client
{
	public:
		EchoClient(dtn::api::Client::COMMUNICATION_MODE mode, std::string app, ibrcommon::socketstream &stream)
		 : dtn::api::Client(app, stream, mode), _stream(stream)
		{
			seq=0;
		}

		virtual ~EchoClient()
		{
		}

		const dtn::data::Bundle waitForReply(const int timeout)
		{
			double wait=(timeout*1000);
			ibrcommon::TimeMeasurement tm;
			while ( wait > 0 )
			{
				try {
					tm.start();
					dtn::data::Bundle b = this->getBundle((int)(wait/1000));
					tm.stop();
					checkReply(b);
					return b;
				} catch (const std::string &errmsg) {
					std::cerr << errmsg << std::endl;
				}
				wait = wait - tm.getMilliseconds();
			}
			throw ibrcommon::Exception("timeout is set to zero");
		}

		void echo(EID destination, int size, int lifetime, bool encryption = false, bool sign = false)
		{
			lastdestination=destination.getString();
			seq++;
			
			// create a bundle
			dtn::data::Bundle b;

			// set bundle destination
			b.destination = destination;

			// enable encryption if requested
			if (encryption) b.set(dtn::data::PrimaryBlock::DTNSEC_REQUEST_ENCRYPT, true);

			// enable signature if requested
			if (sign) b.set(dtn::data::PrimaryBlock::DTNSEC_REQUEST_SIGN, true);

			// set lifetime
			b.lifetime = lifetime;
			
			// create a new blob
			ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();

			// append the blob as payload block to the bundle
			b.push_back(ref);

			// open the iostream
			{
				ibrcommon::BLOB::iostream stream = ref.iostream();

				// Add magic seqno
				(*stream).write((char*)&seq, 4);

				if (size > 4)
				{
					size-=4;

					// create testing pattern, chunkwise to ocnserve memory
					char pattern[CREATE_CHUNK_SIZE];
					for (size_t i = 0; i < sizeof(pattern); ++i)
					{
						pattern[i] = static_cast<char>(static_cast<int>('0') + (i % 10));
					}

					while (size > CREATE_CHUNK_SIZE) {
						(*stream).write(pattern, CREATE_CHUNK_SIZE);
						size -= CREATE_CHUNK_SIZE;
					}

					(*stream).write(pattern, size);
				}
			}

			// send the bundle
			(*this) << b;

			// ... flush out
			flush();
			
		}
		
		void checkReply(dtn::data::Bundle &bundle) {
			size_t reply_seq = 0;
			ibrcommon::BLOB::Reference blob = bundle.find<dtn::data::PayloadBlock>().getBLOB();
			blob.iostream()->read((char *)(&reply_seq),4 );

			if (reply_seq != seq) {
				std::stringstream ss;
				ss << "sequence number mismatch, awaited " << seq << ", got " << reply_seq;
				throw ss.str();
			}
			if (bundle.source.getString() != lastdestination) {
				throw std::string("ignoring bundle from source " + bundle.source.getString() + " awaited " + lastdestination);
			}
		}

	private:
		ibrcommon::socketstream &_stream;
		uint32_t seq;
		std::string lastdestination;
};


	
void print_help()
{
	std::cout << "-- dtnping (IBR-DTN) --" << std::endl
			<< "Syntax: dtnping [options] <dst>"  << std::endl
			<< " <dst>            Set the destination eid (e.g. dtn://node/echo)" << std::endl << std::endl
			<< "* optional parameters *" << std::endl
			<< " -h|--help        Display this text" << std::endl
			<< " --src <name>     Set the source application name (e.g. echo-client)" << std::endl
			<< " --nowait         Do not wait for a reply" << std::endl
			<< " --abortfail      Abort after first packetloss" << std::endl
			<< " --size           The size of the payload" << std::endl
			<< " --count <n>      Send X echo in a row" << std::endl
			<< " --delay <seconds>" << std::endl
			<< "                  Delay between a received response and the next request" << std::endl
			<< " --lifetime <seconds>" << std::endl
			<< "                  Set the lifetime of outgoing bundles; default: 30" << std::endl
			<< " --encrypt        Request encryption on the bundle layer" << std::endl
			<< " --sign           Request signature on the bundle layer" << std::endl
			<< " -U <socket>      Connect to UNIX domain socket API" << std::endl;
}

size_t _received = 0, _transmitted = 0;
double _min = 0.0, _max = 0.0, _avg = 0.0;
ibrcommon::TimeMeasurement _runtime;
ibrcommon::Conditional __pause;
dtn::api::Client *__client = NULL;

EID _addr;
bool __exit = false;

void print_summary()
{
	_runtime.stop();

	double loss = 0;
	if (_transmitted > 0) loss = ((static_cast<double>(_transmitted) - static_cast<double>(_received)) / static_cast<double>(_transmitted)) * 100.0;

	double avg_value = 0;
	if (_received > 0) avg_value = ( _avg / static_cast<double>(_received) );

	std::cout << std::endl << "--- " << _addr.getString() << " echo statistics --- " << std::endl;
	std::cout << _transmitted << " bundles transmitted, " << _received << " received, " << loss << "% bundle loss, time " << _runtime << std::endl;
	std::cout << "rtt min/avg/max = ";
	ibrcommon::TimeMeasurement::format(std::cout, _min) << "/";
	ibrcommon::TimeMeasurement::format(std::cout, avg_value) << "/";
	ibrcommon::TimeMeasurement::format(std::cout, _max) << " ms" << std::endl;
}

void term(int signal)
{
	if (signal >= 1)
	{
		if (!__exit)
		{
			ibrcommon::MutexLock l(__pause);
			if (__client != NULL) __client->abort();
			__exit = true;
			__pause.abort();
		}
	}
}

int main(int argc, char *argv[])
{
	// catch process signals
	ibrcommon::SignalHandler sighandler(term);
	sighandler.handle(SIGINT);
	sighandler.handle(SIGTERM);
	sighandler.initialize();

	std::string ping_destination = "dtn://local/echo";
	std::string ping_source = "";
	int ping_size = 64;
	unsigned int lifetime = 30;
	bool wait_for_reply = true;
	bool stop_after_first_fail = false;
	bool nonstop = true;
	double interval_pause = 1.0f;
	size_t count = 0;
	dtn::api::Client::COMMUNICATION_MODE mode = dtn::api::Client::MODE_BIDIRECTIONAL;
	ibrcommon::File unixdomain;
	bool bundle_encryption = false;
	bool bundle_signed = false;

	if (argc == 1)
	{
		print_help();
		return 0;
	}

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];

		// print help if requested
		if ((arg == "-h") || (arg == "--help"))
		{
			print_help();
			return 0;
		}

		else if (arg == "--encrypt")
		{
			bundle_encryption = true;
		}

		else if (arg == "--sign")
		{
			bundle_signed = true;
		}

		else if (arg == "--nowait")
		{
			mode = dtn::api::Client::MODE_SENDONLY;
			wait_for_reply = false;
		}
		
		else if ( arg == "--abortfail") {
			stop_after_first_fail=true;
		}

		else if (arg == "--src" && argc > i)
		{
			ping_source = argv[i + 1];
			i++;
		}

		else if (arg == "--size" && argc > i)
		{
			std::stringstream str_size;
			str_size.str( argv[i + 1] );
			str_size >> ping_size;
			i++;
		}

		else if (arg == "--count" && argc > i)
		{
			std::stringstream str_count;
			str_count.str( argv[i + 1] );
			str_count >> count;
			i++;
			nonstop = false;
		}

		else if (arg == "--delay" && argc > i)
		{
			std::stringstream str_delay;
			str_delay.str( argv[i + 1] );
			str_delay >> interval_pause;
			i++;
		}

		else if (arg == "--lifetime" && argc > i)
		{
			std::stringstream data; data << argv[i + 1];
			data >> lifetime;
			i++;
		}
		else if (arg == "-U" && argc > i)
		{
			if (++i > argc)
			{
					std::cout << "argument missing!" << std::endl;
					return -1;
			}

			unixdomain = ibrcommon::File(argv[i]);
		}
	}

	// the last parameter is always the destination
	ping_destination = argv[argc - 1];

	// target address
	_addr = EID(ping_destination);

	ibrcommon::TimeMeasurement tm;
	
	
	try {
		// Create a stream to the server using TCP.
		ibrcommon::clientsocket *sock = NULL;

		// check if the unixdomain socket exists
		if (unixdomain.exists())
		{
			// connect to the unix domain socket
			sock = new ibrcommon::filesocket(unixdomain);
		}
		else
		{
			// connect to the standard local api port
			ibrcommon::vaddress addr("localhost", 4550);
			sock = new ibrcommon::tcpsocket(addr);
		}

		ibrcommon::socketstream conn(sock);

		// Initiate a derivated client
		EchoClient client(mode, ping_source, conn);

		// set the global client pointer
		{
			ibrcommon::MutexLock l(__pause);
			__client = &client;
		}

		// Connect to the server. Actually, this function initiate the
		// stream protocol by starting the thread and sending the contact header.
		client.connect();

		std::cout << "ECHO " << _addr.getString() << " " << ping_size << " bytes of data." << std::endl;

		// measure runtime
		_runtime.start();

		try {
			for (unsigned int i = 0; (i < count) || nonstop; ++i)
			{
				// set sending time
				tm.start();

				// Call out a ECHO
				client.echo( _addr, ping_size, lifetime, bundle_encryption, bundle_signed );
				_transmitted++;
			
				if (wait_for_reply)
				{
					try {
						try {
							dtn::data::Bundle response = client.waitForReply(2*lifetime);

							// print out measurement result
							tm.stop();

							size_t reply_seq = 0;
							size_t payload_size = 0;

							// check for min/max/avg
							_avg += tm.getMilliseconds();
							if ((_min > tm.getMilliseconds()) || _min == 0) _min = static_cast<double>(tm.getMilliseconds());
							if ((_max < tm.getMilliseconds()) || _max == 0) _max = static_cast<double>(tm.getMilliseconds());

							{
								ibrcommon::BLOB::Reference blob = response.find<dtn::data::PayloadBlock>().getBLOB();
								blob.iostream()->read((char *)(&reply_seq),4 );
								payload_size = blob.size();
							}

							std::cout << payload_size << " bytes from " << response.source.getString() << ": seq=" << reply_seq << " ttl=" << response.lifetime.toString() << " time=" << tm << std::endl;
							_received++;
						} catch (const dtn::api::ConnectionTimeoutException &e) {
							if (stop_after_first_fail)
								throw dtn::api::ConnectionTimeoutException();

							std::cout << "Timeout." << std::endl;
						}

						if (interval_pause > 0)
						{
							ibrcommon::MutexLock l(__pause);
							__pause.wait(interval_pause * 1000);
						}
					} catch (const ibrcommon::Conditional::ConditionalAbortException &e) {
						if (e.reason == ibrcommon::Conditional::ConditionalAbortException::COND_TIMEOUT)
						{
							continue;
						}
						// aborted
						break;
					} catch (const dtn::api::ConnectionAbortedException&) {
						// aborted... do a clean shutdown
						break;
					} catch (const dtn::api::ConnectionTimeoutException &ex) {
						if (stop_after_first_fail)
						{
							std::cout << "No response, aborting." << std::endl;
							break;
						}
					}
				}
				else{
					try{
						// Added this from the (wait_for_reply) part, because you weren't
						// able to stop the dtnping while using --nowait without
						// this.
						if (interval_pause > 0)
						{
							ibrcommon::MutexLock l(__pause);
							__pause.wait(interval_pause);
						}
					} catch (const ibrcommon::Conditional::ConditionalAbortException &e) {
						if (e.reason == ibrcommon::Conditional::ConditionalAbortException::COND_TIMEOUT)
						{
							continue;
						}
						// aborted
						break;
					}
				}
			}
		} catch (const dtn::api::ConnectionException&) {
			std::cerr << "Disconnected." << std::endl;
		} catch (const ibrcommon::IOException&) {
			std::cerr << "Error while receiving a bundle." << std::endl;
		}

		// Shutdown the client connection.
		client.close();
		conn.close();
	} catch (const ibrcommon::socket_exception&) {
		std::cerr << "Can not connect to the daemon. Does it run?" << std::endl;
		return -1;
	} catch (const std::exception&) {
		std::cerr << "unknown error" << std::endl;
	}

	print_summary();

	return 0;
}
