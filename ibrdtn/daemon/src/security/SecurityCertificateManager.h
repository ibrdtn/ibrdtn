/*
 * SecurityCertificateManager.h
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

#ifndef SECURITYCERTIFICATEMANAGER_H_
#define SECURITYCERTIFICATEMANAGER_H_

#include "Configuration.h"
#include "core/Event.h"
#include "Component.h"

#include <ibrcommon/data/File.h>
#include <ibrcommon/thread/Mutex.h>

#include <ibrdtn/data/EID.h>

#include <openssl/ssl.h>
#include <string>

namespace dtn
{
	namespace security
	{
		class SecurityCertificateException : public ibrcommon::Exception
		{
		public:
			SecurityCertificateException(std::string what = "verification failed") : ibrcommon::Exception(what)
			{};

			virtual ~SecurityCertificateException() noexcept {};
		};

		/*!
		 * \brief This class is a manager to handle certificates
		 */
		class SecurityCertificateManager : public dtn::daemon::IntegratedComponent, public dtn::daemon::Configuration::OnChangeListener
		{
			static const std::string TAG;

		public:
			SecurityCertificateManager();
			virtual ~SecurityCertificateManager();

			/**
			 * Listen for changes of the configuration
			 */
			virtual void onConfigurationChanged(const dtn::daemon::Configuration &conf) noexcept;

			/*!
			 * \brief Validates if the CommonName in the given X509 certificate corresponds to the given EID
			 * \param certificate The Certificate.
			 * \param eid The EID of the sender.
			 * \return returns true if the EID fits, false otherwise
			 */
			static void validateSubject(X509 *certificate, const std::string &cn) noexcept (false);

			/*!
			 * \brief checks if this class has already been initialized with a certificate and private key
			 * \return true if it is initialized, false otherwise
			 */
			bool isInitialized();

			/*!
			 * \brief retrieve the saved certificate
			 * \return The certificate.
			 * \warning Check isInitialized() first, before calling this function
			 */
			const X509 *getCert() const;
			/*!
			 * \brief retrieve the saved private key
			 * \return The private key as an EVP_PKEY pointer (OpenSSL).
			 * \warning Check isInitialized() first, before calling this function
			 */
			const EVP_PKEY *getPrivateKey() const;
			/*!
			 * \brief retrieve the saved directory holding trusted certificates
			 * \return The directory
			 */
			const ibrcommon::File& getTrustedCAPath() const;

			/* functions from IntegratedComponent */
			virtual void componentUp() noexcept;
			virtual void componentDown() noexcept;

			virtual const std::string getName() const;

		private:
			ibrcommon::Mutex _initialization_lock;
			bool _initialized;

			X509 *_cert;
			EVP_PKEY *_privateKey;
			ibrcommon::File _trustedCAPath;
		};
	}
}

#endif /* SECURITYCERTIFICATEMANAGER_H_ */
