/**
 * @file HTTPConvergenceLayer.cpp
 * @date: 08.03.2011
 * @author : Robert Heitz
 *
 * This file contains methods for the HTTP Convergence Layer,
 * libcurl is used for the HTTP communication.
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Robert Heitz
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

#include "Configuration.h"
#include "net/HTTPConvergenceLayer.h"
#include "core/BundleCore.h"
#include <memory>

namespace dtn
{
	namespace net
	{
		/** Set timeout for waiting until next http request */
		const int TIMEOUT = 1000;
		/** Set timeout for waiting until connection retry */
		const int CONN_TIMEOUT = 5000;

		/** HTTP CODE OK */
		const int HTTP_OK = 200;
		/** HTTP CODE NO DATA ON SERVER */
		const int HTTP_NO_DATA = 410;

		/** CURL CODE CONN OK*/
		const int CURL_CONN_OK = 0;
		/** CURL CODE PARTIAL FILE*/
		const int CURL_PARTIAL_FILE = 18;

		/**
		 * Stream read function
		 *
		 * @param ptr
		 * @param size
		 * @param nmemb
		 * @param s
		 */
		static size_t HTTPConvergenceLayer_callback_read(void *ptr, size_t size, size_t nmemb, void *s)
		{
			size_t retcode = 0;
			std::istream *stream = static_cast<std::istream*>(s);

			if (stream->eof()) return 0;

			char *buffer = static_cast<char*>(ptr);

			stream->read(buffer, (size * nmemb));
			retcode = stream->gcount();

			return retcode;
		}

		/**
		 * Stream write function
		 *
		 * @param ptr
		 * @param size
		 * @param nmemb
		 * @param s
		 */
		static size_t HTTPConvergenceLayer_callback_write(void *ptr, size_t size, size_t nmemb, void *s)
		{
			std::ostream *stream = static_cast<std::ostream*>(s);
			char *buffer = static_cast<char*>(ptr);

			if (!stream->good()) return 0;

			stream->write(buffer, (size * nmemb));
			stream->flush();

			return (size * nmemb);
		}

		/**
		 * HTTPConvergenceLayer constructor calls the curl_global_init() method
		 * to initialize curl global. Furthermore the attributes _server _push_iob
		 * and _running will be set.
		 *
		 * @param server The server parameter contains the Tomcat-Server URL
		 */
		HTTPConvergenceLayer::HTTPConvergenceLayer(const std::string &server)
		 : _server(server), _push_iob(NULL), _running(true)
		{
			curl_global_init(CURL_GLOBAL_ALL);
		}


		/**
		 * HTTPConvergenceLayer destructor, when the destructor is calling
		 * it calls the curl_global_cleanup() method to clean curl sessions.
		 */
		HTTPConvergenceLayer::~HTTPConvergenceLayer()
		{
			curl_global_cleanup();
		}

		/**
		 * DownloadThread constructor, connects the istream _stream
		 * variable to the iobuffer and the DownloadThread::run()
		 * method is reading and evaluating the stream.
		 *
		 * @param buf Is an iobuffer where received data is written.
		 */
		DownloadThread::DownloadThread(ibrcommon::iobuffer &buf)
		 : _stream(&buf)
		{
		}

		/**
		 * DownloadThread destructor, calls the join() method to stop
		 * the joinable thread.
		 */
		DownloadThread::~DownloadThread()
		{
			join();
		}

		/**
		 * Joinable Thread run method, this thread starts when the componentRun() method
		 * is running and connected to the tomcat server. This thread is watching permanent
		 * on the istream of the ibrcommon::iobuffer. When a valid bundle is detected
		 * a BundleReceivedEvent will be raised.
		 *
		 */
		void DownloadThread::run() noexcept
		{
			try  {
				// create a filter context
				dtn::core::FilterContext context;
				context.setProtocol(dtn::core::Node::CONN_HTTP);

				while(_stream.good())
				{
					try  {
						dtn::data::Bundle bundle;
						dtn::data::DefaultDeserializer(_stream, dtn::core::BundleCore::getInstance()) >> bundle;

						// push bundle through the filter routines
						context.setBundle(bundle);
						BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::INPUT, context, bundle);

						if (ret != BundleFilter::ACCEPT) continue;

						// raise default bundle received event
						dtn::net::BundleReceivedEvent::raise(dtn::data::EID(), bundle, false);
					} catch (const ibrcommon::Exception &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG("HTTPConvergenceLayer", 10) << "http error: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}

					yield();
				}
			} catch (const ibrcommon::ThreadException &e)  {
				std::cerr << "error: " << e.what() << std::endl;
			}
		}

		void DownloadThread::__cancellation() noexcept
		{
		}

		/**
		 * Function to send data to Tomcat Server. This method is from the
		 * ConvergenceLayer interface. Everytime a bundle is queued, this method
		 * will be called to send the bundle over HTTP. The HTTP transfer is
		 * performed in classical request-response principle. For file upload the
		 * HTTP PUT method is used. Curl uses the HTTPConvergenceLayer_callback_read()
		 * method for reading the bundle content. After a bundle is successfully
		 * transfered this method will raise the events for TransferCompletedEvent and
		 * the BundleEvent for BUNDLE_FORWARDED.
		 *
		 * @param node node informations
		 * @param job parameter to get next bundle to send from storage
		 */
		void HTTPConvergenceLayer::queue(const dtn::core::Node &node, const dtn::net::BundleTransfer &job)
		{
			dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();
			std::string url_send;

			long http_code = 0;
			//double upload_size = 0;

			// create a filter context
			dtn::core::FilterContext context;
			context.setProtocol(dtn::core::Node::CONN_HTTP);

			try {
				// read the bundle out of the storage
				dtn::data::Bundle bundle = storage.get(job.getBundle());

				// push bundle through the filter routines
				context.setBundle(bundle);
				BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::OUTPUT, context, bundle);

				if (ret != BundleFilter::ACCEPT) {
					dtn::net::BundleTransfer local_job = job;
					local_job.abort(dtn::net::TransferAbortedEvent::REASON_REFUSED_BY_FILTER);
					return;
				}

				ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
				{
					ibrcommon::BLOB::iostream io = ref.iostream();
					dtn::data::DefaultSerializer(*io) << bundle;
				}

				ibrcommon::BLOB::iostream io = ref.iostream();
				size_t length = io.size();
				CURLcode res;
				CURL *curl_up;

				url_send = _server + "?eid=" + dtn::core::BundleCore::local.getString();
										 //+ "&dst-eid=dtn://experthe-laptop/filetransfer";
										 //+ "&priority=2";
										 //+ "&ttl=3600";

				//if (job._bundle.source.getString() == (dtn::core::BundleCore::local.getString() + "/echo-client"))
				//{
				//	url_send = _server + "?eid=" + dtn::core::BundleCore::local.getString() + "&dst-eid=echo-client";
				//}

				curl_up = curl_easy_init();
				if(curl_up)
				{
					/* we want to use our own read function */
					curl_easy_setopt(curl_up, CURLOPT_READFUNCTION, HTTPConvergenceLayer_callback_read);

					/* enable uploading */
					curl_easy_setopt(curl_up, CURLOPT_UPLOAD, 1L);

					/* HTTP PUT please */
					curl_easy_setopt(curl_up, CURLOPT_PUT, 1L);

					/* disable connection timeout */
					curl_easy_setopt(curl_up, CURLOPT_CONNECTTIMEOUT, 0);

					/* specify target URL, and note that this URL should include a file
					   name, not only a directory */
					curl_easy_setopt(curl_up, CURLOPT_URL, url_send.c_str());

					/* now specify which file to upload */
					curl_easy_setopt(curl_up, CURLOPT_READDATA, &(*io));

					/* provide the size of the upload, we specicially typecast the value
					   to curl_off_t since we must be sure to use the correct data size */
					curl_easy_setopt(curl_up, CURLOPT_INFILESIZE_LARGE,
									 (curl_off_t)length);

					/* Now run off and do what you've been told! */
					res = curl_easy_perform(curl_up);

					if(res == CURL_CONN_OK)
					{
						/* get HTTP Header StatusCode */
						curl_easy_getinfo (curl_up, CURLINFO_RESPONSE_CODE, &http_code);
						//curl_easy_getinfo (curl, CURLINFO_SIZE_UPLOAD, &upload_size);

						/* DEBUG OUTPUT INFORMATION */
						//std::cout << "CURL CODE    : " << res << std::endl;
						//std::cout << "HTTP CODE    : " << http_code << std::endl;
						//std::cout << "UPLOAD_SIZE: " << upload_size << " Bytes" << std::endl;
						/* DEBUG OUTPUT INFORMATION */

						if(http_code == HTTP_OK)
						{
							dtn::net::BundleTransfer local_job = job;
							local_job.complete();
						}
					}

					curl_easy_cleanup(curl_up);
				}
			} catch (const dtn::storage::NoBundleFoundException&) {
				// send transfer aborted event
				dtn::net::BundleTransfer local_job = job;
				local_job.abort(dtn::net::TransferAbortedEvent::REASON_BUNDLE_DELETED);
			}

		}

		/**
		 * Method to return protocol type, in this case HTTP.
		 */
		dtn::core::Node::Protocol HTTPConvergenceLayer::getDiscoveryProtocol() const
		{
			return dtn::core::Node::CONN_HTTP;
		}

		/**
		 * Method from IndependentComponent interface, this method is called before
		 * the componentRun() method is called. At time here is nothing to do.
		 */
		void HTTPConvergenceLayer::componentUp() noexcept
		{
			// routine checked for noexcept on 15.02.2013
		}

		/**
		 * This method is from IndependentComponent interface. It runs as independent
		 * thread. This method is for receving data vom HTTP server. For the receiving
		 * the long polling mechanism is used. Curl calls when receiving data the
		 * HTTPConvergenceLayer_callback_write() method. There the received data is written
		 * in the ibrcommon::iobuffer, where the DownloadThread::run() method evaluate
		 * the stream. If the connection aborted, it will wait the CONN_TIMEOUT value
		 * and retry to connect until it could connect. When the connection is set up the
		 * DownloadThread the iobuffer will be initialize, when disconnecting the iobuffer
		 * will finalize.
		 */
		void HTTPConvergenceLayer::componentRun() noexcept
		{

			std::string url = _server + "?eid=" + dtn::core::BundleCore::local.getString();

			CURL *curl_down;

			while (_running)
			{
				curl_down = curl_easy_init();

				while(curl_down)
				{
					curl_easy_setopt(curl_down, CURLOPT_URL, url.c_str());

					/* disable connection timeout */
					curl_easy_setopt(curl_down, CURLOPT_CONNECTTIMEOUT, 0);

					/* no progress meter please */
					curl_easy_setopt(curl_down, CURLOPT_NOPROGRESS, 1L);

					/* cURL DEBUG options */
					//curl_easy_setopt(curl_down, CURLOPT_DEBUGFUNCTION, my_trace);
				    //curl_easy_setopt(curl_down, CURLOPT_DEBUGDATA, &config);

				    //curl_easy_setopt(curl_down, CURLOPT_DEBUGFUNCTION, my_trace);
					//curl_easy_setopt(curl_down, CURLOPT_VERBOSE, 1);

					// create a receiver thread
					{
						ibrcommon::MutexLock l(_push_iob_mutex);
						_push_iob = new ibrcommon::iobuffer();
					}

					std::auto_ptr<ibrcommon::iobuffer> auto_kill(_push_iob);
					std::ostream os(_push_iob);
					DownloadThread receiver(*_push_iob);

					/* send all data to this function  */
					curl_easy_setopt(curl_down, CURLOPT_WRITEFUNCTION, HTTPConvergenceLayer_callback_write);

					/* now specify where to write data */
					curl_easy_setopt(curl_down, CURLOPT_WRITEDATA, &os);

					/* do curl */
					receiver.start();
					curl_easy_perform(curl_down);

					{
						ibrcommon::MutexLock l(_push_iob_mutex);
						/* finalize iobuffer */
						_push_iob->finalize();
						receiver.join();
						_push_iob = NULL;
					}

					/* get HTTP Header StatusCode */
					//curl_easy_getinfo (curl_down, CURLINFO_RESPONSE_CODE, &http_code);
					//curl_easy_getinfo (curl, CURLINFO_SIZE_DOWNLOAD, &download_size);
					//curl_easy_getinfo (curl, CURLINFO_NUM_CONNECTS, &connects);

					/* Wait some time an retry to connect */
					ibrcommon::Thread::sleep(CONN_TIMEOUT);  // Wenn Verbindung nicht hergestellt werden konnte warte 5 sec.
					IBRCOMMON_LOGGER_DEBUG_TAG("HTTPConvergenceLayer", 10) << "http error: " << "Couldn't connect to server ... wait " << CONN_TIMEOUT/1000 << "s until retry" << IBRCOMMON_LOGGER_ENDL;
				}

				/* always cleanup */
				curl_easy_cleanup(curl_down);
			}
			yield();
		}

		/**
		 * This method is from IndependentComponent interface and is called when
		 * the IBR-DTN is shutting down. This method trys to stop the DownloadThread
		 * and to finalize the iobuffer, for a clean shut down.
		 */
		void HTTPConvergenceLayer::componentDown() noexcept
		{
		}

		/**
		 * This method is from IndependentComponent interface. It could abort
		 * the componentRun() thread on the hard way.
		 */
		void HTTPConvergenceLayer::__cancellation() noexcept
		{
			_running = false;

			ibrcommon::MutexLock l(_push_iob_mutex);
			if (_push_iob != NULL)
			{
				_push_iob->finalize();
				_push_iob = NULL;
			}
		}

		/**
		 * Get method, returns a string with the name of this class, in this
		 * case "HTTPConvergenceLayer"
		 */
		const std::string HTTPConvergenceLayer::getName() const
		{
			return "HTTPConvergenceLayer";
		}
	}
}
