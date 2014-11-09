


namespace stats {
;

void stats_task() {

	a_deque<int> avg_res_hist[3];

	while (true) {
		BWAPI::Player p = game->self();

		current_minerals = (double)p->minerals();
		current_gas = (double)p->gas();

		current_used_supply[race_terran] = (double)p->supplyUsed(BWAPI::Races::Terran) / 2.0;
		current_max_supply[race_terran] = (double)p->supplyTotal(BWAPI::Races::Terran) / 2.0;
		current_used_supply[race_protoss] = (double)p->supplyUsed(BWAPI::Races::Protoss) / 2.0;
		current_max_supply[race_protoss] = (double)p->supplyTotal(BWAPI::Races::Protoss) / 2.0;
		current_used_supply[race_zerg] = (double)p->supplyUsed(BWAPI::Races::Zerg) / 2.0;
		current_max_supply[race_zerg] = (double)p->supplyTotal(BWAPI::Races::Zerg) / 2.0;

		static const int resource_per_frame_average_size = 15*30;

		auto calculate_average = [&](double&dst,a_deque<int>&hist,int current) {
			if (hist.size()>=resource_per_frame_average_size) hist.pop_front();
			hist.push_back(current);
			int total_difference = 0;
			for (size_t i=1;i<hist.size();++i) total_difference += hist[i] - hist[i-1];
			dst = (double)total_difference / hist.size();
		};
		calculate_average(current_minerals_per_frame,avg_res_hist[0],p->gatheredMinerals());
		calculate_average(current_gas_per_frame,avg_res_hist[1],p->gatheredGas());

		multitasking::sleep(1);
	}

}

void init() {

	multitasking::spawn(0.1,stats_task,"stats");

}

}

