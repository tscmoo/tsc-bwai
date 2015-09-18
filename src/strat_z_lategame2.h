

struct strat_z_lategame2 : strat_z_base {

	virtual void init() override {

		get_upgrades::set_no_auto_upgrades(false);

	}
	bool fight_ok = true;
	int cur_n_mineral_patches = 0;
	int max_workers = 0;
	bool has_ultra_upgrades = false;
	virtual bool tick() override {

		attack_interval = 15 * 60 * 2;

		if (enemy_attacking_army_supply >= 16.0 && army_supply >= enemy_army_supply) {
			combat::no_aggressive_groups = false;
			combat::aggressive_groups_done_only = false;
			combat::aggressive_zerglings = false;
		}

		cur_n_mineral_patches = n_mineral_patches();

		max_workers = get_max_mineral_workers() + 12;

		if (players::my_player->has_upgrade(upgrade_types::anabolic_synthesis) && players::my_player->has_upgrade(upgrade_types::chitinous_plating)) has_ultra_upgrades = true;
		if (players::my_player->has_upgrade(upgrade_types::zerg_melee_attacks_3)) has_ultra_upgrades = true;
		
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		if (st.used_supply[race_zerg] >= 190 && sunken_count + spore_count < 20) {
			int total_n = sunken_count + spore_count;
			int spore_n = (int)(total_n * (enemy_air_army_supply / enemy_army_supply));
			if (spore_count < spore_n) {
				army = [army](state&st) {
					return nodelay(st, unit_types::spore_colony, army);
				};
			} else {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
		}

		st.auto_build_hatcheries = true;

// 		if (larva_count < st.minerals / 50 && larva_count < 40) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::hatchery, army);
// 			};
// 		}

		if (drone_count < 90) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

// 		if (larva_count < 3 && hatch_count < (drone_count + 3) / 4) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::hatchery, army);
// 			};
// 		}

		if (army_supply < enemy_army_supply * 1.5 || drone_count >= max_workers || drone_count >= 80) {
			if (zergling_count < 50) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
// 			if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
// 				if (enemy_ground_small_army_supply >= enemy_ground_army_supply * 0.75) {
// 					if (lurker_count * 2 < enemy_ground_small_army_supply * 1.5) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::lurker, army);
// 						};
// 					}
// 				}
// 			}
			if (enemy_ground_large_army_supply >= enemy_ground_army_supply * 0.75) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
			if (mutalisk_count < enemy_tank_count || mutalisk_count < enemy_army_supply - enemy_anti_air_army_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::mutalisk, army);
				};
			}
			if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
				if (lurker_count * 2 < enemy_ground_small_army_supply - enemy_vulture_count) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lurker, army);
					};
				}
			}
			if (hydralisk_count < enemy_anti_air_army_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}

			if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
				if (enemy_ground_small_army_supply >= enemy_ground_army_supply * 0.75) {
					if (lurker_count * 2 < enemy_ground_small_army_supply * 1.5) {
						army = [army](state&st) {
							return nodelay(st, unit_types::lurker, army);
						};
					}
				}
			}

			if (enemy_weak_against_ultra_supply >= enemy_ground_army_supply * 0.75) {
				if (!my_completed_units_of_type[unit_types::ultralisk_cavern].empty()) {
					if (ultralisk_count * 2 < enemy_weak_against_ultra_supply) {
						army = [army](state&st) {
							return nodelay(st, unit_types::ultralisk, army);
						};
					}
				}
			}

			if (ground_army_supply >= 40.0 || defiler_count == 0 || drone_count >= 45) {
				if (!my_completed_units_of_type[unit_types::defiler_mound].empty()) {
					if (defiler_count < ground_army_supply / 24.0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::defiler, army);
						};
					}
				}
			}

			if (army_supply >= enemy_army_supply + 16.0) {
				if (!st.units[unit_types::greater_spire].empty()) {
					if (guardian_count < enemy_static_defence_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::guardian, army);
						};
					}
				}
			}
			if (cur_n_mineral_patches >= 20) {
				if (army_supply >= enemy_army_supply) {
					if (enemy_anti_air_army_supply < enemy_army_supply / 4) {
						if (guardian_count * 2 < army_supply / 2) {
							army = [army](state&st) {
								return nodelay(st, unit_types::guardian, army);
							};
						}
					}
				}
				if (enemy_ground_large_army_supply >= enemy_army_supply / 2 && enemy_tank_count >= 5) {
					if (queen_count < enemy_tank_count * 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::queen, army);
						};
					}
				}
			}
			if (enemy_tank_count && std::min(enemy_ground_small_army_supply, enemy_anti_air_army_supply) < 16.0) {
				if (enemy_attacking_army_supply < army_supply*0.75 && queen_count < enemy_tank_count * 2) {
					army = [army](state&st) {
						return nodelay(st, unit_types::queen, army);
					};
				}
			}

			if (scourge_count < enemy_air_army_supply && scourge_count + hydralisk_count < enemy_air_army_supply) {
				if (enemy_air_army_supply < 20.0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::scourge, army);
					};
				} else {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk, army);
					};
				}
			}
		}

		if (drone_count >= 45) {
			if (count_units_plus_production(st, unit_types::hive) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hive, army);
				};
			}
			if (!my_completed_units_of_type[unit_types::hive].empty()) {
				if (count_units_plus_production(st, unit_types::ultralisk_cavern) == 0) {
					if (enemy_weak_against_ultra_supply >= enemy_ground_army_supply * 0.75) {
						army = [army](state&st) {
							return nodelay(st, unit_types::ultralisk_cavern, army);
						};
					}
// 					if (has_ultra_upgrades && ultralisk_count * 4 < enemy_weak_against_ultra_supply) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::ultralisk, army);
// 						};
// 					}
				}
				if (count_units_plus_production(st, unit_types::defiler_mound) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::defiler_mound, army);
					};
				}
			}
		}

		if (drone_count >= 60) {
			if (count_units_plus_production(st, unit_types::evolution_chamber) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::evolution_chamber, army);
				};
			}
		}

		if (drone_count < 24 || (army_supply >= enemy_army_supply && air_army_supply + hydralisk_count >= enemy_air_army_supply && count_production(st, unit_types::drone) < 2)) {
			if (drone_count < 90 && drone_count < max_workers && (drone_count < 50 || drone_count + drone_count / 2 < army_supply)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}

		if (st.max_supply[race_zerg] + count_production(st, unit_types::overlord) * 8 - 16 <= st.used_supply[race_zerg]) {
			army = [army](state&st) {
				return nodelay(st, unit_types::overlord, army);
			};
		}

		if (force_expand && count_production(st, unit_types::hatchery) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		return army(st);
	}

};

