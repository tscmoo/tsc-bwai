
struct strat_tvp {


	void run() {

		get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
		
		get_upgrades::set_no_auto_upgrades(true);

		get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, 375.0);

		combat::no_aggressive_groups = true;
		combat::aggressive_wraiths = true;
		bool maxed_out_aggression = false;

		int pull_back_vultures_frame = 0;

		while (true) {

			using namespace buildpred;

			int my_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();
			int my_goliath_count = my_units_of_type[unit_types::goliath].size();
			int my_vulture_count = my_units_of_type[unit_types::vulture].size();
			int my_wraith_count = my_units_of_type[unit_types::wraith].size();

			int enemy_zealot_count = 0;
			int enemy_dragoon_count = 0;
			int enemy_observer_count = 0;
			int enemy_shuttle_count = 0;
			int enemy_scout_count = 0;
			int enemy_carrier_count = 0;
			int enemy_arbiter_count = 0;
			int enemy_corsair_count = 0;
			int enemy_cannon_count = 0;
			int enemy_dt_count = 0;
			int enemy_stargate_count = 0;
			int enemy_fleet_beacon_count = 0;
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
				if (e->type == unit_types::dark_templar) ++enemy_dt_count;
				if (e->type == unit_types::stargate) ++enemy_stargate_count;
				if (e->type == unit_types::fleet_beacon) ++enemy_fleet_beacon_count;
			}

			int attacking_zealot_count = 0;
			int attacking_dragoon_count = 0;
			update_possible_start_locations();
			for (unit*e : enemy_units) {
				if (!e->visible) continue;
				if (e->type == unit_types::zealot || e->type == unit_types::dragoon) {
					double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
						return unit_pathing_distance(unit_types::scv, e->pos, pos);
					});
					if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base)*0.33 < e_d) {
						if (e->type == unit_types::zealot) ++attacking_zealot_count;
						if (e->type == unit_types::dragoon) ++attacking_dragoon_count;
					}
				}
			}

			if (!combat::aggressive_vultures && current_frame - pull_back_vultures_frame >= 15 * 2) {
				combat::aggressive_vultures = true;
			}

			if (attacking_zealot_count + attacking_dragoon_count * 2 >= 15 && my_tank_count < enemy_dragoon_count && my_vulture_count < enemy_zealot_count) {
				pull_back_vultures_frame = current_frame;
				combat::aggressive_vultures = false;
				//combat::build_bunker_count = 3;
			}// else combat::build_bunker_count = 1;
			
			//if (my_tank_count >= 12 || my_tank_count + my_goliath_count >= 20) combat::no_aggressive_groups = false;
			if (my_tank_count + my_goliath_count + my_vulture_count / 2 >= 60) combat::no_aggressive_groups = false;
			//if (enemy_cannon_count >= 3 && enemy_cannon_count >= my_tank_count) combat::no_aggressive_groups = false;
			if (my_tank_count < 6 && my_tank_count <= (enemy_dragoon_count + enemy_zealot_count) / 2) combat::no_aggressive_groups = true;
			if ((enemy_carrier_count >= 2 || enemy_scout_count >= 4) && my_goliath_count < 6) combat::no_aggressive_groups = true;

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				if (current_used_total_supply < 100.0) maxed_out_aggression = false;
			}

			if (my_tank_count >= 5) {
				if (get_upgrades::no_auto_upgrades) {
					get_upgrades::set_no_auto_upgrades(false);
				}
			}

			int desired_science_vessel_count = enemy_arbiter_count + (enemy_dt_count + 2) / 3;
			if (current_used_total_supply >= 140 && desired_science_vessel_count == 0 && (enemy_stargate_count + enemy_carrier_count == 0 || my_goliath_count >= 10)) desired_science_vessel_count = 1;
			int desired_goliath_count = enemy_carrier_count * 4 + enemy_stargate_count * 2;
			if (enemy_fleet_beacon_count) desired_goliath_count += 4;
			desired_goliath_count += (enemy_shuttle_count + enemy_observer_count + 1) / 2;
			desired_goliath_count += enemy_scout_count + enemy_arbiter_count + enemy_corsair_count;
			if (my_tank_count >= 5) desired_goliath_count += 2;
			if (enemy_cannon_count >= 3 && desired_goliath_count < my_tank_count) desired_goliath_count = my_tank_count;
			if (enemy_zealot_count + enemy_dragoon_count < 8 && my_tank_count + my_vulture_count / 3 >= 2 && desired_goliath_count < 8) desired_goliath_count = 8;
			int desired_vulture_count = enemy_zealot_count / 2;
			int desired_wraith_count = my_goliath_count / 6;
			if (my_goliath_count < 8) desired_wraith_count = 0;
			//if (enemy_cannon_count >= 6 && enemy_carrier_count == 0 && enemy_dragoon_count < 6) desired_wraith_count = 4;
			//if (my_wraith_count >= enemy_carrier_count) desired_wraith_count = std::max(desired_wraith_count, enemy_carrier_count + 2);

			auto build = [&](state&st) {
				return nodelay(st, unit_types::scv, [&](state&st) {
					std::function<bool(state&)> army = [&](state&st) {
						if (my_vulture_count > enemy_zealot_count && my_vulture_count >= 10 && st.minerals < 1000) return maxprod1(st, unit_types::goliath);
						if (st.gas < 50 || count_units_plus_production(st, unit_types::vulture) < desired_vulture_count) {
							return maxprod(st, unit_types::vulture, [&](state&st) {
								return maxprod(st, unit_types::siege_tank_tank_mode, [&](state&st) {
									return nodelay(st, unit_types::firebat, [&](state&st) {
										return depbuild(st, state(st), unit_types::marine);
									});
								});
							});
						}
						if (my_tank_count >= 3 && st.gas >= 200) {
							// This is temporary until I fix addon production
							int machine_shops = 0;
							for (auto&v : st.units[unit_types::factory]) {
								if (v.has_addon) ++machine_shops;
							}
							int desired_machine_shops = 4;
							if (desired_machine_shops > (int)st.units[unit_types::factory].size() / 2 + 1) desired_machine_shops = st.units[unit_types::factory].size() / 2 + 1;
							if (machine_shops < desired_machine_shops) return depbuild(st, state(st), unit_types::machine_shop);
						}
						return maxprod(st, unit_types::siege_tank_tank_mode, [&](state&st) {
							return maxprod(st, unit_types::vulture, [&](state&st) {
								return nodelay(st, unit_types::firebat, [&](state&st) {
									return depbuild(st, state(st), unit_types::marine);
								});
							});
						});
					};
					int science_vessel_count = count_units_plus_production(st, unit_types::science_vessel);
					if (science_vessel_count < desired_science_vessel_count) {
						army = [army](state&st) {
							return maxprod(st, unit_types::science_vessel, army);
						};
					}
					int goliath_count = count_units_plus_production(st, unit_types::goliath);
					if (goliath_count < desired_goliath_count) {
						army = [army](state&st) {
							return maxprod(st, unit_types::goliath, army);
						};
					}
					int wraith_count = count_units_plus_production(st, unit_types::wraith);
					if (wraith_count < desired_wraith_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::wraith, army);
						};
					}
					int siege_tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
					if (current_used_total_supply >= 150 && siege_tank_count < 4) {
						army = [army](state&st) {
							return nodelay(st, unit_types::siege_tank_tank_mode, army);
						};
					}
					int vulture_count = count_units_plus_production(st, unit_types::vulture);
					if (current_used_total_supply >= 120 && vulture_count < 8) {
						army = [army](state&st) {
							return nodelay(st, unit_types::vulture, army);
						};
					}
// 					if (attacking_zealot_count + attacking_dragoon_count * 2 >= 10 && count_units_plus_production(st, unit_types::marine) < 8 && my_tank_count > 4) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::marine, army);
// 						};
// 					}

					if (my_tank_count >= 2 && count_units_plus_production(st, unit_types::engineering_bay) == 0) {
						return depbuild(st, state(st), unit_types::engineering_bay);
					}
					
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
				if (enemy_cannon_count >= 7 && buildpred::get_my_current_state().bases.size() < 3 && long_distance_miners()) return true;
				if (!players::my_player->has_upgrade(upgrade_types::siege_mode)) return false;
				if (long_distance_miners() >= 20) return true;
				if (buildpred::get_my_current_state().bases.size() == 2 && (my_tank_count < 12 && my_goliath_count < 8 && my_vulture_count < 30)) return false;
				return long_distance_miners() >= 8;
			};

			execute_build(can_expand(), build);

			multitasking::sleep(15 * 10);
		}

	}

};

