

struct strat_t_proxy_tank: strat_t_base {

// 	bool spider_mines_first = adapt::choose_bool("spider mines first");
// 	bool proxy_both_factories = adapt::choose_bool("proxy both factories");
// 	bool vulture_before_machine_shop = adapt::choose_bool("vulture before machine shop");
	bool spider_mines_first = false;
	bool proxy_both_factories = true;
	bool vulture_before_machine_shop = false;

	virtual void init() override {

		get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::siege_mode, -10.0);

		if (spider_mines_first) {
			get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
			get_upgrades::set_upgrade_order(upgrade_types::spider_mines, -11.0);
		}

		default_upgrades = false;

		sleep_time = 15;

		proxy_builder.fill(nullptr);
		proxy_fact.fill(nullptr);
	}
	refcounted_ptr<build::build_task> bt_fact[2];
	//refcounted_ptr<build::build_task> bt_machine_shop;
	int last_find_proxy_pos = 0;
	std::array<unit*, 2> proxy_builder;
	std::array<unit*, 2> proxy_fact;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(1.0, unit_types::supply_depot);
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(3.0, unit_types::refinery);
				build::add_build_task(4.0, unit_types::barracks);
				build::add_build_task(4.5, unit_types::scv);
				build::add_build_task(4.5, unit_types::scv);
				build::add_build_task(4.5, unit_types::scv);
				build::add_build_task(4.5, unit_types::marine);
				bt_fact[0] = build::add_build_task(5.0, unit_types::factory);
				build::add_build_task(5.5, unit_types::scv);
// 				if (vulture_before_machine_shop) {
// 					build::add_build_task(5.5, unit_types::supply_depot);
// 					build::add_build_task(5.5, unit_types::vulture);
// 				}
// 				build::add_build_task(5.5, unit_types::scv);
// 				build::add_build_task(5.5, unit_types::scv);
//				bt_machine_shop = build::add_build_task(6.0, unit_types::machine_shop);
				bt_fact[1] = build::add_build_task(7.0, unit_types::factory);
// 				build::add_build_task(8.0, unit_types::supply_depot);
// 				build::add_build_task(8.5, unit_types::scv);
// 				build::add_build_task(8.5, unit_types::scv);
// 				build::add_build_task(9.0, unit_types::vulture);
// 				build::add_build_task(9.0, unit_types::vulture);
// 				build::add_build_task(10.0, unit_types::marine);
// 				if (spider_mines_first) {
// 					build::add_build_task(10.0, unit_types::supply_depot);
// 					build::add_build_task(10.0, unit_types::vulture);
// 					build::add_build_task(10.0, unit_types::vulture);
// 				}
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		if (!enemy_buildings.empty()) rm_all_scouts();

		bool failed = false;
		bool refind_proxy = false;
		if (current_frame - last_find_proxy_pos >= 15 * 10) {
			last_find_proxy_pos = current_frame;
			refind_proxy = true;
		}
		for (int i = 0; i < 2; ++i) {
			if (!proxy_fact[i] || !proxy_fact[i]->is_completed) {
				if (proxy_builder[i] && proxy_builder[i]->dead) failed = true;
				if (proxy_fact[i] && proxy_fact[i]->dead) failed = true;
			}
			if (bt_fact[i]) {
				if ((!bt_fact[i]->builder || bt_fact[i]->builder->controller->action != unit_controller::action_build) && current_used_total_supply >= (i == 0 ? 12 : 14)) {
// 					auto*u = get_best_score(my_units_of_type[unit_types::scv], [&](unit*u) {
// 						if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
// 						return -u->hp;
// 					}, std::numeric_limits<double>::infinity());
// 					if (u) build::set_builder(bt_fact[i], u, true);
					bt_fact[i]->send_builder_immediately = true;
				}
				if (refind_proxy) {
					if (i == 0 || proxy_both_factories) {
						build::unset_build_pos(bt_fact[i]);
						build::set_build_pos(bt_fact[i], find_proxy_pos(32 * 20));
					}
				}
				if (bt_fact[i]->builder) proxy_builder[i] = bt_fact[i]->builder;
				if (bt_fact[i]->built_unit) {
					proxy_fact[i] = bt_fact[i]->built_unit;
// 					if (i == 0) {
// 						build::set_builder(bt_machine_shop, bt_fact[i]->built_unit);
// 						bt_machine_shop = nullptr;
// 					}
					bt_fact[i] = nullptr;
				}
			}
		}

		if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, -1.0);
			get_upgrades::set_upgrade_order(upgrade_types::ion_thrusters, -8.0);
		}

		if (my_units_of_type[unit_types::factory].empty()) {
			resource_gathering::max_gas = 100.0;
		} else {
			resource_gathering::max_gas = 150.0;
		}

		combat::no_aggressive_groups = false;
		combat::aggressive_groups_done_only = false;

		if (failed) {
			for (int i = 0; i < 2; ++i) {
				if (proxy_fact[i]) proxy_fact[i]->game_unit->cancelConstruction();
				for (auto&b : build::build_order) {
					if (b.built_unit == proxy_fact[i]) b.dead = true;
				}
			}
			bo_cancel_all();
			return true;
		}
		
		if (enemy_air_army_supply) return true;
		if (current_used_total_supply >= 45) return true;
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		if (count_units_plus_production(st, unit_types::factory) < 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::factory, army);
			};
		}

		army = [army = army](state&st) {
			return nodelay(st, unit_types::marine, army);
		};

		army = [army = army](state&st) {
			return nodelay(st, unit_types::vulture, army);
		};

		if (scv_count < 60) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		if (players::my_player->has_upgrade(upgrade_types::siege_mode) || vulture_count >= (spider_mines_first ? 6 : 2)) {
			if (count_production(st, unit_types::siege_tank_tank_mode) == 0) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::siege_tank_tank_mode, army);
				};
			}
		}

		if ((!vulture_before_machine_shop || vulture_count) && machine_shop_count == 0) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::machine_shop, army);
			};
		}

		//if (force_expand || st.minerals >= 400 || tank_count + vulture_count >= 3) {
		if ((scv_count >= 25 || tank_count + vulture_count >= 4) && count_units_plus_production(st, unit_types::cc) < 3) {
			if (count_units_plus_production(st, unit_types::cc) < 2 || force_expand || st.minerals >= 400) {
				if (count_production(st, unit_types::cc) == 0 && my_units_of_type[unit_types::cc].size() == my_completed_units_of_type[unit_types::cc].size()) {
					army = [army = army](state&st) {
						return nodelay(st, unit_types::cc, army);
					};
				}
			}
		}

		if (army_supply < enemy_attacking_army_supply || (enemy_attacking_army_supply && vulture_count < 2)) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::vulture, army);
			};
		}

		return army(st);
	}

};

