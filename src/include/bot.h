//
// This file defines the bot_t class, which is the main instance of the bot.
// It contains some commonly used methods and variables, and all the modules
// that make up the bot.
// Multiple instances of this class can exist, potentially in multiple threads,
// playing games simultaneously.
//

#ifndef TSC_BWAI_BOT_H
#define TSC_BWAI_BOT_H

#include "multitasking.h"
#include "common.h"
#include "log.h"
#include "units.h"

namespace BWAPI {
	class Game;
}

namespace tsc_bwai {

	class bot_t {
		a_deque<a_string> send_text_queue;
	public:

		// BWAPI interface
		BWAPI::Game* game;

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

		// Sends text in the chat.
		void send_text(a_string str);

		multitasking::multitasking_t multitasking;
		common_t common;
		log_t log;
		units_t units;

		bot_t() :
			multitasking(*this),
			log(*this),
			units(*this)
		{}

		void on_frame();

		void init() {

		}

	};
}

#endif
