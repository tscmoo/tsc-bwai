//
// This file defines flyer_pathing_module, which is "pathfinding" for flyers.
//

#ifndef TSC_BWAI_FLYER_PATHING_H
#define TSC_BWAI_FLYER_PATHING_H

#include "common.h"
#include "dynamic_bitset.h"
#include <queue>

namespace tsc_bwai {

	class flyer_pathing_module {
		bot_t& bot;

		template<typename node_data_t, typename goal_T>
		bool call_goal(const goal_T& goal, xy pos, const node_data_t& n) {
			return goal(pos, n);
		}
		template<typename goal_T>
		bool call_goal(const goal_T& goal, xy pos, const no_value_t& n) {
			return goal(pos);
		}

		template<typename node_data_t, typename goal_T>
		bool call_pred(const goal_T& goal, xy ppos, xy npos, node_data_t& n) {
			return goal(ppos, npos, n);
		}
		template<typename goal_T>
		bool call_pred(const goal_T& goal, xy ppos, xy npos, no_value_t& n) {
			return goal(ppos, npos);
		}

		template<typename node_data_t, typename est_dist_T>
		double call_est_dist(const est_dist_T& est_dist, xy ppos, xy npos, const node_data_t& n) {
			return est_dist(ppos, npos, n);
		}
		template<typename est_dist_T>
		double call_est_dist(const est_dist_T& est_dist, xy ppos, xy npos, const no_value_t& n) {
			return est_dist(ppos, npos);
		}

	public:
		flyer_pathing_module(bot_t& bot) : bot(bot) {}

		template<typename node_data_t = no_value_t, typename pred_T, typename est_dist_T, typename goal_T>
		a_deque<xy> find_path(xy from, pred_T&& pred, est_dist_T&& est_dist, goal_T&& goal) {
			a_deque<xy> r;
			struct closed_t {
				closed_t*prev;
				xy pos;
				node_data_t nd;
			};
			struct open_t {
				closed_t*prev;
				xy pos;
				double distance;
				double est_distance;
				node_data_t nd;
				bool operator<(const open_t&n) const {
					if (est_distance != n.est_distance) return est_distance > n.est_distance;
					return distance > n.distance;
				}
			};
			a_list<closed_t> closed;
			std::priority_queue<open_t, a_vector<open_t>> open;
			dynamic_bitset visited(bot.grid.build_grid_width*bot.grid.build_grid_height);

			open.push({ nullptr, from, 0, 0 });
			visited.set(bot.grid.build_square_index(from));

			int iterations = 0;
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
				open.pop();

				closed.push_back({ cur.prev, cur.pos, cur.nd });
				closed_t&closed_node = closed.back();

				auto add = [&](int n) {
					xy npos = cur.pos;
					if (n & 1) npos.x += 32;
					if (n & 2) npos.y += 32;
					if (n & 4) npos.x -= 32;
					if (n & 8) npos.y -= 32;
					if ((size_t)npos.x >= (size_t)bot.grid.map_width || (size_t)npos.y >= (size_t)bot.grid.map_height) return;
					size_t index = bot.grid.build_square_index(npos);
					if (visited.test(index)) return;
					visited.set(index);
					node_data_t nd = closed_node.nd;
					if (!call_pred(pred, cur.pos, npos, nd)) return;

					double distance = cur.distance + diag_distance(cur.pos - npos);
					double est_distance = distance + call_est_dist(est_dist, cur.pos, npos, nd);
					open.push({ &closed_node, npos, distance, est_distance, nd });
				};
				add(1); add(1 | 2);
				add(2); add(2 | 4);
				add(4); add(4 | 8);
				add(8); add(8 | 1);
			}
			return r;
		}

		a_deque<xy> find_bigstep_path(xy from, xy to);
	};

}

#endif
