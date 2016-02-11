#ifndef TSC_BWAI_MULTITASKING_SYNC_H
#define TSC_BWAI_MULTITASKING_SYNC_H

#include "multitasking.h"
#include "common.h"

namespace tsc_bwai {
	namespace multitasking {

		namespace detail {
			struct task_id_queue {
				typedef a_deque<task_id> deq_t;
				union U {
					task_id inline_id;
					deq_t deq;
					// vc++14 fails with constexpr here :(
					/*constexpr */U() : inline_id(invalid_task_id) {}
					~U() {}
				} u;
				bool deq_exists = false;
				/*constexpr */task_id_queue() = default;
				~task_id_queue() {
					if (deq_exists) u.deq.~deq_t();
				}
				void push_back(task_id id) {
					if (deq_exists) u.deq.push_back(id);
					else {
						if (u.inline_id == invalid_task_id) u.inline_id = id;
						else {
							task_id inline_id = u.inline_id;
							new (&u.deq) deq_t();
							try {
								u.deq.push_back(inline_id);
							} catch (...) {
								u.deq.~deq_t();
								u.inline_id = inline_id;
								throw;
							}
							deq_exists = true;
							u.deq.push_back(id);
						}
					}
				}
				void pop_front() {
					if (deq_exists) u.deq.pop_front();
					else u.inline_id = invalid_task_id;
				}
				task_id front() {
					if (deq_exists) return u.deq.front();
					else return u.inline_id;
				}
				bool empty() {
					if (deq_exists) return u.deq.empty();
					else return u.inline_id == invalid_task_id;
				}
			};
		}

		struct sleep_queue {
		private:
			multitasking_t& mt;
			detail::task_id_queue queue;
		public:
			sleep_queue(multitasking_t& mt) : mt(mt) {}
			void wait() {
				queue.push_back(mt.current_task_id());
				mt.wait();
			}
			void release_one() {
				task_id id = queue.front();
				queue.pop_front();
				mt.wake(id);
			}
			bool empty() {
				return queue.empty();
			}
		};

		struct mutex {
		private:
			sleep_queue queue;
			int lock_count = 0;
		public:
			mutex(multitasking_t& mt) : queue(mt) {}
			mutex(const mutex&) = delete;
			void lock() {
				if (lock_count++) queue.wait();
			}
			bool try_lock() {
				if (lock_count == 0) {
					lock_count = 1;
					return true;
				}
				return false;
			}
			void unlock() {
				if (--lock_count) queue.release_one();
			}
		};


	}
}

#endif
