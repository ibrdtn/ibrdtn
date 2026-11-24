/*
 * Main.cpp
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
#include "Configuration.h"
#include <ibrcommon/Logger.h>
#include <ibrcommon/data/File.h>
#include <ibrcommon/thread/SignalHandler.h>

#ifdef HAVE_LIBDAEMON
#include <libdaemon/daemon.h>
#endif

#include <string.h>
#include <set>

#include <sys/types.h>
#include <unistd.h>

#ifdef __WIN32__
#define LOG_PID 0
#define LOG_DAEMON 0
#else
#include <syslog.h>
#include <pwd.h>
#endif

#include "NativeDaemon.h"

dtn::daemon::NativeDaemon _dtnd;

// ------------------------------------------------------------
#include <ibrcommon/thread/Thread.h>

class TerminationThread: public ibrcommon::JoinableThread
// This class defines a thread that can terminate the daemon after a
// certain execution delay.
{
typedef void (*f_type)(int);
private:
  int delay_; // ms
  f_type f_;
		     
public:
  TerminationThread(int delay, f_type f) {
    // Once the thread is started, function f will be called after the
    // specified delay (ms).
    delay_ = delay;
    f_ = f;
  }
  
  void run() throw ()
  {
    sleep(delay_);
    std::cout << "Terminating daemon as planned\n";
    (*f_)(SIGINT);
  }
  void __cancellation() throw () {}
};
// ------------------------------------------------------------

/**
 * setup logging capabilities
 */

// logging options
unsigned char logopts = ibrcommon::Logger::LOG_DATETIME | ibrcommon::Logger::LOG_LEVEL | ibrcommon::Logger::LOG_TAG;

// error filter
const unsigned char logerr = ibrcommon::Logger::LOGGER_ERR | ibrcommon::Logger::LOGGER_CRIT;

// logging filter, everything but debug, notice, err and crit
const unsigned char logstd = ibrcommon::Logger::LOGGER_ALL ^ (ibrcommon::Logger::LOGGER_DEBUG | ibrcommon::Logger::LOGGER_NOTICE | logerr);

// syslog filter, everything but DEBUG and NOTICE
const unsigned char logsys = ibrcommon::Logger::LOGGER_ALL ^ (ibrcommon::Logger::LOGGER_DEBUG | ibrcommon::Logger::LOGGER_NOTICE);

// debug off by default
bool _debug = false;

// marker if the shutdown was called
ibrcommon::Conditional _shutdown_cond;
bool _shutdown = false;

// on interruption do this!
void func_sighandler(int signal)
{
	// do not handle further signals if the shutdown is in progress
	if (_shutdown) return;

	switch (signal)
	{
	case SIGTERM:
	case SIGINT:
	{
		ibrcommon::MutexLock l(_shutdown_cond);
		_shutdown = true;
		_shutdown_cond.signal(true);
		break;
	}
#ifndef __WIN32__
	case SIGUSR1:
		if (!_debug)
		{
			ibrcommon::Logger::addStream(std::cout, ibrcommon::Logger::LOGGER_DEBUG, logopts);
			_debug = true;
		}

		_dtnd.setDebug(99);
		break;
	case SIGUSR2:
		_dtnd.setDebug(0);
		break;
	case SIGHUP:
		_dtnd.reload();
		break;
#endif
	default:
		// dummy handler
		break;
	}
}

int __daemon_run()
{
	// catch process signals
	ibrcommon::SignalHandler sighandler(func_sighandler);
	sighandler.handle(SIGINT);
	sighandler.handle(SIGTERM);
#ifndef __WIN32__
	sighandler.handle(SIGHUP);
	sighandler.handle(SIGQUIT);
	sighandler.handle(SIGUSR1);
	sighandler.handle(SIGUSR2);

	sigset_t blockset;
	sigemptyset(&blockset);
	sigaddset(&blockset, SIGPIPE);
	::sigprocmask(SIG_BLOCK, &blockset, NULL);
#endif

	// initialize signal handler
	sighandler.initialize();

	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();

	// set default logging tag
	ibrcommon::Logger::setDefaultTag("DTNEngine");

	// enable ring-buffer
	ibrcommon::Logger::enableBuffer(200);

	// enable timestamps in logging if requested
	if (conf.getLogger().display_timestamps())
	{
		logopts = (~(ibrcommon::Logger::LOG_DATETIME) & logopts) | ibrcommon::Logger::LOG_TIMESTAMP;
	}

	// init syslog
	ibrcommon::Logger::enableAsync(); // enable asynchronous logging feature (thread-safe)
	ibrcommon::Logger::enableSyslog("ibrdtn-daemon", LOG_PID, LOG_DAEMON, logsys);

	if (!conf.getDebug().quiet())
	{
		if (conf.getLogger().verbose()) {
			// add logging to the cout
			ibrcommon::Logger::addStream(std::cout, logstd | ibrcommon::Logger::LOGGER_NOTICE, logopts);
		} else {
			// add logging to the cout
			ibrcommon::Logger::addStream(std::cout, logstd, logopts);
		}

		// add logging to the cerr
		ibrcommon::Logger::addStream(std::cerr, logerr, logopts);
	}

	// activate debugging
	if (conf.getDebug().enabled() && !conf.getDebug().quiet())
	{
		// init logger
		ibrcommon::Logger::setVerbosity(conf.getDebug().level());

		ibrcommon::Logger::addStream(std::cout, ibrcommon::Logger::LOGGER_DEBUG, logopts);

		_debug = true;
	}

	// load the configuration file
	conf.load(true);

	try {
		const ibrcommon::File &lf = conf.getLogger().getLogfile();
		ibrcommon::Logger::setLogfile(lf, ibrcommon::Logger::LOGGER_ALL ^ ibrcommon::Logger::LOGGER_DEBUG, logopts);
	} catch (const dtn::daemon::Configuration::ParameterNotSetException&) { };

	// initialize the daemon up to runlevel "Routing Extensions"
	_dtnd.init(dtn::daemon::RUNLEVEL_ROUTING_EXTENSIONS);

#ifdef HAVE_LIBDAEMON
	if (conf.getDaemon().daemonize())
	{
		/* Send OK to parent process */
		daemon_retval_send(0);
		daemon_log(LOG_INFO, "Sucessfully started");
	}
#endif

	ibrcommon::MutexLock l(_shutdown_cond);
	while (!_shutdown) _shutdown_cond.wait();

	_dtnd.init(dtn::daemon::RUNLEVEL_ZERO);

	// stop the asynchronous logger
	ibrcommon::Logger::stop();

	return 0;
}

static char* __daemon_pidfile__ = NULL;

static const char* __daemon_pid_file_proc__(void) {
	return __daemon_pidfile__;
}

int main(int argc, char *argv[])
{
	// create a configuration
	dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();

	// load parameter into the configuration
	conf.params(argc, argv);

	// Processing timing parameters and random seed

	// If argument 'seed' is defined, let us initalize the random
	// generator with this value
	long seed = conf.getDaemon().seed();
	if (seed != -1) {
	  std::cout << "Setting random seed: " << seed << "\n";
	  srandom(seed);
	}
	
	// If argument 'start_time' is defined, let us wait until the
	// specified time before doing anything else (nota: 'start_time'
	// should be an EPOCH value in ms)
	long start_time = conf.getDaemon().start_time();
	if (start_time != -1) {
	  struct timeval now;
	  ::gettimeofday(&now, NULL);
	  long delay = start_time - (now.tv_sec * 1000);
	  if (delay > 0) {
	    std::cout << "Start deferred by " << delay << " ms\n";
	    ibrcommon::Thread::sleep(delay);
	  }
	}

	// If argument 'stop_time' is defined, let us make sure this process
	// stops at the specified time (nota: 'stop_time' should be an EPOCH
	// value in ms)
	long stop_time = conf.getDaemon().stop_time();
	if (stop_time != -1) {
	  struct timeval now;
	  ::gettimeofday(&now, NULL);
	  long delay = stop_time - (now.tv_sec * 1000);
	  if (delay > 0) {
	    std::cout << "Will terminate in " << delay << " ms\n";
	    TerminationThread *t_thread = new TerminationThread(delay, func_sighandler);
	    t_thread->start();
	  }
	}

#ifdef HAVE_LIBDAEMON
	if (conf.getDaemon().daemonize())
	{
		int ret = 0;
		pid_t pid;

#ifdef HAVE_DAEMON_RESET_SIGS
		/* Reset signal handlers */
		if (daemon_reset_sigs(-1) < 0) {
			daemon_log(LOG_ERR, "Failed to reset all signal handlers: %s", strerror(errno));
			return 1;
		}

		/* Unblock signals */
		if (daemon_unblock_sigs(-1) < 0) {
			daemon_log(LOG_ERR, "Failed to unblock all signals: %s", strerror(errno));
			return 1;
		}
#endif

		/* Set identification string for the daemon for both syslog and PID file */
		daemon_pid_file_ident = daemon_log_ident = daemon_ident_from_argv0(argv[0]);

		/* set the pid file path */
		try {
			std::string p = conf.getDaemon().getPidFile().getPath();
			__daemon_pidfile__ = new char[p.length() + 1];
			::strcpy(__daemon_pidfile__, p.c_str());
			daemon_pid_file_proc = __daemon_pid_file_proc__;
		} catch (const dtn::daemon::Configuration::ParameterNotSetException&) { };

		/* Check if we are called with -k parameter */
		if (conf.getDaemon().kill_daemon())
		{
			int ret;

			/* Kill daemon with SIGTERM */

			/* Check if the new function daemon_pid_file_kill_wait() is available, if it is, use it. */
			if ((ret = daemon_pid_file_kill_wait(SIGTERM, 5)) < 0)
				daemon_log(LOG_WARNING, "Failed to kill daemon: %s", strerror(errno));

			return ret < 0 ? 1 : 0;
		}

		/* Check that the daemon is not rung twice a the same time */
		if ((pid = daemon_pid_file_is_running()) >= 0) {
			daemon_log(LOG_ERR, "Daemon already running on PID file %u", pid);
			return 1;
		}

		/* Prepare for return value passing from the initialization procedure of the daemon process */
		if (daemon_retval_init() < 0) {
			daemon_log(LOG_ERR, "Failed to create pipe.");
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
				daemon_log(LOG_ERR, "Could not recieve return value from daemon process: %s", strerror(errno));
				return 255;
			}

			//daemon_log(ret != 0 ? LOG_ERR : LOG_INFO, "Daemon returned %i as return value.", ret);
			return ret;

		} else { /* The daemon */
			/* Close FDs */
			if (daemon_close_all(-1) < 0) {
				daemon_log(LOG_ERR, "Failed to close all file descriptors: %s", strerror(errno));

				/* Send the error condition to the parent process */
				daemon_retval_send(1);
				goto finish;
			}

			/* Create the PID file */
			if (daemon_pid_file_create() < 0) {
				daemon_log(LOG_ERR, "Could not create PID file (%s).", strerror(errno));
				daemon_retval_send(2);
				goto finish;
			}

			/* Initialize signal handling */
			if (daemon_signal_init(SIGINT, SIGTERM, SIGQUIT, SIGHUP, SIGUSR1, SIGUSR2, 0) < 0) {
				daemon_log(LOG_ERR, "Could not register signal handlers (%s).", strerror(errno));
				daemon_retval_send(3);
				goto finish;
			}

			ret = __daemon_run();

	finish:
			/* Do a cleanup */
			daemon_log(LOG_INFO, "Exiting...");
			daemon_retval_send(255);
			daemon_signal_done();
			daemon_pid_file_remove();
		}

		return ret;
	} else {
#endif
		// run the daemon
		return __daemon_run();
#ifdef HAVE_LIBDAEMON
	}
#endif
}
