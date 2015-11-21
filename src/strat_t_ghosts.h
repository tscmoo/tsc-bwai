
struct strat_t_ghosts : strat_t_base {


	virtual void init() override {

		sleep_time = 15;

		get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::personal_cloaking, -1.0);

	}

	xy natural_pos;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::scv, 9);
				build::add_build_total(1.0, unit_types::supply_depot, 1);
				build::add_build_total(2.0, unit_types::scv, 10);
				build::add_build_task(3.0, unit_types::barracks);
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		if (being_rushed) rm_all_scouts();

		if (my_completed_units_of_type[unit_types::marine].size() >= 1) {
			if (natural_pos == xy()) {
				natural_pos = get_next_base()->pos;
			}
			if (players::opponent_player->race != race_zerg || my_completed_units_of_type[unit_types::marine].size() >= 5) {
				combat::build_bunker_count = my_completed_units_of_type[unit_types::marine].size() >= 12 ? 3 : 1;
				if (players::opponent_player->race == race_terran) {
// 					combat::build_bunker_count = 1;
// 					if (enemy_tank_count) combat::build_bunker_count = 0;
					combat::build_bunker_count = 0;
				}
			} else {
				if (being_rushed) combat::force_defence_is_scared_until = current_frame + 15 * 10;
			}
		}
		if (natural_pos != xy() && !being_rushed) {
			combat::my_closest_base_override = natural_pos;
			combat::my_closest_base_override_until = current_frame + 15 * 10;
		}

		combat::no_aggressive_groups = true;
		combat::aggressive_groups_done_only = true;
		if (my_completed_units_of_type[unit_types::ghost].size() >= 10 || my_completed_units_of_type[unit_types::nuclear_missile].size() >= 4) {
			combat::no_aggressive_groups = false;
			combat::aggressive_groups_done_only = false;
		}

		if (players::opponent_player->race == race_terran && enemy_attacking_army_supply >= 2.0) {
			combat::no_aggressive_groups = false;
			combat::aggressive_groups_done_only = false;
		}

		if (current_used_total_supply - my_workers.size() >= 20 || enemy_attacking_army_supply >= 8.0) {
			for (auto*a : combat::live_combat_units) {
				if (!a->u->type->is_flyer && !a->u->type->is_worker) a->use_for_drops_until = current_frame + 15 * 10;
			}
		}

		if (my_units_of_type[unit_types::comsat_station].size() >= 2) scouting::comsat_supply = 400.0;

		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		st.dont_build_refineries = false;

		army = [army](state&st) {
			return maxprod(st, unit_types::marine, army);
		};

		if (marine_count >= 1 && count_units_plus_production(st, unit_types::academy) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::academy, army);
			};
		}

		if (army_supply >= 9.0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::ghost, army);
			};
		}

		if (medic_count < (marine_count + ghost_count) / 8) {
			army = [army](state&st) {
				return nodelay(st, unit_types::medic, army);
			};
		}

		if (army_supply >= 20.0 && count_units_plus_production(st, unit_types::dropship) < (marine_count + ghost_count * 4) / 8) {
			if (army_supply > 80.0 || count_units_plus_production(st, unit_types::dropship) < 3) {
				army = [army](state&st) {
					return nodelay(st, unit_types::dropship, army);
				};
			}
		}

		if ((enemy_tank_count || players::opponent_player->race == race_terran) && enemy_goliath_count == 0) {
			if (wraith_count < 4) {
				army = [army](state&st) {
					return nodelay(st, unit_types::wraith, army);
				};
			}
		}

		if (count_units_plus_production(st, unit_types::nuclear_missile) < ghost_count / 4) {
// 			army = [army](state&st) {
// 				return maxprod(st, unit_types::nuclear_missile, army);
// 			};
			if (my_units_of_type[unit_types::nuclear_missile].size() == my_completed_units_of_type[unit_types::nuclear_silo].size()) {
				army = [army](state&st) {
					return maxprod(st, unit_types::nuclear_silo, army);
				};
			}
			if (my_units_of_type[unit_types::nuclear_missile].size() < my_completed_units_of_type[unit_types::nuclear_silo].size()) {
				return depbuild(st, state(st), unit_types::nuclear_missile);
			}
		}

		if (scv_count < 60) {
			army = [army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		if ((players::opponent_player->race == race_zerg || being_rushed) && marine_count < enemy_army_supply + 4) {
			army = [army](state&st) {
				return maxprod(st, unit_types::marine, army);
			};
		}
		if (players::opponent_player->race == race_terran) {
			//if (enemy_tank_count || enemy_static_defence_count || opponent_has_expanded) {
			if (!being_rushed && marine_count >= 2) {
				if (can_expand && st.bases.size() < 3) {
					army = [army](state&st) {
						return nodelay(st, unit_types::cc, army);
					};
				}
			}
		}

		if (force_expand && count_production(st, unit_types::cc) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::cc, army);
			};
		}

		return army(st);
	}

};

