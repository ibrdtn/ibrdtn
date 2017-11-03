/*
 * Thread.h
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

#ifndef IBRCOMMON_THREAD_H_
#define IBRCOMMON_THREAD_H_

#include <pthread.h>
#include <sys/types.h>
#include "ibrcommon/thread/Mutex.h"
#include "ibrcommon/thread/Conditional.h"
#include "ibrcommon/thread/ThreadsafeState.h"

//2 MB is a sensible default, we set this since system defaults vary widely: https://computing.llnl.gov/tutorials/pthreads/#Stack
#define DEFAULT_STACKSIZE 2*1024*1024

namespace ibrcommon
{
	class ThreadException : public ibrcommon::Exception
	{
	public:
		ThreadException(int err = 0, std::string what = "An error occured during a thread operation.") throw() : ibrcommon::Exception(what), error(err)
		{
		};

		const int error;
	};

	/**
	 * An abstract class for defining classes that operate as a thread.  A derived
	 * thread class has a run method that is invoked with the newly created
	 * thread context, and can use the derived object to store all member data
	 * that needs to be associated with that context.  This means the derived
	 * object can safely hold thread-specific data that is managed with the life
	 * of the object, rather than having to use the clumsy thread-specific data
	 * management and access functions found in thread support libraries.
	 * @author David Sugar <dyfet@gnutelephony.org>
	 */
	class Thread
	{
	protected:
		enum THREAD_STATE
		{
			THREAD_CREATED = 1 << 0,
			THREAD_STARTED = 1 << 1,
			THREAD_RUNNING = 1 << 2,
			THREAD_CANCELLED = 1 << 3,
			THREAD_FINALIZING = 1 << 4,
			THREAD_JOINABLE = 1 << 5,
			THREAD_FINALIZED = 1 << 6
		};

		ibrcommon::ThreadsafeState<THREAD_STATE> _state;

		// thread's id
		pthread_t tid;

		// thread's stack size
		size_t stack;

		// thread's attributes
		pthread_attr_t attr;

		// thread is detached
		bool _detached;

		/**
		 * Create a thread object that will have a preset stack size.  If 0
		 * is used, then the stack size is os defined/default.
		 * @param stack size to use or 0 for default.
		 */
		Thread(size_t stack = DEFAULT_STACKSIZE);

	public:
		static size_t getNumberOfProcessors();

		/**
		 * Destroy thread object, thread-specific data, and execution context.
		 */
		virtual ~Thread() = 0;

		/**
		 * Reset this thread to initial state
		 */
		void reset() throw (ThreadException);

		/**
		 * Yield execution context of the current thread. This is a static
		 * and may be used anywhere.
		 */
		static void yield(void);

		/**
		 * Sleep current thread for a specified time period.
		 * @param timeout to sleep for in milliseconds.
		 */
		static void sleep(time_t timeout);

		/**
		 * Test if thread is currently running.
		 * @return true while thread is running.
		 */
		inline bool isRunning(void)
			{ return _state == THREAD_RUNNING; };

	protected:
		/**
		 * This method is called before the run.
		 */
		virtual void setup(void) throw ()  { };

		/**
		 * Abstract interface for thread context run method.
		 */
		virtual void run(void) throw () = 0;

		/**
		 * This method is called when the run() method finishes.
		 */
		virtual void finally(void) throw () { };

		/**
		 * Set concurrency level of process.  This is essentially a portable
		 * wrapper for pthread_setconcurrency.
		 */
		static void concurrency(int level);

		/**
		 * Returns true if this thread was started and finalized before.
		 */
		inline bool isFinalized(void) throw ()
			{ return _state == THREAD_FINALIZED; };

		/**
		 * Determine if two thread identifiers refer to the same thread.
		 * @param other The thread to compare.
		 * @return True, if both threads are the same.
		 */
		inline bool operator==(const ibrcommon::Thread &other)
			{ return (equal(other.tid, tid) != 0); }

		/**
		 * Cancel the running thread context.
		 */
		void cancel() throw ();
		virtual void __cancellation() throw () = 0;

		/**
		 * Determine if two thread identifiers refer to the same thread.
		 * @param thread1 to test.
		 * @param thread2 to test.
		 * @return true if both are the same context.
		 */
		static bool equal(pthread_t thread1, pthread_t thread2);

		/**
		 * static execute thread method
		 */
		static void* __execute__(void *obj) throw ();
	};

	/**
	 * A child thread object that may be joined by parent.  A child thread is
	 * a type of thread in which the parent thread (or process main thread) can
	 * then wait for the child thread to complete and then delete the child object.
	 * The parent thread can wait for the child thread to complete either by
	 * calling join, or performing a "delete" of the derived child object.  In
	 * either case the parent thread will suspend execution until the child thread
	 * exits.
	 * @author David Sugar <dyfet@gnutelephony.org>
	 */
	class JoinableThread : public Thread
	{
	protected:
		/**
		 * Create a joinable thread with a known context stack size.
		 * @param size of stack for thread context or 0 for default.
		 */
		JoinableThread(size_t size = DEFAULT_STACKSIZE);

	public:
		/**
		 * Delete child thread.  Parent thread suspends until child thread
		 * run method completes or child thread calls it's exit method.
		 */
		virtual ~JoinableThread() = 0;

		/**
		 * Join thread with parent.  Calling from a child thread to exit is
		 * now depreciated behavior and in the future will not be supported.
		 * Threads should always return through their run() method.
		 */
		void join(void) throw (ThreadException);

		/**
		 * Start execution of child context.
		 */
		void start() throw (ThreadException);

		/**
		 * Stop the execution of child context.
		 */
		void stop() throw ();
	};

	/**
	 * A detached thread object that is stand-alone.  This object has no
	 * relationship with any other running thread instance will be automatically
	 * deleted when the running thread instance exits, either by it's run method
	 * exiting, or explicity calling the exit member function.
	 * @author David Sugar <dyfet@gnutelephony.org>
	 */
	class DetachedThread : public Thread
	{
	protected:
		/**
		 * Create a detached thread with a known context stack size.
		 * @param size of stack for thread context or 0 for default.
		 */
		DetachedThread(size_t size = DEFAULT_STACKSIZE);

	public:
		/**
		 * Destroys object when thread context exits.  Never externally
		 * deleted.  Derived object may also have destructor to clean up
		 * thread-specific member data.
		 */
		virtual ~DetachedThread() = 0;

		/**
		 * Start execution of detached context.
		 */
		void start() throw (ThreadException);

		/**
		 * Stop the execution of child context.
		 */
		void stop() throw (ThreadException);
	};
}

#endif /* THREAD_H_ */
