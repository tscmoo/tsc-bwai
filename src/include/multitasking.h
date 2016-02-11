//
// This file defines the multitasking interface.
// Each "task" represents a user-thread, or fiber. They have their own
// stack and control flow, but are not executed asynchronously, they do not
// have their own OS thread.
// Tasks are created by the spawn call, which take a priority (lower value
// is higher priority), a function object and a name. The function object 
// will be run by the scheduler later. The name is only for debugging
// purposes.
// All tasks should explicitly yield (e.g. sleep) or call yield_point
// periodically. The program will remain unresponsive until they do.
// Control is initially passed to the scheduler by the run function, which 
// should be called once per frame.
// The scheduler effectively sorts all tasks by the time since they were last
// run, multiplied by the priority, and runs the first one. Most tasks should
// have the default priority of 1.0.
//
// Functions that operate on the currently running task have undefined behavior
// if there is no running task. They should only be called from within the
// function associated with the task.
//

#ifndef TSC_BWAI_MULTITASKING_H
#define TSC_BWAI_MULTITASKING_H

#include <exception>
#include <functional>

namespace multitasking {

	// This specifies in seconds the amount of time we have available per
	// call to resume. Because of the fact that tasks only call yield_point
	// periodically, this is only an average, and the actual time spent
	// can exceed it by up to the longest time between two yield calls
	// in a running task.
	// If the time is exceeded, then the over-bounding time will be subtracted
	// from the available time during the next resume (up to a threshold).
	const double desired_frame_time = 1.0 / 30.0;

	// Each task will be scheduled for
	// (desired_frame_time / schedule_granularity_divisor) time, at which point
	// the scheduler will be run, but the task is left in the running state.
	// The higher this value, the more evenly time will be divided per task
	// per frame, but there will also be more overhead from running the
	// scheduler more often.
	const double schedule_granularity_divisor = 2.0;

	// These exceptions are used internally to terminate tasks. Don't catch
	// them.
	struct signal : std::exception {};
	struct sig_term : signal {
		virtual const char* what() const {
			return "sigterm";
		}
	};

	typedef size_t task_id;
	static const task_id invalid_task_id = ~(task_id)0;

	// Returns the id of the currently running task.
	task_id current_task_id();

	// Checks if it's time to run the scheduler, or yield to the next frame.
	// Call this periodically to make sure we don't take too long to process
	// each frame.
	// If the task is doing any calculations that might take anywhere close to
	// desired_frame_time seconds, then this should be called at least once
	// during those calculations. Calling it too often can incur a small overhead.
	void yield_point();

	// Spawns a new task.
	// Returns the id of the new task. If stop has been called, no task will
	// be created and invalid_task_id returned.
	// Throws an exception if the maximum number of tasks has been created.
	task_id spawn(double prio, std::function<void()> f, const char*name);

	// Spawns a new task
	template<typename F>
	task_id spawn(double prio, F&& f, const char*name) {
		return spawn(prio, std::function<void()>(f), name);
	}

	// Spawns a new task with priority 1.0
	template<typename F>
	task_id spawn(F&& f, const char*name) {
		return spawn(1.0, std::forward<F>(f), name);
	}

	// Yield for at least the specified number of frames
	void sleep(int frames);

	// Yield until the specified task terminates
	void join(task_id id);

	// Increment the current tasks wait counter by 1, and run the scheduler.
	// The task will be suspended until the wait counter is <= 0. If the
	// wait counter is already < 0, then the task will not be suspended, but
	// the scheduler might still decided to run some other task first.
	void wait();
	// Decrement the specified tasks wait counter, and run the scheduler
	// if the specified task has a lower priority value than the current task.
	void wake(task_id id);

	// Return the total cpu time used by the specified task.
	// id can be the id of currently running task.
	double get_cpu_time(task_id id);

	// This should be called once per frame, from outside any task.
	// It runs the scheduler, and does not return until all tasks are idle
	// or desired_frame_time has been used.
	void resume();

	// Terminates all tasks, and waits until no tasks are running.
	// Undefined behavior if called from a running task.
	void stop();

	// Call this only once during startup.
	void init();

}

#endif
