
struct strat_zvp_opening {

	void run() {

		get_upgrades::set_no_auto_upgrades(true);
		combat::no_aggressive_groups = true;
		combat::aggressive_zerglings = true;
		combat::no_scout_around = true;
		scouting::no_proxy_scout = true;

// 		get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, -1.0);
// 		get_upgrades::set_upgrade_value(upgrade_types::grooved_spines, -1.0);

		using namespace buildpred;

		auto place_static_defence = [&]() {
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					if (b.build_near != combat::defence_choke.center) {
// 						b.build_near = combat::defence_choke.center;
// 						b.require_existing_creep = true;
// 						build::unset_build_pos(&b);

						unit*build_near_unit = get_best_score(my_buildings, [&](unit*u) {
							return unit_pathing_distance(unit_types::zergling, u->pos, combat::op_closest_base);
						});
						if (build_near_unit) {
							build::unset_build_pos(&b);
							std::array<xy, 1> starts;
							starts[0] = build_near_unit->pos;

							auto pred = [&](grid::build_square&bs) {
								if (!build_spot_finder::can_build_at(b.type->unit, bs)) return false;
								return true;
							};
							auto score = [&](xy pos) {
								return units_distance(pos, b.type->unit, build_near_unit->building->build_pos, build_near_unit->type);
							};
							xy pos = build_spot_finder::find_best(starts, 128, pred, score);
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

			//if (st.used_supply[race_zerg] < 24 && st.bases.size() >= 2) return;
			if (my_workers.size() < 24 && st.bases.size() >= 2) return;
			//if (st.bases.size() >= 3) return;

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
		bool go_hydras = false;
		bool being_zealot_rushed = false;
		bool has_sent_scouts = false;
		while (true) {

			int enemy_zealot_count = 0;
			int enemy_dragoon_count = 0;
			int enemy_forge_count = 0;
			int enemy_cannon_count = 0;
			int enemy_gateway_count = 0;
			double enemy_army_supply = 0.0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zealot) ++enemy_zealot_count;
				if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
				if (e->type == unit_types::forge) ++enemy_forge_count;
				if (e->type == unit_types::photon_cannon) ++enemy_cannon_count;
				if (e->type == unit_types::gateway) ++enemy_gateway_count;
				if (!e->type->is_worker) enemy_army_supply += e->type->required_supply;
			}
			if (enemy_cannon_count && !enemy_forge_count) enemy_forge_count = 1;
			if (enemy_zealot_count + enemy_dragoon_count && !enemy_gateway_count) enemy_gateway_count = 1;

			int my_zergling_count = my_units_of_type[unit_types::zergling].size();

			if (enemy_buildings.empty() && enemy_dragoon_count == 0) overlord_scout();
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

			scouting::scout_supply = 20;

			if (enemy_forge_count + enemy_zealot_count) {
				if (!scouting::all_scouts.empty()) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
			} else {
				if (!has_sent_scouts && my_completed_units_of_type[unit_types::zergling].size() >= 3) {
					for (unit*u : my_completed_units_of_type[unit_types::zergling]) {
						scouting::add_scout(u);
					}
					has_sent_scouts = true;
				}
			}

// 			if (!combat::no_aggressive_groups) {
// 				int lose_count = 0;
// 				int total_count = 0;
// 				for (auto*a : combat::live_combat_units) {
// 					if (a->u->type->is_worker) continue;
// 					++total_count;
// 					if (current_frame - a->last_fight <= 15 * 10) {
// 						if (a->last_win_ratio < 1.0) ++lose_count;
// 					}
// 				}
// 				if (lose_count >= total_count / 2) {
// 					combat::no_aggressive_groups = true;
// 				}
// 			}

//			combat::no_aggressive_groups = !(my_zergling_count >= 12 && my_zergling_count > enemy_zealot_count * 3);

			if (my_zergling_count >= 10) {
				get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
				//go_hydras = true;
			}
			//go_hydras = true;

			if (enemy_zealot_count >= 3) {
				being_zealot_rushed = true;
			}

			resource_gathering::max_gas = 150.0;
			if (!my_units_of_type[unit_types::spire].empty()) resource_gathering::max_gas = 400.0;
			if (enemy_zealot_count > my_zergling_count / 4 && my_units_of_type[unit_types::hydralisk_den].empty()) {
				//resource_gathering::max_gas = 1.0;
				combat::aggressive_zerglings = false;
			}

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

				if (!being_zealot_rushed && !enemy_forge_count && st.used_supply[race_zerg] < 18) {
					if (larva_count <= 2) return false;
				}

				int drone_count = count_units_plus_production(st, unit_types::drone);
				int hatch_count = count_units_plus_production(st, unit_types::hatchery) + count_units_plus_production(st, unit_types::lair) + count_units_plus_production(st, unit_types::hive);
				int zergling_count = count_units_plus_production(st, unit_types::zergling);

				st.dont_build_refineries = drone_count < 24;

				std::function<bool(state&)> army = [&](state&st) {
					return false;
				};

				// 				army = [army](state&st) {
				// 					return nodelay(st, unit_types::hatchery, army);
				// 				};
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};

				// 				if (count_units_plus_production(st, unit_types::hatchery) < 3) {
				// 					army = [army](state&st) {
				// 						return nodelay(st, unit_types::hatchery, army);
				// 					};
				// 				}

				if (count_units_plus_production(st, unit_types::lair) && count_units_plus_production(st, unit_types::hatchery) < 3) {
					if (drone_count >= 24) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hatchery, army);
						};
					}
				}
				if (hatch_count < drone_count / 5) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}

				// 				if (my_completed_units_of_type[unit_types::hatchery].size() >= 2) {
				// 					if (count_units_plus_production(st, unit_types::sunken_colony) < 2) {
				// 						army = [army](state&st) {
				// 							return depbuild(st, state(st), unit_types::sunken_colony);
				// 						};
				// 					}
				// 				}

				if (st.used_supply[race_zerg] >= 30 && st.max_supply[race_zerg] - st.used_supply[race_zerg] < 16) {
					army = [army](state&st) {
						return nodelay(st, unit_types::overlord, army);
					};
				}

				double army_supply = st.used_supply[race_zerg] - drone_count;
				//if (army_supply < std::min(enemy_army_supply * 2, enemy_forge_count ? 0.0 : 8.0)) {
				if (army_supply < enemy_army_supply) {
					// 					if (drone_count >= 20) {
					// 						if (army_supply < enemy_army_supply - (my_completed_units_of_type[unit_types::sunken_colony].size() >= 2 ? 8 : 0)) {
					// 							army = [army](state&st) {
					// 								return depbuild(st, state(st), unit_types::zergling);
					// 							};
					// 						}
					// 					} else {
					// 						army = [army](state&st) {
					// 							return depbuild(st, state(st), unit_types::zergling);
					// 						};
					// 					}
					army = [army](state&st) {
						return depbuild(st, state(st), unit_types::zergling);
					};
					if (!my_completed_units_of_type[unit_types::hydralisk_den].empty()) {
						army = [army](state&st) {
							return depbuild(st, state(st), unit_types::hydralisk);
						};
					}
				}

				// 				if (drone_count >= 16 && count_units_plus_production(st, unit_types::spire) == 0) {
				// 					if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
				// 						army = [army](state&st) {
				// 							return nodelay(st, unit_types::hydralisk_den, army);
				// 						};
				// 					}
				// 					army = [army](state&st) {
				// 						return nodelay(st, unit_types::spire, army);
				// 					};
				// 				}

				// 				if (count_units_plus_production(st, unit_types::spire) == 0) {
				// 					army = [army](state&st) {
				// 						return nodelay(st, unit_types::spire, army);
				// 					};
				// 				}
				//if (drone_count >= (being_zealot_rushed ? 20 : 14)) {
// 				if (drone_count >= 14) {
// 					// 					if (count_units_plus_production(st, unit_types::lair) == 0) {
// 					// 						army = [army](state&st) {
// 					// 							return nodelay(st, unit_types::lair, army);
// 					// 						};
// 					// 					}
// 					// 					if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
// 					// 						army = [army](state&st) {
// 					// 							return nodelay(st, unit_types::hydralisk_den, army);
// 					// 						};
// 					// 					}
// 					if (go_hydras) {
// 						if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
// 							army = [army](state&st) {
// 								return nodelay(st, unit_types::hydralisk_den, army);
// 							};
// 						}
// 					} else {
// 						if (count_units_plus_production(st, unit_types::spire) == 0) {
// 							army = [army](state&st) {
// 								return nodelay(st, unit_types::spire, army);
// 							};
// 						}
// 					}
// 				}
				if (drone_count >= 14) {
					if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk_den, army);
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
// 				if (st.used_supply[race_zerg] >= 20 && count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::hydralisk_den, army);
// 					};
// 				}

				//if (being_zealot_rushed && zergling_count < 4 && my_completed_units_of_type[unit_types::sunken_colony].empty()) {
				if (being_zealot_rushed && zergling_count < 12) {
					army = [army](state&st) {
						return nodelay(st, unit_types::zergling, army);
					};
				}

// 				if (drone_count >= 14 && (zergling_count >= enemy_zealot_count * 3 || army_supply >= enemy_army_supply)) {
// 					if (count_units_plus_production(st, unit_types::spire) == 0) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::spire, army);
// 						};
// 					}
// 				}

				if (drone_count >= 13 && count_units_plus_production(st, unit_types::extractor) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::extractor, army);
					};
				}

// 				if (count_production(st, unit_types::drone) == 0) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::drone, army);
// 					};
// 				}

				//if ((enemy_zealot_count || enemy_buildings.empty()) && st.bases.size() == 2 && my_completed_units_of_type[unit_types::hatchery].size() >= 2) {
				if (enemy_forge_count == 0 && st.bases.size() == 2 && my_completed_units_of_type[unit_types::hatchery].size() >= 2) {
					int add = 0;
					for (unit*u : my_completed_units_of_type[unit_types::sunken_colony]) {
						if (u->hp < u->stats->hp / 2) {
							++add;
							break;
						}
					}
					//if (count_units_plus_production(st, unit_types::sunken_colony) < std::max(enemy_zealot_count / 2, 2)) {
					//if (count_units_plus_production(st, unit_types::sunken_colony) < std::min(std::max(enemy_zealot_count / 2, 4), 2) + add) {
// 					if (count_units_plus_production(st, unit_types::sunken_colony) < 1) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::sunken_colony, army);
// 						};
// 					}
				}

				return army(st);
			};

// 			//if (count_units_plus_production(get_my_current_state(), unit_types::hatchery) >= 2) break;
// 			//if (my_workers.size() >= 24) break;
// 			//if (!my_units_of_type[unit_types::spire].empty() && my_units_of_type[unit_types::spire].front()->remaining_build_time <= 15 * 15) break;
 			if (!my_completed_units_of_type[unit_types::spire].empty()) break;
// 			if (current_used_total_supply >= 40 || my_workers.size() >= 28) break;
// 			//if (!my_completed_units_of_type[unit_types::lair].empty()) break;

// 			if (!my_completed_units_of_type[unit_types::spire].empty() && !my_completed_units_of_type[unit_types::hydralisk_den].empty()) break;
// 			if (current_used_total_supply >= 80) break;

// 			if (opening_state == 0) {
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				opening_state = 1;
// 			} else if (opening_state == 1) {
// 				if (current_minerals >= 80) {
// 					build::add_build_task(1.0, unit_types::extractor);
// 					opening_state = 2;
// 				}
// 			} else if (opening_state == 2) {
// 				if (!my_units_of_type[unit_types::extractor].empty() && !build::build_order.empty()) {
// // 					build::add_build_task(2.0, unit_types::drone);
// // 					build::add_build_task(2.5, unit_types::overlord);
// // 					build::add_build_task(3.0, unit_types::spawning_pool);
// // 					build::add_build_task(3.75, unit_types::drone);
// // 					build::add_build_task(4.5, unit_types::drone);
// // 					build::add_build_task(5.0, unit_types::zergling);
// // 					build::add_build_task(5.0, unit_types::drone);
// // 					build::add_build_task(5.0, unit_types::drone);
// // // 					build::add_build_task(5.0, unit_types::zergling);
// // // 					build::add_build_task(5.5, unit_types::zergling);
// // 					build::add_build_task(5.5, unit_types::hatchery);
// 
// 					build::add_build_task(2.0, unit_types::drone);
// 					build::add_build_task(2.5, unit_types::overlord);
// 					build::add_build_task(3.0, unit_types::drone);
// 					build::add_build_task(3.0, unit_types::drone);
// 					build::add_build_task(4.0, unit_types::hatchery);
// 					build::add_build_task(5.0, unit_types::spawning_pool);
// 					build::add_build_task(6.0, unit_types::drone);
// 					build::add_build_task(6.0, unit_types::drone);
// 					build::add_build_task(6.0, unit_types::drone);
// 					build::add_build_task(7.0, unit_types::hatchery);
// 					build::add_build_task(8.0, unit_types::zergling);
// 					build::add_build_task(8.0, unit_types::zergling);
// 					build::add_build_task(8.0, unit_types::zergling);
// 					build::add_build_task(9.0, unit_types::extractor);
// 					opening_state = 3;
// 				}
// 			} else if (opening_state == 3) {
// 				if (count_units_plus_production(get_my_current_state(), unit_types::drone) >= 9) {
// 					for (unit*u : my_units_of_type[unit_types::extractor]) {
// 						if (!u->is_completed) {
// 							u->game_unit->cancelConstruction();
// 							for (auto&b : build::build_tasks) {
// 								if (b.built_unit == u) {
// 									b.dead = true;
// 									break;
// 								}
// 							}
// 							opening_state = 4;
// 							break;
// 						}
// 					}
// 				}
// 			} else if (opening_state == 4) {
// 				bool all_done = true;
// 				for (auto&b : build::build_order) {
// 					if (b.type->unit == unit_types::zergling && enemy_forge_count) b.dead = true;
// 					if (current_frame - b.build_frame < 15 * 15) {
// 						all_done = false;
// 						break;
// 					}
// 				}
// 				if (all_done) opening_state = 5;
// 			} else execute_build(false, build);

// 			if (opening_state == 0) {
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(1.0, unit_types::overlord);
// 				build::add_build_task(2.0, unit_types::spawning_pool);
// // 				build::add_build_task(3.0, unit_types::drone);
// // 				build::add_build_task(3.0, unit_types::drone);
// // 				//build::add_build_task(3.0, unit_types::drone);
// // 				build::add_build_task(4.0, unit_types::zergling);
// // 				build::add_build_task(4.0, unit_types::zergling);
// // 				build::add_build_task(4.0, unit_types::zergling);
// // 				build::add_build_task(5.0, unit_types::hatchery);
// // 				build::add_build_task(5.0, unit_types::hatchery);
// 				build::add_build_task(3.0, unit_types::drone);
// 				build::add_build_task(3.0, unit_types::drone);
// 				build::add_build_task(3.0, unit_types::drone);
// 				build::add_build_task(5.0, unit_types::hatchery);
// 				build::add_build_task(4.0, unit_types::zergling);
// 				build::add_build_task(4.0, unit_types::zergling);
// 				build::add_build_task(4.0, unit_types::zergling);
// 				build::add_build_task(5.0, unit_types::hatchery);
// 				opening_state = 1;
// 			} else if (opening_state == 1) {
// 				bool all_done = true;
// 				for (auto&b : build::build_order) {
// 					if (b.type->unit == unit_types::zergling && enemy_forge_count) b.dead = true;
// 					if (current_frame - b.build_frame < 15 * 15) {
// 						all_done = false;
// 						break;
// 					}
// 				}
// 				if (all_done) opening_state = 5;
// 			} else execute_build(false, build);

// 			if (opening_state == 0) {
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				++opening_state;
// 			} else if (opening_state == 1 || opening_state == 5) {
// 				if (current_minerals >= 80) {
// 					build::add_build_task(100.0, unit_types::extractor);
// 					build::add_build_task(101.0, unit_types::drone);
// 					++opening_state;
// 				}
// 			} else if (opening_state == 4) {
// 				build::add_build_task(3.0, unit_types::hatchery);
// 				build::add_build_task(4.0, unit_types::spawning_pool);
// 				build::add_build_task(5.0, unit_types::drone);
// 				build::add_build_task(6.0, unit_types::overlord);
// 				++opening_state;
// 			} else if (opening_state == 2 || opening_state == 6) {
// 				bool all_done = true;
// 				for (auto&b : build::build_tasks) {
// 					if (!b.built_unit) {
// 						all_done = false;
// 						break;
// 					}
// 				}
// 				if (all_done) ++opening_state;
// 			} else if (opening_state == 3 || opening_state == 7) {
// 				for (unit*u : my_units_of_type[unit_types::extractor]) {
// 					if (!u->is_completed) {
// 						u->game_unit->cancelConstruction();
// 						for (auto&b : build::build_tasks) {
// 							if (b.built_unit == u) {
// 								b.dead = true;
// 								break;
// 							}
// 						}
// 						++opening_state;
// 						break;
// 					}
// 				}
// 			} else if (opening_state == 8) {
// 				build::add_build_task(0.0, unit_types::drone);
// // 				build::add_build_task(0.0, unit_types::drone);
// // 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(1.0, unit_types::zergling);
// // 				build::add_build_task(1.0, unit_types::zergling);
// // 				build::add_build_task(1.0, unit_types::zergling);
// 				build::add_build_task(2.0, unit_types::drone);
// 				build::add_build_task(2.0, unit_types::drone);
// 				//build::add_build_task(2.0, unit_types::hatchery);
// //				build::add_build_task(2.0, unit_types::extractor);
// 				++opening_state;
// 			} else execute_build(false, build);

			if (opening_state == 0) {
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::overlord);
				build::add_build_task(1.0, unit_types::drone);
				build::add_build_task(1.0, unit_types::drone);
				build::add_build_task(1.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::hatchery);
				build::add_build_task(3.0, unit_types::spawning_pool);
				build::add_build_task(4.0, unit_types::drone);
				build::add_build_task(4.0, unit_types::drone);
				build::add_build_task(4.0, unit_types::drone);
				build::add_build_task(5.0, unit_types::hatchery);
				build::add_build_task(6.0, unit_types::zergling);
				build::add_build_task(6.0, unit_types::zergling);
				build::add_build_task(6.0, unit_types::zergling);
				++opening_state;
			} else execute_build(false, build);

// 			if (opening_state == 0) {
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::overlord);
// 				build::add_build_task(1.0, unit_types::spawning_pool);
// 				build::add_build_task(2.0, unit_types::drone);
// 				build::add_build_task(2.0, unit_types::drone);
// 				build::add_build_task(2.0, unit_types::drone);
// 				build::add_build_task(3.0, unit_types::hatchery);
// 				build::add_build_task(4.0, unit_types::zergling);
// 				build::add_build_task(4.0, unit_types::zergling);
// 				build::add_build_task(4.0, unit_types::zergling);
// 				build::add_build_task(5.0, unit_types::hatchery);
// 
// 				build::add_build_task(6.0, unit_types::drone);
// 				build::add_build_task(7.0, unit_types::zergling);
// 				build::add_build_task(7.0, unit_types::zergling);
// 				build::add_build_task(8.0, unit_types::drone);
// 				build::add_build_task(9.0, unit_types::zergling);
// 				build::add_build_task(9.0, unit_types::zergling);
// 				++opening_state;
// 			} else execute_build(false, build);

// 			if (opening_state == 0) {
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.25, unit_types::hatchery);
// 				build::add_build_task(0.5, unit_types::drone);
// 				build::add_build_task(0.75, unit_types::overlord);
// 				build::add_build_task(1.0, unit_types::spawning_pool);
// 				build::add_build_task(2.0, unit_types::drone);
// 				build::add_build_task(2.0, unit_types::drone);
// 				build::add_build_task(2.0, unit_types::drone);
// 				//build::add_build_task(3.0, unit_types::hatchery);
// 				build::add_build_task(4.0, unit_types::zergling);
// 				//build::add_build_task(4.0, unit_types::zergling);
// 				//build::add_build_task(4.0, unit_types::zergling);
// 				build::add_build_task(5.0, unit_types::drone);
// 				build::add_build_task(5.0, unit_types::drone);
// 				build::add_build_task(6.0, unit_types::extractor);
// 				//build::add_build_task(5.0, unit_types::hatchery);
// 				++opening_state;
// 			} else execute_build(false, build);

			place_static_defence();
			place_expos();

			multitasking::sleep(15);
		}
		

		resource_gathering::max_gas = 0.0;

	}

	void render() {
		
	}

};

