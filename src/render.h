
namespace render {
;

typedef int render_func_handle;
a_vector<std::tuple<std::function<void()>, int>> render_funcs;
render_func_handle add(std::function<void()> f) {
	static int next_handle = 0;
	int h = ++next_handle;;
	render_funcs.emplace_back(std::move(f), h);
	return h;
}
void rm(render_func_handle h) {
	for (auto i = render_funcs.begin(); i != render_funcs.end(); ++i) {
		if (std::get<1>(*i) == h) {
			render_funcs.erase(i);
			return;
		}
	}
}

//#include "render_text.h"



a_map<xy,int> text_stack_count;
a_map<std::pair<int,int>,int> text_screen_stack_count;

BWAPI::Color build_color = BWAPI::Colors::Orange;

void draw_stacked_text(xy pos,const char*text) {
	pos.y += text_stack_count[pos]++*10;
	game->drawTextMap(pos.x,pos.y,"\x11%s",text);
};

void draw_screen_stacked_text(int x,int y,const char*text) {
	y += text_screen_stack_count[std::make_pair(x,y)]++*10;
	game->drawTextScreen(x,y,"\x11%s",text);
	//render_text::render_text(x, y, text, BWAPI::Colors::White);
};

void render_task_list() {

	static a_map<multitasking::task_id, double> last_cpu_time;
	static double last_now = multitasking::detail::reference_time;
	double now = multitasking::detail::reference_time;

	//double elapsed = now - last_now;
	double elapsed = multitasking::desired_frame_time;
	last_now = now;

	static a_deque<a_map<multitasking::task_id, double>> cpu_hist;
	if (cpu_hist.size() == 120) cpu_hist.pop_front();
	{
		using namespace multitasking;
		using namespace multitasking::detail;

		a_map<task_id, double> cpu;

		for (task_id id : running_tasks) {
			scheduled_task&t = tasks[id];
			auto it = last_cpu_time.find(id);
			double percent = it == last_cpu_time.end() ? 0 : (t.cpu_time - it->second) / elapsed;
			cpu[id] = percent;
			last_cpu_time[id] = t.cpu_time;
		}
		cpu_hist.push_back(std::move(cpu));
		a_vector<std::tuple<double, task_id>> sorted_list;
		for (task_id id : running_tasks) {
			double sum = 0.0;
			for (auto&v : cpu_hist) sum += v[id];
			sorted_list.emplace_back(sum / cpu_hist.size(), id);
		}
		std::sort(sorted_list.begin(), sorted_list.end());

		for (size_t i = 0; i < sorted_list.size(); ++i) {
			auto&v = sorted_list[i];
			task_id id = std::get<1>(v);
			double percent = std::get<0>(v);

			auto&t = tasks[id];

			int x = 480;
			int y = 300;
			y -= i * 10;

			const char*text = format("%02d  %s", (int)(percent * 100), t.name);

			game->drawTextScreen(x, y, "\x11%s", text);
		}
	}

}

void render() {

	text_stack_count.clear();
	text_screen_stack_count.clear();

	static double fps = 0;
	static int frames = 0;
	++frames;
	static DWORD last_fps = GetTickCount();
	DWORD now = GetTickCount();
	static int last_test = 0;
	static int test = 0;
	if (now-last_fps>=1000) {
		fps = (double)frames / (now-last_fps) *1000;
		last_fps = now;
		frames = 0;
	}

	static a_deque<double> avg_load_vec;
	if (avg_load_vec.size()>=16) avg_load_vec.pop_front();
	avg_load_vec.push_back(multitasking::last_frame_time/multitasking::desired_frame_time);
	double avg_load = 0.0;
	for (auto&v : avg_load_vec) avg_load += v;
	avg_load /= avg_load_vec.size();

	bool render_stuff = test_mode;
	//render_stuff = true;

	draw_screen_stacked_text(16,8,format("FPS: %.02f  load: %02d%%  carry: %.02f   APM: %d",fps,(int)(avg_load*100),-multitasking::detail::frame_time_carry,game->getAPM()));
	if (render_stuff) draw_screen_stacked_text(16, 8, format("%02d:%02d (%02d:%02d)  Income: %.02f/%.02f  Predicted: %.02f/%.02f", current_frame / 15 / 60, current_frame / 15 % 60, current_frame / 24 / 60, current_frame / 24 % 60, current_minerals_per_frame, current_gas_per_frame, predicted_minerals_per_frame, predicted_gas_per_frame));
	else draw_screen_stacked_text(16, 8, format("%02d:%02d (%02d:%02d)", current_frame / 15 / 60, current_frame / 15 % 60, current_frame / 24 / 60, current_frame / 24 % 60));

	if (render_stuff) {
		render_task_list();

		for (auto&v : render_funcs) std::get<0>(v)();
	}

// 	BWAPI::Position mpos = game->getScreenPosition() + game->getMousePosition();
// 	xy pos(mpos.x, mpos.y);
// 	game->drawTextMap(mpos.x / 8 * 8, mpos.y / 8 * 8, "-------- %d %d %d %d", mpos.x / 8, mpos.y / 8, grid::get_walk_square(pos).buildings.size(), grid::get_walk_square(pos).walkable);
// 	game->drawLineMap(mpos.x, mpos.y, 0, 0, BWAPI::Colors::Green);

}


}
