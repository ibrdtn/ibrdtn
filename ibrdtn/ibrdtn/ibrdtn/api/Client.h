/*
 * Client.h
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

#ifndef CLIENT_H_
#define CLIENT_H_

#include "ibrdtn/data/Number.h"
#include "ibrdtn/data/Bundle.h"
#include "ibrdtn/streams/StreamConnection.h"
#include <ibrcommon/net/socketstream.h>
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/Exceptions.h>
#include <ibrcommon/thread/Queue.h>

namespace dtn
{
	namespace api
	{
		/**
		 * This exception should be thrown on an less defined connection error.
		 */
		class ConnectionException : public ibrcommon::Exception
		{
		public:
			ConnectionException(std::string what = "A connection error occurred.") throw() : ibrcommon::Exception(what)
			{
			};
		};

		/**
		 * This exception should be thrown when an timeout occurred on the connection.
		 */
		class ConnectionTimeoutException : public ConnectionException
		{
		public:
			ConnectionTimeoutException(std::string what = "Timeout.") throw() : ConnectionException(what)
			{
			};
		};

		/**
		 * This exception should be thrown when the connection is aborted.
		 */
		class ConnectionAbortedException : public ConnectionException
		{
		public:
			ConnectionAbortedException(std::string what = "Aborted.") throw() : ConnectionException(what)
			{
			};
		};

		/**
		 * This is an abstract class is the base for any API connection to a
		 * IBR-DTN daemon. It uses an existing I/O stream to communicate bidirectional
		 * with the daemon.
		 *
		 * For asynchronous reception of bundle this class contains a thread which deals the
		 * receiving part of the communication and calls the received() methods which should be
		 * overwritten.
		 */
		class Client : public dtn::streams::StreamConnection, public dtn::streams::StreamConnection::Callback
		{
		private:
			/**
			 * This sub-class implements the asynchronous receiver for the connection.
			 * In the run routine a blocking read is called and tries to deserialize the
			 * incoming data into bundle objects. Each time a bundle is received the
			 * received(const dtn::api::Bundle&) method is called and signals the
			 * derived class an incoming bundle.
			 */
			class AsyncReceiver : public ibrcommon::JoinableThread
			{
			public:
				/**
				 * Constructor for the synchronous receiver class. It requires the reference to
				 * the Client class for calling the received() methods.
				 * @param client Reference to the client object.
				 */
				AsyncReceiver(Client &client);

				/**
				 * Destructor of the asynchronous receiver. It do a join() call on the
				 * JoinableThread object and thus waits for the end of the run() method.
				 */
				virtual ~AsyncReceiver();

			protected:
				/**
				 * The run routine try to retrieve bundles from the stream continuously.
				 * It aborts if the stream of the client went bad or an error occurred during
				 * the deserialization.
				 */
				void run() throw ();

				/**
				 * Aborts the receiver thread
				 * @return
				 */
				void __cancellation() throw ();

			private:
				// member variable for the reference to the client object
				Client &_client;

				bool _running;
			};

		public:
			/**
			 * This are the communication flags.
			 */
			enum COMMUNICATION_MODE
			{
				MODE_BIDIRECTIONAL = 0, 	//!< bidirectional communication is requested
				MODE_SENDONLY = 1 			//!< unidirectional communication is requested, no reception of bundles
			};

			/**
			 * Constructor for the API Connection. At least a application suffix
			 * and an existing tcp stream are required. The suffix is appended to the node
			 * id of the daemon. E.g. dtn://<node-id>/example (in this case is "example" the
			 * application id. The stream connects the daemon and this application together
			 * and will be used with the bundle protocol for TCP (draft-irtf-dtnrg-tcp-clayer-02)
			 * provided by the StreamConnection class.
			 * @param app Application suffix.
			 * @param stream TCP stream object.
			 * @param mode Communication mode. Default is bidirectional communication.
			 */
			Client(const std::string &app, ibrcommon::socketstream &stream, const COMMUNICATION_MODE mode = MODE_BIDIRECTIONAL);
			Client(const std::string &app, const dtn::data::EID &group, ibrcommon::socketstream &stream, const COMMUNICATION_MODE mode = MODE_BIDIRECTIONAL);

			/**
			 * Virtual destructor for this class.
			 */
			virtual ~Client();

			/**
			 * This method starts the thread and execute the handshake with the server.
			 */
			void connect();

			/**
			 * Closes the client. Actually, this send out a SHUTDOWN message to the daemon.
			 * The connection itself has to be closed separately.
			 */
			void close();

			/**
			 * Aborts blocking calls of getBundle()
			 */
			void abort();

			/**
			 * The connection down event is called by the StreamConnection object and
			 * aborts the blocking getBundle() method. If a client is working synchonous
			 * this method should not be overloaded!
			 */
			virtual void eventConnectionDown() throw ();

			/**
			 * The bundle ack event is called by the StreamConnection object and stores
			 * the last ACK'd bundle size in the lastack variable.
			 * @param ack ACK'd bundle size
			 */
			virtual void eventBundleAck(const dtn::data::Length &ack) throw ();

			/**
			 * The shutdown event callback method can overloaded to handle shutdown
			 * events.
			 */
			virtual void eventShutdown(StreamConnection::ConnectionShutdownCases) throw () {};

			/**
			 * The timeout event callback method can overloaded to handle timeouts
			 * occurring in the API protocol.
			 */
			virtual void eventTimeout() throw () {};

			/**
			 * The error event callback method can overloaded to handle errors
			 * occurring in the API protocol.
			 */
			virtual void eventError() throw () {};

			/**
			 * The connection up event callback method can overloaded to handle
			 * a successful connection handshake. In this call the header of the
			 * corresponding daemon is available.
			 */
			virtual void eventConnectionUp(const dtn::streams::StreamContactHeader&) throw () {};

			/**
			 * The bundle refused event callback method can overloaded to handle
			 * a bundle refused by the porresponding daemon.
			 */
			virtual void eventBundleRefused() throw () {};

			/**
			 * The bundle forwarded event callback method can overloaded to determine
			 * when a bundle is forwarded to the daemon.
			 */
			virtual void eventBundleForwarded() throw () {};

			/**
			 * Send a bundle to the daemon
			 */
			void operator<<(const dtn::data::Bundle &b);

			/**
			 * This method is for synchronous API usage only. It blocks until a bundle
			 * is received and return it. If the connection is closed during the get() call
			 * an exception is thrown
			 * @param timeout
			 * @return
			 */
			dtn::data::Bundle getBundle(const dtn::data::Timeout timeout = 0) throw (ConnectionException);

			// public variable
			dtn::data::Length lastack;

		protected:
			/**
			 * This method is called on the receipt of the handshake of the daemon. If
			 * you like to validate your connection you could overload this method, but must
			 * call the super method.
			 */
			virtual void received(const dtn::streams::StreamContactHeader&) {};

			/**
			 * This method is called on the receipt of a new bundle. If you like to use
			 * asynchronous API mode you should overload this method to receive bundles.
			 * @param b The received bundle.
			 */
			virtual void received(const dtn::data::Bundle &b);

		private:
			// tcp stream reference to send/receive data to the daemon
			ibrcommon::socketstream &_stream;

			// communication mode flags
			COMMUNICATION_MODE _mode;

			// own application suffix
			std::string _app;

			// group to join
			dtn::data::EID _group;

			// The asynchronous receiver thread which receives incoming bundles
			Client::AsyncReceiver _receiver;

			// the queue for incoming bundles, when used in synchronous mode
			ibrcommon::Queue<dtn::data::Bundle> _inqueue;
		};
	}
}

#endif /* CLIENT_H_ */
