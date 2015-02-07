
struct strat_tvz {

	wall_in::wall_builder wall;

	void run() {

		combat::no_aggressive_groups = true;

		get_upgrades::no_auto_upgrades = true;

		bool maxed_out_aggression = false;
		int wall_calculated = 0;
		bool has_wall = false;
		bool has_zergling_tight_wall = false;
		while (true) {

			using namespace buildpred;

			int my_vulture_count = my_units_of_type[unit_types::vulture].size();
			int my_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();
			int my_goliath_count = my_units_of_type[unit_types::goliath].size();
			int my_marine_count = my_units_of_type[unit_types::marine].size();
			int my_firebat_count = my_units_of_type[unit_types::firebat].size();
			int my_ghost_count = my_units_of_type[unit_types::ghost].size();
			int my_science_vessel_count = my_units_of_type[unit_types::science_vessel].size();
			int my_wraith_count = my_units_of_type[unit_types::wraith].size();
			int my_valkyrie_count = my_units_of_type[unit_types::valkyrie].size();
			int my_bc_count = my_units_of_type[unit_types::battlecruiser].size();

			int enemy_zergling_count = 0;
			int enemy_mutalisk_count = 0;
			int enemy_guardian_count = 0;
			int enemy_lurker_count = 0;
			int enemy_hydralisk_count = 0;
			int enemy_hydralisk_den_count = 0;
			int enemy_lair_count = 0;
			int enemy_spire_count = 0;
			int enemy_spore_count = 0;
			int enemy_hatch_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zergling) ++enemy_zergling_count;
				if (e->type == unit_types::mutalisk) ++enemy_mutalisk_count;
				if (e->type == unit_types::guardian || e->type == unit_types::cocoon) ++enemy_guardian_count;
				if (e->type == unit_types::lurker || e->type == unit_types::lurker_egg) ++enemy_lurker_count;
				if (e->type == unit_types::hydralisk) ++enemy_hydralisk_count;
				if (e->type == unit_types::hydralisk_den) ++enemy_hydralisk_den_count;
				if (e->type == unit_types::lair) ++enemy_lair_count;
				if (e->type == unit_types::spire || e->type==unit_types::greater_spire) ++enemy_spire_count;
				if (e->type == unit_types::spore_colony) ++enemy_spore_count;
				if (e->type == unit_types::hatchery || e->type == unit_types::lair || e->type == unit_types::hive) ++enemy_hatch_count;
			}
			if ((enemy_hydralisk_count || enemy_lurker_count) && !enemy_hydralisk_den_count) ++enemy_hydralisk_den_count;
			if (enemy_mutalisk_count && !enemy_spire_count) ++enemy_spire_count;

			if (my_marine_count / 3 + my_ghost_count / 2 + my_science_vessel_count + my_bc_count * 2 >= 18) combat::no_aggressive_groups = false;
			if (my_marine_count / 3 + my_ghost_count / 2 + my_science_vessel_count + my_bc_count * 2 < 9) combat::no_aggressive_groups = true;

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				if (current_used_total_supply < 100.0) maxed_out_aggression = false;
			}

			if (enemy_spire_count + enemy_mutalisk_count && my_valkyrie_count + my_marine_count / 7 >= 4) {
				combat::build_missile_turret_count = 2;
			}

			bool no_auto_upgrades = get_upgrades::no_auto_upgrades;

			if (current_used_total_supply >= 80) no_auto_upgrades = false;

			if (my_science_vessel_count) get_upgrades::set_upgrade_value(upgrade_types::irradiate, -1.0);
			if (my_tank_count) get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);

			bool lurkers_are_coming = enemy_mutalisk_count == 0 && (enemy_lurker_count || (enemy_hydralisk_den_count && enemy_lair_count)) && my_science_vessel_count == 0 && my_bc_count == 0;

			combat::build_bunker_count = 0;
			if (current_used_total_supply >= 120) no_auto_upgrades = false;

			if (no_auto_upgrades != get_upgrades::no_auto_upgrades) get_upgrades::set_no_auto_upgrades(no_auto_upgrades);

			combat::aggressive_wraiths = my_wraith_count >= 20;
			combat::aggressive_valkyries = false;

			if (my_wraith_count > 2) get_upgrades::set_upgrade_value(upgrade_types::cloaking_field, -1.0);

			double enemy_army_supply = 0;
			for (unit*u : enemy_units) {
				if (current_frame - u->last_seen >= 15 * 6 || u->type->is_worker) continue;
				enemy_army_supply += u->type->required_supply;
			}
			bool go_nukes = enemy_army_supply < 60 && current_used_total_supply >= 150;
			
			auto build = [&](state&st) {
				return nodelay(st, unit_types::scv, [&](state&st) {

					int vulture_count = count_units_plus_production(st, unit_types::vulture);
					int tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
					int goliath_count = count_units_plus_production(st, unit_types::goliath);
					int valkyrie_count = count_units_plus_production(st, unit_types::valkyrie);
					int wraith_count = count_units_plus_production(st, unit_types::wraith);
					int marine_count = count_units_plus_production(st, unit_types::marine);
					int firebat_count = count_units_plus_production(st, unit_types::firebat);
					int medic_count = count_units_plus_production(st, unit_types::medic);
					int ghost_count = count_units_plus_production(st, unit_types::ghost);
					int science_vessel_count = count_units_plus_production(st, unit_types::science_vessel);
					int bc_count = count_units_plus_production(st, unit_types::battlecruiser);


					std::function<bool(state&)> army = [&](state&st) {
						return depbuild(st, state(st), unit_types::marine);
					};

					army = [army](state&st) {
						return nodelay(st, unit_types::vulture, army);
					};

					army = [army](state&st) {
						return maxprod(st, unit_types::ghost, army);
					};

					army = [army](state&st) {
						return nodelay(st, unit_types::science_vessel, army);
					};

					if (ghost_count >= 14 || bc_count >= 3) {
						army = [army](state&st) {
							return maxprod(st, unit_types::battlecruiser, army);
						};
					}

					if (medic_count < (marine_count + ghost_count) / 7) {
						army = [army](state&st) {
							return maxprod(st, unit_types::medic, army);
						};
					}

					if (tank_count < (enemy_hydralisk_count + enemy_lurker_count) / 4) {
						army = [army](state&st) {
							return nodelay(st, unit_types::siege_tank_tank_mode, army);
						};
					}

					if (go_nukes) {
						if (count_units_plus_production(st, unit_types::nuclear_missile) < 1) {
							army = [army](state&st) {
								return nodelay(st, unit_types::nuclear_missile, army);
							};
						}
					}

					return army(st);
				});
			};

			auto long_distance_miners = [&]() {
				int count = 0;
				for (auto&g : resource_gathering::live_gatherers) {
					if (!g.resource) continue;
					unit*ru = g.resource->u;
					resource_spots::spot*rs = nullptr;
					for (auto&s : resource_spots::spots) {
						if (grid::get_build_square(s.cc_build_pos).building) continue;
						for (auto&r : s.resources) {
							if (r.u == ru) {
								rs = &s;
								break;
							}
						}
						if (rs) break;
					}
					if (rs) ++count;
				}
				return count;
			};
			auto can_expand = [&]() {
				return long_distance_miners() >= 4;
			};
			log("there are %d long distance miners\n", long_distance_miners());

			execute_build(can_expand(), build);


			multitasking::sleep(15 * 5);
		}

	}

	void render() {
		wall.render();
	}

};

