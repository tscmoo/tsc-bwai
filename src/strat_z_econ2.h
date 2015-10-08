

struct strat_z_econ2 : strat_z_base {

	virtual void init() override {

		scouting::scout_supply = 9.0;

// 		combat::no_scout_around = true;
// 		combat::aggressive_zerglings = false;
		combat::no_scout_around = false;
		combat::aggressive_zerglings = true;

		default_upgrades = false;

	}

	bool fight_ok = true;
	bool defence_fight_ok = true;
	int max_workers = 0;
	bool has_added_sunken_at_third = false;
	int damaged_overlord_count = 0;
	bool has_made_9_mutas = false;
	double spire_progress = 0.0;
	bool do_ling_backstab = false;
	bool has_lings_for_backstab = false;
	bool has_done_ling_backstab = false;
	virtual bool tick() override {

		fight_ok = eval_combat(false, 0);
		defence_fight_ok = eval_combat(true, 2);

		min_bases = 2;

		if (being_rushed) rm_all_scouts();

		if (army_supply < enemy_army_supply && is_defending) {
			rm_all_scouts();
			max_bases = 2;
		} else {
			max_bases = 0;
		}

		max_workers = get_max_mineral_workers() + 6;

		attack_interval = 15 * 30;
		if ((enemy_goliath_count || enemy_tank_count || enemy_bunker_count) && current_used_total_supply - my_workers.size() < enemy_army_supply + enemy_tank_count * 8.0) {
			if (enemy_army_supply || current_used_total_supply > 20) {
				attack_interval = 0;
			}
		}

		if (my_workers.size() >= 16 && enemy_attacking_army_supply >= 2.0) {
			if (enemy_proxy_building_count || enemy_vulture_count || enemy_tank_count) {
				combat::no_aggressive_groups = false;
				combat::aggressive_groups_done_only = false;
			}
		}

		if (current_used_total_supply >= 60) default_upgrades = true;

		if (!my_completed_units_of_type[unit_types::evolution_chamber].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);
			//get_upgrades::set_upgrade_value(upgrade_types::zerg_melee_attacks_1, -1.0);
			get_upgrades::set_upgrade_order(upgrade_types::zerg_carapace_1, -10.0);
		}
		if (my_units_of_type[unit_types::zergling].size() >= 10) {
			get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
		}

		if (has_made_9_mutas || !my_units_of_type[unit_types::hydralisk_den].empty()) {
			if (!my_completed_units_of_type[unit_types::lair].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
				get_upgrades::set_upgrade_order(upgrade_types::lurker_aspect, -11.0);
			}
		}

		// 		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);
		// 
		// 		if (maybe_being_rushed && !my_units_of_type[unit_types::sunken_colony].empty()) {
		// 			if (is_defending && !has_lings_for_backstab) {
		// 				do_ling_backstab = false;
		// 			} else if (enemy_marine_count >= 4 || enemy_barracks_count >= 2) {
		// 
		// 				if (!has_done_ling_backstab) {
		// 					rm_all_scouts();
		// 
		// 					has_lings_for_backstab = my_units_of_type[unit_types::zergling].size() >= 12;
		// 
		// 					auto path = combat::find_bigstep_path(unit_types::zergling, my_start_location, combat::op_closest_base, square_pathing::pathing_map_index::no_enemy_buildings);
		// 					if (path.size() > 8) {
		// 						xy a = path[path.size() / 2];
		// 						xy b = path[path.size() / 2 + 4];
		// 						xy rel = b - a;
		// 						double ang = std::atan2(rel.y, rel.x);
		// 						ang += PI / 2;
		// 						xy origin = path[path.size() / 2 + 2];
		// 						auto look = [&](double mult) {
		// 							xy pos = origin;
		// 							for (double dist = 0; dist < 32 * 20; dist += 32) {
		// 								xy r = origin;
		// 								r.x += (int)(std::cos(ang)*dist*mult);
		// 								r.y += (int)(std::sin(ang)*dist*mult);
		// 								if (unit_pathing_distance(unit_types::zergling, origin, r) >= 32 * 40) break;
		// 								pos = r;
		// 							}
		// 							return square_pathing::get_nearest_node_pos(unit_types::zergling, pos);
		// 						};
		// 						xy left = look(-1);
		// 						xy right = look(1);
		// 
		// 						xy rally_pos;
		// 						if (diag_distance(left - origin) > diag_distance(right - origin)) rally_pos = left;
		// 						else rally_pos = right;
		// 
		// 						do_ling_backstab = true;
		// 
		// 						combat::my_closest_base_override_until = current_frame + 15 * 20;
		// 						combat::my_closest_base_override = rally_pos;
		// 						combat::defence_choke_use_nearest_until = 0;
		// 
		// 						combat::no_aggressive_groups = true;
		// 						combat::aggressive_zerglings = false;
		// 					}
		// 				}
		// 				if (has_lings_for_backstab && is_defending) {
		// 					has_done_ling_backstab = true;
		// 					combat::no_aggressive_groups = false;
		// 					combat::aggressive_groups_done_only = false;
		// 				}
		// 			}
		// 		}

		if (my_units_of_type[unit_types::mutalisk].size() >= 9) {
			has_made_9_mutas = true;
			//combat::no_aggressive_groups = enemy_attacking_army_supply < 6.0 && (army_supply < enemy_army_supply || enemy_static_defence_count >= 5);
		}
		//combat::aggressive_mutalisks = current_used_total_supply < 90;
		//if (has_made_9_mutas) {
		if (!my_units_of_type[unit_types::mutalisk].empty()) {
			combat::no_aggressive_groups = current_used_total_supply >= 90;
			if (enemy_static_defence_count >= 5 || enemy_goliath_count >= 6) combat::no_aggressive_groups = true;
			combat::aggressive_groups_done_only = false;
		}

		if (!has_added_sunken_at_third && defence_fight_ok) {
			auto st = buildpred::get_my_current_state();
			std::sort(st.bases.begin(), st.bases.end(), [&](const buildpred::st_base&a, const buildpred::st_base&b) {
				return unit_pathing_distance(unit_types::drone, my_start_location, a.s->cc_build_pos) < unit_pathing_distance(unit_types::drone, my_start_location, b.s->cc_build_pos);
			});
			if (st.bases.size() >= 3) {
				auto&third = st.bases[2];
				bool done = false;
				for (unit*u : my_resource_depots) {
					if (u->building->build_pos == third.s->cc_build_pos) {
						done = u->is_completed;
						break;
					}
				}
				if (done) {
					int gatherers = 0;
					for (auto&s : third.s->resources) {
						resource_gathering::resource_t*res = nullptr;
						for (auto&s2 : resource_gathering::live_resources) {
							if (s2.u == s.u) {
								res = &s2;
								break;
							}
						}
						if (res) {
							gatherers += res->gatherers.size();
						}
					}
					if (gatherers >= 4) {
						auto*t = build::add_build_task(-1.0, unit_types::creep_colony);
						t->build_near = third.s->cc_build_pos;
						build::add_build_task(-1.0, unit_types::sunken_colony);
						has_added_sunken_at_third = true;
					}
				}
			}
		}

		spire_progress = 0.0;
		for (unit*u : my_units_of_type[unit_types::spire]) {
			double p = 1.0 - (double)u->remaining_build_time / u->type->build_time;
			if (p > spire_progress) spire_progress = p;
		}

		if (my_units_of_type[unit_types::lair].empty()) resource_gathering::max_gas = 100;
		else resource_gathering::max_gas = 300;
		if (spire_progress) resource_gathering::max_gas = 900;

		damaged_overlord_count = 0;
		for (unit*u : my_completed_units_of_type[unit_types::overlord]) {
			if (u->hp < u->stats->hp) ++damaged_overlord_count;
		}
		
		//return current_used_total_supply >= 60;
		return my_workers.size() >= 45 || current_used_total_supply >= 80;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		// 		if (sunken_count < 6) {
		// 			army = [army](state&st) {
		// 				return nodelay(st, unit_types::sunken_colony, army);
		// 			};
		// 		}

		st.auto_build_hatcheries = true;
		st.dont_build_refineries = drone_count < 20 && count_units_plus_production(st, unit_types::extractor);

		if (spire_progress >= 0.5 && (spire_progress < 1.0 || my_units_of_type[unit_types::mutalisk].empty()) && !has_made_9_mutas) {
			if (drone_count < 26 && spire_progress < 1.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
			army = [army](state&st) {
				return nodelay(st, unit_types::mutalisk, army);
			};
			return army(st);
		}

		if (drone_count < max_workers) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
			// 			if (drone_count >= 11 && hatch_count < 3) {
			// 				army = [army](state&st) {
			// 					return nodelay(st, unit_types::hatchery, army);
			// 				};
			// 			}
		} else {
			// 			st.auto_build_hatcheries = true;
			// 
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};

			if (defence_fight_ok && count_production(st, unit_types::drone) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}

		if (drone_count >= 26 && zergling_count < 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}

		if (opponent_has_expanded || enemy_bunker_count + enemy_cannon_count) {
			if (enemy_proxy_building_count == 0) {
				if (drone_count >= 11 && hatch_count < 3) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}
			}
		}

		if (!my_units_of_type[unit_types::mutalisk].empty()) {
			if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk_den, army);
				};
			}
		}

		if (drone_count >= std::min(max_workers, 34)) {
			if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk_den, army);
				};
			}
			if (count_units_plus_production(st, unit_types::evolution_chamber) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::evolution_chamber, army);
				};
			}
			if (count_units_plus_production(st, unit_types::extractor) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
		}

		if (drone_count >= 13 && defence_fight_ok) {
			if (count_units_plus_production(st, unit_types::extractor) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
		}
		if (!st.units[unit_types::lair].empty()) {
			if (drone_count >= 16 && count_units_plus_production(st, unit_types::spire) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::spire, army);
				};
			}
		}
		if (!st.units[unit_types::spire].empty()) {
			if (!has_made_9_mutas) {
				army = [army](state&st) {
					return nodelay(st, unit_types::mutalisk, army);
				};
			}
		}

		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);

		if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2 && drone_count >= 11) {
			if (!defence_fight_ok && sunken_count < 5) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
			if (maybe_being_rushed && sunken_count < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
				if (sunken_count == 0) return army(st);
			}
			if (enemy_marine_count >= 6 || enemy_barracks_count >= 2) {
				if (drone_count >= 20 && sunken_count < 4) {
					army = [army](state&st) {
						return nodelay(st, unit_types::sunken_colony, army);
					};
				}
			}
		}

		if (drone_count >= 16 && (defence_fight_ok || sunken_count)) {
			if (count_units_plus_production(st, unit_types::lair) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::lair, army);
				};
			}
		}

		//if (maybe_being_rushed && sunken_count) {
// 		if (do_ling_backstab && !has_lings_for_backstab) {
// 			if (zergling_count < 12) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::zergling, army);
// 				};
// 			}
// 		}

		bool build_army = (is_defending && army_supply < enemy_army_supply) || (being_rushed && army_supply < enemy_attacking_army_supply);
		if (drone_count >= 28 && army_supply < 8.0) build_army = true;
		if (maybe_being_rushed && drone_count >= 24 && army_supply < 12.0) build_army = true;
		if (sunken_count == 0 && !defence_fight_ok) {
			if (is_defending || count_production(st, unit_types::drone)) build_army = true;
		}
		if (!being_rushed) {
			if (army_supply > enemy_attacking_army_supply) build_army = false;
			if (is_defending && army_supply < enemy_army_supply) build_army = true;
			if (drone_count < 26 && defence_fight_ok) build_army = false;
		}

		if (players::opponent_player->race == race_zerg && army_supply < (being_rushed ? drone_count >= 16.0 ? 20.0 : 12.0 : 8.0)) build_army = true;

		if (build_army) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}

		if (drone_count >= 16 && enemy_attacking_army_supply >= 2.0) {
			if (enemy_proxy_building_count || enemy_vulture_count || enemy_tank_count) {
				if (army_supply < drone_count && hydralisk_count < enemy_attacking_army_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk, army);
					};
				}
			}
			if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
				if (lurker_count < 4 || lurker_count * 2 < enemy_ground_small_army_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lurker, army);
					};
				}
			}
		}

		if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
			if (enemy_marine_count >= 6) {
				if (lurker_count < 4) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lurker, army);
					};
				}
			}
		}

		if (drone_count >= 30) {
			if (count_units_plus_production(st, unit_types::hive) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hive, army);
				};
			}
		}

		if (!my_completed_units_of_type[unit_types::hive].empty() && drone_count >= 35) {
			if (defiler_count && zergling_count < 40) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
			if (defiler_count < 4) {
				army = [army](state&st) {
					return nodelay(st, unit_types::defiler, army);
				};
			}
			if (enemy_ground_small_army_supply >= (enemy_army_supply - enemy_vulture_count) * 0.75) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			} else {
				army = [army](state&st) {
					return nodelay(st, unit_types::lurker, army);
				};
			}
		}

		if (damaged_overlord_count && st.used_supply[race_zerg] >= st.max_supply[race_zerg] + count_production(st, unit_types::overlord) * 8 - damaged_overlord_count * 8 - 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::overlord, army);
			};
		}

		double anti_air_supply = enemy_air_army_supply;
		if (defence_fight_ok && drone_count >= 40) anti_air_supply += 2;
		if (st.units[unit_types::spire].empty()) {
			if (hydralisk_count < anti_air_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
		} else {
			if (scourge_count < anti_air_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scourge, army);
				};
			}
		}

		if (force_expand && count_production(st, unit_types::hatchery) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		return army(st);
	}

};

