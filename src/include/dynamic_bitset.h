//
// This file defines the dynamic_bitset class
//

#ifndef TSC_BWAI_DYNAMIC_BITSET_H
#define TSC_BWAI_DYNAMIC_BITSET_H

#include "common.h"
#include "tsc/bit.h"

namespace tsc_bwai {
	// dynamic_bitset - A dynamically resizable vector of bits. Bits are stored
	// internally in a a_vector.
	// Note that the number of bits actually stored may be rounded up to some
	// convenient number.
	// Iterating the bitset will return the indices of all the set bits.
	// Bits can not be assigned through iterators, and there is no operator[] (
	// use test, set and reset instead).
	class dynamic_bitset {
	public:
		typedef unsigned long value_type;
	private:
		static const size_t bits_per_value = sizeof(value_type) * 8;
		a_vector<value_type> data;
	public:
		dynamic_bitset() {}
		dynamic_bitset(size_t new_N) : data((new_N + (bits_per_value - 1)) / bits_per_value) {}
		// Resize the bitset to where it can hold at least the specified number of
		// bits.
		void resize(size_t new_N) {
			data.resize((new_N + (bits_per_value - 1)) / bits_per_value);
		}
		// Returns whether the bit at the specified index is set.
		bool test(size_t pos) const {
			size_t index = pos / bits_per_value;
			size_t bitpos = pos%bits_per_value;
			return (data[index] & (value_type(1) << bitpos)) != 0;
		}
		// Sets the bit at the specified index to 1.
		void set(size_t pos) {
			size_t index = pos / bits_per_value;
			size_t bitpos = pos%bits_per_value;
			data[index] |= (value_type(1) << bitpos);
		}
		// Sets the bit at the specified index to 0.
		void reset(size_t pos) {
			size_t index = pos / bits_per_value;
			size_t bitpos = pos%bits_per_value;
			data[index] &= ~(value_type(1) << bitpos);
		}
		// Sets all bits to 1.
		void set() {
			for (auto& v : data) v = ~(value_type)0;
		}
		// Sets all bits to 0.
		void reset() {
			for (auto& v : data) v = 0;
		}
		bool operator==(const dynamic_bitset&n) const {
			for (size_t i = 0; i < data.size(); ++i) {
				if (n.data[i] != data[i]) return false;
			}
			return true;
		}
		bool operator!=(const dynamic_bitset&n) const {
			return !(*this == n);
		}
		bool operator<(const dynamic_bitset&n) const {
			return data < n.data;
		}
		dynamic_bitset operator~() const {
			dynamic_bitset r;
			r.resize(data.size());
			for (size_t i = 0; i < data.size(); ++i) {
				r.data[i] = ~data[i];
			}
			return r;
		}
		dynamic_bitset& operator^=(const dynamic_bitset& n) {
			for (size_t i = 0; i < data.size(); ++i) {
				data[i] ^= n.data[i];
			}
			return *this;
		}
		dynamic_bitset operator^(const dynamic_bitset& n) const {
			dynamic_bitset r = *this;
			return r ^= n;
		}
		dynamic_bitset& operator|=(const dynamic_bitset& n) {
			for (size_t i = 0; i < data.size(); ++i) {
				data[i] |= n.data[i];
			}
			return *this;
		}
		dynamic_bitset operator|(const dynamic_bitset& n) const {
			dynamic_bitset r = *this;
			return r |= n;
		}
		dynamic_bitset& operator&=(const dynamic_bitset& n) {
			for (size_t i = 0; i < data.size(); ++i) {
				data[i] &= n.data[i];
			}
			return *this;
		}
		dynamic_bitset operator&(const dynamic_bitset& n) const {
			dynamic_bitset r = *this;
			return r &= n;
		}
		bool any() const {
			for (size_t i = 0; i < data.size(); ++i) {
				if (data[i]) return true;
			}
			return false;
		}
		bool none() const {
			for (size_t i = 0; i < data.size(); ++i) {
				if (data[i]) return false;
			}
			return true;
		}
		bool all() const {
			for (size_t i = 0; i < data.size(); ++i) {
				if (!data[i]) return true;
			}
			return false;
		}
		size_t count() const {
			size_t r = 0;
			for (size_t i = 0; i < data.size(); ++i) {
				r += tsc::bit_popcount(data[i]);
			}
			return r;
		}
		struct iterator {
			const dynamic_bitset& bs;
			size_t index;
			size_t bitpos;
			dynamic_bitset::value_type value;
			iterator(const dynamic_bitset& bs, size_t index_arg) : bs(bs), value(0) {
				index = index_arg;
				if (index == bs.data.size()) return;
				--index;
				++*this;
			}
			bool operator!=(const iterator&n) const {
				return index != n.index;
			}
			size_t operator*() const {
				size_t idx = index*bits_per_value + bitpos;
				return idx;
			}
			iterator&operator++() {
				while (!value) {
					++index;
					if (index == bs.data.size()) return *this;
					value = bs.data[index];
				}
				bitpos = tsc::bit_count_trailing_zeros(value);
				value &= ~((value_type)1) << bitpos;
				return *this;
			}
		};
		iterator begin() const {
			return iterator(*this, 0);
		}
		iterator end() const {
			return iterator(*this, data.size());
		}
	};
}

#endif
