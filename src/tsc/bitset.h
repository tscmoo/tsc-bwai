#ifndef TSC_BITSET_H
#define TSC_BITSET_H

#include "bit.h"

namespace tsc {
;

template<size_t N>
struct bitset_base {
	typedef unsigned int value_type;
	static const size_t bits_per_value = sizeof(value_type)*8;
	typename std::conditional<N==0,a_vector<value_type>,std::array<value_type,(N+(bits_per_value-1))/bits_per_value>>::type data;
	//template<typename T=int,typename = typename std::enable_if<(sizeof(T),N!=0),int>::type>
	bitset_base() : data() {
	}
	template<size_t foo = N, typename = typename std::enable_if<foo==0>::type>
	bitset_base(size_t new_N) : data((new_N+(bits_per_value-1))/bits_per_value) {}
	template<size_t foo = N, typename = typename std::enable_if<foo==0>::type>
	void resize(size_t new_N) {
		data.resize((new_N+(bits_per_value-1))/bits_per_value);
	}
	bool test(size_t pos) const {
		size_t index = pos/bits_per_value;
		size_t bitpos = pos%bits_per_value;
		return (data[index]&(value_type(1)<<bitpos))!=0;
	}
	void set(size_t pos) {
		size_t index = pos/bits_per_value;
		size_t bitpos = pos%bits_per_value;
		data[index] |= (value_type(1)<<bitpos);
	}
	void reset(size_t pos) {
		size_t index = pos/bits_per_value;
		size_t bitpos = pos%bits_per_value;
		data[index] &= ~(value_type(1)<<bitpos);
	}
	void set() {
		for (size_t i=0;i<N/bits_per_value;++i) data[i] = ~(value_type)0;
		if (N%bits_per_value) {
			data[data.size() - 1] = ~(value_type)0 >> std::max((bits_per_value - (N%bits_per_value)), bits_per_value - 1);
		}
	}
	void reset() {
		for (auto&v : data) v = 0;
	}
	bool operator==(const bitset_base&n) const {
		for (size_t i=0;i<data.size();++i) {
			if (n.data[i] != data[i]) return false;
		}
		return true;
	}
	bool operator!=(const bitset_base&n) const {
		return !(*this == n);
	}
	bool operator<(const bitset_base&n) const {
		return data < n.data;
	}
	bitset_base operator~() const {
		bitset_base r;
		r.resize(data.size());
		for (size_t i=0;i<data.size();++i) {
			r.data[i] = ~data[i];
		}
		if (N%bits_per_value) {
			r.data[data.size()-1] &= ~(value_type)0 >> (bits_per_value-(N%bits_per_value));
		}
		return r;
	}
	bitset_base&operator ^=(const bitset_base&n) {
		for (size_t i=0;i<data.size();++i) {
			data[i] ^= n.data[i];
		}
		return *this;
	}
	bitset_base operator^(const bitset_base&n) const {
		bitset_base r = *this;
		return r ^= n;
	}
	bitset_base&operator |=(const bitset_base&n) {
		for (size_t i=0;i<data.size();++i) {
			data[i] |= n.data[i];
		}
		return *this;
	}
	bitset_base operator|(const bitset_base&n) const {
		bitset_base r = *this;
		return r |= n;
	}
	bitset_base&operator &=(const bitset_base&n) {
		for (size_t i=0;i<data.size();++i) {
			data[i] &= n.data[i];
		}
		return *this;
	}
	bitset_base operator&(const bitset_base&n) const {
		bitset_base r = *this;
		return r &= n;
	}
	bool any() const {
		for (size_t i=0;i<data.size();++i) {
			if (data[i]) return true;
		}
		return false;
	}
	bool none() const {
		for (size_t i=0;i<data.size();++i) {
			if (data[i]) return false;
		}
		return true;
	}
	bool all() const {
		for (size_t i=0;i<data.size();++i) {
			if (!data[i]) return true;
		}
		return false;
	}
	size_t count() const {
		size_t r = 0;
		for (size_t i=0;i<data.size();++i) {
			r += bit_popcount(data[i]);
		}
		return r;
	}
	struct iterator {
		const bitset_base&bs;
		size_t index;
		size_t bitpos;
		typename bitset_base::value_type value;
		iterator(const bitset_base&bs,size_t index_arg) : bs(bs), value(0) {
			index = index_arg;
			if (index==bs.data.size()) return;
			--index;
			++*this;
		}
		bool operator!=(const iterator&n) const {
			return index!=n.index;
		}
		size_t operator*() const {
			size_t idx = index*bits_per_value + bitpos;
			return idx;
		}
		iterator&operator++() {
			while (!value) {
				++index;
				if (index==bs.data.size()) return *this;
				value = bs.data[index];
			}
			bitpos = bit_count_trailing_zeros(value);
			value &= ~((value_type)1)<<bitpos;
			return *this;
		}
	};
	iterator begin() const {
		return iterator(*this,0);
	}
	iterator end() const {
		return iterator(*this,data.size());
	}
};

template<size_t N>
struct bitset: bitset_base<N> {
	//bitset() : bitset_base() {}
	bitset() {}
};

struct dynamic_bitset: bitset_base<0> {
	dynamic_bitset() : bitset_base(0) {}
	dynamic_bitset(size_t size) : bitset_base(size) {}
};


}

#endif
