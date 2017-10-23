/*
 * refcnt_ptr.h
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

#ifndef IBRCOMMON_refcnt_ptr_h
#define IBRCOMMON_refcnt_ptr_h 1

#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/MutexLock.h>

template <class T> class refcnt_ptr
{
	protected:
		// a helper class that holds the pointer to the managed object
		// and its reference count
		class Holder
		{
		public:
			Holder( T* ptr) : ptr_(ptr), count_(1) {};
			virtual ~Holder() { delete ptr_;};

			T* ptr_;
			unsigned count_;
			ibrcommon::Mutex lock;
		};

		Holder* h_;

	private:
		void down()
		{
			bool final = false;
			{
				ibrcommon::MutexLock l(h_->lock);
				if (--h_->count_ == 0) final = true;
			}
			if (final) delete h_;
		}

	public:
		// ctor of refcnt_ptr (p must not be NULL)
		explicit refcnt_ptr(T* p) : h_(new Holder(p))
		{
		}

		// dtor of refcnt_ptr
		~refcnt_ptr()
		{
			down();
		}

		// copy and assignment of refcnt_ptr
		refcnt_ptr (const refcnt_ptr<T>& right) : h_(right.h_)
		{
			ibrcommon::MutexLock l(h_->lock);
			++h_->count_;
		}

		refcnt_ptr<T>& operator= (const refcnt_ptr<T>& right)
		{
			// ignore assignment to myself
			if (h_ == right.h_) return *this;

			ibrcommon::MutexLock l(right.h_->lock);
			++right.h_->count_;

			down();

			h_ = right.h_;

			return *this;
		}

		bool operator==(const T *other) const
		{
			return h_->ptr_ == other;
		}

		bool operator==(const refcnt_ptr<T> &other) const
		{
			return h_->ptr_ == other.h_->ptr_;
		}

		// access to the managed object
		T* operator-> () { return h_->ptr_; }
		T& operator* () { return *h_->ptr_; }

		const T* operator-> () const { return h_->ptr_; }
		const T& operator* () const { return *h_->ptr_; }
};
#endif
