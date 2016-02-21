//
// This file defines resource_gathering_module.
//

#ifndef TSC_BWAI_RESOURCE_GATHERING_H
#define TSC_BWAI_RESOURCE_GATHERING_H

#include "common.h"
#include "intrusive_list.h"
#include <utility>

namespace tsc_bwai {

	class bot_t;

	// resource_gathering_module - Responsible for assigning workers to gather
	// resources. Makes some decisions about where to assign them based on threat
	// (returned by combat), but actually responding to threats is still up to
	// combat.
	class resource_gathering_module {
		bot_t& bot;

		struct gatherer_t;
		struct resource_t {
			std::pair<resource_t*, resource_t*> link;
			int live_frame = 0;
			unit* u = nullptr;
			a_vector<gatherer_t*> gatherers;
			unit* depot = nullptr;
			bool dead = false;
			int round_trip_time = 10;
			int last_round_trip_time_calc = 0;
			a_vector<double> income_rate, income_sum;
			int last_update_income_rate = -1;
			int current_complete_round_trip_time = 0;
			a_vector<int> deliveries;
		};

		struct gatherer_t {
			std::pair<gatherer_t*, gatherer_t*> link;
			int live_frame = 0;
			unit* u = nullptr;
			resource_t* resource = nullptr;
			bool dead = false;
			int last_delivery = 0;
			bool is_carrying = false;
			int last_find_transfer = 0;
			int round_trip_begin = 0;
		};

		static const int max_predicted_frames = 15 * 120;
		a_map<int, int> predicted_mineral_delivery, predicted_gas_delivery;
		double minerals_to_gas_weight = 1.0;
		double max_gas = 0.0;

		a_list<resource_t> all_resources;
		intrusive_list<resource_t, void, &resource_t::link> live_resources, dead_resources;
		a_unordered_map<unit*, resource_t*> unit_resource_map;

		a_list<gatherer_t> all_gatherers;
		intrusive_list<gatherer_t, void, &gatherer_t::link> live_gatherers, dead_gatherers;
		a_unordered_map<unit*, gatherer_t*> unit_gatherer_map;

		a_vector<unit*> resource_depots;


		void clear_deliveries(resource_t& r);
		void update_deliveries(resource_t& r);

		void process(resource_t& r);
		void process(gatherer_t& g);

		void resource_gathering_task();
		void render();

	public:
		resource_gathering_module(bot_t& bot) : bot(bot) {}

		void init();
	};

}



#endif

