/*
 * dtnoutbox.cpp
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 * 			   David Goltzsche <goltzsch@ibr.cs.tu-bs.de>
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
#include <ibrcommon/data/BLOB.h>
#include <ibrcommon/data/File.h>
#include <ibrcommon/Logger.h>

#include "io/TarUtils.h"
#include "io/ObservedFile.h"

#ifdef HAVE_LIBTFFS
#include "io/FatImageReader.h"
#endif

#include <stdlib.h>
#include <iostream>
#include <map>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <regex.h>
#include <getopt.h>

typedef std::list<io::ObservedFile> filelist;
typedef std::set<io::ObservedFile> fileset;
typedef std::set<io::FileHash> hashlist;

// set this variable to false to stop the app
bool _running = true;

//global wait conditional
ibrcommon::Conditional _wait_cond;
bool _wait_abort = false;

const std::string TAG = "dtnoutbox";

class config {
public:
	config()
	 : interval(5000), rounds(3), path("/"), regex_str("^\\."),
		bundle_group(false), invert(false), quiet(false), verbose(false), fat(false), enabled(true)
	{}

	//global conf values
	std::string name;
	std::string outbox;
	std::string destination;

	//optional paramters
	std::string workdir;
	std::size_t interval;
	std::size_t rounds;
	std::string path;
	std::string regex_str;
	regex_t regex;

	int bundle_group;
	int invert;
	int quiet;
	int verbose;
	int fat;
	int enabled;
};
typedef struct config config_t;

struct option long_options[] =
{
	{"destination",  required_argument, 0, 'd'},
	{"help", no_argument, 0, 'h'},
	{"group", no_argument, 0, 'g'},
	{"workdir", required_argument, 0, 'w'},
	{"interval", required_argument, 0, 'i'},
	{"rounds", required_argument, 0, 'r'},
	{"path", required_argument, 0, 'p'},
	{"regex", required_argument, 0, 'R'},
	{"quiet", no_argument, 0, 'q'},
	{"verbose", no_argument, 0, 'v'},
	{0, 0, 0, 0}
};

void init_logger(config_t &conf)
{

	// logging options
	const unsigned char logopts = 0;

	// error filter
	const unsigned char logerr = ibrcommon::Logger::LOGGER_ERR | ibrcommon::Logger::LOGGER_CRIT;

	// logging filter, everything but debug, err and crit
	const unsigned char logstd = ibrcommon::Logger::LOGGER_ALL ^ (ibrcommon::Logger::LOGGER_DEBUG | logerr);

	// syslog filter, everything but DEBUG and NOTICE
	const unsigned char logsys = ibrcommon::Logger::LOGGER_ALL ^ (ibrcommon::Logger::LOGGER_DEBUG | ibrcommon::Logger::LOGGER_NOTICE);

	const unsigned char logall = ibrcommon::Logger::LOGGER_ALL;

	//log notice messages to cout, if quiet not configured
	if(!conf.quiet)
	{
		ibrcommon::Logger::addStream(std::cout, logsys, logopts);
		if(conf.verbose)
		{
			ibrcommon::Logger::addStream(std::cout, logall, logopts);
		}
	}

	// add logging to the cerr
	ibrcommon::Logger::addStream(std::cerr, logerr, logopts);
}

void print_help()
{
	std::cout << "-- dtnoutbox (IBR-DTN) --" << std::endl;
	std::cout << "Syntax: dtnoutbox [options] <name> <outbox> <destination>" << std::endl;
	std::cout << " <name>           The application name" << std::endl;
#ifdef HAVE_LIBTFFS
	std::cout << " <outbox>         Location of outgoing files, directory or vfat-image" << std::endl;
#else
	std::cout << " <outbox>         Directory of outgoing files" << std::endl;
#endif
	std::cout << " <destination>    The destination EID for all outgoing files" << std::endl << std::endl;
	std::cout << "* optional parameters *" << std::endl;
	std::cout << " -h|--help        Display this text" << std::endl;
	std::cout << " -g|--group       Receiver is a destination group" << std::endl;
	std::cout << " -w|--workdir <dir>" << std::endl;
	std::cout << "                  Temporary work directory" << std::endl;
	std::cout << " -i|--interval <milliseconds>" << std::endl;
	std::cout << "                  Interval in milliseconds, in which <outbox> is scanned" << std::endl;
	std::cout << "                  for new/changed files. default: 5000" << std::endl;
	std::cout << " -r|--rounds <n>  Number of rounds of intervals, after which a unchanged" << std::endl;
	std::cout << "                  file is considered as written. default: 3" << std::endl;
#ifdef HAVE_LIBTFFS
	std::cout << " -p|--path <path> Path of outbox within vfat-image. default: /" << std::endl;
#endif
	std::cout << " -R|--regex <regex>" << std::endl;
	std::cout << "                  All files in <outbox> matching this regular expression" << std::endl;
	std::cout << "                  will be ignored. default: ^\\." << std::endl;
	std::cout << " -I|--invert      Invert the regular expression defined with -R"<< std::endl;
	std::cout << " -q|--quiet       Only print error messages" << std::endl;
	std::cout << " -v|--verbose     print more verbose info messages, only works without -q" << std::endl;

	_running = false; //stop this app, after printing help

}

void read_configuration(int argc, char** argv, config_t &conf)
{
	while(1)
	{
		/* getopt_long stores the option index here. */
		int option_index = 0;
		int c = getopt_long (argc, argv, "hw:i:r:p:R:qvIg",
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
		case 'g':
			conf.bundle_group = true;
			break;
		case 'w':
			conf.workdir = std::string(optarg);
			break;
		case 'i':
			conf.interval = atoi(optarg);
			break;
		case 'r':
			conf.rounds = atoi(optarg);
			break;
		case 'p':
			conf.path = std::string(optarg);
			break;
		case 'R':
			conf.regex_str = std::string(optarg);
			break;
		case 'q':
			conf.quiet = true;
			break;
		case 'v':
			conf.verbose = true;
			break;
		case 'I':
			conf.invert = true;
			break;
		case '?':
			break;
		default:
			abort();
			break;
		}
	}

	// print help if not enough parameters are set
	if ((argc - optind) < 3)
	{
		print_help();
		exit(EXIT_FAILURE);
	}

	conf.name = std::string(argv[optind]);
	conf.destination = std::string(argv[optind+2]);
	conf.outbox = std::string(argv[optind+1]);

	//compile regex, if set
	if (conf.regex_str.length() > 0 && regcomp(&conf.regex,conf.regex_str.c_str(),0))
	{
		IBRCOMMON_LOGGER_TAG(TAG,error) << "ERROR: invalid regex: " << optarg << IBRCOMMON_LOGGER_ENDL;
		exit(-1);
	}


	//check outbox path for trailing slash
	if (conf.outbox.at(conf.outbox.length()-1) == '/')
		conf.outbox = conf.outbox.substr(0,conf.outbox.length() -1);
}

void sighandler_func(int signal)
{
	IBRCOMMON_LOGGER_TAG(TAG,notice) << "got signal " << signal << IBRCOMMON_LOGGER_ENDL;
	switch (signal)
	{
	case SIGTERM:
	case SIGINT:
	{
		//stop waiting and stop running, on SIGINT or SIGTERM
		ibrcommon::MutexLock l(_wait_cond);
		_running = false;
		_wait_cond.signal(true);
		break;
	}
#ifndef __WIN32__
	case SIGUSR1:
	{
		//stop waiting on SIGUSR1 -> "quickscan"
		ibrcommon::MutexLock l(_wait_cond);
		_wait_abort = true;
		_wait_cond.signal(true);
		break;
	}
#endif
	default:
		break;
	}
}

/*
 * main application method
 */
int main( int argc, char** argv )
{
	// catch process signals
	ibrcommon::SignalHandler sighandler(sighandler_func);
	sighandler.handle(SIGINT);
	sighandler.handle(SIGTERM);
#ifndef __WIN32__
	sighandler.handle(SIGUSR1);
#endif

	// configration object
	config_t conf;

	// read the configuration
	read_configuration(argc, argv, conf);

	init_logger(conf);

	// initialize sighandler after possible exit call
	sighandler.initialize();

	// init working directory
	if (conf.workdir.length() > 0)
	{
		ibrcommon::File blob_path(conf.workdir);

		if (blob_path.exists())
		{
			ibrcommon::BLOB::changeProvider(new ibrcommon::FileBLOBProvider(blob_path), true);
		}
	}

	// backoff for reconnect
	unsigned int backoff = 2;

	ibrcommon::File outbox_file(conf.outbox);

	// create new file lists
	fileset new_files, prev_files, deleted_files, files_to_send;
	filelist observed_files;
	hashlist sent_hashes;

	// observed root file
	io::ObservedFile root(ibrcommon::File("/"));

#ifdef HAVE_LIBTFFS
	io::FatImageReader *imagereader = NULL;
#endif

	if (outbox_file.exists() && !outbox_file.isDirectory())
	{
#ifdef HAVE_LIBTFFS
		conf.fat = true;
		imagereader = new io::FatImageReader(conf.outbox);
		const io::FATFile fat_root(*imagereader, conf.path);
		root = io::ObservedFile(fat_root);
#else
		IBRCOMMON_LOGGER_TAG(TAG,error) << "ERROR: image-file provided, but this tool has been compiled without libtffs support!" << IBRCOMMON_LOGGER_ENDL;
		return -1;
#endif
	}
	else
	{
		if (!outbox_file.exists()) {
			ibrcommon::File::createDirectory(outbox_file);
		}
		root = io::ObservedFile(outbox_file);
	}

	IBRCOMMON_LOGGER_TAG(TAG,info) << "-- dtnoutbox --" << IBRCOMMON_LOGGER_ENDL;

	// loop, if no stop if requested
	while (_running)
	{
		try
		{
			// Create a stream to the server using TCP.
			ibrcommon::vaddress addr("localhost", 4550);
			ibrcommon::socketstream conn(new ibrcommon::tcpsocket(addr));

			// Initiate a client for synchronous receiving
			dtn::api::Client client(conf.name, conn, dtn::api::Client::MODE_SENDONLY);

			// Connect to the server. Actually, this function initiate the
			// stream protocol by starting the thread and sending the contact header.
			client.connect();

			// reset backoff if connected
			backoff = 2;

			// check the connection
			while (_running)
			{
				// get all files
				fileset current_files;
				root.findFiles(current_files);

				// determine deleted files
				fileset deleted_files;
				std::set_difference(prev_files.begin(), prev_files.end(), current_files.begin(), current_files.end(), std::inserter(deleted_files, deleted_files.begin()));

				// remove deleted files from observation
				for (fileset::const_iterator iter = deleted_files.begin(); iter != deleted_files.end(); ++iter)
				{
					const io::ObservedFile &deletedFile = (*iter);

					// remove references in the sent_hashes
					for (hashlist::iterator hash_it = sent_hashes.begin(); hash_it != sent_hashes.end(); /* blank */) {
						if ((*hash_it).getPath() == deletedFile.getFile().getPath()) {
							sent_hashes.erase(hash_it++);
						} else {
							++hash_it;
						}
					}

					// remove from observed files
					observed_files.remove(deletedFile);

					// output
					IBRCOMMON_LOGGER_TAG(TAG,info) << "file removed: " << deletedFile.getFile().getBasename() << IBRCOMMON_LOGGER_ENDL;
				}

				// determine new files
				fileset new_files;
				std::set_difference(current_files.begin(), current_files.end(), prev_files.begin(), prev_files.end(), std::inserter(new_files, new_files.begin()));

				// add new files to observation
				for (fileset::const_iterator iter = new_files.begin(); iter != new_files.end(); ++iter)
				{
					const io::ObservedFile &of = (*iter);

					int reg_ret = regexec(&conf.regex, of.getFile().getBasename().c_str(), 0, NULL, 0);
					if (!reg_ret && !conf.invert)
						continue;
					if (reg_ret && conf.invert)
						continue;

					// print error message, if regex error occurs
					if (reg_ret && reg_ret != REG_NOMATCH)
					{
							char msgbuf[100];
							regerror(reg_ret,&conf.regex,msgbuf,sizeof(msgbuf));
							IBRCOMMON_LOGGER_TAG(TAG,info) << "ERROR: regex match failed : " << std::string(msgbuf) << IBRCOMMON_LOGGER_ENDL;
					}

					// add new file to the observed set
					observed_files.push_back(of);

					// log output
					IBRCOMMON_LOGGER_TAG(TAG, info) << "file found: " << of.getFile().getBasename() << IBRCOMMON_LOGGER_ENDL;
				}

				// store current files for the next round
				prev_files.clear();
				prev_files.insert(current_files.begin(), current_files.end());

				IBRCOMMON_LOGGER_TAG(TAG, notice)
						<< "file statistics: "
						<< observed_files.size() << " observed, "
						<< deleted_files.size() << " deleted, "
						<< new_files.size() << " new"
						<< IBRCOMMON_LOGGER_ENDL;

				// find files to send, create std::list
				files_to_send.clear();


				IBRCOMMON_LOGGER_TAG(TAG, notice) << "updating observed files:" << IBRCOMMON_LOGGER_ENDL;
				for (filelist::iterator iter = observed_files.begin(); iter != observed_files.end(); ++iter)
				{
					io::ObservedFile &of = (*iter);

					// tick and update all files
					of.update();

					if (of.getStableCounter() > conf.rounds)
					{
						if (sent_hashes.find(of.getHash()) == sent_hashes.end())
						{
							sent_hashes.insert(of.getHash());
							files_to_send.insert(*iter);
						}
					}

					IBRCOMMON_LOGGER_TAG(TAG, notice)
						<< "\t"
						<< of.getFile().getBasename() << ": "
						<< of.getStableCounter()
						<< IBRCOMMON_LOGGER_ENDL;
				}

				if (!files_to_send.empty())
				{
					std::stringstream ss;
					for (fileset::const_iterator it = files_to_send.begin(); it != files_to_send.end(); ++it) {
						ss << (*it).getFile().getBasename() << " ";
					}
					IBRCOMMON_LOGGER_TAG("dtnoutbox",info) << "files sent: " << ss.str() << IBRCOMMON_LOGGER_ENDL;

					try {
						// create a blob
						ibrcommon::BLOB::Reference blob = ibrcommon::BLOB::create();

						// write files into BLOB while it is locked
						{
							ibrcommon::BLOB::iostream stream = blob.iostream();
							io::TarUtils::write(*stream, root, files_to_send);
						}

						// create a new bundle
						dtn::data::EID destination = EID(conf.destination);

						// create a new bundle
						dtn::data::Bundle b;

						// set destination
						b.destination = destination;

						// add payload block using the blob
						b.push_back(blob);

						// set destination address to non-singleton, if configured
						if (conf.bundle_group)
							b.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, false);

						// send the bundle
						client << b;
						client.flush();
					} catch (const ibrcommon::IOException &e) {
						IBRCOMMON_LOGGER_TAG(TAG,error) << "send failed: " << e.what() << IBRCOMMON_LOGGER_ENDL;
					}
				}

				// wait defined seconds
				ibrcommon::MutexLock l(_wait_cond);
				IBRCOMMON_LOGGER_TAG(TAG, notice) << conf.interval <<" ms wait" << IBRCOMMON_LOGGER_ENDL;
				while (!_wait_abort && _running) {
					_wait_cond.wait(conf.interval);
				}
				_wait_abort = false;
			}

			// clean up regex
			regfree(&conf.regex);

			// close the client connection
			client.close();

			// close the connection
			conn.close();

		}
		catch (const ibrcommon::socket_exception&)
		{
			if (_running)
			{
				IBRCOMMON_LOGGER_TAG(TAG,error) << "Connection to bundle daemon failed. Retry in " << backoff << " seconds." << IBRCOMMON_LOGGER_ENDL;
				ibrcommon::Thread::sleep(backoff * 1000);

				// if backoff < 10 minutes
				if (backoff < 600)
				{
					// set a new backoff
					backoff = backoff * 2;
				}
			}
		}
		catch (const ibrcommon::IOException&)
		{
			if (_running)
			{
				IBRCOMMON_LOGGER_TAG(TAG,error) << "Connection to bundle daemon failed. Retry in " << backoff << " seconds." << IBRCOMMON_LOGGER_ENDL;
				ibrcommon::Thread::sleep(backoff * 1000);

				// if backoff < 10 minutes
				if (backoff < 600)
				{
					// set a new backoff
					backoff = backoff * 2;
				}
			}
		}
		catch (const std::exception&) { };
	}

	// clear observed files
	observed_files.clear();

#ifdef HAVE_LIBTFFS
	// clean-up
	if (imagereader != NULL) delete imagereader;
#endif

	return (EXIT_SUCCESS);
}
