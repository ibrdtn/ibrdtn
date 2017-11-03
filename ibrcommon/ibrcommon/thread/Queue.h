/*
 * Queue.h
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

#ifndef IBRCOMMON_QUEUE_H_
#define IBRCOMMON_QUEUE_H_

#include "ibrcommon/thread/MutexLock.h"
#include "ibrcommon/thread/Conditional.h"
#include "ibrcommon/Exceptions.h"
#include "ibrcommon/thread/Semaphore.h"
#include "ibrcommon/thread/Thread.h"
#include <queue>
#include <iostream>

namespace ibrcommon
{
	class QueueUnblockedException : public ibrcommon::Exception
	{
	public:
		enum type_t
		{
			QUEUE_ABORT = 0,
			QUEUE_ERROR = 1,
			QUEUE_TIMEOUT = 2
		};

		QueueUnblockedException(const type_t r, std::string what = "Queue is unblocked.") throw() : ibrcommon::Exception(what), reason(r)
		{
		};

		QueueUnblockedException(const ibrcommon::Conditional::ConditionalAbortException &ex, std::string what = "Queue is unblocked.") throw() : ibrcommon::Exception(what)
		{
			switch (ex.reason)
			{
			case ibrcommon::Conditional::ConditionalAbortException::COND_ABORT:
				reason = QUEUE_ABORT;
				_what = "queue function aborted in " + what;
				break;

			case ibrcommon::Conditional::ConditionalAbortException::COND_ERROR:
				reason = QUEUE_ERROR;
				_what = "queue function error in " + what;
				break;

			case ibrcommon::Conditional::ConditionalAbortException::COND_TIMEOUT:
				reason = QUEUE_TIMEOUT;
				_what = "queue function timeout in " + what;
				break;
			}
		};

		type_t reason;
	};

	template <class T>
	class Queue
	{
		ibrcommon::Conditional _cond;
		std::queue<T> _queue;
		ibrcommon::Semaphore _sem;
		bool _limit;

	public:
		Queue(unsigned int max = 0) : _sem(max), _limit(max > 0)
		{};

		virtual ~Queue()
		{
			abort();
		};

		/* Test whether container is empty (public member function) */
		bool empty ( )
		{
			ibrcommon::MutexLock l(_cond);
			return _queue.empty();
		}

		/* Return size (public member function) */
		size_t size ( ) const
		{
			return _queue.size();
		}

		/* Access next element (public member function) */
		T& front ( )
		{
			ibrcommon::MutexLock l(_cond);
			return _queue.front();
		}

		const T& front ( ) const
		{
			ibrcommon::MutexLock l(_cond);
			return _queue.front();
		}

		/* Access last element (public member function) */
		T& back ( )
		{
			ibrcommon::MutexLock l(_cond);
			return _queue.back();
		}

		const T& back ( ) const
		{
			ibrcommon::MutexLock l(_cond);
			return _queue.back();
		}

		/* Insert element (public member function) */
		void push ( const T& x )
		{
			if (_limit) _sem.wait();

			ibrcommon::MutexLock l(_cond);
			_queue.push(x);
			_cond.signal(true);
		}

		/* Delete next element (public member function) */
		void pop ()
		{
			ibrcommon::MutexLock l(_cond);
			__pop();
		}

		/**
		 * Retrieves and removes the head of this queue.
		 * If the queue is empty an QueueUnblockedException is thrown.
		 *
		 * @return The next element of the queue
		 */
		T take() throw (QueueUnblockedException)
		{
			try {
				ibrcommon::MutexLock l(_cond);
				if (_queue.empty())
				{
					throw QueueUnblockedException(QueueUnblockedException::QUEUE_ABORT, "take(): queue is empty!");
				}

				T ret = _queue.front();
				__pop();
				return ret;
			} catch (const ibrcommon::Conditional::ConditionalAbortException &ex) {
				throw QueueUnblockedException(ex, "take()");
			}
		}

		/**
		 * Retrieves and removes the head of this queue, waiting if necessary up
		 * to the specified wait time if no elements are present on this queue.
		 *
		 * @param timeout A timeout in milliseconds
		 * @return The next element of the queue
		 */
		T poll(size_t timeout = 0) throw (QueueUnblockedException)
		{
			try {
				ibrcommon::MutexLock l(_cond);
				if (_queue.empty())
				{
					if (timeout == 0)
					{
						__wait(QUEUE_NOT_EMPTY);
					}
					else
					{
						__wait(QUEUE_NOT_EMPTY, timeout);
					}
				}

				T ret = _queue.front();
				__pop();
				return ret;
			} catch (const ibrcommon::Conditional::ConditionalAbortException &ex) {
				throw QueueUnblockedException(ex, "poll()");
			}
		}

		void abort() throw ()
		{
			ibrcommon::MutexLock l(_cond);
			_cond.abort();
		}

		void reset() throw ()
		{
			_cond.reset();
		}

		enum WAIT_MODES
		{
			QUEUE_NOT_EMPTY = 0,
			QUEUE_EMPTY = 1
		};

		void wait(WAIT_MODES mode, const size_t timeout = 0) throw (QueueUnblockedException)
		{
			ibrcommon::MutexLock l(_cond);
			if (timeout == 0)
			{
				__wait(mode);
			}
			else
			{
				__wait(mode, timeout);
			}
		}

		class Locked
		{
		public:
			Locked(Queue<T> &queue)
			 : _queue(queue), _lock(queue._cond), _changed(false)
			{
			};

			virtual ~Locked()
			{
				if (_changed) _queue._cond.signal(true);
			};

			void wait(WAIT_MODES mode, const size_t timeout = 0) throw (QueueUnblockedException)
			{
				if (timeout == 0)
				{
					_queue.__wait(mode);
				}
				else
				{
					_queue.__wait(mode, timeout);
				}
			}

			void pop()
			{
				_queue.__pop();
			}

			const T& front() const
			{
				return _queue._queue.front();
			}

			T& front()
			{
				return _queue._queue.front();
			}

			bool empty() const
			{
				return _queue._queue.empty();
			}

			size_t size() const
			{
				return _queue._queue.size();
			}

			void push(const T &p)
			{
				if (_queue._limit) _queue._sem.wait();

				_queue._queue.push(p);
				_changed = true;
			}

		private:
			Queue<T> &_queue;
			ibrcommon::MutexLock _lock;
			bool _changed;
		};

		typename Queue<T>::Locked exclusive()
		{
			return typename Queue<T>::Locked(*this);
		}

	protected:
		void __pop()
		{
			if (!_queue.empty())
			{
				_queue.pop();
				if (_limit) _sem.post();
				_cond.signal(true);
			}
		}

		void __wait(const WAIT_MODES mode) throw (QueueUnblockedException)
		{
			try {
				switch (mode)
				{
					case QUEUE_NOT_EMPTY:
					{
						while (_queue.empty())
						{
							_cond.wait();
						}
						break;
					}

					case QUEUE_EMPTY:
					{
						while (!_queue.empty())
						{
							_cond.wait();
						}
						break;
					}
				}
			} catch (const ibrcommon::Conditional::ConditionalAbortException &ex) {
				switch (ex.reason)
				{
				case ibrcommon::Conditional::ConditionalAbortException::COND_ABORT:
					_cond.reset();
					break;

				default:
					break;
				}

				throw QueueUnblockedException(ex, "__wait()");
			}
		}

		void __wait(const WAIT_MODES mode, const size_t timeout) throw (QueueUnblockedException)
		{
			try {
				struct timespec ts;
				Conditional::gettimeout(timeout, &ts);

				switch (mode)
				{
					case QUEUE_NOT_EMPTY:
					{
						while (_queue.empty())
						{
							_cond.wait(&ts);
						}
						break;
					}

					case QUEUE_EMPTY:
					{
						while (!_queue.empty())
						{
							_cond.wait(&ts);
						}
						break;
					}
				}
			} catch (const ibrcommon::Conditional::ConditionalAbortException &ex) {
				switch (ex.reason)
				{
				case ibrcommon::Conditional::ConditionalAbortException::COND_ABORT:
					_cond.reset();
					break;

				default:
					break;
				}

				throw QueueUnblockedException(ex, "__wait()");
			}
		}
	};
}

#endif /* IBRCOMMON_QUEUE_H_ */
