

struct strat_z_9pool_speed_into_1hatch_spire : strat_z_base {

	virtual void init() override {

		scouting::scout_supply = 14.0;
		scouting::no_proxy_scout = true;

		combat::no_scout_around = true;
		combat::aggressive_zerglings = false;

		default_upgrades = false;

	}

	bool fight_ok = true;
	int max_workers = 0;
	double spire_progress = 0.0;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(1.0, unit_types::spawning_pool);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(3.0, unit_types::extractor);
				build::add_build_task(4.0, unit_types::overlord);
				build::add_build_task(5.0, unit_types::drone);
				build::add_build_task(6.0, unit_types::zergling);
				build::add_build_task(6.0, unit_types::zergling);
				build::add_build_task(6.0, unit_types::zergling);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		if (!my_units_of_type[unit_types::spire].empty()) resource_gathering::max_gas = 500;
		else if (!my_units_of_type[unit_types::lair].empty()) resource_gathering::max_gas = 150;
		else resource_gathering::max_gas = 100.0;

		fight_ok = eval_combat(false, 0);

		double my_army_supply = current_used_total_supply - my_workers.size();
		//if (my_army_supply > enemy_army_supply + 4.0 && fight_ok && !enemy_buildings.empty()) attack_interval = 15 * 30;
		if (my_army_supply > enemy_army_supply + 4.0 && fight_ok) attack_interval = 15 * 30;
		else attack_interval = 0;

		min_bases = 2;

		if (army_supply < 8.0) {
			double visible_enemy_supply = 0.0;
			for (unit*e : visible_enemy_units) {
				visible_enemy_supply += e->type->required_supply;
			}
			if (visible_enemy_supply) rm_all_scouts();
			else {
				int n = 2 - scouting::all_scouts.size();
				for (unit*u : my_completed_units_of_type[unit_types::zergling]) {
					if (n <= 0) break;
					scouting::add_scout(u);
				}
			}
		}

		if (!my_units_of_type[unit_types::mutalisk].empty() && fight_ok) {
			combat::no_aggressive_groups = false;
			combat::aggressive_groups_done_only = false;
		}
		if (army_supply > enemy_army_supply) {
			combat::no_aggressive_groups = false;
			combat::aggressive_groups_done_only = false;
		}

		for (auto&v : scouting::all_scouts) {
			if (v.scout_unit && v.scout_unit->type->is_worker) {
				rm_all_scouts();
				break;
			}
		}

		max_workers = get_max_mineral_workers() + 6;

		if (my_units_of_type[unit_types::mutalisk].empty()) {
			combat::combat_mult_override = 1.0;
			combat::combat_mult_override_until = current_frame + 15 * 10;
		}

		spire_progress = 0.0;
		for (unit*u : my_units_of_type[unit_types::spire]) {
			double p = 1.0 - (double)u->remaining_build_time / u->type->build_time;
			if (p > spire_progress) spire_progress = p;
		}

		if (my_zergling_count) get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
		
		return current_used_total_supply >= 60;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		//st.auto_build_hatcheries = spire_progress >= 0.75;
		st.auto_build_hatcheries = spire_progress > 0.0;

		if (spire_progress >= 0.5 && mutalisk_count == 0 && st.gas >= 100 && st.minerals >= 100) {
			army = [army](state&st) {
				return nodelay(st, unit_types::mutalisk, army);
			};
			return army(st);
		}

		if (count_units_plus_production(st, unit_types::spire) && !fight_ok) {
			if (sunken_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
		}

		army = [army](state&st) {
			return nodelay(st, unit_types::zergling, army);
		};

		if (zergling_count >= 14 && drone_count < 12 && army_supply >= enemy_army_supply) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if (spire_progress >= 0.5) {
			army = [army](state&st) {
				return nodelay(st, unit_types::mutalisk, army);
			};
			if (scourge_count < enemy_spire_count + enemy_air_army_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scourge, army);
				};
			}
		}

		//if (fight_ok && !combat::no_aggressive_groups && army_supply > enemy_army_supply && count_production(st, unit_types::drone) == 0) {
		if (army_supply > drone_count && fight_ok && army_supply > enemy_army_supply && count_production(st, unit_types::drone) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if (count_units_plus_production(st, unit_types::lair) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::lair, army);
			};
		} else if (!st.units[unit_types::lair].empty())  {
			if (count_units_plus_production(st, unit_types::spire) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::spire, army);
				};
			}
		}

		return army(st);
	}

};

