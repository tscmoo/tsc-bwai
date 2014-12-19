
struct strat_tvp_opening {

	wall_in::wall_builder wall;

	void run() {

		combat::no_aggressive_groups = true;

		using namespace buildpred;

		resource_gathering::max_gas = 100;
		bool wall_calculated = false;
		bool has_wall = false;
		while (true) {

			int my_vulture_count = my_units_of_type[unit_types::vulture].size();
			int enemy_zealot_count = 0;
			int enemy_dragoon_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zealot) ++enemy_zealot_count;
				if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
			}

			auto build = [&](state&st) {
				if (enemy_dragoon_count == 0 && enemy_zealot_count > my_vulture_count) {
					return maxprod(st, unit_types::vulture, [&](state&st) {
						return depbuild(st, state(st), unit_types::marine);
					});
				}
				if (count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode) == 0) {
					if (!my_completed_units_of_type[unit_types::machine_shop].empty()) {
						return depbuild(st, state(st), unit_types::siege_tank_tank_mode);
					}
				}
				int scv_count = count_units_plus_production(st, unit_types::scv);
				if (scv_count >= 11 && count_units_plus_production(st, unit_types::barracks) == 0) return depbuild(st, state(st), unit_types::barracks);
				if (scv_count >= 12 && count_units_plus_production(st, unit_types::refinery) == 0) return depbuild(st, state(st), unit_types::refinery);
				if (scv_count >= 16 && count_units_plus_production(st, unit_types::factory) == 0) return depbuild(st, state(st), unit_types::factory);
				return nodelay(st, unit_types::scv, [&](state&st) {
					st.dont_build_refineries = true;
					if (count_units_plus_production(st, unit_types::refinery) == 0) {
						return depbuild(st, state(st), unit_types::refinery);
					}
					auto backbone = [&](state&st) {
						return maxprod(st, unit_types::siege_tank_tank_mode, [&](state&st) {
							return maxprod1(st, unit_types::vulture);
						});
					};
					int siege_tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
					int marine_count = count_units_plus_production(st, unit_types::marine);
					if (siege_tank_count < 2) {
						return nodelay(st, unit_types::siege_tank_tank_mode, [&](state&st) {
							return depbuild(st, state(st), unit_types::marine);
						});
					}
					if (!my_units_of_type[unit_types::factory].empty() && marine_count < 3) {
						return nodelay(st, unit_types::marine, backbone);
					}
					return backbone(st);
				});
			};

			if (!my_units_of_type[unit_types::factory].empty()) {
				resource_gathering::max_gas = 250;
			}
			if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty()) {
				resource_gathering::max_gas = 150;
				get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
				if (players::my_player->has_upgrade(upgrade_types::siege_mode)) get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
			}
			bool expand = false;
			int siege_tank_count = my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size();
			int marine_count = my_completed_units_of_type[unit_types::marine].size();
			auto my_st = get_my_current_state();
			bool has_bunker = !my_units_of_type[unit_types::bunker].empty();
			if (my_st.bases.size() > 1 && siege_tank_count >= 2 && (has_bunker || siege_tank_count >= 3)) break;
			if (my_st.bases.size() > 1 && marine_count >= 1) combat::build_bunker_count = 1;
			if (my_st.bases.size() == 1 && current_used_total_supply >= 18) {
				if (siege_tank_count >= 2) {
					expand = true;
				}
				for (unit*u : enemy_units) {
					if (!u->type->is_resource_depot) continue;
					bool is_expo = true;
					for (xy p : start_locations) {
						if (u->building->build_pos == p) {
							is_expo = false;
							break;
						}
					}
					if (is_expo) expand = true;
				}
			}
			if (!has_wall && !my_units_of_type[unit_types::factory].empty()) {
				combat::build_bunker_count = 1;
			}

			execute_build(expand, build);

			if (combat::defence_choke.center != xy() && !wall_calculated) {
				wall_calculated = true;

				wall.spot = wall_in::find_wall_spot_from_to(unit_types::zealot, combat::my_closest_base, combat::defence_choke.outside, false);
				wall.spot.outside = combat::defence_choke.outside;

				wall.against(unit_types::zealot);
				wall.add_building(unit_types::supply_depot);
				wall.add_building(unit_types::barracks);
				has_wall = true;
				if (!wall.find()) {
					wall.add_building(unit_types::supply_depot);
					if (!wall.find()) {
						log("failed to find wall in :(\n");
						has_wall = false;
					}
				}
			}
			if (has_wall) wall.build();

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

