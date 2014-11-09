
namespace scouting {
;

unit*scout_unit;

refcounted_ptr<resource_spots::spot> dst_s;
int dst_s_seen;
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
		if (dst_s) dst_s_seen = grid::get_build_square(dst_s->cc_build_pos).last_seen;
	}

	if (!dst_s) return;

	auto&bs = grid::get_build_square(dst_s->cc_build_pos);
	if (dst_s_seen!=-1) {
		scout_unit->controller->action = unit_controller::action_scout;
		scout_unit->controller->go_to = dst_s->cc_build_pos;
		if (bs.visible) dst_s_seen = -1;
	} else {
		dst_s = nullptr;
	}

}

void scouting_task() {

	while (true) {

		multitasking::sleep(4);

		scout();
		
	}
}

void render() {

	if (scout_unit) {
		if (dst_s) game->drawLineMap(scout_unit->pos.x, scout_unit->pos.y, dst_s->pos.x, dst_s->pos.y, BWAPI::Colors::Blue);
	}

}

void init() {

	for (auto&v : game->getStartLocations()) {
		starting_locations.emplace_back(v.x * 32, v.y * 32);
	}

	multitasking::spawn(scouting_task, "scouting");

	render::add(render);

}

}

