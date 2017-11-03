/*
 * dtntunnel.cpp
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

#include "config.h"

#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <vector>

#ifdef HAVE_LIBDAEMON
#include <libdaemon/daemon.h>
#endif

// Base for send and receive bundle to/from the IBR-DTN daemon.
#include "ibrdtn/api/Client.h"

//  TCP client implemented as a stream.
#include <ibrcommon/net/socket.h>

// Some classes to be thread-safe.
#include "ibrcommon/thread/Mutex.h"
#include "ibrcommon/thread/MutexLock.h"
#include <ibrcommon/thread/SignalHandler.h>
#include <ibrcommon/Logger.h>

// Basic functionalities for streaming.
#include <iostream>

// A queue for bundles.
#include <queue>

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>

int tun_alloc(char *dev, short int flags) {

  struct ifreq ifr;
  int fd, err;
  char clonedev[] = "/dev/net/tun";

  /* Arguments taken by the function:
   *
   * char *dev: the name of an interface (or '\0'). MUST have enough
   *   space to hold the interface name if '\0' is passed
   * int flags: interface flags (eg, IFF_TUN etc.)
   */

   /* open the clone device */
   if( (fd = open(clonedev, O_RDWR)) < 0 ) {
     return fd;
   }

   /* preparation of the struct ifr, of type "struct ifreq" */
   memset(&ifr, 0, sizeof(ifr));

   ifr.ifr_flags = flags;   /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

   if (*dev) {
     /* if a device name was specified, put it in the structure; otherwise,
      * the kernel will try to allocate the "next" device of the
      * specified type */
     strncpy(ifr.ifr_name, dev, IFNAMSIZ);
   }

   /* try to create the device */
   if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
     close(fd);
     return err;
   }

  /* if the operation was successful, write back the name of the
   * interface to the variable "dev", so the caller can know
   * it. Note that the caller MUST reserve space in *dev (see calling
   * code below) */
  strcpy(dev, ifr.ifr_name);

  /* this is the special file descriptor that the caller will use to talk
   * with the virtual interface */
  return fd;
}

size_t throughput_data_up[5] = { 0, 0, 0, 0, 0 };
size_t throughput_data_down[5] = { 0, 0, 0, 0, 0 };
int throughput_pos = 0;

void add_throughput_data(ssize_t amount, int updown) {
	if (updown == 0) {
		throughput_data_down[throughput_pos] += amount;
	} else {
		throughput_data_up[throughput_pos] += amount;
	}
}

void timer_display_throughput(int) {
	float throughput_sum_up = 0;
	float throughput_sum_down = 0;

	for (int i = 0; i < 5; ++i) {
		throughput_sum_up += static_cast<float>(throughput_data_up[i]);
		throughput_sum_down += static_cast<float>(throughput_data_down[i]);
	}

	std::cout << "  up: " << setiosflags(std::ios::right) << std::setw(12) << setiosflags(std::ios::fixed) << std::setprecision(2) << (throughput_sum_up/1024) << " kB/s ";
	std::cout << "  down: " << setiosflags(std::ios::right) << std::setw(12) << setiosflags(std::ios::fixed) << std::setprecision(2) << (throughput_sum_down/1024) << " kB/s\r" << std::flush;

	throughput_pos++;
	if (throughput_pos > 4) throughput_pos = 0;

	throughput_data_up[throughput_pos] = 0;
	throughput_data_down[throughput_pos] = 0;
}

class TUN2BundleGateway : public dtn::api::Client
{
	public:
		TUN2BundleGateway(const std::string &app, ibrcommon::socketstream &stream, const std::string &ptp_dev)
		: dtn::api::Client(app, stream), _stream(stream), _fd(-1)
		{
			char tun_name[IFNAMSIZ];

			strcpy(tun_name, ptp_dev.c_str());
			_fd = tun_alloc(tun_name, IFF_TUN);  /* tun interface */

			tun_device = tun_name;

			if (_fd == -1)
				throw ibrcommon::Exception("Error: failed to open tun device");

			// connect the API
			this->connect();
		}

		/**
		 * Destructor of the connection.
		 */
		virtual ~TUN2BundleGateway()
		{
			// Close the tcp connection.
			_stream.close();
		};

		void shutdown() {
			if (_fd < 0) return;

			// close client connection
			this->close();

			// close socket
			::close(_fd);
			_fd = -1;
		}

		void process(const dtn::data::EID &endpoint, unsigned int lifetime = 60) {
			if (_fd == -1) throw ibrcommon::Exception("Tunnel closed.");

			std::vector<char> buffer(65536);
			ssize_t ret = ::read(_fd, &buffer[0], buffer.size());

			if (ret == -1) {
				throw ibrcommon::Exception("Error: failed to read from tun device");
			}

			add_throughput_data(ret, 1);

			// create a blob
			ibrcommon::BLOB::Reference blob = ibrcommon::BLOB::create();

			// add the data
			blob.iostream()->write(&buffer[0], ret);

			// create a new bundle
			dtn::data::Bundle b;

			b.destination = endpoint;
			b.push_back(blob);
			b.lifetime = lifetime;

			// transmit the packet
			(*this) << b;
			flush();
		}

		const std::string& getDeviceName() const
		{
			return tun_device;
		}

	private:
		ibrcommon::socketstream &_stream;

		// file descriptor for the tun device
		int _fd;

		std::string tun_device;

		/**
		 * In this API bundles are received asynchronous. To receive bundles it is necessary
		 * to overload the Client::received()-method. This will be call on a incoming bundles
		 * by another thread.
		 */
		void received(const dtn::data::Bundle &b)
		{
			ibrcommon::BLOB::Reference ref = b.find<dtn::data::PayloadBlock>().getBLOB();
			ibrcommon::BLOB::iostream stream = ref.iostream();
			std::vector<char> buffer(65536);
			stream->read(&buffer[0], buffer.size());
			size_t ret = stream->gcount();

			if (::write(_fd, &buffer[0], ret) < 0)
			{
				IBRCOMMON_LOGGER_TAG("Core", error) << "Error while writing" << IBRCOMMON_LOGGER_ENDL;
			}

			add_throughput_data(ret, 0);
		}
};

bool m_running = true;
TUN2BundleGateway *_gateway = NULL;

void term(int signal)
{
	if (signal >= 1)
	{
		m_running = false;
		if (_gateway != NULL)
			_gateway->shutdown();
	}
}

void print_help(const char *argv0)
{
	std::cout << "-- dtntunnel (IBR-DTN) --" << std::endl;
	std::cout << "Syntax: " << argv0 << " [options] <endpoint>" << std::endl;
	std::cout << " <endpoint>       Destination of outgoing bundles" << std::endl << std::endl;
	std::cout << "* optional parameters *" << std::endl;
	std::cout << " -h               Display help message" << std::endl;
	std::cout << " -d <dev>         Virtual network device to create (default: tun0)" << std::endl;
	std::cout << " -s <name>        Application suffix of the local endpoint (default: tunnel)" << std::endl;
	std::cout << " -l <seconds>     Lifetime of each packet (default: 60)" << std::endl;
	std::cout << " -t               Show throughput" << std::endl;
#ifdef HAVE_LIBDAEMON
	std::cout << " -D               Daemonize the process" << std::endl;
	std::cout << " -k               Stop the running daemon" << std::endl;
	std::cout << " -p <file>        Store the pid in this pidfile" << std::endl;
#endif
}

#ifdef HAVE_LIBDAEMON
static char* __daemon_pidfile__ = NULL;

static const char* __daemon_pid_file_proc__(void) {
	return __daemon_pidfile__;
}
#endif

int main(int argc, char *argv[])
{
	int index;
	int c;

	std::string ptp_dev("tun0");
	std::string app_name("tunnel");
	std::string endpoint("dtn:none");
	unsigned int lifetime = 60;
	bool daemonize = false;
	bool stop_daemon = false;
	std::string pidfile;
	bool throughput = false;

#ifdef HAVE_LIBDAEMON
	while ((c = getopt (argc, argv, "td:s:l:hDkp:")) != -1)
#else
	while ((c = getopt (argc, argv, "td:s:l:h")) != -1)
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
		case 'd':
			ptp_dev = optarg;
			break;

		case 't':
			throughput = true;
			break;

		case 's':
			app_name = optarg;
			break;

		case 'l':
			lifetime = atoi(optarg);
			break;

		default:
			print_help(argv[0]);
			return 1;
	}

	int optindex = 0;
	for (index = optind; index < argc; ++index)
	{
		switch (optindex)
		{
		case 0:
			endpoint = std::string(argv[index]);
			break;
		}

		optindex++;
	}

	// print help if not enough parameters are set
	if (!stop_daemon && (optindex < 1)) { print_help(argv[0]); exit(0); }

	// catch process signals
	ibrcommon::SignalHandler sighandler(term);
	sighandler.handle(SIGINT);
	sighandler.handle(SIGTERM);
	sighandler.handle(SIGQUIT);

	//initialize sighandler after possible exit call
	sighandler.initialize();

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

	IBRCOMMON_LOGGER_TAG("Core", info) << "IBR-DTN IP <-> Bundle Tunnel" << IBRCOMMON_LOGGER_ENDL;

	// create a connection to the dtn daemon
	ibrcommon::vaddress addr("localhost", 4550);
	ibrcommon::socketstream conn(new ibrcommon::tcpsocket(addr));

	try {
		// set-up tun2bundle gateway
		TUN2BundleGateway gateway(app_name, conn, ptp_dev);
		_gateway = &gateway;

		IBRCOMMON_LOGGER_TAG("Core", info) << "Local:  " << app_name << IBRCOMMON_LOGGER_ENDL;
		IBRCOMMON_LOGGER_TAG("Core", info) << "Peer:   " << endpoint << IBRCOMMON_LOGGER_ENDL;
		IBRCOMMON_LOGGER_TAG("Core", info) << "Device: " << gateway.getDeviceName() << IBRCOMMON_LOGGER_ENDL;
		IBRCOMMON_LOGGER_TAG("Core", notice) << IBRCOMMON_LOGGER_ENDL;
		IBRCOMMON_LOGGER_TAG("Core", notice) << "Now you need to set-up the ip tunnel. You can use commands like this:" << IBRCOMMON_LOGGER_ENDL;
		IBRCOMMON_LOGGER_TAG("Core", notice) << "# sudo ip link set " << gateway.getDeviceName() << " up mtu 65535" << IBRCOMMON_LOGGER_ENDL;
		IBRCOMMON_LOGGER_TAG("Core", notice) << "# sudo ip addr add 10.0.0.1/24 dev " << gateway.getDeviceName() << IBRCOMMON_LOGGER_ENDL;
		IBRCOMMON_LOGGER_TAG("Core", notice) << IBRCOMMON_LOGGER_ENDL;

		timer_t timerid;
		struct sigevent sev;

		if (!daemonize && throughput) {
			// enable throughput timer
			signal(SIGRTMIN, timer_display_throughput);

			sev.sigev_notify = SIGEV_SIGNAL;
			sev.sigev_signo = SIGRTMIN;
			sev.sigev_value.sival_ptr = &timerid;

			// create a timer
			timer_create(CLOCK_MONOTONIC, &sev, &timerid);

			// arm the timer
			struct itimerspec its;
			size_t freq_nanosecs = 200000000;
			its.it_value.tv_sec = freq_nanosecs / 1000000000;;
			its.it_value.tv_nsec = freq_nanosecs % 1000000000;
			its.it_interval.tv_sec = its.it_value.tv_sec;
			its.it_interval.tv_nsec = its.it_value.tv_nsec;

			if (timer_settime(timerid, 0, &its, NULL) == -1) {
				IBRCOMMON_LOGGER_TAG("Core", error) << "Timer set failed." << IBRCOMMON_LOGGER_ENDL;
			}
		}

		// destination
		dtn::data::EID eid(endpoint);

		while (m_running)
		{
			gateway.process(eid, lifetime);
		}

		gateway.shutdown();
	} catch (const ibrcommon::Exception &ex) {
		if (m_running) {
			IBRCOMMON_LOGGER_TAG("Core", error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			return -1;
		}
	}

#ifdef HAVE_LIBDAEMON
	if (daemonize) {
		/* Do a cleanup */
		IBRCOMMON_LOGGER_TAG("Core", info) << "Stopped " << app_name << IBRCOMMON_LOGGER_ENDL;
		daemon_retval_send(255);
		daemon_signal_done();
		daemon_pid_file_remove();
	} else
#endif
	{
		std::cout << std::endl;
	}

	return 0;
}
