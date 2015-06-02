

struct strat_z_ling_defiler : strat_z_base {

	virtual void init() override {

		combat::no_scout_around = true;
		combat::aggressive_zerglings = false;

		default_upgrades = false;

	}
	bool fight_ok = true;
	bool defence_fight_ok = true;
	bool attack = false;
	bool attack_engage = false;
	virtual bool tick() override {

		if (combat::no_aggressive_groups) {
			combat::no_aggressive_groups = true;
			combat::aggressive_groups_done_only = false;
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

		if (!my_units_of_type[unit_types::lair].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
			get_upgrades::set_upgrade_value(upgrade_types::adrenal_glands, -1.0);
			get_upgrades::set_upgrade_value(upgrade_types::consume, -1.0);
			get_upgrades::set_upgrade_value(upgrade_types::plague, -1.0);
		}

		if (!my_units_of_type[unit_types::defiler].empty()) {
			if (current_used_total_supply - my_workers.size() >= 40 || my_units_of_type[unit_types::defiler].size() >= 6) {
				attack = true;
			}
		}
		if (attack) {
			combat::no_aggressive_groups = false;
			combat::aggressive_groups_done_only = false;
			if (!attack_engage) {
				combat::combat_mult_override = 128.0;
				combat::combat_mult_override_until = current_frame + 15 * 10;
				int lose_count = 0;
				int total_count = 0;
				int defiler_count = 0;
				for (auto*a : combat::live_combat_units) {
					if (a->u->type->is_worker) continue;
					++total_count;
					if (current_frame - a->last_fight <= 15 * 10) {
						if (a->last_run >= a->last_fight) {
							++lose_count;
							if (a->u->type == unit_types::defiler && a->u->energy > 150) ++defiler_count;
						}
					}
				}
				//if (lose_count >= total_count / 2 + total_count / 4 && (defiler_count >= 2 || my_units_of_type[unit_types::defiler].size() < 2)) {
				//if (lose_count >= total_count / 2 + total_count / 4) {
				if (defiler_count >= 2) {
					log("engage!\n");
					attack_engage = true;
				}
			} else {
				combat::combat_mult_override = 0.75;
				combat::combat_mult_override_until = current_frame + 15 * 10;
			}
		}
		
		return current_used_total_supply >= 110;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		st.auto_build_hatcheries = true;

		st.dont_build_refineries = drone_count < 24;

// 		if (drone_count < 30) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::drone, army);
// 			};
// 		}
		army = [army](state&st) {
			return nodelay(st, unit_types::drone, army);
		};

		//if (drone_count >= 30 || (count_units_plus_production(st, unit_types::defiler_mound) && count_production(st, unit_types::drone))) {
		if (!st.units[unit_types::defiler_mound].empty()) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
			if (defiler_count < zergling_count / 8) {
				army = [army](state&st) {
					return nodelay(st, unit_types::defiler, army);
				};
			}
		}

		if (drone_count >= 13 && hatch_count < 3) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);

// 		bool build_army = is_defending;
// 		if (being_rushed && drone_count >= 13 && army_supply < 8) build_army = true;
// 		if (being_rushed && army_supply < enemy_army_supply + 4) build_army = true;

		bool build_army = is_defending && army_supply < enemy_army_supply;
		if (drone_count >= 28 && army_supply < 8.0) build_army = true;
		if (maybe_being_rushed && drone_count >= 24 && army_supply < 12.0) build_army = true;

		if (build_army) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
			if (hydralisk_count < enemy_air_army_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
		}

		if ((!is_defending && !being_rushed) || defence_fight_ok) {
			if (drone_count >= 13 && count_units_plus_production(st, unit_types::extractor) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
			if (count_units_plus_production(st, unit_types::defiler_mound) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::defiler_mound, army);
				};
			}
		}

// 		if (maybe_being_rushed || drone_count >= (opponent_has_expanded ? 26 : 16)) {
// 			if (count_units_plus_production(st, unit_types::sunken_colony) < drone_count / 8) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::sunken_colony, army);
// 				};
// 			}
// 		}
		if (my_completed_units_of_type[unit_types::hatchery].size() >= 2 && drone_count >= 16) {
			if (sunken_count < 1) {
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

		if (attack && (army_supply >= enemy_army_supply / 2 || army_supply >= enemy_attacking_army_supply)) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		return army(st);
	}

};

