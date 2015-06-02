
struct strat_ums {


	void run() {


		get_upgrades::set_no_auto_upgrades(true);
		combat::no_aggressive_groups = true;
		combat::aggressive_zerglings = false;
		combat::no_scout_around = true;
		scouting::no_proxy_scout = true;
		scouting::scout_supply = 20;
		units::dont_call_gg = true;

		get_upgrades::set_upgrade_value(upgrade_types::burrow, 100000.0);
		get_upgrades::set_upgrade_value(upgrade_types::ventral_sacs, 100000.0);
		get_upgrades::set_upgrade_value(upgrade_types::antennae, 100000.0);
		
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
				if (my_resource_depots.empty() && s->resources.size() >= 5) return score;
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

		auto free_mineral_patches = [&]() {
			int free_mineral_patches = 0;
			for (auto&b : get_my_current_state().bases) {
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
			return free_mineral_patches;
		};

// 		auto get_mineral_saturation = [&]() {
// 			double total = 0.0;
// 			int count = 0;
// 			for (auto&b : get_my_current_state().bases) {
// 				for (auto&s : b.s->resources) {
// 					if (!s.u->type->is_minerals) continue;
// 					resource_gathering::resource_t*res = nullptr;
// 					for (auto&s2 : resource_gathering::live_resources) {
// 						if (s2.u == s.u) {
// 							res = &s2;
// 							break;
// 						}
// 					}
// 					if (res) {
// 						size_t n = res->gatherers.size();
// 						double sum;
// 						if (n <= s.full_income_workers[race_zerg]) sum = s.income_rate[race_zerg] * n;
// 						else sum = s.income_sum[race_zerg];
// 						total += sum / s.income_sum[race_zerg];
// 						++count;
// 					}
// 				}
// 			}
// 			return total / count;
// 		};

		auto get_max_mineral_workers = [&]() {
			int count = 0;
			for (auto&b : get_my_current_state().bases) {
				for (auto&s : b.s->resources) {
					if (!s.u->type->is_minerals) continue;
					count += (int)s.full_income_workers[race_zerg];
				}
			}
			return count;
		};

		auto place_expos = [&]() {

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::hatchery) {
					xy pos = b.build_pos;
					build::unset_build_pos(&b);
				}
			}

			//if (!my_resource_depots.empty() && get_mineral_saturation() < 0.75) return;
			if (!my_resource_depots.empty() && (int)my_workers.size() < get_max_mineral_workers() / 2) return;

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

		auto get_static_defence_pos = [&]() {
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
					if (!build::build_has_existing_creep(bs, unit_types::sunken_colony)) return false;
					if (!build_spot_finder::can_build_at(unit_types::sunken_colony, bs)) return false;
					if (combat::entire_threat_area.test(grid::build_square_index(bs))) return false;
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
				return build_spot_finder::find_best(starts, 256, pred, score);
			}
			return xy();
		};

		auto place_static_defence = [&]() {
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					if (b.build_near != combat::defence_choke.center) {
						b.build_near = combat::defence_choke.center;
						build::unset_build_pos(&b);
						xy pos = get_static_defence_pos();
						if (pos != xy()) {
							build::set_build_pos(&b, pos);
						}
					}
				}
			}
		};

		auto weapon_damage_against_size = [&](weapon_stats*w, int size) {
			if (!w) return 0.0;
			double damage = w->damage;
			damage *= combat_eval::get_damage_type_modifier(w->damage_type, size);
			if (damage <= 0) damage = 1.0;
			return damage;
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

		a_unordered_map<unit*, int> last_been_near_unit;

		while (true) {

			int my_zergling_count = my_units_of_type[unit_types::zergling].size();
			int my_queen_count = my_units_of_type[unit_types::queen].size();
			int my_defiler_count = my_units_of_type[unit_types::defiler].size();
			int my_mutalisk_count = my_units_of_type[unit_types::mutalisk].size();

			int enemy_marine_count = 0;
			int enemy_firebat_count = 0;
			int enemy_ghost_count = 0;
			int enemy_vulture_count = 0;
			int enemy_tank_count = 0;
			int enemy_goliath_count = 0;
			int enemy_wraith_count = 0;
			int enemy_valkyrie_count = 0;
			int enemy_bc_count = 0;
			int enemy_science_vessel_count = 0;
			int enemy_dropship_count = 0;
			double enemy_army_supply = 0.0;
			double enemy_air_army_supply = 0.0;
			double enemy_ground_army_supply = 0.0;
			double enemy_ground_large_army_supply = 0.0;
			double enemy_ground_small_army_supply = 0.0;
			double enemy_weak_against_ultra_supply = 0.0;
			int enemy_static_defence_count = 0;
			for (unit*e : enemy_units) {
				if (!e->visible) continue;
				if (e->type == unit_types::marine) ++enemy_marine_count;
				if (e->type == unit_types::firebat) ++enemy_firebat_count;
				if (e->type == unit_types::ghost) ++enemy_ghost_count;
				if (e->type == unit_types::vulture) ++enemy_vulture_count;
				if (e->type == unit_types::siege_tank_tank_mode) ++enemy_tank_count;
				if (e->type == unit_types::siege_tank_siege_mode) ++enemy_tank_count;
				if (e->type == unit_types::goliath) ++enemy_goliath_count;
				if (e->type == unit_types::wraith) ++enemy_wraith_count;
				if (e->type == unit_types::valkyrie) ++enemy_valkyrie_count;
				if (e->type == unit_types::battlecruiser) ++enemy_bc_count;
				if (e->type == unit_types::science_vessel) ++enemy_science_vessel_count;
				if (e->type == unit_types::dropship) ++enemy_dropship_count;
				if (!e->type->is_worker && !e->building) {
					if (e->is_flying) enemy_air_army_supply += e->type->required_supply;
					else {
						enemy_ground_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_large) enemy_ground_large_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_small) enemy_ground_small_army_supply += e->type->required_supply;
						if (weapon_damage_against_size(e->stats->ground_weapon, unit_type::size_large) <= 12.0) enemy_weak_against_ultra_supply += e->type->required_supply;
					}
					enemy_army_supply += e->type->required_supply;
				}
				if (e->type == unit_types::missile_turret) ++enemy_static_defence_count;
				if (e->type == unit_types::photon_cannon) ++enemy_static_defence_count;
				if (e->type == unit_types::sunken_colony) ++enemy_static_defence_count;
				if (e->type == unit_types::spore_colony) ++enemy_static_defence_count;
			}

			if (!is_attacking) {

				bool attack = false;
				if (attack_count == 0) attack = true;
				if (current_frame - last_attack_end >= 15 * 30) attack = true;
				if (!my_resource_depots.empty() || (!my_workers.empty() && current_minerals >= 300)) {
					if (current_frame < 15 * 60 * 5) attack = false;
				}

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

			}

			combat::no_aggressive_groups = !is_attacking;

			for (unit*u : my_workers) {
				u->force_combat_unit = my_resource_depots.empty();
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
				if (!g.enemies.empty()) {
					has_group_with_enemies = true;
					break;
				}
			}
			log("is_attacking is %d, has_engaged_group is %d, has_group_with_enemies is %d\n", is_attacking, has_engaged_group, has_group_with_enemies);
			if (is_attacking && !has_engaged_group && !has_group_with_enemies) {

				a_vector<std::tuple<double, unit*>> units_of_interest;
				for (unit*u : live_units) {
					if (u->owner == players::my_player) continue;
					if (u->gone) continue;
					int&t = last_been_near_unit[u];
					double dist = get_best_score_value(my_units, [&](unit*u2) {
						return units_distance(u, u2);
					});
					if (dist <= 64) t = current_frame;
					int age = current_frame - t;
					if (t > 60 && age < 15 * 60 * 2) continue;
					double s = 4500.0 / (age / (15.0 * 60));
					units_of_interest.emplace_back(s, u);
				}
				log("units_of_interest.size() is %d\n", units_of_interest);

				if (!units_of_interest.empty()) {
					for (auto*a : combat::live_combat_units) {
						if (a->u->type->is_worker) continue;
						if (!a->u->stats->ground_weapon && !a->u->stats->air_weapon) continue;

						unit*target = std::get<1>(get_best_score(units_of_interest, [&](std::tuple<double, unit*> v) {
							double dist = units_pathing_distance(a->u, std::get<1>(v));
							return std::get<0>(v) * dist;
						}));

						if (target) {
							a->strategy_busy_until = current_frame + 15;
							a->action = combat::combat_unit::action_offence;
							a->subaction = combat::combat_unit::subaction_move;
							a->target_pos = target->pos;
						}
					}
				}
				 
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

			if (current_used_total_supply >= 30 && get_upgrades::no_auto_upgrades) {
				get_upgrades::set_no_auto_upgrades(false);
			}


			if (enemy_ground_small_army_supply >= 10) {
				get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
			} else {
				if (current_used_total_supply >= 100) get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
				else get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, 0.0);
			}

			if (enemy_ground_large_army_supply >= 10) {
				get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::grooved_spines, -1.0);
				get_upgrades::set_upgrade_order(upgrade_types::lurker_aspect, -11.0);
				get_upgrades::set_upgrade_order(upgrade_types::muscular_augments, -10.0);
				get_upgrades::set_upgrade_order(upgrade_types::grooved_spines, -9.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_missile_attacks_1, -1.0);
			}
			if (my_queen_count) {
				if (enemy_ground_small_army_supply >= 20) get_upgrades::set_upgrade_value(upgrade_types::ensnare, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::spawn_broodling, -1.0);
				get_upgrades::set_upgrade_order(upgrade_types::ensnare, -10.0);
				get_upgrades::set_upgrade_order(upgrade_types::spawn_broodling, -9.0);
			}

			if (my_mutalisk_count >= 4) {
				get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_1, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_2, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_3, -1.0);
				if (my_mutalisk_count >= 10) {
					get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_attacks_1, -1.0);
					get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_attacks_2, -1.0);
					get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_attacks_3, -1.0);
				}
			}

			if (!my_units_of_type[unit_types::hive].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::antennae, 0.0);
			}
			if (!my_completed_units_of_type[unit_types::hive].empty()) {
				get_upgrades::set_upgrade_order(upgrade_types::adrenal_glands, -20.0);
			}
			if (!my_completed_units_of_type[unit_types::defiler_mound].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::consume, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::plague, -1.0);
				get_upgrades::set_upgrade_order(upgrade_types::consume, -10.0);
				get_upgrades::set_upgrade_order(upgrade_types::plague, -9.0);
			}

			if (enemy_weak_against_ultra_supply >= 14 && !my_completed_units_of_type[unit_types::ultralisk_cavern].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::chitinous_plating, -1.0);
			}


			bool force_expand = (long_distance_miners() >= 1 || (int)my_workers.size() > get_max_mineral_workers() / 2) && get_next_base();
			int n_free_mineral_patches = free_mineral_patches();

			//double mineral_saturation = get_mineral_saturation();
			int max_mineral_workers = get_max_mineral_workers();
			//bool force_expand = mineral_saturation >= 0.75 && get_next_base();
			//log("free mineral patches: %d  mineral saturation: %g  max mineral workers: %d  force expand: %d\n", n_free_mineral_patches, mineral_saturation, max_mineral_workers, force_expand);
			log("long distance miners: %d  free mineral patches: %d  max mineral workers: %d  force expand: %d\n", long_distance_miners(), n_free_mineral_patches, max_mineral_workers, force_expand);

			auto build = [&](state&st) {

				std::function<bool(state&)> army = [&](state&st) {
					return false;
				};

				int drone_count = count_units_plus_production(st, unit_types::drone);
				int hatch_count = count_units_plus_production(st, unit_types::hatchery) + count_units_plus_production(st, unit_types::lair) + count_units_plus_production(st, unit_types::hive);
				int larva_count = 0;
				for (int i = 0; i < 3; ++i) {
					for (st_unit&u : st.units[i == 0 ? unit_types::hive : i == 1 ? unit_types::lair : unit_types::hatchery]) {
						larva_count += u.larva_count;
					}
				}

				int zergling_count = count_units_plus_production(st, unit_types::zergling);
				int mutalisk_count = count_units_plus_production(st, unit_types::mutalisk);
				int scourge_count = count_units_plus_production(st, unit_types::scourge);
				int hydralisk_count = count_units_plus_production(st, unit_types::hydralisk);
				int lurker_count = count_units_plus_production(st, unit_types::lurker);
				int ultralisk_count = count_units_plus_production(st, unit_types::ultralisk);
				int queen_count = count_units_plus_production(st, unit_types::queen);
				int defiler_count = count_units_plus_production(st, unit_types::defiler);
				int devourer_count = count_units_plus_production(st, unit_types::devourer);
				int guardian_count = count_units_plus_production(st, unit_types::guardian);
				int sunken_count = count_units_plus_production(st, unit_types::sunken_colony);
				int spore_count = count_units_plus_production(st, unit_types::spore_colony);

				if (my_workers.size() <= 3) return depbuild(st, state(st), unit_types::drone);

				double army_supply = 0.0;
				double air_army_supply = 0.0;
				double ground_army_supply = 0.0;
				for (auto&v : st.units) {
					unit_type*ut = v.first;
					if (!ut->is_worker) {
						army_supply += ut->required_supply;
						if (ut->is_flyer) air_army_supply += ut->required_supply;
						else ground_army_supply += ut->required_supply;
					}
				}
				for (auto&v : st.production) {
					unit_type*ut = v.second;
					if (!ut->is_worker) {
						army_supply += ut->required_supply;
						if (ut->is_flyer) air_army_supply += ut->required_supply;
						else ground_army_supply += ut->required_supply;
					}
				}

// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::hatchery, army);
// 				};
				if (larva_count < st.minerals / 50 && larva_count < 40) {
					if (count_production(st, unit_types::hatchery) < 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hatchery, army);
						};
					}
				}

				if (drone_count < max_mineral_workers) {
					army = [army](state&st) {
						return nodelay(st, unit_types::drone, army);
					};
				}

				auto tech = [&]() {
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
					if (count_units_plus_production(st, unit_types::hive) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hive, army);
						};
					} else if (!st.units[unit_types::hive].empty()) {
						if (count_units_plus_production(st, unit_types::greater_spire) == 0) {
							bool all_spires_are_busy = true;
							for (unit*u : my_completed_units_of_type[unit_types::spire]) {
								if (!u->remaining_upgrade_time) {
									all_spires_are_busy = false;
									break;
								}
							}
							army = [army](state&st) {
								return nodelay(st, unit_types::greater_spire, army);
							};
							if (all_spires_are_busy && count_units_plus_production(st, unit_types::spire) < 3) {
								army = [army](state&st) {
									return nodelay(st, unit_types::spire, army);
								};
							}
						}
						if (enemy_ground_army_supply >= 40 || army_supply >= 60 || enemy_weak_against_ultra_supply >= 14) {
							if (count_units_plus_production(st, unit_types::ultralisk_cavern) == 0) {
								army = [army](state&st) {
									return nodelay(st, unit_types::ultralisk_cavern, army);
								};
							}
						}
						if (ground_army_supply >= 14 || army_supply >= 60) {
							if (count_units_plus_production(st, unit_types::defiler_mound) == 0) {
								army = [army](state&st) {
									return nodelay(st, unit_types::defiler_mound, army);
								};
							}
						}
					}
				};

				//if (mineral_saturation >= 0.5) {
				if (drone_count >= max_mineral_workers / 2 && count_production(st, unit_types::drone)) {
					tech();
				}

				//bool build_army = is_defending || mineral_saturation >= 0.875;
				bool build_army = is_defending || drone_count >= max_mineral_workers || army_supply < drone_count;

				if (enemy_ground_army_supply >= 4.0 || drone_count >= 6) {
					if (count_units_plus_production(st, unit_types::sunken_colony) < 1) {
						army = [army](state&st) {
							return nodelay(st, unit_types::sunken_colony, army);
						};
					}
				}

				if (build_army) {
					army = [army](state&st) {
						return nodelay(st, unit_types::zergling, army);
					};
					if (count_units_plus_production(st, unit_types::hydralisk_den)) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk, army);
						};
					}
					if (count_units_plus_production(st, unit_types::spire)) {
						army = [army](state&st) {
							return nodelay(st, unit_types::mutalisk, army);
						};
					}

					if (count_units_plus_production(st, unit_types::hydralisk_den)) {
						if (hydralisk_count < enemy_ground_large_army_supply) {
							army = [army](state&st) {
								return nodelay(st, unit_types::hydralisk, army);
							};
						}
					}
					if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
						if (lurker_count * 2 < enemy_ground_small_army_supply) {
							army = [army](state&st) {
								return nodelay(st, unit_types::lurker, army);
							};
						}
						if (mutalisk_count / 2 < lurker_count) {
							army = [army](state&st) {
								return nodelay(st, unit_types::mutalisk, army);
							};
						}
					}

					if (count_units_plus_production(st, unit_types::spire)) {
						if (scourge_count < enemy_air_army_supply) {
							army = [army](state&st) {
								return nodelay(st, unit_types::scourge, army);
							};
						}
					} else {
						if (hydralisk_count < enemy_air_army_supply) {
							army = [army](state&st) {
								return nodelay(st, unit_types::hydralisk, army);
							};
						}
					}

					if (!st.units[unit_types::ultralisk_cavern].empty()) {
						if (ultralisk_count * 4 < enemy_weak_against_ultra_supply) {
							army = [army](state&st) {
								return nodelay(st, unit_types::ultralisk, army);
							};
						}
					}

					if (enemy_tank_count >= 4) {
						if (queen_count < enemy_tank_count / 4 && queen_count * 2 < army_supply / 2 && air_army_supply > queen_count * 2 + 8) {
							army = [army](state&st) {
								return nodelay(st, unit_types::queen, army);
							};
						}
					}

					if (!st.units[unit_types::defiler_mound].empty()) {
						if (defiler_count < ground_army_supply / 14) {
							army = [army](state&st) {
								return nodelay(st, unit_types::defiler, army);
							};
						}
						if (ground_army_supply < enemy_ground_army_supply && zergling_count < defiler_count * 12) {
							army = [army](state&st) {
								return nodelay(st, unit_types::zergling, army);
							};
						}
					}
					if (!st.units[unit_types::greater_spire].empty()) {
						if (enemy_air_army_supply >= 20) {
							double mult = enemy_air_army_supply > enemy_ground_army_supply ? 0.5 : 0.25;
							if (devourer_count < (int)(mutalisk_count * mult)) {
								army = [army](state&st) {
									return nodelay(st, unit_types::devourer, army);
								};
							}
						}
						if (enemy_ground_army_supply >= 20) {
							double mult = enemy_ground_army_supply > enemy_air_army_supply ? 0.5 : 0.25;
							if (guardian_count < (int)(mutalisk_count * mult)) {
								army = [army](state&st) {
									return nodelay(st, unit_types::guardian, army);
								};
							}
						}
					}

					if (army_supply >= enemy_army_supply * 2 && army_supply >= 20.0) {
						if (ground_army_supply < air_army_supply) {
							army = [army](state&st) {
								return nodelay(st, unit_types::hydralisk, army);
							};
						} else {
							army = [army](state&st) {
								return nodelay(st, unit_types::mutalisk, army);
							};
						}
						if (!st.units[unit_types::greater_spire].empty()) {
							if (guardian_count < enemy_static_defence_count) {
								army = [army](state&st) {
									return nodelay(st, unit_types::guardian, army);
								};
							}
						}
					}

				}

				//if (mineral_saturation >= 0.875) {
				if (drone_count >= max_mineral_workers / 2 + max_mineral_workers / 3 && !build_army) {
					tech();
					if (count_units_plus_production(st, unit_types::evolution_chamber) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::evolution_chamber, army);
						};
					}
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
			place_static_defence();

			multitasking::sleep(8);
		}
		

		resource_gathering::max_gas = 0.0;

	}

};


