
struct strat_zvtp_opening {

	void run() {

		get_upgrades::set_no_auto_upgrades(true);
		combat::no_aggressive_groups = true;
		combat::aggressive_zerglings = false;
		combat::no_scout_around = true;
		scouting::no_proxy_scout = true;

		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_2, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_3, -1.0);

		using namespace buildpred;

		auto place_static_defence = [&]() {
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					if (b.build_near != combat::defence_choke.center) {

						build::unset_build_pos(&b);
						b.build_near = combat::defence_choke.center;
						b.require_existing_creep = true;

						if (true) {
							a_vector<xy> my_bases;
							for (auto&v : buildpred::get_my_current_state().bases) {
								my_bases.push_back(square_pathing::get_nearest_node_pos(unit_types::siege_tank_tank_mode, v.s->cc_build_pos));
							}
							xy op_base = combat::op_closest_base;
							a_vector<a_deque<xy>> paths;
							for (xy pos : my_bases) {
								auto path = combat::find_bigstep_path(unit_types::siege_tank_tank_mode, op_base, pos, square_pathing::pathing_map_index::no_enemy_buildings);
								if (path.empty()) continue;
								double len = 0.0;
								xy lp;
								for (size_t i = 0; i < path.size(); ++i) {
									if (lp != xy()) {
										len += diag_distance(path[i] - lp);
										if (len >= 32 * 30) {
											path.resize(i);
											break;
										}
									}
									lp = path[i];
								}
								paths.push_back(std::move(path));
							}
							if (!paths.empty()) {
								std::array<xy, 1> starts;
								starts[0] = combat::defence_choke.center;

								auto pred = [&](grid::build_square&bs) {
									if (!build::build_has_existing_creep(bs, b.type->unit)) return false;
									if (!build_spot_finder::can_build_at(b.type->unit, bs)) return false;
									return true;
								};
								auto score = [&](xy pos) {
									double r = 0.0;
									for (auto&path : paths) {
										double v = 1.0;
										for (xy p : path) {
											double d = diag_distance(pos - p);
											if (d <= 32 * 7) r -= v / std::max(d, 32.0);
											v += 1.0;
										}
									}
									return r;
								};
								xy pos = build_spot_finder::find_best(starts, 128, pred, score);
								if (pos != xy()) {
									build::set_build_pos(&b, pos);
								}
							}
						}

// 						unit*build_near_unit = get_best_score(my_buildings, [&](unit*u) {
// 							return unit_pathing_distance(unit_types::zergling, u->pos, combat::op_closest_base);
// 						});
// 						if (build_near_unit) {
// 							build::unset_build_pos(&b);
// 							std::array<xy, 1> starts;
// 							starts[0] = build_near_unit->pos;
// 
// 							auto pred = [&](grid::build_square&bs) {
// 								if (!build_spot_finder::can_build_at(b.type->unit, bs)) return false;
// 								return true;
// 							};
// 							auto score = [&](xy pos) {
// 								return units_distance(pos, b.type->unit, build_near_unit->building->build_pos, build_near_unit->type);
// 							};
// 							xy pos = build_spot_finder::find_best(starts, 128, pred, score);
// 							if (pos != xy()) {
// 								build::set_build_pos(&b, pos);
// 							}
// 						}
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

			if (my_workers.size() < 24 && st.bases.size() >= 2) return;
			//if (st.bases.size() >= 2) return;

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
		int opening_substate = 0;
		bool has_sent_scouts = false;
		bool being_rushed = false;
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
					if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base)*0.5 < e_d) {
						if (e->building) ++enemy_proxy_building_count;
						if (!e->type->is_worker) enemy_attacking_army_supply += e->type->required_supply;
					}
				}
			}
			if (enemy_cannon_count && !enemy_forge_count) enemy_forge_count = 1;
			if (enemy_zealot_count + enemy_dragoon_count && !enemy_gateway_count) enemy_gateway_count = 1;

			int my_zergling_count = my_units_of_type[unit_types::zergling].size();

			if (enemy_gas_count + enemy_units_that_shoot_up_count + enemy_barracks_count == 0) overlord_scout();
			else {
				for (auto*a : combat::live_combat_units) {
					if (a->u->type == unit_types::overlord && a->subaction == combat::combat_unit::subaction_idle) {
						a->strategy_busy_until = current_frame + 15 * 10;
						a->action = combat::combat_unit::action_offence;
						a->subaction = combat::combat_unit::subaction_move;
						a->target_pos = my_start_location;
					}
				}
			}

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


			scouting::scout_supply = 20;

			if (enemy_army_supply == 0.0 && enemy_bunker_count + enemy_forge_count == 0) {
				if (!has_sent_scouts && my_completed_units_of_type[unit_types::zergling].size() >= 3) {
					for (unit*u : my_completed_units_of_type[unit_types::zergling]) {
						scouting::add_scout(u);
					}
					has_sent_scouts = true;
				}
			} else {
				if (!scouting::all_scouts.empty()) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
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
				return eval.teams[0].end_supply > eval.teams[1].end_supply;
			};
			bool ground_fight_ok = eval_combat(false);
			bool ground_defence_fight_ok = eval_combat(true);

			log("ground_fight_ok is %d\n", ground_fight_ok);
			log("ground_defence_fight_ok is %d\n", ground_defence_fight_ok);

			if (enemy_army_supply >= 5.0 || enemy_gateway_count >= 2 || enemy_proxy_building_count) {
				being_rushed = true;
			}

			if (my_zergling_count >= 10) {
				get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
			}

			resource_gathering::max_gas = 300.0;
			if (!my_units_of_type[unit_types::spire].empty()) resource_gathering::max_gas = 400.0;

			combat::no_aggressive_groups = !ground_fight_ok;

			log("enemy_army_supply: %g my army supply %g\n", enemy_army_supply, current_used_total_supply - my_workers.size());

			bool spire_almost_done = !my_units_of_type[unit_types::spire].empty() && my_units_of_type[unit_types::spire].front()->remaining_build_time <= 15 * 30;
			bool save_larva = spire_almost_done && my_units_of_type[unit_types::larva].size() < std::min(current_minerals / 100, current_gas / 100) + 1;

			auto build = [&](state&st) {

				if (save_larva) return false;

				int larva_count = 0;
				for (int i = 0; i < 3; ++i) {
					for (st_unit&u : st.units[i == 0 ? unit_types::hive : i == 1 ? unit_types::lair : unit_types::hatchery]) {
						larva_count += u.larva_count;
					}
				}

				int drone_count = count_units_plus_production(st, unit_types::drone);
				int hatch_count = count_units_plus_production(st, unit_types::hatchery) + count_units_plus_production(st, unit_types::lair) + count_units_plus_production(st, unit_types::hive);
				int zergling_count = count_units_plus_production(st, unit_types::zergling);
				int hydralisk_count = count_units_plus_production(st, unit_types::hydralisk);

				double army_supply = st.used_supply[race_zerg] - drone_count;
				if (!opponent_has_expanded && enemy_forge_count == 0 && enemy_bunker_count == 0 && st.used_supply[race_zerg] < 18) {
					if (army_supply > enemy_army_supply) {
						if (larva_count <= 2) return false;
					}
				}

				st.dont_build_refineries = drone_count < 24;

				std::function<bool(state&)> army = [&](state&st) {
					return false;
				};

				army = [army](state&st) {
					return nodelay(st, unit_types::hatchery, army);
				};

				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};

				if (hatch_count < drone_count / 5) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}

				if ((!opponent_has_expanded && !enemy_forge_count) && my_completed_units_of_type[unit_types::hatchery].size() >= 2) {
					if (count_units_plus_production(st, unit_types::sunken_colony) < 1) {
						army = [army](state&st) {
							return depbuild(st, state(st), unit_types::sunken_colony);
						};
					}
				}

				if (st.used_supply[race_zerg] >= 30 && st.max_supply[race_zerg] - st.used_supply[race_zerg] < 16) {
					army = [army](state&st) {
						return nodelay(st, unit_types::overlord, army);
					};
				}

				//if (army_supply < enemy_army_supply) {
				if (!ground_defence_fight_ok) {
					army = [army](state&st) {
						return depbuild(st, state(st), unit_types::zergling);
					};
					if (!my_completed_units_of_type[unit_types::hydralisk_den].empty()) {
						army = [army](state&st) {
							return depbuild(st, state(st), unit_types::hydralisk);
						};
					}
					if (zergling_count*0.5 < enemy_small_unit_army_supply) {
						army = [army](state&st) {
							return depbuild(st, state(st), unit_types::zergling);
						};
					}
				}
				if (enemy_air_army_supply > hydralisk_count) {
					army = [army](state&st) {
						return depbuild(st, state(st), unit_types::hydralisk);
					};
				}

				if (drone_count >= (army_supply > enemy_army_supply ? 14 : 24)) {
					if (count_units_plus_production(st, unit_types::evolution_chamber) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::evolution_chamber, army);
						};
					}
					if (count_units_plus_production(st, unit_types::spire) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::spire, army);
						};
					}

				}

				if (army_supply >= enemy_army_supply) {
					if (count_production(st, unit_types::drone) < 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::drone, army);
						};
					}
				}
				if (being_rushed && zergling_count < 6) {
					army = [army](state&st) {
						return nodelay(st, unit_types::zergling, army);
					};
				}

				if (drone_count >= 13 && count_units_plus_production(st, unit_types::extractor) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::extractor, army);
					};
				}

				if (enemy_attacking_army_supply && my_completed_units_of_type[unit_types::hatchery].size() >= 2) {
					if (count_units_plus_production(st, unit_types::sunken_colony) < (enemy_army_supply >= army_supply*1.5 ? 2 : 1)) {
						army = [army](state&st) {
							return depbuild(st, state(st), unit_types::sunken_colony);
						};
					}
				}

				if (army_supply >= 8) {
					if (count_production(st, unit_types::drone) == 0) {
						army = [army](state&st) {
							return depbuild(st, state(st), unit_types::drone);
						};
					}
				}

				return army(st);
			};

 			if (!my_completed_units_of_type[unit_types::spire].empty()) break;

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
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				++opening_state;
			} else if (opening_state==1) {
				gas_trick();
			} else if (opening_state == 2) {
				build::add_build_task(0.0, unit_types::hatchery);
				build::add_build_task(1.0, unit_types::spawning_pool);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(3.0, unit_types::overlord);
				++opening_state;
			} else if (opening_state == 3) {
				gas_trick();
			} else if (opening_state == 4) {
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(1.0, unit_types::zergling);
				build::add_build_task(2.0, unit_types::zergling);
				++opening_state;
			} else if (opening_state == 5) {
				if (all_done()) {
					if (!being_rushed) build::add_build_task(3.0, unit_types::hatchery);
					++opening_state;
				}
			} else execute_build(false, build);


			place_static_defence();
			place_expos();

			multitasking::sleep(15);
		}
		

		resource_gathering::max_gas = 0.0;

	}

	void render() {
		
	}

};

