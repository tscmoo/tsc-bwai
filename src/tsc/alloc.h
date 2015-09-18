#ifndef TSC_ALLOC_H
#define TSC_ALLOC_H

#include "bit.h"

namespace tsc {
;

// This allocator is NOT thread-safe.

namespace alloc_impl {
	size_t bytes_allocated = 0;
	size_t max_bytes_allocated = 0;
	void*raw_allocate(size_t n) {
		bytes_allocated += n;
		if (bytes_allocated>max_bytes_allocated) max_bytes_allocated = bytes_allocated;
		return malloc(n);
	}
	void raw_deallocate(void*ptr,size_t n) {
		bytes_allocated -= n;
		return free(ptr);
	}

	struct base {
		base*next;
		base();
		~base();
	};

	std::array<base*,8> lists{};
	std::array<int,8> allocations{};
	int extra_allocations = 0;

	void*allocate(size_t on) {
		size_t n = on + sizeof(base);
		size_t idx = sizeof(n)*8-bit_count_leading_zeros(n>>3);
		if (idx>=lists.size()) return ++extra_allocations, raw_allocate(n);
		base*&head = lists[idx];
		if (!head) return ++extra_allocations, ((base*)raw_allocate(8<<idx))+1;
		++allocations[idx];
		base*r = head;
		head = r->next;
		return r+1;
	}
	void deallocate(void*ptr,size_t on) {
		size_t n = on + sizeof(base);
		size_t idx = sizeof(n)*8-bit_count_leading_zeros(n>>3);
		if (idx>=lists.size()) return raw_deallocate(ptr,n);
		base*&head = lists[idx];
		base*newhead = ((base*)ptr)-1;
		newhead->next = head;
		head = newhead;
	}

}

template<typename T> struct alloc;
template<>
struct alloc<void> {
	typedef void value_type;
	typedef void*pointer;
	typedef const void*const_pointer;
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	template<typename t>
	struct rebind {
		typedef alloc<t> other;
	};
};
template<typename T>
struct alloc {
	typedef T value_type;
	typedef T*pointer;
	typedef const T*const_pointer;
	typedef T&reference;
	typedef const T&const_reference;
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef std::true_type propagate_on_container_move_assignment;
	typedef std::true_type is_always_equal;
	template<typename t>
	struct rebind {
		typedef alloc<t> other;
	};
	alloc() {}
	alloc(const alloc&other) {}
	template<typename t>
	alloc(const alloc<t>&other) {}
	~alloc() {}
	pointer allocate (size_type n,alloc<void>::const_pointer hint=0) {
		T*r = (T*)alloc_impl::allocate(sizeof(T)*n);
#ifndef TSC_NO_EXCEPTIONS
		if (!r) throw std::bad_alloc();
#else
		std::terminate();
#endif
		return r;
	}
	void deallocate(pointer p,size_type n) {
		alloc_impl::deallocate(p,sizeof(T)*n);
	}
	alloc&operator=(const alloc&n) {
		return *this;
	}
	alloc&operator=(alloc&&n) {
		return *this;
	}
	bool operator==(const alloc&n) const {
		return true;
	}
	bool operator!=(const alloc&n) const {
		return false;
	}
	template<class U, class... Args>
	void construct(U*p,Args&&... args) noexcept {
		::new ((void*)p) U(std::forward<Args>(args)...);
	}
	template<class U>
	void destroy(U*p) {
		p->~U();
	}
	size_type max_size() const {
		return std::numeric_limits<size_type>::max();
	}
// 	pointer address(reference v) {
// 		return &v;
// 	}
// 	const_pointer address(const_reference v) {
// 		return &v;
// 	}
};

}

#endif
