
struct strat_tvt {


	void run() {

// 		unit*dropship_scout = nullptr;
// 		int last_dropship_scout = 0;

		get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::personal_cloaking, 200.0);
		get_upgrades::set_upgrade_value(upgrade_types::ocular_implants, 300.0);

		combat::defensive_spider_mine_count = 8;
		//combat::no_aggressive_groups = false;
		combat::no_aggressive_groups = true;
		bool maxed_out_aggression = false;

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
			int my_goliath_count = my_units_of_type[unit_types::goliath].size();
			int my_vulture_count = my_units_of_type[unit_types::vulture].size();
			int desired_bc_count = enemy_tank_count / 4 - enemy_goliath_count - enemy_wraith_count;
			if (enemy_tank_count / 2 > my_tank_count) desired_bc_count = 0;
			if (current_used_total_supply >= 180.0 && desired_bc_count < 4) desired_bc_count = 4;
			int desired_goliath_count = 1 + enemy_wraith_count + enemy_bc_count * 3;
			int desired_wraith_count = 4 + enemy_wraith_count + enemy_bc_count * 3 - enemy_goliath_count;
			int desired_tank_count = 2 + enemy_tank_count;
			int desired_valkyrie_count = 0;
			if (enemy_wraith_count >= 8) desired_valkyrie_count = enemy_wraith_count / 3;
			int desired_science_vessel_count = my_tank_count >= 8 ? my_tank_count / 4 : 0;
			if (enemy_wraith_count >= 6 && desired_science_vessel_count < 1) desired_science_vessel_count = 1;

			//if (my_tank_count >= 12 || (my_tank_count > enemy_tank_count + 4 && my_tank_count >= 8) || my_goliath_count >= 8) combat::no_aggressive_groups = false;
			//if (my_tank_count > enemy_tank_count + 4 && (my_tank_count >= 8 || my_goliath_count >= 8)) combat::no_aggressive_groups = false;
			if (my_tank_count >= enemy_tank_count && (my_tank_count >= 8 || my_goliath_count >= 8)) combat::no_aggressive_groups = false;
			if (my_tank_count + my_goliath_count < 6 && my_tank_count <= enemy_tank_count / 2) combat::no_aggressive_groups = true;
			if (my_vulture_count >= 50 || current_minerals >= 8000) combat::no_aggressive_groups = false;

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				if (current_used_total_supply < 150.0) maxed_out_aggression = false;
			}


			auto build_vulture = [&](state&st) {
				return nodelay(st, unit_types::scv, [&](state&st) {
					std::function<bool(state&)> army = [&](state&st) {
						if (enemy_tank_count >= 8) {
							return maxprod(st, unit_types::siege_tank_tank_mode, [&](state&st) {
								return maxprod1(st, unit_types::vulture);
							});
						}
						return maxprod1(st, unit_types::vulture);
					};
					int wraith_count = count_units_plus_production(st, unit_types::wraith);
					if (wraith_count < desired_wraith_count) {
						if (desired_wraith_count - wraith_count >= 4 && count_units_plus_production(st, unit_types::starport) < 2) {
							army = [army](state&st) {
								return nodelay(st, unit_types::starport, army);
							};
						}
						army = [army](state&st) {
							return nodelay(st, unit_types::wraith, army);
						};
					}
					int bc_count = count_units_plus_production(st, unit_types::battlecruiser);
					if (bc_count < desired_bc_count) {
						army = [army](state&st) {
							return maxprod(st, unit_types::battlecruiser, army);
						};
					}
					int valkyrie_count = count_units_plus_production(st, unit_types::valkyrie);
					if (valkyrie_count < desired_valkyrie_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::valkyrie, army);
						};
					}
					int science_vessel_count = count_units_plus_production(st, unit_types::science_vessel);
					if (science_vessel_count < desired_science_vessel_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::science_vessel, army);
						};
					}
// 					int droppable_units = count_units_plus_production(st, unit_types::goliath) + count_units_plus_production(st, unit_types::vulture);
// 					if (count_units_plus_production(st, unit_types::dropship) < droppable_units / 12) {
// 						return maxprod(st, unit_types::dropship, backbone);
// 					}
					int vulture_count = count_units_plus_production(st, unit_types::vulture);
					int tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode);
					tank_count += count_units_plus_production(st, unit_types::siege_tank_siege_mode);
					if (tank_count < desired_tank_count && vulture_count >= tank_count * 2) {
						army = [army](state&st) {
							return maxprod(st, unit_types::siege_tank_tank_mode, army);
						};
					}

					int goliath_count = count_units_plus_production(st, unit_types::goliath);
					if (goliath_count < desired_goliath_count) {
						army = [army](state&st) {
							return maxprod(st, unit_types::goliath, army);
						};
					}

// 					if (vulture_count < 4) return maxprod1(st, unit_types::vulture);
// 					if (vulture_count >= 15) {
// 						int ghost_count = count_units_plus_production(st, unit_types::ghost);
// 						int nuclear_missile_count = count_units_plus_production(st, unit_types::nuclear_missile);
// 						if (ghost_count >= 2) {
// 							int nuclear_silo_count = count_units_plus_production(st, unit_types::nuclear_silo);
// 							if (nuclear_silo_count == nuclear_missile_count) return nodelay(st, unit_types::nuclear_silo, army);
// 							if (nuclear_missile_count < 2) return nodelay(st, unit_types::nuclear_missile, army);
// 						}
// 						if (ghost_count < vulture_count / 7) {
// 							return nodelay(st, unit_types::ghost, army);
// 						}
// 						if (wraith_count < vulture_count / 6) {
// 							return nodelay(st, unit_types::wraith, army);
// 						}
// 						if (goliath_count < vulture_count / 9) {
// 							return nodelay(st, unit_types::goliath, army);
// 						}
// 						if (science_vessel_count < vulture_count / 6) {
// 							return nodelay(st, unit_types::science_vessel, army);
// 						}
// 					}
					return army(st);
				});
			};

			auto long_distance_miners = [&]() {
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
				return count;
			};
			auto can_expand = [&]() {
				auto my_st = get_my_current_state();
				if (my_st.bases.size() == 1) {
					for (unit*u : enemy_units) {
						if (!u->type->is_resource_depot) continue;
						bool is_expo = true;
						for (xy p : start_locations) {
							if (u->building->build_pos == p) {
								is_expo = false;
								break;
							}
						}
						if (is_expo) return true;
					}
				}
				if (current_used_total_supply >= 150) {
					if (long_distance_miners()) return true;
				}
				return long_distance_miners() >= 4;
			};

			execute_build(can_expand(), build_vulture);

			multitasking::sleep(15 * 10);
		}

	}

	void render() {

	}

};

