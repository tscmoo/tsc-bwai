//
// This file defines the pathfinding module, which is used to estimate ground
// distances and paths between places.
//

#ifndef TSC_BWAI_SQUARE_PATHING_H
#define TSC_BWAI_SQUARE_PATHING_H

#include "common.h"
#include "dynamic_bitset.h"

namespace tsc_bwai {

	class bot_t;

	namespace square_pathing {

		struct path_node {
			xy pos;
			a_vector<const path_node*> neighbors;
			size_t root_index;
		};

		enum class pathing_map_index { default, no_enemy_buildings, include_liftable_wall };

		struct pathing_map;

		struct closed_t {
			closed_t* prev;
			const path_node* node;
			double distance;
		};
		struct open_t {
			closed_t* prev;
			const path_node* node;
			double distance;
			double est_distance;
			bool operator<(const open_t& n) const {
				if (est_distance != n.est_distance) return est_distance > n.est_distance;
				return distance > n.distance;
			}
		};

	};
	using namespace square_pathing;

	class square_pathing_module {
	public:

		// Returns whether a unit with the specified dimensions can fit
		// anywhere in the 8x8 walk square at the specified position.
		// This accounts for terrain and buildings, but not other units.
		//  include_enemy_buildings - by default (true), we account for enemy
		//    buildings too. If this is set to false, we ignore enemy buildings
		//    and can return true even if the position is blocked by an enemy building.
		//  include_liftable_wall - by default (false), we ignore buildings that are
		//    flagged as unit_building::is_liftable_wall, to allow pathing through
		//    our own wall-in. If this is set to true, those buildings are treated
		//    as regular buildings.
		//  wall_building - if include_liftable_wall is false and this is set,
		//    *wall_building will be set to the building that would need to be lifted
		//    in order to fit the unit.
		bool can_fit_at(xy pos, const std::array<int, 4>& dims, bool include_enemy_buildings = true, bool include_liftable_wall = false, unit** wall_building = nullptr);

		void invalidate_area(xy from, xy to);

		pathing_map& get_pathing_map(const unit_type* ut, pathing_map_index index = pathing_map_index::default);
		size_t nearest_path_node_index(xy pos);
		xy get_pos_in_square(xy pos, const unit_type* ut);
		const path_node* get_nearest_path_node(const pathing_map& map, xy pos);
		std::pair<const path_node*, const path_node*> get_nearest_path_nodes(const pathing_map& map, xy a, xy b);
		xy get_nearest_node_pos(const unit_type* ut, xy pos);
		xy get_nearest_node_pos(const unit* u);
		bool unit_can_reach(const unit_type* ut, xy from, xy to, pathing_map_index index = pathing_map_index::default);
		bool unit_can_reach(const unit* u, xy from, xy to, pathing_map_index index = pathing_map_index::default);

		double get_distance(const pathing_map& map, xy from_pos, xy to_pos);

		a_deque<const path_node*> find_path(const pathing_map& map, const path_node* from, const path_node* to);
		a_deque<const path_node*> find_path(const pathing_map& map, xy from, xy to);

		xy get_go_to_along_path(const unit* u, const a_deque<xy>& path, unit** wall_building = nullptr, xy preferred_go_to = xy());

		xy get_go_to_along_path_and_lift_wall(const unit* u, const a_deque<xy>& path, xy preferred_go_to = xy());

		xy get_move_to(const unit* u, xy goal, int priority, xy last_move_to_pos);

		unit* get_nydus_canal_from_to(const pathing_map& map, xy from, xy to);

		bool pathing_map_walkable(const pathing_map& map, size_t index);

		bool has_any_path_nodes(const pathing_map& map);

		void init();

		//class impl_t;
		//std::unique_ptr<impl_t> impl;
		square_pathing_module(bot_t& bot);
		~square_pathing_module();

	private:

		bot_t& bot;

		size_t walkmap_width;
		size_t walkmap_height;

		size_t nearest_path_node_width;
		size_t nearest_path_node_height;

		dynamic_bitset test_walkable;

		a_list<pathing_map> all_pathing_maps;

		std::array<a_unordered_map<const unit_type*, pathing_map*>, 3> pathing_map_for_unit_type;

		a_vector<std::tuple<xy, xy>> invalidation_queue;

		size_t force_field_size = 32;
		size_t force_field_grid_width;
		size_t force_field_grid_height;
		a_vector<std::tuple<int, int, double, const unit*>> force_field;

		a_deque<xy> render_node_path;
		a_deque<xy> render_path;

		size_t walk_pos_index(xy pos);
		size_t force_field_index(xy pos);
			
		void update_nydus_canals(pathing_map& map);

		void update_maps_task();
		void update_nydus_canals_task();

		void update_map(pathing_map& map, xy from, xy to);
		void generate_path_nodes(pathing_map& map);

		template<typename node_data_t, typename goal_T>
		bool call_goal(goal_T&&goal, xy pos, const node_data_t&n) {
			return goal(pos, n);
		}
		template<typename goal_T>
		bool call_goal(goal_T&&goal, xy pos, const no_value_t&n) {
			return goal(pos);
		}

		template<typename node_data_t, typename goal_T>
		bool call_pred(goal_T&&goal, xy ppos, xy npos, node_data_t&n) {
			return goal(ppos, npos, n);
		}
		template<typename goal_T>
		bool call_pred(goal_T&&goal, xy ppos, xy npos, no_value_t&n) {
			return goal(ppos, npos);
		}

		template<typename node_data_t, typename est_dist_T>
		double call_est_dist(est_dist_T&&est_dist, xy ppos, xy npos, const node_data_t&n) {
			return est_dist(ppos, npos, n);
		}
		template<typename est_dist_T>
		double call_est_dist(est_dist_T&&est_dist, xy ppos, xy npos, const no_value_t&n) {
			return est_dist(ppos, npos);
		}

	public:

		template<typename node_data_t = no_value_t, typename pred_T, typename est_dist_T, typename goal_T>
		a_deque<xy> find_square_path(pathing_map& map, xy from, pred_T&& pred, est_dist_T& est_dist, goal_T&& goal, bool return_best_distance = false) {
			//tsc::high_resolution_timer ht;
			a_deque<xy> r;
			struct closed_t {
				closed_t* prev;
				xy pos;
				node_data_t nd;
			};
			struct open_t {
				closed_t* prev;
				xy pos;
				double distance;
				double est_distance;
				double est_distance_remaining;
				int n;
				node_data_t nd;
				bool operator<(const open_t&n) const {
					if (est_distance != n.est_distance) return est_distance > n.est_distance;
					return distance > n.distance;
				}
			};
			a_list<closed_t> closed;
			std::priority_queue<open_t, a_vector<open_t>> open;
			dynamic_bitset visited(walkmap_width*walkmap_height);

			open.push({ nullptr, from, 0, 0, -1 });
			visited.set(walk_pos_index(from));

			int iterations = 0;
			double best_distance_value = std::numeric_limits<double>::infinity();
			closed_t*best_distance_node = nullptr;
			while (!open.empty()) {
				++iterations;

				open_t cur = open.top();
				if (call_goal(goal, cur.pos, cur.nd)) {
					r.push_front(cur.pos);
					for (closed_t*n = cur.prev; n && n->prev; n = n->prev) {
						r.push_front(n->pos);
					}
					break;
				}
				bool set_as_best_distance_node = false;
				if (iterations > 1 && cur.est_distance_remaining < best_distance_value) {
					best_distance_value = cur.est_distance_remaining;
					set_as_best_distance_node = true;
				}
				open.pop();

				closed.push_back({ cur.prev, cur.pos, cur.nd });
				closed_t& closed_node = closed.back();

				if (set_as_best_distance_node) best_distance_node = &closed_node;

				auto add = [&](int n) {
					xy npos = cur.pos;
					if (n & 1) npos.x += 8;
					if (n & 2) npos.y += 8;
					if (n & 4) npos.x -= 8;
					if (n & 8) npos.y -= 8;
					if ((size_t)npos.x >= (size_t)bot.grid.map_width || (size_t)npos.y >= (size_t)bot.grid.map_height) return false;
					size_t index = walk_pos_index(npos);
					if (visited.test(index)) return false;
					visited.set(index);
					if (!pathing_map_walkable(map, index)) return false;
					node_data_t nd = closed_node.nd;
					if (!call_pred(pred, cur.pos, npos, nd)) return false;

					double distance = cur.distance + diag_distance(npos - cur.pos);
					double est_distance_remaining = call_est_dist(est_dist, cur.pos, npos, nd);
					double est_distance = distance + est_distance_remaining;
					open.push({ &closed_node, npos, distance, est_distance, est_distance_remaining, n, nd });
					return true;
				};
				bool r = add(1);
				bool d = add(2);
				bool l = add(4);
				bool u = add(8);
				if (r || d) add(1 | 2);
				if (d || l) add(2 | 4);
				if (l || u) add(4 | 8);
				if (u || r) add(8 | 1);
			}

			if (return_best_distance && r.empty() && best_distance_node) {
				r.push_front(best_distance_node->pos);
				for (closed_t*n = best_distance_node->prev; n && n->prev; n = n->prev) {
					r.push_front(n->pos);
				}
			}

			//log("find square path found path with %d nodes in %d iterations (%fs)\n", r.size(), iterations, ht.elapsed());

			return r;
		}

		template<typename goal_F>
		void find_path(const pathing_map& map, const path_node* from, const path_node* to, xy from_pos, xy to_pos, goal_F&& goal) {

			a_list<closed_t> closed;
			std::priority_queue<open_t, a_vector<open_t>> open;
			dynamic_bitset visited(map.path_nodes.size());

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

				closed.push_back({ cur.prev, cur.node, cur.distance });
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
	};

};

#endif
