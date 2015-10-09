
struct strat_tvt_bio {

	void run() {

		using namespace buildpred;

		combat::no_aggressive_groups = true;
		combat::no_scout_around = true;

		get_upgrades::set_no_auto_upgrades(true);

		scouting::scout_supply = 12;

		auto get_next_base = [&]() {
			auto st = get_my_current_state();
			unit_type*worker_type = unit_types::drone;
			unit_type*cc_type = unit_types::cc;
			a_vector<refcounted_ptr<resource_spots::spot>> available_bases;
			for (auto&s : resource_spots::spots) {
				if (grid::get_build_square(s.cc_build_pos).building) continue;
				bool okay = true;
				for (auto&v : st.bases) {
					if ((resource_spots::spot*)v.s == &s) okay = false;
				}
				if (!okay) continue;
				if (!build::build_is_valid(grid::get_build_square(s.cc_build_pos), cc_type)) continue;
				available_bases.push_back(&s);
			}
			return get_best_score(available_bases, [&](resource_spots::spot*s) {
				double score = unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
				double res = 0;
				double ned = get_best_score_value(enemy_buildings, [&](unit*e) {
					if (e->type->is_worker) return std::numeric_limits<double>::infinity();
					return diag_distance(s->pos - e->pos);
				});
				if (ned <= 32 * 30) score += 10000;
				bool has_gas = false;
				for (auto&r : s->resources) {
					res += r.u->resources;
					if (r.u->type->is_gas) has_gas = true;
				}
				score -= (has_gas ? res : res / 4) / 10 + ned;
				return score;
			}, std::numeric_limits<double>::infinity());
		};

		auto place_expos = [&]() {

			auto st = get_my_current_state();

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit  && b.type->unit->is_resource_depot) {
					xy pos = b.build_pos;
					build::unset_build_pos(&b);
				}
			}

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit && b.type->unit->is_resource_depot) {
					xy pos = b.build_pos;
					build::unset_build_pos(&b);
					auto s = get_next_base();
					if (s) pos = s->cc_build_pos;
					if (pos != xy()) build::set_build_pos(&b, pos);
					else build::unset_build_pos(&b);
				}
			}

		};

		bool maxed_out_aggression = false;
		int opening_state = 0;
		while (true) {

			int enemy_marine_count = 0;
			int enemy_firebat_count = 0;
			int enemy_ghost_count = 0;
			int enemy_vulture_count = 0;
			int enemy_tank_count = 0;
			int enemy_goliath_count = 0;
			int enemy_wraith_count = 0;
			int enemy_valkyrie_count = 0;
			int enemy_bc_count = 0;
			int enemy_science_vessel_count = 0;
			int enemy_dropship_count = 0;
			double enemy_army_supply = 0.0;
			double enemy_air_army_supply = 0.0;
			double enemy_ground_army_supply = 0.0;
			double enemy_ground_large_army_supply = 0.0;
			double enemy_ground_small_army_supply = 0.0;
			int enemy_static_defence_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::marine) ++enemy_marine_count;
				if (e->type == unit_types::firebat) ++enemy_firebat_count;
				if (e->type == unit_types::ghost) ++enemy_ghost_count;
				if (e->type == unit_types::vulture) ++enemy_vulture_count;
				if (e->type == unit_types::siege_tank_tank_mode) ++enemy_tank_count;
				if (e->type == unit_types::siege_tank_siege_mode) ++enemy_tank_count;
				if (e->type == unit_types::goliath) ++enemy_goliath_count;
				if (e->type == unit_types::wraith) ++enemy_wraith_count;
				if (e->type == unit_types::valkyrie) ++enemy_valkyrie_count;
				if (e->type == unit_types::battlecruiser) ++enemy_bc_count;
				if (e->type == unit_types::science_vessel) ++enemy_science_vessel_count;
				if (e->type == unit_types::dropship) ++enemy_dropship_count;
				if (!e->type->is_worker && !e->building) {
					if (e->is_flying) enemy_air_army_supply += e->type->required_supply;
					else {
						enemy_ground_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_large) enemy_ground_large_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_small) enemy_ground_small_army_supply += e->type->required_supply;
					}
					enemy_army_supply += e->type->required_supply;
				}
				if (e->type == unit_types::missile_turret) ++enemy_static_defence_count;
				if (e->type == unit_types::photon_cannon) ++enemy_static_defence_count;
				if (e->type == unit_types::sunken_colony) ++enemy_static_defence_count;
				if (e->type == unit_types::spore_colony) ++enemy_static_defence_count;
			}


			if (my_workers.size() >= 18 && current_used_total_supply - my_workers.size() < 12) combat::build_bunker_count = 1;
			else combat::build_bunker_count = 0;

// 			if (current_used_total_supply >= 25) {
// 				get_upgrades::set_upgrade_value(upgrade_types::stim_packs, -1.0);
// 				get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, -1.0);
// 			}

			get_upgrades::set_upgrade_value(upgrade_types::spider_mines, 1.0);
			if (!my_units_of_type[unit_types::vulture].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
			}
			if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
			}
			if (!my_units_of_type[unit_types::ghost].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::lockdown, -1.0);
			}

			if (current_used_total_supply >= 100 && get_upgrades::no_auto_upgrades) {
				get_upgrades::set_no_auto_upgrades(false);
			}

			combat::no_aggressive_groups = false;
			combat::aggressive_groups_done_only = true;

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				combat::aggressive_groups_done_only = false;
				if (current_used_total_supply < 150.0) maxed_out_aggression = false;
			}
			if (current_used_total_supply - my_workers.size() >= enemy_army_supply * 2) {
				combat::aggressive_groups_done_only = false;
			}

			bool force_expand = long_distance_miners() >= 1 && get_next_base();

			auto build = [&](state&st) {

				int scv_count = count_units_plus_production(st, unit_types::scv);
				int marine_count = count_units_plus_production(st, unit_types::marine);
				int medic_count = count_units_plus_production(st, unit_types::medic);
				int wraith_count = count_units_plus_production(st, unit_types::wraith);
				int science_vessel_count = count_units_plus_production(st, unit_types::science_vessel);
				int ghost_count = count_units_plus_production(st, unit_types::ghost);
				int tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
				int vulture_count = count_units_plus_production(st, unit_types::vulture);

				double army_supply = 0.0;
				double air_army_supply = 0.0;
				double ground_army_supply = 0.0;
				for (auto&v : st.units) {
					unit_type*ut = v.first;
					if (!ut->is_worker) {
						army_supply += ut->required_supply;
						if (ut->is_flyer) air_army_supply += ut->required_supply;
						else ground_army_supply += ut->required_supply;
					}
				}
				for (auto&v : st.production) {
					unit_type*ut = v.second;
					if (!ut->is_worker) {
						army_supply += ut->required_supply;
						if (ut->is_flyer) air_army_supply += ut->required_supply;
						else ground_army_supply += ut->required_supply;
					}
				}

				std::function<bool(state&)> army = [&](state&st) {
					return false;
				};

// 				army = [army](state&st) {
// 					return maxprod(st, unit_types::marine, army);
// 				};
// 
// 				if (marine_count >= 12 && medic_count < marine_count / 5) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::medic, army);
// 					};
// 				}
// 
// 				int desired_wraith_count = marine_count / 8;
// 				if (wraith_count < desired_wraith_count) {
// 					if (desired_wraith_count - wraith_count >= 4) {
// 						army = [army](state&st) {
// 							return maxprod(st, unit_types::wraith, army);
// 						};
// 					} else {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::wraith, army);
// 						};
// 					}
// 				}
// 				if (army_supply >= 30) {
// 					if (science_vessel_count < army_supply / 12) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::science_vessel, army);
// 						};
// 					}
// 				}
// 				if (army_supply >= 60) {
// 					if (science_vessel_count < army_supply / 6) {
// 						army = [army](state&st) {
// 							return maxprod(st, unit_types::science_vessel, army);
// 						};
// 					}
// 				}

// 				if (army_supply < 20) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::marine, army);
// 					};
// 				}

				army = [army](state&st) {
					return maxprod(st, unit_types::vulture, army);
				};
				army = [army](state&st) {
					return maxprod(st, unit_types::wraith, army);
				};
// 				if (st.used_supply[race_terran] >= 50) {
// 					army = [army](state&st) {
// 						return maxprod(st, unit_types::ghost, army);
// 					};
// 				}

// 				if (army_supply < 4) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::marine, army);
// 					};
// 				}
				
// 				if (army_supply >= 15) {
// 					army = [army](state&st) {
// 						return maxprod(st, unit_types::vulture, army);
// 					};
// 					army = [army](state&st) {
// 						return maxprod(st, unit_types::firebat, army);
// 					};
// 					army = [army](state&st) {
// 						return maxprod(st, unit_types::science_vessel, army);
// 					};
// 				}

				if (army_supply >= 40) {
					if (science_vessel_count < army_supply / 20) {
						army = [army](state&st) {
							return maxprod(st, unit_types::science_vessel, army);
						};
					}
					if (ghost_count < army_supply / 10) {
						army = [army](state&st) {
							return maxprod(st, unit_types::ghost, army);
						};
					}
					if (tank_count < army_supply / 14) {
						army = [army](state&st) {
							return maxprod(st, unit_types::siege_tank_tank_mode, army);
						};
					}
				}

				if (scv_count < 60) {
					army = [army](state&st) {
						return nodelay(st, unit_types::scv, army);
					};
				}

				if (force_expand && count_production(st, unit_types::cc) == 0 && my_units_of_type[unit_types::cc].size() == my_completed_units_of_type[unit_types::cc].size()) {
					army = [army](state&st) {
						return nodelay(st, unit_types::cc, army);
					};
				}

				if (vulture_count >= 1) {
					int machine_shops = count_production(st, unit_types::machine_shop);
					for (auto&v : st.units[unit_types::factory]) {
						if (v.has_addon) ++machine_shops;
					}
					if (machine_shops == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::machine_shop, army);
						};
					}
				}

				return army(st);

			};


			auto all_done = [&]() {
				for (auto&b : build::build_order) {
					if (!b.built_unit) return false;
				}
				return true;
			};
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
					build::add_build_task(2.0, unit_types::barracks);
					build::add_build_task(3.0, unit_types::scv);
					build::add_build_task(3.0, unit_types::scv);
					build::add_build_task(3.0, unit_types::scv);
					build::add_build_task(4.0, unit_types::marine);
					build::add_build_task(5.0, unit_types::cc);
					++opening_state;
				}
			} else if (opening_state == 1) {
				if (all_done()) {
					++opening_state;
				}
			} else execute_build(false, build);

			place_expos();

			multitasking::sleep(15 * 5);
		}


	}

	void render() {

	}

};

