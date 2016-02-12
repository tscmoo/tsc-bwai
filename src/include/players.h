//
// This file declares the interface for retrieving information about players.
//

#ifndef TSC_BWAI_PLAYERS_H
#define TSC_BWAI_PLAYERS_H

#include "common.h"
#include "upgrades.h"

namespace tsc_bwai {

	class bot_t;

	struct player_t {
		// BWAPI interface for the player,
		BWAPI_Player game_player;
		// Flag that specifies whether it is an enemy player.
		bool is_enemy;
		// Estimated resources spent based on scouted units.
		double minerals_spent, gas_spent;
		// Estimated resources lost based on destroyed units.
		double minerals_lost, gas_lost;
		// Number of resource depots that have been scouted.
		int resource_depots_seen;
		// Number of workers that have been scouted.
		int workers_seen;
		// The race, always one of race_terran, race_protoss or race_zerg.
		int race;
		// Flag that specifies whether this player is random, and
		// has not been scouted yet. As soon as any units from this player are
		// seen, this flag will be set to false and race will be set to the
		// appropriate race.
		bool random;
		// Whether units from this team are set as "rescuable", which can only
		// occur in UMS maps.
		bool rescuable;

		// The set of completed upgrades. Use has_upgrade to check for individual
		// ones.
		a_unordered_set<upgrade_type*> upgrades;
		// Returns whether the specified upgrade is completed.
		bool has_upgrade(upgrade_type* t) {
			return upgrades.find(t) != upgrades.end();
		}
	};

	class players_module {
		bot_t& bot;
		a_deque<player_t> player_container;
		a_unordered_map<BWAPI_Player, player_t*> player_map;
		void update_players_task();
	public:
		players_module(bot_t& bot) : bot(bot) {}
		// BWAPI interface to the local player.
		// Is always set to a valid player, even if game->self() returns null.
		BWAPI_Player self;
		// The local player. Never null.
		player_t* my_player = nullptr;
		// The opponent player. Never null.
		player_t* opponent_player = nullptr;
		// The neutral player. Never null.
		player_t* neutral_player = nullptr;

		// Retrieves the player_t for the specified BWAPI player.
		player_t* get_player(BWAPI_Player game_player);

		// Called from bot_t::init.
		void init();

	};
}

#endif
