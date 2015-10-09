
struct strat_fd_push {

	void run() {

		combat::no_aggressive_groups = true;
		get_upgrades::set_no_auto_upgrades(true);
		combat::no_aggressive_groups = true;
		combat::aggressive_vultures = false;

		using namespace buildpred;

		resource_gathering::max_gas = 72;
		while (my_units_of_type[unit_types::refinery].empty()) {
// 			for (auto&v : scouting::all_scouts) {
// 				if (v.type == scouting::scout::type_default) scouting::rm_scout(v.scout_unit);
// 			}

			auto build = [&](state&st) {
				int worker_count = my_workers.size();
				if (worker_count >= 9 && count_units_plus_production(st, unit_types::barracks) == 0) return depbuild(st, state(st), unit_types::barracks);
				if (worker_count >= 9 && count_units_plus_production(st, unit_types::refinery) == 0) return depbuild(st, state(st), unit_types::refinery);
				if (worker_count >= 15) return false;
				return depbuild(st, state(st), unit_types::scv);
			};

			execute_build(false, build);

			multitasking::sleep(15 * 5);
		}
		resource_gathering::max_gas = 100;

		bool scout_sent = false;
		while (my_units_of_type[unit_types::factory].empty()) {
// 			for (auto&v : scouting::all_scouts) {
// 				if (v.type == scouting::scout::type_default) scouting::rm_scout(v.scout_unit);
// 			}

			auto build = [&](state&st) {
				st.dont_build_refineries = true;
				if (current_used_total_supply >= 13 && count_units_plus_production(st, unit_types::factory) == 0)  return depbuild(st, state(st), unit_types::factory);
				if (current_used_total_supply >= 17) return false;

				return nodelay(st, unit_types::scv, [&](state&st) {
					return depbuild(st, state(st), unit_types::marine);
				});
			};

			execute_build(false, build);

			multitasking::sleep(15 * 5);
		}
// 		if (true) {
// 			unit*scout_unit = get_best_score(my_workers, [&](unit*u) {
// 				if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
// 				return 0.0;
// 			}, std::numeric_limits<double>::infinity());
// 			if (scout_unit) scouting::add_scout(scout_unit);
// 		}
		resource_gathering::max_gas = 0;
		while (true) {
			if (my_units_of_type[unit_types::factory].empty()) break;
			if (my_units_of_type[unit_types::factory].front()->remaining_whatever_time <= 15 * 30) break;

			auto build = [&](state&st) {
				return nodelay(st, unit_types::scv, [&](state&st) {
					return depbuild(st, state(st), unit_types::marine);
				});
			};
			execute_build(false, build);

			multitasking::sleep(15);
		}
		resource_gathering::max_gas = 250;
		while (get_my_current_state().bases.size() == 1) {

			int enemy_zealot_count = 0;
			int enemy_dragoon_count = 0;
			int enemy_dt_count = 0;
			int enemy_forge_count = 0;
			int enemy_cannon_count = 0;
			int enemy_citadel_of_adun_count = 0;
			int enemy_templar_archives_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zealot) ++enemy_zealot_count;
				if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
				if (e->type == unit_types::dark_templar) ++enemy_dt_count;
				if (e->type == unit_types::forge) ++enemy_forge_count;
				if (e->type == unit_types::photon_cannon) ++enemy_cannon_count;
				if (e->type == unit_types::citadel_of_adun) ++enemy_citadel_of_adun_count;
				if (e->type == unit_types::templar_archives) ++enemy_templar_archives_count;
			}

			bool expand = false;
			if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1);
				if (players::my_player->has_upgrade(upgrade_types::siege_mode)) get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1);
			}
			int my_tank_count = my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size();
			int my_vulture_count = my_completed_units_of_type[unit_types::vulture].size();
			double my_lost = players::my_player->minerals_lost + players::my_player->gas_lost;
			double op_lost = players::opponent_player->minerals_lost + players::opponent_player->gas_lost;
			if (my_tank_count >= 3 && my_vulture_count >= 2) {
				if ((my_lost * 2 <= op_lost || my_lost < 400) && enemy_dragoon_count - 2 < my_tank_count && enemy_zealot_count - 2 < my_vulture_count) {
					combat::no_aggressive_groups = false;
				} else {
					combat::no_aggressive_groups = true;
					if (get_my_current_state().bases.size() == 1) expand = true;
				}
			} else {
				combat::no_aggressive_groups = true;
			}
			if (my_lost >= 1000) {
				if (my_lost >= op_lost*0.75) combat::no_aggressive_groups = true;
				if (my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() >= 15) combat::no_aggressive_groups = false;
			}

			auto build = [&](state&st) {
				return nodelay(st, unit_types::scv, [&](state&st) {
					std::function<bool(state&)> army = [&](state&st) {
						if (count_units_plus_production(st, unit_types::factory) >= 2) return depbuild(st, state(st), unit_types::siege_tank_tank_mode);
						return depbuild(st, state(st), unit_types::marine);
					};
					army = [army, my_tank_count](state&st) {
						if (st.minerals >= 150 && (st.gas >= 50 || my_tank_count >= 2)) return maxprod(st, unit_types::vulture, army);
						return nodelay(st, unit_types::vulture, army);
					};
					int vulture_count = count_units_plus_production(st, unit_types::vulture);
					int tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
					if (tank_count - 1 <= vulture_count / 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::siege_tank_tank_mode, army);
						};
					}
					return army(st);
				});
			};
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
			execute_build(expand, build);

			multitasking::sleep(15);
		}

		resource_gathering::max_gas = 0.0;

		//combat::no_aggressive_groups = false;

	}

	void render() {
		
	}

};

