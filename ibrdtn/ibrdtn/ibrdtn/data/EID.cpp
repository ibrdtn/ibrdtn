/*
 * EID.cpp
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

#include "ibrdtn/config.h"
#include "ibrdtn/data/EID.h"
#include "ibrdtn/utils/Utils.h"
#include <sstream>
#include <iostream>

#include <ibrcommon/ibrcommon.h>
#ifdef IBRCOMMON_SUPPORT_SSL
#include <ibrcommon/ssl/MD5Stream.h>
#endif

// include code for platform-independent endianess conversion
#include "ibrdtn/data/Endianess.h"

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

namespace dtn
{
	namespace data
	{
		// static initialization of the CBHE map
		EID::cbhe_map& EID::getApplicationMap()
		{
			static cbhe_map app_map;

			// initialize the application map
			if (app_map.empty()) {
				app_map["null"] = 1;
				app_map["debugger"] = 2;
				app_map["bundle-in-bundle"] = 5;
				app_map["echo"] = 11;
				app_map["routing"] = 50;
				app_map["dtntp"] = 60;
			}

			return app_map;
		}

		const std::string EID::getSchemeName(const Scheme s)
		{
			switch (s) {
			case SCHEME_DTN:
				return "dtn";
			case SCHEME_CBHE:
				return "ipn";
			default:
				return "";
			}
		}

		EID::Scheme EID::resolveScheme(const std::string &s)
		{
			if ("dtn" == s) {
				return SCHEME_DTN;
			}
			else if ("ipn" == s) {
				return SCHEME_CBHE;
			}
			else {
				return SCHEME_EXTENDED;
			}
		}

		void EID::extractCBHE(const std::string &ssp, Number &node, Number &app)
		{
			char delimiter = '\0';

			size_t n = 0;
			size_t a = 0;

			std::stringstream ss(ssp);
			ss >> n;
			node = n;

			ss.get(delimiter);

			if (delimiter == '.') {
				ss >> a;
				app = a;
			} else {
				app = 0;
			}
		}

		void EID::extractDTN(const std::string &ssp, std::string &node, std::string &application)
		{
			size_t first_char = 0;
			const char delimiter = '/';

			// first char not "/", e.g. "//node1" -> 2
			first_char = ssp.find_first_not_of(delimiter);

			// only "/" ? thats bad!
			if (first_char == std::string::npos) {
				node = "none";
				application = "";
				return;
			}

			// start of application part
			size_t application_start = ssp.find_first_of(delimiter, first_char);

			// no application part available
			if (application_start == std::string::npos) {
				node = ssp;
				application = "";
				return;
			}

			// set the node part
			node = ssp.substr(0, application_start);

			// set the application part
			application = ssp.substr(application_start + 1, ssp.length() - application_start - 1);
		}

		Number EID::getApplicationNumber(const std::string &app)
		{
			// an empty string returns zero
			if (app.empty()) return 0;

			// check if the string is a number
			std::string::const_iterator char_it = app.begin();
			while (char_it != app.end() && std::isdigit(*char_it)) ++char_it;

			if (char_it == app.end()) {
				// the string contains only digits
				Number ret;

				// convert the string into a number
				std::stringstream ss(app);
				ret.read(ss);

				return ret;
			}

			// ask the well-known numbers
			const cbhe_map &m = getApplicationMap();

			// search for the application in the map
			cbhe_map::const_iterator it = m.find(app);

			// if there is a standard mapping
			if (it != m.end()) {
				// return the standard number
				return (*it).second;
			}

			// there is no standard mapping, hash required
#ifdef IBRCOMMON_SUPPORT_SSL
			ibrcommon::MD5Stream md5;
			md5 << app << std::flush;

			// use 4 byte as integer
			uint32_t hash = 0;
			md5.read((char*)&hash, sizeof(uint32_t));

			// set the highest bit
			return Number(0x80000000 | GUINT32_TO_BE(hash));
#else
			return 0;
#endif
		}

		EID::EID()
		: _scheme_type(SCHEME_DTN), _scheme(), _ssp("none"), _application(), _cbhe_node(0), _cbhe_application(0), _regex(NULL)
		{
		}

		EID::EID(const Scheme scheme_type, const std::string &scheme, const std::string &ssp, const std::string &application)
		: _scheme_type(scheme_type), _scheme(scheme), _ssp(ssp), _application(application), _cbhe_node(0), _cbhe_application(0), _regex(NULL)
		{
			if (scheme_type == SCHEME_CBHE) {
				throw dtn::InvalidDataException("This constructor does not work for CBHE schemes");
			}
		}

		EID::EID(const std::string &scheme, const std::string &ssp)
		 : _scheme_type(SCHEME_EXTENDED), _scheme(), _ssp(ssp), _application(), _cbhe_node(0), _cbhe_application(0), _regex(NULL)
		{
			// resolve scheme
			_scheme_type = resolveScheme(scheme);

			switch (_scheme_type) {
			case SCHEME_CBHE:
				// extract CBHE numbers
				extractCBHE(ssp, _cbhe_node, _cbhe_application);
				if (_cbhe_node == 0) {
					_scheme_type = SCHEME_DTN;
					_ssp = "none";
				}
				break;

			case SCHEME_DTN:
				extractDTN(ssp, _ssp, _application);
				break;

			default:
				_scheme = scheme;
				break;
			}
		}

		EID::EID(const std::string &orig_value)
		: _scheme_type(SCHEME_DTN), _scheme(), _ssp("none"), _application(), _cbhe_node(0), _cbhe_application(0), _regex(NULL)
		{
			try {
				if (orig_value.length() == 0) {
					throw dtn::InvalidDataException("given EID is empty!");
				}

				std::string value = orig_value;
				dtn::utils::Utils::trim(value);

				// search for the delimiter
				const size_t delimiter = value.find_first_of(":");

				// jump to default eid if the format is wrong
				if (delimiter == std::string::npos)
					throw dtn::InvalidDataException("wrong EID format: " + value);

				// the scheme is everything before the delimiter
				const std::string scheme = value.substr(0, delimiter);

				// the ssp is everything else
				const size_t startofssp = delimiter + 1;
				const std::string ssp = value.substr(startofssp, value.length() - delimiter + 1);

				// do syntax check
				if (scheme.length() == 0) {
					throw dtn::InvalidDataException("scheme is empty!");
				}

				if (ssp.length() == 0) {
					throw dtn::InvalidDataException("SSP is empty!");
				}

				// resolve scheme
				_scheme_type = resolveScheme(scheme);

				switch (_scheme_type) {
				case SCHEME_CBHE:
					// extract CBHE numbers
					extractCBHE(ssp, _cbhe_node, _cbhe_application);
					break;

				case SCHEME_DTN:
					// extract DTN scheme node/application
					extractDTN(ssp, _ssp, _application);
					break;

				default:
					_scheme = scheme;
					_ssp = ssp;
					break;
				}
			} catch (const std::exception&) {
				_scheme_type = SCHEME_DTN;
				_ssp = "none";
			}
		}

		EID::EID(const dtn::data::Number &node, const dtn::data::Number &application)
		 : _scheme_type(SCHEME_CBHE), _scheme(), _ssp(), _application(), _cbhe_node(node), _cbhe_application(application), _regex(NULL)
		{
			// set dtn:none if the node is zero
			if (node == 0) {
				_scheme_type = SCHEME_DTN;
				_ssp = "none";
			}
		}

		EID::EID(const EID &other)
		{
			_scheme_type = other._scheme_type;
			_scheme = other._scheme;
			_ssp = other._ssp;

			_application = other._application;

			_cbhe_node = other._cbhe_node;
			_cbhe_application = other._cbhe_application;

			_regex = NULL;

			if(other._regex != NULL)
			{
				prepare();
			}
		}

		EID::~EID()
		{
#ifdef HAVE_REGEX_H
			if (_regex != NULL) {
				regfree((regex_t*)_regex);
				delete (regex_t*)_regex;
				_regex = NULL;
			}
#endif
		}

		bool EID::operator==(const EID &other) const
		{
			if (_scheme_type != other._scheme_type) return false;

			switch (_scheme_type) {
			case SCHEME_CBHE:
				return (_cbhe_node == other._cbhe_node) && (_cbhe_application == other._cbhe_application);

			case SCHEME_DTN:
				return (_ssp == other._ssp) && (_application == other._application);

			default:
				return (_scheme == other._scheme) && (_ssp == other._ssp);
			}
		}

		bool EID::operator==(const std::string &other) const
		{
			return ((*this) == EID(other));
		}

		bool EID::operator!=(const EID &other) const
		{
			return !((*this) == other);
		}

		bool EID::sameHost(const std::string& other) const
		{
			return sameHost( EID(other) );
		}

		bool EID::sameHost(const EID &other) const
		{
			if (_scheme_type != other._scheme_type) return false;

			switch (_scheme_type) {
			case SCHEME_CBHE:
				return _cbhe_node == other._cbhe_node;

			case SCHEME_DTN:
				return _ssp == other._ssp;

			default:
				return (_scheme == other._scheme) && (_ssp == other._ssp);
			}
		}

		bool EID::operator<(const EID &other) const
		{
			if (_scheme_type < other._scheme_type) return true;
			if (_scheme_type != other._scheme_type) return false;

			switch (_scheme_type) {
			case SCHEME_CBHE:
				if (_cbhe_node < other._cbhe_node) return true;
				if (_cbhe_node != other._cbhe_node) return false;

				return (_cbhe_application < other._cbhe_application);

			case SCHEME_DTN:
				if (_ssp < other._ssp) return true;
				if (_ssp != other._ssp) return false;

				return (_application < other._application);

			default:
				if (_scheme < other._scheme) return true;
				if (_scheme != other._scheme) return false;

				return (_ssp < other._ssp);
			}
		}

		bool EID::operator>(const EID &other) const
		{
			return other < (*this);
		}

		std::string EID::getString() const
		{
			std::stringstream ss;

			switch (_scheme_type) {
			case SCHEME_CBHE:
				ss << getSchemeName(SCHEME_CBHE) << ":" << _cbhe_node.get<size_t>();
				ss << "." << _cbhe_application.get<size_t>();
				break;

			case SCHEME_DTN:
				ss << getSchemeName(SCHEME_DTN) << ":" << _ssp;

				if (_application.length() > 0) {
					ss << "/" << _application;
				}
				break;

			default:
				ss << _scheme << ":" << _ssp;
				break;
			}

			return ss.str();
		}

		void EID::setApplication(const Number &app) throw ()
		{
			switch (_scheme_type) {
			case SCHEME_CBHE:
				_cbhe_application = app;
				break;

			case SCHEME_DTN:
				_application = app.toString();
				break;

			default:
				// not defined
				break;
			}

#ifdef HAVE_REGEX_H
			if (_regex != NULL) {
				regfree((regex_t*)_regex);
				delete (regex_t*)_regex;
				_regex = NULL;
			}
#endif
		}

		void EID::setApplication(const std::string &app) throw ()
		{
			switch (_scheme_type) {
			case SCHEME_CBHE:
				// get CBHE Number for the application string
				_cbhe_application = EID::getApplicationNumber(app);
				break;

			case SCHEME_DTN:
				_application = app;
				break;

			default:
				// not defined
				break;
			}

#ifdef HAVE_REGEX_H
			if (_regex != NULL) {
				regfree((regex_t*)_regex);
				delete (regex_t*)_regex;
				_regex = NULL;
			}
#endif
		}

		std::string EID::getApplication() const throw ()
		{
			switch (_scheme_type) {
			case SCHEME_CBHE:
				if (_cbhe_application > 0) {
					return _cbhe_application.toString();
				}
				return "";

			case SCHEME_DTN:
				return _application;

			default:
				return _ssp;
			}
		}

		bool EID::isApplication(const dtn::data::Number &app) const throw ()
		{
			if (_scheme_type != SCHEME_CBHE) return false;
			return (_cbhe_application == app);
		}

		bool EID::isApplication(const std::string &app) const throw ()
		{
			switch (_scheme_type) {
			case SCHEME_CBHE:
				return (_cbhe_application == getApplicationNumber(app));

			case SCHEME_DTN:
				return (_application == app);

			default:
				return (app == _ssp);
			}
		}

		std::string EID::getHost() const throw ()
		{
			switch (_scheme_type) {
			case SCHEME_CBHE:
				return _cbhe_node.toString();
			case SCHEME_DTN:
				return _ssp;
			default:
				return _ssp;
			}
		}

		const std::string EID::getScheme() const
		{
			switch (_scheme_type) {
			case SCHEME_CBHE:
				return getSchemeName(SCHEME_CBHE);
			case SCHEME_DTN:
				return getSchemeName(SCHEME_DTN);
			default:
				return _scheme;
			}
		}

		const std::string EID::getSSP() const
		{
			switch (_scheme_type) {
			case SCHEME_CBHE:
			{
				std::stringstream ss;
				ss << _cbhe_node.get<size_t>();
				ss << "." << _cbhe_application.get<size_t>();

				return ss.str();
			}

			case SCHEME_DTN:
				if (_application.length() > 0) {
					std::stringstream ss;
					ss << _ssp << "/" << _application;
					return ss.str();
				} else {
					return _ssp;
				}

			default:
				return _ssp;
			}
		}

		EID EID::getNode() const throw ()
		{
			switch (_scheme_type) {
			case SCHEME_CBHE:
				return EID(_cbhe_node, 0);
			case SCHEME_DTN:
				return EID(_scheme_type, "", _ssp, "");
			default:
				return EID(_scheme_type, _scheme, _ssp, "");
			}
		}

		bool EID::hasApplication() const
		{
			switch (_scheme_type) {
			case SCHEME_CBHE:
				return (_cbhe_application > 0);
			case SCHEME_DTN:
				return _application != "";
			default:
				return true;
			}
		}

		bool EID::isCompressable() const
		{
			return ((_scheme_type == SCHEME_CBHE) || ((_scheme_type == SCHEME_DTN) && (_ssp == "none")));
		}

		bool EID::isNone() const
		{
			return (_scheme_type == SCHEME_DTN) && (_ssp == "none");
		}

		std::string EID::getDelimiter() const
		{
			if (_scheme_type == EID::SCHEME_CBHE) {
				return ".";
			} else {
				return "/";
			}
		}

		EID::Compressed EID::getCompressed() const
		{
			if (isCompressable())
			{
				return std::make_pair(_cbhe_node, _cbhe_application);
			}

			return std::make_pair(0, 0);
		}

		void EID::prepare() throw (ibrcommon::Exception)
		{
#ifdef HAVE_REGEX_H
			if (_regex != NULL) return;
			_regex = new regex_t();
			const std::string data = std::string("^") + getString() + std::string("$");
			if ( regcomp((regex_t*)_regex, data.c_str(), 0) ) {
				delete (regex_t*)_regex;
				_regex = NULL;
				throw ibrcommon::Exception("Invalid expression");
			}
#endif
		}

		bool EID::match(const dtn::data::EID &other) const
		{
#ifdef HAVE_REGEX_H
			if (_regex != NULL) {
				const std::string data = other.getString();

				// test against the regular expression
				return regexec((regex_t*)_regex, data.c_str(), 0, NULL, 0) == 0;
			}
#endif
			return (*this) == other;
		}
	}
}
