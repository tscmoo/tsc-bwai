
struct strat_tvz {


	void run() {

		combat::no_aggressive_groups = true;

		while (true) {

			using namespace buildpred;

			int my_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();
			int my_goliath_count = my_units_of_type[unit_types::goliath].size();

			int enemy_mutalisk_count = 0;
			int enemy_guardian_count = 0;
			int enemy_lurker_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::mutalisk) ++enemy_mutalisk_count;
				if (e->type == unit_types::guardian || e->type == unit_types::cocoon) ++enemy_guardian_count;
				if (e->type == unit_types::lurker || e->type == unit_types::lurker_egg) ++enemy_lurker_count;
			}

			if (my_tank_count >= 12 || my_goliath_count >= 8) combat::no_aggressive_groups = false;
			if (my_tank_count + my_goliath_count < 8) combat::no_aggressive_groups = true;

			int desired_science_vessel_count = (enemy_lurker_count + 3) / 4;
			int desired_goliath_count = 2 + enemy_mutalisk_count + enemy_guardian_count * 2;
			int desired_wraith_count = (my_tank_count + my_goliath_count) / 8;
			int desired_valkyrie_count = std::min(enemy_mutalisk_count / 4, desired_goliath_count / 2);

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
					int valkyrie_count = count_units_plus_production(st, unit_types::valkyrie);
					if (valkyrie_count < desired_valkyrie_count) {
						return maxprod(st, unit_types::valkyrie, backbone);
					}
					int wraith_count = count_units_plus_production(st, unit_types::wraith);
					if (wraith_count < desired_wraith_count) {
						return maxprod(st, unit_types::wraith, backbone);
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
				if (buildpred::get_my_current_state().bases.size() <= 2 && (my_tank_count < 12 && my_goliath_count < 8)) return false;
				return is_long_distance_mining();
			};

			execute_build(can_expand(), build);

			multitasking::sleep(15 * 10);
		}

	}

};

