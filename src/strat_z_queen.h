

struct strat_z_queen : strat_z_base {

	virtual void init() override {

		combat::no_scout_around = true;
		combat::aggressive_zerglings = false;

		default_upgrades = false;

		attack_interval = 15 * 15;

	}

	bool fight_ok = true;
	bool defence_fight_ok = true;
	virtual bool tick() override {


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
			int low_hp_or_dead_sunkens = 0;
			int total_sunkens = 0;
			for (auto&c : eval.teams[0].units) {
				if (c.st->type == unit_types::sunken_colony) {
					if (c.st->hp <= c.st->hp / 2) ++low_hp_or_dead_sunkens;
					++total_sunkens;
				}
			}
			if (total_sunkens - low_hp_or_dead_sunkens < 2 && total_sunkens >= 2) return false;
			return eval.teams[1].end_supply == 0;
		};

		fight_ok = eval_combat(false);
		defence_fight_ok = eval_combat(true);

		if (!my_units_of_type[unit_types::lair].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
			get_upgrades::set_upgrade_value(upgrade_types::spawn_broodling, -1.0);
			get_upgrades::set_upgrade_value(upgrade_types::gamete_meiosis, -1.0);
		}
		if (my_units_of_type[unit_types::queen].size() >= 8) {
			default_upgrades = true;
			get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, -1.0);
		}
		
		return current_used_total_supply >= 110;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		st.dont_build_refineries = drone_count < 24;

// 		army = [army](state&st) {
// 			return nodelay(st, unit_types::hatchery, army);
// 		};
		st.auto_build_hatcheries = true;

		if (drone_count < 30) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if (drone_count >= 30) {
			army = [army](state&st) {
				return nodelay(st, unit_types::queen, army);
			};
			if (queen_count >= 8 && hydralisk_count < 6) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
		}

// 		if (drone_count >= 13 && hatch_count < 3) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::hatchery, army);
// 			};
// 		}

		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);

		if (!defence_fight_ok) {
			army = [army](state&st) {
				return nodelay(st, unit_types::sunken_colony, army);
			};
		}
		if (my_completed_units_of_type[unit_types::hatchery].size() >= 2) {
			if (maybe_being_rushed && sunken_count < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
		}

// 		bool build_army = is_defending;
// 		if (being_rushed && drone_count >= 13 && army_supply < 8) build_army = true;
// 		if (being_rushed && army_supply < enemy_army_supply + 4) build_army = true;
// 		bool build_army = is_defending && army_supply < enemy_army_supply;
// 		if (drone_count >= 28 && army_supply < 8.0) build_army = true;
// 		if (maybe_being_rushed && drone_count >= 24 && army_supply < 12.0) build_army = true;
		bool build_army = is_defending && !defence_fight_ok;

		if (build_army) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
			if (ground_army_supply >= 14.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
			if (!st.units[unit_types::spire].empty()) {
				army = [army](state&st) {
					return nodelay(st, unit_types::mutalisk, army);
				};
			}
		}
// 		if (st.units[unit_types::spire].empty()) {
// 			if (hydralisk_count < enemy_air_army_supply) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::hydralisk, army);
// 				};
// 			}
// 		} else {
// 			if (scourge_count < enemy_air_army_supply) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::scourge, army);
// 				};
// 			}
// 		}

		if ((!is_defending && !being_rushed) || defence_fight_ok) {
			if (drone_count >= 13 && count_units_plus_production(st, unit_types::extractor) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
			if (drone_count >= 16) {
				if (count_units_plus_production(st, unit_types::lair) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lair, army);
					};
				} else {
					if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk_den, army);
						};
					}
				}
			}
			if (drone_count >= 30 && queen_count) {
				if (count_units_plus_production(st, unit_types::spire) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::spire, army);
					};
				}
			}
		}

// 		if (army_supply < enemy_attacking_army_supply || army_supply < enemy_army_supply / 2) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::zergling, army);
// 			};
// 		}

// 		if (drone_count >= (opponent_has_expanded ? 28 : 18)) {
// 			if (enemy_army_supply >= 8) {
// 				if (hydralisk_count < 4) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::hydralisk, army);
// 					};
// 				}
// 			}
// 		}

		if (maybe_being_rushed) {
			if (drone_count >= 11 && zergling_count < 8) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
// 			if (drone_count >= 18 && zergling_count < 18) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::zergling, army);
// 				};
// 			}
			if (defence_fight_ok && count_production(st, unit_types::drone) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}

// 		if (!is_defending) {
// 			//if (maybe_being_rushed || drone_count >= (opponent_has_expanded ? 26 : 16)) {
// 			if (drone_count >= (maybe_being_rushed ? 16 : 26)) {
// 				int sunkens = drone_count / 8;
// 				if (maybe_being_rushed) ++sunkens;
// 				if (count_units_plus_production(st, unit_types::sunken_colony) < sunkens) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::sunken_colony, army);
// 					};
// 				}
// 			}
// 		}

		if (drone_count >= 26) {
			if (queen_count >= drone_count / 2 || (defence_fight_ok && count_production(st, unit_types::drone) < 2) || army_supply >= drone_count) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}
		if (drone_count >= 40 && hydralisk_count < queen_count) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hydralisk, army);
			};
		}

		double anti_air_supply = enemy_air_army_supply;
		if (defence_fight_ok && drone_count >= 26) anti_air_supply += 2;
		if (st.units[unit_types::spire].empty()) {
			if (hydralisk_count < anti_air_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
		} else {
			if (scourge_count < anti_air_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scourge, army);
				};
			}
		}

		return army(st);
	}

};

