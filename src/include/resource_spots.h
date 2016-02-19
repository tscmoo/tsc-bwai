//
// This file defines the resource_spots module, which is responsible for 
// providing information about all the bases with resources.
//

#ifndef TSC_BWAI_RESOURCE_SPOTS_H
#define TSC_BWAI_RESOURCE_SPOTS_H

#include "intrusive_list.h"

namespace tsc_bwai {

	class bot_t;

	namespace resource_spots {

		struct spot;
		struct resource_t {
			std::pair<resource_t*, resource_t*> link, live_link;
			unit* u;
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
	public:

		a_list<resource_spots::spot> spots;

		resource_spots_module(bot_t& bot) : bot(bot) {}
	};
}

#endif

