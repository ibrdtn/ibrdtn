/*
 * Configuration.h
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

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include "ibrcommon/data/ConfigFile.h"
#include "core/Node.h"
#include "routing/StaticRoutingExtension.h"
#include <ibrcommon/Exceptions.h>
#include <ibrcommon/net/vinterface.h>
#include <map>
#include <list>
#include <ibrcommon/thread/Timer.h>

using namespace dtn::net;
using namespace dtn::core;
using namespace dtn::data;

namespace dtn
{
	namespace daemon
	{
		/**
		 * This class contains the hole configuration for the daemon.
		 */
		class Configuration
		{
		private:
			Configuration();
			virtual ~Configuration();

		public:
			class OnChangeListener {
			public:
				virtual void onConfigurationChanged(const dtn::daemon::Configuration &conf) throw () = 0;
			};

			class NetConfig
			{
			public:
				enum NetType
				{
					NETWORK_UNKNOWN = 0,
					NETWORK_TCP = 1,
					NETWORK_UDP = 2,
					NETWORK_HTTP = 3,
					NETWORK_LOWPAN = 4,
					NETWORK_FILE = 5,
					NETWORK_DGRAM_UDP = 6,
					NETWORK_DGRAM_LOWPAN = 7,
					NETWORK_DGRAM_ETHERNET = 8,
					NETWORK_EMAIL = 9
				};

				NetConfig(const std::string &name, NetType type);
				virtual ~NetConfig();

				std::string name;
				NetType type;
				std::string url;
				ibrcommon::vinterface iface;
				int mtu;
				int port;
			};

			class ParameterNotSetException : ibrcommon::Exception
			{
			};

			class ParameterNotFoundException : ibrcommon::Exception
			{
			};

			static Configuration &getInstance(bool reset = false);

			/**
			 * load the configuration from a file
			 */
			void load(bool quiet = false);
			void load(const std::string &filename, bool quiet = false);

			void params(int argc, char *argv[]);

			/**
			 * Returns the name of the node
			 */
			std::string getNodename() const;

			/**
			 * Generic command to get a specific path. If "name" is
			 * set to "foo" then the parameter "foo_path" is returned.
			 * @param name The prefix of the path to get.
			 * @return The path as file object.
			 */
			ibrcommon::File getPath(string name) const;

			/**
			 * Enable/Disable the API interface.
			 * @return True, if the API interface should be enabled.
			 */
			bool doAPI() const;

			Configuration::NetConfig getAPIInterface() const;
			ibrcommon::File getAPISocket() const;

			/**
			 * Get the version of this daemon.
			 * @return The version string.
			 */
			std::string version() const;

			/**
			 * Get the type of bundle storage to use.
			 * @return
			 */
			std::string getStorage() const;

			/**
			 * returns, whether Persistent BundleSets are used (stored in SQL database)
			 */

			bool getUsePersistentBundleSets() const;

			enum RoutingExtension
			{
				DEFAULT_ROUTING = 0,
				EPIDEMIC_ROUTING = 1,
				FLOOD_ROUTING = 2,
				PROPHET_ROUTING = 3,
				NO_ROUTING = 4
			};

			/**
			 * Returns a limit defined in the configuration file. The given string specify with limit is to return.
			 * If the string is "block", then the value of "limit_block" is returned.
			 * @return A limit in bytes.
			 */
			dtn::data::Size getLimit(const std::string&) const;

			class Extension
			{
			protected:
				virtual void load(const ibrcommon::ConfigFile &conf) = 0;
			};

			class Discovery : public Configuration::Extension
			{
				friend class Configuration;
			protected:
				Discovery();
				virtual ~Discovery();
				void load(const ibrcommon::ConfigFile &conf);

				bool _enabled;
				unsigned int _interval;
				bool _announce;
				bool _short;
				int _version;
				bool _crosslayer;

			public:
				bool enabled() const;
				bool announce() const;
				bool shortbeacon() const;
				int version() const;
				const std::set<ibrcommon::vaddress> address() const throw (ParameterNotFoundException);
				int port() const;
				int localPort() const;
				unsigned int interval() const;
				bool enableCrosslayer() const;
			};

			class Debug : public Configuration::Extension
			{
				friend class Configuration;
			protected:
				Debug();
				virtual ~Debug();
				void load(const ibrcommon::ConfigFile &conf);

				bool _enabled;
				bool _quiet;
				int _level;
				bool _profiling;

			public:
				/**
				 * @return The debug level as integer value.
				 */
				int level() const;

				/**
				 * @return True, if the daemon should work in debug mode.
				 */
				bool enabled() const;

				/**
				 * Returns true if the daemon should work in quiet mode.
				 * @return True, if the daemon should be quiet.
				 */
				bool quiet() const;

				/**
				 * Returns true if the daemon should execute profiling code.
				 * @return True, if profiling is activated
				 */
				bool profiling() const;
			};

			class Logger : public Configuration::Extension
			{
				friend class Configuration;
			protected:
				Logger();
				virtual ~Logger();
				void load(const ibrcommon::ConfigFile &conf);

				bool _quiet;
				unsigned int _options;
				bool _timestamps;
				ibrcommon::File _logfile;
				bool _verbose;

			public:
				/**
				 * Enable the quiet mode if set to true.
				 * @return True, if quiet mode is set.
				 */
				bool quiet() const;

				/**
				 * Get a logfile for standard logging output
				 * @return
				 */
				const ibrcommon::File& getLogfile() const;

				/**
				 * Get the options for logging.
				 * This is an unsigned integer with bit flags.
				 * 1 = DATETIME
				 * 2 = HOSTNAME
				 * 4 = LEVEL
				 * 8 = TIMESTAMP
				 * @return The options to set as bit field.
				 */
				unsigned int options() const;

				/**
				 * The output stream for the logging output
				 */
				std::ostream &output() const;

				/**
				 * Returns true if the logger display timestamp instead of datetime values.
				 */
				bool display_timestamps() const;

				/**
				 * Returns true if verbose logging is activated
				 */
				bool verbose() const;
			};

			class Network :  public Configuration::Extension
			{
				friend class Configuration;
			public:
				/* prophet routing parameters */
				class ProphetConfig{
				public:
					ProphetConfig()
					: p_encounter_max(0), p_encounter_first(0), p_first_threshold(0), beta(0), gamma(0), delta(0),
					  time_unit(0), i_typ(0), next_exchange_timeout(0), forwarding_strategy(), gtmx_nf_max(0), push_notification(false)
					{ }

					~ProphetConfig() { }

					float p_encounter_max;
					float p_encounter_first;
					float p_first_threshold;
					float beta;
					float gamma;
					float delta;
					ibrcommon::Timer::time_t time_unit;
					ibrcommon::Timer::time_t i_typ;
					ibrcommon::Timer::time_t next_exchange_timeout;
					std::string forwarding_strategy;
					unsigned int gtmx_nf_max;
					bool push_notification;
				};
			protected:
				Network();
				virtual ~Network();
				void load(const ibrcommon::ConfigFile &conf);

				std::multimap<std::string, std::string> _static_routes;
				std::list<Node> _nodes;
				std::list<NetConfig> _interfaces;
				std::string _routing;
				bool _forwarding;
				bool _accept_nonsingleton;
				bool _prefer_direct;
				bool _tcp_nodelay;
				dtn::data::Length _tcp_chunksize;
				dtn::data::Timeout _tcp_idle_timeout;
				dtn::data::Timeout _keepalive_timeout;
				ibrcommon::vinterface _default_net;
				bool _use_default_net;
				dtn::data::Timeout _auto_connect;
				bool _fragmentation;
				bool _scheduling;
				ProphetConfig _prophet_config;
				std::set<ibrcommon::vinterface> _internet_devices;
				bool _managed_connectivity;
				size_t _link_request_interval;

			public:
				/**
				 * Returns all configured network interfaces
				 */
				const std::list<NetConfig>& getInterfaces() const;

				/**
				 * Returns all static neighboring nodes
				 */
				const std::list<Node>& getStaticNodes() const;

				/**
				 * Returns all static routes
				 */
				const std::multimap<std::string, std::string>& getStaticRoutes() const;

				/**
				 * @return the routing extension to use.
				 */
				RoutingExtension getRoutingExtension() const;

				/**
				 * Define if forwarding is enabled. If not, only local bundles will be accepted.
				 * @return True, if forwarding is enabled.
				 */
				bool doForwarding() const;

				/**
				 * Define if non-singleton bundles are accepted or not.
				 */
				bool doAcceptNonSingleton() const;

				/**
				 * Define if direct routes are preferred instead of spreading bundles to all
				 * neighbors.
				 * @return True, if direct routes should preferred
				 */
				bool doPreferDirect() const;

				/**
				 * @return True, is tcp options NODELAY should be set.
				 */
				bool getTCPOptionNoDelay() const;

				/**
				 * @return The size of TCP chunks for bundles.
				 */
				dtn::data::Length getTCPChunkSize() const;

				/**
				 * @return The idle timeout for TCP connections in seconds.
				 */
				dtn::data::Timeout getTCPIdleTimeout() const;

				/**
				 * @return The keep-alive interval for network connections.
				 */
				dtn::data::Timeout getKeepaliveInterval() const;

				/**
				 * @return Each x seconds try to connect to all available nodes.
				 */
				dtn::data::Timeout getAutoConnect() const;

				/**
				 * @return True, if fragmentation support is enabled.
				 */
				bool doFragmentation() const;

				/**
				 * @return a struct containing the prophet configuration parameters
				 */
				ProphetConfig getProphetConfig() const;

				/**
				 * @return True, if scheduling is used.
				 */
				bool doScheduling() const;

				/**
				 * @return The interfaces which are potentially connected to the internet
				 */
				std::set<ibrcommon::vinterface> getInternetDevices() const;

				/**
				 * @return Number of milliseconds between two linkstate-requests (as netlink-fallback)
				 */
				size_t getLinkRequestInterval() const;
			};

			class Security : public Configuration::Extension
			{
				friend class Configuration;
			private:
				bool _enabled;
				bool _tlsEnabled;
				bool _tlsRequired;
				bool _tlsOptionalOnBadClock;
				bool _generate_dh_params;

			protected:
				Security();
				virtual ~Security();
				void load(const ibrcommon::ConfigFile &conf);

			public:
				bool enabled() const;

				/*!
				 * \brief checks if TLS shall be activated
				 * \return true if TLS is requested, false otherwise
				 * If TLS is requested, the TCP Convergence Layer Contact Header has the most significant bit
				 * of the flags field set to 1. If both peers support it, a TLS Handshake is executed.
				 */
				bool doTLS() const;

				/*!
				 * \brief Checks if TLS is required
				 * \return true if TLS is required, false otherwise
				 * If TLS is required, this node should abort TCP Convergence Layer Connections immediately if TLS is not available or fails
				 */
				bool TLSRequired() const;

				enum Level
				{
					SECURITY_LEVEL_NONE = 0,
					SECURITY_LEVEL_AUTHENTICATED = 1,
					SECURITY_LEVEL_ENCRYPTED = 2,
					SECURITY_LEVEL_SIGNED = 4
				};

				/**
				 * Get the configured security level
				 */
				int getLevel() const;

				/**
				 * Get the path to security related files
				 */
				const ibrcommon::File& getPath() const;

				/**
				 * Get the path to the default BAB key
				 */
				const ibrcommon::File& getBABDefaultKey() const;

				/**
				 * Get the path to the TLS certificate
				 */
				const ibrcommon::File& getCertificate() const;

				/**
				 * Get the path to the private TLS key
				 */
				const ibrcommon::File& getKey() const;

				/*!
				 * \brief Read the path for trusted Certificates from the Configuration.
				 * \return A file object for the path
				 */
				const ibrcommon::File& getTrustedCAPath() const;

				/*!
				 * \brief Checks if Encryption in TLS shall be disabled.
				 * \return true if encryption shall be disabled, false otherwise
				 */
				bool TLSEncryptionDisabled() const;

				/*!
				 * \brief Generate DH parameters automatically if necessary.
				 * \return true if the DH parameters shall be generated automatically, false otherwise
				 */
				bool isGenerateDHParamsEnabled() const;

			private:
				// security related files
				ibrcommon::File _path;

				// security level
				int _level;

				// local BAB key
				ibrcommon::File _bab_default_key;

				// TLS certificate
				ibrcommon::File _cert;

				// TLS private key
				ibrcommon::File _key;

				// TLS trusted CA path
				ibrcommon::File _trustedCAPath;

				// TLS encryption disabled?
				bool _disableEncryption;
			};

			class Daemon : public Configuration::Extension
			{
				friend class Configuration;
			private:
				bool _daemonize;
				ibrcommon::File _pidfile;
				bool _kill;
				dtn::data::Size _threads;
				long _start_time;
				long _stop_time;
				long _seed;

			protected:
				Daemon();
				virtual ~Daemon();
				void load(const ibrcommon::ConfigFile &conf);

			public:
				bool daemonize() const;
				const ibrcommon::File& getPidFile() const;
				bool kill_daemon() const;
				dtn::data::Size getThreads() const;
				long start_time() const;
				long stop_time() const;
				long seed() const;
			};

			class TimeSync : public Configuration::Extension
			{
				friend class Configuration;
			protected:
				TimeSync();
				virtual ~TimeSync();
				void load(const ibrcommon::ConfigFile &conf);

				bool _reference;
				bool _sync;
				bool _discovery;
				float _sigma;
				float _psi;
				float _sync_level;

			public:
				bool hasReference() const;
				bool doSync() const;
				bool sendDiscoveryBeacons() const;

				float getSigma() const;
				float getPsi() const;
				float getSyncLevel() const;
			};
	
			class DHT: public Configuration::Extension
			{
				friend class Configuration;
			protected:
				DHT();
				virtual ~DHT();
				void load(const ibrcommon::ConfigFile &conf);

			private:
				bool _enabled;
				int _port;
				string _id;
				string _ipv4bind;
				string _ipv6bind;
				std::vector<string> _bootstrappingdomains;
				bool _dnsbootstrapping;
				std::vector<string> _bootstrappingips;
				string _nodesFilePath;
				bool _ipv4;
				bool _ipv6;
				bool _blacklist;
				bool _selfannounce;
				std::vector<int> _portFilter;
				int _minRating;
				bool _allowNeighbourToAnnounceMe;
				bool _allowNeighbourAnnouncement;
				bool _ignoreDHTNeighbourInformations;

			public:
				/**
				 * @return True, if the dht name service is enabled
				 */
				bool enabled() const;

				/**
				 * @return True, if a random port will be chosen
				 */
				bool randomPort() const;

				/**
				 * @return the port, which should be used for the dht
				 */
				unsigned int getPort() const;

				/**
				 * @return a string, which should be used to generate a dht id
				 */
				string getID() const;

				/**
				 * @return True, if no string was given to generate a dht id
				 */
				bool randomID() const;

				/**
				 * @return True, if dns bootstrapping is not disabled
				 */
				bool isDNSBootstrappingEnabled() const;

				/**
				 * @return List of domain names for bootstrapping
				 */
				std::vector<string> getDNSBootstrappingNames() const;

				/**
				 * @return True, if a IP (and port) information was given
				 */
				bool isIPBootstrappingEnabled() const;

				/**
				 * @return list of all IP(and port) informations
				 */
				std::vector<string> getIPBootstrappingIPs() const;

				/**
				 * @return a exact IPv4 address, to be used to bind the dht IPv4 socket
				 */
				string getIPv4Binding() const;

				/**
				 * @return a exact IPv6 address, to be used to bind the dht IPv6 socket
				 */
				string getIPv6Binding() const;

				/**
				 * Gives a path to a file, where the dtndht lib could save good nodes on shutdown,
				 * or read good nodes used as the last session to bootstrap from them.
				 * If it doesn't exist, it will be created by the lib on writing good nodes.
				 *
				 * @return a string with the path to the file
				 */
				string getPathToNodeFiles() const;

				/**
				 * @return True, if IPv4 should be used for the DHT
				 */
				bool isIPv4Enabled() const;

				/**
				 * @return True, if IPv6 should be used for the DHT
				 */
				bool isIPv6Enabled() const;

				/**
				 * @return a list of all allowed ports
				 */
				std::vector<int> getPortFilter() const;

				/**
				 * @returns True, if a blacklist should be used by the dtndht lib
				 */
				bool isBlacklistEnabled() const;

				/**
				 * @return True, if this daemon should announce itself on the dht
				 */
				bool isSelfAnnouncingEnabled() const;

				/**
				 * @return the minimum rating of a dht node information
				 */
				int getMinimumRating() const;

				/**
				 * @return True, if I announce my neighbours too
				 */
				bool isNeighbourAnnouncementEnabled() const;

				/**
				 * @return True, if all neighbours are allowed to announce me
				 */
				bool isNeighbourAllowedToAnnounceMe() const;
				
				/**
				 * If this method return true, neighbours of a found node should be ignored.
				 * This can be useful, if any routing algorithm can do this instead.
				 * If a routing algorithm, like prophet, askes also for the neighbours of a node,
				 * this method should return true, otherwise, the sent informations about the
				 * neighbours will be used to create static routes to the neighbour over the answering node.
				 *
				 * @return True, if the neighbours of a found Node should be ignored, default is false
				 */
				bool ignoreDHTNeighbourInformations() const;
			};

			class P2P : public Configuration::Extension
			{
				friend class Configuration;
			protected:
				P2P();
				virtual ~P2P();
				void load(const ibrcommon::ConfigFile &conf);

				std::string _ctrl_path;
				bool _enabled;

			public:
				const std::string getCtrlPath() const;
				bool enabled() const;
			};

			class EMail: public Configuration::Extension
			{
				friend class Configuration;
			protected:
				EMail();
				virtual ~EMail();
				void load(const ibrcommon::ConfigFile &conf);
			private:
				std::string _address;
				std::string _smtpServer;
				int _smtpPort;
				std::string _smtpUsername;
				std::string _smtpPassword;
				bool _smtpUseTLS;
				bool _smtpUseSSL;
				bool _smtpNeedAuth;
				size_t _smtpInterval;
				size_t _smtpConnectionTimeout;
				size_t _smtpKeepAliveTimeout;
				std::string _imapServer;
				int _imapPort;
				std::string _imapUsername;
				std::string _imapPassword;
				bool _imapUseTLS;
				bool _imapUseSSL;
				std::vector<std::string> _imapFolder;
				size_t _imapInterval;
				size_t _imapConnectionTimeout;
				bool _imapPurgeMail;
				std::vector<std::string> _tlsCACerts;
				std::vector<std::string> _tlsUserCerts;
				size_t _availableTime;
				size_t _returningMailsCheck;

			public:
				/**
				 * Will return the nodes email address. If no address was set
				 * in the configuration file it will return "root@localhost"
				 *
				 * @return The nodes email address
				 */
				std::string getOwnAddress() const;

				/**
				 * Will return the SMTP server for this node. If no SMTP server
				 * was defined it will return "localhost"
				 *
				 * @return The SMTP server for this node
				 */
				std::string getSmtpServer() const;

				/**
				 * Will return the port for the SMTP server. If this option was
				 * not defined the standard SMTP port (25) will be returned
				 *
				 * @return The port of the SMTP server
				 */
				int getSmtpPort() const;

				/**
				 * Will return the user name for the SMTP server. If no name
				 * was specified "root" will be returned
				 *
				 * @return The user name for the SMTP server
				 */
				std::string getSmtpUsername() const;

				/**
				 * Will return the password for the SMTP server. If no password
				 * was specified an empty string will be returned
				 *
				 * @return The password for the SMTP server
				 */
				std::string getSmtpPassword() const;

				/**
				 * Will return the SMTP submit interval. This interval defines
				 * how many seconds will be between two consecutive SMTP submit
				 * intervals. If nothing was specified in the configuration file
				 * "60" will be returned
				 *
				 * @return The SMTP submit interval in seconds
				 */
				size_t getSmtpSubmitInterval() const;

				/**
				 * @return The timeout for the SMTP connection in seconds
				 */
				size_t getSmtpConnectionTimeout() const;

				/**
				 * After a successful SMTP submit the connection will be kept
				 * alive the specified time to allow an immediate transmission
				 * of following bundles
				 *
				 * @return The keep alive interval of an SMTP connection in
				 *         seconds
				 */
				size_t getSmtpKeepAliveTimeout() const;

				/**
				 * @return True, if the SMTP server needs authentication before
				 *         submit
				 */
				bool smtpAuthenticationNeeded() const;

				/**
				 * @return True, if the SMTP server uses an TLS secured
				 *         connection
				 */
				bool smtpUseTLS() const;

				/**
				 * @return True, if the SMTP server uses an SSL secured
				 *         connection
				 */
				bool smtpUseSSL() const;

				/**
				 * Will return the IMAP server for this node. If no IMAP server
				 * was defined it will return "localhost"
				 *
				 * @return The IMAP server for this node
				 */
				std::string getImapServer() const;

				/**
				 * Will return the port for the IMAP server. If this option was
				 * not defined the standard IMAP port (143) will be returned
				 *
				 * @return The port of the IMAP server
				 */
				int getImapPort() const;

				/**
				 * Will return the user name for the IMAP server. If no name
				 * was specified the SMTP user name will be reused
				 *
				 * @return The user name for the IMAP server
				 */
				std::string getImapUsername() const;

				/**
				 * Will return the password for the IMAP server. If no password
				 * was specified the SMTP user name will be reused
				 *
				 * @return The password for the IMAP server
				 */
				std::string getImapPassword() const;

				/**
				 * Will return the folder on the IMAP server which will be used
				 * for the lookup for new mails. If no folder was specified
				 * the default folder defined by the IMAP server will be used
				 *
				 * @return The lookup folder on the IMAP server
				 */
				std::vector<std::string> getImapFolder() const;

				/**
				 * Will return the IMAP lookup interval. This interval defines
				 * how many seconds will be between two consecutive IMAP lookup
				 * intervals. If nothing was specified in the configuration file
				 * "60" will be returned
				 *
				 * @return The SMTP submit interval in seconds
				 */
				size_t getImapLookupInterval() const;

				/**
				 * @return The timeout for the IMAP connection in seconds
				 */
				size_t getImapConnectionTimeout() const;

				/**
				 * @return True, if the IMAP server uses an TLS secured
				 *         connection
				 */
				bool imapUseTLS() const;

				/**
				 * @return True, if the IMAP server uses an SSL secured
				 *         connection
				 */
				bool imapUseSSL() const;

				/**
				 * @return True, if a parsed email should be deleted
				 */
				bool imapPurgeMail() const;

				/**
				 * @return Returns a vector containing paths to defined
				 *         certificate authorities
				 */
				std::vector<std::string> getTlsCACerts() const;

				/**
				 * @return Returns a vector containing paths to defined
				 *         user certificates
				 */
				std::vector<std::string> getTlsUserCerts() const;

				/**
				 * @return The time in seconds for how long a discovered
				 *         node with MCL will be available
				 */
				size_t getNodeAvailableTime() const;

				/**
				 * @return Number of checks for returning mails for a bundle
				 */
				size_t getReturningMailChecks() const;
			};

			const Configuration::Discovery& getDiscovery() const;
			const Configuration::Debug& getDebug() const;
			const Configuration::Logger& getLogger() const;
			const Configuration::Network& getNetwork() const;
			const Configuration::Security& getSecurity() const;
			const Configuration::Daemon& getDaemon() const;
			const Configuration::TimeSync& getTimeSync() const;
			const Configuration::DHT& getDHT() const;
			const Configuration::P2P& getP2P() const;
			const Configuration::EMail& getEMail() const;

		private:
			ibrcommon::ConfigFile _conf;
			Configuration::Discovery _disco;
			Configuration::Debug _debug;
			Configuration::Logger _logger;
			Configuration::Network _network;
			Configuration::Security _security;
			Configuration::Daemon _daemon;
			Configuration::TimeSync _timesync;
			Configuration::DHT _dht;
			Configuration::P2P _p2p;
			Configuration::EMail _email;

			std::string _filename;
			bool _doapi;
		};
	}
}

#endif /*CONFIGURATION_H_*/
