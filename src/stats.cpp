//
// This file implements the stats module.
//

#include "stats.h"
#include "common.h"
#include "bwapi_inc.h"
#include "bot.h"

void tsc_bwai::stats_module::stats_task() {
	a_deque<int> avg_res_hist[3];

	while (true) {
		BWAPI_Player p = bot.players.self;

		bot.current_minerals = (double)p->minerals();
		bot.current_gas = (double)p->gas();

		bot.current_used_supply[race_terran] = (double)p->supplyUsed(BWAPI::Races::Terran) / 2.0;
		bot.current_max_supply[race_terran] = (double)p->supplyTotal(BWAPI::Races::Terran) / 2.0;
		bot.current_used_supply[race_protoss] = (double)p->supplyUsed(BWAPI::Races::Protoss) / 2.0;
		bot.current_max_supply[race_protoss] = (double)p->supplyTotal(BWAPI::Races::Protoss) / 2.0;
		bot.current_used_supply[race_zerg] = (double)p->supplyUsed(BWAPI::Races::Zerg) / 2.0;
		bot.current_max_supply[race_zerg] = (double)p->supplyTotal(BWAPI::Races::Zerg) / 2.0;

		bot.current_used_total_supply = bot.current_used_supply[race_terran] + bot.current_used_supply[race_protoss] + bot.current_used_supply[race_zerg];

		static const int resource_per_frame_average_size = 15 * 30;

		auto calculate_average = [&](double& dst, a_deque<int>& hist, int current) {
			if (hist.size() >= resource_per_frame_average_size) hist.pop_front();
			hist.push_back(current);
			int total_difference = 0;
			for (size_t i = 1; i < hist.size(); ++i) total_difference += hist[i] - hist[i - 1];
			dst = (double)total_difference / hist.size();
		};
		calculate_average(bot.current_minerals_per_frame, avg_res_hist[0], p->gatheredMinerals());
		calculate_average(bot.current_gas_per_frame, avg_res_hist[1], p->gatheredGas());

		bot.multitasking.sleep(1);
	}
}

void tsc_bwai::stats_module::init() {

	bot.multitasking.spawn(0.0, std::bind(&stats_module::stats_task, this), "stats");

}

