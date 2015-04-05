

int long_distance_miners() {
	int count = 0;
	for (auto&g : resource_gathering::live_gatherers) {
		if (!g.resource) continue;
		unit*ru = g.resource->u;
		resource_spots::spot*rs = nullptr;
		for (auto&s : resource_spots::spots) {
			if (grid::get_build_square(s.cc_build_pos).building) continue;
			for (auto&r : s.resources) {
				if (r.u == ru) {
					rs = &s;
					break;
				}
			}
			if (rs) break;
		}
		if (rs) ++count;
	}
	return count;
};


a_vector<combat::combat_unit*> scouting_overlords;
void overlord_scout(bool scout) {
	using namespace buildpred;

	for (auto*a : scouting_overlords) {
		a->strategy_busy_until = 0;
		a->action = combat::combat_unit::action_idle;
		a->subaction = combat::combat_unit::subaction_idle;
	}
	auto prev_scouting_overlords = scouting_overlords;
	scouting_overlords.clear();

	if (scout) {

		a_vector<xy> scouted_positions;
		auto send_scout_to = [&](xy pos) {
			for (xy v : scouted_positions) {
				if (diag_distance(v - pos) <= 32 * 10) return;
			}
			scouted_positions.push_back(pos);
			auto*a = get_best_score(combat::live_combat_units, [&](combat::combat_unit*a) {
				if (!a->u->is_flying || a->u->stats->ground_weapon || a->u->stats->air_weapon) return std::numeric_limits<double>::infinity();
				if (a->subaction != combat::combat_unit::subaction_idle) return std::numeric_limits<double>::infinity();
				return diag_distance(pos - a->u->pos);
			}, std::numeric_limits<double>::infinity());

			if (a) {
				scouting_overlords.push_back(a);
				a->strategy_busy_until = current_frame + 15 * 10;
				a->action = combat::combat_unit::action_offence;
				a->subaction = combat::combat_unit::subaction_move;
				a->target_pos = pos + xy(4 * 16, 3 * 16);
			}
		};

		update_possible_start_locations();
		a_vector<xy> bases_to_scout;
		for (xy pos : possible_start_locations) {
			auto&bs = grid::get_build_square(pos);
			if (bs.last_seen > 30 && (!bs.building || bs.building->owner == players::my_player)) continue;
			bases_to_scout.push_back(pos);
		}
		std::sort(bases_to_scout.begin(), bases_to_scout.end(), [&](xy a, xy b) {
			return diag_distance(my_start_location - a) < diag_distance(my_start_location - b);
		});

		if (scouting_overlords.empty()) {
			for (xy pos : bases_to_scout) {
				send_scout_to(pos);
			}
		}
		for (auto&v : get_op_current_state().bases) {
			send_scout_to(v.s->cc_build_pos);
		}
		for (unit*u : enemy_buildings) {
			if (!u->type->is_resource_depot) continue;
			send_scout_to(u->building->build_pos);
		}
		if (bases_to_scout.size() == 1) {
			xy op_start_loc = bases_to_scout.front();

			a_vector<std::tuple<double, xy>> possible_expos;
			for (auto&s : resource_spots::spots) {
				if (s.cc_build_pos == op_start_loc) continue;
				auto&bs = grid::get_build_square(s.cc_build_pos);
				if (bs.building) continue;
				possible_expos.emplace_back(unit_pathing_distance(unit_types::scv, op_start_loc, s.cc_build_pos), s.cc_build_pos);
			}
			std::sort(possible_expos.begin(), possible_expos.end());
			if (possible_expos.size() > 2) possible_expos.resize(2);
			auto*expo1s = possible_expos.size() >= 1 ? &grid::get_build_square(std::get<1>(possible_expos[0])) : nullptr;
			auto*expo2s = possible_expos.size() >= 2 ? &grid::get_build_square(std::get<1>(possible_expos[1])) : nullptr;
			if (expo1s) send_scout_to(expo1s->pos);
			if (expo2s) send_scout_to(expo2s->pos);
		}
	}

	for (auto*a : prev_scouting_overlords) {
		if (a->action == combat::combat_unit::action_idle) {
			if (diag_distance(my_start_location - a->u->pos) >= 15 * 10) {
				a->strategy_busy_until = current_frame + 15 * 20;
				a->action = combat::combat_unit::action_offence;
				a->subaction = combat::combat_unit::subaction_move;
				a->target_pos = my_start_location;
			}
		}
	}

}
