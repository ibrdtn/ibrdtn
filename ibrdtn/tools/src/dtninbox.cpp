/*
 * dtninbox.cpp
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
#include <ibrdtn/api/Client.h>
#include <ibrcommon/net/socket.h>
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/thread/SignalHandler.h>
#include <ibrdtn/data/PayloadBlock.h>
#include <ibrdtn/data/Bundle.h>
#include <ibrcommon/data/BLOB.h>
#include <ibrcommon/data/File.h>

#include <stdlib.h>
#include <iostream>
#include <map>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#include "io/TarUtils.h"

//global conf values
std::string _conf_name;
std::string _conf_inbox;

//optional parameters
std::string _conf_workdir;
int _conf_quiet = false;

struct option long_options[] =
{
		{"workdir", required_argument, 0, 'w'},
		{"quiet", no_argument, 0, 'q'},
		{0, 0, 0, 0}
};

void print_help()
{
	std::cout << "-- dtninbox (IBR-DTN) --" << std::endl;
	std::cout << "Syntax: dtninbox [options] <name> <inbox>"  << std::endl;
	std::cout << " <name>           The application name" << std::endl;
	std::cout << " <inbox>          Directory where incoming files should be placed" << std::endl << std::endl;
	std::cout << "* optional parameters *" << std::endl;
	std::cout << " -h|--help        Display this text" << std::endl;
	std::cout << " -w|--workdir     Temporary work directory" << std::endl;
	std::cout << " --quiet          Only print error messages" << std::endl;
}

void read_configuration(int argc, char** argv)
{
	while(1)
	{
		/* getopt_long stores the option index here. */
		int option_index = 0;
		int c = getopt_long (argc, argv, "hw:q",
				long_options, &option_index);
		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			/* If this option set a flag, do nothing else now. */
			if (long_options[option_index].flag != 0)
				break;
			printf ("option %s", long_options[option_index].name);
			if (optarg)
				printf (" with arg %s", optarg);
			printf ("\n");
			break;

		case 'h':
			print_help();
			exit(EXIT_SUCCESS);
			break;
		case 'w':
			_conf_workdir = std::string(optarg);
			break;
			// Added case 'q':
		case 'q':
			_conf_quiet = true;
			break;
		default:
			abort();
			break;
		}
	}

	// print help if not enough parameters are set
	if ((argc - optind) < 2)
	{
		print_help();
		exit(EXIT_FAILURE);
	}

	_conf_name = std::string(argv[optind]);
	_conf_inbox = std::string(argv[optind+1]);
}



// set this variable to false to stop the app
bool _running = true;

// global connection
ibrcommon::socketstream *_conn = NULL;

void term(int signal)
{
	if (signal >= 1)
	{
		_running = false;
		if (_conn != NULL) _conn->close();
	}
}

/*
 * main application method
 */
int main(int argc, char** argv)
{
	// catch process signals
	ibrcommon::SignalHandler sighandler(term);
	sighandler.handle(SIGINT);
	sighandler.handle(SIGTERM);

	// read the configuration
	read_configuration(argc, argv);

	//initialize sighandler after possible exit call
	sighandler.initialize();

	if (_conf_workdir.length() > 0)
	{
		ibrcommon::File blob_path(_conf_workdir);

		if (blob_path.exists())
		{
			ibrcommon::BLOB::changeProvider(new ibrcommon::FileBLOBProvider(blob_path), true);
		}
	}

	// backoff for reconnect
	unsigned int backoff = 2;

	// loop, if no stop if requested
	while (_running)
	{
		try {
			// Create a stream to the server using TCP.
			ibrcommon::vaddress addr("localhost", 4550);
			ibrcommon::socketstream conn(new ibrcommon::tcpsocket(addr));

			// set the connection globally
			_conn = &conn;

			// Initiate a client for synchronous receiving
			dtn::api::Client client(_conf_name, conn);

			// Connect to the server. Actually, this function initiate the
			// stream protocol by starting the thread and sending the contact header.
			client.connect();

			// reset backoff if connected
			backoff = 2;

			// check the connection
			while (_running)
			{
				// receive the bundle
				dtn::data::Bundle b = client.getBundle();
				if(!_conf_quiet)
					std::cout << "received bundle: " << b.toString() << std::endl;

				// get the reference to the blob
				ibrcommon::BLOB::Reference ref = b.find<dtn::data::PayloadBlock>().getBLOB();

				// write files into BLOB while it is locked
				{
					ibrcommon::BLOB::iostream stream = ref.iostream();
					io::TarUtils::read(_conf_inbox, *stream);
				}
			}

			// close the client connection
			client.close();

			// close the connection
			conn.close();

			// set the global connection to NULL
			_conn = NULL;
		} catch (const ibrcommon::socket_exception&) {
			// set the global connection to NULL
			_conn = NULL;

			if (_running)
			{
				std::cout << "Connection to bundle daemon failed. Retry in " << backoff << " seconds." << std::endl;
				ibrcommon::Thread::sleep(backoff * 1000);

				// if backoff < 10 minutes
				if (backoff < 600)
				{
					// set a new backoff
					backoff = backoff * 2;
				}
			}
		} catch (const ibrcommon::IOException&) {
			// set the global connection to NULL
			_conn = NULL;

			if (_running)
			{
				std::cout << "Connection to bundle daemon failed. Retry in " << backoff << " seconds." << std::endl;
				ibrcommon::Thread::sleep(backoff * 1000);

				// if backoff < 10 minutes
				if (backoff < 600)
				{
					// set a new backoff
					backoff = backoff * 2;
				}
			}
		} catch (const std::exception&) {
			// set the global connection to NULL
			_conn = NULL;
		}
	}

	return (EXIT_SUCCESS);
}
