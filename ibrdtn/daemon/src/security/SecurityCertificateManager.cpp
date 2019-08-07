/*
 * SecurityCertificateManager.cpp
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Stephen Roettger <roettger@ibr.cs.tu-bs.de>
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

#include "security/SecurityCertificateManager.h"
#include "Configuration.h"

#include <cstring>
#include <cstdlib>

#include <ibrcommon/Logger.h>
#include <ibrcommon/ssl/TLSStream.h>

namespace dtn
{
	namespace security
	{
		const std::string SecurityCertificateManager::TAG = "SecurityCertificateManager";

		SecurityCertificateManager::SecurityCertificateManager()
			: _initialized(false), _cert(NULL), _privateKey(NULL)
		{
		}

		SecurityCertificateManager::~SecurityCertificateManager() {
		}

		bool SecurityCertificateManager::isInitialized()
		{
			return _initialized;
		}

		const X509 *SecurityCertificateManager::getCert() const
		{
			return _cert;
		}

		const EVP_PKEY *SecurityCertificateManager::getPrivateKey() const
		{
			return _privateKey;
		}

		const ibrcommon::File& SecurityCertificateManager::getTrustedCAPath() const
		{
			return _trustedCAPath;
		}

		void SecurityCertificateManager::onConfigurationChanged(const dtn::daemon::Configuration &conf) throw ()
		{
			ibrcommon::File certificate = conf.getSecurity().getCertificate();
			ibrcommon::File privateKey = conf.getSecurity().getKey();
			ibrcommon::File trustedCAPath = conf.getSecurity().getTrustedCAPath();

			try {
				FILE *fp = NULL;
				X509 *cert = NULL;
				EVP_PKEY *key = NULL;

				/* read the certificate */
				fp = fopen(certificate.getPath().c_str(), "r");
				if(!fp || !PEM_read_X509(fp, &cert, NULL, NULL)){
					IBRCOMMON_LOGGER_TAG(SecurityCertificateManager::TAG, error) << "Could not read the certificate File: " << certificate.getPath() << "." << IBRCOMMON_LOGGER_ENDL;
					if(fp){
						fclose(fp);
					}
					throw ibrcommon::IOException("Could not read the certificate.");
				}
				fclose(fp);

				/* read the private key */
				fp = fopen(privateKey.getPath().c_str(), "r");
				if(!fp || !PEM_read_PrivateKey(fp, &key, NULL, NULL)){
					IBRCOMMON_LOGGER_TAG(SecurityCertificateManager::TAG, error) << "Could not read the PrivateKey File: " << privateKey.getPath() << "." << IBRCOMMON_LOGGER_ENDL;
					if(fp){
						fclose(fp);
					}
					throw ibrcommon::IOException("Could not read the PrivateKey.");
				}
				fclose(fp);

				/* check trustedCAPath */
				if(!trustedCAPath.isDirectory()){
					IBRCOMMON_LOGGER_TAG(SecurityCertificateManager::TAG, error) << "the trustedCAPath is not valid: " << trustedCAPath.getPath() << "." << IBRCOMMON_LOGGER_ENDL;
					throw ibrcommon::IOException("Invalid trustedCAPath.");
				}

				_cert = cert;
				_privateKey = key;
				_trustedCAPath = trustedCAPath;
				_initialized = true;
			} catch (const ibrcommon::IOException &ex) {

			}
		}

		void SecurityCertificateManager::componentUp() throw ()
		{
			ibrcommon::MutexLock l(_initialization_lock);

			if (_initialized) {
				IBRCOMMON_LOGGER_TAG(SecurityCertificateManager::TAG, info) << "already initialized." << IBRCOMMON_LOGGER_ENDL;
				return;
			}
			else {
				// load configuration
				onConfigurationChanged( dtn::daemon::Configuration::getInstance() );

				ibrcommon::TLSStream::init(_cert, _privateKey, _trustedCAPath, !dtn::daemon::Configuration::getInstance().getSecurity().TLSEncryptionDisabled());

				IBRCOMMON_LOGGER_TAG(SecurityCertificateManager::TAG, info) << "Initialization succeeded." << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SecurityCertificateManager::componentDown() throw ()
		{
			/* nothing to do */
		}

		const std::string
		SecurityCertificateManager::getName() const
		{
			return SecurityCertificateManager::TAG;
		}

		void
		SecurityCertificateManager::validateSubject(X509 *certificate, const std::string &cn) throw (SecurityCertificateException)
		{
			if(!certificate || cn.empty()){
				throw SecurityCertificateException("certificate or common-name is empty");
			}

			X509_NAME *cert_name;
			X509_NAME_ENTRY *name_entry;
			ASN1_STRING *eid_string;
			int lastpos = -1;
			unsigned char *utf8_eid;
			int utf8_eid_len;

			/* retrieve the X509_NAME structure */
			if(!(cert_name = X509_get_subject_name(certificate))){
				throw SecurityCertificateException("Failed to retrieve the X509_NAME structure.");
			}

			/* convert the eid to an ASN1_STRING, it is needed later for comparison. */
			eid_string = ASN1_STRING_type_new(V_ASN1_PRINTABLESTRING);
			if(!eid_string){
				throw SecurityCertificateException("Error while creating an ASN1_STRING.");
			}
			/* TODO this function returns an int, but the return value is undocumented */
			/* the -1 indicates, that strlen() should be used to calculate the data length */
			ASN1_STRING_set(eid_string, cn.c_str(), -1);

			utf8_eid_len = ASN1_STRING_to_UTF8(&utf8_eid, eid_string);
			if(utf8_eid_len <= 0){
				std::stringstream ss; ss << "ASN1_STRING_to_UTF8() returned " << utf8_eid_len << ".";
				throw SecurityCertificateException(ss.str());
			}

			/* process all entries that are of type NID_commonName */
			while(true){
				lastpos = X509_NAME_get_index_by_NID(cert_name, NID_commonName, lastpos);
				if(lastpos == -1){
					break;
				}

				/* get the NAME_ENTRY structure */
				name_entry = X509_NAME_get_entry(cert_name, lastpos);
				if(!name_entry){
					IBRCOMMON_LOGGER_TAG(SecurityCertificateManager::TAG, error) << "X509_NAME_get_entry returned NULL unexpectedly." << IBRCOMMON_LOGGER_ENDL;
					continue;
				}

				/* retrieve the string */
				ASN1_STRING *asn1 = X509_NAME_ENTRY_get_data(name_entry);
				if(!asn1){
					IBRCOMMON_LOGGER_TAG(SecurityCertificateManager::TAG, error) << "X509_NAME_ENTRY_get_data returned NULL unexpectedly." << IBRCOMMON_LOGGER_ENDL;
					continue;
				}

				unsigned char *utf8_cert_name;
				int utf8_cert_len;
				utf8_cert_len = ASN1_STRING_to_UTF8(&utf8_cert_name, asn1);
				if(utf8_cert_len <= 0){
					IBRCOMMON_LOGGER_TAG(SecurityCertificateManager::TAG, error) << "ASN1_STRING_to_UTF8() returned " << utf8_cert_len << "." << IBRCOMMON_LOGGER_ENDL;
					continue;
				}

				/* check if the string fits the given EID */
				if(utf8_cert_len != utf8_eid_len){
					continue;
				}
				if(memcmp(utf8_eid, utf8_cert_name, utf8_eid_len) == 0){
					OPENSSL_free(utf8_cert_name);
					OPENSSL_free(utf8_eid);
					return;
				}
				OPENSSL_free(utf8_cert_name);
			}

			OPENSSL_free(utf8_eid);

			char *subject_line = X509_NAME_oneline(cert_name, NULL, 0);
			std::stringstream ss;

			if (subject_line) {
				ss << "Certificate does not fit. Expected: " << cn << ", Certificate Subject: " << subject_line << ".";
				delete subject_line;
			}

			throw SecurityCertificateException(ss.str());
		}
	}
}
