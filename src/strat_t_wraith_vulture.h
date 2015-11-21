

struct strat_t_wraith_vulture : strat_t_base {


	virtual void init() override {

		sleep_time = 15;

		get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);

	}
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::scv, 9);
				build::add_build_task(1.0, unit_types::supply_depot);
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		if (my_units_of_type[unit_types::vulture].empty() && current_used_total_supply < 30.0) {
			combat::force_defence_is_scared_until = current_frame + 15 * 10;
		}

		if (being_rushed) rm_all_scouts();

		combat::no_aggressive_groups = false;

		if (current_used_total_supply - my_workers.size() >= 20 || enemy_attacking_army_supply >= 8.0) {
			for (auto*a : combat::live_combat_units) {
				if (!a->u->type->is_flyer && !a->u->type->is_worker) a->use_for_drops_until = current_frame + 15 * 10;
			}
		}

		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		if (st.minerals >= 300) {
			if (count_units_plus_production(st, unit_types::factory) < 4) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::factory, army);
				};
			}
		}
		army = [army = army](state&st) {
			return nodelay(st, unit_types::vulture, army);
		};

		army = [army = army](state&st) {
			return maxprod(st, unit_types::wraith, army);
		};

		if (valkyrie_count * 2 < enemy_air_army_supply && wraith_count > valkyrie_count) {
			army = [army = army](state&st) {
				return maxprod(st, unit_types::valkyrie, army);
			};
		}

		if (vulture_count >= 16) {
			if (dropship_count < vulture_count / 16) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::dropship, army);
				};
			}
		}

		if (vulture_count < 4) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::vulture, army);
			};
			if (marine_count < 4) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::marine, army);
				};
			}
		}

		if (vulture_count >= 2 && !is_defending) {
			if (machine_shop_count == 0) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::machine_shop, army);
				};
			}
		}

		army = [army = army](state&st) {
			return nodelay(st, unit_types::scv, army);
		};

		if (force_expand && count_production(st, unit_types::cc) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::cc, army);
			};
		}

		return army(st);
	}

};

