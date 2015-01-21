
struct strat_tvz_opening {

	wall_in::wall_builder wall;

	void run() {

		combat::no_aggressive_groups = true;
		combat::aggressive_wraiths = true;
		get_upgrades::no_auto_upgrades = true;

		using namespace buildpred;

		resource_gathering::max_gas = 100;
		int wall_calculated = 0;
		bool has_wall = false;
		bool has_zergling_tight_wall = false;
		bool being_zergling_rushed = false;
		while (true) {

			int my_marine_count = my_units_of_type[unit_types::marine].size();
			int enemy_zergling_count = 0;
			int enemy_spire_count = 0;
			int enemy_mutalisk_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::mutalisk) ++enemy_mutalisk_count;
				if (e->type == unit_types::spire || e->type == unit_types::greater_spire) ++enemy_spire_count;
				if (e->type == unit_types::zergling) ++enemy_zergling_count;
			}
			if (enemy_spire_count == 0 && enemy_mutalisk_count) enemy_spire_count = 1;

			auto build = [&](state&st) {
				st.dont_build_refineries = true;
				if (my_completed_units_of_type[unit_types::vulture].empty()) {
					if (enemy_zergling_count > my_marine_count && !being_zergling_rushed) {
						log("waa being zergling rushed!\n");
						being_zergling_rushed = true;
					}
					bool initial_marine = !st.units[unit_types::barracks].empty() && count_units_plus_production(st, unit_types::marine) == 0;
					if ((being_zergling_rushed && my_marine_count < 5) || initial_marine) {
						if (being_zergling_rushed) {
							if (!scouting::all_scouts.empty()) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
						}
						return nodelay(st, unit_types::marine, [&](state&st) {
							return nodelay(st, unit_types::scv, [&](state&st) {
								return depbuild(st, state(st), unit_types::vulture);
							});
						});
					}
				}
				int scv_count = count_units_plus_production(st, unit_types::scv);
				if (scv_count >= 11 && count_units_plus_production(st, unit_types::barracks) == 0) return depbuild(st, state(st), unit_types::barracks);
				if (scv_count >= 12 && count_units_plus_production(st, unit_types::refinery) == 0) return depbuild(st, state(st), unit_types::refinery);
				if (scv_count >= 16 && count_units_plus_production(st, unit_types::factory) == 0) return depbuild(st, state(st), unit_types::factory);
				return nodelay(st, unit_types::scv, [&](state&st) {

					std::function<bool(state&)> army = [&](state&st) {
						return depbuild(st, state(st), unit_types::marine);
					};
					army = [army](state&st) {
						return nodelay(st, unit_types::vulture, army);
					};

					if (count_units_plus_production(st, unit_types::starport)) {
						if (count_units_plus_production(st, unit_types::armory) == 0) {
							army = [army](state&st) {
								return nodelay(st, unit_types::armory, army);
							};
						}
						if (count_units_plus_production(st, unit_types::engineering_bay) == 0) {
							army = [army](state&st) {
								return nodelay(st, unit_types::engineering_bay, army);
							};
						}
					}

					if (count_units_plus_production(st, unit_types::wraith) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::wraith, army);
						};
					}

					int vulture_count = count_units_plus_production(st, unit_types::vulture);
					if (vulture_count <= enemy_zergling_count / 3) {
						army = [army](state&st) {
							return nodelay(st, unit_types::vulture, army);
						};
					} else {
						int machine_shops = count_production(st, unit_types::machine_shop);
						for (auto&v : st.units[unit_types::factory]) {
							if (v.has_addon) ++machine_shops;
						}
						if (machine_shops == 0) {
							army = [army](state&st) {
								return nodelay(st, unit_types::machine_shop, army);
							};
						}
					}

					return army(st);
				});
			};

			if (!my_units_of_type[unit_types::factory].empty()) {
				resource_gathering::max_gas = 250;
			}
			if (!my_units_of_type[unit_types::vulture].empty()) {
				resource_gathering::max_gas = 0.0;
			}
			int vulture_count = my_completed_units_of_type[unit_types::vulture].size();
			int siege_tank_count = my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size();
			int marine_count = my_completed_units_of_type[unit_types::marine].size();
			int wraith_count = my_completed_units_of_type[unit_types::wraith].size();
			auto my_st = get_my_current_state();
			if (!my_units_of_type[unit_types::wraith].empty()) {
				if (enemy_spire_count) {
					for (auto&b : build::build_tasks) {
						if (b.type->unit->builder_type == unit_types::starport) b.dead = true;
					}
					for (unit*u : my_completed_units_of_type[unit_types::starport]) {
						u->game_unit->cancelTrain();
					}
					break;
				}
				if (!my_completed_units_of_type[unit_types::wraith].empty()) break;
			}
			bool expand = false;
			if (!being_zergling_rushed && !my_units_of_type[unit_types::wraith].empty()) expand = true;
			execute_build(expand, build);

// 			if (combat::defence_choke.center != xy()) {
// 				if (wall_calculated < 2) {
// 					++wall_calculated;
// 
// 					wall = wall_in::wall_builder();
// 
// 					wall.spot = wall_in::find_wall_spot_from_to(unit_types::zealot, combat::my_closest_base, combat::defence_choke.outside, false);
// 					wall.spot.outside = combat::defence_choke.outside;
// 
// 					wall.against(wall_calculated == 1 ? unit_types::zergling : unit_types::zealot);
// 					wall.add_building(unit_types::supply_depot);
// 					wall.add_building(unit_types::barracks);
// 					has_wall = true;
// 					if (wall_calculated == 1) has_zergling_tight_wall = true;
// 					if (!wall.find()) {
// 						wall.add_building(unit_types::supply_depot);
// 						if (!wall.find()) {
// 							log("failed to find wall in :(\n");
// 							has_wall = false;
// 							if (wall_calculated == 1) has_zergling_tight_wall = false;
// 						}
// 					}
// 					if (has_wall) wall_calculated = 2;
// 				}
// 			}
// 			if (has_wall) wall.build();

			multitasking::sleep(15 * 5);
		}
		combat::build_bunker_count = 0;
		resource_gathering::max_gas = 0.0;

		//combat::no_aggressive_groups = false;

	}

	void render() {
		wall.render();
	}

};

