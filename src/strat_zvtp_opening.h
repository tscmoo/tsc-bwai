
struct strat_zvtp_opening {

	a_vector<xy> test_positions;
	a_vector<a_deque<xy>> test_paths;

	void run() {


		get_upgrades::set_no_auto_upgrades(true);
		combat::no_aggressive_groups = true;
		combat::aggressive_zerglings = false;
		combat::no_scout_around = true;
		scouting::no_proxy_scout = true;
		scouting::scout_supply = 20;

// 		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);
// 		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_2, -1.0);
// 		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_3, -1.0);

		//get_upgrades::set_upgrade_value(upgrade_types::burrow, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::burrow, 100000.0);
		get_upgrades::set_upgrade_value(upgrade_types::ventral_sacs, 100000.0);
		get_upgrades::set_upgrade_value(upgrade_types::antennae, 100000.0);

// 		get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, -1.0);
// 		get_upgrades::set_upgrade_value(upgrade_types::grooved_spines, -1.0);
		
		using namespace buildpred;

		auto get_static_defence_pos = [&](unit_type*ut) {
			test_positions.clear();
			a_vector<std::tuple<xy, xy>> my_bases;
			for (auto&v : buildpred::get_my_current_state().bases) {
				my_bases.emplace_back(v.s->pos, square_pathing::get_nearest_node_pos(unit_types::siege_tank_tank_mode, v.s->cc_build_pos));
			}
			xy op_base = combat::op_closest_base;
			a_vector<a_deque<xy>> paths;
			for (auto&v : my_bases) {
				xy pos = std::get<1>(v);
				auto path = combat::find_bigstep_path(unit_types::siege_tank_tank_mode, pos, op_base, square_pathing::pathing_map_index::no_enemy_buildings);
				if (path.empty()) continue;
				path.push_front(pos);
				path.push_front(std::get<0>(v));
// 				double len = 0.0;
// 				xy lp;
// 				for (size_t i = 0; i < path.size(); ++i) {
// 					if (lp != xy()) {
// 						len += diag_distance(path[i] - lp);
// 						if (len >= 32 * 64) {
// 							path.resize(i);
// 							break;
// 						}
// 					}
// 					lp = path[i];
// 				}
				paths.push_back(std::move(path));
			}
			log("static defence (%s) %d paths:\n", ut->name, paths.size());
			for (auto&p : paths) {
				log(" - %d nodes\n", p.size());
			}
			if (!paths.empty()) {

				auto pred = [&](grid::build_square&bs) {
					if (!build::build_has_existing_creep(bs, ut)) return false;
					if (!build_spot_finder::can_build_at(ut, bs)) return false;
					return true;
				};
				auto score = [&](xy pos) {
					test_positions.push_back(pos);
					double r = 0.0;
					for (auto&path : paths) {
						bool in_range = false;
						double v = 1.0 / 64;
						//if (path.front() == my_start_location) v /= 32.0;
						double dist = 0.0;
						for (xy p : path) {
							double d = diag_distance(pos - p);
							if (d <= 32 * 7) {
								in_range = true;
								d = std::max(d, 32.0);
								dist += d*d;
							}
// 							d = std::max(d, 32.0);
// 							dist += d*d;
							v /= 2;
						}
						dist = std::sqrt(dist);
						r -= 1.0 / dist;
						if (in_range) r -= 1.0;
					}
					return r;
				};
				test_paths = paths;
				std::vector<xy> starts;
				for (unit*u : my_resource_depots) starts.push_back(u->pos);
				xy pos = build_spot_finder::find_best(starts, 256, pred, score);
				log("score(pos) is %g\n", score(pos));
				if (score(pos) > -2.0) {
					log("not good enough\n");
					return xy();
				}
				log("static defence (%s) build pos -> %d %d (%g away from my_closest_base)\n", ut->name, pos.x, pos.y, (pos - combat::my_closest_base).length());
				return pos;
			}
			return  xy();
		};

		auto place_static_defence = [&]() {
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					if (b.build_near != combat::my_closest_base) {

						build::unset_build_pos(&b);
						b.build_near = combat::my_closest_base;
						b.require_existing_creep = true;

						if (true) {
							xy pos = get_static_defence_pos(b.type->unit);
							if (pos != xy()) {
								build::set_build_pos(&b, pos);
							}
						}
					}
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
				score -= (has_gas ? res : res / 4) / 10 + ned;
				return score;
			}, std::numeric_limits<double>::infinity());
		};

		auto place_expos = [&]() {

			auto st = get_my_current_state();

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::hatchery) {
					xy pos = b.build_pos;
					build::unset_build_pos(&b);
				}
			}

			if (st.bases.size() >= 2) {
				if (my_resource_depots.size() <= st.bases.size()) return;
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
				int n = 8;
				auto s = get_next_base();
				//if (s) n = (int)s->resources.size();
				if (s) {
					n = 0;
					for (auto&v : s->resources) ++n;
				}
				if (free_mineral_patches >= n && long_distance_miners() == 0) return;
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
		if (!my_completed_units_of_type[unit_types::spawning_pool].empty()) opening_state = -1;
		int opening_substate = 0;
		bool has_sent_scouts = false;
		bool is_scouting = false;
		bool being_rushed = false;
		bool enemy_has_cloaked_units = false;
		int end_attack_count = 0;
		//int opening_safety_lings_state = 0;
		while (true) {

			int enemy_zealot_count = 0;
			int enemy_dragoon_count = 0;
			int enemy_forge_count = 0;
			int enemy_cannon_count = 0;
			int enemy_gateway_count = 0;
			double enemy_army_supply = 0.0;
			double enemy_air_army_supply = 0.0;
			double enemy_ground_army_supply = 0.0;
			double enemy_small_unit_army_supply = 0.0;
			int enemy_units_that_shoot_up_count = 0;
			int enemy_gas_count = 0;
			int enemy_barracks_count = 0;
			int enemy_bunker_count = 0;
			int enemy_proxy_building_count = 0;
			double enemy_attacking_army_supply = 0.0;
			int enemy_attacking_worker_count = 0;
			int enemy_vulture_count = 0;
			int enemy_worker_count = 0;
			int enemy_marine_count = 0;
			update_possible_start_locations();
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zealot) ++enemy_zealot_count;
				if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
				if (e->type == unit_types::forge) ++enemy_forge_count;
				if (e->type == unit_types::photon_cannon) ++enemy_cannon_count;
				if (e->type == unit_types::gateway) ++enemy_gateway_count;
				if (!e->type->is_worker) {
					enemy_army_supply += e->type->required_supply;
					if (e->is_flying) enemy_air_army_supply += e->type->required_supply;
					else enemy_ground_army_supply += e->type->required_supply;
					if (e->type->size == unit_type::size_small) enemy_small_unit_army_supply += e->type->required_supply;
				}
				if (e->stats->air_weapon) ++enemy_units_that_shoot_up_count;
				if (e->type->is_gas) ++enemy_gas_count;
				if (e->type == unit_types::barracks) ++enemy_barracks_count;
				if (e->type == unit_types::bunker) ++enemy_bunker_count;
				if (true) {
					double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
						return unit_pathing_distance(unit_types::scv, e->pos, pos);
					});
					if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base) < e_d) {
						log("%s is proxied\n", e->type->name);
						if (e->building) ++enemy_proxy_building_count;
						if (!e->type->is_worker) enemy_attacking_army_supply += e->type->required_supply;
						else ++enemy_attacking_worker_count;
					}
				}
				if (e->type == unit_types::vulture) ++enemy_vulture_count;
				if (e->type->is_worker) ++enemy_worker_count;
				if (e->type == unit_types::marine) ++enemy_marine_count;
			}
			if (enemy_cannon_count && !enemy_forge_count) enemy_forge_count = 1;
			if (enemy_zealot_count + enemy_dragoon_count && !enemy_gateway_count) enemy_gateway_count = 1;

			log("enemy_proxy_building_count is %d\n", enemy_proxy_building_count);

			int my_zergling_count = my_units_of_type[unit_types::zergling].size();
			int my_hydralisk_count = my_units_of_type[unit_types::hydralisk].size();

			overlord_scout(enemy_gas_count + enemy_units_that_shoot_up_count + enemy_barracks_count == 0);

// 			if (!my_units_of_type[unit_types::spore_colony].empty()) {
// 				xy pos = my_units_of_type[unit_types::spore_colony].front()->pos;
// 				for (auto*a : combat::live_combat_units) {
// 					if (a->u->type == unit_types::overlord) {
// 						a->strategy_busy_until = current_frame + 15 * 10;
// 						a->action = combat::combat_unit::action_offence;
// 						a->subaction = combat::combat_unit::subaction_move;
// 						a->target_pos = pos;
// 					}
// 				}
// 			}

			bool opponent_has_expanded = false;
			for (auto&s : resource_spots::spots) {
				bool is_start_location = false;
				for (xy pos : start_locations) {
					if (diag_distance(pos - s.cc_build_pos) <= 32 * 10) {
						is_start_location = true;
						break;
					}
				}
				if (is_start_location) continue;
				auto&bs = grid::get_build_square(s.cc_build_pos);
				if (bs.building && bs.building->owner == players::opponent_player) opponent_has_expanded = true;
			}

			if (enemy_army_supply == 0.0 && enemy_attacking_worker_count < (int)my_workers.size() / 4 && enemy_bunker_count + enemy_forge_count == 0) {
				if (!has_sent_scouts && my_completed_units_of_type[unit_types::zergling].size() >= 2) {
					int n = 0;
					for (unit*u : my_completed_units_of_type[unit_types::zergling]) {
						scouting::add_scout(u);
						if (n++ >= 4) break;
					}
					has_sent_scouts = true;
					is_scouting = true;
				}
			} else {
				if (is_scouting) {
					is_scouting = false;
					for (auto&v : scouting::all_scouts) {
						scouting::rm_scout(v.scout_unit);
					}
				}
			}
			if (being_rushed & !has_sent_scouts) {
				for (auto&v : scouting::all_scouts) {
					auto*a = &combat::combat_unit_map[v.scout_unit];
					a->strategy_busy_until = current_frame + 15 * 10;
					a->action = combat::combat_unit::action_offence;
					a->subaction = combat::combat_unit::subaction_move;
					a->target_pos = my_start_location;
					scouting::rm_scout(v.scout_unit);
				}
				scouting::scout_supply = 40;
			}

			auto eval_combat = [&](bool is_defence) {
				combat_eval::eval eval;
				eval.max_frames = 15 * 120;
				for (unit*u : my_units) {
					if (!u->is_completed) continue;
					if (!is_defence && u->building) continue;
					if (!u->stats->ground_weapon) continue;
					if (u->is_flying || u->type->is_worker || u->type->is_non_usable) continue;
					auto&c = eval.add_unit(u, 0);
					if (u->building) c.move = 0.0;
				}
				for (unit*u : enemy_units) {
					if (u->building || u->is_flying || u->type->is_worker || u->type->is_non_usable) continue;
					if (is_defence) {
						double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
							return unit_pathing_distance(unit_types::scv, u->pos, pos);
						});
						if (unit_pathing_distance(unit_types::scv, u->pos, combat::my_closest_base)*0.5 > e_d) {
							continue;
						}
					}
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
				return eval.teams[1].end_supply == 0;
			};
			bool ground_fight_ok = eval_combat(false);
			bool ground_defence_fight_ok = eval_combat(true);

			log("ground_fight_ok is %d\n", ground_fight_ok);
			log("ground_defence_fight_ok is %d\n", ground_defence_fight_ok);

			bool prev_being_rushed = being_rushed;
			if (enemy_attacking_army_supply >= 4.0 || enemy_attacking_worker_count >= (int)my_workers.size() / 2) {
				being_rushed = true;
			}
			if (enemy_attacking_army_supply > 0 && !ground_defence_fight_ok) {
				being_rushed = true;
			}
			//if ((enemy_gateway_count >= 2 && !my_completed_units_of_type[unit_types::spawning_pool].empty()) || enemy_proxy_building_count) {
			if (enemy_gateway_count >= 2) {
				being_rushed = true;
			}
			if (enemy_barracks_count >= 2 && my_completed_units_of_type[unit_types::spawning_pool].empty()) {
				being_rushed = true;
			}
			if (enemy_proxy_building_count == 0 && (enemy_cannon_count || enemy_bunker_count)) {
				being_rushed = false;
			}
			if (opponent_has_expanded || my_workers.size() >= 20) {
				being_rushed = false;
			}

			if (prev_being_rushed != being_rushed) {
				log("being_rushed is %d\n", being_rushed);
			}

			bool is_defending = false;
			for (auto&g : combat::groups) {
				if (!g.is_aggressive_group && !g.is_defensive_group) {
					if (g.ground_dpf >= 2.0) {
						is_defending = true;
						break;
					}
				}
			}

			if (being_rushed || !my_units_of_type[unit_types::lair].empty()) {
				if (my_zergling_count >= 12 && (!is_defending || my_zergling_count >= 20)) {
					get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
				}
				get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
			}

			if (my_hydralisk_count && (ground_fight_ok || my_workers.size() >= 32)) {
				get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, -1.0);
			} else {
				get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, 0.0);
			}

			if (ground_defence_fight_ok && my_workers.size() >= 22) {
				get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);
				if (my_workers.size() >= 30) {
					get_upgrades::set_upgrade_value(upgrade_types::zerg_melee_attacks_1, -1.0);
					get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_2, -1.0);
				}
			}

			resource_gathering::max_gas = 300.0;
			if (my_workers.size() < 34) resource_gathering::max_gas = 150.0;
			if (!my_units_of_type[unit_types::spire].empty()) resource_gathering::max_gas = 800.0;
			if (my_units_of_type[unit_types::spawning_pool].empty()) resource_gathering::max_gas = 1.0;
			else if (resource_gathering::max_gas < current_minerals) resource_gathering::max_gas = current_minerals;

			if (!is_attacking) {

				bool attack = false;
				if (attack_count == 0) attack = true;
				if (current_frame - last_attack_end >= 15 * 30) attack = true;
				if (!ground_fight_ok) attack = false;
				//if (being_rushed && current_used_total_supply < 24.0) attack = false;

				if (attack) begin_attack();

			} else {

				int lose_count = 0;
				int total_count = 0;
				for (auto*a : combat::live_combat_units) {
					if (a->u->type->is_worker) continue;
					if (!a->u->stats->ground_weapon && !a->u->stats->air_weapon) continue;
					++total_count;
					if (current_frame - a->last_fight <= 15 * 10) {
						//if (a->last_win_ratio < 1.0) ++lose_count;
						if (a->last_run >= a->last_fight) ++lose_count;
					}
				}
				if (lose_count >= total_count / 2 + total_count / 4 || !ground_fight_ok) {
				//if (!ground_fight_ok) {
					end_attack();
					//if (total_count >= 6) ++end_attack_count;
				}

			}

			//is_attacking = ground_fight_ok;

			combat::no_aggressive_groups = !is_attacking;

			log("is_attacking: %d  is_defending: %d\n", is_attacking, is_defending);

			log("enemy_army_supply: %g my army supply %g\n", enemy_army_supply, current_used_total_supply - my_workers.size());

// 			double my_total_army_supply_ever_made = 0.0;
// 			double my_total_worker_supply_ever_made = 0.0;
// 			for (unit*u : all_units_ever) {
// 				if (u->owner == players::my_player) {
// 					if (!u->type->is_worker) {
// 						my_total_army_supply_ever_made += u->type->required_supply;
// 					} else {
// 						my_total_worker_supply_ever_made += u->type->required_supply;
// 					}
// 				}
// 			}

			if (!enemy_has_cloaked_units) {
				for (unit*u : visible_enemy_units) {
					if (u->cloaked && u->type != unit_types::observer) {
						enemy_has_cloaked_units = true;
						get_upgrades::set_upgrade_value(upgrade_types::pneumatized_carapace, -1.0);
					}
				}
			}

			bool make_extra_overlord = false;
			for (unit*u : my_completed_units_of_type[unit_types::overlord]) {
				if (u->hp <= u->stats->hp*0.75) {
					make_extra_overlord = true;
					break;
				}
			}

			double spire_progress = 0.0;
			for (unit*u : my_units_of_type[unit_types::spire]) {
				double p = 1.0 - (double)u->remaining_build_time / u->type->build_time;
				if (p > spire_progress) spire_progress = p;
			}
			//bool spire_almost_done = !my_units_of_type[unit_types::spire].empty() && my_units_of_type[unit_types::spire].front()->remaining_build_time <= 15 * 30;
			//bool save_larva = spire_almost_done && my_units_of_type[unit_types::larva].size() < std::min(current_minerals / 100, current_gas / 100) + 1;

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					build::unset_build_pos(&b);
				}
			}

			bool valid_static_defence_pos = get_static_defence_pos(unit_types::sunken_colony) != xy();

// 			if (opening_safety_lings_state == 0 && my_workers.size() == 13) {
// 				if (my_units_of_type[unit_types::larva].size() >= 3 || being_rushed || enemy_army_supply >= 4.0) {
// 					if (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0) opening_safety_lings_state = 1;
// 					else opening_safety_lings_state = 2;
// 				}
// 			}
			if (is_defending && !ground_defence_fight_ok && my_workers.size() < 18) {
				int n = my_workers.size() / 2;
				//int n = my_workers.size() - 3;
				for (unit*u : my_workers) {
					if (u->force_combat_unit) {
						if (u->hp < u->stats->hp / 2) u->force_combat_unit = false;
						else {
							--n;
							if (n < 0) u->force_combat_unit = false;
						}
					}
				}
				while (n > 0) {
					unit*u = get_best_score(my_workers, [&](unit*u) {
						if (u->force_combat_unit) return 0.0;
						return -u->hp;
					});
					if (!u) break;
					u->force_combat_unit = true;
					--n;
				}
			} else {
				for (unit*u : my_workers) {
					if (u->force_combat_unit) u->force_combat_unit = false;
				}
			}

			auto build = [&](state&st) {
// 				log("build: frame %d - ", st.frame);
// 				for (auto&v : st.units) log("%dx%s ", v.second.size(), v.first->name);
// 				log("\n");
// 				log("  production - ");
// 				for (auto&v : st.production) log("%d %s  ", v.first, v.second->name);
// 				log("\n");

				//if (save_larva) return false;

				int larva_count = 0;
				for (int i = 0; i < 3; ++i) {
					for (st_unit&u : st.units[i == 0 ? unit_types::hive : i == 1 ? unit_types::lair : unit_types::hatchery]) {
						larva_count += u.larva_count;
					}
				}

				int drone_count = count_units_plus_production(st, unit_types::drone);
				int hatch_count = count_units_plus_production(st, unit_types::hatchery) + count_units_plus_production(st, unit_types::lair) + count_units_plus_production(st, unit_types::hive);
				int completed_hatch_count = st.units[unit_types::hatchery].size() + st.units[unit_types::lair].size() + st.units[unit_types::hive].size();
				int zergling_count = count_units_plus_production(st, unit_types::zergling);
				int hydralisk_count = count_units_plus_production(st, unit_types::hydralisk);
				int mutalisk_count = count_units_plus_production(st, unit_types::mutalisk);
				int scourge_count = count_units_plus_production(st, unit_types::scourge);

 				double army_supply = st.used_supply[race_zerg] - drone_count;

				st.dont_build_refineries = drone_count < 32;

				std::function<bool(state&)> army = [&](state&st) {
					return false;
				};

				bool drone_up = (!being_rushed && (opponent_has_expanded || enemy_cannon_count + enemy_bunker_count)) || enemy_worker_count > drone_count;

				//bool build_army = is_defending && army_supply < enemy_army_supply*1.5;
				bool build_army = is_defending;
				if (being_rushed && drone_count >= 13 && army_supply < 8) build_army = true;
				if (being_rushed && army_supply < enemy_army_supply + 4) build_army = true;
// 				if (being_rushed && drone_count >= 16 && spire_progress == 0.0) {
// 					if (army_supply < enemy_army_supply + 8) build_army = true;
// 					if (army_supply < drone_count / 2) build_army = true;
// 					if (army_supply < 8) build_army = true;
// 					if (!ground_defence_fight_ok) build_army = true;
// 				}
// 				if (drone_count >= 13 && (!opponent_has_expanded || enemy_cannon_count + enemy_bunker_count)) {
// 					if (army_supply < drone_count - 8) build_army = true;
// 				}

// 				if (drone_count >= 30 && spire_progress >= 0.5) {
// 					if (army_supply < drone_count + drone_count / 2) build_army = true;
// 				}

// 				if (being_rushed && drone_count >= 13 && army_supply < drone_count) {
// 					if (is_defending || count_production(st, unit_types::drone)) build_army = true;
// 				}
				bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);
				if (drone_count >= 18 && maybe_being_rushed) {
					int drone_prod = 1;
					if (count_units_plus_production(st, unit_types::sunken_colony) >= 2) drone_prod = 2;
					if (!ground_defence_fight_ok) drone_prod = 0;
					if (is_defending || count_production(st, unit_types::drone) >= drone_prod) build_army = true;
				}

				army = [army](state&st) {
					return nodelay(st, unit_types::hatchery, army);
				};

				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};

// 				if (drone_count >= 13 && !maybe_being_rushed) {
// 					if (hatch_count < 3) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::hatchery, army);
// 						};
// 					}
// 				}

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

// 				if (!build_army && drone_count >= 18) {
// 					if (hatch_count < 4) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::hatchery, army);
// 						};
// 					}
// 				}

				if ((!is_defending && !being_rushed) || ground_defence_fight_ok) {
					//if ((hatch_count >= 3 || drone_count >= 15) && count_units_plus_production(st, unit_types::extractor) == 0) {
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
				}

				//if (completed_hatch_count >= 2) {
				//if (my_resource_depots.size() >= 2) {
				if (valid_static_defence_pos) {
					//if ((!build_army && drone_count >= 24) || (maybe_being_rushed && drone_count >= 18) || (being_rushed && enemy_marine_count)) {
					if ((!build_army && drone_count >= 24) || (maybe_being_rushed && drone_count >= 18)) {
						int max_sunkens = 1;
						if (drone_count >= 30) max_sunkens = 3;
						if (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0) {
							max_sunkens = 6;
						}
						if (max_sunkens > (drone_count - 16) / 2) max_sunkens = (drone_count - 16) / 2;
						if (max_sunkens < 1) max_sunkens = 1;
						if (count_units_plus_production(st, unit_types::sunken_colony) < max_sunkens) {
							army = [army](state&st) {
								return nodelay(st, unit_types::sunken_colony, army);
							};
						}
					}
// 					if (!ground_defence_fight_ok) {
// 						if (count_units_plus_production(st, unit_types::sunken_colony) < (being_rushed ? 2 : 1)) {
// 							army = [army](state&st) {
// 								return nodelay(st, unit_types::sunken_colony, army);
// 							};
// 						}
// 					}
				}

// 				if (drone_count == 13 && zergling_count < 8) {
// 					if (opening_safety_lings_state == 0) return false;
// 					if (opening_safety_lings_state == 1) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::zergling, army);
// 						};
// 					}
// 				}

				if (maybe_being_rushed) {
					if (drone_count >= 11 && zergling_count < 8) {
						army = [army](state&st) {
							return nodelay(st, unit_types::zergling, army);
						};
					}
					if (ground_defence_fight_ok && count_production(st, unit_types::drone) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::drone, army);
						};
					}
				}
// 				if (!ground_defence_fight_ok) {
// 					if (count_production(st, unit_types::creep_colony) + count_production(st, unit_types::sunken_colony) == 0) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::sunken_colony, army);
// 						};
// 					}
// 				} else {
// 					if (being_rushed) {
// 						if (drone_count < 18) {
// 							army = [army](state&st) {
// 								return nodelay(st, unit_types::drone, army);
// 							};
// 						}
// 					}
// 				}

// 				if (drone_count == 13 && zergling_count < 8) {
// 					if (being_rushed || build_army || enemy_army_supply >= 4.0) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::zergling, army);
// 						};
// 					} else if (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0) {
// 						if (larva_count < 3) {
// 							return false;
// 						} else {
// 							army = [army](state&st) {
// 								return nodelay(st, unit_types::zergling, army);
// 							};
// 						}
// 					}
// 				}

				if (make_extra_overlord && st.max_supply[race_zerg] + count_production(st, unit_types::overlord) * 8 - 16 <= st.used_supply[race_zerg]) {
					army = [army](state&st) {
						return nodelay(st, unit_types::overlord, army);
					};
				}

				return army(st);

			};

 			//if (!my_completed_units_of_type[unit_types::spire].empty()) break;
			if (current_used_total_supply >= 60) break;
			if (should_transition()) break;

			auto all_done = [&]() {
// 				if (being_rushed) {
// 					opening_state = -1;
// 					for (auto&b : build::build_order) {
// 						if (!b.built_unit && b.build_frame > current_frame + latency_frames + 8) b.dead = true;
// 					}
// 					return false;
// 				}
				for (unit*u : all_units_ever) {
					if (u->dead && u->owner == players::my_player && u->type == unit_types::overlord) {
						opening_state = -1;
						for (auto&b : build::build_order) {
							if (!b.built_unit && b.build_frame > current_frame + latency_frames + 8) b.dead = true;
						}
						return false;
					}
				}
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

			if (players::opponent_player->race == race_protoss) {
				if (opening_state == 0) {
					build::add_build_task(0.0, unit_types::drone);
					build::add_build_task(0.0, unit_types::drone);
					build::add_build_task(0.0, unit_types::drone);
					build::add_build_task(0.0, unit_types::drone);
					build::add_build_task(0.0, unit_types::drone);
					build::add_build_task(1.0, unit_types::overlord);
					build::add_build_task(2.0, unit_types::drone);
					build::add_build_task(2.0, unit_types::drone);
					build::add_build_task(2.0, unit_types::drone);
					build::add_build_task(3.0, unit_types::hatchery);
					build::add_build_task(3.0, unit_types::spawning_pool);
					build::add_build_task(4.0, unit_types::drone);
					build::add_build_task(4.0, unit_types::drone);
					build::add_build_task(4.0, unit_types::drone);
					build::add_build_task(4.0, unit_types::drone);
					build::add_build_task(5.0, unit_types::zergling);
					build::add_build_task(5.0, unit_types::zergling);
					build::add_build_task(5.0, unit_types::zergling);
					build::add_build_task(5.0, unit_types::zergling);
					++opening_state;
				} else if (opening_state == 1) {
					if (all_done()) {
						++opening_state;
					}
				} else execute_build(false, build);
			} else {
				if (opening_state == 0) {
					build::add_build_task(0.0, unit_types::drone);
					build::add_build_task(0.0, unit_types::drone);
					build::add_build_task(0.0, unit_types::drone);
					build::add_build_task(0.0, unit_types::drone);
					build::add_build_task(0.0, unit_types::drone);
					++opening_state;
				} else if (opening_state == 1) {
					gas_trick();
				} else if (opening_state == 2) {
					if (all_done()) {
						build::add_build_task(0.0, unit_types::hatchery);
						build::add_build_task(1.0, unit_types::spawning_pool);
						build::add_build_task(2.0, unit_types::drone);
						build::add_build_task(3.0, unit_types::overlord);
						++opening_state;
					}
				} else if (opening_state == 3) {
					gas_trick();
				} else if (opening_state == 4) {
					if (all_done()) {
						build::add_build_task(0.0, unit_types::drone);
						//build::add_build_task(0.0, unit_types::drone);
						//build::add_build_task(0.0, unit_types::drone);
						++opening_state;
					}
				} else if (opening_state == 5) {
					if (all_done()) {
						if (being_rushed || my_units_of_type[unit_types::larva].size() >= 3) {
							bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);
							if (being_rushed || maybe_being_rushed) {
								build::add_build_task(1.0, unit_types::zergling);
								build::add_build_task(1.0, unit_types::zergling);
								build::add_build_task(1.0, unit_types::zergling);
								build::add_build_task(1.0, unit_types::zergling);
							}
							++opening_state;
						}
					}
				} else if (opening_state == 6) {
					if (all_done()) {
						++opening_state;
					}
				} else execute_build(false, build);
			}

// 			if (opening_state == 0) {
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				++opening_state;
// 			} else if (opening_state == 1) {
// 				gas_trick();
// 			} else if (opening_state == 2) {
// 				//scouting::scout_supply = 9.0;
// 				build::add_build_task(0.0, unit_types::hatchery);
// 				build::add_build_task(1.0, unit_types::spawning_pool);
// 				build::add_build_task(2.0, unit_types::drone);
// 				build::add_build_task(3.0, unit_types::overlord);
// // 				build::add_build_task(1.0, unit_types::overlord);
// // 				build::add_build_task(2.0, unit_types::spawning_pool);
// 				++opening_state;
// 			} else if (opening_state == 3) {
// 				//gas_trick();
// 				if (all_done()) {
// 					scouting::scout_supply = 10.0;
// 					build::add_build_task(3.0, unit_types::drone);
// 					build::add_build_task(4.0, unit_types::drone);
// 					build::add_build_task(5.0, unit_types::drone);
// 					++opening_state;
// 				}
// 			} else if (opening_state == 4) {
// 				if (all_done()) {
// // 					build::add_build_task(0.0, unit_types::drone);
// // 					build::add_build_task(0.0, unit_types::drone);
// 					++opening_state;
// 				}
// 			} else if (opening_state == 5) {
// 				if (all_done()) {
// // 					bool skip_lings = !being_rushed && enemy_bunker_count + enemy_cannon_count;
// // 					if (my_units_of_type[unit_types::larva].size() >= 3 || skip_lings) {
// // 						if (!skip_lings) {
// // 							build::add_build_task(1.0, unit_types::zergling);
// // 							build::add_build_task(1.0, unit_types::zergling);
// // 							build::add_build_task(1.0, unit_types::zergling);
// // 						}
// // 						++opening_state;
// // 					}
// 					build::add_build_task(1.0, unit_types::zergling);
// 					build::add_build_task(1.0, unit_types::zergling);
// 					++opening_state;
// 				}
// 			} else if (opening_state == 6) {
// 				if (all_done()) {
// 					//if (!being_rushed) build::add_build_task(3.0, unit_types::hatchery);
// 					++opening_state;
// 				}
// 			} else execute_build(false, build);

// 			if (opening_state == 0) {
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(1.0, unit_types::spawning_pool);
// 				build::add_build_task(2.0, unit_types::drone);
// 				++opening_state;
// 			} else if (opening_state == 1) {
// 				gas_trick();
// 			} else if (opening_state == 2) {
// 				if (all_done()) {
// 					build::add_build_task(1.0, unit_types::overlord);
// 					build::add_build_task(2.0, unit_types::zergling);
// 					build::add_build_task(2.0, unit_types::zergling);
// 					build::add_build_task(2.0, unit_types::zergling);
// 					build::add_build_task(3.0, unit_types::hatchery);
// 					++opening_state;
// 				}
// 			} else if (opening_state == 3) {
// 				if (all_done()) {
// 					++opening_state;
// 				}
// 			} else execute_build(false, build);

			place_static_defence();
			place_expos();

			multitasking::sleep(15);
		}
		

		resource_gathering::max_gas = 0.0;

	}

	void render() {

		for (xy pos : possible_start_locations) {
			game->drawCircleMap(pos.x, pos.y, 300, BWAPI::Colors::Green);
		}

		for (xy pos : test_positions) {
			game->drawBoxMap(pos.x, pos.y, pos.x + 32 * 2, pos.y + 32 * 2, BWAPI::Colors::White);
		}

		xy screen_pos = bwapi_pos(game->getScreenPosition());
		for (int y = screen_pos.y; y < screen_pos.y + 400; y += 32) {
			for (int x = screen_pos.x; x < screen_pos.x + 640; x += 32) {
				if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;

				x &= -32;
				y &= -32;

				auto&bs = grid::get_build_square(xy(x, y));
				if (bs.mineral_reserved) {
					game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Color(128, 128, 0));
				}
				if (bs.reserved_for_resource_depot) {
					game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Color(255, 0, 0));
				}
				if (bs.blocked_by_larva_or_egg) {
					game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Color(0, 255, 0));
				}
				if (bs.blocked_until >= current_frame) {
					game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Color(0, 0, 255));
				}

			}
		}

		for (auto&v : test_paths) {
			xy lp;
			for (xy pos : v) {
				if (lp != xy()) game->drawLineMap(lp.x, lp.y, pos.x, pos.y, BWAPI::Colors::Cyan);
				game->drawCircleMap(pos.x, pos.y, 32 * 2, BWAPI::Colors::Cyan);
				lp = pos;
			}
		}

		game->drawTextScreen(300, 8, "drones: %d", my_workers.size());
		
	}

};


