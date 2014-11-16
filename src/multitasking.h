

namespace multitasking {
;

double desired_frame_time = 1.0 / 30.0;
double last_frame_time;

double schedule_granularity_divisor = 2.0;

struct signal: std::exception {};

struct sig_term: signal {
	virtual const char* what() const {
		return "sigterm";
	}
};

typedef size_t task_id;
static const task_id invalid_task_id = ~(task_id)0;

namespace detail {

	tsc::ut_impl::ut_t main_ut;
	a_list<std::function<void()>> thread_funcs;
	a_vector<std::tuple<tsc::ut_impl::ut_t,std::function<void()>*>> thread_pool;
#ifdef _WIN32
	static void WINAPI thread_entry(void*ptr) {
#else
	static void thread_entry(void*ptr) {
#endif
		auto&f = *(std::function<void()>*)ptr;
		while (true) {
			f();
		}
	}

	struct scheduled_task {
		double priority = 1.0;
		double last_schedule_time = 0.0;
		double cpu_time = 0.0;
		tsc::ut_impl::ut_t thread;
		enum {sig_no_signal, sig_term};
		int raise_signal = sig_no_signal;
		int sleep_until = 0;
		int wait_counter = 0;
		task_id join_task = invalid_task_id;
		a_vector<task_id> joiners;
		const char*name = nullptr;
	};

	a_vector<scheduled_task> tasks;
	a_deque<scheduled_task*> free_tasks;
	tsc::dynamic_bitset running_tasks;
	scheduled_task*current_task;
	bool dont_yield, dont_spawn;
	tsc::high_resolution_timer reference_timer;
	double reference_time;
	double next_schedule_time, schedule_time_slice;
	double next_frame_yield;
	double frame_time_carry;
	int resume_frame;

	task_id get_task_id(scheduled_task*t) {
		return t - tasks.data();
	}

	double time() {
		return reference_time + reference_timer.elapsed();
	}

	void check_signals() {
		if (current_task->raise_signal) {
			int sig = current_task->raise_signal;
			if (sig==scheduled_task::sig_term) throw sig_term();
			std::terminate();
		}
	}

	void resume_task(scheduled_task*t) {
		current_task = t;
		tsc::ut_impl::switch_to(t->thread);
	}

	bool schedule() {

		double now = time();
		scheduled_task*switch_to = 0;
		double lowest = std::numeric_limits<double>::infinity();
		//log("schedule(): \n");
		for (task_id id : running_tasks) {
			//log("%d is running\n",id);
			scheduled_task&t = tasks[id];
			if (!t.raise_signal) {
				if (t.wait_counter > 0) continue;
				if (t.sleep_until && current_frame<t.sleep_until) continue;
				if (t.join_task!=invalid_task_id) continue;
			}
			double ago = (t.last_schedule_time-now) / t.priority;
			if (ago<lowest) {
				switch_to = &t;
				lowest = ago;
			}
		}
		if (current_task) {
			current_task->cpu_time += now - current_task->last_schedule_time;
		}
		if (switch_to) {
			switch_to->last_schedule_time = now;
			if (switch_to->sleep_until) switch_to->sleep_until = 0;
			next_schedule_time = now + schedule_time_slice;
			if (switch_to==current_task) {
				//log("%g: schedule() best task was current task (%d), next_schedule_time is %g\n",now,get_task_id(switch_to),next_schedule_time);
				if (current_task->raise_signal) check_signals();
				return true;
			}
			//log("%g: schedule() switching to task %d, next_schedule_time is %g\n",now,get_task_id(switch_to),next_schedule_time);
			resume_task(switch_to);
			if (current_task && current_task->raise_signal) check_signals();
			return true;
		}
		//log("schedule() found no task\n");
		if (current_task) {
			current_task = 0;
			tsc::ut_impl::switch_to(main_ut);
			if (current_task->raise_signal) check_signals();
		}
		return false;

	}

	void yield_point() {
		if (dont_yield) return;
		double now = time();
		if (now>=next_frame_yield) {
			//log("frame yield!\n");
			tsc::ut_impl::switch_to(main_ut);
			if (current_task && current_task->raise_signal) check_signals();
			now = time();
		}
		if (now>=next_schedule_time) {
			schedule();
		}
	}

	void resume() {

		reference_timer.reset();

		schedule_time_slice = desired_frame_time / schedule_granularity_divisor;
		next_frame_yield = reference_time + desired_frame_time + frame_time_carry;

		//log("reference_time is %g, next_frame_yield is %g, schedule_time_slice is %g\n",reference_time,next_frame_yield,schedule_time_slice);

		//if (current_task) log("current_task is %d\n",get_task_id(current_task));
		if (next_frame_yield>reference_time) {
			if (current_task) resume_task(current_task);
			else schedule();
		}
		last_frame_time = reference_timer.elapsed();

		reference_time += last_frame_time;
		frame_time_carry = next_frame_yield - reference_time;
		if (frame_time_carry>0) frame_time_carry = 0;
		else if (frame_time_carry<-desired_frame_time) frame_time_carry = -desired_frame_time;
	}

	task_id spawn(double prio,std::function<void()> f,const char*name) {
		if (dont_spawn) return invalid_task_id;
		if (free_tasks.empty()) xcept("too many tasks");
		scheduled_task*t = free_tasks.front();
		free_tasks.pop_front();

		task_id id = get_task_id(t);
		//log("spawning new task with id %d\n",id);
		running_tasks.set(id);
		t->priority = prio;
		t->name = name;
		t->cpu_time = 0.0;
		t->raise_signal = scheduled_task::sig_no_signal;
		t->sleep_until = 0;
		t->join_task = invalid_task_id;
		
		if (thread_pool.empty()) {
			thread_funcs.emplace_back();
			thread_pool.emplace_back(tsc::ut_impl::create(&thread_entry,&thread_funcs.back()),&thread_funcs.back());
		}
		auto v = thread_pool.back();
		thread_pool.pop_back();
		t->thread = std::get<0>(v);
		*std::get<1>(v) = [f = std::move(f),t,v]() {
			try {
				current_task = t;
				check_signals();
				f();
			} catch (const sig_term&) {
				log("%s: terminated\n", current_task->name);
			} catch (const std::exception&e) {
				log("%s: caught exception : %s\n", current_task->name, e.what());
				std::terminate();
			} catch (const char*str) {
				log("%s: caught exception : %s\n", current_task->name, str);
				std::terminate();
			} catch (...) {
				log("%s: caught unknown exception\n", current_task->name);
				std::terminate();
			}
			if (!current_task->joiners.empty()) {
				for (task_id id : current_task->joiners) {
					tasks[id].join_task = invalid_task_id;
				}
				current_task->joiners.clear();
			}
			if (current_task->join_task!=invalid_task_id) {
				auto&j = tasks[current_task->join_task].joiners;
				j.erase(std::find(j.begin(),j.end(),get_task_id(current_task)));
				current_task->join_task = invalid_task_id;
			}
			//log("task %d has exited!\n",get_task_id(current_task));
			running_tasks.reset(get_task_id(current_task));
			free_tasks.push_back(current_task);
			current_task = 0;

			thread_pool.push_back(v);

			yield_point();
			if (!schedule()) tsc::ut_impl::switch_to(main_ut);
		};

		return id;
	}

	void sleep(int frames) {
		current_task->sleep_until = current_frame + frames;
		schedule();
	}
	void join(task_id id) {
		current_task->join_task = id;
		tasks[id].joiners.push_back(get_task_id(current_task));
		schedule();
	}
	void wait() {
		++current_task->wait_counter;
		schedule();
	}
	bool try_wait() {
		if (current_task->wait_counter < 0) {
			++current_task->wait_counter;
			return true;
		}
		return false;
	}
	void wake(task_id id) {
		auto&t = tasks[id];
		if (--t.wait_counter == 0) {
			if (t.priority < current_task->priority) schedule();
		}
	}

	double get_cpu_time(task_id id) {
		if (current_task && get_task_id(current_task) == id) return tasks[id].cpu_time + (time() - current_task->last_schedule_time);
		return tasks[id].cpu_time;
	}

	void terminate_all() {
		for (task_id id : running_tasks) {
			tasks[id].raise_signal = scheduled_task::sig_term;
		}
	}

	void init() {

		tasks.resize(0x100);
		for (auto&v : tasks) free_tasks.push_back(&v);
		running_tasks.resize(tasks.size());

	}
}

task_id current_task_id() {
	return detail::get_task_id(detail::current_task);
}

void yield_point() {
	detail::yield_point();
}

template<typename F>
task_id spawn(double prio, F&&f, const char*name) {
	return detail::spawn(prio, std::forward<F>(f), name);
}

template<typename F>
task_id spawn(F&&f, const char*name) {
	return spawn(1.0, std::forward<F>(f), name);
}

void sleep(int frames) {
	detail::sleep(frames);
}

void join(task_id id) {
	detail::join(id);
}

void wait() {
	detail::wait();
}
bool try_wait() {
	return detail::try_wait();
}
void wake(task_id id) {
	detail::wake(id);
}

double get_cpu_time(task_id id) {
	return detail::get_cpu_time(id);
}

void run() {

	detail::resume();

}

void stop() {
	detail::terminate_all();
	detail::dont_yield = true;
	detail::dont_spawn = true;
	while (detail::running_tasks.any()) detail::resume();
	detail::resume();
	if (detail::running_tasks.any()) xcept("unreachable");
}

void init() {

	tsc::ut_impl::enter(detail::main_ut);
	detail::init();

}

}

