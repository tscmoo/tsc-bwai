
struct strat_tvp_opening {

	wall_in::wall_builder wall;

	void run() {

		combat::no_aggressive_groups = true;

		using namespace buildpred;

		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_1, 2000.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_1, 2000.0);

		get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, 1000.0);

		resource_gathering::max_gas = 100;
		bool wall_calculated = false;
		bool has_wall = false;
		while (true) {

			int my_vulture_count = my_units_of_type[unit_types::vulture].size();
			int enemy_zealot_count = 0;
			int enemy_dragoon_count = 0;
			int enemy_dt_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zealot) ++enemy_zealot_count;
				if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
				if (e->type == unit_types::dark_templar) ++enemy_dt_count;
			}
			
			if (enemy_dt_count) combat::aggressive_vultures = false;

			int my_siege_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();
			bool need_missile_turret = enemy_zealot_count + enemy_dragoon_count < 8 && my_siege_tank_count >= 1 && my_units_of_type[unit_types::missile_turret].empty();

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
				//if (scv_count >= 16 && count_units_plus_production(st, unit_types::factory) == 0) return depbuild(st, state(st), unit_types::factory);
				if (scv_count >= 16 && my_units_of_type[unit_types::factory].empty()) {
					return nodelay(st, unit_types::factory, [&](state&st) {
						return depbuild(st, state(st), unit_types::scv);
					});
				}
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
					int vulture_count = count_units_plus_production(st, unit_types::vulture);
					if (need_missile_turret && count_units_plus_production(st, unit_types::missile_turret) == 0) {
						return nodelay(st, unit_types::missile_turret, [&](state&st) {
							return nodelay(st, unit_types::siege_tank_tank_mode, [&](state&st) {
								return depbuild(st, state(st), unit_types::marine);
							});
						});
					}
					if (siege_tank_count) {
						if (vulture_count < enemy_dt_count / 2 || enemy_zealot_count > vulture_count || (siege_tank_count >= enemy_dragoon_count / 2 && vulture_count < 6)) {
							return depbuild(st, state(st), unit_types::vulture);
						}
					}
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
				get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
				if (players::my_player->has_upgrade(upgrade_types::spider_mines)) get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
			}
			bool expand = false;
			int siege_tank_count = my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size();
			int marine_count = my_completed_units_of_type[unit_types::marine].size();
			int vulture_count = my_completed_units_of_type[unit_types::vulture].size();
			auto my_st = get_my_current_state();
			bool has_bunker = !my_units_of_type[unit_types::bunker].empty();
			if (my_st.bases.size() > 1 && siege_tank_count >= 2 && (has_bunker || siege_tank_count >= 3) && players::my_player->has_upgrade(upgrade_types::siege_mode)) break;
			if (my_st.bases.size() > 1 && marine_count >= 1) combat::build_bunker_count = 1;
			if (my_st.bases.size() == 1 && current_used_total_supply >= 18) {
				if (siege_tank_count * 3 + vulture_count >= 9) {
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
				if (need_missile_turret) expand = false;
				if (!players::my_player->has_upgrade(upgrade_types::siege_mode)) expand = false;
			}
			if (!has_wall && !my_units_of_type[unit_types::factory].empty()) {
				combat::build_bunker_count = 1;
			}

			execute_build(expand, build);

// 			if (combat::defence_choke.center != xy() && !wall_calculated) {
// 				wall_calculated = true;
// 
// 				wall.spot = wall_in::find_wall_spot_from_to(unit_types::zealot, combat::my_closest_base, combat::defence_choke.outside, false);
// 				wall.spot.outside = combat::defence_choke.outside;
// 
// 				wall.against(unit_types::zealot);
// 				wall.add_building(unit_types::supply_depot);
// 				wall.add_building(unit_types::barracks);
// 				has_wall = true;
// 				if (!wall.find()) {
// 					wall.add_building(unit_types::supply_depot);
// 					if (!wall.find()) {
// 						log("failed to find wall in :(\n");
// 						has_wall = false;
// 					}
// 				}
// 			}
// 			if (has_wall) wall.build();

			multitasking::sleep(15 * 5);
		}
		combat::build_bunker_count = 0;
		resource_gathering::max_gas = 0.0;

		get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, 0.0);

		combat::aggressive_vultures = true;

	}

	void render() {
		wall.render();
	}

};

