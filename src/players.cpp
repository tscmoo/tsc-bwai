//
// This file implements the players_t interface.
//

#include "players.h"
#include "bot.h"
#include "bwapi_inc.h"
using namespace tsc_bwai;

void tsc_bwai::players_module::update_players_task() {

	while (true) {

		for (auto&v : bot.players.player_container) {
			//update_upgrades(v);
		}

		bot.multitasking.sleep(15);
	}

}

player_t* tsc_bwai::players_module::get_player(BWAPI_Player game_player) {
	player_t*& p = player_map[game_player];
	if (!p) {
		player_container.emplace_back();
		p = &player_container.back();
		p->game_player = game_player;
		p->is_enemy = game_player->isEnemy(self);
		p->minerals_spent = 0.0;
		p->gas_spent = 0.0;
		p->minerals_lost = 0.0;
		p->gas_lost = 0.0;
		p->resource_depots_seen = 0;
		p->workers_seen = 0;
		p->race = race_terran;
		p->random = false;
		p->rescuable = game_player->getType() == BWAPI::PlayerTypes::RescuePassive;
		if (game_player->getRace() == BWAPI::Races::Terran) p->race = race_terran;
		else if (game_player->getRace() == BWAPI::Races::Protoss) p->race = race_protoss;
		else if (game_player->getRace() == BWAPI::Races::Zerg) p->race = race_zerg;
		else {
			p->random = true;
			p->race = race_protoss;
		}
	}
	return p;
}

void tsc_bwai::players_module::init() {
	self = bot.game->self();
	if (!self) {
		self = *bot.game->getPlayers().begin();
	}
	if (!self) xcept("self is null");

	my_player = get_player(self);
	neutral_player = get_player(bot.game->neutral());
	opponent_player = neutral_player;
	for (auto&v : bot.game->getPlayers()) {
		auto*p = get_player(v);
		if (p->is_enemy) opponent_player = p;
	}

	bot.multitasking.spawn(std::bind(&players_module::update_players_task, this), "update players");
}
