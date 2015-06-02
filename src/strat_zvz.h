
struct strat_zvz {

	void run() {

		combat::no_aggressive_groups = true;
		combat::aggressive_zerglings = false;

		scouting::scout_supply = 40;

		using namespace buildpred;

		get_upgrades::set_no_auto_upgrades(true);


		auto long_distance_miners = [&]() {
			int count = 0;
			for (auto&g : resource_gathering::live_gatherers) {
				if (!g.resource) continue;
				unit*ru = g.resource->u;
				resource_spots::spot*rs = nullptr;
				for (auto&s : resource_spots::spots) {
					if (grid::get_build_square(s.cc_build_pos).building) continue;
					for (auto&r : s.resources) {
						if (r.u == ru) {
							rs = &s;
							break;
						}
					}
					if (rs) break;
				}
				if (rs) ++count;
			}
			return count;
		};

		auto get_next_base = [&]() {
			auto st = get_my_current_state();
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
				available_bases.push_back(&s);
			}
			return get_best_score(available_bases, [&](resource_spots::spot*s) {
				double score = unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
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
				score -= (has_gas ? res : res / 4) / 10;
				return score;
			}, std::numeric_limits<double>::infinity());
		};

		auto place_expos = [&]() {

			auto st = get_my_current_state();
			int free_mineral_patches = 0;
			for (auto&b : st.bases) {
				for (auto&s : b.s->resources) {
					resource_gathering::resource_t*res = nullptr;
					for (auto&s2 : resource_gathering::live_resources) {
						if (s2.u == s.u) {
							res = &s2;
							break;
						}
					}
					if (res) {
						if (res->gatherers.empty()) ++free_mineral_patches;
					}
				}
			}
			//if (free_mineral_patches >= 8 && long_distance_miners() == 0) return;
			if (free_mineral_patches >= 2 && long_distance_miners() == 0) return;

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::hatchery) {
					xy pos = b.build_pos;
					build::unset_build_pos(&b);
					auto s = get_next_base();
					if (s) pos = s->cc_build_pos;
					if (pos != xy()) build::set_build_pos(&b, pos);
					else build::unset_build_pos(&b);
				}
			}

		};
		
		while (true) {

			int my_drone_count = my_units_of_type[unit_types::drone].size();
			int my_zergling_count = my_units_of_type[unit_types::zergling].size();
			int my_mutalisk_count = my_units_of_type[unit_types::mutalisk].size();

			int enemy_zergling_count = 0;
			int enemy_spire_count = 0;
			int enemy_mutalisk_count = 0;
			int enemy_hydralisk_count = 0;
			int enemy_spore_count = 0;
			int enemy_lurker_count = 0;
			int enemy_hatchery_count = 0;
			int enemy_lair_count = 0;
			int enemy_hydralisk_den_count = 0;
			int enemy_drone_count = 0;
			int enemy_total_hatcheries = 0;
			double enemy_mined_gas = 0.0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::mutalisk) ++enemy_mutalisk_count;
				if (e->type == unit_types::spire || e->type == unit_types::greater_spire) ++enemy_spire_count;
				if (e->type == unit_types::zergling) ++enemy_zergling_count;
				if (e->type == unit_types::hydralisk) ++enemy_hydralisk_count;
				if (e->type == unit_types::hydralisk_den) ++enemy_hydralisk_den_count;
				if (e->type == unit_types::spore_colony) ++enemy_spore_count;
				if (e->type == unit_types::lurker) ++enemy_lurker_count;
				if (e->type == unit_types::hatchery) ++enemy_hatchery_count;
				if (e->type == unit_types::lair) ++enemy_lair_count;
				if (e->type->is_resource_depot) ++enemy_total_hatcheries;
				if (e->type == unit_types::drone) ++enemy_drone_count;
				if (e->type->is_refinery) enemy_mined_gas += e->initial_resources - e->resources;
			}
			if (enemy_spire_count == 0 && enemy_mutalisk_count) enemy_spire_count = 1;
			if (enemy_hydralisk_den_count == 0 && enemy_hydralisk_count) enemy_hydralisk_den_count = 1;

			bool enemy_has_anything_that_shoots_up = false;
			for (unit*e : enemy_units) {
				if (e->stats->air_weapon || e->marines_loaded) {
					enemy_has_anything_that_shoots_up = true;
					break;
				}
			}
			if (!enemy_has_anything_that_shoots_up && my_mutalisk_count >= 3) {
				unit*harass_target = get_best_score(enemy_units, [&](unit*e) {
					if (e->gone || (!e->type->is_worker && !e->type->is_resource_depot)) return std::numeric_limits<double>::infinity();
					double r = 0.0;
					for (unit*e2 : enemy_units) {
						if (!e2->type->is_worker && !e2->type->is_resource_depot) continue;
						if (units_distance(e, e2) <= 32 * 10) r += e2->type->is_worker ? 1.0 : 0.125;
					}
					return -r;
				}, std::numeric_limits<double>::infinity());
				if (harass_target) {
					combat::combat_unit*a = get_best_score(combat::live_combat_units, [&](combat::combat_unit*a) {
						if (a->u->type != unit_types::mutalisk) return std::numeric_limits<double>::infinity();
						return diag_distance(harass_target->pos - a->u->pos);
					});
					if (a && diag_distance(harass_target->pos - a->u->pos) > 32 * 15) {
						a->strategy_busy_until = current_frame + 15 * 5;
						a->action = combat::combat_unit::action_offence;
						a->subaction = combat::combat_unit::subaction_move;
						a->target_pos = harass_target->pos;
					}
				}
			}

			auto eval_combat = [&]() {
				combat_eval::eval eval;
				for (unit*u : my_units) {
					if (u->building || u->type->is_worker || u->type->is_non_usable) continue;
					eval.add_unit(u, 0);
				}
				for (unit*u : enemy_units) {
					if (u->building || u->type->is_worker || u->type->is_non_usable) continue;
					eval.add_unit(u, 1);
				}
				eval.run();
				return eval.teams[0].score / eval.teams[1].score;
			};
			double combat_win_ratio = eval_combat();

			combat::aggressive_zerglings = my_zergling_count >= enemy_zergling_count || enemy_mutalisk_count > my_mutalisk_count;
			combat::aggressive_mutalisks = my_mutalisk_count >= enemy_mutalisk_count + 2;
			combat::no_aggressive_groups = combat_win_ratio < 2.0 || current_minerals_per_frame == 0.0;

			if ((my_drone_count < 12 || my_units_of_type[unit_types::spawning_pool].empty()) && my_drone_count >= 4) {
				combat::aggressive_zerglings = false;
				combat::aggressive_mutalisks = false;
				combat::no_aggressive_groups = true;
			}

			if (my_zergling_count >= 16) {
				get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
			}
			if (current_used_total_supply >= 90 && get_upgrades::no_auto_upgrades) {
				get_upgrades::set_no_auto_upgrades(false);
			}

			bool force_expand = long_distance_miners() >= 1 && get_next_base();

			auto build = [&](state&st) {

				int drone_count = count_units_plus_production(st, unit_types::drone);
				int zergling_count = count_units_plus_production(st, unit_types::zergling);
				int mutalisk_count = count_units_plus_production(st, unit_types::mutalisk);
				int scourge_count = count_units_plus_production(st, unit_types::scourge);
				int hatch_count = count_units_plus_production(st, unit_types::hatchery) + count_units_plus_production(st, unit_types::lair) + count_units_plus_production(st, unit_types::hive);

				if (drone_count < 5) return depbuild(st, state(st), unit_types::drone);

				std::function<bool(state&)> army = [&](state&st) {
					return false;
				};

				if (count_production(st, unit_types::hatchery) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};

				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
				army = [army](state&st) {
					return nodelay(st, unit_types::mutalisk, army);
				};
				if (scourge_count <= mutalisk_count / 2 || enemy_mutalisk_count) {
					army = [army](state&st) {
						return nodelay(st, unit_types::scourge, army);
					};
				}

				if ((combat_win_ratio >= 1.25 && count_production(st, unit_types::drone) < 2) || (combat_win_ratio >= 0.75 && count_production(st, unit_types::drone) < 1)) {
					army = [army](state&st) {
						return nodelay(st, unit_types::drone, army);
					};
				}
				if ((drone_count < 12 || enemy_drone_count + 1 > my_drone_count) && count_production(st, unit_types::drone) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::drone, army);
					};
				}

				if (force_expand && count_production(st, unit_types::hatchery) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}
				
				return army(st);
			};

			execute_build(false, build);

			place_expos();

			multitasking::sleep(15 * 2);
		}


		resource_gathering::max_gas = 0.0;

	}

	void render() {

	}

};

