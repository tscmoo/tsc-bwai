//
// This file defines render_module, which provides some helper methods for
// drawing on the screen (mostly just a callback that gets called once per
// frame).
//

#ifndef TSC_BWAI_RENDER_H
#define TSC_BWAI_RENDER_H

#include "common.h"
#include "multitasking.h"
#include "tsc/high_resolution_timer.h"
#include <functional>

namespace tsc_bwai {

	class bot_t;

	class render_module {
		bot_t& bot;

		int next_handle = 0;
		a_vector<std::tuple<std::function<void()>, int>> render_funcs;

		a_map<xy, int> text_stack_count;
		a_map<std::pair<int, int>, int> text_screen_stack_count;

		a_map<multitasking::task_id, double> last_cpu_time;

		double fps = 0.0;
		int fps_frame_counter = 0;
		tsc::high_resolution_timer fps_timer;

		a_deque<double> avg_load_vec;

		a_deque<a_map<multitasking::task_id, double>> cpu_hist;

		void render_task_list();

	public:
		render_module(bot_t& bot);

		typedef int render_func_handle;

		render_func_handle add(const std::function<void()>& f);
		void rm(render_func_handle h);

		void draw_stacked_text(xy pos, const a_string& text);
		void draw_screen_stacked_text(int x, int y, const a_string& text);

		void init();

		// This is called directly from bot_t::on_frame. It can not be part of
		// a task, since it must be run every single frame.
		void render();
	};

}

#endif
