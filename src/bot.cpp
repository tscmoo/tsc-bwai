//
// This file implements the bot interface.
// There isn't much, since everything has its own task which is run
// by multitasking.resume().
//

#include "bot.h"
#include <mutex>

void tsc_bwai::bot_t::on_frame() {
	current_frame = game->getFrameCount();
	latency_frames = game->getRemainingLatencyFrames();
	current_command_frame = current_frame + latency_frames;

	multitasking.resume();

	if (current_frame >= 30 && !send_text_queue.empty()) {
		game->sendText("%s", send_text_queue.front().c_str());
		send_text_queue.pop_front();
	}
}

void tsc_bwai::bot_t::send_text(const a_string& str) {
	if (current_frame < 30 || !send_text_queue.empty()) send_text_queue.push_back(str);
	else game->sendText("%s", str.c_str());
}

namespace {
	using namespace tsc_bwai;
	bool global_data_initialized = false;
	std::mutex init_global_data_mut;
	void init_global_data_if_necessary(bot_t& bot) {
		if (!global_data_initialized) {
			std::lock_guard<std::mutex> l(init_global_data_mut);
			if (!global_data_initialized) {
				global_data_initialized = true;
				unit_types::init_global_data(bot);
				upgrade_types::init_global_data(bot);
			}
		}
	}
}

void tsc_bwai::bot_t::init() {

	init_global_data_if_necessary(*this);

	players.init();

	for (auto& v : game->getStartLocations()) {
		start_locations.emplace_back(((bwapi_pos)v).x * 32, ((bwapi_pos)v).y * 32);
	}
	my_start_location = xy(((bwapi_pos)players.self->getStartLocation()).x * 32, ((bwapi_pos)players.self->getStartLocation()).y * 32);

	grid.init();
	stats.init();
	units.init();
	//square_pathing.init();
	unit_controls.init();
	//resource_gathering.init();
	//resource_spots.init();
	//creep.init();
	//pylons.init();
	//build_spot_finder.init();
	//build.init();
	//buildpred.init();
	//combat.init();
	//scouting.init();
	//get_upgrades.init();
	//wall_in.init();
	//strategy.init();
	//buildopt.init();

}

