//
// This file implements render_module.
//

#include "render.h"
#include "bot.h"
using namespace tsc_bwai;

render_module::render_func_handle render_module::add(const std::function<void()>& f) {
	int h = ++next_handle;
	render_funcs.emplace_back(std::move(f), h);
	return h;
}
void render_module::rm(render_func_handle h) {
	for (auto i = render_funcs.begin(); i != render_funcs.end(); ++i) {
		if (std::get<1>(*i) == h) {
			render_funcs.erase(i);
			return;
		}
	}
}

void render_module::draw_stacked_text(xy pos, const a_string& text) {
	pos.y += text_stack_count[pos]++ * 10;
	bot.game->drawTextMap(pos.x, pos.y, "\x11%s", text.c_str());
};

void render_module::draw_screen_stacked_text(int x, int y, const a_string& text) {
	y += text_screen_stack_count[std::make_pair(x, y)]++ * 10;
	bot.game->drawTextScreen(x, y, "\x11%s", text.c_str());
	//render_text::render_text(x, y, text, BWAPI::Colors::White);
};

void render_module::render_task_list() {

	double now = bot.multitasking.time();

	if (cpu_hist.size() == 120) cpu_hist.pop_front();

	auto cpu_times = bot.multitasking.get_all_cpu_times();

	a_map<multitasking::task_id, double> cpu;
	for (auto& v : cpu_times) {
		auto it = last_cpu_time.find(v.first);
		double percent = it == last_cpu_time.end() ? 0 : (v.second - it->second) / bot.multitasking.desired_frame_time;
		cpu[v.first] = percent;
	}

	last_cpu_time = cpu_times;

	cpu_hist.push_back(std::move(cpu));
	a_vector<std::tuple<double, multitasking::task_id>> sorted_list;
	for (auto&v : cpu_times) {
		auto id = v.first;
		double sum = 0.0;
		for (auto& v : cpu_hist) sum += v[id];
		sorted_list.emplace_back(sum / cpu_hist.size(), id);
	}
	std::sort(sorted_list.begin(), sorted_list.end());

	for (size_t i = 0; i < sorted_list.size(); ++i) {
		auto&v = sorted_list[i];
		auto id = std::get<1>(v);
		double percent = std::get<0>(v);

		int x = 480;
		int y = 300;
		y -= i * 10;

		a_string text = format("%02d  %s", (int)(percent * 100), bot.multitasking.get_name(id));

		bot.game->drawTextScreen(x, y, "\x11%s", text.c_str());
	}

}

void render_module::render() {

	text_stack_count.clear();
	text_screen_stack_count.clear();

	++fps_frame_counter;
	double elapsed = fps_timer.elapsed();
	if (elapsed >= 1.0) {
		fps_timer.reset();
		fps = (double)fps_frame_counter / elapsed;
		fps_frame_counter = 0;
	}

	if (avg_load_vec.size() >= 16) avg_load_vec.pop_front();
	avg_load_vec.push_back(bot.multitasking.get_last_frame_time() / bot.multitasking.desired_frame_time);
	double avg_load = 0.0;
	for (auto&v : avg_load_vec) avg_load += v;
	avg_load /= avg_load_vec.size();

	bool render_stuff = bot.test_mode;
	//render_stuff = true;

	draw_screen_stacked_text(16, 8, format("FPS: %.02f  load: %02d%%  carry: %.02f   APM: %d", fps, (int)(avg_load * 100), 0.0, bot.game->getAPM()));
	if (render_stuff) draw_screen_stacked_text(16, 8, format("%02d:%02d (%02d:%02d)  Income: %.02f/%.02f  Predicted: %.02f/%.02f", bot.current_frame / 15 / 60, bot.current_frame / 15 % 60, bot.current_frame / 24 / 60, bot.current_frame / 24 % 60, bot.current_minerals_per_frame, bot.current_gas_per_frame, bot.predicted_minerals_per_frame, bot.predicted_gas_per_frame));
	else draw_screen_stacked_text(16, 8, format("%02d:%02d (%02d:%02d)", bot.current_frame / 15 / 60, bot.current_frame / 15 % 60, bot.current_frame / 24 / 60, bot.current_frame / 24 % 60));

	if (render_stuff) {
		render_task_list();

		for (auto&v : render_funcs) std::get<0>(v)();
	}

}

void render_module::init() {

}

render_module::render_module(bot_t& bot) : bot(bot) {
	fps_timer.reset();
}
