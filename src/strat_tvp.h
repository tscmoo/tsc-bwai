
struct strat_tvp {


	void run() {

		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_1, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_1, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_2, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_2, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_3, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_3, -1.0);
		
		get_upgrades::set_no_auto_upgrades(true);

		combat::no_aggressive_groups = true;
		combat::aggressive_wraiths = true;
		combat::aggressive_vultures = true;
		bool maxed_out_aggression = false;

		int pull_back_vultures_frame = 0;
		bool theyve_seen_my_wraiths = false;
		int cloaked_wraith_count = 0;

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
			int enemy_observatory_count = 0;
			int enemy_forge_count = 0;
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
				if (e->type == unit_types::observatory) ++enemy_observatory_count;
				if (e->type == unit_types::forge) ++enemy_forge_count;
			}

			int attacking_zealot_count = 0;
			int attacking_dragoon_count = 0;
			update_possible_start_locations();
			for (unit*e : enemy_units) {
				if (!e->visible) continue;
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

			if (my_marine_count / 3 + my_ghost_count / 2 + my_science_vessel_count + my_bc_count * 2 >= 18) combat::no_aggressive_groups = false;
			if (my_marine_count / 3 + my_ghost_count / 2 + my_science_vessel_count + my_bc_count * 2 < 9) combat::no_aggressive_groups = true;

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				if (current_used_total_supply < 150.0) {
					maxed_out_aggression = false;
					combat::no_aggressive_groups = true;
				}
			}

			if (my_tank_count) get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);

			if (my_bc_count) get_upgrades::set_upgrade_value(upgrade_types::yamato_gun, -1.0);

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
						return maxprod1(st, unit_types::marine);
					};

					if (st.used_supply[race_terran] >= 120 || ghost_count >= 3) {
						if (marine_count >= enemy_zealot_count * 2 + enemy_dragoon_count * 2) {
							army = [army](state&st) {
								return maxprod(st, unit_types::ghost, army);
							};
						}
					}

					if (medic_count < (marine_count + firebat_count + ghost_count) / 4) {
						army = [army](state&st) {
							return nodelay(st, unit_types::medic, army);
						};
					}

					if (ghost_count >= 14 || bc_count >= 3) {
						army = [army](state&st) {
							return maxprod(st, unit_types::battlecruiser, army);
						};
					}

					if (count_units_plus_production(st, unit_types::engineering_bay) < (st.bases.size() >= 3 ? 2 : 1)) {
						army = [army](state&st) {
							return maxprod(st, unit_types::engineering_bay, army);
						};
					}

					if (enemy_dt_count + enemy_arbiter_count && science_vessel_count < 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::science_vessel, army);
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

			execute_build(can_expand(), build);

			multitasking::sleep(15 * 10);
		}

	}

};

