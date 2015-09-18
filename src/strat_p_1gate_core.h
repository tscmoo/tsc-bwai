

struct strat_p_1gate_core : strat_p_base {


	virtual void init() override {

		sleep_time = 15;

	}

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

		if (!my_units_of_type[unit_types::dragoon].empty()) {
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

		return false;
		//return current_used_total_supply >= 22.0 || my_units_of_type[unit_types::dragoon].size() >= 2;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		army = [army](state&st) {
			return maxprod(st, unit_types::dragoon, army);
		};

		if (defence_fight_ok) {
			if (observer_count < (int)(army_supply / 40.0) + 1) {
				army = [army](state&st) {
					return nodelay(st, unit_types::observer, army);
				};
			}
		}

		if (probe_count < 16 || defence_fight_ok) {
			army = [army](state&st) {
				return nodelay(st, unit_types::probe, army);
			};
		}

		if (force_expand && count_production(st, unit_types::nexus) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::nexus, army);
			};
		}

// 		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);
// 
// 		if (!defence_fight_ok || probe_count >= 25) {
// 			army = [army](state&st) {
// 				return maxprod(st, unit_types::zealot, army);
// 			};
// 		}
// 
// 		army = [army](state&st) {
// 			return nodelay(st, unit_types::dragoon, army);
// 		};
// 
// 		if (true) {
// 			army = [army](state&st) {
// 				return maxprod(st, unit_types::dragoon, army);
// 			};
// 
// 			//if (!defence_fight_ok || count_production(st, unit_types::probe) || probe_count >= 45) {
// 			if (st.bases.size() >= 2) {
// 				if (count_units_plus_production(st, unit_types::robotics_facility) < 2) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::robotics_facility, army);
// 					};
// 				}
// 				if (army_supply >= 16.0 || reaver_count) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::reaver, army);
// 					};
// 				}
// 				if (shuttle_count < reaver_count) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::shuttle, army);
// 					};
// 				}
// 			}
// 
// 			if ((defence_fight_ok && st.used_supply[race_protoss] >= 26 && dragoon_count >= 2) || enemy_dt_count) {
// // 				if (count_units_plus_production(st, unit_types::robotics_facility) == 0) {
// // 					army = [army](state&st) {
// // 						return maxprod(st, unit_types::robotics_facility, army);
// // 					};
// // 				} else if (count_units_plus_production(st, unit_types::observatory) == 0) {
// // 
// // 				}
// 				if (observer_count < (int)(army_supply / 40.0) + 1) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::observer, army);
// 					};
// 				}
// 			}
// 			if (dragoon_count >= 16) {
// 				if (dragoon_count * 2 >= enemy_air_army_supply + enemy_ground_large_army_supply) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::zealot, army);
// 					};
// 				}
// 				if (count_units_plus_production(st, unit_types::citadel_of_adun) == 0) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::citadel_of_adun, army);
// 					};
// 				}
// 			}
// 		}
// 
// 		if (probe_count >= 16 && dragoon_count >= 2) {
// 			if (observer_count == 0) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::observer, army);
// 				};
// 			}
// 		}
// 
// 		army = [army](state&st) {
// 			return nodelay(st, unit_types::probe, army);
// 		};
// 
// 		if (probe_count >= 16 && (!defence_fight_ok || (probe_count >= 24 && count_production(st, unit_types::probe)))) {
// 			if (!defence_fight_ok) {
// 				army = [army](state&st) {
// 					return maxprod(st, unit_types::zealot, army);
// 				};
// 			}
// 			if (count_production(st, unit_types::dragoon) == 0 || !defence_fight_ok) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::dragoon, army);
// 				};
// 			}
// 		}
// 
// 		if (ground_army_supply >= 8.0) {
// 			//if (shuttle_count < ground_army_supply / 10.0) {
// // 			if (shuttle_count < dragoon_count) {
// // 				army = [army](state&st) {
// // 					return maxprod(st, unit_types::shuttle, army);
// // 				};
// // 			}
// 		}
// 
// 		if (army_supply >= 16.0 && count_production(st, unit_types::probe) == 0) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::probe, army);
// 			};
// 		}
// 
// 		if (enemy_dt_count && observer_count == 0) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::observer, army);
// 			};
// 		}
// 
// 		if (st.used_supply[race_protoss] >= 29) {
// 			if (count_units_plus_production(st, unit_types::gateway) < 3) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::gateway, army);
// 				};
// 			}
// 		}
// 
// 		if (army_supply >= 16.0 && force_expand && count_production(st, unit_types::nexus) == 0) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::nexus, army);
// 			};
// 		}

		return army(st);
	}

};

