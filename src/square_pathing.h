
namespace square_pathing {
;

bool can_fit_at(const xy pos, const std::array<int,4>&dims) {
	using namespace grid;
	int right = pos.x + dims[0];
	int bottom = pos.y + dims[1];
	int left = pos.x - dims[2];
	int top = pos.y - dims[3];
	walk_square*sq = &get_walk_square(xy(left, top));
	for (int y = top&-8; y <= bottom; y += 8) {
		walk_square*nsq = sq;
		for (int x = left&-8; x <= right; x += 8) {
			if (!nsq->walkable) return false;
			for (unit*b : nsq->buildings) {
				xy bpos = b->pos;
				int bright = bpos.x + b->type->dimension_right();
				int bbottom = bpos.y + b->type->dimension_down();
				int bleft = bpos.x - b->type->dimension_left();
				int btop = bpos.y - b->type->dimension_up();
				if (right >= bleft && bottom >= btop && bright >= left && bbottom >= top) return false;
			}
			nsq = &walk_square_neighbor(*nsq, 0);
		}
		sq = &walk_square_neighbor(*sq, 1);
	}
	return true;
};

struct path_node {
	xy pos;
	a_vector<path_node*> neighbors;
	size_t root_index;
};

struct pathing_map {
	bool initialized = false;
	std::array<int, 4> dimensions;
	tsc::dynamic_bitset walkable;
	
	a_vector<path_node> path_nodes;
	a_vector<path_node*> nearest_path_node;
	bool path_nodes_requires_update = false;
	bool update_path_nodes = false;

	size_t path_node_index(path_node&n) {
		return &n - path_nodes.data();
	}
};

size_t walkmap_width;
size_t walkmap_height;
size_t nearest_path_node_width;
size_t nearest_path_node_height;

tsc::dynamic_bitset test_walkable;

a_list<pathing_map> all_pathing_maps;

a_unordered_map<unit_type*, pathing_map*> pathing_map_for_unit_type;

a_vector<std::tuple<xy, xy>> invalidation_queue;

void invalidate_area(xy from, xy to) {
	invalidation_queue.emplace_back(from, to);
}

pathing_map&get_pathing_map(unit_type*ut) {
	auto*&r = pathing_map_for_unit_type[ut];
	if (r) return *r;
	all_pathing_maps.emplace_back();
	r = &all_pathing_maps.back();
	r->dimensions = ut->dimensions;
	r->walkable.resize(walkmap_width * walkmap_height);
	r->nearest_path_node.resize(nearest_path_node_width*nearest_path_node_height);
	return *r;
}

size_t walk_pos_index(xy pos) {
	return (size_t)pos.x / 8 + (size_t)pos.y / 8 * walkmap_width;
}

size_t nearest_path_node_index(xy pos) {
	return (size_t)pos.x / 8 + (size_t)pos.y / 8 * nearest_path_node_width;
}

xy get_pos_in_square(xy pos, unit_type*ut) {
	auto&dims = ut->dimensions;
	pos.x &= -8;
	pos.y &= -8;
	if (can_fit_at(pos, dims)) return pos;
	if (can_fit_at(pos + xy(7, 0), dims)) return pos + xy(7, 0);
	if (can_fit_at(pos + xy(0, 7), dims)) return pos + xy(0, 7);
	if (can_fit_at(pos + xy(7, 7), dims)) return pos + xy(7, 7);
	for (int x = 1; x < 7; ++x) {
		if (can_fit_at(pos + xy(x, 0), dims)) return pos + xy(x, 0);
		if (can_fit_at(pos + xy(x, 7), dims)) return pos + xy(x, 7);
	}
	for (int y = 1; y < 7; ++y) {
		if (can_fit_at(pos + xy(0, y), dims)) return pos + xy(0, y);
		if (can_fit_at(pos + xy(7, y), dims)) return pos + xy(7, y);
	}
	return xy();
}

void update_map(pathing_map&map, xy from, xy to) {

	tsc::high_resolution_timer ht;
	double elapsed = 0.0;

	map.path_nodes_requires_update = true;

	auto dims = map.dimensions;

	if (from.x < 0) from.x = 0;
	if (from.y < 0) from.y = 0;
	if (to.x < 0) to.x = 0;
	if (to.y < 0) to.y = 0;

	for (int y = from.y&-8; y <= to.y; y += 8) {
		if (y >= grid::map_height) break;
		for (int x = from.x&-8; x <= to.x; x += 8) {
			if (x >= grid::map_width) break;
			
			xy pos = xy(x, y);

			if (!grid::get_walk_square(pos).walkable) continue;

			bool kay = can_fit_at(pos, dims) || can_fit_at(pos + xy(7,0),dims);
			if (!kay) kay |= can_fit_at(pos + xy(0, 7), dims) || can_fit_at(pos + xy(7, 7), dims);
			if (!kay) {
				for (int x = 1; x < 7 && !kay; ++x) {
					if (can_fit_at(pos + xy(x, 0), dims)) kay = true;
					else if (can_fit_at(pos + xy(x, 7), dims)) kay = true;
				}
				for (int y = 1; y < 7 && !kay; ++y) {
					if (can_fit_at(pos + xy(0, y), dims)) kay = true;
					else if (can_fit_at(pos + xy(7, y), dims)) kay = true;
				}
			}
			size_t index = walk_pos_index(xy(x, y));
			if (kay) map.walkable.set(index);
			else map.walkable.reset(index);

		}
		elapsed += ht.elapsed();
		multitasking::yield_point();
		ht.reset();
	}

	elapsed += ht.elapsed_and_reset();

	log("updated map in %f\n", elapsed);

}

void generate_path_nodes(pathing_map&map) {

	tsc::high_resolution_timer ht;
	double elapsed = 0.0;

	auto dims = map.dimensions;

	a_vector<path_node> new_path_nodes;

	tsc::dynamic_bitset visited(walkmap_width*walkmap_height);
	a_deque<std::tuple<xy,path_node*>> open;
	std::priority_queue<std::tuple<double, xy, path_node*>> open2;

	for (int y = 0; y < grid::map_height; y += 8) {
		for (int x = 0; x < grid::map_width; x += 8) {
			xy start_pos(x, y);
			size_t start_index = walk_pos_index(start_pos);
			if (visited.test(start_index)) continue;
			if (!map.walkable.test(start_index)) continue;
			size_t root_index = ~(size_t)0;
			open2.emplace(0.0, start_pos, nullptr);

			int iterations = 0;
			while (!open.empty() || !open2.empty()) {
				++iterations;

				xy pos;
				path_node*curnode;
				if (!open.empty()) {
					std::tie(pos, curnode) = open.front();
					open.pop_front();
				} else {
					std::tie(std::ignore, pos, curnode) = open2.top();
					open2.pop();
				}

				if (!curnode) {
					size_t index = walk_pos_index(pos);
					if (visited.test(index)) continue;
					visited.set(index);
					new_path_nodes.emplace_back();
					curnode = &new_path_nodes.back();
					if (root_index == ~(size_t)0) root_index = new_path_nodes.size() - 1;
					curnode->pos = pos;
					curnode->root_index = root_index;
				}

				auto add = [&](int n) {
					xy npos = pos;	
					if (n == 0) npos.x += 8;
					if (n == 1) npos.y += 8;
					if (n == 2) npos.x -= 8;
					if (n == 3) npos.y -= 8;
					if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) return;

					size_t index = walk_pos_index(npos);
					if (!map.walkable.test(index)) return;
					if (!visited.test(index)) {
						path_node*next_node = curnode;
						if (diag_distance(pos - curnode->pos) >= 32 * 8) next_node = nullptr;
						if (next_node) visited.set(index);

						if (next_node) open.emplace_back(npos, next_node);
						else open2.emplace(manhattan_distance(npos - pos), npos, next_node);
					}

				};
				add(0);
				add(1);
				add(2);
				add(3);

				if (iterations % 0x100 == 0) {
					elapsed += ht.elapsed();
					multitasking::yield_point();
					ht.reset();
				}
			}
		}
	}

	for (auto&v : new_path_nodes) {
		if (v.root_index == -1) continue;
		for (auto&v2 : new_path_nodes) {
			if (&v == &v2) continue;
			if (v.root_index != v2.root_index) continue;
			if (diag_distance(v.pos - v2.pos) <= 32 * 2) {
				v2.root_index = -1;
			}
		}
	}
	for (size_t i = 0; i < new_path_nodes.size(); ++i) {
		if (new_path_nodes[i].root_index == -1) {
			if (i < new_path_nodes.size() - 1) std::swap(new_path_nodes[i], new_path_nodes[new_path_nodes.size() - 1]);
			new_path_nodes.pop_back();
		}
	}

	auto add_neighbor = [&](path_node*node, path_node*neighbor) {
		for (auto*ptr : node->neighbors) {
			if (ptr == neighbor) return;
		}
		node->neighbors.push_back(neighbor);
	};

	a_vector<path_node*> new_nearest_path_node;

	new_nearest_path_node.resize(nearest_path_node_width*nearest_path_node_height);

	{
		visited.reset();
		a_deque<std::tuple<xy, path_node*>> open2;

		for (auto&v : new_path_nodes) {
			open.emplace_back(v.pos, &v);
			visited.set(walk_pos_index(v.pos));
		}

		int iterations = 0;
		while (!open.empty() || !open2.empty()) {
			++iterations;

			bool from_open = !open.empty();

			xy pos;
			path_node*curnode;
			std::tie(pos, curnode) = from_open ? open.front() : open2.front();
			from_open ? open.pop_front() : open2.pop_front();

			auto*&v_ptr = new_nearest_path_node[nearest_path_node_index(pos)];
			v_ptr = curnode;

			auto add = [&](int n) {
				xy npos = pos;
				if (n == 0) npos.x += 8;
				if (n == 1) npos.y += 8;
				if (n == 2) npos.x -= 8;
				if (n == 3) npos.y -= 8;
				if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) return;

				size_t index = walk_pos_index(npos);
				bool walkable = map.walkable.test(index);
				
				if (from_open && walkable) {
					auto*nn = new_nearest_path_node[nearest_path_node_index(npos)];
					if (nn && nn != curnode) {
						add_neighbor(curnode, nn);
						add_neighbor(nn, curnode);
					}
				}

				if (!visited.test(index)) {
					visited.set(index);

					(walkable ? open : open2).emplace_back(npos, curnode);
				}

			};
			add(0);
			add(1);
			add(2);
			add(3);

			if (iterations % 0x100 == 0) {
				elapsed += ht.elapsed();
				multitasking::yield_point();
				ht.reset();
			}
		}
	}

	
	map.path_nodes = std::move(new_path_nodes);
	map.nearest_path_node = std::move(new_nearest_path_node);

	elapsed += ht.elapsed_and_reset();

	log("%d nodes generated in %f\n", map.path_nodes.size(), elapsed);

}

path_node*get_nearest_path_node(pathing_map&map, xy pos) {
	if (map.path_nodes_requires_update) map.update_path_nodes = true;
	if (pos.x < 0) pos.x = 0;
	if (pos.x >= grid::map_width) pos.x = grid::map_width - 1;
	if (pos.y < 0) pos.y = 0;
	if (pos.y >= grid::map_height) pos.y = grid::map_height - 1;
	return map.nearest_path_node[nearest_path_node_index(pos)];
}

xy get_nearest_node_pos(unit*u) {
	auto*n = square_pathing::get_nearest_path_node(square_pathing::get_pathing_map(u->type), u->pos);
	if (!n) return xy();
	return n->pos;
}

struct closed_t {
	closed_t*prev;
	path_node*node;
};
struct open_t {
	closed_t*prev;
	path_node*node;
	double distance;
	double est_distance;
	bool operator<(const open_t&n) const {
		if (est_distance != n.est_distance) return est_distance > n.est_distance;
		return distance > n.distance;
	}
};

template<typename goal_T>
void find_path(pathing_map&map, path_node*from, path_node*to, xy from_pos, xy to_pos, goal_T&&goal) {
	a_list<closed_t> closed;
	std::priority_queue<open_t, a_vector<open_t>> open;
	tsc::dynamic_bitset visited(map.path_nodes.size());

	open.push({ nullptr, from, 0.0, 0.0 });
	visited.set(map.path_node_index(*from));

	int iterations = 0;
	while (!open.empty()) {
		++iterations;

		open_t cur = open.top();
		if (cur.node == to) {
			goal(cur);
			break;
		}
		open.pop();

		closed.push_back({ cur.prev, cur.node });
		closed_t&closed_node = closed.back();

		for (auto*n : cur.node->neighbors) {
			size_t index = map.path_node_index(*n);
			if (visited.test(index)) continue;
			visited.set(index);

			double d;
			xy n_pos = n->pos;
			if (n == to) n_pos = to_pos;
			if (cur.node == from) d = diag_distance(n_pos - from_pos);
			else d = diag_distance(n_pos - cur.node->pos);

			double distance = cur.distance + d;
			double est_distance = distance + diag_distance(n->pos - to->pos);
			open.push({ &closed_node, n, distance, est_distance });
		}

	}
}

double get_distance(pathing_map&map, xy from_pos, xy to_pos) {
	path_node*from = get_nearest_path_node(map, from_pos);
	path_node*to = get_nearest_path_node(map, to_pos);
	double r = std::numeric_limits<double>::infinity();
	if (!from || !to) return r;
	if (from->root_index != to->root_index) return r;

	find_path(map, from, to, from_pos, to_pos, [&](open_t&cur) {
		r = cur.distance;
	});

	return r;
}

a_deque<path_node*> find_path(pathing_map&map, path_node*from, path_node*to) {
	a_deque<path_node*> r;
	if (!from || !to) return r;
	if (from->root_index != to->root_index) return r;

	tsc::high_resolution_timer ht;

	find_path(map, from, to, from->pos, to->pos, [&](open_t&cur) {
		r.push_front(cur.node);
		for (closed_t*n = cur.prev; n && n->prev; n = n->prev) {
			r.push_front(n->node);
		}
	});

	//log("find path found path with %d nodes in %d iterations (%fs)\n", r.size(), iterations, ht.elapsed());

	return r;
}

a_deque<path_node*> find_path(pathing_map&map, xy from, xy to) {
	return find_path(map, get_nearest_path_node(map, from), get_nearest_path_node(map, to));
}

template<typename pred_T, typename est_dist_T, typename goal_T>
a_deque<xy> find_square_path(pathing_map&map, xy from, pred_T&&pred, est_dist_T&est_dist, goal_T&&goal) {
	tsc::high_resolution_timer ht;
	a_deque<xy> r;
	struct closed_t {
		closed_t*prev;
		xy pos;
	};
	struct open_t {
		closed_t*prev;
		xy pos;
		double distance;
		double est_distance;
		int n;
		bool operator<(const open_t&n) const {
			if (est_distance != n.est_distance) return est_distance > n.est_distance;
			return distance > n.distance;
		}
	};
	a_list<closed_t> closed;
	std::priority_queue<open_t, a_vector<open_t>> open;
	tsc::dynamic_bitset visited(walkmap_width*walkmap_height);

	open.push({ nullptr, from, 0, 0, -1 });
	visited.set(walk_pos_index(from));

	int iterations = 0;
	while (!open.empty()) {
		++iterations;

		open_t cur = open.top();
		//if (cur.pos.x <= to.x && cur.pos.y <= to.y && cur.pos.x + 8 > to.x && cur.pos.y + 8 > to.y) {
		if (goal(cur.pos)) {
			r.push_front(cur.pos);
			for (closed_t*n = cur.prev; n && n->prev; n = n->prev) {
				r.push_front(n->pos);
			}
			break;
		}
		open.pop();

		closed.push_back({ cur.prev, cur.pos });
		closed_t&closed_node = closed.back();

		auto add = [&](int n) {
			xy npos = cur.pos;
			if (n == 0) npos.x += 8;
			if (n == 1) npos.y += 8;
			if (n == 2) npos.x -= 8;
			if (n == 3) npos.y -= 8;
			if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) return;
			size_t index = walk_pos_index(npos);
			if (visited.test(index)) return;
			visited.set(index);
			if (!map.walkable.test(index)) return;
			if (!pred(cur.pos, npos)) return;

			double distance = cur.distance + (cur.n == -1 || cur.n % 2 == n % 2 ? 8 : 11.313708498984760390413509793678);
			double est_distance = distance + est_dist(cur.pos, npos);
			open.push({ &closed_node, npos, distance, est_distance, n });
		};
		add(0);
		add(1);
		add(2);
		add(3);
	}

	//log("find square path found path with %d nodes in %d iterations (%fs)\n", r.size(), iterations, ht.elapsed());

	return r;
}

size_t force_field_size = 32;
size_t force_field_grid_width;
size_t force_field_grid_height;
a_vector<std::tuple<int, int, double, unit*>> force_field;
size_t force_field_index(xy pos) {
	return (size_t)pos.x / force_field_size + (size_t)pos.y / force_field_size * force_field_grid_width;
};

a_deque<xy> render_node_path;
a_deque<xy> render_path;

xy get_move_to(unit*u, xy goal, int priority) {

	auto&map = get_pathing_map(u->type);
	path_node*from_node = get_nearest_path_node(map, u->pos);
	path_node*to_node = get_nearest_path_node(map, goal);
	auto path = find_path(map, from_node, to_node);
	if (path.empty()) return goal;
	path_node*next_node = path.front();

	render_node_path.clear();
	for (auto&v : path) render_node_path.emplace_back(v->pos);

	double angle = atan2(u->pos.y - next_node->pos.y, u->pos.x - next_node->pos.x);

	auto square_path = find_square_path(map, u->pos, [&](xy pos, xy npos) {
		path_node*nn = map.nearest_path_node[nearest_path_node_index(npos)];
		if (nn != from_node && nn != next_node) return false;
		auto&ff = force_field[force_field_index(npos)];
		if (std::get<0>(ff) >= current_frame - 15 && std::get<1>(ff) <= priority) {
			double da = std::abs(angle - std::get<2>(ff));
			if (da > PI) da = -da + PI * 2;
			if (da >= PI / 2) return false;
		}
		return true;
	}, [&](xy pos, xy npos) {
		return diag_distance(npos - next_node->pos);
	}, [&](xy pos) {
		return pos.x / 8 == next_node->pos.x / 8 && pos.y / 8 == next_node->pos.y / 8;
	});
	if (diag_distance(u->pos - goal) <= 32) {
		auto&ff = force_field[force_field_index(goal)];
		if (std::get<0>(ff) >= current_frame - 15 && std::get<1>(ff) <= priority) {
			square_path.clear();
		}
	}

	render_path = square_path;

	if (square_path.empty()) {
		auto escape_path = find_square_path(map, u->pos, [&](xy pos,xy npos) {
			return true;
		}, [&](xy pos, xy npos) {
			auto&ff = force_field[force_field_index(npos)];
			if (std::get<0>(ff) >= current_frame - 15 && std::get<1>(ff) <= priority) {
				double angle = atan2(pos.y - npos.y, pos.x - npos.x);
				double da = std::abs(angle - std::get<2>(ff));
				if (da > PI) da = -da + PI * 2;
				if (da >= PI) return 32.0 * 4;
			}
			return 0.0;
		}, [&](xy pos) {
			auto&ff = force_field[force_field_index(pos)];
			if (std::get<0>(ff) < current_frame - 15 || std::get<1>(ff) > priority) {
				unit*taken = std::get<3>(ff);
				if (taken && taken != u) return false;
				return true;
			}
			return false;
		});
		if (escape_path.empty()) return goal;
		auto&final_ff = force_field[force_field_index(escape_path.back())];
		std::get<3>(final_ff) = u;
		if (std::get<0>(final_ff) >= current_frame) {
			final_ff = std::make_tuple(current_frame, priority, angle, u);
		}
		return escape_path.back();
	}
	if (next_node != to_node) {
		for (xy pos : square_path) {
			auto&ff = force_field[force_field_index(pos)];
			ff = std::make_tuple(current_frame, priority, angle, nullptr);
		}
	} else return goal;
	double dis = 0.0;
	xy lpos = u->pos;
	for (xy pos : square_path) {
		dis += diag_distance(pos - lpos);
		if (dis >= 32 * 4) {
			auto&dims = u->type->dimensions;
			if (!can_fit_at(pos, dims)) continue;
			if (!can_fit_at(pos + xy(7, 0), dims)) continue;
			if (!can_fit_at(pos + xy(0, 7), dims)) continue;
			if (!can_fit_at(pos + xy(7, 7), dims)) continue;
			return pos + xy(4, 4);
		}
		lpos = pos;
		//if (frames_to_reach(u, diag_distance(pos - u->pos)) >= 30) return pos;
	}
	return square_path.back();
}

void square_pathing_update_maps_task() {

	a_vector<std::tuple<xy, xy>> queue;
	while (true) {

		queue = invalidation_queue;
		invalidation_queue.clear();

		for (auto&map : all_pathing_maps) {
			bool updated = false;
			if (!map.initialized) {
				update_map(map, xy(0, 0), xy(grid::map_width - 1, grid::map_height - 1));
				map.initialized = true;
				updated = true;
			}
			for (auto&v : queue) {
				xy from, to;
				std::tie(from, to) = v;
				from.x -= map.dimensions[0] + 16;
				from.y -= map.dimensions[1] + 16;
				to.x += map.dimensions[2] + 16;
				to.y += map.dimensions[3] + 16;
				update_map(map, from, to);
				updated = true;
			}
			if (map.update_path_nodes) {
				map.update_path_nodes = false;
				map.path_nodes_requires_update = false;
				generate_path_nodes(map);
			}
		}

		multitasking::sleep(15);
	}

}

void render() {

	auto&test = get_pathing_map(unit_types::scv);

// 	BWAPI::Position screen_pos = game->getScreenPosition();
// 	for (int y = screen_pos.y + 480 / 2 - 480 / 4 / 2; y < screen_pos.y + 480 / 2 + 480 / 4 / 2; y += 8) {
// 		for (int x = screen_pos.x + 640 / 2 - 640 / 4 / 2; x < screen_pos.x + 640 / 2 + 640 / 4 / 2; x += 8) {
// 			if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;
// 
// 			x &= -8;
// 			y &= -8;
// 
// 			size_t index = walk_pos_index(xy(x, y));
// 			if (!test.walkable.test(index)) {
// 				game->drawBoxMap(x, y, x + 8, y + 8, BWAPI::Colors::Purple);
// 			}
// 
// 		}
// 	}

// 	for (int y = screen_pos.y; y < screen_pos.y + 480; y += 32) {
// 		for (int x = screen_pos.x; x < screen_pos.x + 640; x += 32) {
// 			if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;
// 
// 			x &= -32;
// 			y &= -32;
// 
// 			auto&bs = grid::get_build_square(xy(x, y));
// 			if (bs.building) {
// 				game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Colors::Purple);
// 			}
// 			if (bs.reserved.first) {
// 				game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Colors::Red);
// 			}
// 
// 		}
// 	}

	{
		for (auto&v : test.path_nodes) {

			game->drawCircleMap(v.pos.x + 4, v.pos.y + 4, 16, BWAPI::Colors::Yellow);

			for (auto*n : v.neighbors) {
				game->drawLineMap(v.pos.x + 4, v.pos.y + 4, n->pos.x + 4, n->pos.y + 4, BWAPI::Colors::Yellow);
			}

		}
	}

	{
		bwapi_pos mouse_pos = game->getScreenPosition() + game->getMousePosition();
		if ((size_t)mouse_pos.x < (size_t)grid::map_width && (size_t)mouse_pos.y < (size_t)grid::map_height) {
			path_node*nn = test.nearest_path_node[nearest_path_node_index(xy(mouse_pos.x, mouse_pos.y))];
			if (nn) game->drawLineMap(mouse_pos.x, mouse_pos.y, nn->pos.x, nn->pos.y, BWAPI::Colors::Green);
		}
	}

// 	{
// 		xy start_pos;
// 		for (auto*u : my_units) {
// 			if (u->type == unit_types::marine) {
// 				start_pos = u->pos;
// 			}
// 		}
// 		auto path = find_path(test, start_pos, xy(64, 64));
// 		xy lp = start_pos;
// 		for (auto*n : path) {
// 			game->drawLineMap(lp.x, lp.y, n->pos.x, n->pos.y, BWAPI::Colors::Teal);
// 			lp = n->pos;
// 		}
// 	}

		{
			xy lp;
			for (xy pos : render_node_path) {
				if (lp.x || lp.y) game->drawLineMap(lp.x, lp.y, pos.x, pos.y, BWAPI::Colors::Teal);
				lp = pos;
			}
			lp = xy();
			for (auto pos : render_path) {
				if (lp.x || lp.y) game->drawLineMap(lp.x, lp.y, pos.x, pos.y, BWAPI::Colors::Red);
				lp = pos;
			}
		}

	{
		bwapi_pos mouse_pos = game->getScreenPosition() + game->getMousePosition();
		if ((size_t)mouse_pos.x < (size_t)grid::map_width && (size_t)mouse_pos.y < (size_t)grid::map_height) {
			xy to;
			for (unit*u : my_units) {
				if (u->type->is_resource_depot) to = u->pos;
			}
			game->drawLineMap(mouse_pos.x, mouse_pos.y, to.x, to.y, BWAPI::Colors::White);
			game->drawTextMouse(0, -12, "%f", get_distance(test, xy(mouse_pos.x, mouse_pos.y), to));
		}
	}
	
}

void init() {

	walkmap_width = grid::walk_grid_width;
	walkmap_height = grid::walk_grid_height;

// 	nearest_path_node_width = (walkmap_width + 1) / 2;
// 	nearest_path_node_height = (walkmap_height + 1) / 2;
	nearest_path_node_width = walkmap_width;
	nearest_path_node_height = walkmap_height;

	force_field_grid_width = (grid::map_width + force_field_size - 1) / force_field_size;
	force_field_grid_height = (grid::map_height + force_field_size - 1) / force_field_size;
	force_field.resize(force_field_grid_width*force_field_grid_height);

	multitasking::spawn(square_pathing_update_maps_task, "square pathing update maps");

	//multitasking::spawn(test_task, "test");

	render::add(render);

}



}

