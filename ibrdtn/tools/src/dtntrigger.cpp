/*
 * dtntrigger.cpp
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
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/api/Client.h>
#include <ibrcommon/net/socket.h>
#include <ibrcommon/data/File.h>
#include <ibrcommon/thread/SignalHandler.h>
#include <ibrcommon/appstreambuf.h>
#include <ibrcommon/Logger.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_LIBDAEMON
#include <libdaemon/daemon.h>
#endif

// set this variable to false to stop the app
bool _running = true;

// global connection
ibrcommon::socketstream *_conn = NULL;

std::string _appname = "trigger";
std::string _command = "";

ibrcommon::File blob_path("/tmp");

dtn::data::EID group;

bool signed_only = false;

bool daemonize = false;
bool stop_daemon = false;
std::string pidfile;

void print_help()
{
	std::cout << "-- dtntrigger (IBR-DTN) --" << std::endl;
	std::cout << "Syntax: dtntrigger [options] <app-name> <command>"  << std::endl;
	std::cout << "<app-name>    The application name" << std::endl;
	std::cout << "<command>     Shell to execute the trigger script" << std::endl;
	std::cout << "* optional parameters *" << std::endl;
	std::cout << " -h           Display this text" << std::endl;
	std::cout << " -g <group>   Join a group" << std::endl;
	std::cout << " -w           Temporary work directory" << std::endl;
	std::cout << " -s           Process signed bundles only" << std::endl;
#ifdef HAVE_LIBDAEMON
	std::cout << " -D           Daemonize the process" << std::endl;
	std::cout << " -k           Stop the running daemon" << std::endl;
	std::cout << " -p <file>    Store the pid in this pidfile" << std::endl;
#endif
}

#ifdef HAVE_LIBDAEMON
static char* __daemon_pidfile__ = NULL;

static const char* __daemon_pid_file_proc__(void) {
	return __daemon_pidfile__;
}
#endif

int init(int argc, char** argv)
{
	int index;
	int c;

	opterr = 0;

#ifdef HAVE_LIBDAEMON
	while ((c = getopt (argc, argv, "hw:g:sDkp:")) != -1)
#else
	while ((c = getopt (argc, argv, "hw:g:s")) != -1)
#endif
	switch (c)
	{
#ifdef HAVE_LIBDAEMON
		case 'D':
			daemonize = true;
			break;

		case 'k':
			daemonize = true;
			stop_daemon = true;
			break;

		case 'p':
			pidfile = optarg;
			break;
#endif
		case 'w':
			blob_path = ibrcommon::File(optarg);
			break;

		case 'g':
			group = std::string(optarg);
			break;

		case '?':
			if (optopt == 'w')
			fprintf (stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint (optopt))
			fprintf (stderr, "Unknown option `-%c'.\n", optopt);
			else
			fprintf (stderr,
					 "Unknown option character `\\x%x'.\n",
					 optopt);
			return 1;

		case 's':
			signed_only = true;
			break;

		default:
			print_help();
			return 1;
	}

	int optindex = 0;
	for (index = optind; index < argc; ++index)
	{
		switch (optindex)
		{
		case 0:
			_appname = std::string(argv[index]);
			break;

		default:
			_command += std::string(argv[index]) + " ";
			break;
		}

		optindex++;
	}

	// print help if not enough parameters are set
	if (!stop_daemon && (optindex < 2)) { print_help(); exit(0); }

	// enable file based BLOBs if a correct path is set
	if (blob_path.exists())
	{
		ibrcommon::BLOB::changeProvider(new ibrcommon::FileBLOBProvider(blob_path));
	}

	return 0;
}

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
	// read the configuration
	if (init(argc, argv) > 0)
	{
		return (EXIT_FAILURE);
	}

	// logging options
	//const unsigned char logopts = ibrcommon::Logger::LOG_DATETIME | ibrcommon::Logger::LOG_LEVEL;
	const unsigned char logopts = 0;

	// error filter
	const unsigned char logerr = ibrcommon::Logger::LOGGER_ERR | ibrcommon::Logger::LOGGER_CRIT;

	// logging filter, everything but debug, err and crit
	const unsigned char logstd = ibrcommon::Logger::LOGGER_ALL ^ (ibrcommon::Logger::LOGGER_DEBUG | logerr);

	// syslog filter, everything but DEBUG and NOTICE
	const unsigned char logsys = ibrcommon::Logger::LOGGER_ALL ^ (ibrcommon::Logger::LOGGER_DEBUG | ibrcommon::Logger::LOGGER_NOTICE);

#ifdef HAVE_LIBDAEMON
	if (daemonize) {
		// enable syslog logging
		ibrcommon::Logger::enableSyslog(argv[0], LOG_PID, LOG_DAEMON, logsys);
	} else
#endif
	{
		// add logging to the cout
		ibrcommon::Logger::addStream(std::cout, logstd, logopts);

		// add logging to the cerr
		ibrcommon::Logger::addStream(std::cerr, logerr, logopts);
	}

#ifdef HAVE_LIBDAEMON
	if (daemonize)
	{
#ifdef HAVE_DAEMON_RESET_SIGS
		/* Reset signal handlers */
		if (daemon_reset_sigs(-1) < 0) {
			IBRCOMMON_LOGGER_TAG("Core", error) << "Failed to reset all signal handlers: " << strerror(errno) << IBRCOMMON_LOGGER_ENDL;
			return 1;
		}

		/* Unblock signals */
		if (daemon_unblock_sigs(-1) < 0) {
			IBRCOMMON_LOGGER_TAG("Core", error) << "Failed to unblock all signals: " << strerror(errno) << IBRCOMMON_LOGGER_ENDL;
			return 1;
		}
#endif
		pid_t pid;

		/* Set identification string for the daemon for both syslog and PID file */
		daemon_pid_file_ident = daemon_log_ident = daemon_ident_from_argv0(argv[0]);

		/* set the pid file path */
		if (pidfile.length() > 0) {
			__daemon_pidfile__ = new char[pidfile.length() + 1];
			::strcpy(__daemon_pidfile__, pidfile.c_str());
			daemon_pid_file_proc = __daemon_pid_file_proc__;
		}

		/* Check if we are called with -k parameter */
		if (stop_daemon)
		{
			int ret;

			/* Kill daemon with SIGTERM */

			/* Check if the new function daemon_pid_file_kill_wait() is available, if it is, use it. */
			if ((ret = daemon_pid_file_kill_wait(SIGTERM, 5)) < 0)
				IBRCOMMON_LOGGER_TAG("Core", warning) << "Failed to kill daemon: " << strerror(errno) << IBRCOMMON_LOGGER_ENDL;

			return ret < 0 ? 1 : 0;
		}

		/* Check that the daemon is not rung twice a the same time */
		if ((pid = daemon_pid_file_is_running()) >= 0) {
			IBRCOMMON_LOGGER_TAG("Core", error) << "Daemon already running on PID file " << pid << IBRCOMMON_LOGGER_ENDL;
			return 1;
		}

		/* Prepare for return value passing from the initialization procedure of the daemon process */
		if (daemon_retval_init() < 0) {
			IBRCOMMON_LOGGER_TAG("Core", error) << "Failed to create pipe." << IBRCOMMON_LOGGER_ENDL;
			return 1;
		}

		/* Do the fork */
		if ((pid = daemon_fork()) < 0) {

			/* Exit on error */
			daemon_retval_done();
			return 1;

		} else if (pid) { /* The parent */
			int ret;

			/* Wait for 20 seconds for the return value passed from the daemon process */
			if ((ret = daemon_retval_wait(20)) < 0) {
				IBRCOMMON_LOGGER_TAG("Core", error) << "Could not recieve return value from daemon process: " << strerror(errno) << IBRCOMMON_LOGGER_ENDL;
				return 255;
			}

			return ret;

		} else { /* The daemon */
			/* Close FDs */
			if (daemon_close_all(-1) < 0) {
				IBRCOMMON_LOGGER_TAG("Core", error) << "Failed to close all file descriptors: " << strerror(errno) << IBRCOMMON_LOGGER_ENDL;

				/* Send the error condition to the parent process */
				daemon_retval_send(1);

				/* Do a cleanup */
				daemon_retval_send(255);
				daemon_signal_done();
				daemon_pid_file_remove();

				return -1;
			}

			/* Create the PID file */
			if (daemon_pid_file_create() < 0) {
				IBRCOMMON_LOGGER_TAG("Core", error) << "Could not create PID file ( " << strerror(errno) << ")." << IBRCOMMON_LOGGER_ENDL;
				daemon_retval_send(2);

				/* Do a cleanup */
				daemon_retval_send(255);
				daemon_signal_done();
				daemon_pid_file_remove();

				return -1;
			}

			/* Send OK to parent process */
			daemon_retval_send(0);
		}
	}
#endif

	// catch process signals
	ibrcommon::SignalHandler sighandler(term);
	sighandler.handle(SIGINT);
	sighandler.handle(SIGTERM);

	// initialize signal-handler after possible exit call
	sighandler.initialize();

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
			dtn::api::Client client(_appname, group, conn);

			// Connect to the server. Actually, this function initiate the
			// stream protocol by starting the thread and sending the contact header.
			client.connect();

			// reset backoff if connected
			backoff = 2;

			IBRCOMMON_LOGGER_TAG("Core", info) << "dtntrigger registered on endpoint '" << _appname << "'" << IBRCOMMON_LOGGER_ENDL;

			// check the connection
			while (_running)
			{
				// receive the bundle
				dtn::data::Bundle b = client.getBundle();

				// skip non-signed bundles if we should accept signed bundles only
				if (signed_only && !b.get(dtn::data::PrimaryBlock::DTNSEC_STATUS_VERIFIED)) continue;

				// get the reference to the blob
				ibrcommon::BLOB::Reference ref = b.find<dtn::data::PayloadBlock>().getBLOB();

				// write data to temporary file
				try {
					// set-up context
					::setenv("B_SOURCE", b.source.getString().c_str(), 1);
					::setenv("B_DESTINATION", b.destination.getString().c_str(), 1);
					::setenv("B_LIFETIME", b.lifetime.toString().c_str(), 1);
					::setenv("B_TIMESTAMP", b.timestamp.toString().c_str(), 1);
					::setenv("B_SEQUENCENUMBER", b.sequencenumber.toString().c_str(), 1);
					::setenv("B_PROCFLAGS", b.procflags.toString().c_str(), 1);

					// call the script
					ibrcommon::appstreambuf outbuf(_command, ibrcommon::appstreambuf::MODE_WRITE);
					std::ostream out(&outbuf);

					out.exceptions(std::ios::badbit | std::ios::eofbit);
					out << ref.iostream()->rdbuf();
					out.flush();
				} catch (const std::ios_base::failure&) {

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
				IBRCOMMON_LOGGER_TAG("Core", warning) << "Connection to bundle daemon failed. Retry in " << backoff << " seconds." << IBRCOMMON_LOGGER_ENDL;
				ibrcommon::Thread::sleep(backoff * 1000);

				// if backoff < 10 minutes
				if (backoff < 600)
				{
					// set a new backoff
					backoff = backoff * 2;
				}
			}
		} catch (const ibrcommon::IOException &ex) {
			// set the global connection to NULL
			_conn = NULL;

			if (_running)
			{
				IBRCOMMON_LOGGER_TAG("Core", warning) << "Connection to bundle daemon failed. Retry in " << backoff << " seconds." << IBRCOMMON_LOGGER_ENDL;
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

#ifdef HAVE_LIBDAEMON
	if (daemonize) {
		/* Do a cleanup */
		IBRCOMMON_LOGGER_TAG("Core", info) << "Stopped dtntrigger on endpoint '" << _appname << "'" << IBRCOMMON_LOGGER_ENDL;
		daemon_retval_send(255);
		daemon_signal_done();
		daemon_pid_file_remove();
	}
#endif

	return (EXIT_SUCCESS);
}
