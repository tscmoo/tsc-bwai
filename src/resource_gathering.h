

namespace resource_gathering {
;

static const int max_predicted_frames = 15 * 120;
a_map<int, int> predicted_mineral_delivery, predicted_gas_delivery;
double minerals_to_gas_weight = 1.0;
double max_gas = 0.0;

struct gatherer_t;
struct resource_t {
	std::pair<resource_t*, resource_t*> link;
	int live_frame = 0;
	unit*u = nullptr;
	a_vector<gatherer_t*> gatherers;
	unit*depot = nullptr;
	bool dead = false;
	int round_trip_time = 10;
	int last_round_trip_time_calc = 0;
	a_vector<double> income_rate, income_sum;
	int last_update_income_rate = -1;
	int current_complete_round_trip_time = 0;
	a_vector<int> deliveries;
};

a_list<resource_t> all_resources;
tsc::intrusive_list<resource_t, &resource_t::link> live_resources, dead_resources;
a_unordered_map<unit*, resource_t*> unit_resource_map;

struct gatherer_t {
	std::pair<gatherer_t*, gatherer_t*> link;
	int live_frame = 0;
	unit*u = nullptr;
	resource_t*resource = nullptr;
	bool dead = false;
	int last_delivery = 0;
	bool is_carrying = false;
	int last_find_transfer = 0;
	int round_trip_begin = 0;
};

a_list<gatherer_t> all_gatherers;
tsc::intrusive_list<gatherer_t, &gatherer_t::link> live_gatherers, dead_gatherers;
a_unordered_map<unit*, gatherer_t*> unit_gatherer_map;

a_vector<unit*> resource_depots;


void clear_deliveries(resource_t&r) {
	auto&pd = r.u->type->is_gas ? predicted_gas_delivery : predicted_mineral_delivery;
	for (int f : r.deliveries) {
		auto i = pd.find(f&-16);
		i->second -= 8;
		if (i->second == 0) pd.erase(i);
	}
	r.deliveries.clear();
}
void update_deliveries(resource_t&r) {
	clear_deliveries(r);
	if (r.current_complete_round_trip_time == 0) return;
	auto&pd = r.u->type->is_gas ? predicted_gas_delivery : predicted_mineral_delivery;
	for (gatherer_t*g : r.gatherers) {
		for (int f = g->last_delivery + r.current_complete_round_trip_time; f < current_frame + max_predicted_frames; f += r.current_complete_round_trip_time) {
			if (f < current_frame) f = current_frame + r.current_complete_round_trip_time;
			pd[f&-16] += 8;
			r.deliveries.push_back(f&-16);
		}
	}
}

void process(resource_t&r) {

	if ((r.u->type->is_gas && r.u->owner != players::my_player) || (!r.u->is_completed && r.u->remaining_build_time >= 15 * 4)) {
		r.depot = nullptr;
		return;
	}
	unit*depot = nullptr;
	//if (!resource_depots.empty() && combat::can_transfer_to(r.u)) {
	if (!resource_depots.empty()) {
		depot = get_best_score(resource_depots, [&](unit*u) {
			if (!u->is_completed && !u->is_morphing && u->remaining_build_time > 15 * 20) return std::numeric_limits<double>::infinity();
			return units_pathing_distance(unit_types::scv, r.u, u);
		}, std::numeric_limits<double>::infinity());
		if (!depot && current_frame < 15 * 60) {
			depot = get_best_score(resource_depots, [&](unit*u) {
				return units_distance(r.u, u);
			}, std::numeric_limits<double>::infinity());
		}
	}
	bool depot_changed = false;
	if (depot != r.depot) {
		r.depot = depot;
		depot_changed = true;
	}
	//log("depot %p\n", depot);
	if ((current_frame - r.last_round_trip_time_calc >= 60 || depot_changed) && depot) {
		r.last_round_trip_time_calc = current_frame;
		gatherer_t*nearest_gatherer = 0;
		if (!live_gatherers.empty()) nearest_gatherer = &live_gatherers.front();
		if (nearest_gatherer) {
			int round_trip_time = 0;
			xy depot_top_left = depot->pos - xy(nearest_gatherer->u->type->dimension_right() + depot->type->dimension_left(), nearest_gatherer->u->type->dimension_down() + depot->type->dimension_up());
			xy depot_bottom_right = depot->pos + xy(nearest_gatherer->u->type->dimension_left() + depot->type->dimension_right(), nearest_gatherer->u->type->dimension_up() + depot->type->dimension_down());
			xy depot_pos = nearest_spot_in_square(r.u->pos, depot_top_left, depot_bottom_right);
			xy resource_top_left = r.u->pos - xy(nearest_gatherer->u->type->dimension_right() + r.u->type->dimension_left(), nearest_gatherer->u->type->dimension_down() + r.u->type->dimension_up());
			xy resource_bottom_right = r.u->pos + xy(nearest_gatherer->u->type->dimension_left() + r.u->type->dimension_right(), nearest_gatherer->u->type->dimension_up() + r.u->type->dimension_down());
			xy resource_pos = nearest_spot_in_square(depot->pos, resource_top_left, resource_bottom_right);

			double length = diag_distance(depot_pos - resource_pos);
			if (length >= 32 * 8) length = unit_pathing_distance(nearest_gatherer->u->type, resource_pos, depot_pos);
			round_trip_time = frames_to_reach(nearest_gatherer->u->stats, 0.0, length) * 2;

			round_trip_time += (int)(PI / nearest_gatherer->u->stats->turn_rate);
			
			r.round_trip_time = round_trip_time;
		}
	}
	if (!depot && (r.round_trip_time || !r.income_rate.empty())) {
		r.round_trip_time = 0;
		r.income_rate.clear();
		r.income_sum.clear();
	}

	if (r.last_update_income_rate < r.last_round_trip_time_calc && r.round_trip_time && r.depot) {
		r.last_update_income_rate = current_frame;
		int gather_time = r.u->type->is_gas ? 37 : 82;

		r.income_rate.clear();
		r.income_sum.clear();
		int t = r.round_trip_time + gather_time;
		double om = (double)t / (double)gather_time;
		double m = om;
		double sum = 0;
		while (m > 0) {
			double inc = 8.0 / (double)t;
			if (m < 1) inc *= m;
			m -= 1;
			sum += inc;
			r.income_rate.push_back(inc);
			r.income_sum.push_back(sum);
		}
		r.current_complete_round_trip_time = t + (int)(std::max(r.gatherers.size() - om, 0.0)*gather_time);
		update_deliveries(r);
	}

	for (auto i = r.gatherers.begin(); i != r.gatherers.end();) {
		gatherer_t*g = *i;
		if (g->dead) {
			g->resource = nullptr;
			i = r.gatherers.erase(i);
		} else ++i;
	}

}

void process(gatherer_t&g) {
	unit*u = g.u;

	bool is_carrying = u->game_unit->isCarryingMinerals() || u->game_unit->isCarryingGas();
	bool delivered = false;
	if (is_carrying != g.is_carrying) {
		g.is_carrying = is_carrying;
		if (!is_carrying) {
			delivered = true;
			g.last_delivery = current_frame;
			if (g.resource) {
				update_deliveries(*g.resource);

				if (g.round_trip_begin) {
					int round_trip = current_frame - g.round_trip_begin;
					log(" - actual round trip %d, resource round_trip_time %d, current_complete_round_trip_time %d\n", round_trip, g.resource->round_trip_time, g.resource->current_complete_round_trip_time);
				}
			}
			g.round_trip_begin = current_frame;
		}
	}

	double gas_limit = 0.0;
	if (my_workers.size() < 15) gas_limit = current_minerals;
	if (my_workers.size() >= 15) gas_limit = 400;
	if (my_workers.size() >= 30) gas_limit = 1000;
	if (current_minerals > gas_limit) gas_limit = current_minerals;
	if (current_gas<gas_limit) minerals_to_gas_weight = 0.25;
	else minerals_to_gas_weight = 4.0;
	if (max_gas) gas_limit = max_gas;
	if (max_gas && current_gas < max_gas) minerals_to_gas_weight = 0.125;
	if (max_gas && current_gas >= max_gas) minerals_to_gas_weight = 8.0;

	bool relocate = current_frame - g.last_find_transfer >= 15 && g.resource && !g.resource->depot;

	if (g.last_find_transfer == 0 || current_frame - g.last_find_transfer >= 15 * 15 || delivered || relocate) {
		g.last_find_transfer = current_frame;
		if (g.resource) {
			find_and_erase(g.resource->gatherers, &g);
			g.resource = nullptr;
		}
		a_unordered_set<resource_t*> reachable;
		if (!g.u->is_flying) {
			for (auto&r : live_resources) {
				bool can_reach = true;
				for (auto*n : square_pathing::find_path(square_pathing::get_pathing_map(g.u->type), g.u->pos, r.u->pos)) {
					xy pos = n->pos;
					if (!combat::can_transfer_through(n->pos)) {
						can_reach = false;
						break;
					}
				}
				if (can_reach) reachable.insert(&r);
			}
		}
		resource_t*new_r = get_best_score_p(live_resources, [&](resource_t*r) {
			if (!r->depot) return 0.0;
			if (!reachable.empty() && !reachable.count(r)) return 0.0;
			if (!square_pathing::unit_can_reach(g.u, g.u->pos, r->u->pos)) return 0.0;
			if (r->u->type->is_gas && r->gatherers.size() >= 3) return 0.0;
			if (r->u->type->is_gas && current_gas + 8 * r->gatherers.size() >= gas_limit) return 0.0;
			double next_income = 0.0;
			if (r->income_rate.size() > r->gatherers.size()) next_income = r->income_rate[r->gatherers.size()];
			if (next_income == 0.0) return 0.0;
// 			double distance;
// 			if (diag_distance(g.u->pos - r->u->pos) <= 32 * 10) distance = 0.0;
// 			else distance = units_pathing_distance(g.u, r->u);
// 			if (distance >= 32 * 40) distance = 32 * 40;
// 			double distance_cost = distance / (32 * 1800);
			if (diag_distance(g.u->pos - r->u->pos) > 32 * 10) next_income *= 0.75;
			double distance_cost = 0.0;
			double mult = r->u->type->is_gas ? 1.0 : minerals_to_gas_weight;
			return -(std::max(next_income - distance_cost, 1e-8)) * mult;
		}, 0.0);
		if (new_r) {
			new_r->gatherers.push_back(&g);
			g.resource = new_r;
			if (!delivered) g.round_trip_begin = 0;
		}
	}

	if (g.resource) {
		u->controller->depot = g.resource->depot;
	}

	if (g.resource && g.resource->dead) {
		g.resource->gatherers.erase(std::find(g.resource->gatherers.begin(), g.resource->gatherers.end(), &g));
		g.resource = nullptr;
	}
	if (g.resource) {
		if (u->controller->action == unit_controller::action_gather) {
			u->controller->target = g.resource->u;
		}
	} else {
		if (u->controller->action == unit_controller::action_gather) {
			u->controller->target = nullptr;
		}
		if (!u->is_carrying_minerals_or_gas) u->controller->depot = nullptr;
	}

}

void resource_gathering_task() {

	a_unordered_set<unit*> initial_worker_mine_taken;

	for (unit*u : my_units_of_type[unit_types::cc]) {
		u->game_unit->train(BWAPI::UnitTypes::Terran_SCV);
	}

	while (true) {

		resource_depots.clear();
		for (unit*u : my_units) {
			if (!u->type->game_unit_type.isResourceDepot()) continue;
			if (u->building && u->building->is_lifted) continue;
			if (!u->is_completed && u->type != unit_types::lair && u->type != unit_types::hive) continue;
			resource_depots.push_back(u);
		}

		for (unit*u : my_workers) {
			unit_controller*c = get_unit_controller(u);
			if (c->action == unit_controller::action_idle && !my_resource_depots.empty()) {
				c->action = unit_controller::action_gather;
				c->target = nullptr;
				c->depot = nullptr;
			}
			if (c->action == unit_controller::action_gather && my_resource_depots.empty()) {
				c->action = unit_controller::action_idle;
			}
			if (c->action == unit_controller::action_gather) {
				gatherer_t*&g = unit_gatherer_map[u];
				if (!g) {
					log("new gatherer %p %s\n", u, u->type->name);
					all_gatherers.emplace_back();
					g = &all_gatherers.back();
					live_gatherers.push_back(*g);
					g->u = u;
				}
				if (g->dead) {
					log("gatherer %p revived!\n", u);
					g->dead = false;
					dead_gatherers.erase(*g);
					live_gatherers.push_back(*g);
				}
				g->live_frame = current_frame;
			}
		}
		for (unit*u : resource_units) {
			//if (u->resources == 0) continue;
			resource_t*&r = unit_resource_map[u];
			if (!r) {
				log("new resource %p %s\n", u, u->type->name);
				all_resources.emplace_back();
				r = &all_resources.back();
				live_resources.push_back(*r);
				r->u = u;
			}
			if (r->dead) {
				log("resource %p revived!\n", u);
				r->dead = false;
				dead_resources.erase(*r);
				live_resources.push_back(*r);
			}
			r->live_frame = current_frame;
		}

		for (auto i = live_resources.begin(); i != live_resources.end();) {
			if (i->live_frame != current_frame) {
				log("resource %p dead!\n", i->u);
				i->dead = true;
				auto&v = *i;
				i = live_resources.erase(v);
				dead_resources.push_back(v);
				if (!v.deliveries.empty()) clear_deliveries(v);
			} else ++i;
		}

		for (auto i = live_gatherers.begin(); i != live_gatherers.end();) {
			if (i->live_frame != current_frame) {
				log("gatherer %p dead!\n", i->u);
				i->dead = true;
				if (i->resource) {
					find_and_erase(i->resource->gatherers, &*i);
					i->resource = nullptr;
				}
				i->last_find_transfer = 0;
				auto&v = *i;
				i = live_gatherers.erase(v);
				dead_gatherers.push_back(v);
			} else ++i;
		}

		for (auto&r : live_resources) {
			process(r);
			multitasking::yield_point();
		}
		for (auto&g : live_gatherers) {
			process(g);
		}

		double mpf = 0;
		double gpf = 0;
		for (auto&r : live_resources) {
			if (r.income_sum.empty()) continue;
			if (r.gatherers.empty()) continue;
			double inc = r.income_sum.back();
			if (r.gatherers.size() <= r.income_sum.size()) inc = r.income_sum[r.gatherers.size() - 1];
			if (r.u->type->is_gas) gpf += inc;
			else mpf += inc;
		}

		predicted_minerals_per_frame = mpf;
		predicted_gas_per_frame = gpf;

		multitasking::sleep(1);
	}

}


void render() {

	// 	for (auto&g : live_gatherers) {
	// 		for (auto&v : g.reserved) {
	// 			render::draw_stacked_text(g.u->pos + xy(0,16),format("%d",1+~current_frame+v.first));
	// 		}
	// 	}
	// 
	// 	for (auto&r : live_resources) {
	// 		for (auto&v : r.reserved) {
	// 			render::draw_stacked_text(r.u->pos + xy(0,16),format("%d",1+~current_frame+v.first));
	// 		}
	// 	}
	for (auto&r : live_resources) {
		for (double v : r.income_rate) {
			//render::draw_stacked_text(r.u->pos + xy(0,0),format("%g",v));
		}
	}

	// 	for (auto&v : predicted_mineral_delivery) {
	// 		render::draw_screen_stacked_text(16,46,format("min %.02f + %d",(double)(v.first-current_frame)/15.0,v.second));
	// 	}
	// 	for (auto&v : predicted_gas_delivery) {
	// 		render::draw_screen_stacked_text(128,46,format("gas %.02f + %d",(double)(v.first-current_frame)/15.0,v.second));
	// 	}

	// 	for (auto&g : live_gatherers) {
	// 		if (resource_depots.empty()) continue;
	// 		unit*depot = resource_depots.front();
	// 		unit*u = g.u;
	// 		xy top_left = depot->pos - xy(u->type->dimension_right() + depot->type->dimension_left(),u->type->dimension_down() + depot->type->dimension_up());
	// 		xy bottom_right = depot->pos + xy(u->type->dimension_left() + depot->type->dimension_right(),u->type->dimension_up() + depot->type->dimension_down());
	// 		xy depot_pos = nearest_spot_in_square(u->pos,top_left,bottom_right);
	// 
	// 		game->drawLineMap(u->pos.x,u->pos.y,depot_pos.x,depot_pos.y,BWAPI::Colors::Yellow);
	// 
	// 		//log("speed %g\n",g.u->speed);
	// 	}
	// 
	// 	for (auto&v : render_lines) {
	// 		auto&val = std::get<1>(v);
	// 		xy a, b;
	// 		std::tie(a,b) = val;
	// 		game->drawLineMap(a.x,a.y,b.x,b.y,BWAPI::Colors::Teal);
	// 	}
}


void init() {

	multitasking::spawn(resource_gathering_task, "resource gathering");

	render::add(render);

}

}

