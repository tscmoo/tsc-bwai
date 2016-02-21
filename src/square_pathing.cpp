//
// This file implements the square_pathing module.
//

#include "square_pathing.h"
#include "bot.h"

using namespace tsc_bwai;

namespace tsc_bwai {
	namespace square_pathing {
		struct pathing_map {
			bool initialized = false;
			std::array<int, 4> dimensions;
			const unit_type* ut;
			pathing_map_index index;
			bool include_enemy_buildings = true;
			bool include_liftable_wall = false;

			dynamic_bitset walkable;

			a_vector<path_node> path_nodes;
			a_vector<const path_node*> nearest_path_node;
			bool path_nodes_requires_update = false;
			mutable bool update_path_nodes = false;
			mutable int update_path_nodes_frame = 0;
			int last_update_path_nodes_frame = 0;

			a_multimap<std::pair<size_t, size_t>, unit*> nydus_canals;

			struct cached_distance_hash {
				size_t operator()(const std::tuple<const path_node*, const path_node*>&v) const {
					return std::hash<const path_node*>()(std::get<0>(v)) ^ std::hash<const path_node*>()(std::get<1>(v));
				}
			};

			mutable a_unordered_map<std::tuple<const path_node*, const path_node*>, std::tuple<const path_node*, const path_node*, double>, cached_distance_hash> cached_distance;

			size_t path_node_index(const path_node& n) const {
				return &n - path_nodes.data();
			}
		};
	}
}

bool square_pathing_module::can_fit_at(xy pos, const std::array<int, 4>& dims, bool include_enemy_buildings, bool include_liftable_wall, unit** wall_building) {
	int right = pos.x + dims[0];
	int bottom = pos.y + dims[1];
	int left = pos.x - dims[2];
	int top = pos.y - dims[3];
	if (left < 0) return false;
	if (top < 0) return false;
	if ((unsigned)right >= (unsigned)bot.grid.map_width) return false;
	if ((unsigned)bottom >= (unsigned)bot.grid.map_height) return false;
	walk_square* sq = &bot.grid.get_walk_square(xy(left, top));
	unit* prev_building = nullptr;
	for (int y = top&-8; y <= bottom; y += 8) {
		walk_square* nsq = sq;
		for (int x = left&-8; x <= right; x += 8) {
			if (!nsq->walkable) return false;
			for (unit* b : nsq->buildings) {
				if (b == prev_building) continue;
				prev_building = b;
				// TODO: something about neutral buildings
				if (!include_enemy_buildings && b->owner == bot.players.opponent_player) continue;
				if (!include_liftable_wall && b->building->is_liftable_wall) {
					if (wall_building && !b->building->is_lifted) *wall_building = b;
					continue;
				}
				xy bpos = b->pos;
				int bright = bpos.x + b->type->dimension_right();
				int bbottom = bpos.y + b->type->dimension_down();
				int bleft = bpos.x - b->type->dimension_left();
				int btop = bpos.y - b->type->dimension_up();
				if (right >= bleft && bottom >= btop && bright >= left && bbottom >= top) return false;
			}
			nsq = &bot.grid.walk_square_neighbor(*nsq, 0);
		}
		sq = &bot.grid.walk_square_neighbor(*sq, 1);
	}
	return true;
};

void square_pathing_module::invalidate_area(xy from, xy to) {
	invalidation_queue.emplace_back(from, to);
}

pathing_map& square_pathing_module::get_pathing_map(const unit_type* ut, pathing_map_index index) {
	auto*& r = pathing_map_for_unit_type[(size_t)index][ut];
	if (r) return *r;
	all_pathing_maps.emplace_back();
	r = &all_pathing_maps.back();
	r->dimensions = ut->dimensions;
	if (ut->is_flyer) xcept("get_pathing_map for a flyer");
	r->walkable.resize(walkmap_width * walkmap_height);
	r->nearest_path_node.resize(nearest_path_node_width*nearest_path_node_height);
	r->ut = ut;
	r->index = index;
	r->include_enemy_buildings = index != pathing_map_index::no_enemy_buildings;
	r->include_liftable_wall = index == pathing_map_index::include_liftable_wall;
	return *r;
}

xy square_pathing_module::get_pos_in_square(xy pos, const unit_type* ut) {
	auto& dims = ut->dimensions;
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

size_t square_pathing_module::nearest_path_node_index(xy pos) {
	return (size_t)pos.x / 8 + (size_t)pos.y / 8 * nearest_path_node_width;
}

const path_node* square_pathing_module::get_nearest_path_node(const pathing_map& map, xy pos) {
	if (map.path_nodes_requires_update && !map.update_path_nodes) {
		map.update_path_nodes = true;
		map.update_path_nodes_frame = bot.current_frame;
	}
	if (pos.x < 0) pos.x = 0;
	if (pos.x >= bot.grid.map_width) pos.x = bot.grid.map_width - 1;
	if (pos.y < 0) pos.y = 0;
	if (pos.y >= bot.grid.map_height) pos.y = bot.grid.map_height - 1;
	return map.nearest_path_node[nearest_path_node_index(pos)];
}

std::pair<const path_node*, const path_node*> square_pathing_module::get_nearest_path_nodes(const pathing_map& map, xy a, xy b) {
	const path_node* na = get_nearest_path_node(map, a);
	const path_node* nb = get_nearest_path_node(map, b);
	if (!na || !nb) return std::make_pair(na, nb);
	if (na->root_index != nb->root_index) {
		auto getpos = [&](int index) {
			if (index == 0) return xy(-32, 0);
			if (index == 1) return xy(0, -32);
			if (index == 2) return xy(32, 0);
			if (index == 3) return xy(0, 32);
			return xy();
		};
		for (int ia = 0; ia < 4; ++ia) {
			xy pa = a + getpos(ia);
			const path_node* npa = get_nearest_path_node(map, pa);
			for (int ib = 0; ib < 4; ++ib) {
				xy pb = b + getpos(ib);
				const path_node* npb = get_nearest_path_node(map, pb);
				if (npa && npb && npa->root_index == npb->root_index) return std::make_pair(npa, npb);
			}
		}
	}
	return std::make_pair(na, nb);
}

xy square_pathing_module::get_nearest_node_pos(const unit_type* ut, xy pos) {
	auto* n = bot.square_pathing.get_nearest_path_node(bot.square_pathing.get_pathing_map(ut), pos);
	if (!n) return xy();
	return n->pos;
}
xy square_pathing_module::get_nearest_node_pos(const unit* u) {
	return get_nearest_node_pos(u->type, u->pos);
}

bool square_pathing_module::unit_can_reach(const unit_type* ut, xy from, xy to, pathing_map_index index) {
	if (ut->is_flyer) return true;
	auto& map = bot.square_pathing.get_pathing_map(ut, index);
	const path_node* a;
	const path_node* b;
	std::tie(a, b) = get_nearest_path_nodes(map, from, to);
	if (!a) return !b;
	if (!b) return !a;
	return a->root_index == b->root_index;
}
bool square_pathing_module::unit_can_reach(const unit* u, xy from, xy to, pathing_map_index index) {
	return unit_can_reach(u->type, from, to, index);
}

double square_pathing_module::get_distance(const pathing_map& map, xy from_pos, xy to_pos) {
	const path_node* from;
	const path_node* to;
	std::tie(from, to) = get_nearest_path_nodes(map, from_pos, to_pos);
	double r = std::numeric_limits<double>::infinity();
	if (!from || !to) return r;
	if (from->root_index != to->root_index) return r;
	if (from == to) return diag_distance(to_pos - from_pos);

	auto it = map.cached_distance.find(std::make_tuple(from, to));
	if (it != map.cached_distance.end()) {
		double d;
		std::tie(from, to, d) = it->second;
		if (from) d += diag_distance(from->pos - from_pos);
		if (to) d += diag_distance(to->pos - to_pos);
		double r = d;
		//log("distance (cached) from %d %d to %d %d returning %g\n", from_pos.x, from_pos.y, to_pos.x, to_pos.y, r);
		return r;
	}

	bool is_first = true;
	const path_node* first_node = nullptr;
	const path_node* next_to_last_node = nullptr;
	double first_distance = 0;
	double next_to_last_distance = 0;
	path_node* prev_node = nullptr;
	find_path(map, from, to, from_pos, to_pos, [&](open_t&cur) {
		if (cur.prev) {
			next_to_last_node = cur.prev->node;
			next_to_last_distance = cur.prev->distance;
		}
		for (closed_t*n = cur.prev; n && n->prev; n = n->prev) {
			if (n->node != from) {
				first_node = n->node;
				first_distance = n->distance;
			}
		}
		r = cur.distance;
	});

	if (map.cached_distance.size() >= 0x100) map.cached_distance.clear();
	map.cached_distance[std::make_tuple(from, to)] = std::make_tuple(first_node, next_to_last_node, next_to_last_distance - first_distance);

	//log("saving cached distance %g - first_node %p, next_to_last_node %p, distance %g\n", r, first_node, next_to_last_node, next_to_last_distance - first_distance);

	return r;
}


a_deque<const path_node*> square_pathing_module::find_path(const pathing_map& map, const path_node* from, const path_node* to) {
	a_deque<const path_node*> r;
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

a_deque<const path_node*> square_pathing_module::find_path(const pathing_map& map, xy from, xy to) {
	const path_node* a;
	const path_node* b;
	std::tie(a, b) = get_nearest_path_nodes(map, from, to);
	return find_path(map, a, b);
}

// Brood war is too picky about what can be the final destination for a path
xy square_pathing_module::get_go_to_along_path(const unit* u, const a_deque<xy>& path, unit** wall_building, xy preferred_go_to) {
	if (path.empty()) return u->pos;
	double dis = 0.0;
	xy lpos = path.front();
	for (xy pos : path) {
		can_fit_at(pos, u->type->dimensions, true, false, wall_building);
		dis += diag_distance(pos - lpos);
		bool close_to_preferred = dis >= 32 * 4 && diag_distance(pos - preferred_go_to) <= 32 * 2;
		if (close_to_preferred) pos = preferred_go_to;
		if (close_to_preferred || dis >= 32 * 10) {
			auto&dims = u->type->dimensions;
			auto test = [&](xy npos) {
				if (!can_fit_at(npos, dims, true, false, wall_building)) return false;
				return bot.grid.get_build_square(npos).entirely_walkable;
			};
			if (test(pos)) return pos;
			xy apos = pos / 32 * 32;
			apos += xy(16, 16);
			if (test(apos)) return apos;
			if (test(apos + xy(32, 0))) return apos + xy(32, 0);
			if (test(apos + xy(0, 32))) return apos + xy(32, 32);
			if (test(apos + xy(-32, 0))) return apos + xy(-32, 0);
			if (test(apos + xy(0, -32))) return apos + xy(0, -32);
		}
		lpos = pos;
		//if (frames_to_reach(u, diag_distance(pos - u->pos)) >= 30) return pos;
	}
	return path.back();
}

xy square_pathing_module::get_go_to_along_path_and_lift_wall(const unit *u, const a_deque<xy>& path, xy preferred_go_to) {
	unit*wall_building = nullptr;
	xy r = get_go_to_along_path(u, path, &wall_building, preferred_go_to);
	//if (wall_building) bot.wall_in.lift_please(wall_building);
	if (wall_building) xcept("fixme: bot.wall_in.lift_please(wall_building)");
	return r;
}

xy square_pathing_module::get_move_to(const unit* u, xy goal, int priority, xy last_move_to_pos) {

	auto& map = get_pathing_map(u->type);
	const path_node* from_node;
	const path_node* to_node;
	std::tie(from_node, to_node) = get_nearest_path_nodes(map, u->pos, goal);
	auto path = find_path(map, from_node, to_node);
	if (path.empty()) return goal;
	const path_node* middle_node = path.front();
	if (path.size() > 1) path.pop_front();
	const path_node* next_node = path.front();

	render_node_path.clear();
	for (auto&v : path) render_node_path.emplace_back(v->pos);

	double angle = std::atan2(u->pos.y - next_node->pos.y, u->pos.x - next_node->pos.x);

	auto square_path = find_square_path(map, u->pos, [&](xy pos, xy npos) {
		const path_node* nn = map.nearest_path_node[nearest_path_node_index(npos)];
		if (nn != from_node && nn != middle_node && nn != next_node) return false;
		auto& ff = force_field[force_field_index(npos)];
		if (std::get<0>(ff) >= bot.current_frame - 15 && std::get<1>(ff) <= priority) {
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
		if (std::get<0>(ff) >= bot.current_frame - 15 && std::get<1>(ff) <= priority) {
			square_path.clear();
		}
	}

	render_path = square_path;

	if (square_path.empty()) {
		auto escape_path = find_square_path(map, u->pos, [&](xy pos, xy npos) {
			return true;
		}, [&](xy pos, xy npos) {
			auto& ff = force_field[force_field_index(npos)];
			if (std::get<0>(ff) >= bot.current_frame - 15 && std::get<1>(ff) <= priority) {
				double angle = atan2(pos.y - npos.y, pos.x - npos.x);
				double da = std::abs(angle - std::get<2>(ff));
				if (da > PI) da = -da + PI * 2;
				if (da >= PI) return 32.0 * 4;
			}
			return 0.0;
		}, [&](xy pos) {
			auto& ff = force_field[force_field_index(pos)];
			if (std::get<0>(ff) < bot.current_frame - 15 || std::get<1>(ff) > priority) {
				const unit* taken = std::get<3>(ff);
				if (taken && taken != u) return false;
				return true;
			}
			return false;
		});
		if (escape_path.empty()) return goal;
		auto& final_ff = force_field[force_field_index(escape_path.back())];
		std::get<3>(final_ff) = u;
		if (std::get<0>(final_ff) >= bot.current_frame) {
			final_ff = std::make_tuple(bot.current_frame, priority, angle, u);
		}
		return get_go_to_along_path_and_lift_wall(u, escape_path, last_move_to_pos);
	}
	if (next_node != to_node) {
		for (xy pos : square_path) {
			auto& ff = force_field[force_field_index(pos)];
			ff = std::make_tuple(bot.current_frame, priority, angle, nullptr);
		}
	} else return goal;
	return get_go_to_along_path_and_lift_wall(u, square_path, last_move_to_pos);
}

unit* square_pathing_module::get_nydus_canal_from_to(const pathing_map& map, xy from, xy to) {
	const path_node* a;
	const path_node* b;
	std::tie(a, b) = get_nearest_path_nodes(map, from, to);
	if (!a || !b) return nullptr;
	//log("get_nydus_canal from %d to %d\n", a->root_index, b->root_index);
	if (a->root_index == b->root_index) return nullptr;
	unit* r = nullptr;
	double dist = std::numeric_limits<double>::infinity();
	auto key = std::make_pair(a->root_index, b->root_index);
	for (auto i = map.nydus_canals.lower_bound(key); i != map.nydus_canals.end() && i->first == key; ++i) {
		unit*u = i->second;
		double d = diag_distance(u->pos - from);
		if (d < dist) {
			dist = d;
			r = u;
		}
	}
	//log("returning %p\n", r);
	return r;
}

bool square_pathing_module::pathing_map_walkable(const pathing_map& map, size_t index) {
	return map.walkable.test(index);
}

bool square_pathing_module::has_any_path_nodes(const pathing_map& map) {
	if (map.path_nodes_requires_update && !map.update_path_nodes) {
		map.update_path_nodes = true;
		map.update_path_nodes_frame = bot.current_frame;
	}
	return !map.path_nodes.empty();
}

size_t square_pathing_module::walk_pos_index(xy pos) {
	return (size_t)pos.x / 8 + (size_t)pos.y / 8 * walkmap_width;
}

size_t square_pathing_module::force_field_index(xy pos) {
	return (size_t)pos.x / force_field_size + (size_t)pos.y / force_field_size * force_field_grid_width;
};



void square_pathing_module::update_map(pathing_map&map, xy from, xy to) {

	tsc::high_resolution_timer ht;
	double elapsed = 0.0;

	map.path_nodes_requires_update = true;

	auto dims = map.dimensions;

	if (from.x < 0) from.x = 0;
	if (from.y < 0) from.y = 0;
	if (to.x < 0) to.x = 0;
	if (to.y < 0) to.y = 0;

	for (int y = from.y&-8; y <= to.y; y += 8) {
		if (y >= bot.grid.map_height) break;
		for (int x = from.x&-8; x <= to.x; x += 8) {
			if (x >= bot.grid.map_width) break;

			xy pos(x, y);

			if (!bot.grid.get_walk_square(pos).walkable) continue;

			bool kay = can_fit_at(pos, dims, map.include_enemy_buildings, map.include_liftable_wall);
			if (!kay && can_fit_at(pos + xy(4, 4), { 0, 0, 0, 0 }, map.include_enemy_buildings, map.include_liftable_wall)) {
				kay = can_fit_at(pos + xy(7, 0), dims, map.include_enemy_buildings, map.include_liftable_wall);
				if (!kay) kay |= can_fit_at(pos + xy(0, 7), dims, map.include_enemy_buildings, map.include_liftable_wall) || can_fit_at(pos + xy(7, 7), dims, map.include_enemy_buildings, map.include_liftable_wall);
				if (!kay) {
					for (int x = 1; x < 7 && !kay; ++x) {
						if (can_fit_at(pos + xy(x, 0), dims, map.include_enemy_buildings, map.include_liftable_wall)) kay = true;
						else if (can_fit_at(pos + xy(x, 7), dims, map.include_enemy_buildings, map.include_liftable_wall)) kay = true;
					}
					for (int y = 1; y < 7 && !kay; ++y) {
						if (can_fit_at(pos + xy(0, y), dims, map.include_enemy_buildings, map.include_liftable_wall)) kay = true;
						else if (can_fit_at(pos + xy(7, y), dims, map.include_enemy_buildings, map.include_liftable_wall)) kay = true;
					}
				}
			}
			size_t index = walk_pos_index(xy(x, y));
			if (kay) map.walkable.set(index);
			else map.walkable.reset(index);

		}
		elapsed += ht.elapsed();
		bot.multitasking.yield_point();
		ht.reset();
	}

	elapsed += ht.elapsed_and_reset();

	bot.log("updated map in %f\n", elapsed);

}

void square_pathing_module::generate_path_nodes(pathing_map&map) {

	tsc::high_resolution_timer ht;
	double elapsed = 0.0;

	map.cached_distance.clear();

	auto dims = map.dimensions;

	bot.log("generate path nodes for %s %d %d\n", map.ut->name, map.include_enemy_buildings, map.include_liftable_wall);
	
	a_vector<path_node> new_path_nodes;

	dynamic_bitset visited(walkmap_width*walkmap_height);
	a_deque<std::tuple<xy, path_node*>> open;
	std::priority_queue<std::tuple<double, xy, path_node*>> open2;

	for (int y = 0; y < bot.grid.map_height; y += 8) {
		for (int x = 0; x < bot.grid.map_width; x += 8) {
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
				path_node* curnode;
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
					if ((size_t)npos.x >= (size_t)bot.grid.map_width || (size_t)npos.y >= (size_t)bot.grid.map_height) return;

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
					bot.multitasking.yield_point();
					ht.reset();
				}
			}
		}
	}

	bot.log("initial %d new nodes generated in %f\n", new_path_nodes.size(), elapsed);

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

	auto add_neighbor = [&](path_node* node, path_node* neighbor) {
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
			path_node* curnode;
			std::tie(pos, curnode) = from_open ? open.front() : open2.front();
			from_open ? open.pop_front() : open2.pop_front();

			auto*& v_ptr = new_nearest_path_node[nearest_path_node_index(pos)];
			v_ptr = curnode;

			auto add = [&](int n) {
				xy npos = pos;
				if (n == 0) npos.x += 8;
				if (n == 1) npos.y += 8;
				if (n == 2) npos.x -= 8;
				if (n == 3) npos.y -= 8;
				if ((size_t)npos.x >= (size_t)bot.grid.map_width || (size_t)npos.y >= (size_t)bot.grid.map_height) return;

				size_t index = walk_pos_index(npos);
				bool walkable = map.walkable.test(index);

				if (from_open && walkable) {
					auto* nn = new_nearest_path_node[nearest_path_node_index(npos)];
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
				bot.multitasking.yield_point();
				ht.reset();
			}
		}
	}

	map.path_nodes = std::move(new_path_nodes);
	map.nearest_path_node.resize(new_nearest_path_node.size());
	for (size_t i = 0; i < map.nearest_path_node.size(); ++i) {
		map.nearest_path_node[i] = new_nearest_path_node[i];
	}

	elapsed += ht.elapsed_and_reset();

	bot.log("%d nodes generated in %f\n", map.path_nodes.size(), elapsed);

}


void square_pathing_module::update_maps_task() {


	a_vector<std::tuple<xy, xy>> queue;
	while (true) {

		queue = invalidation_queue;
		invalidation_queue.clear();

		for (auto&map : all_pathing_maps) {
			if (!map.initialized) {
				update_map(map, xy(0, 0), xy(bot.grid.map_width - 1, bot.grid.map_height - 1));
				map.initialized = true;
				map.update_path_nodes = true;
			}
			for (auto&v : queue) {
				xy from, to;
				std::tie(from, to) = v;
				from.x -= map.dimensions[0] + 16;
				from.y -= map.dimensions[1] + 16;
				to.x += map.dimensions[2] + 16;
				to.y += map.dimensions[3] + 16;
				update_map(map, from, to);
			}
			if (map.update_path_nodes && map.ut != unit_types::siege_tank_tank_mode) {
				auto&siege_tank_map = get_pathing_map(unit_types::siege_tank_tank_mode, map.index);
				if (siege_tank_map.path_nodes_requires_update && !siege_tank_map.update_path_nodes) {
					siege_tank_map.update_path_nodes = true;
					siege_tank_map.update_path_nodes_frame = bot.current_frame;
				}
			}
			if (map.update_path_nodes && ((bot.current_frame - map.update_path_nodes_frame >= 15 * 5 && bot.current_frame - map.last_update_path_nodes_frame >= 15 * 10) || bot.current_frame < 15 * 60)) {
				map.last_update_path_nodes_frame = bot.current_frame;
				map.update_path_nodes = false;
				map.path_nodes_requires_update = false;

				auto copy_nodes = [&](pathing_map&dst, pathing_map&src) {
					dst.path_nodes = src.path_nodes;
					dst.nearest_path_node = src.nearest_path_node;
					for (auto&v : dst.path_nodes) {
						for (auto*&n : v.neighbors) {
							if (n) n = dst.path_nodes.data() + src.path_node_index(*n);
						}
					}
					for (auto*&n : dst.nearest_path_node) {
						if (n) n = dst.path_nodes.data() + src.path_node_index(*n);
					}
				};

				if (map.ut == unit_types::siege_tank_tank_mode) {
					generate_path_nodes(map);
					for (auto&v : all_pathing_maps) {
						if (&v == &map) continue;
						if (v.index != map.index) continue;
						copy_nodes(v, map);
					}
				} else {
					map.cached_distance.clear();
					auto&siege_tank_map = get_pathing_map(unit_types::siege_tank_tank_mode, map.index);
					if (siege_tank_map.path_nodes.empty() && !siege_tank_map.update_path_nodes) {
						siege_tank_map.update_path_nodes = true;
						siege_tank_map.update_path_nodes_frame = bot.current_frame;
					}
					copy_nodes(map, siege_tank_map);
				}
			}
		}

		bot.multitasking.sleep(15);
	}
	
}

void square_pathing_module::update_nydus_canals(pathing_map& map) {
	map.nydus_canals.clear();
	for (unit* u : bot.units.my_completed_units_of_type[unit_types::nydus_canal]) {
		unit* e = bot.units.get_unit(u->game_unit->getNydusExit());
		if (!e) continue;
		const path_node* a;
		const path_node* b;
		std::tie(a, b) = get_nearest_path_nodes(map, u->pos, e->pos);
		if (a && b && a->root_index != b->root_index) {
			bot.log("added nydus canal from %d to %d\n", a->root_index, b->root_index);
			map.nydus_canals.emplace(std::make_pair(a->root_index, b->root_index), u);
		}
	}
}


void square_pathing_module::update_nydus_canals_task() {
	while (true) {

		for (auto&map : all_pathing_maps) {
			update_nydus_canals(map);
		}

		bot.multitasking.sleep(15 * 10);
	}
}


square_pathing_module::square_pathing_module(bot_t& bot) : bot(bot) {
}

square_pathing_module::~square_pathing_module() {
}


void square_pathing_module::init() {
	walkmap_width = bot.grid.walk_grid_width;
	walkmap_height = bot.grid.walk_grid_height;

	nearest_path_node_width = walkmap_width;
	nearest_path_node_height = walkmap_height;

	force_field_grid_width = (bot.grid.map_width + force_field_size - 1) / force_field_size;
	force_field_grid_height = (bot.grid.map_height + force_field_size - 1) / force_field_size;
	force_field.resize(force_field_grid_width*force_field_grid_height);

	bot.multitasking.spawn(std::bind(&square_pathing_module::update_maps_task, this), "square pathing update maps");
	bot.multitasking.spawn(std::bind(&square_pathing_module::update_nydus_canals_task, this), "square pathing update nydus canals");

	//render::add(render);

}
