

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

struct strat_choice_t {
	a_string name;
	double my_lost_minerals;
	double my_lost_gas;
	double op_lost_minerals;
	double op_lost_gas;
	int end_frame;
	int dead_frames;
};
a_vector<strat_choice_t> strat_choices;
bool strat_choice_active = false;
double strat_choice_begin_my_lost_minerals;
double strat_choice_begin_my_lost_gas;
double strat_choice_begin_op_lost_minerals;
double strat_choice_begin_op_lost_gas;

int strat_choice_dead_frames;

void end_strat_choice() {
	if (!strat_choice_active || strat_choices.empty()) return;
	strat_choice_active = false;
	auto&e = strat_choices.back();
	e.my_lost_minerals = players::my_player->minerals_lost - strat_choice_begin_my_lost_minerals;
	e.my_lost_gas = players::my_player->gas_lost - strat_choice_begin_my_lost_gas;
	e.op_lost_minerals = players::opponent_player->minerals_lost - strat_choice_begin_op_lost_minerals;
	e.op_lost_gas = players::opponent_player->gas_lost - strat_choice_begin_op_lost_gas;
	e.end_frame = current_frame;
	e.dead_frames = strat_choice_dead_frames;
}

void set_strat_choice(const a_string& name) {
	if (strat_choice_active) end_strat_choice();
	strat_choice_t t;
	t.name = name;
	strat_choices.push_back(std::move(t));
	strat_choice_begin_my_lost_minerals = players::my_player->minerals_lost;
	strat_choice_begin_my_lost_gas = players::my_player->gas_lost;
	strat_choice_begin_op_lost_minerals = players::opponent_player->minerals_lost;
	strat_choice_begin_op_lost_gas = players::opponent_player->gas_lost;
	strat_choice_dead_frames = 0;
	strat_choice_active = true;
}

enum {
	tag_pool_first = 1,
	tag_hatch_first = 2,
	tag_aggressive = 4,
	tag_very_aggressive = 8,
	tag_safe = 0x10,
	tag_greedy = 0x20,
	tag_low_econ = 0x40,
	tag_high_econ = 0x80,
	tag_defensive = 0x100,
	tag_opening = 0x200,
	tag_midgame = 0x400,
	tag_lategame = 0x800
};

a_map<int, a_string> tag_names = {
	{ tag_pool_first, "pool first" },
	{ tag_hatch_first, "hatch first" },
	{ tag_aggressive, "aggressive" },
	{ tag_very_aggressive, "very aggressive" },
	{ tag_safe, "safe" },
	{ tag_greedy, "greedy" },
	{ tag_low_econ, "low econ" },
	{ tag_high_econ, "high econ" },
	{ tag_defensive, "defensive" },
	{ tag_opening, "opening" },
	{ tag_midgame, "midgame" },
	{ tag_lategame, "lategame" }
};

struct choice_t {
	a_string name;
	int tags = 0;
//	std::function<void()> execute;
	choice_t(a_string name, int tags) : name(name), tags(tags) {}
};

a_vector<choice_t> choices = {
	{ "zvz opening", tag_opening | tag_pool_first | tag_aggressive },
	{ "z 5pool", tag_opening | tag_pool_first | tag_very_aggressive | tag_low_econ },
	{ "z 9pool", tag_opening | tag_pool_first | tag_safe },
	{ "z 10hatch", tag_opening | tag_hatch_first },
	{ "z 9pool speed into 1hatch spire", tag_opening | tag_pool_first | tag_aggressive | tag_low_econ },
	{ "z 2hatch muta", tag_opening | tag_safe | tag_high_econ },
	{ "z 9pool -> 10hatch ling", tag_opening | tag_pool_first | tag_very_aggressive | tag_low_econ },
	{ "z 10hatch ling", tag_opening | tag_hatch_first | tag_very_aggressive | tag_low_econ },
	{ "z fast mass expand", tag_opening | tag_hatch_first | tag_greedy | tag_high_econ},
	{ "z 13pool muta", tag_opening | tag_aggressive },

	{ "z vp hydra", tag_midgame | tag_safe | tag_high_econ },
	{ "z 9pool speed into 1hatch spire (midgame)", tag_midgame | tag_aggressive | tag_low_econ },
	{ "z 2hatch muta (midgame)", tag_midgame | tag_high_econ },
	{ "z econ2", tag_midgame | tag_safe | tag_defensive },
	{ "z hydra into muta", tag_midgame | tag_safe | tag_high_econ },
	{ "z 3hatch spire", tag_midgame | tag_safe},
	{ "z queen", tag_midgame | tag_greedy },
	{ "z econ", tag_midgame | tag_high_econ },

	{ "z lategame", tag_lategame },
	{ "z lategame2", tag_lategame },
};

void validate_strat_name() {
}
template<typename A, typename...Ax>
void validate_strat_name(A&&a, Ax&&...ax) {
	for (auto&v : choices) {
		if (v.name == a) {
			validate_strat_name(std::forward<Ax>(ax)...);
			return;
		}
	}
	xcept("no such choice: %s\n", a);
}

template<typename A>
choice_t find_choice(A&&name) {
	for (auto&v : choices) {
		if (v.name == name) {
			return v;
		}
	}
	return { "", 0 };
}

template<typename...args_T>
a_string choose_strat(args_T&&...args) {
	validate_strat_name(args...);
	return adapt::choose(std::forward<args_T>(args)...);
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

			a_string main = choose_strat("z 5pool", "z 10hatch ling", "z 13pool muta", "z 2hatch muta", "z vp hydra", "z econ2", "z 3hatch spire", "z econ");

			if (find_choice(main).tags & tag_midgame) {
				a_string opening = choose_strat("z 9pool", "z 10hatch");
				set_strat_choice(opening);
				run_strat(opening);
				set_strat_choice(main);
				run_strat(main);
			} else {
				set_strat_choice(main);
				run_strat(main);
				if (current_used_total_supply < 80) {
					a_string midgame = choose_strat("z vp hydra", "z econ2", "z 3hatch spire", "z econ");
					set_strat_choice(midgame);
					run_strat(midgame);
				}
			}

			a_string lategame = adapt::choose("z lategame", "z lategame2");
			set_strat_choice(lategame);
			run_strat(lategame);

		} else if (players::opponent_player->race == race_protoss) {

			a_string main = choose_strat("z 5pool", "z 10hatch ling", "z 13pool muta", "z 2hatch muta", "z vp hydra", "z econ2", "z hydra into muta");

			if (find_choice(main).tags & tag_midgame) {
				a_string opening = choose_strat("z 9pool", "z 10hatch");
				set_strat_choice(opening);
				run_strat(opening);
				set_strat_choice(main);
				run_strat(main);
			} else {
				set_strat_choice(main);
				run_strat(main);
				if (current_used_total_supply < 80) {
					a_string midgame = choose_strat("z vp hydra", "z econ2", "z hydra into muta");
					set_strat_choice(midgame);
					run_strat(midgame);
				}
			}

			a_string lategame = adapt::choose("z lategame", "z lategame2");
			set_strat_choice(lategame);
			run_strat(lategame);

		} else if (players::opponent_player->race == race_zerg) {

			a_string main = choose_strat("zvz opening", "z 5pool", "z 9pool -> 10hatch ling", "z 9pool speed into 1hatch spire", "z 2hatch muta", "z vp hydra", "z econ2");

			if (main == "zvz opening") {
				set_strat_choice(main);
				run_strat(main);
				run_strat("zvz");
			}

			if (find_choice(main).tags & tag_midgame) {
				a_string opening = choose_strat("z 9pool", "z 10hatch");
				set_strat_choice(opening);
				run_strat(opening);
				set_strat_choice(main);
				run_strat(main);
			} else {
				set_strat_choice(main);
				if (main == "z 9pool -> 10hatch ling") {
					run_strat("z 9pool");
					run_strat("z 10hatch ling");
				} else run_strat(main);
				if (current_used_total_supply < 80) {
					a_string midgame = choose_strat("z vp hydra", "z 9pool speed into 1hatch spire (midgame)", "z 2hatch muta (midgame)", "z econ2", "z hydra into muta");
					set_strat_choice(midgame);
					if (midgame == "z 9pool speed into 1hatch spire (midgame)") midgame = "z 9pool speed into 1hatch spire";
					if (midgame == "z 2hatch muta (midgame)") midgame = "z 2hatch muta";
					run_strat(midgame);
				}
			}

			a_string lategame = adapt::choose("z lategame", "z lategame2");
			set_strat_choice(lategame);
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

void strategy_count_dead_frames_task() {
	
	while (true) {
		int f = current_frame;
		bool dead = current_minerals_per_frame < 0.125;
		multitasking::sleep(10);
		if (dead) strat_choice_dead_frames += current_frame - f;
	}

}

bool group_adapt_by_opponent_name = true;
bool group_adapt_by_race = true;
a_string adapt_read_filename = "bwapi-data/read/tsc-bwai-adapt-";
a_string adapt_write_filename = "bwapi-data/write/tsc-bwai-adapt-";
bool adapt_filename_append_opponent_name = true;

a_string opponent_name;
a_string opponent_race;

a_string escape_filename(const a_string&str) {
	a_string r;
	const char*c = str.c_str();
	const char*lc = c;
	auto flush = [&]() {
		r.append(lc, c - lc);
		lc = c;
	};
	while (*c) {
		if (*c == '-' || *c == '.' || (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9')) ++c;
		else {
			flush();
			++lc;
			char buf[3];
			buf[0] = '_';
			buf[1] = *c >> 4 & 0xf;
			if (buf[1] < 10) buf[1] += '0';
			else buf[1] += -10 + 'a';
			buf[2] = *c & 0xf;
			if (buf[2] < 10) buf[2] += '0';
			else buf[2] += -10 + 'a';
			r.append(buf, 3);
			++c;
		}
	}
	flush();
	return r;
}

a_string get_read_filename() {
	if (adapt_filename_append_opponent_name) return adapt_read_filename + escape_filename(opponent_name) + ".txt";
	return adapt_read_filename;
}
a_string get_write_filename() {
	if (adapt_filename_append_opponent_name) return adapt_write_filename + escape_filename(opponent_name) + ".txt";
	return adapt_write_filename;
}

void init() {

	multitasking::spawn(strategy_task, "strategy");
	multitasking::spawn(strategy_count_dead_frames_task, "strategy count dead frames");

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

	auto dat = read_json(get_read_filename());

	adapt::weights["z lategame2"] = 2.5;
	adapt::weights["z lategame"] = 1.0;
// 	if (enemy_race == race_terran) {
// 		adapt::weights["z fast mass expand"] = 2.0;
// 		adapt::weights["z 10hatch"] = 3.0;
// 		adapt::weights["z 10hatch ling"] = 2.0;
// 
// 		adapt::weights["z econ2"] = 5.0;
// 		adapt::weights["z 3hatch spire"] = 2.5;
// 	} else if (enemy_race == race_protoss) {
// 		adapt::weights["z fast mass expand"] = 2.0;
// 		adapt::weights["z 10hatch"] = 3.0;
// 		adapt::weights["z 10hatch ling"] = 2.0;
// 
// 		adapt::weights["z vp hydra"] = 5.0;
// 	} else if (enemy_race == race_zerg) {
// 		adapt::weights["z 10hatch ling"] = 3.0;
// 		adapt::weights["z 2hatch muta (midgame)"] = 2.5;
// 		adapt::weights["z 9pool -> 10hatch ling"] = 3.0;
// 	}

// 	adapt::weights["t bio tank"] = 2.0;
// 	adapt::weights["t mech"] = 2.0;
// 	if (enemy_race == race_terran) {
// 		adapt::weights["t 14cc"] = 3.0;
// 		adapt::weights["t 2fact vulture"] = 3.0;
// 		adapt::weights["t proxy tank"] = 2.0;
// 	} else if (enemy_race == race_protoss) {
// 		adapt::weights["t siege expand"] = 3.0;
// 		adapt::weights["tvp opening"] = 2.0;
// 		adapt::weights["t wraith rush"] = 1.5;
// 	} else if (enemy_race == race_zerg) {
// 		adapt::weights["t 8rax fe"] = 3.0;
// 		adapt::weights["tvz opening"] = 2.0;
// 		adapt::weights["z 2fact vulture"] = 2.0;
// 	}

	for (auto&ch : choices) {
		double& w = adapt::weights[ch.name];
		if (w == 0.0) w = 1.0;
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
		a_vector<a_string> cur_choices;
// 		if (v["strat choices"]) {
// 			for (auto&v : v["strat choices"].vector) {
// 				cur_choices.push_back(v["name"]);
// 			}
// 		} else {
			for (a_string choice : v["choices"].vector) {
				cur_choices.push_back(choice);
			}
//		}
		if (true) {
			a_vector<a_string> prev_choices;
			a_vector<a_string> combination_choices;
			for (auto&v : cur_choices) {
				if (!prev_choices.empty()) {
					a_string seq;
					for (auto&v : prev_choices) {
						seq += ".";
						seq += v;
					}
					seq += ".";
					seq += v;
					combination_choices.push_back(seq);
				}
				prev_choices.push_back(v);
			}
			for (auto&v : combination_choices) cur_choices.push_back(std::move(v));
		}
		int n = 0;
		for (a_string choice : cur_choices) {
			++n;
			double& w = adapt::weights[choice];
			if (w == 0.0) w = 1.0;
			if (n == 1 && (choice.empty() || choice[0] != '.')) {
				if (won) w += 1.0;
				else if (w > 5.0) w *= 0.8;
				else if (w <= 2.0) w *= 0.5;
				else w -= 1.0;
			} else {
				if (won) w += 0.15;
				else if (w > 1.0) w /= 1.25;
			}
			log("%s %s - w adjusted to %g\n", choice, won ? "won" : "lost", w);

			if (won) ++choice_wins[choice];
			else ++choice_losses[choice];
		}

		if (!won) {
			for (auto&v : adapt::weights) {
				double& w = std::get<1>(v);
				if (std::get<0>(v).empty() || std::get<0>(v)[0] != '.') {
					double adj = (2.5 - w) / 20.0;
					w += adj;
				} else {
					double adj = (1.0 - w) / 20.0;
					w += adj;
				}
			}
		}
	}
	a_map<a_string, int> rewards;
	auto reward_tags = [&](int tags) {
		log(log_level_info, "reward tags - ");
		for (int i = 0; i < 32; ++i) {
			if (tags&(1 << i)) log(log_level_info, "%s ", tag_names[1 << i]);
		}
		log(log_level_info, "\n");
		int gamestage_tags = tags & (tag_opening | tag_midgame | tag_lategame);
		tags &= ~(tag_opening | tag_midgame | tag_lategame);
		for (auto&v : choices) {
			if (v.tags & gamestage_tags && v.tags & tags) rewards[v.name] += tsc::bit_popcount(v.tags & tags);
		}
	};
	auto reward_if_missing_tags = [&](int tags) {
		log(log_level_info, "reward if missing tags - ");
		for (int i = 0; i < 32; ++i) {
			if (tags&(1 << i)) log(log_level_info, "%s ", tag_names[1 << i]);
		}
		log(log_level_info, "\n");
		int gamestage_tags = tags & (tag_opening | tag_midgame | tag_lategame);
		tags &= ~(tag_opening | tag_midgame | tag_lategame);
		for (auto&v : choices) {
			if (v.tags & gamestage_tags && (~v.tags & tags) == tags) ++rewards[v.name];
		}
	};
	for (size_t i = hist.vector.size(); i; --i) {
		auto&hist_v = hist.vector[i - 1];
		bool won = hist_v["result"] == "won";
		int prev_tags = 0;
		int all_tags = 0;
		int frames = 0;
		int last_frame = 0;
		for (auto&v : hist_v["strat choices"].vector) {
			auto ch = find_choice((a_string)v["name"]);
			double my_lost_minerals = v["my lost minerals"];
			double my_lost_gas = v["my lost gas"];
			double op_lost_minerals = v["op lost minerals"];
			double op_lost_gas = v["op lost gas"];
			int end_frame = v["end frame"];
			int dead_frames = v["dead frames"];

			double my_lost = my_lost_minerals + my_lost_gas;
			double op_lost = op_lost_minerals + op_lost_gas;
			int tags = ch.tags;

			all_tags |= tags;

			frames += end_frame - last_frame - dead_frames;
			last_frame = end_frame;

			int gamestage_tags = tags & (tag_opening | tag_midgame | tag_lategame);

			if (~tags & tag_lategame) {
				if (op_lost >= 250 && op_lost > my_lost * 1.5) {
					reward_tags(tags);
					reward_tags(prev_tags);
				}
			}

			if (tags & (tag_aggressive | tag_very_aggressive)) {
				if (my_lost >= op_lost) {
					reward_if_missing_tags(gamestage_tags | tag_aggressive | tag_very_aggressive);
				}
			}

			prev_tags = tags;
		}

		if (!won) {

			if (all_tags & tag_greedy) reward_if_missing_tags(tag_opening | tag_midgame | tag_greedy);

			if (frames < 15 * 60 * 8) {
				if (all_tags & tag_hatch_first) reward_tags(tag_opening | tag_midgame | tag_pool_first);
				if (all_tags & tag_safe) reward_tags(tag_opening | tag_aggressive | tag_very_aggressive);
			}
			if (frames >= 15 * 60 * 20) {
				if (all_tags & tag_low_econ) reward_if_missing_tags(tag_opening | tag_midgame | tag_low_econ);
				if (~all_tags & (tag_low_econ | tag_high_econ)) {
					reward_tags(tag_midgame | tag_high_econ);
				}
			} else {
				if (~all_tags & (tag_safe | tag_defensive)) reward_tags(tag_midgame | tag_safe | tag_defensive);
				else reward_tags(tag_opening | tag_midgame | tag_aggressive | tag_very_aggressive);
				if (all_tags & tag_pool_first) reward_tags(tag_opening | tag_hatch_first);
			}
		}

	}
	for (auto&v : rewards) {
		log(log_level_info, "reward %s x%d\n", v.first, v.second);
	}
	int highest_reward = 0;
	for (auto&v : rewards) {
		if (v.second > highest_reward) highest_reward = v.second;
	}
	for (auto&v : rewards) {
		double& w = adapt::weights[v.first];
		if (w == 0.0) w = 1.0;
		w *= 1.0 + (double)v.second / (double)highest_reward * 3.0;
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
	log(log_level_info, "%s\n", str);
	send_text(str);
	log(log_level_info, "weights loaded - \n");
	for (auto&v : adapt::weights) {
		log(log_level_info, " %s - %g\n", v.first, v.second);
	}

}

void on_end(bool won) {
	for (auto&f : on_end_funcs) {
		f(won);
	}

	end_strat_choice();

	auto dat = read_json(get_read_filename());

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

	json_value jv_strat_choices;
	jv_strat_choices.type = json_value::t_array;
	for (size_t i = 0; i < strat_choices.size(); ++i) {
		auto&e = strat_choices[i];
		json_object jv_e;
		jv_e["name"] = e.name;
		jv_e["my lost minerals"] = e.my_lost_minerals;
		jv_e["my lost gas"] = e.my_lost_gas;
		jv_e["op lost minerals"] = e.op_lost_minerals;
		jv_e["op lost gas"] = e.op_lost_gas;
		jv_e["end frame"] = e.end_frame;
		jv_e["dead frames"] = e.dead_frames;
		jv_strat_choices[i] = std::move(jv_e);
	}
	e["strat choices"] = jv_strat_choices;

	write_json(get_write_filename(), dat);
	
}

}

