
struct strat_zvz_opening {

	void run() {

		combat::no_aggressive_groups = true;
		combat::aggressive_zerglings = false;
		combat::no_scout_around = true;

		scouting::no_proxy_scout = true;

		using namespace buildpred;

		get_upgrades::set_upgrade_value(upgrade_types::burrow, 100000.0);
		get_upgrades::set_upgrade_value(upgrade_types::ventral_sacs, 100000.0);

		get_upgrades::set_no_auto_upgrades(true);

		auto place_static_defence = [&]() {
			update_possible_start_locations();
			xy op_pos;
			if (!possible_start_locations.empty()) op_pos = possible_start_locations.front();
			if (!enemy_buildings.empty()) op_pos = enemy_buildings.front()->pos;
			unit*nearest_cc = get_best_score(my_buildings, [&](unit*u) {
				if (!u->type->is_resource_depot) return std::numeric_limits<double>::infinity();
				return square_pathing::get_distance(square_pathing::get_pathing_map(unit_types::drone, square_pathing::pathing_map_index::no_enemy_buildings), u->pos, op_pos);
				//return unit_pathing_distance(unit_types::drone, u->pos, op_pos);
			}, std::numeric_limits<double>::infinity());
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					build::unset_build_pos(&b);
					std::array<xy, 1> starts;
					starts[0] = nearest_cc ? nearest_cc->pos : my_start_location;
					xy pos = build_spot_finder::find(starts, b.type->unit, [&](grid::build_square&bs) {
						return build::build_has_existing_creep(bs, b.type->unit);
					});
					if (pos == xy() || diag_distance(starts[0] - pos) >= 32 * 15) b.dead = true;
					else build::set_build_pos(&b, pos);
				}
			}
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

			if (st.bases.size() >= 2) return;

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

		int opening_state = 0;

		a_vector<combat::combat_unit*> scouting_overlords;
		bool has_expanded = false;
		int highest_enemy_zergling_count = 0;
		while (true) {

			int my_zergling_count = my_units_of_type[unit_types::zergling].size();
			int my_mutalisk_count = my_units_of_type[unit_types::mutalisk].size();

			int my_hatch_count = my_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() + my_units_of_type[unit_types::hive].size();
			int my_completed_hatch_count = my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() + my_units_of_type[unit_types::hive].size();

			int enemy_zergling_count = 0;
			int enemy_spire_count = 0;
			int enemy_mutalisk_count = 0;
			int enemy_hydralisk_count = 0;
			int enemy_sunken_count = 0;
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
				if (e->type == unit_types::sunken_colony) ++enemy_sunken_count;
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

			if (enemy_zergling_count > highest_enemy_zergling_count) highest_enemy_zergling_count = enemy_zergling_count;

			if (!my_completed_units_of_type[unit_types::zergling].empty() && enemy_zergling_count + enemy_sunken_count == 0) scouting::scout_supply = 0;
			else {
				scouting::scout_supply = 80;
				if (!scouting::all_scouts.empty()) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
			}

			for (auto*a : scouting_overlords) {
				a->strategy_busy_until = 0;
				a->action = combat::combat_unit::action_idle;
				a->subaction = combat::combat_unit::subaction_idle;
			}
			auto prev_scouting_overlords = scouting_overlords;
			scouting_overlords.clear();

			if (enemy_spire_count + enemy_hydralisk_den_count + enemy_spore_count == 0 && my_units_of_type[unit_types::mutalisk].empty()) {

				a_vector<xy> scouted_positions;
				auto send_scout_to = [&](xy pos) {
					for (xy v : scouted_positions) {
						if (diag_distance(v - pos) <= 32 * 10) return;
					}
					scouted_positions.push_back(pos);
					auto*a = get_best_score(combat::live_combat_units, [&](combat::combat_unit*a) {
						if (!a->u->is_flying || a->u->stats->ground_weapon || a->u->stats->air_weapon) return std::numeric_limits<double>::infinity();
						if (a->subaction != combat::combat_unit::subaction_idle) return std::numeric_limits<double>::infinity();
						return diag_distance(pos - a->u->pos);
					}, std::numeric_limits<double>::infinity());

					if (a) {
						scouting_overlords.push_back(a);
						a->strategy_busy_until = current_frame + 15 * 10;
						a->action = combat::combat_unit::action_offence;
						a->subaction = combat::combat_unit::subaction_move;
						a->target_pos = pos + xy(4 * 16, 3 * 16);
					}
				};

				update_possible_start_locations();
				a_vector<xy> bases_to_scout;
				for (xy pos : possible_start_locations) {
					auto&bs = grid::get_build_square(pos);
					if (bs.last_seen > 30 && (!bs.building || bs.building->owner == players::my_player)) continue;
					bases_to_scout.push_back(pos);
				}
				std::sort(bases_to_scout.begin(), bases_to_scout.end(), [&](xy a, xy b) {
					return diag_distance(my_start_location - a) < diag_distance(my_start_location - b);
				});

				if (scouting_overlords.empty()) {
					for (xy pos : bases_to_scout) {
						send_scout_to(pos);
					}
				}
				for (auto&v : get_op_current_state().bases) {
					send_scout_to(v.s->cc_build_pos);
				}
				for (unit*u : enemy_buildings) {
					if (!u->type->is_resource_depot) continue;
					send_scout_to(u->building->build_pos);
				}
				if (bases_to_scout.size() == 1) {
					xy op_start_loc = bases_to_scout.front();

					a_vector<std::tuple<double, xy>> possible_expos;
					for (auto&s : resource_spots::spots) {
						if (s.cc_build_pos == op_start_loc) continue;
						auto&bs = grid::get_build_square(s.cc_build_pos);
						if (bs.building) continue;
						possible_expos.emplace_back(unit_pathing_distance(unit_types::scv, op_start_loc, s.cc_build_pos), s.cc_build_pos);
					}
					std::sort(possible_expos.begin(), possible_expos.end());
					if (possible_expos.size() > 2) possible_expos.resize(2);
					auto*expo1s = possible_expos.size() >= 1 ? &grid::get_build_square(std::get<1>(possible_expos[0])) : nullptr;
					auto*expo2s = possible_expos.size() >= 2 ? &grid::get_build_square(std::get<1>(possible_expos[1])) : nullptr;
					if (expo1s) send_scout_to(expo1s->pos);
					if (expo2s) send_scout_to(expo2s->pos);
				}
			}
			for (auto*a : prev_scouting_overlords) {
				if (a->action == combat::combat_unit::action_idle) {
					if (diag_distance(my_start_location - a->u->pos) >= 15 * 10) {
						a->strategy_busy_until = current_frame + 15 * 10;
						a->action = combat::combat_unit::action_offence;
						a->subaction = combat::combat_unit::subaction_move;
						a->target_pos = my_start_location;
					}
				}
			}

			auto eval_combat = [&](bool include_static_defence) {
				combat_eval::eval eval;
				eval.max_frames = 15 * 120;
				for (unit*u : my_units) {
					if (!u->is_completed) continue;
					if (!include_static_defence && u->building) continue;
					if (!u->stats->ground_weapon) continue;
					if (u->is_flying || u->type->is_worker || u->type->is_non_usable) continue;
					auto&c = eval.add_unit(u, 0);
					if (u->building) c.move = 0.0;
				}
				for (unit*u : enemy_units) {
					if (u->building || u->is_flying || u->type->is_worker || u->type->is_non_usable) continue;
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
				return eval.teams[0].score / eval.teams[1].score;
			};
			double ground_win_ratio = eval_combat(false);
			double ground_defence_win_ratio = eval_combat(true);

			log("ground_win_ratio is %g\n", ground_win_ratio);
			log("ground_defence_win_ratio is %g\n", ground_defence_win_ratio);

			bool opponent_going_mutas = enemy_spire_count || (enemy_lair_count && enemy_mined_gas >= 300.0);

			resource_gathering::max_gas = 100.0;
			if (!my_units_of_type[unit_types::lair].empty()) {
				resource_gathering::max_gas = 1.0;
				if (my_units_of_type[unit_types::lair].front()->remaining_build_time <= 15 * 45) {
					resource_gathering::max_gas = 150.0;
				}
			}
			bool build_mutas = false;
			if (!my_units_of_type[unit_types::spire].empty()) {
				resource_gathering::max_gas = 100.0;
				if (my_units_of_type[unit_types::spire].front()->remaining_build_time <= 15 * 60 || enemy_spire_count) {
					resource_gathering::max_gas = opponent_going_mutas ? 1000.0 : 300.0;
					if (my_workers.size() < 9) resource_gathering::max_gas = 120.0;
					build_mutas = true;
				}
			}

			bool attack_ground = ground_win_ratio >= 16.0 || (ground_win_ratio >= 1.0 && opponent_going_mutas);
			if (my_zergling_count >= 10 && enemy_zergling_count <= 2) attack_ground = true;
			bool attack_air = my_mutalisk_count >= enemy_mutalisk_count + 2;
			combat::aggressive_zerglings = attack_ground;
			combat::aggressive_mutalisks = attack_air;
			combat::no_aggressive_groups = !(attack_ground && (my_mutalisk_count == 0 || attack_air));


			auto build = [&](state&st) {

				int larva_count = 0;
				for (int i = 0; i < 3; ++i) {
					for (st_unit&u : st.units[i == 0 ? unit_types::hive : i == 1 ? unit_types::lair : unit_types::hatchery]) {
						larva_count += u.larva_count;
					}
				}

				int drone_count = count_units_plus_production(st, unit_types::drone);
				int zergling_count = count_units_plus_production(st, unit_types::zergling);
				int mutalisk_count = count_units_plus_production(st, unit_types::mutalisk);
				int scourge_count = count_units_plus_production(st, unit_types::scourge);
				int spire_count = count_units_plus_production(st, unit_types::spire);
				int hatch_count = count_units_plus_production(st, unit_types::hatchery) + count_units_plus_production(st, unit_types::lair) + count_units_plus_production(st, unit_types::hive);
				int sunken_count = count_units_plus_production(st, unit_types::sunken_colony);

				st.dont_build_refineries = true;

				if (drone_count < 5) return depbuild(st, state(st), unit_types::drone);

				std::function<bool(state&)> army = [&](state&st) {
					return false;
				};

				if (current_minerals >= 300) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}

				army = [army](state&st) {
					return nodelay(st, unit_types::mutalisk, army);
				};

				if (drone_count < 24 && ground_defence_win_ratio >= 1.5) {
					army = [army](state&st) {
						return nodelay(st, unit_types::drone, army);
					};
				}

				if (ground_defence_win_ratio < 2.0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::zergling, army);
					};
				}

				if (ground_defence_win_ratio >= 1.5 || enemy_spire_count) {
					army = [army](state&st) {
						return nodelay(st, unit_types::mutalisk, army);
					};
				}

				if (drone_count < enemy_drone_count + 2 || ground_defence_win_ratio >= 1.5) {
					if (drone_count < enemy_drone_count || (drone_count < std::max(16, enemy_drone_count + 8) && count_production(st, unit_types::drone) < 2)) {
						army = [army](state&st) {
							return nodelay(st, unit_types::drone, army);
						};
					}
				}

				if (zergling_count < (enemy_lair_count ? 6 : enemy_total_hatcheries ? 12 : 16)) {
					army = [army](state&st) {
						return nodelay(st, unit_types::zergling, army);
					};
				}

				if (zergling_count >= 6 && drone_count < 11) {
					army = [army](state&st) {
						return nodelay(st, unit_types::drone, army);
					};
				}

				if (!my_units_of_type[unit_types::spire].empty() && mutalisk_count < 10) {
					if (ground_defence_win_ratio < 2.0 && st.units[unit_types::mutalisk].size() < 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::zergling, army);
						};
					}
					if (build_mutas) {
						army = [army](state&st) {
							return nodelay(st, unit_types::mutalisk, army);
						};
						army = [army](state&st) {
							return nodelay(st, unit_types::mutalisk, army);
						};
						if (enemy_spire_count) {
							if (scourge_count < enemy_mutalisk_count * 2 + 4) {
								army = [army](state&st) {
									return nodelay(st, unit_types::scourge, army);
								};
							}
						}
					}
				}

				return army(st);
			};

			if (!has_expanded && get_my_current_state().bases.size() >= 2) has_expanded = true;

			if (my_units_of_type[unit_types::mutalisk].size() >= 2) {
				break;
			}

			if (opening_state == 0) {
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				opening_state = 1;
			} else if (opening_state == 1) {
				if (current_minerals >= 80) {
					build::add_build_task(1.0, unit_types::extractor);
					opening_state = 2;
				}
			} else if (opening_state == 2) {
				if (!my_units_of_type[unit_types::extractor].empty()) {
					build::add_build_task(2.0, unit_types::drone);
					build::add_build_task(3.0, unit_types::spawning_pool);
					build::add_build_task(3.5, unit_types::extractor);
					build::add_build_task(3.75, unit_types::drone);
					//build::add_build_task(3.75, unit_types::drone);
					build::add_build_task(4.0, unit_types::overlord);
					build::add_build_task(4.75, unit_types::lair);
					build::add_build_task(5.0, unit_types::zergling);
					build::add_build_task(5.0, unit_types::zergling);
					//build::add_build_task(5.25, unit_types::drone);
					build::add_build_task(5.5, unit_types::zergling);
					build::add_build_task(6.0, unit_types::drone);
					build::add_build_task(6.0, unit_types::drone);
					opening_state = 3;
				}
			} else if (opening_state == 3) {
				if (count_units_plus_production(get_my_current_state(), unit_types::drone) >= 9) {
					for (unit*u : my_units_of_type[unit_types::extractor]) {
						if (!u->is_completed) {
							u->game_unit->cancelConstruction();
							for (auto&b : build::build_tasks) {
								if (b.built_unit == u) {
									b.dead = true;
									break;
								}
							}
							opening_state = 4;
							break;
						}
					}
				}
			} else if (opening_state == 4) {
				bool all_done = true;
				for (auto&b : build::build_order) {
					if (current_frame - b.build_frame < 15 * 60) {
						all_done = false;
						break;
					}
				}
				if (all_done) opening_state = 5;
			} else execute_build(false, build);

			place_static_defence();
			if (opponent_going_mutas) place_expos();

			multitasking::sleep(15);
		}


		resource_gathering::max_gas = 0.0;

	}

	void render() {

	}

};

