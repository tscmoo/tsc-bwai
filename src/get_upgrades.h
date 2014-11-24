

namespace get_upgrades {
;

void get_upgrades() {

	a_unordered_set<unit_type*> visited;
	for (auto&upg : upgrades::upgrade_type_container) {
		if (upg.gas_cost && current_gas_per_frame == 0) continue;
		if (upg.prev && !players::my_player->upgrades.count(upg.prev)) continue;
		if (players::my_player->upgrades.count(&upg)) continue;
		double sum = 0.0;
		for (unit_type*ut : upg.what_uses) {
			if (ut->is_worker) continue;
			double val = ut->total_minerals_cost + ut->total_gas_cost;
			sum += my_units_of_type[ut].size()*val;
		}
		double val = upg.minerals_cost + upg.gas_cost;
		visited.clear();
		std::function<void(unit_type*)> add = [&](unit_type*ut) {
			if (!visited.insert(ut).second) return;
			if (!my_units_of_type[ut].empty()) return;
			val += ut->minerals_cost + ut->gas_cost;
			for (unit_type*req : ut->required_units) add(req);
		};
		for (unit_type*req : upg.required_units) add(req);
		if (&upg == upgrade_types::spider_mines) val = 1.0;
		if (sum >= val*1.5) {
			bool already_upgrading = false;
			for (build::build_task&b : build::build_tasks) {
				if (b.type->upgrade == &upg) {
					already_upgrading = true;
					break;
				}
			}
			if (!already_upgrading) {
				bool builder_found = false;
				for (unit*u : my_units_of_type[upg.builder_type]) {
					if (u->remaining_whatever_time) continue;
					builder_found = true;
					break;
				}
				if (builder_found) {
					build::add_build_sum(0, &upg, 1);
					break;
				} else {
					if (sum >= val * 3) {
						build::add_build_sum(0, upg.builder_type, 1);
						break;
					}
				}
			}
		}

	}

}

void get_upgrades_task() {

	while (true) {
		multitasking::sleep(60);

		get_upgrades();

	}

}

void init() {

	multitasking::spawn(get_upgrades_task, "get upgrades");

}

}

