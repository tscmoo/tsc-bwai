
namespace grid {
;

int map_width, map_height;

struct walk_square {
	bool walkable; // Static map value; not updated by buildings
	// Not guaranteed to be immediately updated; every 4 ticks if scheduling allows.
	// Only add-ons can extend outside their build squares (to the left), so there should be
	// a maximum of 2 buildings in any given walk square.
	// ++ It seems some map features has buildings on top of each other? (minerals blocking paths on destination)
	//tsc::static_vector<unit*, 2> buildings;
	a_vector<unit*> buildings;
};
a_vector<walk_square> walk_grid;
int walk_grid_width, walk_grid_height;
size_t walk_grid_last_x, walk_grid_last_y;

size_t walk_square_index(walk_square&sq) {
	return &sq - walk_grid.data();
}

xy walk_square_pos(walk_square&sq) {
	size_t index = walk_square_index(sq);
	return xy(index % (size_t)walk_grid_width * 8, index / (size_t)walk_grid_width * 8);
}

walk_square&walk_square_neighbor(walk_square&sq, int n) {
	if (n == 0) return *(&sq + 1); // right
	if (n == 1) return *(&sq + walk_grid_width); // down;
	if (n == 2) return *(&sq - 1); // left;
	if (n == 3) return *(&sq - walk_grid_width); // up;
	return *(walk_square*)nullptr;
}

walk_square&get_walk_square(xy pos) {
	if (pos.x<0) pos.x = 0;
	if (pos.y<0) pos.y = 0;
	size_t x = (size_t)pos.x/8;
	size_t y = (size_t)pos.y/8;
	if (x>walk_grid_last_x) x = walk_grid_last_x;
	if (y>walk_grid_last_y) y = walk_grid_last_y;
	return walk_grid[x + y*walk_grid_width];
}

bool is_walkable(xy pos) {
	return get_walk_square(pos).walkable;
}

struct build_square {
	xy pos;
	int height;
	bool visible;
	bool has_creep;
	bool buildable;
	unit*building;
	std::pair<unit_type*,xy> reserved;
	bool mineral_reserved;
	bool no_resource_depot;
	bool entirely_walkable;
	int last_seen;
	template<int=0>
	build_square*get_neighbor(int d) {
		xy p;
		if (d==0) p = pos + xy(32,0);
		else if (d==1) p = pos + xy(0,32);
		else if (d==2) p = pos + xy(-32,0);
		else p = pos + xy(0,-32);
		if (p.x<0 || p.y<0 || p.x>=map_width || p.y>=map_height) return 0;
		return &build_grid[(unsigned)p.x/32 + (unsigned)p.y/32 * build_grid_width];
	}
};
a_vector<build_square> build_grid;
int build_grid_width, build_grid_height;
size_t build_grid_last_x, build_grid_last_y;

struct build_grid_indexer {
	size_t operator()(xy pos) const {
		return (unsigned)pos.x/32 + (unsigned)pos.y/32 * build_grid_width;
	}
};

size_t build_square_index(const build_square&bs) {
	return &bs - build_grid.data();
}
size_t build_square_index(xy pos) {
	return build_grid_indexer()(pos);
}

build_square&get_build_square(xy pos) {
	if (pos.x<0) pos.x = 0;
	if (pos.y<0) pos.y = 0;
	size_t x = (size_t)pos.x/32;
	size_t y = (size_t)pos.y/32;
	if (x>build_grid_last_x) x = build_grid_last_x;
	if (y>build_grid_last_y) y = build_grid_last_y;
	return build_grid[x + y*build_grid_width];
}

bool is_visible(xy pos) {
	return get_build_square(pos).visible;
}
bool is_visible(xy pos, int tile_width, int tile_height) {
	for (int y = 0; y < tile_height; ++y) {
		for (int x = 0; x < tile_width; ++x) {
			if (!is_visible(pos + xy(x * 32, y * 32))) return false;
		}
	}
	return true;
}
template<int=0>
void reserve_build_squares(xy pos,unit_type*ut,bool reserve=true) {
	int w = ut->tile_width;
	int h = ut->tile_height;
	if (pos.x<0) xcept("attempt to reserve outside map boundaries");
	if (pos.y<0) xcept("attempt to reserve outside map boundaries");
	size_t x = (size_t)pos.x/32;
	size_t y = (size_t)pos.y/32;
	if (x+w-1>build_grid_last_x) xcept("attempt to reserve outside map boundaries");
	if (y+h-1>build_grid_last_y) xcept("attempt to reserve outside map boundaries");
	for (int iy=0;iy<h;++iy) {
		for (int ix=0;ix<w;++ix) {
			auto&bs = build_grid[x+ix + (y+iy)*build_grid_width];
			if (!!bs.reserved.first==reserve) xcept("attempt to %s build square",reserve ? "reserve reserved" : "unreserve unreserved");
			bs.reserved.first = reserve ? ut : 0;
			if (reserve) bs.reserved.second = pos;
		}
	}
}
void unreserve_build_squares(xy pos,unit_type*ut) {
	reserve_build_squares(pos,ut,false);
}

void update_build_grid() {
	for (int y=0;y<build_grid_height;++y) {
		for (int x=0;x<build_grid_width;++x) {
			build_square&bs = build_grid[x + y*build_grid_width];
			bs.visible = game->isVisible(x,y);
			if (bs.visible) bs.has_creep = game->hasCreep(x,y); 
			if (bs.visible) bs.last_seen = current_frame;
		}
	}
}

struct sixtyfour_square {
	//a_vector<pathing::distance_map*> distance_map_used;
};
a_vector<sixtyfour_square> sixtyfour_grid;
int sixtyfour_grid_width, sixtyfour_grid_height;
size_t sixtyfour_grid_last_x, sixtyfour_grid_last_y;

sixtyfour_square&get_sixtyfour_square(xy pos) {
	if (pos.x<0) pos.x = 0;
	if (pos.y<0) pos.y = 0;
	size_t x = (size_t)pos.x/64;
	size_t y = (size_t)pos.y/64;
	if (x>sixtyfour_grid_last_x) x = sixtyfour_grid_last_x;
	if (y>sixtyfour_grid_last_y) y = sixtyfour_grid_last_y;
	return sixtyfour_grid[x + y*sixtyfour_grid_width];
}

template<int=0>
void update_mineral_reserved_task() {
	multitasking::sleep(1);
	while (true) {

		for (auto&v : build_grid) {
			v.mineral_reserved = false;
			v.no_resource_depot = false;
		}
		
		for (unit*u : resource_units) {
			if (!u->building) continue;
			if (u->resources<8*(int)my_workers.size()/4) continue;
			for (int y=-3;y<3+u->type->tile_height;++y) {
				for (int x=-3;x<3+u->type->tile_width;++x) {
					get_build_square(u->building->build_pos + xy(x*32,y*32)).no_resource_depot = true;
				}
			}

		}

		for (unit*u : resource_units) {
			if (!u->building) continue;
			if (u->resources==0) continue;

			auto invalidate = [&](unit*u) {
				for (int y = -1; y < u->type->tile_height + 1; ++y) {
					for (int x = -1; x < u->type->tile_width + 1; ++x) {
						get_build_square(u->building->build_pos + xy(x * 32, y * 32)).mineral_reserved = true;
					}
				}
			};
			invalidate(u);

			unit*n = get_best_score(my_resource_depots, [&](unit*n) {
				if (u->building->is_lifted) return std::numeric_limits<double>::infinity();
				return units_distance(n,u);
			},std::numeric_limits<double>::infinity());
			if (n && units_distance(n,u)<=32*8) {
				invalidate(n);
			}
		}

		// quick hack to allow space for comsats
		for (unit*u : my_resource_depots) {
			if (u->type != unit_types::cc) continue;
			for (int y = 1; y < 3+1; ++y) {
				for (int x = 4; x < 6+1; ++x) {
					get_build_square(u->building->build_pos + xy(x * 32, y * 32)).mineral_reserved = true;
				}
			}
		}

		multitasking::sleep(15*10);
	}
}

void init() {

	map_width = game->mapWidth()*32;
	map_height = game->mapHeight()*32;

	build_grid_width = map_width/32;
	build_grid_height = map_height/32;
	build_grid.resize(build_grid_width * build_grid_height);
	build_grid_last_x = build_grid_width-1;
	build_grid_last_y = build_grid_height-1;

	for (int y=0;y<build_grid_height;++y) {
		for (int x=0;x<build_grid_width;++x) {
			xy pos(x*32,y*32);
			build_square&bs = get_build_square(pos);
			bs.pos = pos;
			bs.height = game->getGroundHeight(x,y);
			bs.buildable = game->isBuildable(x,y);
			bs.has_creep = false;
			bs.building = nullptr;
			bs.mineral_reserved = false;
			bs.no_resource_depot = false;
			bs.entirely_walkable = false;
			bs.last_seen = 0;
		}
	}

	walk_grid_width = map_width/8;
	walk_grid_height = map_height/8;
	walk_grid.resize(walk_grid_width * walk_grid_height);
	walk_grid_last_x = walk_grid_width-1;
	walk_grid_last_y = walk_grid_height-1;

	for (int y=0;y<walk_grid_height;++y) {
		for (int x=0;x<walk_grid_width;++x) {
			walk_square&ws = get_walk_square(xy(x*8,y*8));
			ws.walkable = game->isWalkable(x,y);
		}
	}

	for (int y = 0; y < build_grid_height; ++y) {
		for (int x = 0; x < build_grid_width; ++x) {
			xy pos(x * 32, y * 32);
			build_square&bs = get_build_square(pos);
			bs.entirely_walkable = true;
			for (int yy = 0; yy < 4 && bs.entirely_walkable; ++yy) {
				for (int xx = 0; xx < 4 && bs.entirely_walkable; ++xx) {
					walk_square&ws = get_walk_square(pos + xy(xx * 8, yy * 8));
					if (!ws.walkable) bs.entirely_walkable = false;
				}
			}
		}
	}

	sixtyfour_grid_width = map_width/64;
	sixtyfour_grid_height = map_height/64;
	sixtyfour_grid.resize(sixtyfour_grid_width * sixtyfour_grid_height);
	sixtyfour_grid_last_x = sixtyfour_grid_width-1;
	sixtyfour_grid_last_y = sixtyfour_grid_height-1;


	multitasking::spawn(update_mineral_reserved_task<0>,"update mineral reserved");

}

}


