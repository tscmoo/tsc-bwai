
struct strat_tvt_opening {

	void run() {

		combat::no_aggressive_groups = true;
		combat::aggressive_wraiths = true;

		using namespace buildpred;

		auto build = [&](state&st) {
			int scv_count = count_units_plus_production(st, unit_types::scv);
			if (scv_count >= 11 && count_units_plus_production(st, unit_types::barracks) == 0) return depbuild(st, state(st), unit_types::barracks);
			if (scv_count >= 12 && count_units_plus_production(st, unit_types::refinery) == 0) return depbuild(st, state(st), unit_types::refinery);
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
					if (count_units_plus_production(st, unit_types::factory) >= 3) {
						return depbuild(st, state(st), unit_types::vulture);
					}
					return maxprod1(st, unit_types::vulture);
				};
				int marine_count = count_units_plus_production(st, unit_types::marine);
				int vulture_count = count_units_plus_production(st, unit_types::vulture);
				if (vulture_count==0 && marine_count < 3) {
					return nodelay(st, unit_types::marine, backbone);
				}
				if (vulture_count >= 1) {
					int machine_shops = count_production(st, unit_types::machine_shop);
					for (auto&v : st.units[unit_types::factory]) {
						if (v.has_addon) ++machine_shops;
					}
					if (machine_shops == 0) {
						return nodelay(st, unit_types::machine_shop, backbone);
					}
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
			int enemy_vulture_count = 0;
			int enemy_tank_count = 0;
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
				if (e->type == unit_types::vulture) ++enemy_vulture_count;
				if (e->type == unit_types::siege_tank_tank_mode || e->type == unit_types::siege_tank_siege_mode) ++enemy_tank_count;
			}

			int vulture_count = my_completed_units_of_type[unit_types::vulture].size();
			int marine_count = my_completed_units_of_type[unit_types::marine].size();
			bool defence_ok = (marine_count + vulture_count * 2 >= proxy_marine_count + proxy_scv_count + 4 && marine_count + vulture_count * 2 >= 10) || enemy_tank_count;

			bool defend_proxy = (proxy_barracks_count || proxy_marine_count || proxy_scv_count >= 4 || enemy_barracks_count >= 2) && !defence_ok;

			if (defend_proxy && !has_detected_proxy) {
				log("waa detected proxy\n");
				has_detected_proxy = true;
				for (auto&b : build::build_tasks) {
					if (b.built_unit) continue;
					build::unset_build_pos(&b);
				}
			}
			if (!defend_proxy && has_detected_proxy && my_units_of_type[unit_types::vulture].empty()) {
				combat::no_aggressive_groups = false;
			} else combat::no_aggressive_groups = true;

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

			if (!defend_proxy && vulture_count > enemy_vulture_count && enemy_tank_count == 0) {
				scouting::comsat_supply = 50;
			} else scouting::comsat_supply = 70;

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

			bool expand = false;
			auto my_st = get_my_current_state();
			bool has_bunker = !my_units_of_type[unit_types::bunker].empty();
			if (my_st.bases.size() > 2 || vulture_count >= 16 || long_distance_miners() >= 8) break;
			if (my_st.bases.size() == 1) {
				if (vulture_count >= 2 && vulture_count > enemy_vulture_count) {
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
			} else if (my_st.bases.size() == 2) {
				if (vulture_count > enemy_vulture_count && vulture_count >= 8) expand = true;
			}
			if (my_workers.size() < 18) expand = false;
			if (expand && my_completed_units_of_type[unit_types::siege_tank_tank_mode].empty()) {
				if (!enemy_buildings.empty() && (proxy_barracks_count + proxy_marine_count + proxy_scv_count)) {
					double d = get_best_score_value(enemy_buildings, [&](unit*u) {
						return unit_pathing_distance(unit_types::scv, my_start_location, u->pos);
					});
					if (d <= 32 * 40) expand = false;
				}
			}

			if (!defend_proxy) {
				for (unit*u : my_workers) {
					if (u->force_combat_unit) u->force_combat_unit = false;
				}
			}

			if (defend_proxy) {
				get_upgrades::set_no_auto_upgrades(true);

				if (proxy_barracks_count && enemy_barracks_count < 2) {
					if ((int)my_workers.size() >= proxy_scv_count + proxy_marine_count + 5 && proxy_marine_count <= 2) {
						combat::no_aggressive_groups = false;
						int combat_count = 0;
						for (unit*u : my_workers) {
							if (u->force_combat_unit) ++combat_count;
						}
						for (unit*u : my_workers) {
							if (combat_count >= proxy_scv_count + proxy_marine_count + 2) break;
							if (u->force_combat_unit) continue;
							u->force_combat_unit = true;
							++combat_count;
						}
					}
				}

				combat::defence_is_scared = true;
				if (!scouting::all_scouts.empty()) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
				expand = false;
				if (my_workers.size() < 12 ) resource_gathering::max_gas = 1.0;
				else if (resource_gathering::max_gas == 1.0) resource_gathering::max_gas = 100;
				auto proxy_defence_build = [&](state&st) {
					std::function<bool(state&)> build = [&](state&st) {
						if (my_units_of_type[unit_types::marine].size() > my_units_of_type[unit_types::scv].size()) {
							return nodelay(st, unit_types::scv, [&](state&st) {
								return nodelay(st, unit_types::marine, [&](state&st) {
									return depbuild(st, state(st), unit_types::vulture);
								});
							});
						} else {
							return nodelay(st, unit_types::marine, [&](state&st) {
								int scv_count = count_units_plus_production(st, unit_types::scv);
								int barracks_count = count_units_plus_production(st, unit_types::barracks);
								int factory_count = count_units_plus_production(st, unit_types::factory);
								if (barracks_count >= 1 && factory_count == 0 && scv_count >= 12) {
									return nodelay(st, unit_types::factory, [&](state&st) {
										return nodelay(st, unit_types::marine, [&](state&st) {
											return depbuild(st, state(st), unit_types::scv);
										});
									});
								}
								if (barracks_count < 1) {
									return nodelay(st, unit_types::barracks, [&](state&st) {
										return nodelay(st, unit_types::scv, [&](state&st) {
											return depbuild(st, state(st), unit_types::vulture);
										});
									});
								}
								return nodelay(st, unit_types::scv, [&](state&st) {
									return depbuild(st, state(st), unit_types::vulture);
								});
							});
						}
					};
					if (my_workers.size() < 8) {
						build = [build](state&st) {
							return nodelay(st, unit_types::scv, build);
						};
					}
					if (!my_completed_units_of_type[unit_types::factory].empty()) {
						return nodelay(st, unit_types::vulture, build);
					}
					return build(st);
				};
				execute_build(expand, proxy_defence_build);
			} else execute_build(expand, build);

			multitasking::sleep(15 * 2);
		}
		combat::build_bunker_count = 0;
		resource_gathering::max_gas = 0.0;

		get_upgrades::set_no_auto_upgrades(false);

	}

	void render() {

	}

};

