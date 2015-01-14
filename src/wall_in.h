
namespace wall_in {
;

struct wall_spot {
	bool valid = false;
	a_deque<xy> path;
	a_vector<grid::build_square*> build_squares;
	double wall_width;
	xy inside;
	xy center;
	xy outside;
};

void generate(unit_type*path_ut, wall_spot&ws) {

	ws.valid = false;
	ws.build_squares.clear();

	if (!grid::get_build_square(ws.center).buildable) return;

	double max_center_distance = std::max(diag_distance(ws.inside - ws.center), diag_distance(ws.outside - ws.center));
	log("generate %d %d  %d %d  %d %d\n", ws.inside.x, ws.inside.y, ws.center.x, ws.center.y, ws.outside.x, ws.outside.y);

	tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
	struct node_t {
		node_t*prev;
		grid::build_square*bs;
	};
	a_deque<node_t*> open;
	a_deque<node_t> nodes;
	nodes.push_back({ nullptr, &grid::get_build_square(ws.center) });
	open.emplace_back(&nodes.back());
	visited.set(grid::build_square_index(ws.center));
	a_unordered_set<node_t*> edges;
	int iterations = 0;
	while (!open.empty()) {
		++iterations;
		node_t*cn = open.front();
		open.pop_front();

		for (int n = 0; n < 4;++n) {
			grid::build_square*nbs = cn->bs->get_neighbor(n);
			if (!nbs) continue;
			size_t index = grid::build_square_index(*nbs);
			if (visited.test(index)) continue;
			visited.set(index);
			//if (diag_distance(nbs->pos - ws.center) >= max_center_distance - 32) continue;
			if (!nbs->buildable) {
				if (!nbs->entirely_walkable) {
					size_t len = 0;
					for (node_t*i = cn; i->prev; i = i->prev) ++len;
					for (auto*en : edges) {

						tsc::dynamic_bitset walled(grid::build_grid_width*grid::build_grid_height);
						for (node_t*i = cn; i; i = i->prev) walled.set(grid::build_square_index(*i->bs));
						for (node_t*i = en; i; i = i->prev) walled.set(grid::build_square_index(*i->bs));
						walled.set(grid::build_square_index(ws.center));

						a_deque<xy> path = square_pathing::find_square_path(square_pathing::get_pathing_map(path_ut), ws.outside, [&](xy pos, xy npos) {
							if (diag_distance(npos - ws.center) >= max_center_distance + 32) return false;
							if (walled.test(grid::build_square_index(npos))) return false;
							return true;
						}, [&](xy pos, xy npos) {
							return diag_distance(npos - ws.inside);
						}, [&](xy pos) {
							return manhattan_distance(ws.inside - pos) <= 32;
						});
						multitasking::yield_point();
						if (path.empty()) {
							for (node_t*i = cn; i; i = i->prev) ws.build_squares.push_back(i->bs);
							for (node_t*i = en; i; i = i->prev) ws.build_squares.push_back(i->bs);
							ws.wall_width = diag_distance(cn->bs->pos - ws.center) + diag_distance(en->bs->pos - ws.center);
							ws.build_squares.push_back(&grid::get_build_square(ws.center));
							ws.valid = true;
							log("generated spot after %d iterations\n", iterations);
							return;
						}
					}
					edges.insert(cn);
				}
			} else {
				nodes.push_back({ cn, nbs });
				open.emplace_back(&nodes.back());
			}
		};
	}
	log("failed to generate spot after %d iterations\n", iterations);
}

wall_spot find_wall_spot_from_to(unit_type*path_ut, xy from, xy to, bool test_wall) {
	while (square_pathing::get_pathing_map(path_ut).path_nodes.empty()) multitasking::sleep(1);
	size_t iterations = 0;
	tsc::high_resolution_timer ht;
	a_deque<xy> path = square_pathing::find_square_path(square_pathing::get_pathing_map(path_ut), from, [&](xy pos, xy npos) {
		if (++iterations % 1024 == 0) multitasking::yield_point();
		return true;
	}, [&](xy pos, xy npos) {
		return 0.0;
	}, [&](xy pos) {
		return manhattan_distance(to - pos) <= 32;
	});
	const size_t n_space = 32;
	double len = 0.0;
	for (size_t i = 1; i < path.size(); ++i) {
		len += diag_distance(path[i] - path[i - 1]);
		if (len >= 32 * 40) {
			path.resize(std::min(i + n_space * 2, path.size()));
			break;
		}
	}
	bool has_path2 = false;
	a_vector<xy> path2;

	double best_score = std::numeric_limits<double>::infinity();
	wall_spot best;
	grid::build_square*prev_bs = nullptr;
	for (size_t i = 0; i + n_space * 2 < path.size(); ++i) {
		wall_spot ws;
		ws.center = path[i + n_space];
		auto*bs = &grid::get_build_square(ws.center);
		if (bs == prev_bs) continue;
		prev_bs = bs;

		ws.inside = get_best_score(make_iterators_range(path.begin() + i, path.begin() + i + n_space), [&](xy pos) {
			return -diag_distance(pos - ws.center);
		});
		ws.outside = get_best_score(make_iterators_range(path.begin() + i + n_space, path.begin() + i + n_space + n_space), [&](xy pos) {
			return -diag_distance(pos - ws.center);
		});
		generate(path_ut, ws);
		if (ws.valid) {
			if (test_wall) {
				tsc::dynamic_bitset walled(grid::build_grid_width*grid::build_grid_height);
				for (auto*bs : ws.build_squares) walled.set(grid::build_square_index(*bs));
				a_deque<xy> new_path = square_pathing::find_square_path(square_pathing::get_pathing_map(path_ut), from, [&](xy pos, xy npos) {
					if (++iterations % 1024 == 0) multitasking::yield_point();
					if (walled.test(grid::build_square_index(npos))) return false;
					return true;
				}, [&](xy pos, xy npos) {
					return 0.0;
				}, [&](xy pos) {
					return manhattan_distance(to - pos) <= 32;
				});
				ws.valid = new_path.empty();
			}
			if (ws.valid) {
				double s = ws.wall_width;
				s -= (grid::get_build_square(ws.center).height - grid::get_build_square(ws.outside).height) * 32 * 6;
				log("found valid spot with score %g\n", s);
				if (s < best_score) {
					best_score = s;
					best = std::move(ws);
				}
			}
		} else log("not valid :(\n");
	}
	log("find_wall_spot_from_to done\n");
	best.path = path;
	return best;
}

wall_spot default_find_wall_spot(unit_type*path_ut) {
	return find_wall_spot_from_to(path_ut, my_start_location, get_best_score(start_locations, [&](xy pos) {
		return -diag_distance(pos - my_start_location);
	}), true);
}


struct wall_builder {

	wall_spot spot;
	unit_type*against_ut = nullptr;
	a_vector<unit_type*> buildings;
	a_vector<xy> buildings_pos;

	wall_builder();
	~wall_builder();

	void against(unit_type*ut) {
		against_ut = ut;
	}

	void add_building(unit_type*ut) {
		buildings.push_back(ut);
	}

	bool find() {
		const size_t buildings_n = buildings.size();
		a_vector<a_vector<xy>> build_spots;
		//std::minstd_rand shuffle_rng(11729);
		auto&shuffle_rng = tsc::rng_engine;
		for (size_t i = 0; i < buildings_n; ++i) {
			a_set<xy> set;
			unit_type*ut = buildings[i];
			auto find_build_spots = [&](int leeway) {
				for (auto*bs : spot.build_squares) {
					xy pos = bs->pos;
					for (int y = -ut->tile_height + 1 - leeway; y <= leeway; ++y) {
						for (int x = -ut->tile_width + 1 - leeway; x <= leeway; ++x) {
							if (build_spot_finder::is_buildable_at(ut, pos + xy(x * 32, y * 32))) set.insert(pos + xy(x * 32, y * 32));
						}
					}
				}
			};
			for (int i = 0; i < 16 && set.size() < 40; ++i) {
				find_build_spots(i);
			}
			a_vector<xy> vec;
			for (auto&v : set) vec.push_back(v);
			log("%d: %d build spots\n", i, vec.size());
			std::shuffle(vec.begin(), vec.end(), shuffle_rng);
			build_spots.push_back(std::move(vec));
		}
		a_vector<xy> build_pos(buildings_n);
		a_vector<size_t> index(buildings_n);
		size_t n = 0;

		auto&pathing_map = square_pathing::get_pathing_map(against_ut);
		while (pathing_map.path_nodes.empty()) multitasking::sleep(1);
		auto&siege_tank_pathing_map = square_pathing::get_pathing_map(unit_types::siege_tank_tank_mode);
		while (siege_tank_pathing_map.path_nodes.empty()) multitasking::sleep(1);

		auto can_fit_at = [&](xy pos, unit_type*ut, bool include_liftable) {
			using namespace grid;
			auto&dims = ut->dimensions;
			int right = pos.x + dims[0];
			int bottom = pos.y + dims[1];
			int left = pos.x - dims[2];
			int top = pos.y - dims[3];
			for (size_t i = 0; i < buildings_n; ++i) {
				unit_type*bt = buildings[i];
				if (!include_liftable && bt->is_liftable) continue;
				xy bpos = build_pos[i] + xy(bt->tile_width * 16, bt->tile_height * 16);
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
		double max_center_distance = std::max(diag_distance(spot.inside - spot.center), diag_distance(spot.outside - spot.center));

		bool found = false;
		size_t iterations = 0;
		while (true) {
			++iterations;
			if (n == 0) multitasking::yield_point();
			//if (iterations >= 250000) break;
			if (index[n] >= build_spots[n].size()) {
				if (n == 0) break;
				std::shuffle(build_spots[n].begin(), build_spots[n].end(), shuffle_rng);
				index[n] = 0;
				--n;
				continue;
			}
			xy upper_left = build_spots[n][index[n]++];
			xy bottom_right = upper_left + xy(buildings[n]->tile_width * 32, buildings[n]->tile_height * 32);
			bool valid = true;
			for (size_t i = 0; i < n; ++i) {
				xy n_upper_left = build_pos[i];
				xy n_bottom_right = n_upper_left + xy(buildings[i]->tile_width * 32, buildings[i]->tile_height * 32);
				if (bottom_right.x <= n_upper_left.x) continue;
				if (bottom_right.y <= n_upper_left.y) continue;
				if (n_bottom_right.x <= upper_left.x) continue;
				if (n_bottom_right.y <= upper_left.y) continue;
				valid = false;
				break;
			}
			if (valid) {
				valid = n == 0;
				for (size_t i = 0; i < n; ++i) {
					xy n_upper_left = build_pos[i];
					xy n_bottom_right = n_upper_left + xy(buildings[i]->tile_width * 32, buildings[i]->tile_height * 32);
					xy a0 = upper_left;
					xy a1 = bottom_right;
					xy b0 = n_upper_left;
					xy b1 = n_bottom_right;

					int x, y;
					if (a0.x > b1.x) x = a0.x - b1.x;
					else if (b0.x > a1.x) x = b0.x - a1.x;
					else x = 0;
					if (a0.y > b1.y) y = a0.y - b1.y;
					else if (b0.y > a1.y) y = b0.y - a1.y;
					else y = 0;
					if (x == 0 && y == 0) {
						valid = true;
						break;
					}
				}
			}
			//log("build spot %d %d for %d is %s\n", upper_left.x, upper_left.y, n, valid ? "valid" : "not valid");
			if (!valid) continue;
			build_pos[n] = upper_left;
			if (n == buildings_n - 1) {
				if (!path_pred(spot.outside, against_ut, true) || !path_pred(spot.inside, against_ut, true)) continue;
				a_deque<xy> path = square_pathing::find_square_path(pathing_map, spot.outside, [&](xy pos, xy npos) {
					if (diag_distance(npos - spot.center) >= max_center_distance * 2 + 32) return false;
					return path_pred(npos, against_ut, true);
				}, [&](xy pos, xy npos) {
					return diag_distance(npos - spot.inside);
				}, [&](xy pos) {
					return manhattan_distance(spot.inside - pos) <= 32;
				});
				multitasking::yield_point();
				//log("path.size() is %d\n", path.size());
				if (path.empty()) {
					bool test_siege_tank = true;
					bool okay = !test_siege_tank;
					if (test_siege_tank) {
						xy siege_tank_inside = get_nearest_path_node(siege_tank_pathing_map, spot.inside)->pos;
						xy siege_tank_outside = get_nearest_path_node(siege_tank_pathing_map, spot.outside)->pos;
						size_t path_iterations = 0;
						a_deque<xy> path = square_pathing::find_square_path(siege_tank_pathing_map, siege_tank_inside, [&](xy pos, xy npos) {
							if (++path_iterations >= 1024) multitasking::yield_point();
							if (diag_distance(npos - spot.center) >= max_center_distance * 2 + 32) return false;
							return path_pred(npos, unit_types::siege_tank_tank_mode, false);
						}, [&](xy pos, xy npos) {
							return diag_distance(npos - siege_tank_outside);
						}, [&](xy pos) {
							return manhattan_distance(siege_tank_outside - pos) <= 64;
						});
						okay = !path.empty();
					}
					if (okay) {
						found = true;
						break;
					}
				}
			} else ++n;
		}
		buildings_pos.clear();
		if (found) buildings_pos = build_pos;
		if (found) log("wall_builder::find succeeded\n");
		else log("wall_builder::find failed\n");
		log("wall_builder::find: %d iterations\n", iterations);

		return found;
	}

	bool build() {

		log("build wall!\n");

		a_vector<bool> found(buildings.size());
		a_unordered_set<build::build_task*> build_task_taken;
		for (auto&b : build::build_tasks) {
			for (size_t i = 0; i < buildings_pos.size(); ++i) {
				if (b.build_pos == buildings_pos[i]) {
					log("found wall %d\n", i);
					found[i] = true;
					build_task_taken.insert(&b);
					if (b.built_unit) log("%s->is_liftable is %d\n", b.built_unit->type->name, b.built_unit->type->is_liftable);
					if (b.built_unit && b.built_unit->type->is_liftable) {
						if (!b.built_unit->building->is_liftable_wall) {
							unit*u = b.built_unit;
							xy upper_left = u->pos - xy(u->type->dimension_left(), u->type->dimension_up());
							xy bottom_right = u->pos + xy(u->type->dimension_right(), u->type->dimension_down());
							square_pathing::invalidate_area(upper_left, bottom_right);
						}
						b.built_unit->building->is_liftable_wall = true;
						log("%s marked as liftable wall\n", b.built_unit->type->name);
					}
					break;
				}
			}
		}
		int remaining = 0;
		for (size_t i = 0; i < buildings_pos.size(); ++i) {
			if (found[i]) continue;
			if (!build_spot_finder::is_buildable_at(buildings[i], buildings_pos[i])) continue;
			++remaining;
			for (auto&b : build::build_tasks) {
				if (b.built_unit) continue;
				if (b.type->unit != buildings[i]) continue;
				if (build_task_taken.count(&b)) continue;
				build::set_build_pos(&b, buildings_pos[i]);
				build_task_taken.insert(&b);
				log("build wall %d!\n", i);
				break;
			}
		}
		log("remaining is %d\n", remaining);

		return remaining != 0;
	}

	void render() {
		bwapi_pos screen_pos = game->getScreenPosition();
		xy lp;
		for (auto&v : spot.path) {
			if (v.x < screen_pos.x || v.y < screen_pos.y) continue;
			if (v.x >= screen_pos.x + 640 || v.y >= screen_pos.y + 400) continue;
			if (lp != xy()) game->drawLineMap(lp.x, lp.y, v.x, v.y, BWAPI::Colors::Yellow);
			lp = v;
		}
		for (auto*bs : spot.build_squares) {
			xy pos = bs->pos;
			game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Colors::Yellow);
		}
		game->drawCircleMap(spot.inside.x, spot.inside.y, 16, BWAPI::Color(128, 255, 0));
		game->drawCircleMap(spot.center.x, spot.center.y, 12, BWAPI::Color(255, 255, 0));
		game->drawCircleMap(spot.outside.x, spot.outside.y, 16, BWAPI::Color(255, 128, 0));

		for (size_t i = 0; i < buildings_pos.size(); ++i) {
			unit_type*ut = buildings[i];
			xy pos = buildings_pos[i];
			game->drawBoxMap(pos.x, pos.y, pos.x + ut->tile_width * 32, pos.y + ut->tile_height * 32, BWAPI::Colors::Orange);
		}
	}

};

a_vector<wall_builder*> active_walls;
wall_builder::wall_builder() {
	active_walls.push_back(this);
}
wall_builder::~wall_builder() {
	find_and_erase(active_walls, this);
}

a_set<unit*> lift_queue;

void lift_please(unit*building) {
	lift_queue.insert(building);
}

int active_wall_count() {
	return active_walls.size();
}

void lift_wall_task() {
	while (true) {

		for (unit*u : my_buildings) {
			if (u->building->is_liftable_wall) {
				bool lift = lift_queue.count(u) != 0;
				bool low_prio_lift = false;
				if (!lift) {
					for (unit*nu : my_units) {
						if (nu->controller->action == unit_controller::action_gather || nu->controller->action == unit_controller::action_attack) {
							if (units_distance(u, nu) <= 32 * 2) {
								low_prio_lift = true;
								break;
							}
						}
					}
				}
				if (lift || low_prio_lift) {
					if (lift) u->controller->noorder_until = current_frame + 15;
					if (!u->building->is_lifted) {
						double my_army = 0;
						combat_eval::eval eval;
						for (unit*nu : my_units) {
							if (nu->building || nu->type->is_worker) continue;
							if (diag_distance(nu->pos - u->pos) > 32 * 15) continue;
							my_army += nu->type->required_supply;
							eval.add_unit(nu, 0);
						}
						double op_army = 0;
						for (unit*nu : enemy_units) {
							if (nu->building || nu->type->is_worker) continue;
							if (diag_distance(nu->pos - u->pos) > 32 * 15) continue;
							op_army += nu->type->required_supply;
							eval.add_unit(nu, 1);
						}
						eval.run();
						log("lift - my_army %g op_army %g - scores %g %g\n", my_army, op_army, eval.teams[0].score, eval.teams[1].score);
						if (op_army == 0 || eval.teams[0].score > eval.teams[1].score) u->game_unit->lift();
						//if (op_army == 0 || my_army >= op_army + 4) u->game_unit->lift();
					}
				} else {
					if (u->building->is_lifted) {
						a_vector<xy> avail_pos;
						for (auto*v : active_walls) {
							for (size_t i = 0; i < v->buildings.size(); ++i) {
								if (v->buildings[i] == u->type && !grid::get_build_square(v->buildings_pos[i]).building) {
									avail_pos.push_back(v->buildings_pos[i]);
								}
							}
						}
						if (!avail_pos.empty()) {
							xy pos = get_best_score(avail_pos, [&](xy pos) {
								return diag_distance(pos - u->building->build_pos);
							});
							u->controller->noorder_until = current_frame + 15;
							u->game_unit->land(BWAPI::TilePosition(pos.x / 32, pos.y / 32));
						}
					}
				}

				bool found = false;
				for (auto*v : active_walls) {
					for (xy pos : v->buildings_pos) {
						if (pos == u->building->build_pos) {
							found = true;
							break;
						}
					}
					if (found) break;
				}
				if (!found) {
					if (!u->building->is_lifted) {
						u->controller->noorder_until = current_frame + 15;
						if (u->game_unit->lift()) u->building->is_liftable_wall = false;
					}
				}
			}
		}

		lift_queue.clear();

		multitasking::sleep(8);
	}
}

void init() {

	multitasking::spawn(lift_wall_task, "lift wall");

}

}
