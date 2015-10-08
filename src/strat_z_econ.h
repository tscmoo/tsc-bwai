

struct strat_z_econ : strat_z_base {

	virtual void init() override {

		scouting::scout_supply = 9.0;

		combat::no_scout_around = true;
		combat::aggressive_zerglings = false;

		default_upgrades = false;

		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);

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
			return eval.teams[1].end_supply == 0;
		};

		fight_ok = eval_combat(false);
		defence_fight_ok = eval_combat(true);

		if (being_rushed) min_bases = 1;
		else min_bases = 3;

		if (being_rushed) rm_all_scouts();

		if (defence_fight_ok && my_workers.size() >= 28) {
			default_upgrades = true;
			if (enemy_ground_small_army_supply >= 12.0) {
				get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
			}
		}
		if (zergling_count >= 12) {
			get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
		}
		
		return current_used_total_supply >= 70;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		st.dont_build_refineries = drone_count < 30;

		st.auto_build_hatcheries = true;

		army = [army](state&st) {
			return nodelay(st, unit_types::drone, army);
		};

		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);

		bool build_army = is_defending;
		if (being_rushed && drone_count >= 13 && army_supply < 8) build_army = true;
		if (being_rushed && army_supply < enemy_army_supply + 4) build_army = true;

		if (drone_count >= 28 && army_supply < drone_count) build_army = true;

		if (build_army) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
			if (drone_count >= 28 || zergling_count >= 20) {
				if (hydralisk_count < enemy_ground_large_army_supply || zergling_count >= 20) {
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
				}
			}
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

		if (drone_count >= 18) {
			if (count_units_plus_production(st, unit_types::extractor) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			} else {
				if (count_units_plus_production(st, unit_types::evolution_chamber) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::evolution_chamber, army);
					};
				}
				if (count_units_plus_production(st, unit_types::lair) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lair, army);
					};
				} else {
					if (count_units_plus_production(st, unit_types::spire) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::spire, army);
						};
					}
					if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk_den, army);
						};
					}
				}
			}
		}

		if (army_supply < enemy_attacking_army_supply || army_supply < enemy_army_supply / 2) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}

		if (maybe_being_rushed) {
			if (drone_count >= 11 && zergling_count < 8) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
			if (drone_count >= 18 && zergling_count < 18) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
			if (defence_fight_ok && count_production(st, unit_types::drone) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			} else if (army_supply < enemy_attacking_army_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
		}

// 		if (!is_defending || army_supply >= 6.0) {
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

		return army(st);
	}

};

