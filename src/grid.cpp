//
// This file defines the interface for retrieving information about squares
// on the map, e.g. walkability and fog-of-war.
//

#include "grid.h"
#include "common.h"
#include "units.h"
#include "bot.h"

namespace tsc_bwai {

	size_t grid_module::walk_square_index(walk_square& sq) {
		return &sq - walk_grid.data();
	}

	xy grid_module::walk_square_pos(walk_square& sq) {
		size_t index = walk_square_index(sq);
		return xy(index % (size_t)walk_grid_width * 8, index / (size_t)walk_grid_width * 8);
	}

	walk_square& grid_module::walk_square_neighbor(walk_square& sq, int n) {
		if (n == 0) return *(&sq + 1); // right
		if (n == 1) return *(&sq + walk_grid_width); // down;
		if (n == 2) return *(&sq - 1); // left;
		if (n == 3) return *(&sq - walk_grid_width); // up;
		return *(walk_square*)nullptr;
	}

	walk_square& grid_module::get_walk_square(xy pos) {
		if (pos.x < 0) pos.x = 0;
		if (pos.y < 0) pos.y = 0;
		size_t x = (size_t)pos.x / 8;
		size_t y = (size_t)pos.y / 8;
		if (x > walk_grid_last_x) x = walk_grid_last_x;
		if (y > walk_grid_last_y) y = walk_grid_last_y;
		return walk_grid[x + y*walk_grid_width];
	}

	bool grid_module::is_walkable(xy pos) {
		return get_walk_square(pos).walkable;
	}

	size_t grid_module::build_square_index(const build_square& bs) {
		return &bs - build_grid.data();
	}

	size_t grid_module::build_square_index(xy pos) {
		return (unsigned)pos.x / 32 + (unsigned)pos.y / 32 * build_grid_width;
	}

	build_square& grid_module::get_build_square(xy pos) {
		if (pos.x < 0) pos.x = 0;
		if (pos.y < 0) pos.y = 0;
		size_t x = (size_t)pos.x / 32;
		size_t y = (size_t)pos.y / 32;
		if (x > build_grid_last_x) x = build_grid_last_x;
		if (y > build_grid_last_y) y = build_grid_last_y;
		return build_grid[x + y*build_grid_width];
	}

	build_square* grid_module::find_build_square(xy pos) {
		if ((size_t)pos.x >= (size_t)map_width) return nullptr;
		if ((size_t)pos.y >= (size_t)map_height) return nullptr;
		return &build_grid[(size_t)pos.x / 32 + (size_t)pos.y / 32 * build_grid_width];
	}

	bool grid_module::is_visible(xy pos) {
		return get_build_square(pos).visible;
	}

	bool grid_module::is_visible(xy pos, int tile_width, int tile_height) {
		for (int y = 0; y < tile_height; ++y) {
			for (int x = 0; x < tile_width; ++x) {
				if (!is_visible(pos + xy(x * 32, y * 32))) return false;
			}
		}
		return true;
	}

	void grid_module::reserve_build_squares(xy pos, unit_type* ut, bool reserve) {
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
	void grid_module::unreserve_build_squares(xy pos, unit_type*ut) {
		reserve_build_squares(pos, ut, false);
	}

	void grid_module::update_build_grid() {
		for (int y = 0; y < build_grid_height; ++y) {
			for (int x = 0; x < build_grid_width; ++x) {
				build_square& bs = build_grid[x + y*build_grid_width];
				bs.visible = bot.game->isVisible(x, y);
				if (bs.visible) bs.has_creep = bot.game->hasCreep(x, y);
				if (bs.visible) bs.last_seen = bot.current_frame;
			}
		}
	}

	void grid_module::update_blocked_build_squares_task() {
		bot.multitasking.sleep(1);
		while (true) {

			// This loop regularly flags some build squares so that we don't build on them.

			// Initially nothing is flagged.
			for (auto&v : build_grid) {
				v.mineral_reserved = false;
				v.no_resource_depot = false;
				v.blocked_by_larva_or_egg = false;
				v.blocked_until = 0;
			}

			// It's not possible to build resource depots very close to resources, so
			// flag the space around them.
			for (unit*u : bot.units.resource_units) {
				if (!u->building) continue;
				if (u->resources < 8 * (int)bot.units.my_workers.size() / 4) continue;
				for (int y = -3; y < 3 + u->type->tile_height; ++y) {
					for (int x = -3; x < 3 + u->type->tile_width; ++x) {
						get_build_square(u->building->build_pos + xy(x * 32, y * 32)).no_resource_depot = true;
					}
				}

			}

			// Flag the space around resources. Easy way to prevent blocking them off.
			for (unit*u : bot.units.resource_units) {
				if (!u->building) continue;
				if (u->resources == 0) continue;

				auto invalidate = [&](unit*u) {
					for (int y = -1; y < u->type->tile_height + 1; ++y) {
						for (int x = -1; x < u->type->tile_width + 1; ++x) {
							get_build_square(u->building->build_pos + xy(x * 32, y * 32)).mineral_reserved = true;
						}
					}
				};
				invalidate(u);
			}

			// Flag in a line from the resource depot to the resource, to prevent
			// building in the mineral line.
			for (unit*u : bot.units.my_resource_depots) {
				for (unit*u2 : bot.units.resource_units) {
					if (units_distance(u, u2) >= 32 * 8) continue;
					xy relpos = u2->pos - u->pos;
					for (int i = 0; i < 8; ++i) {
						get_build_square(u->pos + relpos / 8 * (i + 1)).mineral_reserved = true;
					}
				}
			}

			// Flag the space that a comsat or nuclear silo would occupy, since we really
			// don't want to move the command center to build those.
			for (unit*u : bot.units.my_resource_depots) {
				if (u->type != unit_types::cc) continue;
				for (int y = 1; y < 3 + 1; ++y) {
					for (int x = 4; x < 6 + 1; ++x) {
						get_build_square(u->building->build_pos + xy(x * 32, y * 32)).mineral_reserved = true;
					}
				}
			}

			// Flag the space around larva and eggs.
			for (unit*u : bot.units.visible_units) {
				if (u->type != unit_types::larva && u->type != unit_types::egg && u->type != unit_types::lurker_egg) continue;
				get_build_square(u->pos).blocked_by_larva_or_egg = true;
				get_build_square(u->pos + xy(-u->type->dimension_left(), -u->type->dimension_up())).blocked_by_larva_or_egg = true;
				get_build_square(u->pos + xy(u->type->dimension_right(), -u->type->dimension_up())).blocked_by_larva_or_egg = true;
				get_build_square(u->pos + xy(u->type->dimension_right(), u->type->dimension_down())).blocked_by_larva_or_egg = true;
				get_build_square(u->pos + xy(-u->type->dimension_left(), u->type->dimension_down())).blocked_by_larva_or_egg = true;
			}

			bot.multitasking.sleep(15 * 10);
		}
	}


	void grid_module::init() {

		map_width = bot.game->mapWidth() * 32;
		map_height = bot.game->mapHeight() * 32;

		build_grid_width = map_width / 32;
		build_grid_height = map_height / 32;
		build_grid.resize(build_grid_width * build_grid_height);
		build_grid_last_x = build_grid_width - 1;
		build_grid_last_y = build_grid_height - 1;

		for (int y = 0; y < build_grid_height; ++y) {
			for (int x = 0; x < build_grid_width; ++x) {
				xy pos(x * 32, y * 32);
				build_square& bs = get_build_square(pos);
				bs.pos = pos;
				bs.height = bot.game->getGroundHeight(x, y);
				bs.buildable = bot.game->isBuildable(x, y);
				bs.has_creep = false;
				bs.building = nullptr;
				bs.mineral_reserved = false;
				bs.no_resource_depot = false;
				bs.entirely_walkable = false;
				bs.last_seen = 0;
				bs.reserved_for_resource_depot = false;
				bs.blocked_by_larva_or_egg = false;
			}
		}

		walk_grid_width = map_width / 8;
		walk_grid_height = map_height / 8;
		walk_grid.resize(walk_grid_width * walk_grid_height);
		walk_grid_last_x = walk_grid_width - 1;
		walk_grid_last_y = walk_grid_height - 1;

		for (int y = 0; y < walk_grid_height; ++y) {
			for (int x = 0; x < walk_grid_width; ++x) {
				walk_square& ws = get_walk_square(xy(x * 8, y * 8));
				ws.walkable = bot.game->isWalkable(x, y);
			}
		}

		for (int y = 0; y < build_grid_height; ++y) {
			for (int x = 0; x < build_grid_width; ++x) {
				xy pos(x * 32, y * 32);
				build_square& bs = get_build_square(pos);
				bs.entirely_walkable = true;
				for (int yy = 0; yy < 4 && bs.entirely_walkable; ++yy) {
					for (int xx = 0; xx < 4 && bs.entirely_walkable; ++xx) {
						walk_square& ws = get_walk_square(pos + xy(xx * 8, yy * 8));
						if (!ws.walkable) bs.entirely_walkable = false;
					}
				}
			}
		}

		bot.multitasking.spawn(std::bind(&grid_module::update_blocked_build_squares_task, this), "update blocked build squares");

	}

}

