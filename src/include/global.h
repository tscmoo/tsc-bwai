//
// This file declares facilities for defining global data, taking
// care to associate it with the current instance of the bot, such that
// the bot can be instantiated in the same program multiple times
// independently.
//

#ifndef TSC_BWAI_GLOBAL_H
#define TSC_BWAI_GLOBAL_H

#include "containers.h"
#include <functional>
#include <tuple>
#include <memory>

namespace tsc_bwai {

	// globals_container - This struct holds all the globals
	struct globals_container {
		a_vector<char> vec;
		globals_container();
		~globals_container();
	};

	// This should be called before the bot can run on the current thread.
	// If this is the first time it is called for the specified globals_container
	// object, then it will be initialized and all globals constructed.
	// There is a default globals_container instance set at program start, but
	// this is mainly to allow globals to refer to each other in their
	// initialization.
	// If any new global objects are constructed, then this must be called
	// again.
	void set_globals_container(globals_container& gc);

	namespace global_detail {
		size_t get_new_offset(size_t size, size_t alignment);
		void* get_ptr(size_t offset);
		void add_ctor(std::function<void()> f);
		void add_dtor(std::function<void()> f);

		// Helper function used below to forward arguments to the constructor
		// with a tuple.
		template<typename T, typename tuple_T, size_t... indices>
		void tuple_ctor_helper(T* ptr, tuple_T&& tuple, std::index_sequence<indices...>) {
			new (ptr) T(std::get<indices>(std::forward<tuple_T>(tuple))...);
		}
	}

	// global - Class that defines global data.
	// Implicitly convertible to T&, otherwise similar to unique_ptr, but always
	// has a value.
	// This object must be constructed before set_globals_container is called.
	// Example:
	//   global<int> foo = 4;
	//   set_globals_container(...);
	//   int bar = foo;
	//   log("%d %d", *foo, bar + foo);
	template<typename T>
	class global {
		size_t offset;
	public:
		template<typename... args_T>
		global(args_T&&... args) {
			offset = global_detail::get_new_offset(sizeof(T), alignof(T));

			auto tup = std::make_tuple(std::forward<args_T>(args)...);

			// The object can not be constructed yet, so we store a lambda that will
			// construct it when it is allocated.
			// Perfectly forward the arguments to the lambda and then to the constructor.
			global_detail::add_ctor([this, args_tuple = std::move(tup)]{
				global_detail::tuple_ctor_helper<T>(get(), args_tuple, std::make_index_sequence<std::tuple_size<decltype(args_tuple)>::value> {});
			});
			global_detail::add_dtor([this]{
				get()->~T();
			});
		}
		template<typename n_T>
		global& operator=(n_T&& n) {
			*get() = std::forward<n_T>(n);
			return *this;
		}
		operator T&() const {
			return *get();
		}
		operator T&() {
			return *get();
		}
		T* get() const {
			return (T*)global_detail::get_ptr(offset);
		}
		T* get() {
			return (T*)global_detail::get_ptr(offset);
		}
		const T* operator->() const {
			return get();
		}
		T* operator->() {
			return get();
		}
		const T& operator*() const {
			return *get();
		}
		T& operator*() {
			return *get();
		}
	};
}

#endif
