

namespace strategy {
;

struct strat_exception : std::exception {};
struct strat_failed : strat_exception {};

a_vector<xy> possible_start_locations;

void update_possible_start_locations() {
	possible_start_locations.clear();
	for (auto&p : start_locations) {
		if (p == my_start_location) continue;
		if (grid::get_build_square(p).last_seen > 30 && !enemy_buildings.empty()) {
			double nearest_building = get_best_score_value(enemy_buildings, [&](unit*e) {
				return diag_distance(e->pos - p);
			});
			if (nearest_building < 15 * 30) {
				possible_start_locations.clear();
				possible_start_locations.push_back(p);
				break;
			}
		}
		possible_start_locations.push_back(p);
	}

	for (auto&p : possible_start_locations) {
		if (p == my_start_location) continue;
		auto*s = get_best_score_p(resource_spots::spots, [&](const resource_spots::spot*s) {
			return diag_distance(p - s->pos);
		});
		if (s) {
			bool found = test_pred(s->resources, [](resource_spots::resource_t&r) {
				return r.u->resources <= r.u->initial_resources - 8 * 4;
			});
			auto&bs = grid::get_build_square(s->cc_build_pos);
			if (bs.building && bs.building->owner == players::opponent_player) found = true;
			if (found) {
				xy pos = p;
				possible_start_locations.clear();
				possible_start_locations.push_back(pos);
				break;
			}
		}
	}
	if (possible_start_locations.size() > 1) {
		for (unit*u : enemy_buildings) {
			if (u->type->is_resource_depot) {
				xy pos = get_best_score(possible_start_locations, [&](xy pos) {
					return diag_distance(pos - u->pos);
				});
				possible_start_locations.clear();
				possible_start_locations.push_back(pos);
				break;
			}
		}
	}
}

void execute_build(bool expand, const std::function<bool(buildpred::state&)>&func) {
	using namespace  buildpred;
	auto initial_state = get_my_current_state();
	ruleset rules;
	rules.bases = initial_state.bases.size() + (expand ? 1 : 0);
	rules.end_frame = current_frame + 15 * 60 * 4;
	rules.func = func;

	a_vector<state> all_states;
	all_states.push_back(initial_state);
	run(all_states, rules, true);
	add_builds(all_states.back());
}
void execute_build(const std::function<bool(buildpred::state&)>&func) {
	execute_build(false, func);
}


template<typename T>
struct render_helper {
	template<typename T>
	static std::true_type test(decltype(&T::render)*);
	template<typename T>
	static std::false_type test(...);
	static const bool has_render = decltype(test<T>(0))::value;
	render::render_func_handle h;
	template<bool b = has_render, typename std::enable_if<b>::type* = 0>
	render_helper(T&strat) {
		h = render::add(std::bind(&T::render, &strat));
	}
	template<bool b = has_render, typename std::enable_if<!b>::type* = 0>
	render_helper(T&strat) {}
	template<bool b = has_render, typename std::enable_if<b>::type* = 0>
	void unregister() {
		render::rm(h);
	}
	template<bool b = has_render, typename std::enable_if<!b>::type* = 0>
	void unregister() {}
	~render_helper() {
		unregister();
	}
};

template<typename strat_T>
std::function<void()> wrap() {
	return []() {
		strat_T strat;
		render_helper<strat_T> rh(strat);
		strat.run();
	};
}

a_list<std::function<void(bool)>> on_end_funcs;
struct register_on_end_func {
	a_list<std::function<void(bool)>>::iterator it;
	register_on_end_func(std::function<void(bool)>&&func) {
		on_end_funcs.push_back(std::move(func));
		it = --on_end_funcs.end();
	}
	~register_on_end_func() {
		on_end_funcs.erase(it);
	}
};

json_value read_json(a_string filename) {
	FILE*f = fopen(filename.c_str(), "rb");
	if (!f) {
		a_string str = format(" !! error - failed to open %s for reading", filename);
		log("%s\n", str.c_str());
		send_text(str);
	} else {
		a_string buf;
		fseek(f, 0, SEEK_END);
		buf.resize(ftell(f));
		fseek(f, 0, SEEK_SET);
		fread((void*)buf.data(), buf.size(), 1, f);
		fclose(f);
		return json_parse(buf);
	}
	return json_value();
}

void write_json(a_string filename, const json_value&val) {
	FILE*f = fopen(filename.c_str(), "wb");
	if (!f) {
		a_string str = format(" !! error - failed to open %s for writing\n", filename);
		log("%s\n", str);
		send_text(str);
	} else {
		a_string data = val.dump();
		fwrite(data.data(), data.size(), 1, f);
		fclose(f);
	}
}

a_string current_running_strat;
a_string next_strat;

#include "adapt.h"

std::function<void()> check_transition_func;

bool should_transition() {
	if (current_frame < 15 * 60 * 2) return false;
	if (check_transition_func) check_transition_func();
	return next_strat != current_running_strat;
}

#include "strategy_util.h"

#include "strat_proxy_rax.h"
#include "strat_wraith.h"
#include "strat_tvt_opening.h"
#include "strat_tvt.h"
#include "strat_tvp_opening.h"
#include "strat_tvp.h"
#include "strat_tvz_opening.h"
#include "strat_tvz.h"

#include "strat_zvtp_opening.h"
#include "strat_zvtp.h"
#include "strat_zvz_opening.h"
#include "strat_zvz.h"

#include "strat_p_opening.h"

#include "strat_ums.h"

#include "strat_t_base.h"
#include "strat_t_14cc.h"
#include "strat_t_2fact_vulture.h"
#include "strat_t_siege_expand.h"
#include "strat_t_bio_tank.h"
#include "strat_t_mech.h"
#include "strat_t_wraith_rush.h"
#include "strat_t_proxy_tank.h"
#include "strat_t_air.h"
#include "strat_t_1rax_fe.h"
#include "strat_t_8rax_fe.h"

#include "strat_t_test.h"

#include "strat_z_base.h"
#include "strat_z_5pool.h"
#include "strat_z_9pool.h"
#include "strat_z_10hatch.h"
#include "strat_z_12hatch.h"
#include "strat_z_1hatch_lurker.h"
#include "strat_z_13pool_muta.h"
#include "strat_z_3hatch_before_pool.h"
#include "strat_z_9pool_speed_into_1hatch_spire.h"
#include "strat_z_10hatch_ling.h"
#include "strat_z_fast_mass_expand.h"
#include "strat_z_2hatch_muta.h"

#include "strat_z_hydra_lurker.h"
#include "strat_z_hydra_into_muta.h"
#include "strat_z_vp_hydra.h"
#include "strat_z_3hatch_spire.h"
#include "strat_z_ling_defiler.h"
#include "strat_z_queen.h"
#include "strat_z_econ.h"
#include "strat_z_econ2.h"
#include "strat_z_1hatch_spire.h"
#include "strat_z_tech.h"

#include "strat_z_lategame.h"
#include "strat_z_lategame2.h"

#include "strat_z_ums.h"

#include "strat_p_base.h"
#include "strat_p_1gate_core.h"

a_map<a_string, std::function<void()>> strat_map = {
	{ "proxy rax", wrap<proxy_rax>() },
	{ "wraith", wrap<wraith>() },
	{ "tvt opening", wrap<strat_tvt_opening>() },
	{ "tvt", wrap<strat_tvt>() },
	{ "tvp opening", wrap<strat_tvp_opening>() },
	{ "tvp", wrap<strat_tvp>() },
	{ "tvz opening", wrap<strat_tvz_opening>() },
	{ "tvz", wrap<strat_tvz>() },
	{ "zvtp opening", wrap<strat_zvtp_opening>() },
	{ "zvtp", wrap<strat_zvtp>() },
	{ "zvz opening", wrap<strat_zvz_opening>() },
	{ "zvz", wrap<strat_zvz>() },
	{ "p opening", wrap<strat_p_opening>() },
	{ "ums", wrap<strat_ums>() },

	{ "t 14cc", wrap<strat_t_14cc>() },
	{ "t 2fact vulture", wrap<strat_t_2fact_vulture>() },
	{ "t siege expand", wrap<strat_t_siege_expand>() },
	{ "t wraith rush", wrap<strat_t_wraith_rush>() },
	{ "t proxy tank", wrap<strat_t_proxy_tank>() },
	{ "t 1rax fe", wrap<strat_t_1rax_fe>() },
	{ "t 8rax fe", wrap<strat_t_8rax_fe>() },

	{ "t test", wrap<strat_t_test>() },

	{ "t bio tank", wrap<strat_t_bio_tank>() },
	{ "t mech", wrap<strat_t_mech>() },
	{ "t air", wrap<strat_t_air>() },

	{ "z 5pool", wrap<strat_z_5pool>() },
	{ "z 9pool", wrap<strat_z_9pool>() },
	{ "z 10hatch", wrap<strat_z_10hatch>() },
	{ "z 12hatch", wrap<strat_z_12hatch>() },
	{ "z 1hatch lurker", wrap<strat_z_1hatch_lurker>() },
	{ "z 13pool muta", wrap<strat_z_13pool_muta>() },
	{ "z 3hatch before pool", wrap<strat_z_3hatch_before_pool>() },
	{ "z 9pool speed into 1hatch spire", wrap<strat_z_9pool_speed_into_1hatch_spire>() },
	{ "z 10hatch ling", wrap<strat_z_10hatch_ling>() },
	{ "z fast mass expand", wrap<strat_z_fast_mass_expand>() },
	{ "z 2hatch muta", wrap<strat_z_2hatch_muta>() },

	{ "z 3hatch spire", wrap<strat_z_3hatch_spire>() },
	{ "z ling defiler", wrap<strat_z_ling_defiler>() },
	{ "z queen", wrap<strat_z_queen>() },
	{ "z econ", wrap<strat_z_econ>() },
	{ "z econ2", wrap<strat_z_econ2>() },
	{ "z 1hatch spire", wrap<strat_z_1hatch_spire>() },
	{ "z hydra lurker", wrap<strat_z_hydra_lurker>() },
	{ "z hydra into muta", wrap<strat_z_hydra_into_muta>() },
	{ "z vp hydra", wrap<strat_z_vp_hydra>() },
	{ "z tech", wrap<strat_z_tech>() },

	{ "z lategame", wrap<strat_z_lategame>() },
	{ "z lategame2", wrap<strat_z_lategame2>() },

	{ "z ums", wrap<strat_z_ums>() },

	{ "p 1gate core", wrap<strat_p_1gate_core>() },

};

bool run_strat(const a_string& name) {
	try {
		//log("running strat %s\n", name);
		log(log_level_info, "running strat %s\n", name);
		current_running_strat = name;
		next_strat = name;
		auto i = strat_map.find(name);
		if (i == strat_map.end()) {
			a_string str = format(" !! error - no such strategy '%s'", name);
			log(log_level_info, "%s\n", str);
			send_text(str);
			return false;
		}
		i->second();
		log("strat %s done\n", name);
		return true;
	} catch (strat_failed&) {
		log("strat %s failed\n", name);
		return false;
	}
}

bool run_strat(const a_string& name, std::function<void()> check_transition_func_arg) {
	check_transition_func = check_transition_func_arg;
	bool r = run_strat(name);
	check_transition_func = std::function<void()>();
	return r;
}

void strategy_task() {

	multitasking::sleep(1);

	if (players::my_player->race == race_terran) {

		if (players::opponent_player->random) {
			//run_strat("tvp opening");
		} else if (players::opponent_player->race == race_terran) {

			a_string opening = adapt::choose("tvt opening", "t 14cc", "t 2fact vulture", "t siege expand", "t wraith rush", "t proxy tank", "t 1rax fe", "proxy rax", "t 8rax fe");
			run_strat(opening);
			if (opening == "tvt opening") run_strat("tvt");
			a_string midlategame = adapt::choose("t bio tank", "t mech", "t air");
			run_strat(midlategame);

		} else if (players::opponent_player->race == race_protoss) {

			a_string opening = adapt::choose("tvp opening", "t 14cc", "t 2fact vulture", "t siege expand", "t wraith rush", "t proxy tank", "t 1rax fe", "proxy rax", "t 8rax fe");
			run_strat(opening);
			if (opening == "tvp opening") run_strat("tvp");
			a_string midlategame = adapt::choose("t bio tank", "t mech", "t air");
			run_strat(midlategame);

		} else if (players::opponent_player->race == race_zerg) {

			a_string opening = adapt::choose("tvz opening", "t 14cc", "t 2fact vulture", "t siege expand", "t wraith rush", "t proxy tank", "t 1rax fe", "proxy rax", "t 8rax fe");
			run_strat(opening);
			if (opening == "tvz opening") run_strat("tvz");
			a_string midlategame = adapt::choose("t bio tank", "t mech", "t air");
			run_strat(midlategame);

		}

		if (players::opponent_player->race == race_terran) {
			run_strat("tvt");
		} else if (players::opponent_player->race == race_protoss) {
			run_strat("tvp");
		} else if (players::opponent_player->race == race_zerg) {
			run_strat("tvz");
		}

	} else if (players::my_player->race == race_protoss) {
		
		if (players::opponent_player->race == race_terran) {
			
		} else if (players::opponent_player->race == race_protoss) {
			run_strat("p 1gate core");
		} else if (players::opponent_player->race == race_zerg) {
			
		}

	} else if (players::my_player->race == race_zerg) {
		if (game->getGameType() == BWAPI::GameTypes::Use_Map_Settings) run_strat("z ums");
		//run_strat("ums");

		if (players::opponent_player->race == race_terran) {

			a_string opening = adapt::choose("z 5pool", "z 9pool", "z 10hatch", "z 10hatch ling", "z fast mass expand", "z 13pool muta", "z 2hatch muta");
			run_strat(opening);
			if (current_used_total_supply < 80) {
				a_string midgame = adapt::choose("z vp hydra", "z econ2", "z 3hatch spire", "z queen", "z econ");
				run_strat(midgame);
			}
			a_string lategame = adapt::choose("z lategame", "z lategame2");
			run_strat(lategame);

		} else if (players::opponent_player->race == race_protoss) {

			a_string opening = adapt::choose("z 5pool", "z 9pool", "z 10hatch", "z 10hatch ling", "z fast mass expand", "z 13pool muta", "z 2hatch muta");
			run_strat(opening);
			if (current_used_total_supply < 80) {
				a_string midgame = adapt::choose("z vp hydra", "z econ2", "z hydra into muta");
				run_strat(midgame);
			}
			a_string lategame = adapt::choose("z lategame", "z lategame2");
			run_strat(lategame);

		} else if (players::opponent_player->race == race_zerg) {

			a_string opening = adapt::choose("zvz opening", "z 5pool", "z 9pool", "z 10hatch", "z 10hatch ling", "z 9pool speed into 1hatch spire", "z 2hatch muta", "z 9pool -> 10hatch ling");
			if (opening == "z 9pool -> 10hatch ling") {
				run_strat("z 9pool");
				run_strat("z 10hatch ling");
			} else {
				run_strat(opening);
			}
			if (opening == "zvz opening") run_strat("zvz");
			if (current_used_total_supply < 80) {
				if (opening == "z 5pool" || opening == "z 9pool" || opening=="z 10hatch" || opening == "z 10hatch ling") {
					a_string midgame = adapt::choose("z vp hydra", "z 9pool speed into 1hatch spire (midgame)", "z 2hatch muta (midgame)");
					if (midgame == "z 9pool speed into 1hatch spire (midgame)") midgame = "z 9pool speed into 1hatch spire";
					if (midgame == "z 2hatch muta (midgame)") midgame = "z 2hatch muta";
					run_strat(midgame);
				} else {
					a_string midgame = adapt::choose("z vp hydra", "z lategame", "z lategame2");
					run_strat(midgame);
				}
			}
			a_string lategame = adapt::choose("z lategame", "z lategame2");
			run_strat(lategame);

		}

		if (players::opponent_player->race == race_terran) {
			run_strat("zvtp opening");
			run_strat("zvtp");
		} else if (players::opponent_player->race == race_protoss) {
			run_strat("zvtp opening");
			run_strat("zvtp");
		} else if (players::opponent_player->race == race_zerg) {
			run_strat("zvz opening");
			run_strat("zvz");
		}
	}

	while (true) {

		using namespace buildpred;

		multitasking::sleep(15 * 10);
	}

}

bool group_adapt_by_opponent_name = true;
bool group_adapt_by_race = true;
a_string adapt_read_filename = "bwapi-data/read/tsc-bwai-adapt.txt";
a_string adapt_write_filename = "bwapi-data/write/tsc-bwai-adapt.txt";

a_string opponent_name;
a_string opponent_race;


void init() {

	multitasking::spawn(strategy_task, "strategy");

	int enemies = 0;
	int enemy_race = race_unknown;
	for (auto&v : game->getPlayers()) {
		auto*p = players::get_player(v);
		if (p->is_enemy) {
			opponent_name = p->game_player->getName().c_str();
			if (!p->random) enemy_race = p->race;
			++enemies;
		}
	}

	if (group_adapt_by_opponent_name) {
		if (enemies == 0) opponent_name = "<none>";
		if (enemies > 1) opponent_name = "<multiple>";
	} else {
		opponent_name = "<any>";
	}
	if (enemy_race == race_terran) opponent_race = "terran";
	else if (enemy_race == race_protoss) opponent_race = "protoss";
	else if (enemy_race == race_zerg) opponent_race = "zerg";
	else opponent_race = "unknown";

	auto dat = read_json(adapt_read_filename);

	adapt::weights["z lategame2"] = 2.5;
	adapt::weights["z lategame"] = 1.0;
	if (enemy_race == race_terran) {
		adapt::weights["z fast mass expand"] = 2.0;
		adapt::weights["z 10hatch"] = 3.0;
		adapt::weights["z 10hatch ling"] = 2.0;

		adapt::weights["z econ2"] = 5.0;
		adapt::weights["z 3hatch spire"] = 2.5;
	} else if (enemy_race == race_protoss) {
		adapt::weights["z fast mass expand"] = 2.0;
		adapt::weights["z 10hatch"] = 3.0;
		adapt::weights["z 10hatch ling"] = 2.0;

		adapt::weights["z vp hydra"] = 5.0;
	} else if (enemy_race == race_zerg) {
		adapt::weights["z 10hatch ling"] = 3.0;
		adapt::weights["z 2hatch muta (midgame)"] = 2.5;
		adapt::weights["z 9pool -> 10hatch ling"] = 3.0;
	}

	adapt::weights["t bio tank"] = 2.0;
	adapt::weights["t mech"] = 2.0;
	if (enemy_race == race_terran) {
		adapt::weights["t 14cc"] = 3.0;
		adapt::weights["t 2fact vulture"] = 3.0;
		adapt::weights["t proxy tank"] = 2.0;
	} else if (enemy_race == race_protoss) {
		adapt::weights["t siege expand"] = 3.0;
		adapt::weights["tvp opening"] = 2.0;
		adapt::weights["t wraith rush"] = 1.5;
	} else if (enemy_race == race_zerg) {
		adapt::weights["t 8rax fe"] = 3.0;
		adapt::weights["tvz opening"] = 2.0;
		adapt::weights["z 2fact vulture"] = 2.0;
	}

	auto& a = dat["adapt"][opponent_name][opponent_race];
	auto& hist = a["history"];
	int wins = 0;
	int losses = 0;
	int total_games = 0;
	a_unordered_map<a_string, int> choice_wins;
	a_unordered_map<a_string, int> choice_losses;
	for (auto&v : hist.vector) {
		bool won = v["result"] == "won";
		++total_games;
		if (won) ++wins;
		else ++losses;
		for (a_string choice : v["choices"].vector) {
			double& w = adapt::weights[choice];
			if (w == 0.0) w = 1.0;
			if (won) w += 1.0;
			else if (w > 5.0) w *= 0.8;
			else if (w <= 2.0) w *= 0.5;
			else w -= 1.0;
			if (!won) {
				if (choice == "z 5pool") w *= 0.5;
				if (choice == "z 10hatch ling") w *= 0.5;
				if (choice == "z fast mass expand") w *= 0.5;
			}
			log("%s %s - w adjusted to %g\n", choice, won ? "won" : "lost", w);

			if (won) ++choice_wins[choice];
			else ++choice_losses[choice];
		}
	}
	for (auto&v : choice_wins) {
		log("%s: %d wins\n", v.first, v.second);
	}
	for (auto&v : choice_losses) {
		log("%s: %d losses\n", v.first, v.second);
	}
	for (auto&v : choice_wins) {
		double winrate = (double)v.second / (v.second + choice_losses[v.first]);
		if (winrate >= 0.9) adapt::weights[v.first] += 50.0 + (winrate - 0.9) * 500.0;
	}
	if (losses >= 8) {
		for (auto&v : adapt::weights) {
			v.second = std::pow(1.0 + v.second, 1.5) - 1.0;
			if (v.second < 0.0125) v.second = 0.0125;
		}
	} else {
		for (auto&v : adapt::weights) {
			if (choice_wins[v.first] + choice_losses[v.first]) v.second /= 4;
		}
	}
	a_string str = format("adapt %s %s - wins: %d  losses: %d  winrate: %.02f", opponent_name, opponent_race, wins, losses, (double)wins / total_games * 100);
	log("%s\n", str);
	send_text(str);
	log("weights loaded - \n");
	for (auto&v : adapt::weights) {
		log(" %s - %g\n", v.first, v.second);
	}

}

void on_end(bool won) {
	for (auto&f : on_end_funcs) {
		f(won);
	}

	auto dat = read_json(adapt_read_filename);

	auto& a = dat["adapt"][opponent_name][opponent_race];
	auto& hist = a["history"];
	auto&e = hist[hist.vector.size()];
	e["result"] = won ? "won" : "lost";
	json_value choices;
	choices.type = json_value::t_array;
	for (size_t i = 0; i < adapt::all_choices.size(); ++i) {
		choices[i] = adapt::all_choices[i];
	}
	e["choices"] = choices;

	write_json(adapt_write_filename, dat);
	
}

}

