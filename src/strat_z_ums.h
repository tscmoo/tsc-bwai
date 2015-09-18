

struct strat_z_ums : strat_z_base {

	virtual void init() override {

		sleep_time = 15;

		scouting::no_proxy_scout = true;
		scouting::scout_supply = 20.0;

		default_worker_defence = false;

		units::dont_call_gg = true;
		game->setLocalSpeed(-1);

		get_next_base = []() {
			auto st = buildpred::get_my_current_state();
			unit_type*worker_type = unit_types::drone;
			unit_type*cc_type = unit_types::hatchery;
			a_vector<refcounted_ptr<resource_spots::spot>> available_bases;
			for (auto&s : resource_spots::spots) {
				if (grid::get_build_square(s.cc_build_pos).building) continue;
				bool okay = true;
				for (auto&v : st.bases) {
					if ((resource_spots::spot*)v.s == &s) okay = false;
				}
				if (!okay) continue;
				if (!build::build_is_valid(grid::get_build_square(s.cc_build_pos), cc_type)) continue;

				if (!my_resource_depots.empty()) {
					xy from_pos = my_resource_depots.front()->pos;
					bool can_reach = true;
					for (auto*n : square_pathing::find_path(square_pathing::get_pathing_map(worker_type), from_pos, s.cc_build_pos)) {
						xy pos = n->pos;
						if (!combat::can_transfer_through(n->pos)) {
							can_reach = false;
							break;
						}
					}
					if (!can_reach) continue;
				}

				available_bases.push_back(&s);
			}
			return get_best_score(available_bases, [&](resource_spots::spot*s) {
				//double score = unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
				double score = get_best_score_value(my_workers, [&](unit*u) {
					return unit_pathing_distance(u, s->cc_build_pos);
				});
				//if (my_resource_depots.empty() && s->resources.size() >= 5) return score;
				if (my_resource_depots.empty()) {
					size_t size = 0;
					for (auto&v : s->resources) ++size;
					if (size >= 5) return score;
				}
				double res = 0;
				double ned = get_best_score_value(enemy_buildings, [&](unit*e) {
					if (e->type->is_worker) return std::numeric_limits<double>::infinity();
					return diag_distance(s->pos - e->pos);
				});
				if (ned <= 32 * 30) score += 10000;
				bool has_gas = false;
				for (auto&r : s->resources) {
					res += r.u->resources;
					if (r.u->type->is_gas) has_gas = true;
				}
				score -= (has_gas ? res : res / 4) / 10 + ned;
				return score;
			}, std::numeric_limits<double>::infinity());
		};

	}

	double defending_enemy_army_supply = 0.0;
	double defending_enemy_air_army_supply = 0.0;
	double defending_enemy_ground_army_supply = 0.0;
	double engaged_enemy_army_supply = 0.0;

	a_unordered_map<unit*, int> last_been_near_unit;

	int max_workers = 0;
	virtual bool tick() override {

		max_workers = get_max_mineral_workers() + my_units_of_type[unit_types::extractor].size() * 3;

// 		for (unit*u : my_workers) {
// 			u->force_combat_unit = my_resource_depots.empty();
// 		}

		defending_enemy_army_supply = 0.0;
		defending_enemy_air_army_supply = 0.0;
		defending_enemy_ground_army_supply = 0.0;
		for (auto&g : combat::groups) {
			if (!g.is_aggressive_group && !g.is_defensive_group) {
				for (unit*e : g.enemies) {
					defending_enemy_army_supply += e->type->required_supply;
					if (e->is_flying) defending_enemy_air_army_supply += e->type->required_supply;
					else defending_enemy_ground_army_supply += e->type->required_supply;
				}
			}
		}

		engaged_enemy_army_supply = 0.0;
		for (auto&g : combat::groups) {
			if (g.is_defensive_group) continue;
			if (!g.allies.empty() && !g.enemies.empty()) {
				for (unit*e : g.enemies) {
					engaged_enemy_army_supply += e->type->required_supply;
				}
			}
		}

		bool has_engaged_group = false;
		for (auto&g : combat::groups) {
			if (g.is_defensive_group) continue;
			if (!g.allies.empty() && !g.enemies.empty()) {
				has_engaged_group = true;
				break;
			}
		}
		bool has_group_with_enemies = false;
		for (auto&g : combat::groups) {
			if (g.is_defensive_group) continue;
			for (unit*u : g.enemies) {
				if (u->owner->is_enemy && current_frame - u->last_seen < 15 * 20) {
					has_group_with_enemies = true;
					break;
				}
			}
			if (has_group_with_enemies) break;
// 			if (!g.enemies.empty()) {
// 				has_group_with_enemies = true;
// 				break;
// 			}
		}
		//if (my_workers.size() == 1 && current_used_total_supply == 1.0 && !my_units_of_type[unit_types::scv].empty()) has_group_with_enemies = false;

		if (has_group_with_enemies) rm_all_scouts();

		log(log_level_info, "is_attacking is %d, has_engaged_group is %d, has_group_with_enemies is %d\n", is_attacking, has_engaged_group, has_group_with_enemies);
		log(log_level_info, "current_used_supply[race_zerg] is %g\n", current_used_supply[race_zerg]);
		if ((is_attacking && !has_engaged_group && !has_group_with_enemies) || (!has_engaged_group && current_used_supply[race_zerg] < 2.0)) {

			a_vector<std::tuple<double, unit*>> units_of_interest;
			for (unit*u : live_units) {
				if (u->owner == players::my_player) continue;
				if (u->gone) continue;
				int&t = last_been_near_unit[u];
				double dist = get_best_score_value(my_units, [&](unit*u2) {
					return units_distance(u, u2);
				});
				if (dist <= 32) t = current_frame;
				int age = current_frame - t;
				if (t > 60 && age < 15 * 60 * 2) continue;
				double s = 4500.0 / (age / (15.0 * 60));
				units_of_interest.emplace_back(s, u);
			}
			log(log_level_info, "units_of_interest.size() is %d\n", units_of_interest.size());

			if (!units_of_interest.empty()) {
				for (auto*a : combat::live_combat_units) {
					//if (a->u->type->is_worker) continue;
					if (a->u->type == unit_types::drone) continue;
					if (!a->u->stats->ground_weapon && !a->u->stats->air_weapon) continue;

					unit*target = std::get<1>(get_best_score(units_of_interest, [&](std::tuple<double, unit*> v) {
						double dist = units_pathing_distance(a->u, std::get<1>(v));
						if (std::get<1>(v)->type->is_worker) dist /= 4.0;
						return std::get<0>(v) * dist;
					}));

					if (target) {
						a->strategy_busy_until = current_frame + 20;
						a->action = combat::combat_unit::action_offence;
						a->subaction = combat::combat_unit::subaction_move;
						a->target_pos = target->pos;
					}
				}
			}

		}

		attack_interval = 15 * 60;
		combat::no_scout_around = !is_defending;
		if (!get_next_base() || my_workers.empty()) {
			if (current_minerals_per_frame == 0.0) {
				combat::no_aggressive_groups = false;
				combat::no_scout_around = true;
			}
		}
		//if ((int)my_workers.size() >= max_workers && current_used_total_supply - my_workers.size() > engaged_enemy_army_supply) {
		if ((int)my_workers.size() >= max_workers && current_used_total_supply - my_workers.size() > engaged_enemy_army_supply) {
			combat::no_aggressive_groups = false;
			combat::no_scout_around = true;
		}

// 		if ((int)my_workers.size() >= max_workers || my_workers.size() >= 20) {
// 			get_upgrades::set_upgrade_value(upgrade_types::ventral_sacs, -1.0);
// 		}
		
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		if (hatch_count == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
			return army(st);
		}

		if (drone_count < 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
			return army(st);
		}

		st.auto_build_hatcheries = true;

		if (drone_count < max_workers || drone_count < 6) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		double desired_army_supply = drone_count*drone_count * 0.015 + drone_count * 0.8 - 8;

		if (army_supply < desired_army_supply || defending_enemy_army_supply >= army_supply || drone_count >= max_workers) {
			if (zergling_count < 40 || st.minerals >= 1000) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
			if (count_units_plus_production(st, unit_types::hydralisk_den)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
				if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
					if (lurker_count * 2 < enemy_ground_small_army_supply - enemy_vulture_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::lurker, army);
						};
					}
				}
			}
			if (count_units_plus_production(st, unit_types::spire)) {
				if (mutalisk_count < enemy_tank_count || mutalisk_count < enemy_army_supply - enemy_anti_air_army_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::mutalisk, army);
					};
				}
			}
		}
		if (army_supply >= desired_army_supply || (army_supply >= defending_enemy_army_supply && drone_count >= max_workers && current_minerals_per_frame >= 1.0)) {
			if (drone_count >= 14) {
				if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk_den, army);
					};
				}
				if (st.gas >= 150 && count_units_plus_production(st, unit_types::lair) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lair, army);
					};
				}
				if (!my_completed_units_of_type[unit_types::lair].empty()) {
					if (count_units_plus_production(st, unit_types::hive) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hive, army);
						};
					}
					if (count_units_plus_production(st, unit_types::spire) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::spire, army);
						};
					}
				}
			}
		}

		if (sunken_count < (drone_count >= 18 ? 2 : drone_count >= 12 ? 1 : 0)) {
			army = [army](state&st) {
				return nodelay(st, unit_types::sunken_colony, army);
			};
		}

		if (hydralisk_count + scourge_count < defending_enemy_air_army_supply) {
			if (count_units_plus_production(st, unit_types::spire) && st.minerals < 200) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scourge, army);
				};
			} else {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
		}

		if (force_expand && count_production(st, unit_types::hatchery) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		return army(st);
	}

};

