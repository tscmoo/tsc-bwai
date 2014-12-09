
namespace build_spot_finder {
;

xy get_pos(xy pos) {
	return pos;
}
xy get_pos(const grid::build_square&bs) {
	return bs.pos;
}
xy get_pos(unit*u) {
	return u->building ? u->building->build_pos : u->pos;
}

template<typename at_T>
bool has_path_around(unit_type*ut, const at_T&at, bool use_large_diff = false) {
	xy start_pos = get_pos(at);

	a_deque<std::tuple<xy,xy,int,int>> open;
	tsc::dynamic_bitset visited(grid::build_grid_width * grid::build_grid_height);

	open.emplace_back(start_pos,start_pos,ut->tile_width,ut->tile_height);
	//visited.set((unsigned)start_pos.x/32 + (unsigned)start_pos.y/32 * grid::build_grid_width);

	int allowed_diff_x = 32*10;
	int allowed_diff_y = 32*10;
	if (ut==unit_types::creep_colony) {
		allowed_diff_x = 32*12;
		allowed_diff_y = 32*12;
	}
	if (use_large_diff) {
		allowed_diff_x *= 2;
		allowed_diff_y *= 2;
	}

	bool okay = true;
	int count = 0;
	xy min = start_pos, max = start_pos;
	grid::reserve_build_squares(start_pos,ut);
	while (!open.empty() && okay) {

		xy prev_pos, pos;
		int w, h;
		std::tie(prev_pos,pos,w,h) = open.front();
		open.pop_front();
		size_t v_idx = (unsigned)pos.x/32 + (unsigned)pos.y/32 * grid::build_grid_width;
		if (visited.test(v_idx)) continue;
		visited.set(v_idx);
		if (pos.x<min.x) min.x = pos.x;
		else if (pos.x+w*32>max.x) max.x = pos.x+w*32;
		if (pos.y<min.y) min.y = pos.y;
		else if (pos.y+h*32>max.y) max.y = pos.y+h*32;
		count += w*h;

		int empty_neighbors = 0;
		int visited_count =0;
		auto visit = [&](xy vpos) {
			grid::build_square*n = (vpos.x<0 || vpos.y<0 || vpos.x>=grid::map_width || vpos.y>=grid::map_height) ? nullptr : &grid::get_build_square(vpos);
			if (!n || !n->buildable) {
				okay = false;
				return;
			}
			if (!n->reserved.first && !n->building) {
				++empty_neighbors;
				return;
			}
			vpos = n->building ? n->building->building->build_pos : n->reserved.second;
			size_t v_idx = (unsigned)vpos.x/32 + (unsigned)vpos.y/32 * grid::build_grid_width;
			if (visited.test(v_idx)) {
				bool needs_space = n->building ? n->building->type->needs_neighboring_free_space : n->reserved.first->needs_neighboring_free_space;
				if (vpos!=prev_pos && needs_space) ++visited_count;
				return;
			}
			if (n->building) open.emplace_back(pos,vpos,n->building->type->tile_width,n->building->type->tile_height);
			else open.emplace_back(pos,vpos,n->reserved.first->tile_width,n->reserved.first->tile_height);
			//visited.set(v_idx);
		};

		for (int x=-1;x<w+1;++x) {
			visit(pos + xy(x*32,-32));
			visit(pos + xy(x*32,h*32));
		}
		for (int y=0;y<h;++y) {
			visit(pos + xy(-32,y*32));
			visit(pos + xy(w*32,y*32));
		}
		if (max.x-min.x>=allowed_diff_x) okay = false;
		if (max.y-min.y>=allowed_diff_y) okay = false;
		if ((empty_neighbors==0 && ut->needs_neighboring_free_space) || visited_count) {
			okay = false;
		}

	}
	grid::unreserve_build_squares(start_pos,ut);
	return okay;
}

template<typename at_T>
bool is_buildable_at(unit_type*ut,const at_T&at) {
	xy pos = get_pos(at);
// 	if (ut->is_refinery) {
// 		auto&bs = grid::get_build_square(pos);
// 		return (bs.building && bs.building->type==unit_types::vespene_geyser && bs.building->building->pos==pos);
// 	}
	for (int y=0;y<ut->tile_height;++y) {
		for (int x=0;x<ut->tile_width;++x) {
			xy p = pos + xy(x*32,y*32);
			if (p.x<0 || p.y<0 || p.x>=grid::map_width || p.y>=grid::map_height) return false;
			auto&bs = grid::build_grid[(unsigned)p.x/32 + (unsigned)p.y/32 * grid::build_grid_width];
			if (!bs.buildable) return false;
			if (bs.building || bs.reserved.first) return false;
			if (!ut->is_resource_depot && bs.mineral_reserved) return false;
			if (ut->is_resource_depot && bs.no_resource_depot) return false;
		}
	}
	return true;
}
template<typename at_T>
bool can_build_at(unit_type*ut, const at_T&at, bool allow_large_path_around = false) {
	return is_buildable_at(ut, at) && has_path_around(ut, at, allow_large_path_around);
}

template<typename start_cont_T,typename pred_T>
a_vector<xy> find(start_cont_T&&starts,int max_count,pred_T&&pred,bool only_walkable=true) {
	a_vector<xy> rv;

	//if (near.x<0 || near.y<0 || near.x>=grid::map_width || near.y>=grid::map_height) return rv;

	a_deque<grid::build_square*> open;
	tsc::dynamic_bitset visited(grid::build_grid_width * grid::build_grid_height);
	//open.push_back(&grid::get_build_square(near));
	//visited.set((unsigned)near.x/32 + (unsigned)near.y/32 * grid::build_grid_width);
	for (auto&v : starts) {
		xy pos = get_pos(v);
		if (pos.x<0 || pos.y<0 || pos.x>=grid::map_width || pos.y>=grid::map_height) continue;
		open.push_back(&grid::get_build_square(pos));
		visited.set((unsigned)pos.x/32 + (unsigned)pos.y/32 * grid::build_grid_width);
	}
	int iterations = 0;
	while (!open.empty() && iterations<1000) {
	//while (!open.empty()) {
		++iterations;
		grid::build_square&bs = *open.front();
		open.pop_front();
		if (pred(bs)) {
			rv.push_back(bs.pos);
			if (rv.size()==max_count) break;
		}
		auto add = [&](xy pos) {
			if (pos.x<0 || pos.y<0 || pos.x>=grid::map_width || pos.y>=grid::map_height) return;
			size_t v_idx = (unsigned)pos.x/32 + (unsigned)pos.y/32 * grid::build_grid_width;
			if (visited.test(v_idx)) return;
			visited.set(v_idx);
			auto&bs = grid::build_grid[v_idx];
			if (!bs.buildable && only_walkable) {
				for (int y=0;y<4;++y) {
					for (int x=0;x<4;++x) {
						if (!grid::is_walkable(bs.pos + xy(x*8,y*8))) return;
					}
				}
			}
			open.push_back(&bs);
		};
		add(bs.pos + xy(32,0));
		add(bs.pos + xy(0,32));
		add(bs.pos + xy(-32,0));
		add(bs.pos + xy(0,-32));
	}
	//log("%d iterations, rv.size() is %d\n",iterations,rv.size());
	return rv;
}

template<typename starts_T,typename pred_T,typename score_T>
xy find_best(starts_T&&starts,int max_count,pred_T&pred,score_T&score,bool only_walkable=true) {
	auto vec = find(std::forward<starts_T>(starts),max_count,pred,only_walkable);
	return get_best_score(vec,score);
}

template<typename starts_T,typename pred_T>
xy find(starts_T&&starts,unit_type*ut,pred_T&&pred,bool only_walkable=true) {
	return find_best(std::forward<starts_T>(starts),32,[&](grid::build_square&bs) {
		return can_build_at(ut,bs) && pred(bs);
	},[&](xy pos) {
		double r = 0;
		for (unit*w : my_workers) r += unit_pathing_distance(w,pos);
		return r;
	},only_walkable);;
}
template<typename starts_T>
xy find(starts_T&&starts,unit_type*ut,bool only_walkable=true) {
	return find(std::forward<starts_T>(starts),ut,[&](grid::build_square&bs) {
		return true;
	},only_walkable);
}



void init() {


}

}

