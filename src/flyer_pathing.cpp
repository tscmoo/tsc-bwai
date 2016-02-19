
#include "flyer_pathing.h"
#include "common.h"
#include "bot.h"
using namespace tsc_bwai;

a_deque<xy> tsc_bwai::flyer_pathing_module::find_bigstep_path(xy from, xy to) {
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
	dynamic_bitset visited(bot.grid.build_grid_width * bot.grid.build_grid_height);

	open.push({ nullptr, from, 0, 0 });
	visited.set(bot.grid.build_square_index(from));

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
			if ((size_t)npos.x >= (size_t)bot.grid.map_width || (size_t)npos.y >= (size_t)bot.grid.map_height) return;
			size_t index = bot.grid.build_square_index(npos);
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
