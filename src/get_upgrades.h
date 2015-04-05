

namespace get_upgrades {
;
a_unordered_map<upgrade_type*, double> upgrade_value_overrides;
a_unordered_map<upgrade_type*, double> upgrade_order_overrides;
void set_upgrade_value(upgrade_type*upg, double value) {
	upgrade_value_overrides[upg] = value;
}
void set_upgrade_order(upgrade_type*upg, double value) {
	upgrade_order_overrides[upg] = value;
}
bool no_auto_upgrades = false;
void set_no_auto_upgrades(bool val) {
	for (auto&b : build::build_tasks) {
		if (b.type->upgrade && !b.upgrade_done_frame) {
			b.dead = true;
		}
	}
	no_auto_upgrades = val;
}

void get_upgrades() {

	for (auto&b : build::build_tasks) {
		if (b.type->upgrade && !b.upgrade_done_frame) {
			b.dead = true;
		}
	}

	a_vector<upgrade_type*> sorted_upgrades;
	for (auto&upg : upgrades::upgrade_type_container) {
		sorted_upgrades.push_back(&upg);
	}
	std::stable_sort(sorted_upgrades.begin(), sorted_upgrades.end(), [&](upgrade_type*a, upgrade_type*b) {
		return upgrade_order_overrides[a] < upgrade_order_overrides[b];
	});

	a_unordered_set<unit_type*> visited;
	for (auto*p_upg : sorted_upgrades) {
		auto&upg = *p_upg;
		if (upg.gas_cost && current_gas < upg.gas_cost && current_gas_per_frame == 0) continue;
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
		if (upgrade_value_overrides[&upg]) val = upgrade_value_overrides[&upg];
		else if (no_auto_upgrades) continue;
		if (sum >= val*1.5) {
			bool already_upgrading = false;
			for (build::build_task&b : build::build_tasks) {
				if (b.dead) continue;
				if (b.type->upgrade == &upg) {
					already_upgrading = true;
					break;
				}
			}
			if (!already_upgrading) {
				bool builder_found = false;
				for (unit*u : my_units_of_type[upg.builder_type]) {
					if (u->remaining_whatever_time >= 15 * 20) continue;
					builder_found = true;
					break;
				}
				double prio = val / 1000.0;
				if (builder_found) {
					if (current_minerals < 600 || current_gas >= upg.gas_cost) {
						build::add_build_sum(prio, &upg, 1);
					}
					break;
				} else {
					double mult = 3.0;
					if (!my_units_of_type[upg.builder_type].empty()) mult = 9.0;
					if (val > 0.0 && sum >= val * mult && !no_auto_upgrades) {
						build::add_build_sum(prio, upg.builder_type, 1);
						//break;
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

