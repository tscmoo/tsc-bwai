
struct strat_z_base {

	int my_zergling_count;
	int my_queen_count;
	int my_defiler_count;
	int my_mutalisk_count;

	int my_hatch_count;

	int enemy_terran_unit_count = 0;
	int enemy_protoss_unit_count = 0;
	int enemy_zerg_unit_count = 0;
	int enemy_marine_count = 0;
	int enemy_firebat_count = 0;
	int enemy_ghost_count = 0;
	int enemy_vulture_count = 0;
	int enemy_tank_count = 0;
	int enemy_goliath_count = 0;
	int enemy_wraith_count = 0;
	int enemy_valkyrie_count = 0;
	int enemy_bc_count = 0;
	int enemy_science_vessel_count = 0;
	int enemy_dropship_count = 0;
	int enemy_reaver_count = 0;
	int enemy_shuttle_count = 0;
	int enemy_robotics_facility_count = 0;
	double enemy_army_supply = 0.0;
	double enemy_air_army_supply = 0.0;
	double enemy_ground_army_supply = 0.0;
	double enemy_ground_large_army_supply = 0.0;
	double enemy_ground_small_army_supply = 0.0;
	double enemy_weak_against_ultra_supply = 0.0;
	double enemy_anti_air_army_supply = 0.0;
	int enemy_static_defence_count = 0;
	int enemy_proxy_building_count = 0;
	double enemy_attacking_army_supply = 0.0;
	int enemy_attacking_worker_count = 0;
	int enemy_dt_count = 0;
	int enemy_lurker_count = 0;
	int enemy_arbiter_count = 0;
	int enemy_cannon_count = 0;
	int enemy_bunker_count = 0;
	int enemy_units_that_shoot_up_count = 0;
	int enemy_gas_count = 0;
	int enemy_barracks_count = 0;
	int enemy_gateway_count = 0;
	int enemy_zergling_count = 0;
	int enemy_spire_count = 0;
	int enemy_cloaked_unit_count = 0;
	int enemy_zealot_count = 0;
	int enemy_dragoon_count = 0;
	int enemy_worker_count = 0;
	int enemy_spawning_pool_count = 0;
	int enemy_evolution_chamber_count = 0;

	bool opponent_has_expanded = false;
	bool being_rushed = false;
	bool is_defending = false;

	bool default_upgrades = true;
	bool default_worker_defence = true;
	bool pull_workers_for_ling_zealot_defence = false;

	int min_bases = 2;
	int max_bases = 0;

	int opening_state = 0;
	int gas_trick_state = 0;
	int sleep_time = 15 * 5;

	int attack_interval = 0;

	bool call_build = true;
	bool call_place_static_defence = true;
	bool place_static_defence_only_at_main = false;

	void bo_gas_trick(unit_type*ut = unit_types::drone) {
		if (!bo_all_done()) return;
		if (gas_trick_state == 0) {
			if (current_minerals >= 80) {
				build::add_build_task(100.0, unit_types::extractor);
				build::add_build_task(101.0, ut);
				++gas_trick_state;
			}
		} else {
			bool done = false;
			int gases = 0;
			for (auto&b : build::build_tasks) {
				if (b.type->unit && b.type->unit->is_gas) ++gases;
			}
			if (gases == 0) done = true;
			for (unit*u : my_units_of_type[unit_types::extractor]) {
				if (!u->is_completed) {
					u->game_unit->cancelConstruction();
					for (auto&b : build::build_tasks) {
						if (b.built_unit == u) {
							b.dead = true;
							break;
						}
					}
					done = true;
					break;
				}
			}
			if (done) {
				++opening_state;
				gas_trick_state = 0;
			}
		}
	};

	bool bo_all_done() {
		for (auto&b : build::build_order) {
			if (b.type->unit && !b.built_unit) return false;
		}
		return true;
	}
	void bo_cancel_all() {
		for (auto&b : build::build_order) {
			if (b.type->unit && !b.built_unit) {
				b.dead = true;
			}
		}
	}

	bool can_expand = false;
	bool force_expand = false;

	bool is_attacking = false;

	bool static_defence_pos_is_valid = false;

	virtual void init() = 0;
	virtual bool tick() = 0;

	int drone_count;
	int hatch_count;
	int completed_hatch_count;
	int larva_count;
	int damaged_overlord_count;

	int zergling_count;
	int mutalisk_count;
	int scourge_count;
	int hydralisk_count;
	int lurker_count;
	int ultralisk_count;
	int queen_count;
	int defiler_count;
	int devourer_count;
	int guardian_count;
	int sunken_count;
	int spore_count;

	double army_supply = 0.0;
	double air_army_supply = 0.0;
	double ground_army_supply = 0.0;

	std::function<bool(buildpred::state&)> army;

	virtual bool build(buildpred::state&st) = 0;

	int get_max_mineral_workers() {
		using namespace buildpred;
		int count = 0;
		for (auto&b : get_my_current_state().bases) {
			for (auto&s : b.s->resources) {
				if (!s.u->type->is_minerals) continue;
				count += (int)s.full_income_workers[players::my_player->race];
			}
		}
		return count;
	};

	std::function<refcounted_ptr<resource_spots::spot>()> get_next_base = []() {
		auto st = buildpred::get_my_current_state();
		unit_type*worker_type = unit_types::drone;
		unit_type*cc_type = unit_types::hatchery;
		a_vector<refcounted_ptr<resource_spots::spot>> available_bases;
		for (auto&s : resource_spots::spots) {
			if (grid::get_build_square(s.cc_build_pos).building) continue;
			bool okay = true;
			for (auto&v : st.bases) {
				if ((resource_spots::spot*)v.s == &s) okay = false;
			}
			if (!okay) continue;
			if (!build::build_is_valid(grid::get_build_square(s.cc_build_pos), cc_type)) continue;
			available_bases.push_back(&s);
		}
		return get_best_score(available_bases, [&](resource_spots::spot*s) {
			double score = unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
			double res = 0;
// 			double ned = get_best_score_value(enemy_buildings, [&](unit*e) {
// 				if (e->type->is_worker) return std::numeric_limits<double>::infinity();
// 				return diag_distance(s->pos - e->pos);
// 			});
			unit*ne = get_best_score(enemy_buildings, [&](unit*e) {
				if (e->type->is_worker) return std::numeric_limits<double>::infinity();
				return diag_distance(s->pos - e->pos);
			});
			double ned = ne ? diag_distance(s->pos - ne->pos) : 0.0;
			if (st.bases.size() == 1 && (!ne || (!ne->stats->ground_weapon && ne->type != unit_types::bunker))) ned = 0.0;
			if (ned && ned <= 32 * 30) score += 10000;
			bool has_gas = false;
			for (auto&r : s->resources) {
				res += r.u->resources;
				if (r.u->type->is_gas) has_gas = true;
			}
			score -= (has_gas ? res : res / 4) / 10 + ned;
			return score;
		}, std::numeric_limits<double>::infinity());
	};

	void rm_all_scouts() {
		for (auto&v : scouting::all_scouts) {
			scouting::rm_scout(v.scout_unit);
		}
	}

	bool eval_combat(bool include_static_defence, int min_sunkens) {
		combat_eval::eval eval;
		eval.max_frames = 15 * 120;
		for (unit*u : my_units) {
			if (!u->is_completed) continue;
			if (!include_static_defence && u->building) continue;
			if (!u->stats->ground_weapon) continue;
			if (u->type->is_worker || u->type->is_non_usable) continue;
			auto&c = eval.add_unit(u, 0);
			if (u->building) c.move = 0.0;
		}
		for (unit*u : enemy_units) {
			if (u->building || u->type->is_worker || u->type->is_non_usable) continue;
			if (u->is_flying) continue;
			eval.add_unit(u, 1);
		}
		eval.run();
		a_map<unit_type*, int> my_count, op_count;
		for (auto&v : eval.teams[0].units) ++my_count[v.st->type];
		for (auto&v : eval.teams[1].units) ++op_count[v.st->type];
		log("eval_combat::\n");
		log("my units -");
		for (auto&v : my_count) log(" %dx%s", v.second, short_type_name(v.first));
		log("\n");
		log("op units -");
		for (auto&v : op_count) log(" %dx%s", v.second, short_type_name(v.first));
		log("\n");

		log("result: score %g %g  supply %g %g  damage %g %g  in %d frames\n", eval.teams[0].score, eval.teams[1].score, eval.teams[0].end_supply, eval.teams[1].end_supply, eval.teams[0].damage_dealt, eval.teams[1].damage_dealt, eval.total_frames);
		if (min_sunkens) {
			int low_hp_or_dead_sunkens = 0;
			int total_sunkens = 0;
			for (auto&c : eval.teams[0].units) {
				if (c.st->type == unit_types::sunken_colony) {
					if (c.st->hp <= c.st->hp / 2) ++low_hp_or_dead_sunkens;
					++total_sunkens;
				}
			}
			if (total_sunkens - low_hp_or_dead_sunkens < min_sunkens && total_sunkens >= min_sunkens) return false;
		}
		return eval.teams[1].end_supply == 0;
	};

	void set_build_vars(buildpred::state&st) {
		using namespace buildpred;
		drone_count = count_units_plus_production(st, unit_types::drone);
		hatch_count = count_units_plus_production(st, unit_types::hatchery) + count_units_plus_production(st, unit_types::lair) + count_units_plus_production(st, unit_types::hive);
		completed_hatch_count = st.units[unit_types::hatchery].size() + st.units[unit_types::lair].size() + st.units[unit_types::hive].size();
		larva_count = 0;
		for (int i = 0; i < 3; ++i) {
			for (st_unit&u : st.units[i == 0 ? unit_types::hive : i == 1 ? unit_types::lair : unit_types::hatchery]) {
				larva_count += u.larva_count;
			}
		}
		damaged_overlord_count = 0;
		for (unit*u : my_completed_units_of_type[unit_types::overlord]) {
			if (u->hp < u->stats->hp) ++damaged_overlord_count;
		}

		zergling_count = count_units_plus_production(st, unit_types::zergling);
		mutalisk_count = count_units_plus_production(st, unit_types::mutalisk);
		scourge_count = count_units_plus_production(st, unit_types::scourge);
		hydralisk_count = count_units_plus_production(st, unit_types::hydralisk);
		lurker_count = count_units_plus_production(st, unit_types::lurker);
		ultralisk_count = count_units_plus_production(st, unit_types::ultralisk);
		queen_count = count_units_plus_production(st, unit_types::queen);
		defiler_count = count_units_plus_production(st, unit_types::defiler);
		devourer_count = count_units_plus_production(st, unit_types::devourer);
		guardian_count = count_units_plus_production(st, unit_types::guardian);
		sunken_count = count_units_plus_production(st, unit_types::sunken_colony);
		spore_count = count_units_plus_production(st, unit_types::spore_colony);

		army_supply = 0.0;
		air_army_supply = 0.0;
		ground_army_supply = 0.0;
		for (auto&v : st.units) {
			if (v.second.empty()) continue;
			unit_type*ut = v.first;
			if (!ut->is_worker) {
				army_supply += ut->required_supply * v.second.size();
				if (ut->is_flyer) air_army_supply += ut->required_supply * v.second.size();
				else ground_army_supply += ut->required_supply * v.second.size();
			}
		}
		for (auto&v : st.production) {
			unit_type*ut = v.second;
			if (!ut->is_worker) {
				army_supply += ut->required_supply;
				if (ut->is_flyer) air_army_supply += ut->required_supply;
				else ground_army_supply += ut->required_supply;
			}
		}
	}

	int n_mineral_patches() {
		int r = 0;
		for (auto&b : buildpred::get_my_current_state().bases) {
			for (auto&s : b.s->resources) {
				resource_gathering::resource_t*res = nullptr;
				for (auto&s2 : resource_gathering::live_resources) {
					if (s2.u == s.u) {
						res = &s2;
						break;
					}
				}
				if (res) {
					if (!res->gatherers.empty()) ++r;
				}
			}
		}
		return r;
	};

	void run() {

		using namespace buildpred;

		combat::no_aggressive_groups = true;
		combat::no_scout_around = false;

		combat::aggressive_zerglings = false;
		combat::aggressive_mutalisks = false;

		get_upgrades::set_upgrade_value(upgrade_types::burrow, 100000.0);
		get_upgrades::set_upgrade_value(upgrade_types::ventral_sacs, 100000.0);
		get_upgrades::set_upgrade_value(upgrade_types::antennae, 100000.0);

		get_upgrades::set_no_auto_upgrades(true);

		scouting::scout_supply = 12;

		resource_gathering::max_gas = 0.0;

		auto get_static_defence_pos = [&](unit_type*ut) {
			a_vector<std::tuple<xy, xy>> my_bases;
			for (auto&v : buildpred::get_my_current_state().bases) {
				my_bases.emplace_back(v.s->pos, square_pathing::get_nearest_node_pos(unit_types::siege_tank_tank_mode, v.s->cc_build_pos));
			}
			xy op_base = combat::op_closest_base;
			a_vector<a_deque<xy>> paths;
			for (auto&v : my_bases) {
				xy pos = std::get<1>(v);
				auto path = combat::find_bigstep_path(unit_types::siege_tank_tank_mode, pos, op_base, square_pathing::pathing_map_index::no_enemy_buildings);
				if (path.empty()) continue;
				path.push_front(pos);
				path.push_front(std::get<0>(v));
				paths.push_back(std::move(path));
			}
			log("static defence (%s) %d paths:\n", ut->name, paths.size());
			for (auto&p : paths) {
				log(" - %d nodes\n", p.size());
			}
			if (!paths.empty()) {

				auto pred = [&](grid::build_square&bs) {
					if (!build::build_has_existing_creep(bs, ut)) {
						bool is_next_to_hatchery = false;
						for (unit*u : my_units_of_type[unit_types::hatchery]) {
							if (!u->building) continue;
							if (u->is_completed) continue;
							if (u->remaining_build_time > 15 * 20) continue;
							xy pos = u->building->build_pos;
							xy a0 = bs.pos;
							xy a1 = bs.pos + xy(ut->tile_width * 32, ut->tile_height * 32);
							xy b0 = u->pos;
							xy b1 = u->pos + xy(u->type->tile_width * 32, u->type->tile_height * 32);
							if (a0.x > b1.x) continue;
							else if (b0.x > a1.x) continue;
							if (a0.y > b1.y) continue;
							else if (b0.y > a1.y) continue;
							is_next_to_hatchery = true;
							break;
						}
						if (!is_next_to_hatchery) return false;
					}
					if (!build_spot_finder::can_build_at(ut, bs)) return false;
					return true;
				};
				auto score = [&](xy pos) {
					double r = 0.0;
					for (auto&path : paths) {
						bool in_range = false;
						double v = 1.0 / 64;
						double dist = 0.0;
						for (xy p : path) {
							if (place_static_defence_only_at_main && diag_distance(p - my_start_location) >= 32 * 12) continue;
							double d = diag_distance(pos - p);
							if (d <= 32 * 7) {
								in_range = true;
								d = std::max(d, 32.0);
								dist += d*d;
							}
							v /= 2;
						}
						dist = std::sqrt(dist);
						if (dist == 0.0) dist = 32 * 15;
						if (dist < 64.0) dist = 64.0;
						r -= 1.0 / dist;
						if (in_range) r -= 1.0;
					}
					return r;
				};
				std::vector<xy> starts;
				xy pos;
				if (place_static_defence_only_at_main) {
					for (unit*u : my_resource_depots) {
						if (diag_distance(u->pos - my_start_location) <= 32 * 6) starts.push_back(u->pos);
					}
					pos = build_spot_finder::find_best(starts, 256, pred, score);
				} else {
					for (unit*u : my_resource_depots) starts.push_back(u->pos);
					pos = build_spot_finder::find_best(starts, 256, pred, score);
					log("score(pos) is %g\n", score(pos));
					if (score(pos) > -2.0) {
						log("not good enough\n");
						return xy();
					}
				}
				log("static defence (%s) build pos -> %d %d (%g away from my_closest_base)\n", ut->name, pos.x, pos.y, (pos - combat::my_closest_base).length());
				return pos;
			}
			return xy();
		};

		auto place_static_defence = [&]() {
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					//if (b.build_near != combat::defence_choke.center) {
					if (true) {
						b.build_near = combat::defence_choke.center;
						b.require_existing_creep = true;
						xy prev_pos = b.build_pos;
						build::unset_build_pos(&b);
						xy pos = get_static_defence_pos(b.type->unit);
						if (pos != xy()) {
							build::set_build_pos(&b, pos);
						} else build::set_build_pos(&b, prev_pos);
					}
				}
			}
		};

		auto free_mineral_patches = [&]() {
			int free_mineral_patches = 0;
			for (auto&b : get_my_current_state().bases) {
				for (auto&s : b.s->resources) {
					resource_gathering::resource_t*res = nullptr;
					for (auto&s2 : resource_gathering::live_resources) {
						if (s2.u == s.u) {
							res = &s2;
							break;
						}
					}
					if (res) {
						if (res->gatherers.empty()) ++free_mineral_patches;
					}
				}
			}
			return free_mineral_patches;
		};

		auto place_expos = [&]() {

			auto st = get_my_current_state();

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit && b.type->unit->is_resource_depot && b.type->unit->builder_type->is_worker) {
					xy pos = b.build_pos;
					build::unset_build_pos(&b);
				}
			}

			int bases_with_minerals = 0;
			for (auto&b : get_my_current_state().bases) {
				int min_count = 0;
				for (auto&s : b.s->resources) {
					if (s.u->type->is_minerals) ++min_count;
				}
				if (min_count >= 4) ++bases_with_minerals;
			}

			if (bases_with_minerals && (current_used_total_supply < 60.0 || free_mineral_patches())) {
				if (max_bases && bases_with_minerals >= max_bases) return;
				if ((int)st.bases.size() >= min_bases) {
					int my_hatch_count = my_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() + my_units_of_type[unit_types::hive].size();
					if (my_hatch_count > 5 || my_hatch_count >= (int)st.bases.size() * 2) {
						int n = 8;
						auto s = get_next_base();
						//if (s) n = (int)s->resources.size();
						if (s) {
							n = 0;
							for (auto&v : s->resources) ++n;
						}
						if (free_mineral_patches() >= n && long_distance_miners() == 0) return;
					} else {
						if (free_mineral_patches() >= 2 && long_distance_miners() == 0) return;
					}
				}
			}

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				//if (b.type->unit && b.type->unit->is_resource_depot && b.type->unit->builder_type->is_worker) {
				if (b.type->unit == unit_types::hatchery) {
					xy pos = b.build_pos;
					build::unset_build_pos(&b);
					auto s = get_next_base();
					if (s) pos = s->cc_build_pos;
					if (pos != xy()) build::set_build_pos(&b, pos);
					else build::unset_build_pos(&b);
				}
			}

		};

		init();

		auto weapon_damage_against_size = [&](weapon_stats*w, int size) {
			if (!w) return 0.0;
			double damage = w->damage;
			damage *= combat_eval::get_damage_type_modifier(w->damage_type, size);
			if (damage <= 0) damage = 1.0;
			return damage;
		};

		double attack_my_lost;
		double attack_op_lost;
		int last_attack_begin = 0;
		int last_attack_end = 0;
		int attack_count = 0;

		auto begin_attack = [&]() {
			is_attacking = true;
			attack_my_lost = players::my_player->minerals_lost + players::my_player->gas_lost;
			attack_op_lost = players::opponent_player->minerals_lost + players::opponent_player->gas_lost;
			last_attack_begin = current_frame;
			++attack_count;
			log("begin attack\n");
		};
		auto end_attack = [&]() {
			is_attacking = false;
			last_attack_end = current_frame;
			log("end attack\n");
		};

		bool maxed_out_aggression = false;
		while (true) {

			my_hatch_count = my_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() + my_units_of_type[unit_types::hive].size();

			my_zergling_count = my_units_of_type[unit_types::zergling].size();
			my_queen_count = my_units_of_type[unit_types::queen].size();
			my_defiler_count = my_units_of_type[unit_types::defiler].size();
			my_mutalisk_count = my_units_of_type[unit_types::mutalisk].size();

			set_build_vars(get_my_current_state());

			enemy_terran_unit_count = 0;
			enemy_protoss_unit_count = 0;
			enemy_zerg_unit_count = 0;
			enemy_marine_count = 0;
			enemy_firebat_count = 0;
			enemy_ghost_count = 0;
			enemy_vulture_count = 0;
			enemy_tank_count = 0;
			enemy_goliath_count = 0;
			enemy_wraith_count = 0;
			enemy_valkyrie_count = 0;
			enemy_bc_count = 0;
			enemy_science_vessel_count = 0;
			enemy_dropship_count = 0;
			enemy_reaver_count = 0;
			enemy_shuttle_count = 0;
			enemy_robotics_facility_count = 0;
			enemy_army_supply = 0.0;
			enemy_air_army_supply = 0.0;
			enemy_ground_army_supply = 0.0;
			enemy_ground_large_army_supply = 0.0;
			enemy_ground_small_army_supply = 0.0;
			enemy_weak_against_ultra_supply = 0.0;
			enemy_anti_air_army_supply = 0.0;
			enemy_static_defence_count = 0;
			enemy_proxy_building_count = 0;
			enemy_attacking_army_supply = 0.0;
			enemy_attacking_worker_count = 0;
			enemy_dt_count = 0;
			enemy_lurker_count = 0;
			enemy_arbiter_count = 0;
			enemy_cannon_count = 0;
			enemy_bunker_count = 0;
			enemy_units_that_shoot_up_count = 0;
			enemy_gas_count = 0;
			enemy_barracks_count = 0;
			enemy_gateway_count = 0;
			enemy_zergling_count = 0;
			enemy_spire_count = 0;
			enemy_cloaked_unit_count = 0;
			enemy_zealot_count = 0;
			enemy_dragoon_count = 0;
			enemy_worker_count = 0;
			enemy_spawning_pool_count = 0;
			enemy_evolution_chamber_count = 0;

			update_possible_start_locations();
			for (unit*e : enemy_units) {

				if (e->type->race == race_terran) ++enemy_terran_unit_count;
				if (e->type->race == race_protoss) ++enemy_protoss_unit_count;
				if (e->type->race == race_zerg) ++enemy_zerg_unit_count;

				if (e->type == unit_types::marine) ++enemy_marine_count;
				if (e->type == unit_types::firebat) ++enemy_firebat_count;
				if (e->type == unit_types::ghost) ++enemy_ghost_count;
				if (e->type == unit_types::vulture) ++enemy_vulture_count;
				if (e->type == unit_types::siege_tank_tank_mode) ++enemy_tank_count;
				if (e->type == unit_types::siege_tank_siege_mode) ++enemy_tank_count;
				if (e->type == unit_types::goliath) ++enemy_goliath_count;
				if (e->type == unit_types::wraith) ++enemy_wraith_count;
				if (e->type == unit_types::valkyrie) ++enemy_valkyrie_count;
				if (e->type == unit_types::battlecruiser) ++enemy_bc_count;
				if (e->type == unit_types::science_vessel) ++enemy_science_vessel_count;
				if (e->type == unit_types::dropship) ++enemy_dropship_count;
				if (e->type == unit_types::reaver) ++enemy_reaver_count;
				if (e->type == unit_types::shuttle) ++enemy_shuttle_count;
				if (e->type == unit_types::robotics_facility) ++enemy_robotics_facility_count;
				if (!e->type->is_worker && !e->building) {
					if (e->is_flying) enemy_air_army_supply += e->type->required_supply;
					else {
						enemy_ground_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_large) enemy_ground_large_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_small) enemy_ground_small_army_supply += e->type->required_supply;
						if (weapon_damage_against_size(e->stats->ground_weapon, unit_type::size_large) <= 12.0) enemy_weak_against_ultra_supply += e->type->required_supply;
					}
					enemy_army_supply += e->type->required_supply;
					if (e->stats->air_weapon) enemy_anti_air_army_supply += e->type->required_supply;
				}
				if (e->type == unit_types::missile_turret) ++enemy_static_defence_count;
				if (e->type == unit_types::photon_cannon) ++enemy_static_defence_count;
				if (e->type == unit_types::sunken_colony) ++enemy_static_defence_count;
				if (e->type == unit_types::spore_colony) ++enemy_static_defence_count;
				if (!e->type->is_refinery) {
					double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
						return unit_pathing_distance(unit_types::scv, e->pos, pos);
					});
					if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base) < e_d) {
						log("%s is proxied\n", e->type->name);
						if (e->building) ++enemy_proxy_building_count;
						if (!e->type->is_worker) enemy_attacking_army_supply += e->type->required_supply;
						else ++enemy_attacking_worker_count;
					}
				}
				if (e->type == unit_types::dark_templar) ++enemy_dt_count;
				if (e->type == unit_types::lurker) ++enemy_lurker_count;
				if (e->type == unit_types::arbiter) ++enemy_arbiter_count;
				if (e->type == unit_types::photon_cannon) ++enemy_cannon_count;
				if (e->type == unit_types::bunker) ++enemy_bunker_count;
				if (e->stats->air_weapon) ++enemy_units_that_shoot_up_count;
				if (e->type->is_gas) ++enemy_gas_count;
				if (e->type == unit_types::barracks) ++enemy_barracks_count;
				if (e->type == unit_types::gateway) ++enemy_gateway_count;
				if (e->type == unit_types::zergling) ++enemy_zergling_count;
				if (e->type == unit_types::spire) ++enemy_spire_count;
				if (e->cloaked) ++enemy_cloaked_unit_count;
				if (e->type == unit_types::zealot) ++enemy_zealot_count;
				if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
				if (e->type->is_worker) ++enemy_worker_count;
				if (e->type == unit_types::spawning_pool) ++enemy_spawning_pool_count;
				if (e->type == unit_types::evolution_chamber) ++enemy_evolution_chamber_count;
			}

			if (enemy_terran_unit_count + enemy_protoss_unit_count) overlord_scout(enemy_gas_count + enemy_units_that_shoot_up_count + enemy_barracks_count == 0);
			else overlord_scout(enemy_units_that_shoot_up_count + enemy_spire_count == 0);

			opponent_has_expanded = false;
			for (auto&s : resource_spots::spots) {
				bool is_start_location = false;
				for (xy pos : start_locations) {
					if (diag_distance(pos - s.cc_build_pos) <= 32 * 10) {
						is_start_location = true;
						break;
					}
				}
				if (is_start_location) continue;
				auto&bs = grid::get_build_square(s.cc_build_pos);
				if (bs.building && bs.building->owner == players::opponent_player) opponent_has_expanded = true;
			}

			if (enemy_attacking_army_supply > 2.0 || enemy_attacking_worker_count >= 4 || enemy_proxy_building_count) {
				if (current_frame < 15 * 60 * 8 && !opponent_has_expanded) {
					being_rushed = true;
				}
			} else {
				if (current_used_total_supply - my_workers.size() >= enemy_army_supply + 8.0) {
					being_rushed = false;
				}
			}

			is_defending = false;
			for (auto&g : combat::groups) {
				if (!g.is_aggressive_group && !g.is_defensive_group) {
					if (g.ground_dpf >= 2.0) {
						is_defending = true;
						break;
					}
				}
			}

			static_defence_pos_is_valid = get_static_defence_pos(unit_types::creep_colony) != xy();

// 			if (army_supply < 8.0 && enemy_proxy_building_count) {
// 				for (unit*u : my_workers) {
// 					u->force_combat_unit = true;
// 				}
// 			} else {
// 				for (unit*u : my_workers) {
// 					u->force_combat_unit = false;
// 				}
// 			}

			//if (default_worker_defence) {
			if (default_worker_defence && current_used_total_supply - my_workers.size() < 8.0 && enemy_proxy_building_count && enemy_army_supply == 0.0) {

				bool dont_pull_workers = false;
				xy proxy_pos;
				for (auto&g : combat::groups) {
					if (!g.is_aggressive_group && !g.is_defensive_group) {
						for (unit*e : g.enemies) {
							if (!e->building) continue;
							if (e->type->is_gas) continue;
							if (e->is_flying) continue;
							if (e->type != unit_types::pylon && e->type != unit_types::photon_cannon && e->type != unit_types::bunker) {
								dont_pull_workers = true;
								break;
							}
						}
						for (unit*e : g.enemies) {
							if (!e->building) continue;
							if (e->type->is_gas) continue;
							if (e->is_flying) continue;
							if (e->type != unit_types::pylon && e->type != unit_types::photon_cannon && e->type != unit_types::bunker) {
							}
							double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
								return unit_pathing_distance(unit_types::scv, e->pos, pos);
							});
							if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base) < e_d) {
								proxy_pos = e->pos;
								break;
							}
						}
					}
				}
				if (dont_pull_workers) proxy_pos = xy();

				//if (is_defending && !ground_defence_fight_ok && my_workers.size() < 18) {
				//if (is_defending && my_workers.size() < 18 && current_used_total_supply < 20) {
				if (proxy_pos != xy() && my_workers.size() < 18 && current_used_total_supply < 20) {
					//int n = my_workers.size() / 2;
					int n = my_workers.size() - 3;
					if (enemy_proxy_building_count <= 1) n = 3;
					if (n > enemy_proxy_building_count * 4) n = enemy_proxy_building_count * 4;
					for (unit*u : my_workers) {
						if (u->force_combat_unit) {
							if (u->hp < u->stats->hp / 2) u->force_combat_unit = false;
							else {
								--n;
								if (n < 0) u->force_combat_unit = false;
							}
						}
					}
					while (n > 0) {
						unit*u = get_best_score(my_workers, [&](unit*u) {
							if (u->force_combat_unit) return 0.0;
							return -u->hp;
						});
						if (!u) break;
						u->force_combat_unit = true;
						combat::combat_unit*cu = nullptr;
						for (auto*a : combat::live_combat_units) {
							if (a->u == u) cu = a;
						}
						if (cu) {
							if (diag_distance(cu->u->pos - proxy_pos) > 32 * 6) {
								cu->strategy_busy_until = current_frame + 15 * 7;
								cu->action = combat::combat_unit::action_offence;
								cu->subaction = combat::combat_unit::subaction_move;
								cu->target_pos = proxy_pos;
							} else {
								cu->strategy_attack_until = current_frame + 15 * 7;
								cu->strategy_busy_until = 0;
							}
// 							cu->strategy_attack_until = current_frame + 15 * 2;
// 							cu->strategy_busy_until = 0;
						}
						--n;
					}
				} else {
					for (unit*u : my_workers) {
						if (u->force_combat_unit) u->force_combat_unit = false;
					}
				}
			} else {
				for (unit*u : my_workers) {
					if (u->force_combat_unit) u->force_combat_unit = false;
				}
			}

			if (pull_workers_for_ling_zealot_defence && is_defending && my_completed_units_of_type[unit_types::sunken_colony].empty() && army_supply < 12.0) {
				//log(log_level_info, "pull_workers_for_ling_zealot_defence!\n");
				for (auto&g : combat::groups) {
					if (g.is_aggressive_group || g.is_defensive_group) continue;
					if (g.ground_dpf < 2.0) continue;
					bool skip = false;
					bool any_attacking = false;
					for (unit*e : g.enemies) {
						if (e->type != unit_types::zergling && e->type != unit_types::zealot && !e->type->is_worker && e->type != unit_types::overlord) {
							skip = true;
							break;
						}
						if (current_frame - e->last_attacked <= 15 * 12) any_attacking = true;
						if (!combat::my_base.test(grid::build_square_index(e->pos))) continue;
					}
					//log(log_level_info, "skip is %d\n", skip);
					if (skip) continue;
					int my_non_workers = 0;
					for (combat::combat_unit*cu : g.allies) {
						if (!cu->u->type->is_worker) ++my_non_workers;
					}
					//log(log_level_info, "my_non_workers is %d\n", my_non_workers);
					if (my_non_workers < 3) continue;

					a_vector<unit*> pulls;

					while (true) {
						combat_eval::eval eval;
						eval.max_frames = 15 * 20;
						for (combat::combat_unit*cu : g.allies) {
							if (cu->u->type->is_worker) continue;
							eval.add_unit(cu->u, 0);
						}
						int skip = 3;
						for (unit*u : pulls) {
							if (skip--) continue;
							eval.add_unit(u, 0);
						}
						for (unit*u : g.enemies) {
							eval.add_unit(u, 1);
							if (!combat::my_base.test(grid::build_square_index(u->pos))) continue;
						}
						eval.run();

						if (eval.teams[1].end_supply == 0.0) break;
						if (eval.teams[0].end_supply) break;

						size_t prev_size = pulls.size();
						for (unit*u : my_workers) {
							//if (u->controller->action != unit_controller::action_gather) continue;
							if (u->controller->action == unit_controller::action_build) continue;
							if (u->controller->action == unit_controller::action_scout) continue;
							if (std::find(pulls.begin(), pulls.end(), u) != pulls.end()) continue;
							pulls.push_back(u);
							if (pulls.size() - prev_size >= 2) break;
						}
						if (prev_size == pulls.size()) {
							//log(log_level_info, "tried pulling %d workers, not good enough and nothing more to pull :(\n", pulls.size());
							//pulls.clear();
							break;
						}
					}

					//log(log_level_info, "pull %d workers!\n", pulls.size());

					if (!pulls.empty()) {
						combat::combat_mult_override = 0.0;
						combat::combat_mult_override_until = current_frame + 15 * 4;
					}

					for (unit*u : pulls) {
						combat::combat_unit*cu = nullptr;
						for (auto*a : combat::live_combat_units) {
							if (a->u == u) cu = a;
						}
						if (cu) {
							if (diag_distance(u->pos - g.enemies.front()->pos) > 32 * 10) {
								cu->strategy_busy_until = current_frame + 15 * 2;
								cu->action = combat::combat_unit::action_offence;
								cu->subaction = combat::combat_unit::subaction_move;
								cu->target_pos = g.enemies.front()->pos;
							}
						}
						u->force_combat_unit = true;
					}
					
				}
			}

			if (current_used_total_supply >= 100 && get_upgrades::no_auto_upgrades) {
				get_upgrades::set_no_auto_upgrades(false);
			}

			combat::no_aggressive_groups = true;
			combat::aggressive_groups_done_only = false;
			if (!attack_interval) {
// 				combat::no_aggressive_groups = true;
// 				combat::aggressive_groups_done_only = false;
			} else {
				if (!is_attacking) {

					bool attack = false;
					if (attack_count == 0) attack = true;
					if (current_frame - last_attack_end >= attack_interval) attack = true;

					if (attack) begin_attack();

				} else {

					int lose_count = 0;
					int total_count = 0;
					for (auto*a : combat::live_combat_units) {
						if (a->u->type->is_worker) continue;
						if (!a->u->stats->ground_weapon && !a->u->stats->air_weapon) continue;
						++total_count;
						if (current_frame - a->last_fight <= 15 * 10) {
							if (a->last_run >= a->last_fight) ++lose_count;
						}
					}
					if (lose_count >= total_count / 2 + total_count / 4 || (lose_count >= total_count / 4 && !eval_combat(false, 0))) {
						end_attack();
					}

				}

				combat::no_aggressive_groups = !is_attacking;
				//combat::aggressive_zerglings = is_attacking;
			}

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				combat::aggressive_groups_done_only = false;
				if (current_used_total_supply < 150.0) maxed_out_aggression = false;
			}

			if (default_upgrades) {

				if (enemy_dt_count || enemy_lurker_count || enemy_cloaked_unit_count) {
					get_upgrades::set_upgrade_value(upgrade_types::pneumatized_carapace, -1.0);
				}

				if (my_zergling_count >= 8) {
					get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
				}

				if (!my_completed_units_of_type[unit_types::lair].empty()) {
					if (enemy_ground_small_army_supply >= 10 && current_used_total_supply >= 60) {
						get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
					} else {
						if (current_used_total_supply >= 100) get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
						else get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, 0.0);
					}
				}

				if (enemy_ground_large_army_supply >= 10 || my_units_of_type[unit_types::hydralisk].size() >= 12) {
					get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, -1.0);
					get_upgrades::set_upgrade_value(upgrade_types::grooved_spines, -1.0);
					get_upgrades::set_upgrade_order(upgrade_types::lurker_aspect, -11.0);
					get_upgrades::set_upgrade_order(upgrade_types::muscular_augments, -10.0);
					get_upgrades::set_upgrade_order(upgrade_types::grooved_spines, -9.0);
					get_upgrades::set_upgrade_value(upgrade_types::zerg_missile_attacks_1, -1.0);
				}
				if (my_queen_count) {
					//if (enemy_ground_small_army_supply >= 20) get_upgrades::set_upgrade_value(upgrade_types::ensnare, -1.0);
					get_upgrades::set_upgrade_value(upgrade_types::spawn_broodling, -1.0);
					//get_upgrades::set_upgrade_order(upgrade_types::ensnare, -10.0);
					get_upgrades::set_upgrade_order(upgrade_types::spawn_broodling, -9.0);
				}

				if (my_mutalisk_count >= 4) {
					get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_1, -1.0);
					get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_2, -1.0);
					get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_carapace_3, -1.0);
					if (my_mutalisk_count >= 10) {
						get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_attacks_1, -1.0);
						get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_attacks_2, -1.0);
						get_upgrades::set_upgrade_value(upgrade_types::zerg_flyer_attacks_3, -1.0);
					}
				}

				if (!my_units_of_type[unit_types::hive].empty()) {
					get_upgrades::set_upgrade_value(upgrade_types::antennae, 0.0);
				}
				if (!my_completed_units_of_type[unit_types::hive].empty()) {
					get_upgrades::set_upgrade_order(upgrade_types::adrenal_glands, -20.0);
				}
				if (!my_completed_units_of_type[unit_types::defiler_mound].empty()) {
					get_upgrades::set_upgrade_value(upgrade_types::consume, -1.0);
					get_upgrades::set_upgrade_value(upgrade_types::plague, -1.0);
					get_upgrades::set_upgrade_order(upgrade_types::consume, -10.0);
					get_upgrades::set_upgrade_order(upgrade_types::plague, -9.0);
				}

				if (current_used_total_supply >= 100 && enemy_weak_against_ultra_supply >= 14 && !my_completed_units_of_type[unit_types::ultralisk_cavern].empty()) {
					get_upgrades::set_upgrade_value(upgrade_types::chitinous_plating, -1.0);
					get_upgrades::set_upgrade_value(upgrade_types::anabolic_synthesis, -1.0);
				}
			}

			if (current_used_total_supply < 60) {
				if (!my_completed_units_of_type[unit_types::sunken_colony].empty()) {
					combat::my_closest_base_override_until = current_frame + 15 * 20;
					combat::my_closest_base_override = my_completed_units_of_type[unit_types::sunken_colony].front()->pos;
					combat::defence_choke_use_nearest_until = current_frame + 15 * 20;
				}
			}

			can_expand = get_next_base();
			force_expand = can_expand && long_distance_miners() >= 1 && my_units_of_type[unit_types::hatchery].size() == my_completed_units_of_type[unit_types::hatchery].size();
			if (can_expand && free_mineral_patches() == 0 && my_workers.size() >= 45) force_expand = true;

			if (opening_state != -1 && current_used_supply[race_zerg] + 0.5 >= current_max_supply[race_zerg]) {
				for (auto&b : build::build_tasks) {
					if (b.dead) continue;
					if (b.type->unit && b.type->unit->is_gas) {
						auto st = get_my_current_state();
						int gas_count = 0;
						for (auto&base : st.bases) {
							for (auto&r : base.s->resources) {
								if (r.u->type == unit_types::vespene_geyser) ++gas_count;
								if (r.u->type == unit_types::extractor && r.u->owner == players::my_player) ++gas_count;
							}
						}
						if (gas_count == 0) {
							b.dead = true;
						}
					}
				}

				auto st = get_my_current_state();
				int overlords = 0;
				int buildings = 0;
				double supply_needed = 0.0;
				for (auto&b : build::build_tasks) {
					if (b.type->unit == unit_types::overlord) ++overlords;
					if (b.dead || b.built_unit) continue;
					if (b.type->unit && b.type->unit->is_building) ++buildings;
					if (b.type->unit) supply_needed += b.type->unit->required_supply;
				}
				if (overlords == 0 && st.used_supply[race_zerg] + supply_needed - buildings > st.max_supply[race_zerg]) {
					bo_cancel_all();
				}
			}

			if (tick() || should_transition()) {
				bo_cancel_all();
				break;
			}

			if (my_workers.size() <= 4) {
				for (auto&v : scouting::all_scouts) {
					if (v.scout_unit && v.scout_unit->type->is_worker) scouting::rm_scout(v.scout_unit);
				}
			}

			auto build = [&](state&st) {

				set_build_vars(st);

				army = [&](state&st) {
					return false;
				};

				if (drone_count <= 2) {
					return depbuild(st, state(st), unit_types::drone);
				}

				return this->build(st);

			};

			if (call_build) execute_build(false, build);

			place_expos();
			if (call_place_static_defence) place_static_defence();

			multitasking::sleep(sleep_time);
		}


	}

	void render() {

		game->drawTextScreen(300, 8, "drones: %d", my_workers.size());

	}

};

