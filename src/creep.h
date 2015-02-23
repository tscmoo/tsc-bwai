

namespace creep {
;

enum {creep_source_hatchery, creep_source_creep_colony};

xy creep_spread_creep_colony_arr[] = {
	{0,0},{32,0},{0,32},{-32,0},{0,-32},{64,0},{32,32},{32,-32},
	{0,64},{-32,32},{-64,0},{-32,-32},{0,-64},{96,0},{64,32},{64,-32},
	{32,64},{32,-64},{0,96},{-32,64},{-64,32},{-96,0},{-64,-32},{-32,-64},
	{0,-96},{128,0},{96,32},{96,-32},{64,64},{64,-64},{32,96},{32,-96},
	{0,128},{-32,96},{-64,64},{-96,32},{-128,0},{-96,-32},{-64,-64},{-32,-96},
	{0,-128},{160,0},{128,32},{128,-32},{96,64},{96,-64},{64,96},{64,-96},
	{32,128},{32,-128},{0,160},{-32,128},{-64,96},{-96,64},{-128,32},{-160,0},
	{-128,-32},{-96,-64},{-64,-96},{-32,-128},{0,-160},{192,0},{160,32},{160,-32},
	{128,64},{128,-64},{96,96},{96,-96},{64,128},{64,-128},{32,160},{32,-160},
	{0,192},{-32,160},{-64,128},{-96,96},{-128,64},{-160,32},{-192,0},{-160,-32},
	{-128,-64},{-96,-96},{-64,-128},{-32,-160},{224,0},{192,32},{192,-32},{160,64},
	{160,-64},{128,96},{128,-96},{96,128},{96,-128},{64,160},{64,-160},{32,192},
	{-32,192},{-64,160},{-96,128},{-128,96},{-160,64},{-192,32},{-224,0},{-192,-32},
	{-160,-64},{-128,-96},{-96,-128},{-64,-160},{256,0},{224,32},{224,-32},{192,64},
	{192,-64},{160,96},{160,-96},{128,128},{128,-128},{96,160},{96,-160},{64,192},
	{-64,192},{-96,160},{-128,128},{-160,96},{-192,64},{-224,32},{-256,0},{-224,-32},
	{-192,-64},{-160,-96},{-128,-128},{-96,-160},{288,0},{256,32},{256,-32},{224,64},
	{224,-64},{192,96},{192,-96},{160,128},{160,-128},{128,160},{128,-160},{96,192},
	{-96,192},{-128,160},{-160,128},{-192,96},{-224,64},{-256,32},{-288,0},{-256,-32},
	{-224,-64},{-192,-96},{-160,-128},{-128,-160},{320,0},{288,32},{288,-32},{256,64},
	{256,-64},{224,96},{224,-96},{192,128},{192,-128},{160,160},{160,-160},{128,192},
	{-128,192},{-160,160},{-192,128},{-224,96},{-256,64},{-288,32},{-288,-32},{-256,-64},
	{-224,-96},{-192,-128},{320,32},{320,-32},{288,64},{288,-64},{256,96},{256,-96},
	{224,128},{224,-128},{192,160},{160,192},{-192,160},{-224,128},{-256,96},{-288,64},
	{320,64},{288,96},{256,128},{224,160}
};

xy creep_spread_hatchery_arr[] = {
	{0,0},{32,0},{0,32},{-32,0},{0,-32},{64,0},{32,32},{32,-32},
	{0,64},{-32,32},{-64,0},{-32,-32},{0,-64},{96,0},{64,32},{64,-32},
	{32,64},{32,-64},{0,96},{-32,64},{-64,32},{-96,0},{-64,-32},{-32,-64},
	{0,-96},{128,0},{96,32},{96,-32},{64,64},{64,-64},{32,96},{32,-96},
	{0,128},{-32,96},{-64,64},{-96,32},{-128,0},{-96,-32},{-64,-64},{-32,-96},
	{0,-128},{160,0},{128,32},{128,-32},{96,64},{96,-64},{64,96},{64,-96},
	{32,128},{32,-128},{0,160},{-32,128},{-64,96},{-96,64},{-128,32},{-160,0},
	{-128,-32},{-96,-64},{-64,-96},{-32,-128},{0,-160},{192,0},{160,32},{160,-32},
	{128,64},{128,-64},{96,96},{96,-96},{64,128},{64,-128},{32,160},{32,-160},
	{0,192},{-32,160},{-64,128},{-96,96},{-128,64},{-160,32},{-192,0},{-160,-32},
	{-128,-64},{-96,-96},{-64,-128},{-32,-160},{224,0},{192,32},{192,-32},{160,64},
	{160,-64},{128,96},{128,-96},{96,128},{96,-128},{64,160},{64,-160},{32,192},
	{0,224},{-32,192},{-64,160},{-96,128},{-128,96},{-160,64},{-192,32},{-224,0},
	{-192,-32},{-160,-64},{-128,-96},{-96,-128},{256,0},{224,32},{224,-32},{192,64},
	{192,-64},{160,96},{160,-96},{128,128},{128,-128},{96,160},{96,-160},{64,192},
	{32,224},{-32,224},{-64,192},{-96,160},{-128,128},{-160,96},{-192,64},{-224,32},
	{-256,0},{-224,-32},{-192,-64},{-160,-96},{-128,-128},{288,0},{256,32},{256,-32},
	{224,64},{224,-64},{192,96},{192,-96},{160,128},{160,-128},{128,160},{128,-160},
	{96,192},{64,224},{-96,192},{-128,160},{-160,128},{-192,96},{-224,64},{-256,32},
	{-224,-64},{-192,-96},{320,0},{288,32},{288,-32},{256,64},{256,-64},{224,96},
	{224,-96},{192,128},{192,-128},{160,160},{128,192},{96,224},{-128,192},{-160,160},
	{-192,128},{-224,96},{-256,64},{352,0},{320,32},{320,-32},{288,64},{288,-64},
	{256,96},{256,-96},{224,128},{224,-128},{192,160},{160,192},{128,224},{-192,160},
	{-224,128},{352,32},{320,64},{320,-64},{288,96},{288,-96},{256,128},{224,160},
	{192,192},{352,64},{320,96},{288,128},{256,160},{224,192},{320,128},{288,160}
};

a_vector<xy> creep_spread_hatchery_vec, creep_spread_creep_colony_vec;
static const int creep_spread_bitset_width = 9*2 + 4 + 2;
static const int creep_spread_bitset_height = 5*2 + 3 + 2;
typedef tsc::bitset<creep_spread_bitset_width * creep_spread_bitset_height> creep_spread_bitset_t;
creep_spread_bitset_t creep_spread_hatchery_bitset, creep_spread_creep_colony_bitset;

a_vector<xy>&get_creep_spread_vec(int creep_source) {
	return creep_source==creep_source_hatchery ? creep_spread_hatchery_vec : creep_spread_creep_colony_vec;
}
creep_spread_bitset_t&get_creep_spread_bitset(int creep_source) {
	return creep_source==creep_source_hatchery ? creep_spread_hatchery_bitset : creep_spread_creep_colony_bitset;
}
bool creep_spread_in_range(xy rel,int creep_source) {
	int x = rel.x + creep_spread_bitset_width*32/2;
	int y = rel.y + creep_spread_bitset_height*32/2;
	if (x<0 || y<0) return false;
	x = (unsigned)x/32;
	y = (unsigned)y/32;
	if (x>=creep_spread_bitset_width || y>=creep_spread_bitset_height) return false;
	return (creep_source==creep_source_hatchery ? creep_spread_hatchery_bitset : creep_spread_creep_colony_bitset).test(x + y*creep_spread_bitset_width);
}

tsc::dynamic_bitset existing_creep, complete_creep_spread;

bool can_have_creep_at(xy p) {
	if (p.x<0 || p.y<0 || p.x>=grid::map_width || p.y+32>=grid::map_height) return false;
	grid::build_square&bs = grid::get_build_square(p);
	if (!bs.buildable) return false;
	grid::build_square&bs2 = grid::get_build_square(p + xy(0,32));
	if (!bs2.buildable) return false;
	p.x &= -32;
	p.y &= -32;
	for (int y=0;y<4;++y) {
		for (int x=0;x<4;++x) {
			xy pos = p + xy(x*8,y*8);
			grid::walk_square&ws = grid::get_walk_square(pos);
			//if (ws.building && ws.building->type->race!=race_zerg) return false;
			for (unit*b : ws.buildings) {
				if (b->type->race != race_zerg) return false;
			}
		}
	}
	return true;
}

template<typename func_T>
void for_each_creep_from(xy pos,int creep_source,tsc::dynamic_bitset&existing_creep,func_T&&cb) {

	creep_spread_bitset_t visited;

	a_vector<xy> open;
	open.reserve(0x10);

	xy bottom_right = pos;
	if (creep_source==creep_source_creep_colony) bottom_right += xy(unit_types::creep_colony->tile_width*32,unit_types::creep_colony->tile_height*32);
	else bottom_right += xy(unit_types::hatchery->tile_width*32,unit_types::hatchery->tile_height*32);

	for (xy&rel : get_creep_spread_vec(creep_source)) {

		xy p = pos + rel;
		if (p.x<0 || p.y<0 || p.x>=grid::map_width || p.y>=grid::map_height) continue;

		//if (rel!=xy(0,0) && !existing_creep.test((unsigned)p.x/32 + (unsigned)p.y/32*creep_spread_bitset_width)) continue;
		if (!(p>=pos && p<bottom_right) && !existing_creep.test((unsigned)p.x/32 + (unsigned)p.y/32*grid::build_grid_width)) continue;

		int idx_x = (unsigned)(rel.x + creep_spread_bitset_width*32/2)/32;
		int idx_y = (unsigned)(rel.y + creep_spread_bitset_height*32/2)/32;
		if (visited.test(idx_x + idx_y*creep_spread_bitset_width)) continue;
		visited.set(idx_x + idx_y*creep_spread_bitset_width);
		if (!cb(p)) return;
		open.push_back(rel);
		while (!open.empty()) {

			xy rel = open.back();
			open.pop_back();

			xy p_rel = rel;
			auto add = [&](xy rel2) {
				xy rel = p_rel + rel2;
				if (!creep_spread_in_range(rel,creep_source)) return;
				xy p = pos + rel;
				if (!can_have_creep_at(p)) return;
				int idx_x = (unsigned)(rel.x + creep_spread_bitset_width*32/2)/32;
				int idx_y = (unsigned)(rel.y + creep_spread_bitset_height*32/2)/32;
				if (visited.test(idx_x + idx_y*creep_spread_bitset_width)) return;
				visited.set(idx_x + idx_y*creep_spread_bitset_width);
				if (!cb(p)) return;
				open.push_back(rel);
			};

			add(xy(32,0));
			add(xy(32,32));
			add(xy(0,32));
			add(xy(-32,32));
			add(xy(-32,0));
			add(xy(-32,-32));
			add(xy(0,-32));
			add(xy(32,-32));
		}

	}
}

a_vector<grid::build_square*> get_creep_from(xy pos,int creep_source,tsc::dynamic_bitset&existing_creep) {

	a_vector<grid::build_square*> rv;

	for_each_creep_from(pos,creep_source,existing_creep,[&](xy p) {
		rv.push_back(&grid::get_build_square(p));
		return true;
	});

	return rv;
}

a_vector<grid::build_square*> get_creep_from(xy pos,int creep_source) {
	return get_creep_from(pos,creep_source,existing_creep);
}

bool is_there_creep_at(xy pos) {
	if (pos.x<0 || pos.y<0 || pos.x>=grid::map_width || pos.y>=grid::map_height) return false;
	return existing_creep.test((unsigned)pos.x/32 + (unsigned)pos.y/32 * grid::build_grid_width);
}

struct creep_source {
	xy source_pos;
	creep_spread_bitset_t bs;
	creep_source(xy pos,int source) : source_pos(pos) {
		for_each_creep_from(pos,source,complete_creep_spread,[&](xy p) {
			xy rel = p - pos;
			int idx_x = (unsigned)(rel.x + creep_spread_bitset_width*32/2)/32;
			int idx_y = (unsigned)(rel.y + creep_spread_bitset_height*32/2)/32;
			bs.set(idx_x + idx_y * creep_spread_bitset_width);
			return true;
		});
	}
	bool spreads_to(xy pos) {
		xy rel = pos - source_pos;
		int idx_x = rel.x + creep_spread_bitset_width*32/2;
		int idx_y = rel.y + creep_spread_bitset_height*32/2;
		if (idx_x<0 || idx_y<0) return false;
		idx_x = (unsigned)idx_x/32;
		idx_y = (unsigned)idx_y/32;
		if (idx_x>=creep_spread_bitset_width || idx_y>=creep_spread_bitset_height) return false;
		if (!bs.test(idx_x + idx_y*creep_spread_bitset_width)) return false;
		return true;
	}
};

// a_vector<grid::build_square*> get_creep_from(xy pos,int creep_source,tsc::dynamic_bitset visited) {
// 
// 	a_vector<grid::build_square*> rv;
// 	a_deque<grid::build_square*> open;
// 
// 	if (pos.x<0 || pos.y<0 || pos.x>=grid::map_width || pos.y>=grid::map_height) return rv;
// 
// 	int w = creep_source==creep_source_hatchery ? 4 : 2;
// 	int h = creep_source==creep_source_hatchery ? 3 : 2;
// 	double max_d = creep_source==creep_source_hatchery ? 8*32 : 9*32;
// 
// 	xy top_left = pos;
// 	xy bottom_right = pos + xy(w*32 - 16,h*32 - 16);
// 
// 	a_vector<grid::build_square*> circle;
// 
// 	log("creep %d - \n",creep_source);
// 	open.push_back(&grid::get_build_square(pos));
// 	visited.set((unsigned)pos.x/32 + (unsigned)pos.y/32 * grid::build_grid_width);
// 	size_t n = 0;
// 	while (!open.empty()) {
// 		grid::build_square*s = open.front();
// 		open.pop_front();
// // 		auto pos = nearest_spot_in_square(s->pos,top_left,bottom_right);
// // 		if (std::abs(s->pos.y - pos.y)>5*32) continue;
// // 		double d = (pos - s->pos).length();
// // 		if (d>max_d) continue;
// 		if (!existing_creep.test(s->pos.x/32 + s->pos.y/32 * grid::build_grid_width)) continue;
// 		circle.push_back(s);
// 		xy r = s->pos - pos;
// 		log("{%d,%d},",r.x,r.y);
// 		if (++n%8==0) log("\n");
// 
// 		auto add = [&](xy pos) {
// 			if (pos.x<0 || pos.y<0 || pos.x>=grid::map_width || pos.y>=grid::map_height) return;
// 			if (visited.test((unsigned)pos.x/32 + (unsigned)pos.y/32 * grid::build_grid_width)) return;
// 			open.push_back(&grid::get_build_square(pos));
// 			visited.set((unsigned)pos.x/32 + (unsigned)pos.y/32 * grid::build_grid_width);
// 		};
// 		add(s->pos + xy(32,0));
// 		add(s->pos + xy(0,32));
// 		add(s->pos + xy(-32,0));
// 		add(s->pos + xy(0,-32));
// 	}
// 	log("\n");
// 	
// 	return circle;
// 	return rv;
// }

void update_creep_task() {

	while (true) {

		existing_creep.reset();
		for (int y=0;y<grid::build_grid_height;++y) {
			for (int x=0;x<grid::build_grid_width;++x) {
				grid::build_square&bs = grid::build_grid[x + y*grid::build_grid_width];
				if (bs.has_creep) existing_creep.set(x + y*grid::build_grid_width);
			}
		}

		multitasking::sleep(4);
	}

}

void update_complete_creep_spread_task() {
	while (true) {

		complete_creep_spread.reset();
		for (unit*u : my_units) {
			if (!u->building) continue;
			if (!u->is_completed && !u->is_morphing) continue;
			bool h = u->type == unit_types::hatchery || u->type==unit_types::lair || u->type==unit_types::hive;
			bool c = u->type == unit_types::creep_colony || u->type == unit_types::sunken_colony || u->type == unit_types::spore_colony;
			if (h || c) {
				for (grid::build_square*bs : get_creep_from(u->building->build_pos,h ? creep_source_hatchery : creep_source_creep_colony)) {
					complete_creep_spread.set(bs->pos.x/32 + bs->pos.y/32 * grid::build_grid_width);
				}
			}
		}

		multitasking::sleep(15);
	}
}

void render() {

// 	tsc::dynamic_bitset creep(grid::build_grid_width * grid::build_grid_height);
// 	for (unit*u : my_units) {
// 		if (!u->building) continue;
// 		bool h = u->type->game_unit_type==BWAPI::UnitTypes::Zerg_Hatchery;
// 		bool c = u->type->game_unit_type==BWAPI::UnitTypes::Zerg_Creep_Colony || u->type->game_unit_type==BWAPI::UnitTypes::Zerg_Sunken_Colony || u->type->game_unit_type==BWAPI::UnitTypes::Zerg_Spore_Colony;
// 		if (h || c) {
// 			for (grid::build_square*bs : get_creep_from(u->building->pos,h ? creep_source_hatchery : creep_source_creep_colony)) {
// 				creep.set(bs->pos.x/32 + bs->pos.y/32 * grid::build_grid_width);
// 			}
// 		}
// 	}
// 
// 	for (int y=0;y<grid::build_grid_height;++y) {
// 		for (int x=0;x<grid::build_grid_width;++x) {
// 			bool has = existing_creep.test(x + y*grid::build_grid_width);
// 			bool should_have = creep.test(x + y*grid::build_grid_width);
// 			if (!has && !should_have) continue;
// 
// 			game->drawBoxMap(x*32,y*32,x*32+32,y*32+32,has ? (should_have ? BWAPI::Colors::Green : BWAPI::Colors::Red) : BWAPI::Colors::Blue);
// 		}
// 	}

// 	for (int y=0;y<grid::walk_grid_height;++y) {
// 		for (int x=0;x<grid::walk_grid_width;++x) {
// 			auto&ws = grid::get_walk_square(xy(x*8,y*8));
// 			if (ws.walkable) continue;
// 
// 			game->drawBoxMap(x*8,y*8,x*8+8,y*8+8,BWAPI::Colors::Purple);
// 		}
// 	}

}

void init() {

	for (xy&rel : creep_spread_hatchery_arr) creep_spread_hatchery_vec.push_back(rel);
	creep_spread_hatchery_vec.shrink_to_fit();
	for (xy&rel : creep_spread_creep_colony_arr) creep_spread_creep_colony_vec.push_back(rel);
	creep_spread_creep_colony_vec.shrink_to_fit();

	for (xy&rel : creep_spread_hatchery_arr) {
		int x = rel.x + creep_spread_bitset_width*32/2;
		int y = rel.y + creep_spread_bitset_height*32/2;
		if (x<0 || y<0) xcept("creep spread bitset too small");
		x = (unsigned)x/32;
		y = (unsigned)y/32;
		if (x>=creep_spread_bitset_width || y>=creep_spread_bitset_height) xcept("creep spread bitset too small");
		creep_spread_hatchery_bitset.set(x + y*creep_spread_bitset_width);
	}
	for (xy&rel : creep_spread_creep_colony_arr) {
		int x = rel.x + creep_spread_bitset_width*32/2;
		int y = rel.y + creep_spread_bitset_height*32/2;
		if (x<0 || y<0) xcept("creep spread bitset too small");
		x = (unsigned)x/32;
		y = (unsigned)y/32;
		if (x>=creep_spread_bitset_width || y>=creep_spread_bitset_height) xcept("creep spread bitset too small");
		creep_spread_creep_colony_bitset.set(x + y*creep_spread_bitset_width);
	}

	existing_creep.resize(grid::build_grid_width * grid::build_grid_height);
	complete_creep_spread.resize(grid::build_grid_width * grid::build_grid_height);

	multitasking::spawn(update_creep_task,"update creep");
	multitasking::spawn(update_complete_creep_spread_task,"update complete creep spread");

	render::add(render);

}

}