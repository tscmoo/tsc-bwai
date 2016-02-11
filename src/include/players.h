//
// This file declares the interface for retrieving information about players.
//

#ifndef TSC_BWAI_PLAYERS_H
#define TSC_BWAI_PLAYERS_H

#include "common.h"
#include "upgrades.h"

namespace tsc_bwai {

	struct player_t {
		BWAPI_Player game_player;
		bool is_enemy;
		double minerals_spent;
		double gas_spent;
		double minerals_lost;
		double gas_lost;
		int resource_depots_seen;
		int workers_seen;
		int race;
		bool random;
		bool rescuable;

		a_unordered_set<upgrade_type*> upgrades;
		bool has_upgrade(upgrade_type*t) {
			return upgrades.find(t) != upgrades.end();
		}
	};

	class players_t {

		a_deque<player_t> player_container;
		a_unordered_map<BWAPI_Player, player_t*> player_map;

		BWAPI_Player self;

	public:
		player_t* my_player = nullptr;
		player_t* opponent_player = nullptr;
		player_t* neutral_player = nullptr;

		player_t* get_player(BWAPI_Player game_player);
		void init();

	};
}

#endif
