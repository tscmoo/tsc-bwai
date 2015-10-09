

struct strat_z_vp_hydra : strat_z_base {

	virtual void init() override {

		scouting::scout_supply = 9.0;

		combat::no_scout_around = false;
		combat::aggressive_zerglings = true;

		default_upgrades = true;

		sleep_time = 15;

	}

	bool fight_ok = true;
	bool defence_fight_ok = true;
	int max_workers = 0;
	//bool go_mutas = false;
	int last_not_fight_ok = 0;
	int damaged_overlord_count = 0;
	bool has_added_sunken_at_third = false;
	bool was_rushed = false;
	virtual bool tick() override {

		fight_ok = eval_combat(false, 0);
		defence_fight_ok = eval_combat(true, 2);

		army_supply = current_used_total_supply - my_workers.size();

		min_bases = being_rushed ? 2 : 3;
		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);
		max_bases = maybe_being_rushed && army_supply < 24.0 && my_workers.size() < 30.0 ? 2 : 0;
		if (max_bases == 0 && my_hatch_count < 5) max_bases = 3;
		if (hydralisk_count) {
			min_bases = 3;
			max_bases = 0;
		}

// 		if (enemy_attacking_army_supply >= 12.0) was_rushed = true;
// 		if (players::opponent_player->minerals_lost >= 600.0) was_rushed = true;
// 		log(log_level_info, "was_rushed is %d\n", was_rushed);

		if (being_rushed) rm_all_scouts();

		if (my_workers.size() < 12 && (enemy_gateway_count >= 2 || enemy_army_supply)) rm_all_scouts();
		if (my_workers.size() < 12 && enemy_static_defence_count) rm_all_scouts();

		max_workers = get_max_mineral_workers() + 6;

		if (defence_fight_ok || army_supply > enemy_attacking_army_supply) {
			if (!my_completed_units_of_type[unit_types::hydralisk_den].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, -1.0);
				get_upgrades::set_upgrade_order(upgrade_types::muscular_augments, -10.0);
				if (players::my_player->has_upgrade(upgrade_types::muscular_augments)) {
					get_upgrades::set_upgrade_value(upgrade_types::grooved_spines, -1.0);
				}

				get_upgrades::set_upgrade_value(upgrade_types::pneumatized_carapace, -1.0);

				if (!my_completed_units_of_type[unit_types::lair].empty()) {
					get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
					get_upgrades::set_upgrade_order(upgrade_types::lurker_aspect, -11.0);
				}
			}
			if (!my_completed_units_of_type[unit_types::evolution_chamber].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::zerg_missile_attacks_1, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_missile_attacks_2, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_2, -1.0);
			}
		}

		//combat::aggressive_zerglings = !being_rushed || army_supply >= enemy_army_supply * 1.5;
		combat::aggressive_zerglings = !being_rushed || army_supply >= enemy_army_supply;

// 		if (my_completed_units_of_type[unit_types::hydralisk].size() >= 40 || current_used_total_supply >= 100 || my_workers.size() >= 40) {
// 			go_mutas = true;
// 		}

		//if (opponent_has_expanded || enemy_cannon_count + enemy_bunker_count) min_bases = 3;

		if (!fight_ok) last_not_fight_ok = current_frame;
		if (current_frame - last_not_fight_ok >= 15 * 60) attack_interval = 15 * 60;
		else attack_interval = 0;
		if (army_supply < enemy_army_supply + 15.0) attack_interval = 0;

		if (players::opponent_player->race != race_zerg) {
			if (enemy_attacking_army_supply >= 8.0) {
				combat::no_aggressive_groups = !is_defending;
				combat::aggressive_groups_done_only = false;
				combat::aggressive_zerglings = false;
			}
			if ((maybe_being_rushed || was_rushed) && army_supply > enemy_army_supply) {
				combat::no_aggressive_groups = false;
				combat::aggressive_groups_done_only = false;
			}
		}

		//if ((being_rushed || enemy_barracks_count >= 2 || enemy_marine_count >= 6) && !defence_fight_ok && sunken_count == 0) {
		if ((being_rushed || (maybe_being_rushed && completed_hatch_count < 2 && (enemy_barracks_count >= 2 || enemy_marine_count >= 4))) && !defence_fight_ok && sunken_count == 0) {
			//place_static_defence_only_at_main = true;
			call_place_static_defence = false;
		} else if (sunken_count >= 2) {
			call_place_static_defence = true;
		}

		damaged_overlord_count = 0;
		for (unit*u : my_completed_units_of_type[unit_types::overlord]) {
			if (u->hp < u->stats->hp) ++damaged_overlord_count;
		}

		resource_gathering::max_gas = 100.0;
		if (my_workers.size() >= 14) resource_gathering::max_gas = 250.0;
		if (my_workers.size() >= 30) resource_gathering::max_gas = 500.0;

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
		
		return current_used_total_supply >= 125;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		st.auto_build_hatcheries = true;
		st.dont_build_refineries = drone_count < 26 || (drone_count < 35 && count_units_plus_production(st, unit_types::extractor) >= 2);

		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);

		double desired_army_supply = drone_count*drone_count * 0.015 + drone_count * 0.8 - 16;
		if (players::opponent_player->race == race_zerg) {
			desired_army_supply = drone_count*drone_count * 0.08 + drone_count * 0.5 - 4;
		}

		if (army_supply >= desired_army_supply || drone_count < 32) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		} else {
			army = [army](state&st) {
				return nodelay(st, unit_types::hydralisk, army);
			};
		}

		if (drone_count >= 15 && count_units_plus_production(st, unit_types::extractor) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::extractor, army);
			};
		}

		if (hatch_count < 3) {
			if (!maybe_being_rushed || (sunken_count && defence_fight_ok)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hatchery, army);
				};
			}
		}
		if (drone_count >= 26 && defence_fight_ok) {
			if (hatch_count < 5) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hatchery, army);
				};
			}
		}

		if (drone_count >= 36 + enemy_static_defence_count * 3) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hydralisk, army);
			};
			if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
				if (lurker_count * 2 < enemy_ground_small_army_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lurker, army);
					};
				}
				if (lurker_count < hydralisk_count / 2 && hydralisk_count > enemy_air_army_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lurker, army);
					};
				}
			}
		}
		if (drone_count >= 32 && army_supply < (drone_count >= 48 ? enemy_army_supply : enemy_attacking_army_supply)) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hydralisk, army);
			};
		}
		if (drone_count >= 28 && army_supply < 12.0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hydralisk, army);
			};
		}
		if (drone_count >= 60 && hydralisk_count < 40 && count_production(st, unit_types::drone)) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hydralisk, army);
			};
		}

		if (was_rushed && drone_count >= 20 && enemy_static_defence_count == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hydralisk, army);
			};
		}


		if (drone_count >= 30 && defence_fight_ok) {
			if (count_units_plus_production(st, unit_types::evolution_chamber) < (army_supply >= 16 ? 2 : 1)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::evolution_chamber, army);
				};
			}
			if (count_units_plus_production(st, unit_types::lair) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::lair, army);
				};
			}
		}

		if (drone_count >= 20 && defence_fight_ok) {
			if (count_units_plus_production(st, unit_types::lair) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::lair, army);
				};
			}
		}

		if (is_defending && !defence_fight_ok) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
			if (drone_count >= 22) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
		}


		if (players::opponent_player->race != race_zerg || drone_count >= 18) {
			if (my_completed_units_of_type[unit_types::hatchery].size() >= 2 && (maybe_being_rushed || enemy_army_supply >= 4.0) && sunken_count < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}

		}

		if (players::opponent_player->race == race_zerg && (army_supply < enemy_army_supply || army_supply < 12.0)) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}

		if (enemy_reaver_count || enemy_shuttle_count) {
			if (hydralisk_count < 12) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
		}
		if (drone_count < 24 && (enemy_shuttle_count || enemy_robotics_facility_count) && enemy_reaver_count == 0) {
			if (army_supply < 8.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
		}

		if ((is_defending && !fight_ok && zergling_count < 14) || (drone_count >= 18 && sunken_count >= 2 && maybe_being_rushed && zergling_count < 14 && players::opponent_player->minerals_lost < 250.0)) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}

		if (army_supply < 1.0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}

		if (damaged_overlord_count && st.used_supply[race_zerg] >= st.max_supply[race_zerg] + count_production(st, unit_types::overlord) * 8 - damaged_overlord_count * 8 - 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::overlord, army);
			};
		}

		double anti_air_supply = enemy_air_army_supply;
		if (defence_fight_ok && drone_count >= 26) anti_air_supply += 2;
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

		if (army_supply >= desired_army_supply) {
			if (defence_fight_ok || (army_supply >= 30.0 && !is_defending)) {
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
	}

};

