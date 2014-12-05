
struct strat_vulture {


	void run() {

// 		unit*dropship_scout = nullptr;
// 		int last_dropship_scout = 0;

		get_upgrades::set_upgrade_value(upgrade_types::spider_mines, 1.0);
		get_upgrades::set_upgrade_value(upgrade_types::personal_cloaking, 200.0);
		get_upgrades::set_upgrade_value(upgrade_types::ocular_implants, 300.0);


		while (true) {

// 			if (dropship_scout && dropship_scout->dead) dropship_scout = nullptr;
// 			if (!dropship_scout && my_completed_units_of_type[unit_types::dropship].size() >= 2 && current_frame - last_dropship_scout >= 15 * 60 * 5) {
// 				dropship_scout = get_best_score(my_completed_units_of_type[unit_types::dropship], [&](unit*u) {
// 					if (!u->loaded_units.empty()) return std::numeric_limits<double>::infinity();
// 					return u->hp;
// 				}, std::numeric_limits<double>::infinity());
// 				if (dropship_scout) {
// 					scouting::add_scout(dropship_scout);
// 					last_dropship_scout = current_frame;
// 				}
// 			}
// 			if (dropship_scout && current_frame - last_dropship_scout >= 15 * 60 * 2) {
// 				scouting::rm_scout(dropship_scout);
// 				dropship_scout = nullptr;
// 				last_dropship_scout = current_frame;
// 			}

			using namespace buildpred;

			int enemy_tank_count = 0;
			int enemy_goliath_count = 0;
			int enemy_wraith_count = 0;
			int enemy_bc_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::siege_tank_tank_mode) ++enemy_tank_count;
				if (e->type == unit_types::siege_tank_siege_mode) ++enemy_tank_count;
				if (e->type == unit_types::goliath) ++enemy_goliath_count;
				if (e->type == unit_types::wraith) ++enemy_wraith_count;
				if (e->type == unit_types::battlecruiser) ++enemy_bc_count;
			}
			int my_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();
			int desired_bc_count = enemy_tank_count / 4 - enemy_goliath_count - enemy_wraith_count;
			if (enemy_tank_count / 2 > my_tank_count) desired_bc_count = 0;
			if (current_used_total_supply >= 180.0 && desired_bc_count < 4) desired_bc_count = 4;
			int desired_goliath_count = 1 + enemy_wraith_count + enemy_bc_count * 3;
			int desired_wraith_count = 1 + enemy_wraith_count + enemy_bc_count * 3 - enemy_goliath_count;
			int desired_tank_count = 2 + enemy_tank_count + enemy_bc_count * 3;
			int desired_valkyrie_count = 0;
			if (enemy_wraith_count >= 8) desired_valkyrie_count = enemy_wraith_count / 3;
			int desired_science_vessel_count = my_tank_count / 4;
			if (enemy_wraith_count >= 6 && desired_science_vessel_count<1) desired_science_vessel_count = 1;

			auto build_vulture = [&](state&st) {
				return nodelay(st, unit_types::scv, [&](state&st) {
					auto backbone = [&](state&st) {
						if (enemy_tank_count >= 8) {
							return maxprod(st, unit_types::siege_tank_tank_mode, [&](state&st) {
								return maxprod1(st, unit_types::vulture);
							});
						}
						return maxprod1(st, unit_types::vulture);
					};
					int bc_count = count_units_plus_production(st, unit_types::battlecruiser);
					if (bc_count < desired_bc_count) {
						return maxprod(st, unit_types::battlecruiser, backbone);
					}
					int valkyrie_count = count_units_plus_production(st, unit_types::valkyrie);
					if (valkyrie_count < desired_valkyrie_count) {
						return maxprod(st, unit_types::valkyrie, backbone);
					}
					int science_vessel_count = count_units_plus_production(st, unit_types::science_vessel);
					if (science_vessel_count < desired_science_vessel_count) {
						return maxprod(st, unit_types::science_vessel, backbone);
					}
					int goliath_count = count_units_plus_production(st, unit_types::goliath);
					if (goliath_count < desired_goliath_count) {
						return maxprod(st, unit_types::goliath, backbone);
					}
					int wraith_count = count_units_plus_production(st, unit_types::wraith);
					if (wraith_count < desired_wraith_count) {
						return maxprod(st, unit_types::wraith, backbone);
					}
					int droppable_units = count_units_plus_production(st, unit_types::goliath) + count_units_plus_production(st, unit_types::vulture);
					if (count_units_plus_production(st, unit_types::dropship) < droppable_units / 12) {
						return maxprod(st, unit_types::dropship, backbone);
					}
					int tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode);
					tank_count += count_units_plus_production(st, unit_types::siege_tank_siege_mode);
					if (tank_count < desired_tank_count) {
						return maxprod(st, unit_types::siege_tank_tank_mode, backbone);
					}
					int vulture_count = count_units_plus_production(st, unit_types::vulture);
					if (vulture_count >= 15) {
						int ghost_count = count_units_plus_production(st, unit_types::ghost);
						int nuclear_missile_count = count_units_plus_production(st, unit_types::nuclear_missile);
						if (ghost_count >= 2) {
							int nuclear_silo_count = count_units_plus_production(st, unit_types::nuclear_silo);
							if (nuclear_silo_count == nuclear_missile_count) return nodelay(st, unit_types::nuclear_silo, backbone);
							if (nuclear_missile_count < 2) return nodelay(st, unit_types::nuclear_missile, backbone);
						}
						if (ghost_count < vulture_count / 7) {
							return maxprod(st, unit_types::ghost, backbone);
						}
						if (wraith_count < vulture_count / 6) {
							return maxprod(st, unit_types::wraith, backbone);
						}
						if (goliath_count < vulture_count / 9) {
							return maxprod(st, unit_types::goliath, backbone);
						}
						if (science_vessel_count < vulture_count / 6) {
							return maxprod(st, unit_types::science_vessel, backbone);
						}
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
				return is_long_distance_mining();
			};

			execute_build(can_expand(), build_vulture);

			multitasking::sleep(15 * 10);
		}

	}

	void render() {

	}

};

