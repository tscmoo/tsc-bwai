
#include "global.h"
#include <memory>
#include <mutex>
using namespace tsc_bwai;

globals_container default_gc;

thread_local globals_container* current_gc = nullptr;
thread_local char* current_vec_data = nullptr;
size_t current_size = 0;

// These are allocated as-needed and never deallocated, since they are used
// in global constructors and destructors
a_vector<std::function<void()>>* ctors;
a_vector<std::function<void()>>* dtors;

tsc_bwai::globals_container::globals_container() {
}

tsc_bwai::globals_container::~globals_container() {
	auto prev_gc = current_gc;
	auto prev_vec_data = current_vec_data;
	current_gc = this;
	current_vec_data = this->vec.data();
	if (dtors) {
		for (auto& v : *dtors) {
			v();
		}
	}
	current_gc = prev_gc;
	current_vec_data = prev_vec_data;
}

void tsc_bwai::global_detail::add_ctor(std::function<void()> f) {
	if (default_gc.vec.size() < current_size) default_gc.vec.resize(current_size);
	current_vec_data = default_gc.vec.data();
	current_gc = &default_gc;
	f();
	if (!ctors) ctors = new a_vector<std::function<void()>>();
	ctors->push_back(std::move(f));
}
void tsc_bwai::global_detail::add_dtor(std::function<void()> f) {
	if (!dtors) dtors = new a_vector<std::function<void()>>();
	dtors->push_back(std::move(f));
}

void tsc_bwai::set_globals_container(globals_container& gc) {
	bool is_new = gc.vec.size() != current_size;
	if (is_new) gc.vec.resize(current_size);
	current_gc = &gc;
	current_vec_data = gc.vec.data();
	if (is_new) {
		if (ctors) {
			for (auto& v : *ctors) {
				v();
			}
		}
	}
}

size_t global_detail::get_new_offset(size_t size, size_t alignment) {
	if (current_size % alignment != 0) {
		current_size += alignment - (current_size % alignment);
	}
	size_t r = current_size;
	current_size += size;
	return r;
}

void* global_detail::get_ptr(size_t offset) {
	return (void*)&current_vec_data[offset];
}
