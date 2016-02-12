#ifndef TSC_USERTHREADS_H
#define TSC_USERTHREADS_H

#ifndef _WIN32
#include <ucontext.h>
#endif

namespace tsc {
;

namespace ut_impl {

#ifdef _WIN32
	struct ut_t {
		void* user = nullptr;
		void* h;
	};
#else
	struct ut_t {
		void* user = nullptr;
		ucontext_t h;
	};
#endif

	ut_t* current_ut;
	void enter(ut_t& ut);
	void leave(ut_t&);
	template<typename F>
	ut_t create(F&& f,void* a);
	void switch_to(ut_t&);
	void erase(ut_t&);
	void return_to(ut_t&);

#ifdef _WIN32
	void enter(ut_t&t) {
		t.h = ConvertThreadToFiber(nullptr);
		current_ut = &t;
	}
	void leave(ut_t&) {
		current_ut = nullptr;
		ConvertFiberToThread();
	}
	template<typename F>
	ut_t create(F&& f,void* a) {
		ut_t r;
		r.h = CreateFiber(0, f, a);
		if (!r.h) throw "CreateFiber failed";
		return r;
	}
	void switch_to(ut_t& t) {
		current_ut = &t;
		SwitchToFiber(t.h);
	}
	void erase(ut_t& t) {
		if (t.h) DeleteFiber(t.h);
	}
	void return_to(ut_t& t) {
		switch_to(t);
	}
#else

	void enter(ut_t& ut) {
		current_ut = &ut;
	}
	void leave(ut_t&) {
		current_ut = nullptr;
	}
	template<typename F>
	ut_t create(F&& f,void*a) {
		const size_t stack_size = 1024*1024*2;
		char*stack = (char*)malloc(stack_size);
		if (!stack) throw "failed to allocate stack";
		ut_t r;
		getcontext(&r.h);
		r.h.uc_stack.ss_sp = stack;
		r.h.uc_stack.ss_size = stack_size;
		makecontext(&r.h, (void(*)())(void(*)(void*))f, 1, a);
		return r;
	}
	void switch_to(ut_t&ut) {
		ut_t* p = current_ut;
		current_ut = &ut;
		swapcontext(p->h, current_ut->h);
	}
	void erase(ut_t&) {
		// fixme: free(r.uc_stack.ss_sp);
	}
	void return_to(ut_t&ut) {
		current_ut->uc_link = &ut;
	}

#endif

}

}

#endif
