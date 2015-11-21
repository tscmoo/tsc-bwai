

struct strat_p_corsair_zealot : strat_p_base {


	virtual void init() override {

		scouting::scout_supply = 10;
		sleep_time = 15;

	}

	bool has_sent_scout = false;
	xy natural_pos;
	bool defence_fight_ok = true;
	bool fight_ok = true;
	bool build_base_cannons = false;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::probe, 8);
				build::add_build_task(1.0, unit_types::pylon);
				build::add_build_task(2.0, unit_types::probe);
				build::add_build_task(2.0, unit_types::probe);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		if (current_frame >= 15 * 60 && natural_pos == xy()) {
			natural_pos = get_next_base()->pos;
		}
		if (natural_pos != xy()) {
			combat::my_closest_base_override = natural_pos;
			combat::my_closest_base_override_until = current_frame + 15 * 10;
		}

		if (!has_sent_scout && !my_units_of_type[unit_types::pylon].empty()) {
			has_sent_scout = true;
			unit*u = get_best_score(my_workers, [&](unit*u) {
				return -diag_distance(my_start_location - u->pos);
			});
			if (u) scouting::add_scout(u);
		}

		//attack_interval = 15 * 60;

		defence_fight_ok = eval_combat(true, 2);
		fight_ok = eval_combat(false, 0);

		if (enemy_army_supply < 20.0) combat::no_aggressive_groups = !fight_ok;
		else attack_interval = 15 * 60;

		combat::aggressive_corsairs = air_army_supply > enemy_air_army_supply;

		if (!my_completed_units_of_type[unit_types::cybernetics_core].empty()) {

			//get_upgrades::set_upgrade_value(upgrade_types::protoss_air_weapons_1, -1.0);
			//get_upgrades::set_upgrade_value(upgrade_types::singularity_charge, -1.0);
			//get_upgrades::set_upgrade_order(upgrade_types::protoss_air_weapons_1, -10.0);
			//get_upgrades::set_upgrade_order(upgrade_types::singularity_charge, -9.0);
			if (my_units_of_type[unit_types::dragoon].size() >= 4) {
				get_upgrades::set_upgrade_value(upgrade_types::singularity_charge, -1.0);
			}

			get_upgrades::set_upgrade_value(upgrade_types::protoss_ground_weapons_1, -1.0);

			get_upgrades::set_upgrade_value(upgrade_types::leg_enhancements, -1.0);
			get_upgrades::set_upgrade_value(upgrade_types::psionic_storm, -1.0);
		}

		build_base_cannons = false;
		if (enemy_spire_count || enemy_air_army_supply || my_resource_depots.size() >= 3) {
			if ((int)my_units_of_type[unit_types::photon_cannon].size() < (enemy_mutalisk_count >= 8 ? 6 : 4)) {
				build_base_cannons = true;
			}
		}

		return current_used_total_supply >= 160;
	}
	a_vector<double> last_costgrid;
	a_unordered_set<build::build_task*> has_placed;
	virtual void post_build() override {

		if (current_frame < 15 * 60) return;

		auto&dragoon_pathing_map = square_pathing::get_pathing_map(unit_types::dragoon);
		while (dragoon_pathing_map.path_nodes.empty()) multitasking::sleep(1);
		double max_center_distance = std::max(diag_distance(combat::defence_choke.inside - combat::defence_choke.center), diag_distance(combat::defence_choke.outside - combat::defence_choke.center));

		auto get_cannon_pos_old = [&](unit_type*ut) {

			bool is_second_pylon = ut == unit_types::pylon && !my_units_of_type[unit_types::pylon].empty();

			xy dragoon_inside = get_nearest_path_node(dragoon_pathing_map, combat::defence_choke.inside)->pos;
			xy dragoon_outside = get_nearest_path_node(dragoon_pathing_map, combat::defence_choke.outside)->pos;

			a_vector<std::pair<unit_type*, xy>> buildings;
			buildings.emplace_back(ut, xy());
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.build_pos != xy()) buildings.emplace_back(b.type->unit, b.build_pos);
			}

			auto can_fit_at = [&](xy pos, unit_type*ut, bool include_liftable) {
				using namespace grid;
				auto&dims = ut->dimensions;
				int right = pos.x + dims[0];
				int bottom = pos.y + dims[1];
				int left = pos.x - dims[2];
				int top = pos.y - dims[3];
				for (auto&v : buildings) {
					unit_type*bt = v.first;
					if (!include_liftable && bt->is_liftable) continue;
					xy bpos = v.second + xy(bt->tile_width * 16, bt->tile_height * 16);
					int bright = bpos.x + bt->dimension_right();
					int bbottom = bpos.y + bt->dimension_down();
					int bleft = bpos.x - bt->dimension_left();
					int btop = bpos.y - bt->dimension_up();
					if (right >= bleft && bottom >= btop && bright >= left && bbottom >= top) return false;
				}
				return true;
			};
			auto path_pred = [&](xy pos, unit_type*ut, bool include_liftable) {
				auto&dims = ut->dimensions;
				pos.x &= -8;
				pos.y &= -8;
				if (can_fit_at(pos, ut, include_liftable)) return true;
				if (can_fit_at(pos + xy(7, 0), ut, include_liftable)) return true;
				if (can_fit_at(pos + xy(0, 7), ut, include_liftable)) return true;
				if (can_fit_at(pos + xy(7, 7), ut, include_liftable)) return true;
				for (int x = 1; x < 7; ++x) {
					if (can_fit_at(pos + xy(x, 0), ut, include_liftable)) return true;
					if (can_fit_at(pos + xy(x, 7), ut, include_liftable)) return true;
				}
				for (int y = 1; y < 7; ++y) {
					if (can_fit_at(pos + xy(0, y), ut, include_liftable)) return true;
					if (can_fit_at(pos + xy(7, y), ut, include_liftable)) return true;
				}
				return false;
			};

			size_t path_iterations = 0;
			auto pred = [&](grid::build_square&bs) {
				if (!build_spot_finder::is_buildable_at(ut, bs)) return false;
				a_deque<xy> path = square_pathing::find_square_path(dragoon_pathing_map, dragoon_inside, [&](xy pos, xy npos) {
					if (++path_iterations >= 1024) multitasking::yield_point();
					if (diag_distance(npos - combat::defence_choke.center) >= max_center_distance * 2 + 32) return false;
					buildings[0].second = npos;
					return path_pred(npos, unit_types::siege_tank_tank_mode, false);
				}, [&](xy pos, xy npos) {
					return diag_distance(npos - dragoon_outside);
				}, [&](xy pos) {
					return manhattan_distance(dragoon_outside - pos) <= 64;
				});
				return !path.empty();
			};
			auto score = [&](xy pos) {
				xy center_pos = pos + xy(ut->tile_width * 16, ut->tile_height * 16);
				double r = 0.0;
				if (is_second_pylon) {
					double cannons_distance_sum = 0.0;
					for (auto*u : my_units_of_type[unit_types::photon_cannon]) {
						cannons_distance_sum += diag_distance(center_pos - u->pos);
					}
					double outside_distance = 32 * 20 - diag_distance(combat::defence_choke.outside);
					r += std::sqrt(cannons_distance_sum*cannons_distance_sum + outside_distance*outside_distance);
				} else {
					double choke_build_squares_distance_sum = 0.0;
					for (auto*bs : combat::defence_choke.build_squares) {
						double closest_existing_cannon_distance = get_best_score_value(my_units_of_type[unit_types::photon_cannon], [&](unit*u) {
							return diag_distance(center_pos - bs->pos);
						});
						double my_distance = diag_distance(center_pos - bs->pos);
						double d = std::min(my_distance, closest_existing_cannon_distance);
						choke_build_squares_distance_sum += d*d;
					}
					r += std::sqrt(choke_build_squares_distance_sum);
				}
				for (auto*bs : combat::defence_choke.inside_squares) {
					if (bs->pos == pos) {
						r /= 8;
						break;
					}
				}
				return r;
			};
			std::vector<xy> starts;
			starts.push_back(combat::defence_choke.center);
			xy pos = build_spot_finder::find_best(starts, 128, pred, score);
			return pos;
		};

		auto get_cannon_pos = [&](unit_type*ut) {

			a_vector<double> costgrid(grid::build_grid_width * grid::build_grid_height);
			using node_t = std::tuple<xy, double>;
			struct node_t_cmp {
				bool operator()(const node_t&a, const node_t&b) {
					return std::get<1>(a) > std::get<1>(b);
				}
			};
			std::priority_queue<node_t, a_vector<node_t>, node_t_cmp> open;

			a_vector<unit*> buildings;
			for (unit*u : my_units_of_type[unit_types::photon_cannon]) buildings.push_back(u);
			for (unit*u : my_units_of_type[unit_types::pylon]) buildings.push_back(u);
			for (unit*u : my_units_of_type[unit_types::forge]) buildings.push_back(u);
			for (unit*u : my_units_of_type[unit_types::gateway]) buildings.push_back(u);

			int iterations = 0;
			auto generate_costgrid = [&](xy pos) {
				xy center_pos = pos + xy(ut->tile_width * 16, ut->tile_height * 16);
				auto get_cost = [&](xy npos) {
					if (npos.x >= pos.x && npos.x < pos.x + ut->tile_width * 32 && npos.y >= pos.y && npos.y < pos.y + ut->tile_height * 32) return 0.0;
					double r = 1.0;
					for (unit*u : buildings) {
						if (u->building && npos.x >= u->building->build_pos.x && npos.x < u->building->build_pos.x + u->type->tile_width * 32 && npos.y >= u->building->build_pos.y && npos.y < u->building->build_pos.y + u->type->tile_height * 32) return 0.0;
						double d = diag_distance(center_pos - u->pos);
						r += 256.0 / (d*d);
						if (u->type == unit_types::photon_cannon) {
							if (d <= 32 * 5) r += 1.0;
						}
					}
					xy n_center_pos = npos + xy(16, 16);
					double d = diag_distance(center_pos - n_center_pos);
					//r += 2560.0 / (d*d);
					r += 256.0 / (d*d);
					if (d <= 32 * 5) r += 1.0;

// 					for (unit*u : buildings) {
// 						double d = diag_distance(u->pos - n_center_pos);
// 						r += 256000.0 / (d*d);
// 					}

// 					for (unit*u : my_units_of_type[unit_types::photon_cannon]) {
// 						if (diag_distance(u->pos - npos) <= 32 * 5) r += 1.0;
// 					}
// 					//if (ut == unit_types::photon_cannon) {
// 						if (diag_distance(center_pos - npos) <= 32 * 5) return 1;
// 					//}
					return r;
				};

				std::fill(costgrid.begin(), costgrid.end(), 0);
				for (xy pos : start_locations) {
					if (pos != my_start_location) {
						open.emplace(pos, 1.0);
						costgrid[grid::build_square_index(pos)] = 1.0;
					}
				}
				while (!open.empty()) {
					++iterations;
					if (iterations % 0x200 == 0) multitasking::yield_point();
					xy cpos;
					double cost;
					std::tie(cpos, cost) = open.top();
					open.pop();

					auto add = [&](xy npos) {
						if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) return;
						size_t index = grid::build_square_index(npos);
						if (!grid::build_grid[index].entirely_walkable) return;
						if (costgrid[index]) return;
						double ncost = get_cost(npos);
						if (ncost == 0.0) return;
						if (ncost < cost) ncost = cost;
						costgrid[index] = ncost;
						open.emplace(npos, ncost);
// 						costgrid[index] = cost + ncost;
// 						open.emplace(npos, cost + ncost);
					};
					add(cpos + xy(32, 0));
					add(cpos + xy(0, 32));
					add(cpos + xy(-32, 0));
					add(cpos + xy(0, -32));
				}
			};

			xy screen_pos = bwapi_pos(game->getScreenPosition());
			generate_costgrid(screen_pos + xy(200, 320));
			last_costgrid = costgrid;

			size_t path_iterations = 0;
			auto pred = [&](grid::build_square&bs) {
				if (!build_spot_finder::is_buildable_at(ut, bs)) return false;
				if (ut->requires_pylon && !build::build_has_pylon(bs, ut)) {
					for (unit*u : my_units_of_type[unit_types::pylon]) {
						if (pylons::is_in_pylon_range(u->pos - (bs.pos + xy(ut->tile_width * 16, ut->tile_height * 16)))) return true;
					}
					return false;
				}
				return true;
			};
			auto score = [&](xy pos) {

				generate_costgrid(pos);

				double cost_to_main = costgrid[grid::build_square_index(my_start_location)];
				double cost_to_natural = costgrid[grid::build_square_index(natural_pos)];

				return std::max(1.0 / cost_to_main, 1.0 / cost_to_natural);
			};
			std::vector<xy> starts;
			starts.push_back(natural_pos);
			xy pos = build_spot_finder::find_best(starts, 256, pred, score);
			return pos;
		};

		a_vector<unit*> nexuses_without_a_pylon;
		for (auto&u : my_units_of_type[unit_types::nexus]) {
			double d = get_best_score_value(my_units_of_type[unit_types::photon_cannon], [&](unit*u2) {
				return diag_distance(u->pos - u2->pos);
			});
			if (my_units_of_type[unit_types::photon_cannon].empty() || d >= 32 * 12) nexuses_without_a_pylon.push_back(u);
		}
		unit*least_protected_nexus = get_best_score(my_resource_depots, [&](unit*u) {
			int n = 0;
			for (unit*u2 : my_units_of_type[unit_types::photon_cannon]) {
				if (diag_distance(u->pos - u2->pos) < 32 * 12) ++n;
			}
			return n;
		});

		auto at_wall = [&](unit_type*ut) {
			if (ut == unit_types::forge && my_units_of_type[unit_types::forge].empty()) return true;
			if (ut == unit_types::photon_cannon && !build_base_cannons) return true;
			if (ut == unit_types::pylon && (my_units_of_type[unit_types::pylon].size() == 0 || my_units_of_type[unit_types::pylon].size() == 2)) return true;
			if (ut == unit_types::gateway && my_units_of_type[unit_types::gateway].size() < 2) return true;
			return false;
		};

// 		for (auto&b : build::build_order) {
// 			if (b.built_unit || b.dead) continue;
// 			if (at_wall(b.type->unit)) {
// 				build::unset_build_pos(&b);
// 			}
// 		}

		for (auto&b : build::build_order) {
			if (b.built_unit || b.dead) continue;
			if (at_wall(b.type->unit)) {
				if ((b.type->unit == unit_types::pylon || !my_units_of_type[unit_types::pylon].empty()) && has_placed.insert(&b).second) {
					b.build_near = natural_pos;
					xy prev_pos = b.build_pos;
					build::unset_build_pos(&b);
					b.dont_find_build_pos = true;
					xy pos = get_cannon_pos(b.type->unit);
					b.dont_find_build_pos = false;
					if (pos != xy()) {
						build::set_build_pos(&b, pos);
					} else build::set_build_pos(&b, prev_pos);
				}
			} else if (b.type->unit == unit_types::pylon) {
				if (!nexuses_without_a_pylon.empty()) b.build_near = nexuses_without_a_pylon.front()->pos;
			} else if (b.type->unit == unit_types::photon_cannon) {
				if (least_protected_nexus) b.build_near = least_protected_nexus->pos;
			}
		}

	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		army = [army](state&st) {
			return maxprod(st, unit_types::zealot, army);
		};
		if (zealot_count >= 18) {
			army = [army](state&st) {
				return maxprod(st, unit_types::dragoon, army);
			};
		}

		if (cannon_count < 2) {
			army = [army](state&st) {
				return nodelay(st, unit_types::photon_cannon, army);
			};
		}

		if (army_supply >= 18.0 && high_templar_count < zealot_count / 5) {
			army = [army](state&st) {
				return nodelay(st, unit_types::high_templar, army);
			};
		}

		if (dragoon_count * 2 + corsair_count * 2 < enemy_air_army_supply) {
			army = [army](state&st) {
				return maxprod(st, unit_types::dragoon, army);
			};
		}

		if (corsair_count < 6) {
			army = [army](state&st) {
				return nodelay(st, unit_types::corsair, army);
			};
		}

		if (army_supply >= 20.0 && defence_fight_ok) {
			if (observer_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::observer, army);
				};
			}
		}

		if (enemy_air_army_supply == 0.0 && (corsair_count >= 1 || !defence_fight_ok)) {
			if (count_production(st, unit_types::zealot) < 3) {
				army = [army](state&st) {
					return maxprod(st, unit_types::zealot, army);
				};
			}
		}

		if (count_units_plus_production(st, unit_types::nexus) < 2) {
			army = [army](state&st) {
				return nodelay(st, unit_types::nexus, army);
			};
		}

// 		if (build_base_cannons && defence_fight_ok) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::photon_cannon, army);
// 			};
// 		}

		if (probe_count < 44 || army_supply >= 32.0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::probe, army);
			};
		}

		if ((count_units_plus_production(st, unit_types::gateway) && zealot_count < 1) || !defence_fight_ok) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zealot, army);
			};
			if (dragoon_count * 2 + corsair_count * 2 < enemy_air_army_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::dragoon, army);
				};
			}
		}

		if (enemy_air_army_supply || enemy_spire_count || enemy_lair_count || (zealot_count >= 4 && dragoon_count < 2)) {
			if (dragoon_count < 4) {
				army = [army](state&st) {
					return nodelay(st, unit_types::dragoon, army);
				};
			}
		}

		if ((!defence_fight_ok || count_units_plus_production(st, unit_types::templar_archives)) && !my_completed_units_of_type[unit_types::photon_cannon].empty()) {
			if (cannon_count < (count_units_plus_production(st, unit_types::stargate) ? 5 : 3)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::photon_cannon, army);
				};
			}
		}

		if (cannon_count == 0 || (enemy_worker_count < 10 && !opponent_has_expanded && army_supply == 0.0)) {
			if (cannon_count < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::photon_cannon, army);
				};
			}
		}

		if (army_supply >= 24.0) {
			if (force_expand && count_production(st, unit_types::nexus) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::nexus, army);
				};
			}
		}

		return army(st);
	}

	void render() {

		if (!last_costgrid.empty()) {
			double cost_to_main = last_costgrid[grid::build_square_index(my_start_location)];
			log("cost_to_main is %g\n", cost_to_main);
			double highest_val = 0.0;
			for (auto&v : last_costgrid) {
				if (v > highest_val) highest_val = v;
			}
			xy screen_pos = bwapi_pos(game->getScreenPosition());
			for (int y = screen_pos.y; y < screen_pos.y + 400; y += 32) {
				for (int x = screen_pos.x; x < screen_pos.x + 640; x += 32) {
					if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;

					x &= -32;
					y &= -32;

					size_t index = grid::build_square_index(xy(x, y));
					double val = last_costgrid[index];
					int c = (int)((val / highest_val) * 255);

					game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Color(c, c, c));

				}
			}
		}

	}

};

