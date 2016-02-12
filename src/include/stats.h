//
// This file defines the stats module.
//

#ifndef TSC_BWAI_STATS_H
#define TSC_BWAI_STATS_H

namespace tsc_bwai {

	class bot_t;
	
	// stats_module - responsible for updating some common variables
	// like current minerals and supplies
	class stats_module {
		bot_t& bot;
		void stats_task();
	public:
		stats_module(bot_t& bot) : bot(bot) {}
		void init();
	};

}

#endif
