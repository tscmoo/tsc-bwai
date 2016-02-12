//
// This file declares the interface for retrieving information about squares
// on the map, e.g. walkability and fog-of-war.
//

#ifndef TSC_BWAI_GRID_H
#define TSC_BWAI_GRID_H

#include "common.h"

namespace tsc_bwai {

	struct unit;
	struct unit_type;

	// A walk_square is an 8x8 square which is the resolution at which Broodwar
	// stores walkability information.
	struct walk_square {
		// True if the square is walkable according to static map information.
		// (Does not account for buildings)
		bool walkable;
		// A list of all buildings that touch this square, according to the actual
		// unit dimensions (not build squares). Usually just one, but add-ons
		// extend slightly to the left of their build squares, and you can stack
		// buildings on top of each other with a map editor.
		a_vector<unit*> buildings;
	};

	// A build_square is a 32x32 square which is the resolution at which you
	// can place buildings, and Broodwar stores some map information.
	struct build_square {
		// The position of this square in pixels.
		xy pos;
		// The height of this square as returned from BWAPI getGroundHeight().
		// Unsure about the actual range, might be [0, 5].
		int height;
		// Whether this square is currently visible (not in fog of war).
		bool visible;
		// Whether this square currently has creep.
		bool has_creep;
		// Whether it's possible to build on this square.
		bool buildable;
		// The building that currently occupies this square.
		// This is primarily used to find a build location, and indicates that this
		// square is occupied. Other buildings might also occupy this square.
		unit* building;
		// Used by the build system to reserve this square for a planned building.
		// .first is the unit type that it is reserved for, and .second is the
		// top-left position that it would be built at.
		std::pair<unit_type*, xy> reserved;
		// Flag that indicates that building on this square might block off minerals
		// or other resources, and should be avoided.
		bool mineral_reserved;
		// Flag that indicates that a resource depot can not be built here (probably
		// because it is too close to resources).
		bool no_resource_depot;
		// Flag that indicates all 8x8 squares inside this square is walkable.
		bool entirely_walkable;
		// The frame at which this square was last visible.
		int last_seen;
		// Flag that indicates that this square is reserved for resource depots,
		// and other buildings should not be built here.
		bool reserved_for_resource_depot;
		// Flag that indicates that larva or egg is currently near this square,
		// and since they can not be moved, we should build somewhere else.
		bool blocked_by_larva_or_egg;
		// Flag that otherwise indicates that this square is blocked and we should
		// not build here until current_frame reaches this value.
		// This might be set after we have tried to build here for a while, but
		// it repeatedly fails for some unknown reason (could be blocked by a
		// enemy unit).
		int blocked_until;
	};

	struct grid_module {

		bot_t& bot;
		grid_module(bot_t& bot) : bot(bot) {}

		// Map dimensions, in pixels.
		int map_width, map_height;

		// All walk_squares on the map.
		a_vector<walk_square> walk_grid;
		// The dimensions of the walk_grid array, which is the map dimensions in pixels
		// divided by 8.
		int walk_grid_width, walk_grid_height;
		// Convenience variables that contain the highest possible x and y walk_grid
		// indices.
		size_t walk_grid_last_x, walk_grid_last_y;

		// Returns the index into walk_grid of the specified walk_square.
		size_t walk_square_index(walk_square& sq);

		// Returns the coordinates for the specified walk_square (in pixels).
		xy walk_square_pos(walk_square& sq);

		// Returns a reference to the neighbor walk_square in direction n as seen on
		// screen where n:
		//   0 - right
		//   1 - down
		//   2 - left
		//   3 - up
		// The behavior is undefined if the specified direction is off the map.
		walk_square& walk_square_neighbor(walk_square& sq, int n);

		// Returns a reference to the walk_square at the specified position.
		// If the position is off the map, then the nearest valid walk_square is
		// returned.
		walk_square& get_walk_square(xy pos);

		// Returns whether the walk_square at the specified position is walkable.
		bool is_walkable(xy pos);

		// All build_squares on the map.
		a_vector<build_square> build_grid;
		// The dimensions of the build_grid array, which is the map dimensions in pixels
		// divided by 32 (also the numbers that specify "map size", e.g. 128x128).
		int build_grid_width, build_grid_height;
		// Convenience variables which hold the highest possible build_grid x and y
		// coordinates.
		size_t build_grid_last_x, build_grid_last_y;

		// A function object that returns the build grid index for a specified
		// coordinate.
// 		struct build_grid_indexer {
// 			size_t operator()(xy pos) const {
// 				return (unsigned)pos.x / 32 + (unsigned)pos.y / 32 * build_grid_width;
// 			}
// 		};

		// Returns the index into build_grid of the specified build_square.
		size_t build_square_index(const build_square&bs);
		// Returns the index into build_grid of the specified position.
		size_t build_square_index(xy pos);

		// Returns a reference to the build_square at the specified position.
		// If the position is off the map, then the nearest valid build_square is
		// returned.
		build_square& get_build_square(xy pos);
		
		// Returns whether the specified position is currently visible (not in fog
		// of war).
		bool is_visible(xy pos);
		// Returns whether the specified rectangle is entirely visible.
		// tile_width and tile_height is specified in counts of build_square,
		// eg. (..., 2, 3), would check a rectangle of size 64x96 in pixels.
		// This is so that you can use unit_type::tile_width and
		// unit_type::tile_height as inputs.
		bool is_visible(xy pos, int tile_width, int tile_height);
		
		// Reserves or unreserved the squares at the specified position for
		// the specified unit, setting or unsetting build_square::reserved.
		// By default it reserves. If reserve is false, then it will unreserve.
		// This function will throw an exception if trying to reserve an already
		// reserved square or unreserved a square that is not reserved, or
		// if the position is invalid.
		void reserve_build_squares(xy pos, unit_type* ut, bool reserve = true);
		// Calls reserve_build_squares with reserve = false, as to unreserve.
		void unreserve_build_squares(xy pos, unit_type*ut);

		// Updates visibility and creep for all build squares.
		// This is called from units instead of having its own task, since
		// it's important that visibility information is updated in sync with
		// unit information.
		void update_build_grid();

		// This task updates the various fields in build_square dealing with
		// them being blocked for building.
		void update_blocked_build_squares_task();

		void init();

	};

}

#endif

