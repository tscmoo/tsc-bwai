
struct strat_tvp_opening {

	wall_in::wall_builder wall;

	void run() {

		combat::no_aggressive_groups = true;

		using namespace buildpred;

		get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::stim_packs, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::u_238_shells, -10.0);
		get_upgrades::set_upgrade_order(upgrade_types::stim_packs, -9.0);

		get_upgrades::set_upgrade_value(upgrade_types::moebius_reactor, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::lockdown, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::personal_cloaking, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::lockdown, -10.0);
		get_upgrades::set_upgrade_order(upgrade_types::personal_cloaking, -9.0);
		get_upgrades::set_upgrade_order(upgrade_types::moebius_reactor, -8.0);

		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_1, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_1, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::terran_infantry_weapons_1, -3.0);
		get_upgrades::set_upgrade_order(upgrade_types::terran_infantry_armor_1, -2.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_2, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_2, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_3, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_3, -1.0);

		get_upgrades::set_upgrade_value(upgrade_types::restoration, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::optical_flare, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::caduceus_reactor, 10000.0);

		get_upgrades::set_no_auto_upgrades(true);

		scouting::scout_supply = 9;
		scouting::comsat_supply = 30;

		resource_gathering::max_gas = 100.0;
		bool wall_calculated = false;
		bool has_wall = false;
		unit*scout_unit = nullptr;
		size_t scout_index = 0;
		bool opponent_has_fast_expanded = false;
		bool is_bunker_rushing = false;
		a_vector<combat::combat_unit*> bunker_rushing_scvs;
		a_set<combat::combat_unit*> bunker_rushing_marines;
		unit*bunker_rush_nexus_target = nullptr;
		unit*last_bunker_rush_bunker = nullptr;
		bool bunker_rush_repair_bunker_to_full = false;
		while (true) {

			if (current_used_total_supply < 20 && !opponent_has_fast_expanded) {
				bool has_expanded = false;
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
					if (bs.building && bs.building->owner == players::opponent_player) has_expanded = true;
				}
				log("has_expanded is %d\n", has_expanded);
				if (has_expanded) {
					if (current_used_total_supply < 16) {
						log("begin bunker rush!\n");
						is_bunker_rushing = true;
					}
					opponent_has_fast_expanded = true;
				}
			}

			int my_marine_count = my_units_of_type[unit_types::marine].size();
			int my_firebat_count = my_units_of_type[unit_types::firebat].size();
			int my_vulture_count = my_units_of_type[unit_types::vulture].size();
			int my_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();

			int enemy_zealot_count = 0;
			int enemy_dragoon_count = 0;
			int enemy_dt_count = 0;
			int enemy_forge_count = 0;
			int enemy_cannon_count = 0;
			int enemy_citadel_of_adun_count = 0;
			int enemy_templar_archives_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zealot) ++enemy_zealot_count;
				if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
				if (e->type == unit_types::dark_templar) ++enemy_dt_count;
				if (e->type == unit_types::forge) ++enemy_forge_count;
				if (e->type == unit_types::photon_cannon) ++enemy_cannon_count;
				if (e->type == unit_types::citadel_of_adun) ++enemy_citadel_of_adun_count;
				if (e->type == unit_types::templar_archives) ++enemy_templar_archives_count;
			}

			int attacking_zealot_count = 0;
			int attacking_dragoon_count = 0;
			for (unit*e : enemy_units) {
				if (current_frame - e->last_seen >= 15 * 60) continue;
				if (e->type == unit_types::zealot || e->type == unit_types::dragoon) {
					double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
						return unit_pathing_distance(unit_types::scv, e->pos, pos);
					});
					if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base)*0.33 < e_d) {
						if (e->type == unit_types::zealot) ++attacking_zealot_count;
						if (e->type == unit_types::dragoon) ++attacking_dragoon_count;
					}
				}
			}
			log("attacking_zealot_count is %d, attacking_dragoon_count is %d\n", attacking_zealot_count, attacking_dragoon_count);
			
			if (enemy_dt_count) combat::aggressive_vultures = false;

			double my_lost = players::my_player->minerals_lost + players::my_player->gas_lost;
			double op_lost = players::opponent_player->minerals_lost + players::opponent_player->gas_lost;

			if (attacking_zealot_count + attacking_dragoon_count >= 4 || is_bunker_rushing) {
				if (!scouting::all_scouts.empty()) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
			}

			if (!my_units_of_type[unit_types::academy].empty()) {
				resource_gathering::max_gas = 150.0;
			}

			if (!my_units_of_type[unit_types::science_facility].empty()) {
				resource_gathering::max_gas = 800.0;
			}

			if (my_tank_count) get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);

			update_possible_start_locations();
			if (possible_start_locations.size() == 1) {
				resource_spots::spot*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
					return diag_distance(s->pos - possible_start_locations.front());
				});
				if (s) {
					unit*gas = nullptr;
					for (auto&v : s->resources) {
						if (v.u->type->is_gas) gas = v.u;
					}
					if (gas && gas->last_seen && gas->type == unit_types::vespene_geyser) {
						int completed_gateways = 0;
						int gateways = 0;
						for (unit*e : enemy_buildings) {
							if (e->type == unit_types::gateway) {
								++gateways;
								if (e->is_completed) ++completed_gateways;
							}
						}
						if ((completed_gateways || enemy_zealot_count) && (gateways >= 2 || enemy_zealot_count >= 2)) {
							attacking_zealot_count = enemy_zealot_count + gateways * 2;
						}
					}
				}
			}

			bool opponent_has_expanded = get_op_current_state().bases.size() >= 2;

			bool being_zealot_rushed = !opponent_has_expanded && attacking_zealot_count >= 2 && attacking_zealot_count > attacking_dragoon_count && attacking_zealot_count > my_vulture_count + my_marine_count / 3 && my_firebat_count < 8;
			if (being_zealot_rushed && current_used_total_supply >= 60) being_zealot_rushed = false;

			if (being_zealot_rushed && enemy_dragoon_count == 0) {
				get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, 200.0);
				get_upgrades::set_upgrade_value(upgrade_types::stim_packs, 200.0);
			}


			if (is_bunker_rushing) {
				if (!bunker_rush_nexus_target) {
					bunker_rush_nexus_target = get_best_score(enemy_buildings, [&](unit*e) {
						if (e->type != unit_types::nexus) return std::numeric_limits<double>::infinity();
						return unit_pathing_distance(unit_types::scv, my_start_location, e->pos);
					}, std::numeric_limits<double>::infinity());
				}
				if (!bunker_rush_nexus_target || bunker_rush_nexus_target->dead || (my_resource_depots.size() >= 2 && my_units_of_type[unit_types::bunker].empty())) {
					is_bunker_rushing = false;
				} else {

					combat::no_aggressive_groups = false;

					xy op_start_loc = possible_start_locations.front();

					if (bunker_rushing_marines.size() < 4) {
						for (auto*a : combat::live_combat_units) {
							if (a->u->type == unit_types::marine && bunker_rushing_marines.insert(a).second) break;
						}
					}
					if (!bunker_rushing_marines.empty()) {
						bool all_dead = true;
						for (auto*a : bunker_rushing_marines) {
							if (!a->u->dead) {
								all_dead = false;
								break;
							}
						}
						if (all_dead) is_bunker_rushing = false;
					}
					if (enemy_zealot_count + enemy_dragoon_count + enemy_cannon_count && my_units_of_type[unit_types::bunker].empty()) is_bunker_rushing = false;

					if (my_units_of_type[unit_types::marine].size() >= 1) {
						size_t use_scv_count = 4;
						if (my_workers.size() > 16) {
							if (my_workers.size() > 18) use_scv_count = 6;
							else use_scv_count = my_workers.size() - 12;
						}
						if (bunker_rushing_scvs.size() < use_scv_count) {
							auto get_best = [&]() {
								return get_best_score(combat::live_combat_units, [&](combat::combat_unit*cu) {
									for (auto*a : bunker_rushing_scvs) {
										if (cu == a) return std::numeric_limits<double>::infinity();
									}
									if (cu->u->type != unit_types::scv) return std::numeric_limits<double>::infinity();
									return unit_pathing_distance(cu->u, bunker_rush_nexus_target->pos);
								}, std::numeric_limits<double>::infinity());
							};
							while (bunker_rushing_scvs.size() < use_scv_count) {
								combat::combat_unit*cu = get_best();
								if (!cu) break;
								bunker_rushing_scvs.push_back(cu);
							}
						}
						log("bunker_rushing_scvs.size() is %d\n", bunker_rushing_scvs.size());
						if (bunker_rushing_scvs.empty()) is_bunker_rushing = false;
						
						bool build_bunkers = true;
						if (last_bunker_rush_bunker && last_bunker_rush_bunker->dead) build_bunkers = false;
						if (my_units_of_type[unit_types::marine].empty()) build_bunkers = false;

						static int flag;
						build::build_task*bunker_build_task = nullptr;
						for (auto&b : build::build_tasks) {
							if (b.flag == &flag) {
								if (build_bunkers) bunker_build_task = &b;
								else b.dead = true;
								break;
							}
						}
						if (bunker_build_task && bunker_build_task->built_unit) last_bunker_rush_bunker = bunker_build_task->built_unit;
						if (!bunker_build_task && my_completed_units_of_type[unit_types::bunker].size() < 1 && build_bunkers) {
							bunker_build_task = build::add_build_task(0, unit_types::bunker);
							bunker_build_task->flag = &flag;
						}
						auto*next_builder = get_best_score(bunker_rushing_scvs, [&](combat::combat_unit*cu) {
							if (cu->u->dead || cu->u->controller->action == unit_controller::action_build) return std::numeric_limits<double>::infinity();
							return diag_distance(cu->u->pos - op_start_loc);
						}, std::numeric_limits<double>::infinity());
						if (!next_builder) is_bunker_rushing = false;
						if (bunker_build_task && next_builder && !bunker_build_task->built_unit) {

							std::array<xy, 1> starts;
							starts[0] = bunker_rush_nexus_target->pos;

							a_vector<unit*> nearby_enemies;
							for (unit*e : enemy_units) {
								if (!e->visible) continue;
								if (diag_distance(e->pos - starts[0]) < 32 * 15) nearby_enemies.push_back(e);
							}

							auto pred = [&](grid::build_square&bs) {
								if (!build_spot_finder::can_build_at(unit_types::bunker, bs)) return false;
 								xy pos = bs.pos + xy(unit_types::bunker->tile_width * 16, unit_types::bunker->tile_height * 16);
								if (units_distance(bunker_rush_nexus_target->pos, bunker_rush_nexus_target->type, pos, unit_types::bunker) > 32 * 5) return false;
								for (unit*e : nearby_enemies) {
									if (units_distance(e->pos, e->type, pos, unit_types::bunker) == 0) return false;
								}

								return true;
							};
							auto score = [&](xy build_pos) {
								xy pos = build_pos + xy(unit_types::bunker->tile_width * 16, unit_types::bunker->tile_height * 16);
								double r = 0;
								for (unit*u : my_units_of_type[unit_types::bunker]) {
									if ((pos - u->pos).length() <= 32 * 6) r -= 10000;
								}
								if (!my_units_of_type[unit_types::bunker].empty()) {
									if ((bunker_rush_nexus_target->pos - pos).length() <= 32 * 6) r -= 20000;
								}
								//r += diag_distance(bunker_rush_nexus_target->pos - pos);
								r += unit_pathing_distance(unit_types::scv, my_start_location, pos) / 2;
								return r;
							};
							build::unset_build_pos(bunker_build_task);
							xy pos = build_spot_finder::find_best(starts, 128, pred, score);

							build::set_build_pos(bunker_build_task, pos);
							if (!bunker_build_task->builder) build::set_builder(bunker_build_task, next_builder->u);

						}

						xy rally_pos = bunker_rush_nexus_target->pos;
						if (last_bunker_rush_bunker) rally_pos = last_bunker_rush_bunker->pos;
						if (bunker_build_task) rally_pos = bunker_build_task->build_pos;
						unit*repair_bunker = get_best_score(my_completed_units_of_type[unit_types::bunker], [&](unit*u) {
							if (u->hp == u->stats->hp) return std::numeric_limits<double>::infinity();
							if (u->hp > u->stats->hp*0.75 && !bunker_rush_repair_bunker_to_full) return std::numeric_limits<double>::infinity();
							return u->hp;
						}, std::numeric_limits<double>::infinity());
						if (!repair_bunker && bunker_rush_repair_bunker_to_full) bunker_rush_repair_bunker_to_full = false;
						for (auto*a : bunker_rushing_scvs) {
							a->u->force_combat_unit = true;
							if (a->u->controller->action == unit_controller::action_build) continue;
							if (repair_bunker) {
								bunker_rush_repair_bunker_to_full = true;
								a->strategy_busy_until = current_frame + 15 * 2;
								a->action = combat::combat_unit::action_offence;
								a->subaction = combat::combat_unit::subaction_repair;
								a->target = repair_bunker;
							} else if (diag_distance(a->u->pos - rally_pos) > 32 * 3) {
								a->strategy_busy_until = current_frame + 15 * 2;
								a->action = combat::combat_unit::action_offence;
								a->subaction = combat::combat_unit::subaction_move;
								a->target_pos = rally_pos;
							} else {
								a->strategy_attack_until = current_frame + 15 * 2;
								a->strategy_busy_until = 0;
							}
						}
						for (auto*a : bunker_rushing_marines) {
							if (diag_distance(a->u->pos - rally_pos) >= 32 * 20) {
								a->strategy_busy_until = current_frame + 15 * 2;
								a->action = combat::combat_unit::action_offence;
								a->subaction = combat::combat_unit::subaction_move;
								a->target_pos = rally_pos;
							}
						}
					}
				}
			} else {
				combat::no_aggressive_groups = true;
			}


			auto build = [&](state&st) {
				if (st.used_supply[race_terran] < 30 && count_units_plus_production(st, unit_types::marine) < 4) st.dont_build_refineries = true;

				int scv_count = count_units_plus_production(st, unit_types::scv);
				if (!being_zealot_rushed && scv_count < 22 && !is_bunker_rushing) {
					if (scv_count >= 11 && count_units_plus_production(st, unit_types::barracks) == 0) return depbuild(st, state(st), unit_types::barracks);
					if (count_units_plus_production(st, unit_types::cc) == 1) {
						if (scv_count >= 15) return false;
						return depbuild(st, state(st), unit_types::scv);
					}
					if (count_units_plus_production(st, unit_types::barracks) == 0) return depbuild(st, state(st), unit_types::scv);
				}

				int marine_count = count_units_plus_production(st, unit_types::marine);
				int medic_count = count_units_plus_production(st, unit_types::medic);
				int firebat_count = count_units_plus_production(st, unit_types::firebat);
				int ghost_count = count_units_plus_production(st, unit_types::ghost);
				int wraith_count = count_units_plus_production(st, unit_types::wraith);
				int valkyrie_count = count_units_plus_production(st, unit_types::valkyrie);
				int science_vessel_count = count_units_plus_production(st, unit_types::science_vessel);
				int tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
				int vulture_count = count_units_plus_production(st, unit_types::vulture);

				std::function<bool(state&)> army = [&](state&st) {
					if (marine_count < 4) return depbuild(st, state(st), unit_types::marine);
					return maxprod1(st, unit_types::marine);
				};

				if (count_units_plus_production(st, unit_types::barracks) >= 5 && count_units_plus_production(st, unit_types::science_facility) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::science_facility, army);
					};
				}

				if (medic_count < (marine_count + firebat_count - 2) / 4) {
					army = [army](state&st) {
						return nodelay(st, unit_types::medic, army);
					};
 				}
				if (marine_count >= 4 && firebat_count < 3 && marine_count < 12) {
					army = [army](state&st) {
						return nodelay(st, unit_types::firebat, army);
					};
				}

				if (!being_zealot_rushed && marine_count >= 2) {
					if (count_units_plus_production(st, unit_types::barracks) < 3) {
						army = [army](state&st) {
							return nodelay(st, unit_types::barracks, army);
						};
					}
				}

				if (count_units_plus_production(st, unit_types::academy) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::academy, army);
					};
				} else {
					if (!being_zealot_rushed) {
						if (count_units_plus_production(st, unit_types::engineering_bay) == 0) {
							army = [army](state&st) {
								return nodelay(st, unit_types::engineering_bay, army);
							};
						}
					}
				}
				if (count_units_plus_production(st, unit_types::refinery) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::refinery, army);
					};
				}

				if (marine_count < (count_units_plus_production(st, unit_types::academy) ? 4 : 1)) {
					army = [army](state&st) {
						return nodelay(st, unit_types::marine, army);
					};
				}

				if (enemy_dt_count && count_units_plus_production(st, unit_types::missile_turret) == 0) {
					army = [army](state&st) {
						return maxprod(st, unit_types::missile_turret, army);
					};
				}

				if (being_zealot_rushed) {
					army = [army](state&st) {
						return maxprod(st, unit_types::marine, army);
					};
					army = [army](state&st) {
						return nodelay(st, unit_types::firebat, army);
					};
					army = [army](state&st) {
						return nodelay(st, unit_types::vulture, army);
					};
					if (marine_count < 8) {
						army = [army](state&st) {
							return nodelay(st, unit_types::marine, army);
						};
					}
				}

				if (being_zealot_rushed && (!has_wall || !wall.is_walled_off)) {
					if (count_units_plus_production(st, unit_types::bunker) < std::min(marine_count / 2, 2)) {
						army = [army](state&st) {
							return maxprod(st, unit_types::bunker, army);
						};
					}
				}

				if (marine_count < 2) {
					return nodelay(st, unit_types::marine, [&](state&st) {
						return nodelay(st, unit_types::scv, [&](state&st) {
							return army(st);
						});
					});
				}

				return nodelay(st, unit_types::scv, [&](state&st) {
					return army(st);
				});

			};

			if (being_zealot_rushed && my_units_of_type[unit_types::bunker].empty()) {
				for (unit*u : my_units_of_type[unit_types::cc]) {
					if (!u->is_completed) {
						u->game_unit->cancelConstruction();
						for (auto&b : build::build_tasks) {
							if (b.type->unit == unit_types::cc) b.dead = true;
						}
						break;
					}
				}
			}

			xy natural_cc_pos = my_start_location;
			if (true) {
				auto*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
					if (diag_distance(s->cc_build_pos - my_start_location) <= 32 * 10) return std::numeric_limits<double>::infinity();
					double score = unit_pathing_distance(unit_types::scv, s->cc_build_pos, my_start_location);
					double res = 0;
					bool has_gas = false;
					for (auto&r : s->resources) {
						res += r.u->resources;
						if (r.u->type->is_gas) has_gas = true;
					}
					score -= (has_gas ? res : res / 4) / 10;
					return score;
				}, std::numeric_limits<double>::infinity());
				if (s) natural_cc_pos = s->cc_build_pos;
			}

			unit*free_cc = nullptr;
			for (unit*u : my_completed_units_of_type[unit_types::cc]) {
				bool is_free = true;
				for (auto&s : resource_spots::spots) {
					if (!u->building->is_lifted && u->building->build_pos == s.cc_build_pos) {
						is_free = false;
					}
				}
				if (!u->building->is_lifted && u->building->build_pos != my_start_location) {
					if (being_zealot_rushed) is_free = true;
				}
				if (!is_free) continue;
				free_cc = u;
				break;
			}
			if (free_cc) {
				xy land_pos = natural_cc_pos;
				if (land_pos != xy()) {
					free_cc->controller->action = unit_controller::action_building_movement;
					if (being_zealot_rushed) free_cc->controller->lift = true;
					else {
						if (free_cc->building->is_lifted) free_cc->controller->lift = false;
						else if (free_cc->building->build_pos != land_pos) free_cc->controller->lift = true;
					}
					free_cc->controller->target_pos = land_pos;
					free_cc->controller->timeout = current_frame + 15 * 30;
				}
			}

			if ((being_zealot_rushed || current_used_total_supply >= 18) && !is_bunker_rushing) {
				combat::build_bunker_count = 1;
				if (attacking_zealot_count >= 5 || current_used_total_supply >= 65 || my_marine_count + my_firebat_count > 4) combat::build_bunker_count = 2;
				if (being_zealot_rushed && my_marine_count == 0) combat::build_bunker_count = 0;
			} else combat::build_bunker_count = 0;

			if (wall_calculated && !being_zealot_rushed) {
				combat::my_closest_base_override = natural_cc_pos;
				combat::my_closest_base_override_until = current_frame + 15 * 20;
			}

			if (my_completed_units_of_type[unit_types::cc].size() >= 2 && current_used_total_supply >= 80) break;

			bool expand = false;
			if (my_units_of_type[unit_types::cc].size() == 1) {
				if (current_used_total_supply >= (opponent_has_fast_expanded ? 24 : 40)) expand = true;
				if (current_used_total_supply >= (is_bunker_rushing ? 20 : 15)) expand = true;
				if (being_zealot_rushed) expand = false;
			}

			execute_build(expand, build);


			if (combat::defence_choke.center != xy() && !wall_calculated) {
				wall_calculated = true;

				wall.spot = wall_in::find_wall_spot_from_to(unit_types::zealot, combat::my_closest_base, combat::defence_choke.outside, false);
				wall.spot.outside = combat::defence_choke.outside;

				wall.against(unit_types::zealot);
				wall.add_building(unit_types::supply_depot);
				wall.add_building(unit_types::barracks);
				has_wall = true;
				if (!wall.find()) {
					wall.add_building(unit_types::academy);
					if (!wall.find()) {
						log("failed to find wall in :(\n");
						has_wall = false;
					}
				}
			}
			if (has_wall) {
				wall.build();

				if (!being_zealot_rushed && my_completed_units_of_type[unit_types::bunker].size() >= 1 && my_marine_count >= 4) {
					wall = wall_in::wall_builder();
					has_wall = false;
				}
				if (opponent_has_fast_expanded && my_marine_count) {
					wall = wall_in::wall_builder();
					has_wall = false;
				}
			}

			multitasking::sleep(is_bunker_rushing ? 15 : 15 * 5);
		}
		combat::build_bunker_count = 0;
		resource_gathering::max_gas = 0.0;

		get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, 0.0);
		get_upgrades::set_no_auto_upgrades(false);

		combat::aggressive_vultures = true;

	}

	void render() {
		wall.render();
	}

};

