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
#include <ibrdtn/data/StatusReportBlock.h>
#include <ibrdtn/data/TrackingBlock.h>
#include "ibrdtn/api/Client.h"
#include <ibrcommon/net/socket.h>
#include <ibrcommon/net/socketstream.h>
#include "ibrcommon/thread/Mutex.h"
#include "ibrcommon/thread/MutexLock.h"
#include "ibrcommon/TimeMeasurement.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <unistd.h>

class ProbeBundle : public dtn::data::Bundle
{
public:
	ProbeBundle(const dtn::data::EID &d, bool tracking, bool group)
	{
		destination = d;
		set(DESTINATION_IS_SINGLETON, !group);

		if (tracking) {
			push_back<dtn::data::TrackingBlock>();
		}
	}

	virtual ~ProbeBundle()
	{
	}
};

class TrackingBundle : public dtn::data::Bundle
{
public:
	TrackingBundle(const dtn::data::Bundle &apib)
	 : dtn::data::Bundle(apib)
	{
	}

	virtual ~TrackingBundle() {
	}

	bool isAdmRecord() {
		return (get(dtn::data::PrimaryBlock::APPDATA_IS_ADMRECORD));
	}

	dtn::data::StatusReportBlock getStatusReport() const {
		StatusReportBlock srb;
		const dtn::data::PayloadBlock &payload = find<dtn::data::PayloadBlock>();
		srb.read(payload);
		return srb;
	}

	const dtn::data::TrackingBlock& getTrackingBlock() const {
		return find<dtn::data::TrackingBlock>();
	}
};

class Tracer : public dtn::api::Client
{
	public:
		Tracer(dtn::api::Client::COMMUNICATION_MODE mode, ibrcommon::socketstream &stream)
		 : dtn::api::Client("", stream, mode), _stream(stream)
		{
		}

		virtual ~Tracer()
		{
		}

		void printReason(char code) {
			switch (code) {
			case dtn::data::StatusReportBlock::LIFETIME_EXPIRED:
				::printf("      * lifetime expired\n");
				break;
			case dtn::data::StatusReportBlock::FORWARDED_OVER_UNIDIRECTIONAL_LINK:
				::printf("      * forwarded over unidirectional link\n");
				break;
			case dtn::data::StatusReportBlock::TRANSMISSION_CANCELED:
				::printf("      * transmission canceled\n");
				break;
			case dtn::data::StatusReportBlock::DEPLETED_STORAGE:
				::printf("      * depleted storage\n");
				break;
			case dtn::data::StatusReportBlock::DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE:
				::printf("      * destination endpoint ID is unintelligible\n");
				break;
			case dtn::data::StatusReportBlock::NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE:
				::printf("      * no known route to destination from here\n");
				break;
			case dtn::data::StatusReportBlock::NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE:
				::printf("      * no timely contact with next node on route\n");
				break;
			case dtn::data::StatusReportBlock::BLOCK_UNINTELLIGIBLE:
				::printf("      * block unintelligible\n");
				break;
			default:
				break;
			}
		}

		void formattedLog(size_t i, const dtn::data::Bundle &b, const ibrcommon::TimeMeasurement &tm)
		{
			TrackingBundle tb(b);

			const std::string source = b.source.getString();

			// format time data
			std::stringstream time;
			time << tm;

			if (tb.isAdmRecord()) {
				dtn::data::StatusReportBlock sr = tb.getStatusReport();

				if (sr.status & dtn::data::StatusReportBlock::FORWARDING_OF_BUNDLE) {
					std::stringstream type;
					type << "forwarded";
					::printf(" %3d: %-48s %-12s %6s\n", (unsigned int)i, source.c_str(), type.str().c_str(), time.str().c_str());
					printReason(sr.reasoncode);
				}

				if (sr.status & dtn::data::StatusReportBlock::RECEIPT_OF_BUNDLE) {
					std::stringstream type;
					type << "reception";
					::printf(" %3d: %-48s %-12s %6s\n", (unsigned int)i, source.c_str(), type.str().c_str(), time.str().c_str());
					printReason(sr.reasoncode);
				}

				if (sr.status & dtn::data::StatusReportBlock::DELIVERY_OF_BUNDLE) {
					std::stringstream type;
					type << "delivery";
					::printf(" %3d: %-48s %-12s %6s\n", (unsigned int)i, source.c_str(), type.str().c_str(), time.str().c_str());
					printReason(sr.reasoncode);
				}

				if (sr.status & dtn::data::StatusReportBlock::DELETION_OF_BUNDLE) {
					std::stringstream type;
					type << "deletion";
					::printf(" %3d: %-48s %-12s %6s\n", (unsigned int)i, source.c_str(), type.str().c_str(), time.str().c_str());
					printReason(sr.reasoncode);
				}
			} else {
				std::stringstream type;
				type << "ECHO";

				::printf(" %3d: %-48s %-12s %6s\n", (unsigned int)i, source.c_str(), type.str().c_str(), time.str().c_str());

				try {
					const dtn::data::TrackingBlock &track_block = tb.getTrackingBlock();
					const dtn::data::TrackingBlock::tracking_list &list = track_block.getTrack();

					for (dtn::data::TrackingBlock::tracking_list::const_iterator iter = list.begin(); iter != list.end(); ++iter)
					{
						const dtn::data::TrackingBlock::TrackingEntry &entry = (*iter);
						::printf("       # %s\n", entry.endpoint.getString().c_str());
					}

				} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) { };
			}

			::fflush(stdout);
		}

		void tracepath(const dtn::data::EID &destination, bool group, size_t timeout, unsigned char options, bool tracking)
		{
			// create a bundle
			ProbeBundle b(destination, tracking, group);

			// set lifetime
			b.lifetime = timeout;

			// set some stupid payload
			ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
			b.push_back(ref);
			(*ref.iostream()) << "follow the white rabbit";

			// request forward and delivery reports
			if (options & 0x01) b.set(dtn::data::PrimaryBlock::REQUEST_REPORT_OF_BUNDLE_FORWARDING, true);
			if (options & 0x02) b.set(dtn::data::PrimaryBlock::REQUEST_REPORT_OF_BUNDLE_DELIVERY, true);
			if (options & 0x04) b.set(dtn::data::PrimaryBlock::REQUEST_REPORT_OF_BUNDLE_RECEPTION, true);
			if (options & 0x08) b.set(dtn::data::PrimaryBlock::REQUEST_REPORT_OF_BUNDLE_DELETION, true);

			b.reportto = dtn::data::EID("api:me");

			ibrcommon::TimeMeasurement tm;

			std::list<dtn::data::Bundle> bundles;

			try {
				size_t i = 0;

				// start the timer
				tm.start();

				std::cout << "TRACE TO " << destination.getString() << std::endl;

				// send the bundle
				(*this) << b; (*this).flush();

				try {
					// now receive all incoming bundles
					while (true)
					{
						dtn::data::Bundle recv = getBundle(timeout);
						bundles.push_back(recv);
						tm.stop();

						// generate formatted output
						i++;
						formattedLog(i, recv, tm);
					}
				} catch (const std::exception &e) {
					// error or timeout
				}

				std::cout << std::endl;
			} catch (const dtn::api::ConnectionException&) {
				std::cout << "Disconnected." << std::endl;
			} catch (const ibrcommon::IOException&) {
				std::cout << "Error while receiving a bundle." << std::endl;
			}
		}

	private:
		ibrcommon::socketstream &_stream;
};

void print_help()
{
	std::cout << "-- dtntracepath (IBR-DTN) --" << std::endl
			<< "Syntax: dtntracepath [options] <dst>"  << std::endl
			<< " <dst>    set the destination eid (e.g. dtn://node/null)" << std::endl << std::endl
			<< "* optional parameters *" << std::endl
			<< " -h               Display this text" << std::endl
			<< " -t <seconds>     Time to wait for reports (default: 10)" << std::endl
			<< " -d               Request deletion report" << std::endl
			<< " -f               Request forward report" << std::endl
			<< " -r               Request reception report" << std::endl
			<< " -p               Add tracking block to record the bundle path" << std::endl
			<< " -g               Destination is a group endpoint" << std::endl
			<< " -U <socket>      Connect to UNIX domain socket API" << std::endl;
}

int main(int argc, char *argv[])
{
	std::string trace_destination = "dtn://local";
	// forwarded = 0x01
	// delivered = 0x02
	// reception = 0x04
	// deletion = 0x08
	unsigned char report_options = 0x02;
	bool tracking = false;
	bool group = false;

	size_t timeout = 10;
	int opt = 0;
	ibrcommon::File unixdomain;

	dtn::api::Client::COMMUNICATION_MODE mode = dtn::api::Client::MODE_BIDIRECTIONAL;

	if (argc == 1)
	{
		print_help();
		return 0;
	}

	while ((opt = getopt(argc, argv, "htU:fdrpg")) != -1)
	{
		switch (opt)
		{
		case 'h':
			print_help();
			return 0;

		case 't':
			timeout = atoi(optarg);
			break;

		case 'U':
			unixdomain = ibrcommon::File(optarg);
			break;

		case 'f':
			report_options |= 0x01;
			break;

		case 'd':
			report_options |= 0x08;
			break;

		case 'r':
			report_options |= 0x04;
			break;

		case 'p':
			tracking = true;
			break;

		case 'g':
			group = true;
			break;
		}
	}

	// the last parameter is always the destination
	trace_destination = argv[argc - 1];

	try {
		ibrcommon::clientsocket *sock = NULL;
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
		Tracer tracer(mode, conn);

		// Connect to the server. Actually, this function initiate the
		// stream protocol by starting the thread and sending the contact header.
		tracer.connect();

		// target address
		tracer.tracepath(trace_destination, group, timeout, report_options, tracking);

		// Shutdown the client connection.
		tracer.close();
		conn.close();

	} catch (const ibrcommon::socket_exception&) {
		std::cerr << "Can not connect to the daemon. Does it run?" << std::endl;
		return -1;
	} catch (const std::exception &e) {
		std::cerr << "Unexcepted error: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}
