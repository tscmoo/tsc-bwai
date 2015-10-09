

struct strat_z_lategame : strat_z_base {

	virtual void init() override {


	}
	bool fight_ok = true;
	virtual bool tick() override {

		if (combat::no_aggressive_groups) {
			combat::no_aggressive_groups = false;
			combat::aggressive_groups_done_only = true;
			if (enemy_army_supply < 16 && current_used_total_supply - my_workers.size() >= 40) {
				combat::no_aggressive_groups = false;
				combat::aggressive_groups_done_only = false;
			}
		}

		auto eval_combat = [&](bool include_static_defence) {
			combat_eval::eval eval;
			eval.max_frames = 15 * 120;
			for (unit*u : my_units) {
				if (!u->is_completed) continue;
				if (!include_static_defence && u->building) continue;
				if (!u->stats->ground_weapon) continue;
				if (u->type->is_worker || u->type->is_non_usable) continue;
				auto&c = eval.add_unit(u, 0);
				if (u->building) c.move = 0.0;
			}
			for (unit*u : enemy_units) {
				if (u->building || u->type->is_worker || u->type->is_non_usable) continue;
				eval.add_unit(u, 1);
			}
			eval.run();
			a_map<unit_type*, int> my_count, op_count;
			for (auto&v : eval.teams[0].units) ++my_count[v.st->type];
			for (auto&v : eval.teams[1].units) ++op_count[v.st->type];
			log("eval_combat::\n");
			log("my units -");
			for (auto&v : my_count) log(" %dx%s", v.second, short_type_name(v.first));
			log("\n");
			log("op units -");
			for (auto&v : op_count) log(" %dx%s", v.second, short_type_name(v.first));
			log("\n");

			log("result: score %g %g  supply %g %g  damage %g %g  in %d frames\n", eval.teams[0].score, eval.teams[1].score, eval.teams[0].end_supply, eval.teams[1].end_supply, eval.teams[0].damage_dealt, eval.teams[1].damage_dealt, eval.total_frames);
			//return eval.teams[0].score > eval.teams[1].score;
			return eval.teams[1].end_supply == 0;
		};

		fight_ok = eval_combat(false);
		
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

		if (drone_count < 90 && ((fight_ok && army_supply < drone_count * 2) || count_production(st, unit_types::drone) == 0)) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

// 		if (larva_count < 3 && hatch_count < (drone_count + 3) / 4) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::hatchery, army);
// 			};
// 		}

		auto tech = [&]() {
			if (count_units_plus_production(st, unit_types::hive) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hive, army);
				};
			} else if (!st.units[unit_types::hive].empty()) {
				if (count_units_plus_production(st, unit_types::greater_spire) == 0) {
					bool all_spires_are_busy = true;
					for (unit*u : my_completed_units_of_type[unit_types::spire]) {
						if (!u->remaining_upgrade_time) {
							all_spires_are_busy = false;
							break;
						}
					}
					army = [army](state&st) {
						return nodelay(st, unit_types::greater_spire, army);
					};
					if (all_spires_are_busy && count_units_plus_production(st, unit_types::spire) < 3) {
						army = [army](state&st) {
							return nodelay(st, unit_types::spire, army);
						};
					}
				}
				if (enemy_ground_army_supply >= 40 || army_supply >= 60 || enemy_weak_against_ultra_supply >= 14) {
					if (count_units_plus_production(st, unit_types::ultralisk_cavern) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::ultralisk_cavern, army);
						};
					}
				}
				if (ground_army_supply >= 14 || army_supply >= 60) {
					if (count_units_plus_production(st, unit_types::defiler_mound) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::defiler_mound, army);
						};
					}
				}
			}
		};

		if (drone_count >= 50) {
			tech();

			if (count_units_plus_production(st, unit_types::evolution_chamber) < (army_supply >= enemy_army_supply ? 2 : 1)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::evolution_chamber, army);
				};
			}
		}

		if (!fight_ok || (drone_count >= 34 && army_supply < enemy_army_supply) || drone_count >= 70) {
			if (zergling_count < 80 || enemy_ground_army_supply > ground_army_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
			if (zergling_count >= 20 || (players::my_player->has_upgrade(upgrade_types::zerg_missile_attacks_2) && !players::my_player->has_upgrade(upgrade_types::zerg_melee_attacks_1))) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
			army = [army](state&st) {
				return nodelay(st, unit_types::mutalisk, army);
			};
			if (hydralisk_count < mutalisk_count * 2 && ground_army_supply >= enemy_ground_army_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}

			if (hydralisk_count < guardian_count * 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}

			if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
				if (lurker_count * 2 < enemy_ground_small_army_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lurker, army);
					};
				}
				if (mutalisk_count / 2 < lurker_count) {
					army = [army](state&st) {
						return nodelay(st, unit_types::mutalisk, army);
					};
				}
			}

			if (hydralisk_count < enemy_ground_large_army_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}

			if (!st.units[unit_types::greater_spire].empty() && enemy_air_army_supply > scourge_count + hydralisk_count + mutalisk_count) {
				if (devourer_count < (hydralisk_count + scourge_count / 2 + mutalisk_count) / 2) {
					army = [army](state&st) {
						return nodelay(st, unit_types::mutalisk, army);
					};
					army = [army](state&st) {
						return nodelay(st, unit_types::devourer, army);
					};
				}
			}
			if (devourer_count > mutalisk_count * 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::mutalisk, army);
				};
			}

			if (!st.units[unit_types::ultralisk_cavern].empty()) {
				if (ultralisk_count * 4 < enemy_weak_against_ultra_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::ultralisk, army);
					};
				}
			}

			if (!st.units[unit_types::ultralisk_cavern].empty()) {
				if (ultralisk_count * 4 < enemy_weak_against_ultra_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::ultralisk, army);
					};
				}
			}

			if (mutalisk_count < enemy_tank_count) {
				army = [army](state&st) {
					return nodelay(st, unit_types::mutalisk, army);
				};
			}
			if (hydralisk_count < enemy_goliath_count) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}

			if (enemy_tank_count >= 4) {
				if (queen_count < enemy_tank_count / 4 && queen_count * 2 < army_supply / 2 && air_army_supply > queen_count * 2 + 8) {
					army = [army](state&st) {
						return nodelay(st, unit_types::queen, army);
					};
				}
			}

			if (mutalisk_count + hydralisk_count < enemy_air_army_supply) {
				if ((st.gas > st.minerals || hydralisk_count > mutalisk_count * 2) && mutalisk_count + mutalisk_count / 2 > enemy_corsair_count) {
					army = [army](state&st) {
						return nodelay(st, unit_types::mutalisk, army);
					};
				} else {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk, army);
					};
				}
			}
			if (scourge_count < enemy_air_army_supply && (hydralisk_count >= enemy_air_army_supply / 2 || enemy_air_army_supply < 20.0)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scourge, army);
				};
			}

			if (!st.units[unit_types::defiler_mound].empty()) {
				if (defiler_count < ground_army_supply / 20) {
					army = [army](state&st) {
						return nodelay(st, unit_types::defiler, army);
					};
				}
				if (defiler_count < zergling_count / 14) {
					army = [army](state&st) {
						return nodelay(st, unit_types::defiler, army);
					};
				}
// 				if (ground_army_supply < enemy_ground_army_supply && zergling_count < defiler_count * 6) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::zergling, army);
// 					};
// 				}
			}
			if (!st.units[unit_types::greater_spire].empty()) {
				if (enemy_air_army_supply >= 20) {
					double mult = enemy_air_army_supply > enemy_ground_army_supply ? 0.5 : 0.25;
					if (devourer_count < (int)(mutalisk_count * mult)) {
						army = [army](state&st) {
							return nodelay(st, unit_types::devourer, army);
						};
					}
				}
				if (enemy_ground_army_supply >= 20) {
					double mult = enemy_ground_army_supply > enemy_air_army_supply ? 0.5 : 0.25;
					if (guardian_count < (int)(mutalisk_count * mult)) {
						army = [army](state&st) {
							return nodelay(st, unit_types::guardian, army);
						};
					}
				}
			}


			if (army_supply >= enemy_army_supply * 2) {
				if (ground_army_supply < air_army_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk, army);
					};
				} else {
					army = [army](state&st) {
						return nodelay(st, unit_types::mutalisk, army);
					};
				}
				if (!st.units[unit_types::greater_spire].empty()) {
					if (guardian_count < enemy_static_defence_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::guardian, army);
						};
					}
				}
			}

		}

		//if ((army_supply > enemy_army_supply*1.25 && drone_count >= 40) || drone_count >= 60 || army_supply >= 50) {
		if ((army_supply > enemy_army_supply*1.25 && drone_count >= 40) || (drone_count >= 60 && army_supply >= 50)) {
			tech();
		}

// 		if (enemy_ground_large_army_supply - hydralisk_count >= 10) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::hydralisk, army);
// 			};
// 		}

		if (drone_count >= 45) {
			if (count_units_plus_production(st, unit_types::hive) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hive, army);
				};
			}
		}

		if (force_expand && count_production(st, unit_types::hatchery) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		if (drone_count < 24 || (army_supply >= enemy_army_supply && air_army_supply + hydralisk_count >= enemy_air_army_supply && count_production(st, unit_types::drone) < 2)) {
			if (drone_count < 70) {
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

		return army(st);
	}

};

