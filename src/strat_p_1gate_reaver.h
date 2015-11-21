

struct strat_p_1gate_reaver : strat_p_base {


	virtual void init() override {

		sleep_time = 15;

	}

	bool defence_fight_ok = true;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::probe, 10);
				build::add_build_task(1.0, unit_types::gateway);
				build::add_build_task(2.0, unit_types::probe);
				build::add_build_task(3.0, unit_types::assimilator);
				build::add_build_total(4.0, unit_types::probe, 13);
				build::add_build_task(5.0, unit_types::cybernetics_core);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		attack_interval = 15 * 60 * 2;

		defence_fight_ok = eval_combat(true, 0);

		combat::no_aggressive_groups = army_supply < 10.0;

		if ((!being_rushed || defence_fight_ok) && army_supply >= 6.0) {
			if (my_units_of_type[unit_types::dragoon].size() >= 3 && defence_fight_ok) {
				get_upgrades::set_upgrade_value(upgrade_types::singularity_charge, -1.0);
			}

			if (my_units_of_type[unit_types::shuttle].size() >= 2) {
				get_upgrades::set_upgrade_value(upgrade_types::gravitic_drive, -1.0);
			}
		}

		if (!is_defending && enemy_attacking_army_supply < 4.0) {
			if (!my_completed_units_of_type[unit_types::reaver].empty()) {
				for (auto*a : combat::live_combat_units) {
					if (!a->u->type->is_flyer && !a->u->type->is_worker) a->use_for_drops_until = current_frame + 15 * 5;
				}
			}
		}

		return current_used_total_supply >= 160.0;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		army = [army](state&st) {
			return maxprod(st, unit_types::zealot, army);
		};

		army = [army](state&st) {
			return maxprod(st, unit_types::dragoon, army);
		};

		if (army_supply >= 32.0) {
			if (observer_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::observer, army);
				};
			}
		}

		if (shuttle_count < army_supply / 16.0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::shuttle, army);
			};
		}
		if (reaver_count == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::reaver, army);
			};
		}
		if (shuttle_count == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::shuttle, army);
			};
		} else {
			army = [army](state&st) {
				return nodelay(st, unit_types::dark_templar, army);
			};
			army = [army](state&st) {
				return nodelay(st, unit_types::reaver, army);
			};
		}
		if (dragoon_count < reaver_count / 4 || dragoon_count * 2 < enemy_air_army_supply) {
			army = [army](state&st) {
				return nodelay(st, unit_types::dragoon, army);
			};
		}

		//if (!defence_fight_ok || (!opponent_has_expanded && enemy_static_defence_count == 0) || enemy_attacking_army_supply || enemy_proxy_building_count) {
// 		if (!defence_fight_ok || enemy_attacking_army_supply || enemy_proxy_building_count) {
// 			if (count_units_plus_production(st, unit_types::gateway) < 4) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::gateway, army);
// 				};
// 			}
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::dragoon, army);
// 			};
// 			if (zealot_count < dragoon_count) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::zealot, army);
// 				};
// 			}
// 		}

		army = [army](state&st) {
			return nodelay(st, unit_types::probe, army);
		};

		if (opponent_has_expanded || enemy_static_defence_count || army_supply >= 24.0) {
			if (force_expand && count_production(st, unit_types::nexus) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::nexus, army);
				};
			}
		}

		return army(st);
	}

};

