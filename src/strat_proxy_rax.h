

struct proxy_rax {

	a_vector<double> test_cost_map = a_vector<double>(grid::build_grid_width*grid::build_grid_height);

	xy find_proxy_pos() {
		while (square_pathing::get_pathing_map(unit_types::scv).path_nodes.empty()) multitasking::sleep(1);
		update_possible_start_locations();

		a_vector<double> cost_map(grid::build_grid_width*grid::build_grid_height);
		auto spread = [&]() {

			double desired_range = units::get_unit_stats(unit_types::scv, players::my_player)->sight_range * 2;
			desired_range = 32 * 15;

			auto spread_from_to = [&](xy from, xy to) {
				a_vector<double> this_cost_map(grid::build_grid_width*grid::build_grid_height);

				tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
				struct node_t {
					grid::build_square*sq;
					double distance;
					double start_distance;
				};
				a_deque<node_t> open;
				auto add_open = [&](xy pos, double start_dist) {
					open.push_back({ &grid::get_build_square(pos), 0.0, start_dist });
					visited.set(grid::build_square_index(pos));
				};
				auto&map = square_pathing::get_pathing_map(unit_types::scv);
				while (map.path_nodes.empty()) multitasking::sleep(1);
				auto path = square_pathing::find_path(map, from, to);
				double path_dist = 0.0;
				xy lp;
				double total_path_dist = square_pathing::get_distance(map, from, to);
				double best_path_dist = total_path_dist*0.5;
				double max_path_dist_diff = std::max(total_path_dist - best_path_dist, best_path_dist);
				add_open(from, 0.0);
				for (auto*n : path) {
					if (lp != xy()) path_dist += diag_distance(n->pos - lp);
					lp = n->pos;
					double d = path_dist;
					add_open(n->pos, d);
				}
				while (!open.empty()) {
					node_t curnode = open.front();
					open.pop_front();
					double offset = desired_range - curnode.distance;
					double variation = (32 * 4)*(32 * 4);
					if (offset < 0) variation = (32 * 6)*(32 * 6);
					double v = exp(-offset*offset / variation) * rng(1.0);;
					if (offset > 0) v -= 1;
					//else v -= 0.25;
					//if (v > 0) v *= 1.0 - (std::abs(curnode.start_distance - best_path_dist) / max_path_dist_diff);
					double&ov = this_cost_map[grid::build_square_index(*curnode.sq)];
					ov = v;
					for (int i = 0; i < 4; ++i) {
						auto*n_sq = curnode.sq->get_neighbor(i);
						if (!n_sq) continue;
						if (!n_sq->entirely_walkable) continue;
						size_t index = grid::build_square_index(*n_sq);
						if (visited.test(index)) continue;
						visited.set(index);
						open.push_back({ n_sq, curnode.distance + 32.0 });
					}
				}
				for (size_t i = 0; i < cost_map.size(); ++i) {
					cost_map[i] += this_cost_map[i];
					//if (cost_map[i] == 0) cost_map[i] = this_cost_map[i];
					//else cost_map[i] = std::min(cost_map[i], this_cost_map[i]);
				}
			};
			for (auto&p : possible_start_locations) {
				for (auto&p2 : start_locations) {
					if (p == p2) continue;
					spread_from_to(p, p2);
					multitasking::yield_point();
				}
				spread_from_to(my_start_location, p);
			}
			// 		for (auto&p : start_locations) spread_from(p, 0.0);
			// 		add_open(my_start_location, 0.0);
		};
		spread();
		test_cost_map = cost_map;

		double best_score;
		xy best_pos;
		for (size_t i = 0; i < cost_map.size(); ++i) {
			xy pos(i % (size_t)grid::build_grid_width * 32, i / (size_t)grid::build_grid_width * 32);
			if (pos.x == 0) multitasking::yield_point();
			double v = cost_map[i];
			if (v == 0) continue;
			double d = get_best_score_value(possible_start_locations, [&](xy p) {
				return diag_distance(pos - p);
			});
			if (d < 32 * 30) continue;
			unit_type*ut = unit_types::barracks;
			if (!build::build_is_valid(grid::get_build_square(pos), ut)) continue;
			v = 0.0;
			for (int y = 0; y < ut->tile_height; ++y) {
				for (int x = 0; x < ut->tile_width; ++x) {
					v += cost_map[grid::build_square_index(pos + xy(x * 32, y * 32))];
				}
			}
			double distance_sum = 0.0;
			for (auto&p : possible_start_locations) distance_sum += unit_pathing_distance(unit_types::scv, pos, p);
			//distance_sum -= unit_pathing_distance(unit_types::scv, pos, my_start_location) * 2;
			distance_sum = distance_sum / (32 * 1);
			double score = v - distance_sum;
			//log("%d %d :: v %g d %g sum %g score %g\n", pos.x, pos.y, v, d, distance_sum, score);
			if (best_pos == xy() || score > best_score) {
				best_score = score;
				best_pos = pos;
			}
		}
		//log("best score is %g\n", best_score);
		return best_pos;
	}

	void run() {

		combat::no_aggressive_groups = true;
		scouting::scout_supply = 50;
		get_upgrades::set_no_auto_upgrades(true);

		refcounted_ptr<build::build_task> bt_rax[2];
		xy proxy_pos[2];

		build::add_build_sum(0, unit_types::scv, 8 - my_completed_units_of_type.size());
		multitasking::sleep(15 * 4);
		bt_rax[0] = build::add_build_task(4, unit_types::barracks);
		bt_rax[1] = build::add_build_task(5, unit_types::barracks);
		build::add_build_task(6, unit_types::supply_depot);
		build::add_build_sum(7, unit_types::marine, 4);

		proxy_pos[0] = find_proxy_pos();
		proxy_pos[1] = find_proxy_pos();
		build::set_build_pos(bt_rax[0], proxy_pos[0]);
		build::set_build_pos(bt_rax[1], proxy_pos[1]);

		unit*scv[2] = { nullptr, nullptr };
		unit*rax[2] = { nullptr, nullptr };

		for (int i = 0; i < 2; ++i) {
			proxy_pos[i] = find_proxy_pos();
			build::set_build_pos(bt_rax[i], proxy_pos[i]);
		}

		int last_find_proxy_pos = 0;
		while (true) {
			bool refind = current_frame - last_find_proxy_pos >= 15 * 10;
			if (refind) last_find_proxy_pos = current_frame;
			for (int i = 0; i < 2; ++i) {
				if (!bt_rax[i]->built_unit) {
					if (bt_rax[i]->build_pos != proxy_pos[i] || refind) {
						build::unset_build_pos(bt_rax[i]);
						proxy_pos[i] = find_proxy_pos();
						build::set_build_pos(bt_rax[i], proxy_pos[i]);
					}
				} else {
					rax[i] = bt_rax[i]->built_unit;
				}
				if (bt_rax[i]->builder != scv[i]) {
					if (scv[i] && scv[i]->dead) throw strat_failed();
					scv[i] = bt_rax[i]->builder;
				}
				if (scv[i] && scv[i]->dead) throw strat_failed();
				if (rax[i] && rax[i]->dead) throw strat_failed();
			}
			if (scv[0] && scv[1] && rax[0] && rax[1]) break;

			multitasking::sleep(90);
		}
		if (true) {
			bool done[2] = { false, false };
			while (true) {
				bool fail = false;
				for (int i = 0; i < 2; ++i) {
					if (done[i]) continue;
					if (scv[i]->dead || rax[i]->dead) fail = true;
					if (rax[i]->is_completed) {
						done[i] = true;
						bt_rax[i] = nullptr;
						scouting::add_scout(scv[i]);
					}
				}
				if (fail) {
					for (int i = 0; i < 2; ++i) {
						bt_rax[i]->dead = true;
						rax[i]->game_unit->cancelConstruction();
					}
					throw strat_failed();
				}
				if (done[0] && done[1]) break;
				multitasking::sleep(4);
			}
		}
// 		for (int i = 0; i < 2; ++i) {
// 			while (!rax[i]->is_completed) {
// 				if (rax[i]->dead || scv[i]->dead) throw strat_failed();
// 				multitasking::sleep(4);
// 			}
// 			bt_rax[i] = nullptr;
// 			scouting::add_scout(scv[i]);
// 		}
		auto test_over = [&]() {
			combat_eval::eval eval;
			for (unit*u : my_units) {
				if (u->type->is_worker && !u->force_combat_unit) continue;
				eval.add_unit(u, 0);
			}
			for (unit*u : enemy_units) {
				if (u->type->is_worker) continue;
				if (!u->stats->ground_weapon) continue;
				eval.add_unit(u, 1);
			}
			eval.run();
			//if (eval.teams[0].end_supply == 0 && eval.teams[1].end_supply>4) throw strat_failed();
			if (eval.teams[0].end_supply == 0 && eval.teams[1].end_supply == eval.teams[1].start_supply) throw strat_failed();
		};

		while (true) {
			if (!combat::no_aggressive_groups) test_over();
			if (current_used_total_supply >= 50) break;

			if (!enemy_buildings.empty()) {
				for (auto&v : scouting::all_scouts) {
					//if (v.scout_unit == scv[0] || v.scout_unit == scv[1]) v.scout_unit = nullptr;
					scouting::rm_scout(v.scout_unit);
				}
			}

			if (my_units_of_type[unit_types::marine].size() >= 2) {
				if (!enemy_units.empty()) {
					scv[0]->force_combat_unit = true;
					scv[1]->force_combat_unit = true;
					//buildpred::attack_now = true;
					combat::no_aggressive_groups = false;
				}
			}
// 			for (auto*a : combat::live_combat_units) {
// 				a->strategy_attack_until = current_frame + 15 * 15;
// 			}
			
			execute_build([](buildpred::state&st) {
				using namespace buildpred;
				return nodelay(st, unit_types::marine, [](state&st) {
					return nodelay(st, unit_types::scv, [](state&st) {
						return depbuild(st, state(st), unit_types::barracks);
					});
				});
			});


			multitasking::sleep(30);
		}

	}
	
	void render() {
		if (true) {
			double highest_v = 0.0;
			for (size_t i = 0; i < test_cost_map.size(); ++i) {
				double v = test_cost_map[i];
				if (v > highest_v) highest_v = v;
				if (-v > highest_v) highest_v = -v;
			}
			for (size_t i = 0; i < test_cost_map.size(); ++i) {
				xy pos(i % (size_t)grid::build_grid_width * 32, i / (size_t)grid::build_grid_width * 32);
				bwapi_pos screen_pos = game->getScreenPosition();
				if (pos.x < screen_pos.x) continue;
				if (pos.x > screen_pos.x + 640) continue;
				if (pos.y < screen_pos.y) continue;
				if (pos.y > screen_pos.y + 480) continue;
				double v = test_cost_map[i];
				if (v == 0) continue;
				if (v < 0) {
					double c = -v / highest_v;
					int byteval = (int)(c * 255 + 0.5);
					if (byteval > 255) byteval = 255;
					game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Color(byteval, byteval / 2, 0));
				} else {
					double c = v / highest_v;
					int byteval = (int)(c * 255 + 0.5);
					if (byteval > 255) byteval = 255;
					game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Color(0, byteval / 2, byteval));
				}
			}

			for (auto&v : possible_start_locations) {
				game->drawLineMap(0, 0, v.x, v.y, BWAPI::Colors::Red);
			}
		}

	}

};

