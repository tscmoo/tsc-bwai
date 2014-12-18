
struct strat_tvt_opening {

	void run() {

		combat::no_aggressive_groups = true;
		combat::aggressive_wraiths = true;

		using namespace buildpred;

		auto build = [&](state&st) {
			if (count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode) == 0) {
				if (!my_completed_units_of_type[unit_types::machine_shop].empty()) {
					return depbuild(st, state(st), unit_types::siege_tank_tank_mode);
				}
			}
			return nodelay(st, unit_types::scv, [&](state&st) {
				st.dont_build_refineries = true;
				if (count_units_plus_production(st, unit_types::refinery) == 0) {
					return depbuild(st, state(st), unit_types::refinery);
				}
				auto backbone = [&](state&st) {
					return maxprod1(st, unit_types::vulture);
				};
				int marine_count = count_units_plus_production(st, unit_types::marine);
				if (!my_units_of_type[unit_types::factory].empty() && marine_count < 3) {
					return nodelay(st, unit_types::marine, backbone);
				}
				return backbone(st);
			});
		};

		bool built_bunker = false;
		bool has_detected_proxy = false;
		while (true) {
			resource_gathering::max_gas = 100;

			int proxy_barracks_count = 0;
			int proxy_scv_count = 0;
			int proxy_marine_count = 0;
			int enemy_barracks_count = 0;
			update_possible_start_locations();
			for (unit*e : enemy_units) {
				if (e->type == unit_types::barracks || e->type == unit_types::scv || e->type == unit_types::marine) {
					double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
						return unit_pathing_distance(unit_types::scv, e->pos, pos);
					});
					if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base)*0.5 < e_d) {
						if (e->type == unit_types::barracks) ++proxy_barracks_count;
						if (e->type == unit_types::scv) ++proxy_scv_count;
						if (e->type == unit_types::marine) ++proxy_marine_count;
					}
				}
				if (e->type == unit_types::barracks) ++enemy_barracks_count;
			}

			int vulture_count = my_completed_units_of_type[unit_types::vulture].size();
			int marine_count = my_completed_units_of_type[unit_types::marine].size();
			bool defence_ok = marine_count + vulture_count * 2 >= proxy_marine_count + proxy_scv_count + 4;

			bool defend_proxy = (proxy_barracks_count || proxy_marine_count || proxy_scv_count >= 4 || enemy_barracks_count >= 2) && !defence_ok;

			if (defend_proxy && !has_detected_proxy) {
				has_detected_proxy = true;
				for (auto&b : build::build_tasks) {
					if (b.built_unit) continue;
					build::unset_build_pos(&b);
				}
			}
			if (!defend_proxy && has_detected_proxy) {
				combat::no_aggressive_groups = false;
			}

			if (!my_units_of_type[unit_types::factory].empty()) {
				resource_gathering::max_gas = 250;
			}
			if (!my_units_of_type[unit_types::vulture].empty()) {
				resource_gathering::max_gas = 0.0;
				get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
				if (players::my_player->has_upgrade(upgrade_types::spider_mines)) {
					get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
					if (players::my_player->has_upgrade(upgrade_types::siege_mode)) {
						get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, -1.0);
					}
				}
			}

			bool expand = false;
			auto my_st = get_my_current_state();
			bool has_bunker = !my_units_of_type[unit_types::bunker].empty();
			if (my_st.bases.size() > 1 || vulture_count >= 4) break;
			if (my_st.bases.size() == 1) {
				if (vulture_count >= 2) {
					expand = true;
				}
				for (unit*u : enemy_units) {
					if (!u->type->is_resource_depot) continue;
					bool is_expo = true;
					for (xy p : start_locations) {
						if (u->building->build_pos == p) {
							is_expo = false;
							break;
						}
					}
					if (is_expo) expand = true;
				}
			}

			if (defend_proxy) {
				if (my_units_of_type[unit_types::factory].empty()) {
					if (!built_bunker && marine_count >= 2 && proxy_marine_count > marine_count) {
						built_bunker = true;
						auto*t = build::add_build_task(0, unit_types::bunker);
					}
					for (auto&b : build::build_tasks) {
						if (b.built_unit) continue;
						if (b.type->unit != unit_types::bunker) continue;
						unit*nu = get_best_score(my_buildings, [&](unit*u) {
							double ned = get_best_score_value(enemy_units, [&](unit*e) {
								return units_distance(e, u);
							});
							if (ned <= 32 * 8) return std::numeric_limits<double>::infinity();
							if (!enemy_buildings.empty()) {
								return get_best_score_value(enemy_buildings, [&](unit*e) {
									return units_pathing_distance(e, u);
								});
							}
							return ned;
						}, std::numeric_limits<double>::infinity());
						if (nu) {
							build::unset_build_pos(&b);
							log("nu is %s\n", nu->type->name);
							std::array<xy, 1> starts;
							starts[0] = nu->pos;
							unit_type*ut = unit_types::bunker;
							xy pos = build_spot_finder::find_best(starts, 32, [&](grid::build_square&bs) {
								return build_spot_finder::is_buildable_at(ut, bs);
							}, [&](xy pos) {
								int n = 0;
								for (int y = 0; y < ut->tile_height; ++y) {
									for (int x = 0; x < ut->tile_width; ++x) {
										if (combat::entire_threat_area.test(grid::build_square_index(pos + xy(x * 32, y * 32)))) ++n;
									}
								}
								return diag_distance(pos - nu->pos);
								//return n + diag_distance(pos - nu->pos) / 32 * 4;
							});
							log("bunker - move from %d %d to %d %d\n", b.build_pos.x, b.build_pos.y, pos.x, pos.y);
							build::set_build_pos(&b, pos);
						}
					}
				}
				if (!scouting::all_scouts.empty()) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
				expand = false;
				if (my_workers.size() <= 20) resource_gathering::max_gas = 1.0;
				auto proxy_defence_build = [&](state&st) {
					auto marines = [&](state&st) {
						if (my_units_of_type[unit_types::marine].size()>my_units_of_type[unit_types::scv].size()) {
							return maxprod(st, unit_types::scv, [&](state&st) {
								return nodelay(st, unit_types::marine, [&](state&st) {
									return depbuild(st, state(st), unit_types::vulture);
								});
							});
						} else {
							return maxprod(st, unit_types::marine, [&](state&st) {
								return nodelay(st, unit_types::scv, [&](state&st) {
									return depbuild(st, state(st), unit_types::vulture);
								});
							});
						}
					};
					if (!my_completed_units_of_type[unit_types::factory].empty()) {
						return nodelay(st, unit_types::vulture, marines);
					}
					return marines(st);
				};
				execute_build(expand, proxy_defence_build);
			} else execute_build(expand, build);

			multitasking::sleep(15 * 2);
		}
		combat::build_bunker_count = 0;
		resource_gathering::max_gas = 0.0;

		//combat::no_aggressive_groups = false;

	}

	void render() {

	}

};

