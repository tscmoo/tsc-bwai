
struct strat_tvp {


	void run() {

		get_upgrades::set_upgrade_value(upgrade_types::siege_mode, 1.0);
		get_upgrades::set_upgrade_value(upgrade_types::spider_mines, 1.0);

// 		combat::combat_tactics::careful_siege = true;
// 		combat::combat_tactics::siege_requires_upgrade = false;

		combat::no_aggressive_groups = true;

		while (true) {

			using namespace buildpred;

			int my_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();
			int my_goliath_count = my_units_of_type[unit_types::goliath].size();

			int enemy_zealot_count = 0;
			int enemy_dragoon_count = 0;
			int enemy_observer_count = 0;
			int enemy_shuttle_count = 0;
			int enemy_scout_count = 0;
			int enemy_carrier_count = 0;
			int enemy_arbiter_count = 0;
			int enemy_corsair_count = 0;
			int enemy_cannon_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zealot) ++enemy_zealot_count;
				if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
				if (e->type == unit_types::observer) ++enemy_observer_count;
				if (e->type == unit_types::shuttle) ++enemy_shuttle_count;
				if (e->type == unit_types::scout) ++enemy_scout_count;
				if (e->type == unit_types::carrier) ++enemy_carrier_count;
				if (e->type == unit_types::arbiter) ++enemy_arbiter_count;
				if (e->type == unit_types::corsair) ++enemy_corsair_count;
				if (e->type == unit_types::photon_cannon) ++enemy_cannon_count;
			}
			
			if (my_tank_count >= 12) combat::no_aggressive_groups = false;
			if (enemy_cannon_count >= 3 && enemy_cannon_count >= my_tank_count) combat::no_aggressive_groups = false;
			if (my_tank_count < 6 && my_tank_count <= (enemy_dragoon_count + enemy_zealot_count) / 2) combat::no_aggressive_groups = true;

			int desired_science_vessel_count = enemy_arbiter_count;
			int desired_goliath_count = enemy_carrier_count * 4;
			desired_goliath_count += (enemy_shuttle_count + enemy_observer_count + 1) / 2;
			desired_goliath_count += enemy_scout_count + enemy_arbiter_count + enemy_corsair_count;
			if (my_tank_count >= 5) desired_goliath_count += 2;

			auto build = [&](state&st) {
				return nodelay(st, unit_types::scv, [&](state&st) {
					auto backbone = [&](state&st) {
						if (st.gas < 100) {
							return maxprod(st, unit_types::vulture, [&](state&st) {
								return maxprod1(st, unit_types::siege_tank_tank_mode);
							});
						}
						return maxprod(st, unit_types::siege_tank_tank_mode, [&](state&st) {
							return maxprod1(st, unit_types::vulture);
						});
					};
					int science_vessel_count = count_units_plus_production(st, unit_types::science_vessel);
					if (science_vessel_count < desired_science_vessel_count) {
						return maxprod(st, unit_types::science_vessel, backbone);
					}
					int goliath_count = count_units_plus_production(st, unit_types::goliath);
					if (goliath_count < desired_goliath_count) {
						return maxprod(st, unit_types::goliath, backbone);
					}
					
					return backbone(st);
				});
			};

			auto is_long_distance_mining = [&]() {
				int count = 0;
				for (auto&g : resource_gathering::live_gatherers) {
					if (!g.resource) continue;
					unit*ru = g.resource->u;
					resource_spots::spot*rs = nullptr;
					for (auto&s : resource_spots::spots) {
						if (grid::get_build_square(s.cc_build_pos).building) continue;
						for (auto&r : s.resources) {
							if (r.u == ru) {
								rs = &s;
								break;
							}
						}
						if (rs) break;
					}
					if (rs) ++count;
				}
				return count >= 8;
			};
			auto can_expand = [&]() {
				if (buildpred::get_my_current_state().bases.size() == 2 && (my_tank_count < 12 && my_goliath_count < 8)) return false;
				return is_long_distance_mining();
			};

			execute_build(can_expand(), build);

			multitasking::sleep(15 * 10);
		}

	}

};

