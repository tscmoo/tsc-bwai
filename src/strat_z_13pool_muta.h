

struct strat_z_13pool_muta : strat_z_base {


	virtual void init() override {

		sleep_time = 15;

		//scouting::scout_supply = 20;

		default_upgrades = false;

	}
	bool has_cancelled_stuff = false;
	virtual bool tick() override {
		bool being_early_rushed = false;
		if (my_completed_units_of_type[unit_types::lair].empty()) {
			//if (being_rushed || enemy_gateway_count >= 2 || enemy_barracks_count >= 2) {
			if (being_rushed) {
				being_early_rushed = true;
			}
			if (current_used_total_supply - my_workers.size() == 0.0 && (enemy_army_supply >= 3.0 || enemy_attacking_army_supply)) {
				being_early_rushed = true;
			}
		}
		if (being_early_rushed) being_rushed = true;

		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() > 9) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::drone, 9);
				build::add_build_total(1.0, unit_types::overlord, 12);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(3.0, unit_types::spawning_pool);
				build::add_build_task(4.0, unit_types::extractor);
				build::add_build_task(5.0, unit_types::hatchery);
				build::add_build_task(6.0, unit_types::lair);
				build::add_build_task(7.0, unit_types::zergling);
				build::add_build_task(7.0, unit_types::zergling);

// 				build::add_build_task(5.0, unit_types::drone);
// 				build::add_build_task(5.0, unit_types::drone);
// 				build::add_build_task(6.0, unit_types::lair);

// 				build::add_build_task(7.0, unit_types::hatchery);
// 				build::add_build_task(8.0, unit_types::zergling);
// 				build::add_build_task(8.0, unit_types::zergling);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (being_rushed) bo_cancel_all();
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

// 		combat::aggressive_zerglings = true;
// 		combat::no_aggressive_groups = my_units_of_type[unit_types::mutalisk].empty();
// 		combat::aggressive_groups_done_only = false;
		combat::no_aggressive_groups = false;

		resource_gathering::max_gas = 100.0;
		if (!my_units_of_type[unit_types::lair].empty()) {
			resource_gathering::max_gas = 600.0;
		}
		army_supply = current_used_total_supply - my_workers.size();
		if (being_rushed && army_supply < enemy_army_supply + 4.0 && army_supply < 12.0) {
			if (larva_count && my_units_of_type[unit_types::sunken_colony].size() < 2) resource_gathering::max_gas = 1.0;
			max_bases = 1;
			rm_all_scouts();
			//default_upgrades = true;
			if (my_units_of_type[unit_types::sunken_colony].size() >= 2) combat::no_aggressive_groups = true;
		} else {
			max_bases = 0;
		}
		if (!my_units_of_type[unit_types::mutalisk].empty()) {
			min_bases = 3;
			max_bases = 0;
		}

		place_static_defence_only_at_main = being_early_rushed;

		if (being_early_rushed && !has_cancelled_stuff) {
			has_cancelled_stuff = true;
			log(log_level_info, "cancel stuff!\n");
			auto st = buildpred::get_my_current_state();
			for (auto&b : build::build_order) {
				if (b.type->unit && b.built_unit) {
					if (diag_distance(my_start_location - b.built_unit->pos) <= 32 * 12) continue;
					for (auto&base : st.bases) {
						if (diag_distance(base.s->pos - my_start_location) <= 32 * 12) continue;
						if (diag_distance(base.s->pos - b.built_unit->pos) <= 32 * 8) {
							b.dead = true;
							b.built_unit->game_unit->cancelConstruction();
							log(log_level_info, "cancel %s!\n", b.built_unit->type->name);
							break;
						}
					}
				}
			}
		}

		return current_used_total_supply >= 60;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		st.dont_build_refineries = count_units_plus_production(st, unit_types::extractor) && drone_count < 18;
		st.auto_build_hatcheries = true;

		auto army = this->army;

		if (mutalisk_count >= 6) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::hatchery, army);
// 			};

			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if (drone_count < 18) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if (count_units_plus_production(st, unit_types::lair) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::lair, army);
			};
		} else {
			if (count_units_plus_production(st, unit_types::spire) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::spire, army);
				};
			}
			if (drone_count < 13) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}

// 		if (!being_rushed && hatch_count < 2) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::hatchery, army);
// 			};
// 		}

		if (zergling_count < 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}

		if (is_defending && army_supply < enemy_army_supply + 2.0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}

		if (mutalisk_count && st.bases.size() == 1 && count_production(st, unit_types::hatchery) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		//if (drone_count >= 18 || (drone_count >= 8 && !st.units[unit_types::spire].empty())) {
		if (drone_count >= 18 && drone_count > army_supply) {
			army = [army](state&st) {
				return nodelay(st, unit_types::mutalisk, army);
			};
		}
		if (enemy_static_defence_count && army_supply > enemy_attacking_army_supply) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		double desired_army_supply = drone_count*drone_count * 0.015 + drone_count * 0.8 - 16;
		if ((army_supply < desired_army_supply || mutalisk_count < 4) && !st.units[unit_types::spire].empty()) {
			army = [army](state&st) {
				return nodelay(st, unit_types::mutalisk, army);
			};
		}

		//if (damaged_overlord_count && st.used_supply[race_zerg] >= st.max_supply[race_zerg] + count_production(st, unit_types::overlord) * 8 - damaged_overlord_count * 8 - 4) {
		if (damaged_overlord_count && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - damaged_overlord_count * 8 - 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::overlord, army);
			};
		}

		if (scourge_count + mutalisk_count / 2 < enemy_air_army_supply) {
			army = [army](state&st) {
				return nodelay(st, unit_types::scourge, army);
			};
		}

		if (st.units[unit_types::spire].empty()) {
			if (being_rushed && army_supply < enemy_army_supply + 4.0 && army_supply < 6.0) {
				if (drone_count >= 12) {
					army = [army](state&st) {
						return nodelay(st, unit_types::zergling, army);
					};
				}
				if (my_units_of_type[unit_types::sunken_colony].size() < 2 && current_minerals < 300) st.auto_build_hatcheries = false;
				if (sunken_count < 2) {
					army = [army](state&st) {
						return nodelay(st, unit_types::sunken_colony, army);
					};
				}
			}
		}

		if (sunken_count >= (is_defending ? 2 : 1)) {
			if (!st.units[unit_types::lair].empty() && count_units_plus_production(st, unit_types::spire) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::spire, army);
				};
			}
		}

		if (drone_count >= 16 && my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
			if (sunken_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
		}

		return army(st);
	}

};

