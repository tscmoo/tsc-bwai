

template<typename T,typename filter_F>
struct range_filter {
	T&cont;
	filter_F filter;
	range_filter(T&cont,filter_F filter) : cont(cont), filter(filter) {}

	typename T::iterator it;

	typedef typename T::value_type value_type;

	template<typename iterator_T>
	struct iterator_impl {
		typedef typename iterator_T::value_type value_type;
		typedef typename iterator_T::reference reference;
		const range_filter&r;
		iterator_T it;
		iterator_impl(const range_filter&r,iterator_T it) : r(r), it(it) {
			if (it!=r.cont.end() && !r.filter(*it)) ++*this;
		}
		bool operator!=(const iterator_impl&n) {
			return it!=n.it;
		}
		iterator_impl&operator++() {
			do {
				++it;
			} while (it!=r.cont.end() && !r.filter(*it));
			return *this;
		}
		reference operator*() const {
			return *it;
		}

	};
	typedef iterator_impl<typename T::iterator> iterator;
	typedef iterator_impl<typename T::const_iterator> const_iterator;
	iterator begin() {
		return iterator(*this,cont.begin());
	}
	iterator end() {
		return iterator(*this,cont.end());
	}
	const_iterator begin() const {
		return const_iterator(*this,cont.begin());
	}
	const_iterator end() const {
		return const_iterator(*this,cont.end());
	}

};



template<typename T,typename filter_F>
range_filter<T,filter_F> make_range_filter(T&cont,filter_F&&filter) {
	return range_filter<T,filter_F>(cont,std::forward<filter_F>(filter));
}

template<typename T,typename filter_F>
struct transform_filter {
	T&cont;
	filter_F filter;
	transform_filter(T&cont,filter_F filter) : cont(cont), filter(filter) {}

	typename T::iterator it;

	typedef typename std::result_of<filter_F(typename T::reference)>::type value_type;
	
	template<typename iterator_T>
	struct iterator_impl {
		typedef decltype(std::declval<const transform_filter>().filter(std::declval<iterator_T::reference>())) value_type;
		const transform_filter&r;
		iterator_T it;
		iterator_impl(const transform_filter&r,iterator_T it) : r(r), it(it) {}
		bool operator!=(const iterator_impl&n) {
			return it!=n.it;
		}
		iterator_impl&operator++() {
			++it;
			return *this;
		}
		value_type operator*() const {
			return r.filter(*it);
		}

	};
	typedef iterator_impl<typename T::iterator> iterator;
	iterator begin() const {
		return iterator(*this,cont.begin());
	}
	iterator end() const {
		return iterator(*this,cont.end());
	}

};



template<typename T,typename filter_F>
transform_filter<T,filter_F> make_transform_filter(T&cont,filter_F&&filter) {
	return transform_filter<T,filter_F>(cont,std::forward<filter_F>(filter));
}


template<typename iter_T>
struct iterators_range {
	iter_T begin_it;
	iter_T end_it;
	iterators_range(iter_T begin_it, iter_T end_it) : begin_it(begin_it), end_it(end_it) {}

	typedef iter_T iterator;

	typedef typename iterator::value_type value_type;
	typedef typename iterator::pointer pointer;
	typedef typename iterator::reference reference;

	iterator begin() {
		return begin_it;
	}
	iterator end() {
		return end_it;
	}

};


template<typename iter_T>
iterators_range<iter_T> make_iterators_range(iter_T begin, iter_T end) {
	return iterators_range<iter_T>(begin, end);
}
