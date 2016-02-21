//
// This file defines the resource_spots module, which is responsible for 
// providing information about all the bases with resources.
//

#ifndef TSC_BWAI_RESOURCE_SPOTS_H
#define TSC_BWAI_RESOURCE_SPOTS_H

#include "common.h"
#include "intrusive_list.h"

namespace tsc_bwai {

	class bot_t;
	struct unit;

	namespace resource_spots {

		struct spot;
		struct resource_t {
			std::pair<resource_t*, resource_t*> link, live_link;
			const unit* u;
			bool dead;
			int live_frame;

			spot *s;
			std::array<double, 3> full_income_workers;
			std::array<double, 3> last_worker_mult;
			std::array<double, 3> income_rate;
			std::array<double, 3> income_sum;
		};

		struct spot : refcounted {

			xy pos;
			xy cc_build_pos;

			intrusive_list<resource_t, void, &resource_t::link> resources;

			std::array<std::vector<double>, 3> min_income_rate, gas_income_rate;
			std::array<std::vector<double>, 3> min_income_sum, gas_income_sum;
			std::array<double, 3> total_min_income, total_gas_income;
			std::array<double, 3> total_workers;
			spot() : total_min_income(), total_gas_income(), total_workers() {}
		};
	}


	class resource_spots_module {
		bot_t& bot;

		a_list<resource_spots::resource_t> all_resources;
		intrusive_list<resource_spots::resource_t, void, &resource_spots::resource_t::live_link> live_resources, dead_resources;
		a_unordered_map<const unit*, resource_spots::resource_t*> unit_resource_map;
		int last_update;

		// This sets cc_build_pos, pos and flags
		// build_square::reserved_for_resource_depot where necessary.
		void update_spots_pos();
		// Estimates the income rates for spots.
		void update_incomes();
		// The main function that creates resource spots and assigns resources
		// to them. Called continuously throughout the game to keep information
		// up-to-date.
		void update_spots();
		// This task finds and updates resources and spots.
		void resource_spots_task();

		// Main render function that gets called directly from bot_t::on_frame.
		void render();
	public:
		a_list<resource_spots::spot> spots;

		resource_spots_module(bot_t& bot) : bot(bot) {}


		void init();
	};
}

#endif

