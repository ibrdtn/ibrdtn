/*
 * BloomFilter.cpp
 *
 * This bloom filter implementation is based on
 * Open Bloom Filter (http://www.partow.net) written by Arash Partow.
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

#include "ibrcommon/data/BloomFilter.h"
#include "ibrcommon/Exceptions.h"
#include <limits>
#include <math.h>

namespace ibrcommon
{
	HashProvider::~HashProvider()
	{ }

	DefaultHashProvider::DefaultHashProvider(size_t salt_count)
	 : salt_count_(salt_count)
	{
		generate_salt();
	}

	DefaultHashProvider::~DefaultHashProvider()
	{
	}

	bool DefaultHashProvider::operator==(const DefaultHashProvider &provider) const
	{
		return ( provider.salt_count_ == salt_count_);
	}

	void DefaultHashProvider::clear()
	{
		_salt.clear();
	}

	void DefaultHashProvider::add(bloom_type hash)
	{
		_salt.push_back(hash);
	}

	size_t DefaultHashProvider::count() const
	{
		return _salt.size();
	}

	const std::list<bloom_type> DefaultHashProvider::hash(const unsigned char* begin, std::size_t remaining_length) const
	{
		std::list<bloom_type> hashes;

		for (std::vector<bloom_type>::const_iterator iter = _salt.begin(); iter != _salt.end(); ++iter)
		{
			hashes.push_back(hash_ap(begin, remaining_length, (*iter)));
		}

		return hashes;
	}


	void DefaultHashProvider::generate_salt()
	{
		const unsigned int predef_salt_count = 64;
		static const bloom_type predef_salt[predef_salt_count] =
									{
										  0xAAAAAAAA, 0x55555555, 0x33333333, 0xCCCCCCCC,
										  0x66666666, 0x99999999, 0xB5B5B5B5, 0x4B4B4B4B,
										  0xAA55AA55, 0x55335533, 0x33CC33CC, 0xCC66CC66,
										  0x66996699, 0x99B599B5, 0xB54BB54B, 0x4BAA4BAA,
										  0xAA33AA33, 0x55CC55CC, 0x33663366, 0xCC99CC99,
										  0x66B566B5, 0x994B994B, 0xB5AAB5AA, 0xAAAAAA33,
										  0x555555CC, 0x33333366, 0xCCCCCC99, 0x666666B5,
										  0x9999994B, 0xB5B5B5AA, 0xFFFFFFFF, 0xFFFF0000,
										  0xB823D5EB, 0xC1191CDF, 0xF623AEB3, 0xDB58499F,
										  0xC8D42E70, 0xB173F616, 0xA91A5967, 0xDA427D63,
										  0xB1E8A2EA, 0xF6C0D155, 0x4909FEA3, 0xA68CC6A7,
										  0xC395E782, 0xA26057EB, 0x0CD5DA28, 0x467C5492,
										  0xF15E6982, 0x61C6FAD3, 0x9615E352, 0x6E9E355A,
										  0x689B563E, 0x0C9831A8, 0x6753C18B, 0xA622689B,
										  0x8CA63C47, 0x42CC2884, 0x8E89919B, 0x6EDBD7D3,
										  0x15B6796C, 0x1D6FDFE4, 0x63FF9092, 0xE7401432
									};

		if (salt_count_ > predef_salt_count)
		{
			throw ibrcommon::Exception("Max. 64 hash salts supported!");
		}

		for(unsigned int i = 0; i < salt_count_; ++i)
		{
			add(predef_salt[i]);
		}
	}

	bloom_type DefaultHashProvider::hash_ap(const unsigned char* begin, std::size_t remaining_length, bloom_type hash) const
	{
		const unsigned char* it = begin;
		while(remaining_length >= 2)
		{
			hash ^=    (hash <<  7) ^  (*it++) * (hash >> 3);
			hash ^= (~((hash << 11) + ((*it++) ^ (hash >> 5))));
			remaining_length -= 2;
		}
		if (remaining_length)
		{
			hash ^= (hash <<  7) ^ (*it) * (hash >> 3);
		}
		return hash;
	}

	// allocation threshold for growing = 0.01%
	const double BloomFilter::table_allocation_max = 0.0001;

	const unsigned char BloomFilter::bit_mask[bits_per_char] = {
														   0x01,  //00000001
														   0x02,  //00000010
														   0x04,  //00000100
														   0x08,  //00001000
														   0x10,  //00010000
														   0x20,  //00100000
														   0x40,  //01000000
														   0x80   //10000000
														 };

	BloomFilter::BloomFilter(std::size_t table_size, std::size_t table_max, std::size_t salt_count)
	 : _hashp(salt_count), table_size_(table_size * bits_per_char), table_size_init_(table_size * bits_per_char), table_size_max_(std::max(table_size, table_max) * bits_per_char), _itemcount(0), salt_count_(salt_count)
	{
		bit_table_ = new cell_type[table_size_ / bits_per_char];
		std::fill_n(bit_table_,(table_size_ / bits_per_char),0x00);
	}

	BloomFilter::BloomFilter(const BloomFilter& filter)
	 : _hashp(filter._hashp), table_size_(filter.table_size_), table_size_init_(filter.table_size_init_), table_size_max_(filter.table_size_max_), _itemcount(filter._itemcount), salt_count_(filter.salt_count_)
	{
		bit_table_ = new cell_type[table_size_ / bits_per_char];
		std::copy(filter.bit_table_, filter.bit_table_ + (table_size_ / bits_per_char), bit_table_);
	}

	BloomFilter::~BloomFilter()
	{
		delete[] bit_table_;
	}

	BloomFilter& BloomFilter::operator=(const BloomFilter& filter)
	{
		_hashp = filter._hashp;
		table_size_ = filter.table_size_;
		delete[] bit_table_;
		bit_table_ = new cell_type[table_size_ / bits_per_char];
		std::copy(filter.bit_table_, filter.bit_table_ + (table_size_ / bits_per_char), bit_table_);
		return *this;
	}

	bool BloomFilter::operator!() const
	{
		return (0 == table_size_);
	}

	void BloomFilter::clear()
	{
		// shrink Bloom-filter to initial size
		if (table_size_ > table_size_init_) {
			table_size_ = table_size_init_;
			delete[] bit_table_;
			bit_table_ = new cell_type[table_size_ / bits_per_char];
		}

		std::fill_n(bit_table_,(table_size_ / bits_per_char),0x00);
		_itemcount = 0;
	}

	void BloomFilter::insert(const unsigned char* key_begin, const std::size_t length)
	{
		std::size_t bit_index = 0;
		std::size_t bit = 0;

		std::list<bloom_type> hashes = _hashp.hash(key_begin, length);

		for (std::list<bloom_type>::iterator iter = hashes.begin(); iter != hashes.end(); ++iter)
		{
			compute_indices( (*iter), bit_index, bit );
			bit_table_[bit_index / bits_per_char] |= bit_mask[bit];
		}

		if (_itemcount < std::numeric_limits<unsigned int>::max()) _itemcount++;
	}

	void BloomFilter::insert(const std::string& key)
	{
		insert(reinterpret_cast<const unsigned char*>(key.c_str()),key.size());
	}

	void BloomFilter::insert(const char* data, const std::size_t& length)
	{
		insert(reinterpret_cast<const unsigned char*>(data),length);
	}

	bool BloomFilter::contains(const unsigned char* key_begin, const std::size_t length) const
	{
		std::size_t bit_index = 0;
		std::size_t bit = 0;

		const std::list<bloom_type> hashes = _hashp.hash(key_begin, length);

		for (std::list<bloom_type>::const_iterator iter = hashes.begin(); iter != hashes.end(); ++iter)
		{
			compute_indices( (*iter), bit_index, bit );
			if ((bit_table_[bit_index / bits_per_char] & bit_mask[bit]) != bit_mask[bit])
			{
				return false;
			}
		}

		return true;
	}

	bool BloomFilter::contains(const std::string& key) const
	{
		return contains(reinterpret_cast<const unsigned char*>(key.c_str()),key.size());
	}

	bool BloomFilter::contains(const char* data, const std::size_t& length) const
	{
		return contains(reinterpret_cast<const unsigned char*>(data),length);
	}

	std::size_t BloomFilter::size() const
	{
		return (table_size_ / bits_per_char);
	}

	BloomFilter& BloomFilter::operator &= (const BloomFilter& filter)
	{
		  /* intersection */
		  if (
			  (_hashp  == filter._hashp) &&
			  (table_size_  == filter.table_size_)
			 )
		  {
			 for (std::size_t i = 0; i < (table_size_ / bits_per_char); ++i)
			 {
				bit_table_[i] &= filter.bit_table_[i];
			 }
		  }
		  return *this;
	}

	BloomFilter& BloomFilter::operator |= (const BloomFilter& filter)
	{
	  /* union */
	  if (
		  (_hashp  == filter._hashp) &&
		  (table_size_  == filter.table_size_)
		 )
	  {
		 for (std::size_t i = 0; i < (table_size_ / bits_per_char); ++i)
		 {
			bit_table_[i] |= filter.bit_table_[i];
		 }
	  }
	  return *this;
	}

	BloomFilter& BloomFilter::operator ^= (const BloomFilter& filter)
	{
		  /* difference */
		  if (
			  (_hashp  == filter._hashp) &&
			  (table_size_  == filter.table_size_)
			 )
		  {
			 for (std::size_t i = 0; i < (table_size_ / bits_per_char); ++i)
			 {
				bit_table_[i] ^= filter.bit_table_[i];
			 }
		  }
		  return *this;
	}

	void BloomFilter::load(const cell_type* data, size_t len)
	{
		if (len == (table_size_ / bits_per_char))
		{
			std::copy(data, data + len, bit_table_);
		}
		else
		{
			table_size_ = len * bits_per_char;
			delete[] bit_table_;
			bit_table_ = new cell_type[len];
			std::copy(data, data + len, bit_table_);
		}

		_itemcount = 0;
	}

	bool BloomFilter::grow(size_t num)
	{
		// do not grow if maximum size is already reached
		if (table_size_ >= table_size_max_) return false;

		// return true, if the allocation is acceptable
		if (estimateAllocation(num, table_size_) < table_allocation_max) return false;

		// double table_size
		while (table_size_ < table_size_max_)
		{
			// limit the table size to the maximum value
			table_size_ = std::min(table_size_ * 2, table_size_max_);

			// if allocation is sufficient low, stop growing
			if (estimateAllocation(num, table_size_) < table_allocation_max) break;
		}

		// create a new table
		delete[] bit_table_;
		bit_table_ = new cell_type[table_size_ / bits_per_char];

		std::fill_n(bit_table_, (table_size_ / bits_per_char), 0x00);
		_itemcount = 0;

		// return true to indicate that the filter should get re-filled
		return true;
	}

	const cell_type* BloomFilter::table() const
	{
		return bit_table_;
	}

	void BloomFilter::compute_indices(const bloom_type& hash, std::size_t& bit_index, std::size_t& bit) const
	{
		bit_index = hash % table_size_;
		bit = bit_index % bits_per_char;
	}

	double BloomFilter::estimateAllocation(size_t items, size_t table_size) const
	{
		double n = static_cast<double>(items);
		double k = static_cast<double>(salt_count_);
		double s = static_cast<double>(table_size);
		return pow(1 - pow(1 - (1 / s), k * n), k);
	}

	double BloomFilter::getAllocation() const
	{
		return estimateAllocation(_itemcount, table_size_);
	}
}
