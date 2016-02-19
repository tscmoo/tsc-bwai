//
// This file defines the bot_t class, which is the main instance of the bot.
//

#ifndef TSC_BWAI_BOT_H
#define TSC_BWAI_BOT_H

#include "multitasking.h"
#include "common.h"
#include "log.h"
#include "players.h"
#include "grid.h"
#include "stats.h"
#include "units.h"
#include "square_pathing.h"
#include "flyer_pathing.h"
#include "unit_controls.h"
#include "resource_spots.h"

namespace BWAPI {
	class Game;
}

namespace tsc_bwai {
	
	// bot_t - the main instance of the bot.
	// Contains some commonly used methods and variables, and all the modules
	// that make up the bot.
	class bot_t {
		a_deque<a_string> send_text_queue;
	public:

		// BWAPI interface, this needs to be set before init is called.
		BWAPI::Game* game = nullptr;

		// Setting test_mode will enable *a ton* of debugging output, both in the
		// console, on-screen drawing and output to log.txt.
		bool test_mode = true;
		// The current frame, as counted by BWAPI.
		// The convention used in this bot is that 15 frames makes up one second,
		// though most (unrelated) timers count 24 frames in one second. It doesn't
		// matter greatly, since raw frame counts are used to actually measure game
		// time.
		int current_frame = 0;

		// The remaining latency frames, as returned by BWAPI
		// getRemainingLatencyFrames().
		// This is the number of frames until any orders issued this frame will
		// actually be issued.
		// Note that this value is not static, since Broodwar accumulates all
		// commands for a few frames, then sends the most recent ones for each unit.
		int latency_frames = 0;
		// The frame at which any orders issued will actually be issued.
		// Equivalent to current_frame + latency_frames.
		int current_command_frame = 0;

		// Our current minerals.
		double current_minerals = 0;
		// Our current gas.
		double current_gas = 0;
		// Our current used supply for each race (index with race_terran etc).
		std::array<double, 3> current_used_supply{ 0,0,0 };
		// Our current max supply for each race (index with race_terran etc).
		std::array<double, 3> current_max_supply{ 0,0,0 };
		// The sum of all our used supplies for each race.
		// Exists for convenience when we just want to check our current supply
		// without caring for which race we are playing.
		double current_used_total_supply = 0;

		// Our current mineral income per frame, averaged out over a certain number
		// of frames.
		double current_minerals_per_frame = 0;
		// Our current gas income per frame, averaged out over a certain number of
		// frames.
		double current_gas_per_frame = 0;
		// Our current predicted mineral income, as calculated by resource_gathering.
		double predicted_minerals_per_frame = 0;
		// Our current predicted gas income, as calculated by resource_gathering.
		double predicted_gas_per_frame = 0;

		// All possible start locations.
		a_vector<xy> start_locations;
		// Our start location.
		xy my_start_location;

		// By default we call gg and quit the game if we evaluate that we have lost
		// the game. This behavior can be disabled by setting this to true (should
		// be done for UMS maps, for instance).
		bool dont_call_gg = false;

		// Sends text in the chat.
		void send_text(const a_string& str);

		multitasking::multitasking_module multitasking;
		common_module common;
		log_module log;
		players_module players;
		grid_module grid;
		stats_module stats;
		units_module units;
		square_pathing_module square_pathing;
		flyer_pathing_module flyer_pathing;
		unit_controls_module unit_controls;
		resource_spots_module resource_spots;

		bot_t() :
			multitasking(*this),
			log(*this),
			players(*this),
			grid(*this),
			stats(*this),
			units(*this),
			square_pathing(*this),
			flyer_pathing(*this),
			unit_controls(*this),
			//resource_gathering(*this),
			resource_spots(*this)
		{}

// 		grid::init();
// 		stats::init();
// 		units::init();
// 		upgrades::init();
// 		square_pathing::init();
// 		unit_controls::init();
// 		resource_gathering::init();
// 		resource_spots::init();
// 		creep::init();
// 		pylons::init();
// 		build_spot_finder::init();
// 		build::init();
// 		buildpred::init();
// 		combat::init();
// 		scouting::init();
// 		get_upgrades::init();
// 		wall_in::init();
// 		strategy::init();
// 		//patrec::init();
// 		//test_stratm::init();
// 		buildopt::init();

		// This should be called once per frame, preferably from the onFrame handler.
		void on_frame();

		// This should be once when the game starts, preferably from the onStart
		// handler.
		void init();

	};
}

#endif
