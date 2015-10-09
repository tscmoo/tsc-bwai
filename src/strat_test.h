
struct strat_test {


	void run() {


		get_upgrades::set_no_auto_upgrades(true);
		combat::no_aggressive_groups = true;
		combat::no_scout_around = true;
		scouting::scout_supply = 9;
		
		using namespace buildpred;

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
				score -= (has_gas ? res : res / 4) / 10 + ned;
				return score;
			}, std::numeric_limits<double>::infinity());
		};
		auto place_expos = [&]() {

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::hatchery) {
					xy pos = b.build_pos;
					build::unset_build_pos(&b);
				}
			}

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

		bool is_attacking = false;
		double attack_my_lost;
		double attack_op_lost;
		int last_attack_begin = 0;
		int last_attack_end = 0;
		int attack_count = 0;

		auto begin_attack = [&]() {
			is_attacking = true;
			attack_my_lost = players::my_player->minerals_lost + players::my_player->gas_lost;
			attack_op_lost = players::opponent_player->minerals_lost + players::opponent_player->gas_lost;
			last_attack_begin = current_frame;
			++attack_count;
			log("begin attack\n");
		};
		auto end_attack = [&]() {
			is_attacking = false;
			last_attack_end = current_frame;
			log("end attack\n");
		};
		
		int opening_state = 0;
		int opening_substate = 0;
		bool drone_up = false;
		while (true) {

			int my_zergling_count = my_units_of_type[unit_types::zergling].size();

			int enemy_bunker_count = 0;
			int enemy_cannon_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::photon_cannon) ++enemy_cannon_count;
				if (e->type == unit_types::bunker) ++enemy_bunker_count;
			}

			if (!is_attacking) {

				bool attack = false;
				if (attack_count == 0) attack = true;
				if (current_frame - last_attack_end >= 15 * 30) attack = true;
				if (my_zergling_count < 8) attack = false;
				if (attack_count >= 1 && my_zergling_count < 30) attack = false;

				if (attack) begin_attack();

			} else {

				int lose_count = 0;
				int total_count = 0;
				for (auto*a : combat::live_combat_units) {
					if (a->u->type->is_worker) continue;
					if (!a->u->stats->ground_weapon && !a->u->stats->air_weapon) continue;
					++total_count;
					if (current_frame - a->last_fight <= 15 * 10) {
						if (a->last_run >= a->last_fight) ++lose_count;
					}
				}
				if (lose_count >= total_count / 2 + total_count / 4) {
					end_attack();
				}
				if (attack_count == 1 && (players::my_player->minerals_lost >= 400 || enemy_bunker_count + enemy_cannon_count)) {
					end_attack();
				}

			}

			combat::no_aggressive_groups = !is_attacking;
			if (is_attacking) {
// 				if (my_zergling_count >= 24) {
// 					for (auto*a : combat::live_combat_units) {
// 						a->u->force_combat_unit = true;
// 						a->strategy_attack_until = current_frame + 15 * 20;
// 					}
// 				}
			} else {
				int ling_n = 0;
				for (auto*a : combat::live_combat_units) {
					if (!a->u->type->is_worker) {
						++ling_n;
						if (ling_n > 6) {
							double ned = get_best_score_value(enemy_units, [&](unit*e) {
								return diag_distance(e->pos - a->u->pos);
							});
							if (ned > 32 * 12) {
								a->strategy_busy_until = current_frame + 15 * 2;
								a->action = combat::combat_unit::action_offence;
								a->subaction = combat::combat_unit::subaction_move;
								a->target_pos = my_start_location;
							}
						}
					}
				}
			}

			update_possible_start_locations();
			if (possible_start_locations.size() == 1) {
				for (auto&v : scouting::all_scouts) {
					scouting::rm_scout(v.scout_unit);
				}
			} else {
				for (unit*u : my_completed_units_of_type[unit_types::zergling]) {
					if (!scouting::is_scout(u)) scouting::add_scout(u);
				}
			}

			if (current_used_total_supply >= 38) drone_up = true;

			auto build = [&](state&st) {

				std::function<bool(state&)> army = [&](state&st) {
					return false;
				};

				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};

				if (drone_up) {
					army = [army](state&st) {
						return nodelay(st, unit_types::drone, army);
					};
				}

				return army(st);

			};

			if (my_workers.size() >= 20 || current_frame >= 15 * 60 * 16) break;

			auto all_done = [&]() {
				for (auto&b : build::build_order) {
					if (!b.built_unit) return false;
				}
				return true;
			};

			auto gas_trick = [&]() {
				if (!all_done()) return;
				if (opening_substate == 0) {
					if (current_minerals >= 80) {
						build::add_build_task(100.0, unit_types::extractor);
						build::add_build_task(101.0, unit_types::drone);
						++opening_substate;
					}
				} else {
					for (unit*u : my_units_of_type[unit_types::extractor]) {
						if (!u->is_completed) {
							u->game_unit->cancelConstruction();
							for (auto&b : build::build_tasks) {
								if (b.built_unit == u) {
									b.dead = true;
									break;
								}
							}
							++opening_state;
							opening_substate = 0;
							break;
						}
					}
				}
			};

			if (opening_state == 0) {
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// // 				build::add_build_task(1.0, unit_types::spawning_pool);
// // 				build::add_build_task(2.0, unit_types::overlord);
// // 				build::add_build_task(3.0, unit_types::zergling);
// // 				build::add_build_task(4.0, unit_types::zergling);
// // 				build::add_build_task(5.0, unit_types::zergling);
// // 				build::add_build_task(6.0, unit_types::hatchery);
// 				build::add_build_task(1.0, unit_types::hatchery);
// 				build::add_build_task(2.0, unit_types::overlord);
// 				build::add_build_task(3.0, unit_types::spawning_pool);
// 				build::add_build_task(4.0, unit_types::zergling);
// 				build::add_build_task(4.0, unit_types::zergling);
// 				build::add_build_task(4.0, unit_types::zergling);

				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(1.0, unit_types::hatchery);
				build::add_build_task(1.0, unit_types::hatchery);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(3.0, unit_types::spawning_pool);
				build::add_build_task(4.0, unit_types::drone);
				++opening_state;
			} else if (opening_state == 1) {
				gas_trick();
			} else if (opening_state == 2) {
				if (all_done()) {
					build::add_build_task(0.0, unit_types::overlord);
					build::add_build_task(0.5, unit_types::drone);
					build::add_build_task(0.5, unit_types::drone);
					build::add_build_task(1.0, unit_types::zergling);
					build::add_build_task(1.0, unit_types::zergling);
					build::add_build_task(1.0, unit_types::zergling);
					++opening_state;
				}
			} else if (opening_state == 3) {
				if (all_done()) ++opening_state;
			} else execute_build(false, build);

			place_expos();

			multitasking::sleep(15);
		}
		

		resource_gathering::max_gas = 0.0;

	}

};


