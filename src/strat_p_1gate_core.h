

struct strat_p_1gate_core : strat_p_base {


	virtual void init() override {

		sleep_time = 15;

	}

	double last_attacked_army_supply = 0.0;
	bool last_attacking = false;
	bool defence_fight_ok = true;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_task(0.0, unit_types::probe);
				build::add_build_task(0.0, unit_types::probe);
				build::add_build_task(0.0, unit_types::probe);
				build::add_build_task(0.0, unit_types::probe);
				build::add_build_task(1.0, unit_types::pylon);
				build::add_build_task(2.0, unit_types::probe);
				build::add_build_task(2.0, unit_types::probe);
				build::add_build_task(3.0, unit_types::gateway);
				build::add_build_task(4.0, unit_types::probe);
				build::add_build_task(4.0, unit_types::probe);
				build::add_build_task(5.0, unit_types::assimilator);
				build::add_build_task(6.0, unit_types::probe);
				build::add_build_task(7.0, unit_types::zealot);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		attack_interval = 15 * 60 * 2;

		defence_fight_ok = eval_combat(true, 0);

		if (!defence_fight_ok) attack_interval = 0;
		if (army_supply < std::min(last_attacked_army_supply * 2, 100.0)) attack_interval = 0;
		if (last_attacking && combat::no_aggressive_groups) {
			last_attacked_army_supply = army_supply;
		}
		if (!combat::no_aggressive_groups != last_attacking) last_attacking = !combat::no_aggressive_groups;

		if ((!being_rushed || defence_fight_ok) && army_supply >= 6.0) {
			if (my_units_of_type[unit_types::dragoon].size() >= 3 && defence_fight_ok) {
				get_upgrades::set_upgrade_value(upgrade_types::singularity_charge, -1.0);
			}
			if (!my_units_of_type[unit_types::citadel_of_adun].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::leg_enhancements, -1.0);
			}

			if (my_units_of_type[unit_types::shuttle].size() >= 2) {
				get_upgrades::set_upgrade_value(upgrade_types::gravitic_drive, -1.0);
			}
			if (my_units_of_type[unit_types::reaver].size() >= 2) {
				get_upgrades::set_upgrade_value(upgrade_types::reaver_capacity, -1.0);
			}

			if (!my_units_of_type[unit_types::high_templar].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::psionic_storm, -1.0);
				get_upgrades::set_upgrade_order(upgrade_types::psionic_storm, -10.0);
			}
			if (!my_units_of_type[unit_types::arbiter].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::stasis_field, -1.0);
			}
			if (!my_units_of_type[unit_types::dark_archon].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::mind_control, -1.0);
				if (enemy_biological_army_supply >= 16.0) {
					get_upgrades::set_upgrade_value(upgrade_types::maelstrom, -1.0);
					get_upgrades::set_upgrade_order(upgrade_types::maelstrom, -9.0);
				}
			}
			if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
			}
		}

		return false;
		//return current_used_total_supply >= 22.0 || my_units_of_type[unit_types::dragoon].size() >= 2;
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

		if (zealot_count / 2 < enemy_tank_count && dragoon_count > zealot_count) {
			army = [army](state&st) {
				return maxprod(st, unit_types::zealot, army);
			};
		}

		double core_army_supply = dragoon_count * 2.0 + zealot_count * 2.0;

		if (st.bases.size() >= 2) {
			if (count_units_plus_production(st, unit_types::robotics_facility) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::robotics_facility, army);
				};
			}
			if (enemy_ground_army_supply >= 16.0) {
				if (reaver_count < core_army_supply / 16) {
					army = [army](state&st) {
						return nodelay(st, unit_types::reaver, army);
					};
				}
			}
		}

		if (army_supply >= 60.0 && high_templar_count < core_army_supply / 16) {
			army = [army](state&st) {
				return nodelay(st, unit_types::high_templar, army);
			};
		}

		if (shuttle_count < reaver_count + high_templar_count / 2) {
			army = [army](state&st) {
				return nodelay(st, unit_types::shuttle, army);
			};
		}

		if (arbiter_count < enemy_tank_count / 6 || (enemy_tank_count && army_supply >= 40.0)) {
			army = [army](state&st) {
				return maxprod(st, unit_types::arbiter, army);
			};
		}

// 		if (army_supply >= 20.0 && count_units_plus_production(st, unit_types::stargate) == 0) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::stargate, army);
// 			};
// 		}

		if (count_units_plus_production(st, unit_types::scv)) {
			army = [army](state&st) {
				return nodelay(st, unit_types::siege_tank_tank_mode, army);
			};
		}

		// 		if (!is_defending || defence_fight_ok) {
		// 			army = [army](state&st) {
		// 				return nodelay(st, unit_types::probe, army);
		// 			};
		// 		}

		// 		double desired_army_supply = probe_count*probe_count * 0.015 + probe_count * 1.0;
		// 
		// 		if (probe_count >= 16 && ((is_defending && !defence_fight_ok) || (probe_count >= 24 && army_supply < desired_army_supply))) {
		// 			army = [army](state&st) {
		// 				return maxprod(st, unit_types::dragoon, army);
		// 			};
		// 
		// 			if (zealot_count / 2 < enemy_tank_count && dragoon_count > zealot_count) {
		// 				army = [army](state&st) {
		// 					return maxprod(st, unit_types::zealot, army);
		// 				};
		// 			}
		// 
		// 			if (enemy_tank_count >= 10 && count_units_plus_production(st, unit_types::arbiter) < enemy_tank_count / 8) {
		// 				army = [army](state&st) {
		// 					return nodelay(st, unit_types::arbiter, army);
		// 				};
		// 			}
		// 		}

		if (probe_count >= 14 && count_units_plus_production(st, unit_types::cybernetics_core) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::cybernetics_core, army);
			};
		}

		if (army_supply >= 24.0) {
			if (count_units_plus_production(st, unit_types::citadel_of_adun) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::citadel_of_adun, army);
				};
			}
		}

		if (players::opponent_player->race == race_terran) {
			if (!being_rushed && probe_count >= 16 && count_units_plus_production(st, unit_types::nexus) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::nexus, army);
				};
			}
		}

		if (defence_fight_ok && (army_supply >= enemy_army_supply + 24.0 || army_supply >= 60.0 || enemy_static_defence_count >= 5)) {
			if (dark_archon_count < core_army_supply / 8) {
				army = [army](state&st) {
					return nodelay(st, unit_types::dark_archon, army);
				};
			}
		}

		if (being_rushed && !defence_fight_ok) {
			army = [army](state&st) {
				return maxprod(st, unit_types::zealot, army);
			};
		}

		if (enemy_dt_count || (army_supply >= 6.0 && defence_fight_ok) || army_supply >= 14.0) {
			if (observer_count < (int)(army_supply / 20.0) + 1) {
				army = [army](state&st) {
					return nodelay(st, unit_types::observer, army);
				};
			}
		}

		army = [army](state&st) {
			return nodelay(st, unit_types::probe, army);
		};
		if (count_units_plus_production(st, unit_types::cc) && count_units_plus_production(st, unit_types::scv) < 10) {
			army = [army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		if (count_units_plus_production(st, unit_types::scv) && count_units_plus_production(st, unit_types::cc) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::cc, army);
			};
		}

		if (force_expand && count_production(st, unit_types::nexus) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::nexus, army);
			};
		}

		if (probe_count >= 16 && army_supply < 12.0 && !opponent_has_expanded && army_supply < probe_count) {
			if (count_units_plus_production(st, unit_types::gateway) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::gateway, army);
				};
			}
			army = [army](state&st) {
				return nodelay(st, unit_types::zealot, army);
			};
			army = [army](state&st) {
				return nodelay(st, unit_types::dragoon, army);
			};
		}

		return army(st);
	}

};

