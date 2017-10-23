/*
 * Configuration.cpp
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
#include "net/DiscoveryBeacon.h"
#include "net/DiscoveryBeacon.h"
#include "core/Node.h"

#include <ibrdtn/ibrdtn.h>
#include <ibrdtn/utils/Utils.h>
#include <ibrdtn/utils/Clock.h>

#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/link/LinkManager.h>

#ifdef __WIN32__
#include <ibrcommon/link/Win32LinkManager.h>
#include <windows.h>

#endif

#include <getopt.h>
#include <unistd.h>

#ifdef __DEVELOPMENT_ASSERTIONS__
#include <cassert>
#endif

using namespace dtn::net;
using namespace dtn::core;
using namespace dtn::utils;
using namespace ibrcommon;

namespace dtn
{
	namespace daemon
	{
		Configuration::NetConfig::NetConfig(const std::string &n, NetType t)
		 : name(n), type(t), iface(ibrcommon::vinterface::ANY), mtu(0), port(0)
		{
		}

		Configuration::NetConfig::~NetConfig()
		{
		}

		std::string Configuration::version() const
		{
			std::stringstream ss;
			ss << PACKAGE_VERSION;
		#ifdef BUILD_NUMBER
			ss << " (build " << BUILD_NUMBER << ")";
		#endif

			return ss.str();
		}

		Configuration::Configuration()
		 : _filename("config.ini"), _doapi(true)
		{
		}

		Configuration::~Configuration()
		{}

		Configuration::Discovery::Discovery()
		 : _enabled(true), _interval(5), _announce(true), _short(false), _version(2), _crosslayer(false) {}

		Configuration::Debug::Debug()
		 : _enabled(false), _quiet(false), _level(0), _profiling(false) {}

		Configuration::Logger::Logger()
		 : _quiet(false), _options(0), _timestamps(false), _verbose(false) {}

		Configuration::Network::Network()
		 : _routing("default"), _forwarding(true), _accept_nonsingleton(true), _prefer_direct(true), _tcp_nodelay(true), _tcp_chunksize(4096), _tcp_idle_timeout(0), _keepalive_timeout(60), _default_net("lo"), _use_default_net(false), _auto_connect(0), _fragmentation(false), _scheduling(false), _managed_connectivity(false), _link_request_interval(5000)
		{}

		Configuration::Security::Security()
		 : _enabled(false), _tlsEnabled(false), _tlsRequired(false), _tlsOptionalOnBadClock(false), _level(SECURITY_LEVEL_NONE), _disableEncryption(false), _generate_dh_params(false)
		{}

		Configuration::Daemon::Daemon()
		 : _daemonize(false), _kill(false), _threads(0)
		{}

		Configuration::TimeSync::TimeSync()
		 : _reference(true), _sync(false), _discovery(false), _sigma(1.001f), _psi(0.8f), _sync_level(0.10f)
		{}

		Configuration::DHT::DHT()
		 : _enabled(true), _port(0), _dnsbootstrapping(true), _ipv4(true), _ipv6(true), _blacklist(true), _selfannounce(true),
			_minRating(1), _allowNeighbourToAnnounceMe(true), _allowNeighbourAnnouncement(true),
			_ignoreDHTNeighbourInformations(false)
		{}

		Configuration::P2P::P2P()
		 : _ctrl_path(""), _enabled(false)
		{}

		Configuration::EMail::EMail()
		 : _smtpPort(25), _smtpUseTLS(false), _smtpUseSSL(false), _smtpNeedAuth(false), _smtpInterval(60), _smtpConnectionTimeout(-1), _smtpKeepAliveTimeout(30),
		   _imapPort(143), _imapUseTLS(false), _imapUseSSL(false), _imapInterval(60), _imapConnectionTimeout(-1), _imapPurgeMail(false),
		   _availableTime(1800), _returningMailsCheck(3)
		{}

		Configuration::Discovery::~Discovery() {}
		Configuration::Debug::~Debug() {}
		Configuration::Logger::~Logger() {}
		Configuration::Network::~Network() {}
		Configuration::Daemon::~Daemon() {}
		Configuration::TimeSync::~TimeSync() {}
		Configuration::DHT::~DHT() {}
		Configuration::P2P::~P2P() {}
		Configuration::EMail::~EMail() {}

		const Configuration::Discovery& Configuration::getDiscovery() const
		{
			return _disco;
		}

		const Configuration::Debug& Configuration::getDebug() const
		{
			return _debug;
		}

		const Configuration::Logger& Configuration::getLogger() const
		{
			return _logger;
		}

		const Configuration::Network& Configuration::getNetwork() const
		{
			return _network;
		}

		const Configuration::Security& Configuration::getSecurity() const
		{
			return _security;
		}

		const Configuration::Daemon& Configuration::getDaemon() const
		{
			return _daemon;
		}

		const Configuration::TimeSync& Configuration::getTimeSync() const
		{
			return _timesync;
		}

		const Configuration::DHT& Configuration::getDHT() const
		{
			return _dht;
		}

		const Configuration::P2P& Configuration::getP2P() const
		{
			return _p2p;
		}

		const Configuration::EMail& Configuration::getEMail() const
		{
			return _email;
		}

		Configuration& Configuration::getInstance(bool reset)
		{
			static Configuration conf;

			// reset configuration
			if (reset) conf = Configuration();

			return conf;
		}

		void Configuration::params(int argc, char *argv[])
		{
			int c;
			int doapi = _doapi;
			int disco = _disco._enabled;
			int timestamp = _logger._timestamps;
			int showversion = 0;

			// set number of threads to the number of available cpus
			_daemon._threads = static_cast<dtn::data::Size>(ibrcommon::Thread::getNumberOfProcessors());

			while (1)
			{
				static struct option long_options[] =
				{
						/* These options set a flag. */
						{"noapi", no_argument, &doapi, 0},
						{"nodiscovery", no_argument, &disco, 0},
						{"timestamp", no_argument, &timestamp, 1},
						{"version", no_argument, &showversion, 1},

						/* These options don't set a flag. We distinguish them by their indices. */
						{"help", no_argument, 0, 'h'},
#ifdef HAVE_LIBDAEMON
						{"daemon", no_argument, 0, 'D'},
						{"kill", no_argument, 0, 'k'},
						{"pidfile", required_argument, 0, 'p'},
#endif

						{"quiet", no_argument, 0, 'q'},
						{"interface", required_argument, 0, 'i'},
						{"configuration", required_argument, 0, 'c'},
						{"debug", required_argument, 0, 'd'},
#ifdef __WIN32__
						{"interfaces", no_argument, 0, 'I'},
#endif
						{0, 0, 0, 0}
				};

				/* getopt_long stores the option index here. */
				int option_index = 0;

#ifdef HAVE_LIBDAEMON
				c = getopt_long (argc, argv, "qhDkp:vi:c:d:t:",
						long_options, &option_index);
#else
				c = getopt_long (argc, argv, "qhvi:c:d:t:",
						long_options, &option_index);
#endif

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
					std::cout << "IBR-DTN version: " << version() << std::endl;
					std::cout << "Syntax: dtnd [options]"  << std::endl;
					std::cout << " -h|--help       display this text" << std::endl;
					std::cout << " -c <file>       set a configuration file" << std::endl;
#ifdef HAVE_LIBDAEMON
					std::cout << " -D              daemonize the process" << std::endl;
					std::cout << " -k              stop the running daemon" << std::endl;
					std::cout << " -p <file>       store the pid in this pidfile" << std::endl;
#endif
					std::cout << " -i <interface>  interface to bind on (e.g. eth0)" << std::endl;
					std::cout << " -d <level>      enable debugging and set a verbose level" << std::endl;
					std::cout << " -q              enables the quiet mode (no logging to the console)" << std::endl;
					std::cout << " -t <threads>    specify a number of threads for parallel event processing" << std::endl;
					std::cout << " -v              be verbose - show NOTICE log messages" << std::endl;
					std::cout << " --version       show version and exit" << std::endl;
					std::cout << " --noapi         disable API module" << std::endl;
					std::cout << " --nodiscovery   disable discovery module" << std::endl;
					std::cout << " --timestamp     enables timestamps for logging instead of datetime values" << std::endl;
#ifdef __WIN32__
					std::cout << " --interfaces    list all available interfaces" << std::endl;
#endif
					exit(0);
					break;

				case 'v':
					_logger._verbose = true;
					break;

				case 'q':
					_debug._quiet = true;
					break;

				case 'c':
					_filename = optarg;
					break;

				case 'i':
					_network._default_net = ibrcommon::vinterface(optarg);
					_network._use_default_net = true;
					break;

				case 'd':
					_debug._enabled = true;
					_debug._level = atoi(optarg);
					break;

				case 'D':
					_daemon._daemonize = true;
					_debug._quiet = true;
					break;

				case 'k':
					_daemon._daemonize = true;
					_daemon._kill = true;
					break;

				case 'p':
					_daemon._pidfile = std::string(optarg);
					break;

				case 't':
					_daemon._threads = atoi(optarg);
					break;

#ifdef __WIN32__
				case 'I':
				{
					ibrcommon::LinkManager &lm = ibrcommon::LinkManager::getInstance();
					try {
						ibrcommon::Win32LinkManager &wlm = dynamic_cast<ibrcommon::Win32LinkManager&>(lm);

						std::set<ibrcommon::vinterface> ifs = wlm.getInterfaces();

						std::cout << "Available interfaces:" << std::endl;

						for (std::set<ibrcommon::vinterface>::const_iterator it = ifs.begin(); it != ifs.end(); ++it)
						{
							const ibrcommon::vinterface &iface = (*it);
							std::cout << iface.toString() << '\t';
							std::wcout << iface.getFriendlyName() << std::endl;
						}
					} catch (const std::bad_cast&) {
					}
					exit(0);
					break;
				}
#endif

				case '?':
					/* getopt_long already printed an error message. */
					break;

				default:
					abort();
					break;
				}
			}

			if (showversion == 1) {
				std::cout << "IBR-DTN version: " << version() << std::endl;
				exit(0);
			}

			_doapi = doapi;
			_disco._enabled = disco;
			_logger._timestamps = timestamp;
		}

		void Configuration::load(bool quiet)
		{
			load(_filename, quiet);
		}

		void Configuration::load(const std::string &filename, bool quiet)
		{
			try {
				// load main configuration
				_conf = ibrcommon::ConfigFile(filename);
				_filename = filename;

				if (!quiet) IBRCOMMON_LOGGER_TAG("Configuration", info) << "Configuration: " << filename << IBRCOMMON_LOGGER_ENDL;
			} catch (const ibrcommon::ConfigFile::file_not_found&) {
				if (!quiet) IBRCOMMON_LOGGER_TAG("Configuration", info) << "Using default settings. Call with --help for options." << IBRCOMMON_LOGGER_ENDL;
				_conf = ConfigFile();

				// set the default user to nobody
				_conf.add<std::string>("user", "nobody");
			}

			// load all configuration extensions
			_disco.load(_conf);
			_debug.load(_conf);
			_logger.load(_conf);
			_network.load(_conf);
			_security.load(_conf);
			_timesync.load(_conf);
			_dht.load(_conf);
			_p2p.load(_conf);
			_email.load(_conf);
		}

		void Configuration::Discovery::load(const ibrcommon::ConfigFile &conf)
		{
			_interval = conf.read<unsigned int>("discovery_interval", 5);
			_announce = (conf.read<int>("discovery_announce", 1) == 1);
			_short = (conf.read<int>("discovery_short", 0) == 1);
			_version = conf.read<int>("discovery_version", 2);
			_crosslayer = (conf.read<std::string>("discovery_crosslayer", "no") == "yes");
		}

		void Configuration::Logger::load(const ibrcommon::ConfigFile &conf)
		{
			try {
				_logfile = conf.read<std::string>("logfile");
			} catch (const ibrcommon::ConfigFile::key_not_found&) {
			}
		}

		void Configuration::Debug::load(const ibrcommon::ConfigFile &conf)
		{
			try {
				_profiling = (conf.read<std::string>("profiling") == "yes");
			} catch (const ibrcommon::ConfigFile::key_not_found&) { };
		}

		void Configuration::Daemon::load(const ibrcommon::ConfigFile&)
		{
		}

		void Configuration::TimeSync::load(const ibrcommon::ConfigFile &conf)
		{
			try {
				_reference = (conf.read<std::string>("time_reference") == "yes");
			} catch (const ibrcommon::ConfigFile::key_not_found&) { };

			try {
				_sync = (conf.read<std::string>("time_synchronize") == "yes");
			} catch (const ibrcommon::ConfigFile::key_not_found&) { };

			try {
				_discovery = (conf.read<std::string>("time_discovery_announcements") == "yes");
			} catch (const ibrcommon::ConfigFile::key_not_found&) { };

			_sigma = conf.read<float>("time_sigma", 1.001f);
			_psi = conf.read<float>("time_psi", 0.9f);
			_sync_level = conf.read<float>("time_sync_level", 0.15f);

			// enable the clock modify feature
			dtn::utils::Clock::setModifyClock(conf.read<std::string>("time_set_clock", "no") == "yes");
		}

		void Configuration::DHT::load(const ibrcommon::ConfigFile &conf)
		{
			_enabled = (conf.read<std::string> ("dht_enabled", "no") == "yes");
			_port = conf.read<int> ("dht_port", 9999);
			_id = conf.read<std::string> ("dht_id", "");
			_blacklist = (conf.read<std::string> ("dht_blacklist", "yes") == "yes");
			_selfannounce = (conf.read<std::string> ("dht_self_announce", "yes") == "yes");
			_dnsbootstrapping = (conf.read<std::string> ("dht_bootstrapping", "yes") == "yes");
			std::string list = conf.read<std::string> ("dht_bootstrapping_domains", "");
			_bootstrappingdomains = dtn::utils::Utils::tokenize(" ", list);
			list = conf.read<std::string> ("dht_bootstrapping_ips", "");
			_bootstrappingips = dtn::utils::Utils::tokenize(";", list);
			_ipv4bind = conf.read<std::string> ("dht_bind_ipv4", "");
			_ipv6bind = conf.read<std::string> ("dht_bind_ipv6", "");
			_nodesFilePath = conf.read<std::string> ("dht_nodes_file", "");
			_ipv4 = (conf.read<std::string> ("dht_enable_ipv4", "yes") == "yes");
			_ipv6 = (conf.read<std::string> ("dht_enable_ipv6", "yes") == "yes");
			_minRating = conf.read<int> ("dht_min_rating", 1);
			_allowNeighbourToAnnounceMe = (conf.read<std::string> ("dht_allow_neighbours_to_announce_me", "yes") == "yes");
			_allowNeighbourAnnouncement = (conf.read<std::string> ("dht_allow_neighbour_announcement", "yes") == "yes");
			_ignoreDHTNeighbourInformations = (conf.read<std::string> ("dht_ignore_neighbour_informations", "no") == "yes");

			if (_minRating < 0)	_minRating = 0;
		}

		void Configuration::P2P::load(const ibrcommon::ConfigFile &conf)
		{
			try {
				_ctrl_path = conf.read<std::string>("p2p_ctrlpath");
				_enabled = true;
			} catch (const ibrcommon::ConfigFile::key_not_found&) {
				// do nothing here...
				_enabled = false;
			}
		}

		void Configuration::EMail::load(const ibrcommon::ConfigFile &conf)
		{
			std::string tmp;
			_address = conf.read<std::string> ("email_address", "root@localhost");
			_smtpServer = conf.read<std::string> ("email_smtp_server", "localhost");
			_smtpPort = conf.read<int> ("email_smtp_port", 25);
			_smtpUsername = conf.read<std::string> ("email_smtp_username", "root");
			_smtpPassword = conf.read<std::string> ("email_smtp_password", "");
			_smtpInterval = conf.read<size_t> ("email_smtp_submit_interval", 60);
			_smtpConnectionTimeout = conf.read<size_t> ("email_smtp_connection_timeout", -1);
			_smtpKeepAliveTimeout = conf.read<size_t> ("email_smtp_keep_alive", 30);
			_smtpNeedAuth = (conf.read<std::string> ("email_smtp_need_authentication", "no") == "yes");
			_smtpUseTLS = (conf.read<std::string> ("email_smtp_socket_type", "") == "tls");
			_smtpUseSSL = (conf.read<std::string> ("email_smtp_socket_type", "") == "ssl");
			_imapServer = conf.read<std::string> ("email_imap_server", "localhost");
			_imapPort = conf.read<int> ("email_imap_port", 143);
			_imapUsername = conf.read<std::string> ("email_imap_username", _smtpUsername);
			_imapPassword = conf.read<std::string> ("email_imap_password", _smtpPassword);
			tmp = conf.read<std::string> ("email_imap_folder", "");
			_imapFolder = dtn::utils::Utils::tokenize("/", tmp);
			_imapInterval = conf.read<size_t> ("email_imap_lookup_interval", 60);
			_imapConnectionTimeout = conf.read<size_t> ("email_imap_connection_timeout", -1);
			_imapUseTLS = (conf.read<std::string> ("email_imap_socket_type", "") == "tls");
			_imapUseSSL = (conf.read<std::string> ("email_imap_socket_type", "") == "ssl");
			_imapPurgeMail = (conf.read<std::string> ("email_imap_purge_mail", "no") == "yes");
			tmp = conf.read<std::string> ("email_certs_ca", "");
			_tlsCACerts = dtn::utils::Utils::tokenize(",", tmp);
			tmp = conf.read<std::string> ("email_certs_user", "");
			_tlsUserCerts = dtn::utils::Utils::tokenize(",", tmp);
			_availableTime = conf.read<size_t> ("email_node_available_time", 1800);
			_returningMailsCheck = conf.read<size_t> ("email_returning_mails_checks", 3);
		}

		bool Configuration::Debug::quiet() const
		{
			return _quiet;
		}

		bool Configuration::Debug::profiling() const
		{
			return _profiling;
		}

		bool Configuration::Debug::enabled() const
		{
			return _enabled;
		}

		int Configuration::Debug::level() const
		{
			return _level;
		}

		std::string Configuration::getNodename() const
		{
			try {
				return _conf.read<std::string>("local_uri");
			} catch (const ibrcommon::ConfigFile::key_not_found&) {
				std::vector<char> hostname_array(255);
				if ( gethostname(&hostname_array[0], hostname_array.size()) != 0 )
				{
#ifdef __WIN32__
					// read hostname from registry
					bool success = false;
					HKEY hKey = 0;
					DWORD dwType = REG_SZ;
					DWORD dwBufSize = hostname_array.size();

					const char* subkey = "System\\CurrentControlSet\\Control\\ComputerName\\ActiveComputerName";
					const char* win9x_subkey = "System\\CurrentControlSet\\Control\\ComputerName\\ComputerName";

					if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
					{
						if ( RegQueryValueEx(hKey, "ComputerName", NULL, &dwType, (BYTE*)&hostname_array[0], &dwBufSize) == ERROR_SUCCESS )
						{
							success = true;
						}

						RegCloseKey(hKey);
					}

					if (!success) {
						if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, win9x_subkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
						{
							if ( RegQueryValueEx(hKey, "ComputerName", NULL, &dwType, (BYTE*)&hostname_array[0], &dwBufSize) == ERROR_SUCCESS )
							{
								success = true;
							}

							RegCloseKey(hKey);
						}
					}

					if (!success) {
						// not hostname available
						return "dtn://local";
					}
#endif
				}

				return "dtn://" + std::string(&hostname_array[0]);
			}
			return "dtn://noname";
		}

		const std::list<Configuration::NetConfig>& Configuration::Network::getInterfaces() const
		{
			return _interfaces;
		}

		const std::set<ibrcommon::vaddress> Configuration::Discovery::address() const throw (ParameterNotFoundException)
		{
			std::set<ibrcommon::vaddress> ret;

			try {
				std::string address_str = Configuration::getInstance()._conf.read<std::string>("discovery_address");
				std::vector<std::string> addresses = dtn::utils::Utils::tokenize(" ", address_str);

				for (std::vector<std::string>::iterator iter = addresses.begin(); iter != addresses.end(); ++iter) {
					ret.insert( ibrcommon::vaddress(*iter, port(), AF_UNSPEC) );
				}
			} catch (const ConfigFile::key_not_found&) {
				throw ParameterNotFoundException();
			}

			return ret;
		}

		int Configuration::Discovery::port() const
		{
			return Configuration::getInstance()._conf.read<int>("discovery_port", 4551);
		}

		unsigned int Configuration::Discovery::interval() const
		{
			return _interval;
		}

		bool Configuration::Discovery::enableCrosslayer() const
		{
			return _crosslayer;
		}

		Configuration::NetConfig Configuration::getAPIInterface() const
		{
			Configuration::NetConfig nc("api", Configuration::NetConfig::NETWORK_TCP);

			nc.port = _conf.read<int>("api_port", 4550);

			try {
				const std::string interface_name = _conf.read<std::string>("api_interface");
				nc.iface = ibrcommon::vinterface(interface_name);
			} catch (const ConfigFile::key_not_found&) {
				nc.iface = ibrcommon::vinterface(ibrcommon::vinterface::LOOPBACK);
			}

			return nc;
		}

		ibrcommon::File Configuration::getAPISocket() const
		{
			try {
				return ibrcommon::File(_conf.read<std::string>("api_socket"));
			} catch (const ConfigFile::key_not_found&) {
				throw ParameterNotSetException();
			}

			throw ParameterNotSetException();
		}

		std::string Configuration::getStorage() const
		{
			return _conf.read<std::string>("storage", "default");
		}

		bool Configuration::getUsePersistentBundleSets() const
		{
			return _conf.read<std::string>("use_persistent_bundlesets", "no") == "yes";
		}

		void Configuration::Network::load(const ibrcommon::ConfigFile &conf)
		{
			/**
			 * Load static routes
			 */
			_static_routes.clear();

			std::string key = "route1";
			unsigned int keynumber = 1;

			while (conf.keyExists( key ))
			{
				std::vector<std::string> route = dtn::utils::Utils::tokenize(" ", conf.read<std::string>(key, "dtn:none dtn:none"));
				_static_routes.insert( std::pair<std::string, std::string>( route.front(), route.back() ) );

				keynumber++;
				std::stringstream ss; ss << "route" << keynumber; ss >> key;
			}

			/**
			 * load static nodes
			 */
			// read the node count
			int count = 1;

			// initial prefix
			std::string prefix = "static1_";

			while ( conf.keyExists(prefix + "uri") )
			{
				const dtn::data::EID node_eid( conf.read<std::string>(prefix + "uri", "dtn:none") );

				// create an address URI
				std::stringstream ss;
				ss << "ip=" << conf.read<std::string>(prefix + "address", "127.0.0.1") << ";port=" << conf.read<unsigned int>(prefix + "port", 4556) << ";";

				dtn::core::Node::Protocol p = Node::CONN_UNDEFINED;

				const std::string protocol = conf.read<std::string>(prefix + "proto", "tcp");
				if (protocol == "tcp") p = Node::CONN_TCPIP;
				if (protocol == "udp") p = Node::CONN_UDPIP;
				if (protocol == "lowpan") p = Node::CONN_LOWPAN;
				if (protocol == "zigbee") p = Node::CONN_LOWPAN; //Legacy: Stay compatible with older config files
				if (protocol == "bluetooth") p = Node::CONN_BLUETOOTH;
				if (protocol == "http") p = Node::CONN_HTTP;
				if (protocol == "file") p = Node::CONN_FILE;
				if (protocol == "dgram:udp") p = Node::CONN_DGRAM_UDP;
				if (protocol == "dgram:ethernet") p = Node::CONN_DGRAM_ETHERNET;
				if (protocol == "dgram:lowpan") p = Node::CONN_DGRAM_LOWPAN;
				if (protocol == "email") {
					p = Node::CONN_EMAIL;
					ss.clear();
					ss << "email=" << conf.read<std::string>(prefix + "email", "root@localhost") << ";";
				}

				bool node_exists = false;

				dtn::core::Node::Type t = (conf.read<std::string>(prefix + "global", "no") == "yes") ?
						dtn::core::Node::NODE_STATIC_GLOBAL : dtn::core::Node::NODE_STATIC_LOCAL;

				// get node
				for (std::list<Node>::iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
				{
					dtn::core::Node &n = (*iter);

					if (n.getEID() == node_eid)
					{
						n.add( dtn::core::Node::URI( t, p, ss.str(), 0, -5 ) );
						n.setConnectImmediately( conf.read<std::string>(prefix + "immediately", "no") == "yes" );
						node_exists = true;
						break;
					}
				}

				if (!node_exists)
				{
					Node n(node_eid);
					n.add( dtn::core::Node::URI( t, p, ss.str(), 0, -5 ) );
					n.setConnectImmediately( conf.read<std::string>(prefix + "immediately", "no") == "yes" );
					_nodes.push_back(n);
				}

				count++;

				std::stringstream prefix_stream;
				prefix_stream << "static" << count << "_";
				prefix = prefix_stream.str();
			}

			/**
			 * get routing extension
			 */
			_routing = conf.read<std::string>("routing", "default");

			if(_routing == "prophet"){
				/* read prophet parameters */
				_prophet_config.p_encounter_max = conf.read<float>("prophet_p_encounter_max", 0.7f);
				if(_prophet_config.p_encounter_max > 1 || _prophet_config.p_encounter_max <= 0)
					_prophet_config.p_encounter_max = 0.7f;
				_prophet_config.p_encounter_first = conf.read<float>("prophet_p_encounter_first", 0.5f);
				if(_prophet_config.p_encounter_first > 1 || _prophet_config.p_encounter_first <= 0)
					_prophet_config.p_encounter_first = 0.5f;
				_prophet_config.p_first_threshold = conf.read<float>("prophet_p_first_threshold", 0.1f);
				if(_prophet_config.p_first_threshold < 0 || _prophet_config.p_first_threshold >= _prophet_config.p_encounter_first)
					_prophet_config.p_first_threshold = 0; //disable first threshold on misconfiguration
				_prophet_config.beta = conf.read<float>("prophet_beta", 0.9f);
				if(_prophet_config.beta < 0 || _prophet_config.beta > 1)
					_prophet_config.beta = 0.9f;
				_prophet_config.gamma = conf.read<float>("prophet_gamma", 0.999f);
				if(_prophet_config.gamma <= 0 || _prophet_config.gamma > 1)
					_prophet_config.gamma = 0.999f;
				_prophet_config.delta = conf.read<float>("prophet_delta", 0.01f);
				if(_prophet_config.delta < 0 || _prophet_config.delta > 1)
					_prophet_config.delta = 0.01f;
				_prophet_config.time_unit = conf.read<ibrcommon::Timer::time_t>("prophet_time_unit", 1);
				if(_prophet_config.time_unit < 1)
					_prophet_config.time_unit = 1;
				_prophet_config.i_typ = conf.read<ibrcommon::Timer::time_t>("prophet_i_typ", 300);
				if(_prophet_config.i_typ < 1)
					_prophet_config.i_typ = 1;
				_prophet_config.next_exchange_timeout = conf.read<ibrcommon::Timer::time_t>("prophet_next_exchange_timeout", 600);
				_prophet_config.forwarding_strategy = conf.read<std::string>("prophet_forwarding_strategy", "GRTR");
				_prophet_config.gtmx_nf_max = conf.read<unsigned int>("prophet_gtmx_nf_max", 30);
				_prophet_config.push_notification = (conf.read<std::string>("prophet_push_notification", "no") == "yes");
			}

			/**
			 * get the routing parameters
			 */
			_forwarding = (conf.read<std::string>("routing_forwarding", "yes") == "yes");
			_accept_nonsingleton = (conf.read<std::string>("routing_accept_nonsingleton", "yes") == "yes");

			/**
			 * prefer direct routes
			 */
			_prefer_direct = (conf.read<std::string>("routing_prefer_direct", "yes") == "yes");

			/**
			 * get network interfaces
			 */
			_interfaces.clear();

			if (_use_default_net)
			{
				// create a new netconfig object
				Configuration::NetConfig nc("default", Configuration::NetConfig::NETWORK_TCP);

				// set default interface
				nc.iface = ibrcommon::vinterface(_default_net);

				// set default port
				nc.port = 4556;

				// add to interfaces list
				_interfaces.push_back( nc );
			}
			else try
			{
				std::vector<std::string> nets = dtn::utils::Utils::tokenize(" ", conf.read<std::string>("net_interfaces") );
				for (std::vector<std::string>::const_iterator iter = nets.begin(); iter != nets.end(); ++iter)
				{
					const std::string &netname = (*iter);

					const std::string key_type = "net_" + netname + "_type";
					const std::string key_port = "net_" + netname + "_port";
					const std::string key_interface = "net_" + netname + "_interface";
					const std::string key_address = "net_" + netname + "_address";
					const std::string key_path = "net_" + netname + "_path";
					const std::string key_mtu = "net_" + netname + "_mtu";

					const std::string type_name = conf.read<std::string>(key_type, "tcp");
					Configuration::NetConfig::NetType type = Configuration::NetConfig::NETWORK_UNKNOWN;

					if (type_name == "tcp") type = Configuration::NetConfig::NETWORK_TCP;
					if (type_name == "udp") type = Configuration::NetConfig::NETWORK_UDP;
					if (type_name == "http") type = Configuration::NetConfig::NETWORK_HTTP;
					if (type_name == "lowpan") type = Configuration::NetConfig::NETWORK_LOWPAN;
					if (type_name == "file") type = Configuration::NetConfig::NETWORK_FILE;
					if (type_name == "dgram:udp") type = Configuration::NetConfig::NETWORK_DGRAM_UDP;
					if (type_name == "dgram:lowpan") type = Configuration::NetConfig::NETWORK_DGRAM_LOWPAN;
					if (type_name == "dgram:ethernet") type = Configuration::NetConfig::NETWORK_DGRAM_ETHERNET;
					if (type_name == "email") type = Configuration::NetConfig::NETWORK_EMAIL;

					// create a new netconfig object
					Configuration::NetConfig nc(netname, type);

					switch (type)
					{
						case Configuration::NetConfig::NETWORK_HTTP:
						{
							nc.url = conf.read<std::string>(key_address, "http://localhost/");
							break;
						}

						case Configuration::NetConfig::NETWORK_FILE:
						{
							nc.url = conf.read<std::string>(key_path, "");
							break;
						}

						default:
						{
							nc.port = conf.read<int>(key_port, 4556);
							nc.mtu = conf.read<int>(key_mtu, 1280);

							try {
								nc.iface = ibrcommon::vinterface(conf.read<std::string>(key_interface));
							} catch (const ConfigFile::key_not_found&) {
								// no interface specfied - use ANY
								nc.iface = ibrcommon::vinterface(ibrcommon::vinterface::ANY);
							}

							break;
						}
					}

					// add to interfaces list
					_interfaces.push_back(nc);
				}
			} catch (const ConfigFile::key_not_found&) {
				// stop the one network is not found.
			}

			/**
			 * Set ANY as default interface if no network is configured
			 */
			if (_interfaces.empty())
			{
				// create a new netconfig object
				Configuration::NetConfig any("default", Configuration::NetConfig::NETWORK_TCP);

				// create any interface object
				any.iface = ibrcommon::vinterface(ibrcommon::vinterface::ANY);

				// set default port
				any.port = 4556;

				// add to interfaces list
				_interfaces.push_back( any );
			}

			/**
			 * TCP options
			 */
			_tcp_nodelay = (conf.read<std::string>("tcp_nodelay", "yes") == "yes");
			_tcp_chunksize = conf.read<unsigned int>("tcp_chunksize", 4096);
			_tcp_idle_timeout = conf.read<unsigned int>("tcp_idle_timeout", 0);

			/**
			 * Keep alive interval for network connections
			 */
			_keepalive_timeout = conf.read<unsigned int>("keepalive_timeout", 60);

			/**
			 * auto connect interval
			 */
			_auto_connect = conf.read<dtn::data::Timeout>("net_autoconnect", 0);

			/**
			 * fragmentation support
			 */
			_fragmentation = (conf.read<std::string>("fragmentation", "yes") == "yes");

			/**
			 * read internet devices
			 */
			try {
				std::vector<std::string> inets = dtn::utils::Utils::tokenize(" ", conf.read<std::string>("net_internet") );
				for (std::vector<std::string>::const_iterator iter = inets.begin(); iter != inets.end(); ++iter)
				{
					ibrcommon::vinterface inet_dev(*iter);
					_internet_devices.insert(inet_dev);
				}
			} catch (const ibrcommon::ConfigFile::key_not_found&) { };

			/**
			 * scheduling support
			 */
			_scheduling = (conf.read<std::string>("scheduling", "no") == "yes");

			/**
			 * read link-request-interval
			 */
			_link_request_interval = conf.read<dtn::data::Timeout>("link_request_interval",5000);
		}

		const std::multimap<std::string, std::string>& Configuration::Network::getStaticRoutes() const
		{
			return _static_routes;
		}

		const std::list<Node>& Configuration::Network::getStaticNodes() const
		{
			return _nodes;
		}

		ibrcommon::File Configuration::getPath(std::string name) const
		{
			std::stringstream ss;
			ss << name << "_path";
			std::string key; ss >> key;

			try {
				return ibrcommon::File(_conf.read<std::string>(key));
			} catch (const ConfigFile::key_not_found&) { }

			throw ParameterNotSetException();
		}

		bool Configuration::Discovery::enabled() const
		{
			return _enabled;
		}

		bool Configuration::Discovery::announce() const
		{
			return _announce;
		}

		bool Configuration::Discovery::shortbeacon() const
		{
			return _short;
		}

		int Configuration::Discovery::version() const
		{
			return _version;
		}

		bool Configuration::doAPI() const
		{
			return _doapi;
		}

		Configuration::RoutingExtension Configuration::Network::getRoutingExtension() const
		{
			if ( _routing == "none" ) return NO_ROUTING;
			if ( _routing == "epidemic" ) return EPIDEMIC_ROUTING;
			if ( _routing == "flooding" ) return FLOOD_ROUTING;
			if ( _routing == "prophet" ) return PROPHET_ROUTING;
			return DEFAULT_ROUTING;
		}


		bool Configuration::Network::doForwarding() const
		{
			return _forwarding;
		}

		bool Configuration::Network::doAcceptNonSingleton() const
		{
			return _accept_nonsingleton;
		}

		bool Configuration::Network::doPreferDirect() const
		{
			return _prefer_direct;
		}

		bool Configuration::Network::doFragmentation() const
		{
			return _fragmentation;
		}

		bool Configuration::Network::doScheduling() const
		{
			return _scheduling;
		}

		bool Configuration::Network::getTCPOptionNoDelay() const
		{
			return _tcp_nodelay;
		}

		dtn::data::Length Configuration::Network::getTCPChunkSize() const
		{
			return _tcp_chunksize;
		}

		dtn::data::Timeout Configuration::Network::getTCPIdleTimeout() const
		{
			return _tcp_idle_timeout;
		}

		dtn::data::Timeout Configuration::Network::getKeepaliveInterval() const
		{
			return _keepalive_timeout;
		}

		dtn::data::Timeout Configuration::Network::getAutoConnect() const
		{
			return _auto_connect;
		}

		Configuration::Network::ProphetConfig Configuration::Network::getProphetConfig() const
		{
			return _prophet_config;
		}

		std::set<ibrcommon::vinterface> Configuration::Network::getInternetDevices() const
		{
			return _internet_devices;
		}

		size_t Configuration::Network::getLinkRequestInterval() const
		{
			return _link_request_interval;
		}

		dtn::data::Size Configuration::getLimit(const std::string &suffix) const
		{
			std::string unparsed = _conf.read<std::string>("limit_" + suffix, "0");

			std::stringstream ss(unparsed);

			float value; ss >> value;
			char multiplier = 0; ss >> multiplier;

			switch (multiplier)
			{
			default:
				return static_cast<dtn::data::Size>(value);
				break;

			case 'G':
				return static_cast<dtn::data::Size>(value * 1000000000);
				break;

			case 'M':
				return static_cast<dtn::data::Size>(value * 1000000);
				break;

			case 'K':
				return static_cast<dtn::data::Size>(value * 1000);
				break;
			}

			return 0;
		}

		void Configuration::Security::load(const ibrcommon::ConfigFile &conf)
		{
			bool withTLS = false;
#ifdef WITH_TLS
			withTLS = true;
			/* enable TLS if the certificate path, a certificate and the private key was given */
			bool activateTLS = true;

			// load public key file
			try {
				_cert = conf.read<std::string>("security_certificate");

				if (!_cert.exists())
				{
					IBRCOMMON_LOGGER_TAG("Configuration", warning) << "Certificate file " << _cert.getPath() << " does not exists!" << IBRCOMMON_LOGGER_ENDL;
					activateTLS = false;
				}
			} catch (const ibrcommon::ConfigFile::key_not_found&) {
				activateTLS = false;
			}

			// load KEY file
			try {
				_key = conf.read<std::string>("security_key");

				if (!_key.exists())
				{
					IBRCOMMON_LOGGER_TAG("Configuration", warning) << "KEY file " << _key.getPath() << " does not exists!" << IBRCOMMON_LOGGER_ENDL;
					activateTLS = false;
				}
			} catch (const ibrcommon::ConfigFile::key_not_found&) {
				activateTLS = false;
			}

			// read trustedCAPath
			try{
				_trustedCAPath = conf.read<std::string>("security_trusted_ca_path");
				if(!_trustedCAPath.isDirectory()){
					IBRCOMMON_LOGGER_TAG("Configuration", warning) << "Trusted CA Path " << _trustedCAPath.getPath() << " does not exists or is no directory!" << IBRCOMMON_LOGGER_ENDL;
					activateTLS = false;
				}
			} catch (const ibrcommon::ConfigFile::key_not_found&) {
				activateTLS = false;
			}

			// read if encryption should be disabled
			_disableEncryption = (conf.read<std::string>("security_tls_disable_encryption", "no") == "yes");

			if (activateTLS)
			{
				_tlsEnabled = true;

				/* read if TLS is required */
				_tlsRequired = (conf.read<std::string>("security_tls_required", "no") == "yes");

				/* If the clock is not in sync, SSL will fail. Accept plain connection when the clock is not sync'ed. */
				_tlsOptionalOnBadClock = (conf.read<std::string>("security_tls_fallback_badclock", "no") == "yes");
			}
#endif

#ifdef IBRDTN_SUPPORT_BSP
			// enable security if the security path is set
			try {
				_path = conf.read<std::string>("security_path");

				if (!_path.exists())
				{
					ibrcommon::File::createDirectory(_path);
				}

				_enabled = true;
			} catch (const ibrcommon::ConfigFile::key_not_found&) {
				return;
			}

			// load level
			_level = Level(conf.read<int>("security_level", 0));

			if ( !withTLS )
			{
				/* if TLS is enabled, the Certificate file and the key have been read earlier */
				// load CA file
				try {
					_cert = conf.read<std::string>("security_certificate");

					if (!_cert.exists())
					{
						IBRCOMMON_LOGGER_TAG("Configuration", warning) << "Certificate file " << _cert.getPath() << " does not exists!" << IBRCOMMON_LOGGER_ENDL;
					}
				} catch (const ibrcommon::ConfigFile::key_not_found&) { }

				// load KEY file
				try {
					_key = conf.read<std::string>("security_key");

					if (!_key.exists())
					{
						IBRCOMMON_LOGGER_TAG("Configuration", warning) << "KEY file " << _key.getPath() << " does not exists!" << IBRCOMMON_LOGGER_ENDL;
					}
				} catch (const ibrcommon::ConfigFile::key_not_found&) { }
			}

			// load KEY file
			try {
				_bab_default_key = conf.read<std::string>("security_bab_default_key");

				if (!_bab_default_key.exists())
				{
					IBRCOMMON_LOGGER_TAG("Configuration", warning) << "KEY file " << _bab_default_key.getPath() << " does not exists!" << IBRCOMMON_LOGGER_ENDL;
				}
			} catch (const ibrcommon::ConfigFile::key_not_found&) {
			}

			// automatic DH parameters
			try {
				_generate_dh_params = conf.read<std::string>("generate_dh_params") == "yes";
			} catch (const ibrcommon::ConfigFile::key_not_found&) {
			}
#endif
		}

		Configuration::Security::~Security() {}

		bool Configuration::Security::enabled() const
		{
			return _enabled;
		}

		bool Configuration::Security::doTLS() const
		{
			return _tlsEnabled;
		}

		bool Configuration::Security::TLSRequired() const
		{
			// TLS is only required, if the clock is in sync
			if ((dtn::utils::Clock::getRating() == 0) && _tlsOptionalOnBadClock) return false;
			return _tlsRequired;
		}

		const ibrcommon::File& Configuration::Security::getPath() const
		{
			return _path;
		}

		int Configuration::Security::getLevel() const
		{
			return _level;
		}

		const ibrcommon::File& Configuration::Security::getBABDefaultKey() const
		{
			return _bab_default_key;
		}

		const ibrcommon::File& Configuration::Security::getCertificate() const
		{
			return _cert;
		}

		const ibrcommon::File& Configuration::Security::getKey() const
		{
			return _key;
		}

		const ibrcommon::File& Configuration::Security::getTrustedCAPath() const
		{
			return _trustedCAPath;
		}

		bool Configuration::Security::TLSEncryptionDisabled() const
		{
			return _disableEncryption;
		}

		bool Configuration::Security::isGenerateDHParamsEnabled() const
		{
			return _generate_dh_params;
		}

		bool Configuration::Logger::quiet() const
		{
			return _quiet;
		}

		const ibrcommon::File& Configuration::Logger::getLogfile() const
		{
			if (_logfile.getPath() == "") throw Configuration::ParameterNotSetException();
			return _logfile;
		}

		bool Configuration::Logger::verbose() const
		{
			return _verbose;
		}

		bool Configuration::Logger::display_timestamps() const
		{
			return _timestamps;
		}

		unsigned int Configuration::Logger::options() const
		{
			return _options;
		}

		std::ostream& Configuration::Logger::output() const
		{
			return std::cout;
		}

		bool Configuration::Daemon::daemonize() const
		{
			return _daemonize;
		}

		bool Configuration::Daemon::kill_daemon() const
		{
			return _kill;
		}

		dtn::data::Size Configuration::Daemon::getThreads() const
		{
			return _threads;
		}

		const ibrcommon::File& Configuration::Daemon::getPidFile() const
		{
			if (_pidfile == ibrcommon::File()) throw ParameterNotSetException();
			return _pidfile;
		}

		bool Configuration::TimeSync::hasReference() const
		{
			return _reference;
		}

		bool Configuration::TimeSync::doSync() const
		{
			return _sync;
		}

		bool Configuration::TimeSync::sendDiscoveryBeacons() const
		{
			return _discovery;
		}

		float Configuration::TimeSync::getSigma() const
		{
			return _sigma;
		}

		float Configuration::TimeSync::getPsi() const
		{
			return _psi;
		}

		float Configuration::TimeSync::getSyncLevel() const
		{
			return _sync_level;
		}

		bool Configuration::DHT::enabled() const
		{
			return _enabled;
		}

		bool Configuration::DHT::randomPort() const
		{
			return _port == 0;
		}

		unsigned int Configuration::DHT::getPort() const
		{
			return _port;
		}

		std::string Configuration::DHT::getID() const
		{
			return _id;
		}

		bool Configuration::DHT::randomID() const
		{
			return _id == "";
		}

		bool Configuration::DHT::isDNSBootstrappingEnabled() const
		{
			return _dnsbootstrapping;
		}

		std::vector<std::string> Configuration::DHT::getDNSBootstrappingNames() const
		{
			return _bootstrappingdomains;
		}

		bool Configuration::DHT::isIPBootstrappingEnabled() const
		{
			return !_bootstrappingips.empty();
		}

		std::vector<std::string> Configuration::DHT::getIPBootstrappingIPs() const
		{
			return _bootstrappingips;
		}

		std::string Configuration::DHT::getIPv4Binding() const
		{
			return _ipv4bind;
		}
		std::string Configuration::DHT::getIPv6Binding() const
		{
			return _ipv6bind;
		}

		std::string Configuration::DHT::getPathToNodeFiles() const
		{
			return _nodesFilePath;
		}

		bool Configuration::DHT::isIPv4Enabled() const
		{
			return _ipv4;
		}

		bool Configuration::DHT::isIPv6Enabled() const
		{
			return _ipv6;
		}

		bool Configuration::DHT::isSelfAnnouncingEnabled() const
		{
			return _selfannounce;
		}

		int Configuration::DHT::getMinimumRating() const
		{
			return _minRating;
		}

		bool Configuration::DHT::isNeighbourAnnouncementEnabled() const
		{
			return _allowNeighbourAnnouncement;
		}

		bool Configuration::DHT::isNeighbourAllowedToAnnounceMe() const
		{
			return _allowNeighbourToAnnounceMe;
		}

		bool Configuration::DHT::isBlacklistEnabled() const
		{
			return _blacklist;
		}

		bool Configuration::DHT::ignoreDHTNeighbourInformations() const
		{
			return _ignoreDHTNeighbourInformations;
		}

		bool Configuration::P2P::enabled() const
		{
			return _enabled;
		}

		const std::string Configuration::P2P::getCtrlPath() const
		{
			return _ctrl_path;
		}

		std::string Configuration::EMail::getOwnAddress() const
		{
			return _address;
		}

		std::string Configuration::EMail::getSmtpServer() const
		{
			return _smtpServer;
		}

		int Configuration::EMail::getSmtpPort() const
		{
			return _smtpPort;
		}

		std::string Configuration::EMail::getSmtpUsername() const
		{
			return _smtpUsername;
		}

		std::string Configuration::EMail::getSmtpPassword() const
		{
			return _smtpPassword;
		}

		size_t Configuration::EMail::getSmtpSubmitInterval() const
		{
			return _smtpInterval;
		}

		size_t Configuration::EMail::getSmtpConnectionTimeout() const
		{
			return _smtpConnectionTimeout;
		}
		size_t Configuration::EMail::getSmtpKeepAliveTimeout() const
		{
			return _smtpKeepAliveTimeout * 1000;
		}

		bool Configuration::EMail::smtpAuthenticationNeeded() const
		{
			return _smtpNeedAuth;
		}

		bool Configuration::EMail::smtpUseTLS() const
		{
			return _smtpUseTLS;
		}

		bool Configuration::EMail::smtpUseSSL() const
		{
			return _smtpUseSSL;
		}

		std::string Configuration::EMail::getImapServer() const
		{
			return _imapServer;
		}

		int Configuration::EMail::getImapPort() const
		{
			return _imapPort;
		}

		std::string Configuration::EMail::getImapUsername() const
		{
			return _imapUsername;
		}

		std::string Configuration::EMail::getImapPassword() const
		{
			return _imapPassword;
		}

		std::vector<std::string> Configuration::EMail::getImapFolder() const
		{
			return _imapFolder;
		}

		size_t Configuration::EMail::getImapLookupInterval() const
		{
			return _imapInterval;
		}

		size_t Configuration::EMail::getImapConnectionTimeout() const
		{
			return _imapConnectionTimeout;
		}

		bool Configuration::EMail::imapUseTLS() const
		{
			return _imapUseTLS;
		}

		bool Configuration::EMail::imapUseSSL() const
		{
			return _imapUseSSL;
		}

		bool Configuration::EMail::imapPurgeMail() const
		{
			return _imapPurgeMail;
		}

		std::vector<std::string> Configuration::EMail::getTlsCACerts() const
		{
			return _tlsCACerts;
		}

		std::vector<std::string> Configuration::EMail::getTlsUserCerts() const
		{
			return _tlsUserCerts;
		}

		size_t Configuration::EMail::getNodeAvailableTime() const
		{
			return _availableTime;
		}

		size_t Configuration::EMail::getReturningMailChecks() const
		{
			return _returningMailsCheck;
		}
	}
}
