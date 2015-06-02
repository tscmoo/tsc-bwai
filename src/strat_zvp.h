
struct strat_zvp {

	void run() {

		using namespace buildpred;

		combat::no_aggressive_groups = false;
		get_upgrades::set_no_auto_upgrades(true);

 		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);
// 		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_2, -1.0);
// 		get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_3, -1.0);

		get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_1, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_2, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_3, -1.0);

		get_upgrades::set_upgrade_value(upgrade_types::pneumatized_carapace, -1.0);

		get_upgrades::set_upgrade_value(upgrade_types::consume, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::plague, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::consume, -10.0);
		get_upgrades::set_upgrade_order(upgrade_types::plague, -9.0);

		get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::adrenal_glands, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::adrenal_glands, -20.0);

		auto place_static_defence = [&]() {
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					if (b.build_near != combat::defence_choke.center) {
						b.build_near = combat::defence_choke.center;
						build::unset_build_pos(&b);
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
			if (free_mineral_patches >= 8 && long_distance_miners() == 0) return;

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

		bool maxed_out_aggression = false;

		while (true) {

			int my_zergling_count = my_units_of_type[unit_types::zergling].size();

			int enemy_zealot_count = 0;
			int enemy_dragoon_count = 0;
			int enemy_observer_count = 0;
			int enemy_shuttle_count = 0;
			int enemy_scout_count = 0;
			int enemy_carrier_count = 0;
			int enemy_arbiter_count = 0;
			int enemy_corsair_count = 0;
			int enemy_cannon_count = 0;
			int enemy_dt_count = 0;
			int enemy_stargate_count = 0;
			int enemy_fleet_beacon_count = 0;
			double enemy_army_supply = 0.0;
			double enemy_air_army_supply = 0.0;
			double enemy_ground_army_supply = 0.0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zealot) ++enemy_zealot_count;
				if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
				if (e->type == unit_types::observer) ++enemy_observer_count;
				if (e->type == unit_types::shuttle) ++enemy_shuttle_count;
				if (e->type == unit_types::scout) ++enemy_scout_count;
				if (e->type == unit_types::carrier) ++enemy_carrier_count;
				if (e->type == unit_types::arbiter) ++enemy_arbiter_count;
				if (e->type == unit_types::corsair) ++enemy_corsair_count;
				if (e->type == unit_types::photon_cannon) ++enemy_cannon_count;
				if (e->type == unit_types::dark_templar) ++enemy_dt_count;
				if (e->type == unit_types::stargate) ++enemy_stargate_count;
				if (e->type == unit_types::fleet_beacon) ++enemy_fleet_beacon_count;
				if (!e->type->is_worker) {
					if (e->is_flying) enemy_air_army_supply += e->type->required_supply;
					else enemy_ground_army_supply += e->type->required_supply;
					enemy_army_supply += e->type->required_supply;
				}
			}

			double my_air_army_supply = 0.0;
			double my_ground_army_supply = 0.0;
			for (unit*u : my_units) {
				if (!u->type->is_worker) {
					if (u->is_flying)  my_air_army_supply += u->type->required_supply;
					else my_ground_army_supply += u->type->required_supply;
				}
			}

			if (!is_attacking) {

				bool attack = false;
				if (attack_count == 0) attack = true;
				if (current_frame - last_attack_end >= 15 * 60 * 2) attack = true;

				if (attack) begin_attack();

			} else {

				int lose_count = 0;
				int total_count = 0;
				for (auto*a : combat::live_combat_units) {
					if (a->u->type->is_worker) continue;
					++total_count;
					if (current_frame - a->last_fight <= 15 * 10) {
						if (a->last_win_ratio < 1.0) ++lose_count;
					}
				}
				if (lose_count >= total_count / 2) {
					end_attack();
				}

			}

			combat::no_aggressive_groups = !is_attacking;

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				if (current_used_total_supply < 150.0) maxed_out_aggression = false;
			}

			//if (current_used_total_supply >= 100 && get_upgrades::no_auto_upgrades) {
			if (!my_units_of_type[unit_types::hive].empty() && get_upgrades::no_auto_upgrades) {
				get_upgrades::set_no_auto_upgrades(false);
			}

			if (enemy_zealot_count < 16) {
				if (current_used_total_supply >= 100) get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
				else get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, 0.0);
			} else {
				get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
			}

			bool force_expand = long_distance_miners() >= 1 && get_next_base();

			auto build = [&](state&st) {
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
				int guardian_count = count_units_plus_production(st, unit_types::guardian);
				int devourer_count = count_units_plus_production(st, unit_types::devourer);
				int scourge_count = count_units_plus_production(st, unit_types::scourge);
				int hydralisk_count = count_units_plus_production(st, unit_types::hydralisk);
				int lurker_count = count_units_plus_production(st, unit_types::lurker);
				int ultralisk_count = count_units_plus_production(st, unit_types::ultralisk);
				int queen_count = count_units_plus_production(st, unit_types::queen);
				int defiler_count = count_units_plus_production(st, unit_types::defiler);

				std::function<bool(state&)> army = [&](state&st) {
					return false;
				};

				if (larva_count < st.minerals / 50 && larva_count < 40) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}
				if (drone_count < 100) {
					army = [army](state&st) {
						return nodelay(st, unit_types::drone, army);
					};
				}

				if (drone_count >= 24) {
					if (count_units_plus_production(st, unit_types::evolution_chamber) == (drone_count >= 60 ? 3 : 1)) {
						army = [army](state&st) {
							return nodelay(st, unit_types::evolution_chamber, army);
						};
					}
				}

				if (my_air_army_supply < 24) {
					army = [army](state&st) {
						return nodelay(st, unit_types::mutalisk, army);
					};
					if (enemy_corsair_count > scourge_count / 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::scourge, army);
						};
					}
				}

				double army_supply = st.used_supply[race_zerg] - drone_count;
				if (army_supply < enemy_army_supply || drone_count >= 60) {
					if (my_ground_army_supply < enemy_ground_army_supply) {
						army = [army](state&st) {
							return nodelay(st, unit_types::zergling, army);
						};
					}
					army = [army](state&st) {
						return nodelay(st, unit_types::mutalisk, army);
					};
					//if (enemy_army_supply < 20 && st.gas >= st.minerals) {
					//if (enemy_army_supply < 20) {
						if (enemy_corsair_count > scourge_count / 2) {
							army = [army](state&st) {
								return nodelay(st, unit_types::scourge, army);
							};
						}
					//}
					if (hydralisk_count < mutalisk_count * 2 && my_ground_army_supply >= enemy_ground_army_supply) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk, army);
						};
					}

					if (enemy_air_army_supply > my_air_army_supply && my_ground_army_supply >= enemy_ground_army_supply) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk, army);
						};
					}
					if (army_supply >= enemy_army_supply * 2) {
						if (my_ground_army_supply < my_air_army_supply) {
							army = [army](state&st) {
								return nodelay(st, unit_types::hydralisk, army);
							};
						} else {
							army = [army](state&st) {
								return nodelay(st, unit_types::mutalisk, army);
							};
							if (enemy_cannon_count) {
								army = [army](state&st) {
									return nodelay(st, unit_types::guardian, army);
								};
							}
						}
					}
					if (hydralisk_count < guardian_count * 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk, army);
						};
					}

					if (defiler_count < my_ground_army_supply / 24) {
						army = [army](state&st) {
							return nodelay(st, unit_types::defiler, army);
						};
					}

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

				//if (army_supply >= enemy_army_supply && count_units_plus_production(st, unit_types::hive) == 0) {
				if (army_supply >= 30) {
					if (count_units_plus_production(st, unit_types::greater_spire) == 0) {
						bool all_spires_are_busy = true;
						for (unit*u : my_completed_units_of_type[unit_types::spire]) {
							if (!u->remaining_whatever_time) {
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
					if (count_units_plus_production(st, unit_types::hive) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hive, army);
						};
					}
				}

				if (army_supply > enemy_army_supply && drone_count < 100 && army_supply >= 30) {
					if (count_production(st, unit_types::drone) < 8) {
						army = [army](state&st) {
							return nodelay(st, unit_types::drone, army);
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

			place_static_defence();
			place_expos();

			multitasking::sleep(15 * 5);
		}

	}

};

