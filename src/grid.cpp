//
// This file defines the interface for retrieving information about squares
// on the map, e.g. walkability and fog-of-war.
//

#include "grid.h"
#include "common.h"
#include "units.h"
#include "bot.h"

namespace tsc_bwai {

	size_t grid_t::walk_square_index(walk_square& sq) {
		return &sq - walk_grid.data();
	}

	xy grid_t::walk_square_pos(walk_square& sq) {
		size_t index = walk_square_index(sq);
		return xy(index % (size_t)walk_grid_width * 8, index / (size_t)walk_grid_width * 8);
	}

	walk_square& grid_t::walk_square_neighbor(walk_square& sq, int n) {
		if (n == 0) return *(&sq + 1); // right
		if (n == 1) return *(&sq + walk_grid_width); // down;
		if (n == 2) return *(&sq - 1); // left;
		if (n == 3) return *(&sq - walk_grid_width); // up;
		return *(walk_square*)nullptr;
	}

	walk_square& grid_t::get_walk_square(xy pos) {
		if (pos.x < 0) pos.x = 0;
		if (pos.y < 0) pos.y = 0;
		size_t x = (size_t)pos.x / 8;
		size_t y = (size_t)pos.y / 8;
		if (x > walk_grid_last_x) x = walk_grid_last_x;
		if (y > walk_grid_last_y) y = walk_grid_last_y;
		return walk_grid[x + y*walk_grid_width];
	}

	bool grid_t::is_walkable(xy pos) {
		return get_walk_square(pos).walkable;
	}

	size_t grid_t::build_square_index(const build_square&bs) {
		return &bs - build_grid.data();
	}

	size_t grid_t::build_square_index(xy pos) {
		return (unsigned)pos.x / 32 + (unsigned)pos.y / 32 * build_grid_width;
	}

	build_square& grid_t::get_build_square(xy pos) {
		if (pos.x < 0) pos.x = 0;
		if (pos.y < 0) pos.y = 0;
		size_t x = (size_t)pos.x / 32;
		size_t y = (size_t)pos.y / 32;
		if (x > build_grid_last_x) x = build_grid_last_x;
		if (y > build_grid_last_y) y = build_grid_last_y;
		return build_grid[x + y*build_grid_width];
	}

	bool grid_t::is_visible(xy pos) {
		return get_build_square(pos).visible;
	}

	bool grid_t::is_visible(xy pos, int tile_width, int tile_height) {
		for (int y = 0; y < tile_height; ++y) {
			for (int x = 0; x < tile_width; ++x) {
				if (!is_visible(pos + xy(x * 32, y * 32))) return false;
			}
		}
		return true;
	}

	void grid_t::reserve_build_squares(xy pos, unit_type* ut, bool reserve) {
		int w = ut->tile_width;
		int h = ut->tile_height;
		if (pos.x < 0) xcept("attempt to reserve outside map boundaries");
		if (pos.y < 0) xcept("attempt to reserve outside map boundaries");
		size_t x = (size_t)pos.x / 32;
		size_t y = (size_t)pos.y / 32;
		if (x + w - 1 > build_grid_last_x) xcept("attempt to reserve outside map boundaries");
		if (y + h - 1 > build_grid_last_y) xcept("attempt to reserve outside map boundaries");
		for (int iy = 0; iy < h; ++iy) {
			for (int ix = 0; ix < w; ++ix) {
				auto&bs = build_grid[x + ix + (y + iy)*build_grid_width];
				if (!!bs.reserved.first == reserve) xcept("attempt to %s build square", reserve ? "reserve reserved" : "unreserve unreserved");
				bs.reserved.first = reserve ? ut : 0;
				if (reserve) bs.reserved.second = pos;
			}
		}
	}
	void grid_t::unreserve_build_squares(xy pos, unit_type*ut) {
		reserve_build_squares(pos, ut, false);
	}




}

