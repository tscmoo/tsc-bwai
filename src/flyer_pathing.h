
namespace flyer_pathing {
;

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

template<typename node_data_t = no_value_t, typename pred_T, typename est_dist_T, typename goal_T>
a_deque<xy> find_path(xy from, pred_T&&pred, est_dist_T&&est_dist, goal_T&&goal) {
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
	tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);

	open.push({ nullptr, from, 0, 0 });
	visited.set(grid::build_square_index(from));

	int iterations = 0;
	while (!open.empty()) {
		++iterations;

		open_t cur = open.top();
		//if (cur.pos.x <= to.x && cur.pos.y <= to.y && cur.pos.x + 8 > to.x && cur.pos.y + 8 > to.y) {
		if (call_goal(goal,cur.pos,cur.nd)) {
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
			if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) return;
			size_t index = grid::build_square_index(npos);
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

a_deque<xy> find_bigstep_path(xy from, xy to) {
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
		bool operator<(const open_t&n) const {
			if (est_distance != n.est_distance) return est_distance > n.est_distance;
			return distance > n.distance;
		}
	};
	a_list<closed_t> closed;
	std::priority_queue<open_t, a_vector<open_t>> open;
	tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);

	open.push({ nullptr, from, 0, 0 });
	visited.set(grid::build_square_index(from));

	int step = 32 * 8;

	int iterations = 0;
	while (!open.empty()) {
		++iterations;

		open_t cur = open.top();
		if (cur.pos.x <= to.x && cur.pos.y <= to.y && cur.pos.x + step > to.x && cur.pos.y + step > to.y) {
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
			if (n & 1) npos.x += step;
			if (n & 2) npos.y += step;
			if (n & 4) npos.x -= step;
			if (n & 8) npos.y -= step;
			if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) return;
			size_t index = grid::build_square_index(npos);
			if (visited.test(index)) return;
			visited.set(index);

			double distance = cur.distance + diag_distance(cur.pos - npos);
			double est_distance = distance + diag_distance(to - cur.pos);
			open.push({ &closed_node, npos, distance, est_distance });
		};
		add(1); add(1 | 2);
		add(2); add(2 | 4);
		add(4); add(4 | 8);
		add(8); add(8 | 1);
	}
	return r;
}

}