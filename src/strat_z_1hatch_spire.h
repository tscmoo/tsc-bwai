

struct strat_z_1hatch_spire : strat_z_base {

	virtual void init() override {


	}
	bool fight_ok = true;
	bool defence_fight_ok = true;
	double spire_progress = 0.0;
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
		defence_fight_ok = eval_combat(true);

		spire_progress = 0.0;
		for (unit*u : my_units_of_type[unit_types::spire]) {
			double p = 1.0 - (double)u->remaining_build_time / u->type->build_time;
			if (p > spire_progress) spire_progress = p;
		}
		
		return current_used_total_supply >= 62;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		st.dont_build_refineries = drone_count < 24;

		army = [army](state&st) {
			return nodelay(st, unit_types::hatchery, army);
		};

		army = [army](state&st) {
			return nodelay(st, unit_types::zergling, army);
		};
		army = [army](state&st) {
			return nodelay(st, unit_types::mutalisk, army);
		};

		if (spire_progress >= 0.5 && (spire_progress < 1.0 || my_units_of_type[unit_types::mutalisk].empty())) {
			if (drone_count < 30) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
			army = [army](state&st) {
				return nodelay(st, unit_types::mutalisk, army);
			};
			return army(st);
		}

		if (drone_count < 80) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if (drone_count >= 13 && hatch_count < 3) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);

		bool build_army = is_defending;
		if (being_rushed && drone_count >= 13 && army_supply < 8) build_army = true;
		if (being_rushed && army_supply < enemy_army_supply + 4) build_army = true;

		if (build_army) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
			if (spire_progress >= 0.5) {
				army = [army](state&st) {
					return nodelay(st, unit_types::mutalisk, army);
				};
			}
			if (hydralisk_count + scourge_count < enemy_air_army_supply) {
				if (spire_progress >= 0.5) {
					army = [army](state&st) {
						return nodelay(st, unit_types::scourge, army);
					};
				}
				if (spire_progress == 0.0 || count_units_plus_production(st, unit_types::hydralisk_den)) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk, army);
					};
				}
			}
		}

		if ((!is_defending && !being_rushed) || defence_fight_ok) {
			if (drone_count >= 13 && count_units_plus_production(st, unit_types::extractor) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
			if (drone_count >= 28 && spire_progress > 0.0) {
				if (count_units_plus_production(st, unit_types::evolution_chamber) < 2) {
					army = [army](state&st) {
						return nodelay(st, unit_types::evolution_chamber, army);
					};
				}
			}
			if (!st.units[unit_types::extractor].empty()) {
				if (count_units_plus_production(st, unit_types::lair) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lair, army);
					};
				}
				if (!st.units[unit_types::lair].empty()) {
					if (count_units_plus_production(st, unit_types::spire) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::spire, army);
						};
					}
				}
			}
			if (mutalisk_count >= 9) {
				if (count_units_plus_production(st, unit_types::evolution_chamber) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::evolution_chamber, army);
					};
				}
				if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk_den, army);
					};
				}
			}
		}

		if (maybe_being_rushed || drone_count >= (opponent_has_expanded ? 26 : 16)) {
			if (count_units_plus_production(st, unit_types::sunken_colony) < drone_count / 8) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
		}



		if (maybe_being_rushed) {
			if (drone_count >= 11 && zergling_count < 8) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
			if (defence_fight_ok && count_production(st, unit_types::drone) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}

		return army(st);
	}

};

