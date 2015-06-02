
struct strat_p_opening {

	a_vector<xy> test_positions;
	a_vector<a_deque<xy>> test_paths;

	void run() {


		get_upgrades::set_no_auto_upgrades(true);
		combat::no_aggressive_groups = true;
		combat::no_scout_around = true;
		scouting::scout_supply = 9;
		
		using namespace buildpred;

		auto get_static_defence_pos = [&](unit_type*ut) {
			test_positions.clear();
			a_vector<xy> my_bases;
			for (auto&v : buildpred::get_my_current_state().bases) {
				my_bases.push_back(square_pathing::get_nearest_node_pos(unit_types::siege_tank_tank_mode, v.s->cc_build_pos));
			}
			xy op_base = combat::op_closest_base;
			a_vector<a_deque<xy>> paths;
			for (xy pos : my_bases) {
				auto path = combat::find_bigstep_path(unit_types::siege_tank_tank_mode, pos, op_base, square_pathing::pathing_map_index::no_enemy_buildings);
				if (path.empty()) continue;
				path.push_front(pos);
				double len = 0.0;
				xy lp;
				for (size_t i = 0; i < path.size(); ++i) {
					if (lp != xy()) {
						len += diag_distance(path[i] - lp);
						if (len >= 32 * 64) {
							path.resize(i);
							break;
						}
					}
					lp = path[i];
				}
				paths.push_back(std::move(path));
			}
			log("static defence (%s) %d paths:\n", ut->name, paths.size());
			for (auto&p : paths) {
				log(" - %d nodes\n", p.size());
			}
			if (!paths.empty()) {
				std::array<xy, 1> starts;
				//starts[0] = combat::defence_choke.center;
				starts[0] = combat::my_closest_base;

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
								//r -= v / std::max(d, 32.0);
								in_range = true;
							}
							//r -= v / (std::max(d, 32.0) * std::max(d, 32.0));
							d = std::max(d, 32.0);
							dist += d*d;
							v /= 2;
						}
						dist = std::sqrt(dist);
						r -= 1.0 / dist;
						if (in_range) r -= 1.0;
					}
					return r;
				};
				test_paths = paths;
				xy pos = build_spot_finder::find_best(starts, 256, pred, score);
				log("score(pos) is %g\n", score(pos));
				if (my_units_of_type[unit_types::sunken_colony].empty()) {
					if (score(pos) > -2.0 && diag_distance(pos - combat::my_closest_base) > 32 * 10) {
						log("not good enough\n");
						pos = xy();
					}
				}
// 				if (score(pos) > -2.0) {
// 					log("not good enough\n");
// 					pos = xy();
// 				}
				log("static defence (%s) build pos -> %d %d (%g away from my_closest_base)\n", ut->name, pos.x, pos.y, (pos - combat::my_closest_base).length());
				return pos;
			}
			return  xy();
		};

		auto place_static_defence = [&]() {
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					//if (b.build_near != combat::defence_choke.center) {
					if (b.build_near != combat::my_closest_base) {

						build::unset_build_pos(&b);
						//b.build_near = combat::defence_choke.center;
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
				if (b.type->unit && b.type->unit->is_resource_depot) {
					build::unset_build_pos(&b);
				}
			}

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit && b.type->unit->is_resource_depot) {
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
		bool maxed_out_aggression = false;
		while (true) {

			int enemy_zealot_count = 0;
			int enemy_dragoon_count = 0;
			int enemy_forge_count = 0;
			int enemy_cannon_count = 0;
			int enemy_gateway_count = 0;
			double enemy_army_supply = 0.0;
			double enemy_air_army_supply = 0.0;
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
			}
			if (enemy_cannon_count && !enemy_forge_count) enemy_forge_count = 1;
			if (enemy_zealot_count + enemy_dragoon_count && !enemy_gateway_count) enemy_gateway_count = 1;

			log("enemy_proxy_building_count is %d\n", enemy_proxy_building_count);

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
				return eval.teams[1].end_supply == 0;
			};

			bool fight_ok = eval_combat(false);

			resource_gathering::max_gas = 300.0;
			if (my_workers.size() < 34) resource_gathering::max_gas = 150.0;

			if (!is_attacking) {

				bool attack = false;
				if (attack_count == 0) attack = true;
				if (current_frame - last_attack_end >= 15 * 60 * 2) attack = true;
				if (fight_ok && current_frame - last_attack_end >= 15 * 30) attack = true;
				if (!fight_ok) attack = false;

				if (attack) begin_attack();

			} else {

				if (!fight_ok) {
					end_attack();
				}

			}

			//combat::no_aggressive_groups = !is_attacking;
			combat::no_aggressive_groups = current_used_total_supply - my_workers.size() < 90.0;
			combat::aggressive_groups_done_only = true;

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				combat::aggressive_groups_done_only = false;
				if (current_used_total_supply < 150.0) maxed_out_aggression = false;
			}
			if (current_used_total_supply - my_workers.size() >= enemy_army_supply * 2) {
				combat::aggressive_groups_done_only = false;
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

			if (current_used_total_supply >= 100.0) {
				get_upgrades::set_no_auto_upgrades(false);
			}
			if (current_used_total_supply >= 70.0) {
				get_upgrades::set_upgrade_value(upgrade_types::singularity_charge, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::leg_enhancements, -1.0);
			}

			log("is_attacking: %d  is_defending: %d\n", is_attacking, is_defending);

			log("enemy_army_supply: %g my army supply %g\n", enemy_army_supply, current_used_total_supply - my_workers.size());

			bool force_expand = long_distance_miners() >= 1 && get_next_base();

			auto build = [&](state&st) {


				int probe_count = count_units_plus_production(st, unit_types::probe);
				int nexus_count = count_units_plus_production(st, unit_types::nexus);
				int zealot_count = count_units_plus_production(st, unit_types::zealot);
				int dragoon_count = count_units_plus_production(st, unit_types::dragoon);

 				double army_supply = st.used_supply[race_protoss] - probe_count;

				st.dont_build_refineries = false;

				std::function<bool(state&)> army = [&](state&st) {
					return false;
				};

				army = [army](state&st) {
					return maxprod(st, unit_types::zealot, army);
				};

				army = [army](state&st) {
					return maxprod(st, unit_types::dragoon, army);
				};

// 				if (zealot_count < dragoon_count * 2) {
// 					army = [army](state&st) {
// 						return maxprod(st, unit_types::zealot, army);
// 					};
// 				}

				if (st.used_supply[race_protoss] >= 21) {
					if (nexus_count < 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::nexus, army);
						};
					}
				}

				if (fight_ok) {
					if (army_supply >= 8.0) {
						if (count_units_plus_production(st, unit_types::robotics_facility) == 0) {
							army = [army](state&st) {
								return nodelay(st, unit_types::robotics_facility, army);
							};
						}
					}
				}

				if (army_supply >= 20.0) {
					if (count_units_plus_production(st, unit_types::observer) < (int)(army_supply / 20.0) + 1) {
						army = [army](state&st) {
							return nodelay(st, unit_types::observer, army);
						};
					}
				}

				if (probe_count < 20 || army_supply > probe_count) {
					army = [army](state&st) {
						return nodelay(st, unit_types::probe, army);
					};
				}

				if (force_expand && count_production(st, unit_types::nexus) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::nexus, army);
					};
				}

				return army(st);

			};

			auto all_done = [&]() {
				for (auto&b : build::build_order) {
					if (!b.built_unit) return false;
				}
				return true;
			};

			if (opening_state == 0) {
				if (false) {
					build::add_build_task(0.0, unit_types::probe);
					build::add_build_task(0.0, unit_types::probe);
					build::add_build_task(0.0, unit_types::probe);
					build::add_build_task(0.0, unit_types::probe);
					build::add_build_task(1.0, unit_types::pylon);
					build::add_build_task(2.0, unit_types::probe);
					build::add_build_task(2.0, unit_types::probe);
					build::add_build_task(3.0, unit_types::gateway);
					build::add_build_task(4.0, unit_types::probe);
					build::add_build_task(4.0, unit_types::probe);
					build::add_build_task(5.0, unit_types::assimilator);
					build::add_build_task(6.0, unit_types::probe);
					build::add_build_task(7.0, unit_types::cybernetics_core);
				} else {
					build::add_build_task(0.0, unit_types::probe);
					build::add_build_task(0.0, unit_types::probe);
					build::add_build_task(0.0, unit_types::probe);
					build::add_build_task(0.0, unit_types::probe);
					build::add_build_task(1.0, unit_types::pylon);
					build::add_build_task(2.0, unit_types::probe);
					build::add_build_task(2.0, unit_types::probe);
					build::add_build_task(2.0, unit_types::probe);
					build::add_build_task(2.0, unit_types::probe);
					build::add_build_task(3.0, unit_types::nexus);
					build::add_build_task(4.0, unit_types::gateway);
					build::add_build_task(5.0, unit_types::probe);
					build::add_build_task(5.0, unit_types::probe);
					build::add_build_task(6.0, unit_types::gateway);
					build::add_build_task(7.0, unit_types::zealot);
				}
				++opening_state;
			} else if (opening_state==1) {
				if (all_done()) ++opening_state;
			} else execute_build(false, build);

			place_static_defence();
			place_expos();

			multitasking::sleep(15);
		}
		

		resource_gathering::max_gas = 0.0;

	}

};


