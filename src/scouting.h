
namespace scouting {
;

unit*scout_unit;

refcounted_ptr<resource_spots::spot> dst_s;
int dst_s_seen;
bool scout_resources = false;
xy scout_location;
int last_scout;

a_vector<xy> starting_locations;

void scout() {

	if (scout_unit){
		if (scout_unit->controller->action != unit_controller::action_scout) scout_unit = nullptr;
		else if (scout_unit->dead) scout_unit = nullptr;
	}

	if (!scout_unit && (last_scout == 0 || current_frame - last_scout >= 15 * 60 * 3)) {
		if (my_workers.size() < 10) return;
		scout_unit = get_best_score(my_workers, [&](unit*u) {
			if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
			return 0.0;
		}, std::numeric_limits<double>::infinity());
		if (scout_unit) last_scout = current_frame;
	}
	if (!scout_unit) return;

	if (!dst_s) {
		dst_s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
			double d;
			if (!enemy_buildings.empty()) {
				d = get_best_score_value(enemy_buildings, [&](unit*u) {
					return unit_pathing_distance(scout_unit->type, u->pos, s->cc_build_pos);
				});
			} else {
				d = get_best_score_value(starting_locations, [&](xy pos) {
					return unit_pathing_distance(scout_unit->type, pos, s->cc_build_pos);
				});
			}
			auto&bs = grid::get_build_square(s->cc_build_pos);
			int age = current_frame - bs.last_seen;
			//d /= std::max(age, 1);
			d -= age * 2;
			d += unit_pathing_distance(scout_unit, s->cc_build_pos) / 10000;
			return d;
		}, std::numeric_limits<double>::infinity());
		if (dst_s) {
			dst_s_seen = grid::get_build_square(dst_s->cc_build_pos).last_seen;
			scout_resources = false;
		}
	}

	if (!dst_s) return;

	if (dst_s_seen!=-1) {
		scout_unit->controller->action = unit_controller::action_scout;
		scout_unit->controller->go_to = dst_s->cc_build_pos;
		if (!scout_resources) {
			if (grid::is_visible(dst_s->cc_build_pos, 4, 3)) scout_resources = true;
		} else {
			auto*r = get_best_score_p(dst_s->resources, [&](const resource_spots::resource_t*r) {
				double age = current_frame - r->u->last_seen;
				if (age <= 15 * 10) return std::numeric_limits<double>::infinity();
				return -age;
			}, std::numeric_limits<double>::infinity());
			if (!r) dst_s_seen = -1;
			else scout_unit->controller->go_to = r->u->pos;
		}
	} else {
		dst_s = nullptr;
	}

}

void scan() {

	double scan_energy_cost = bwapi_tech_type_energy_cost(BWAPI::TechTypes::Scanner_Sweep);
	int scans_available = 0;
	for (unit*u : my_units_of_type[unit_types::comsat_station]) {
		if (u->energy < scan_energy_cost) continue;
		++scans_available;
	}
	if (!scans_available) return;

	a_map<xy, double> values;

	for (auto&v : buildpred::opponent_states) {
		auto&st = std::get<1>(v);
		for (auto&ri : st.resource_info) {
			values[ri.first->u->pos] += ri.second.gathered;
		}
		for (auto&v : st.bases) {
			values[v.s->pos] += 20;
			if (!v.verified) values[v.s->cc_build_pos] += 200;
		}
	}
	auto*scan_st = units::get_unit_stats(unit_types::spell_scanner_sweep, players::my_player);
	double best_score = 0.0;
	xy best_pos;
	for (auto&v : values) {
		double s = 0.0;
		for (auto&v2 : values) {
			if (&v2 == &v || (v.first - v2.first).length() <= scan_st->sight_range) s += v2.second;
		}
		if (s > best_score) {
			best_score = s;
			best_pos = v.first;
		}
	}
	if (best_score >= 500.0) {
		unit*u = get_best_score(my_units_of_type[unit_types::comsat_station], [&](unit*u) {
			return -u->energy;
		});
		if (u) {
			u->game_unit->useTech(BWAPI::TechTypes::Scanner_Sweep, BWAPI::Position(best_pos.x, best_pos.y));
		}
	}
}

void scouting_task() {

	int last_scan = 0;

	while (true) {

		multitasking::sleep(4);

		scout();

		if (current_used_supply[race_terran] >= 60) {
			if (!my_units_of_type[unit_types::cc].empty()) {
				if (my_units_of_type[unit_types::academy].empty()) {
					build::add_build_sum(0, unit_types::academy, 1);
				} else {
					for (unit*cc : my_units_of_type[unit_types::cc]) {
						if (cc->addon) continue;
						//if (cc->game_unit->canBuildAddon(unit_types::comsat_station->game_unit_type)) {
						if (true) {
							build::add_build_sum(0, unit_types::comsat_station, 1);
							break;
						}
					}
				}
			}
		}

		if (current_frame - last_scan >= 15 * 5) {
			last_scan = current_frame;
			scan();
		}
		
	}
}

void render() {

	if (scout_unit) {
		if (dst_s) game->drawLineMap(scout_unit->pos.x, scout_unit->pos.y, dst_s->pos.x, dst_s->pos.y, BWAPI::Colors::Blue);
	}

}

void init() {

	for (auto&v : game->getStartLocations()) {
		bwapi_pos p = v;
		starting_locations.emplace_back(p.x * 32, p.y * 32);
	}

	multitasking::spawn(scouting_task, "scouting");

	render::add(render);

}

}

