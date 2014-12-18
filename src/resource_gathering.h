

namespace resource_gathering {
;

static const int max_predicted_frames = 15*120;
a_map<int,int> predicted_mineral_delivery, predicted_gas_delivery;
double minerals_to_gas_weight = 1.0;
double max_gas = 0.0;

struct gatherer_t;
struct resource_t {
	std::pair<resource_t*,resource_t*> link;
	int live_frame = 0;
	unit*u = nullptr;
	int begin_gather_frame = 0;
	gatherer_t*gatherer = nullptr;
	a_vector<gatherer_t*> gatherers;
	unit*depot = nullptr;
	bool dead = false;
	int round_trip_time = 10;
	int calculated_round_trip_time = 0;
	int last_round_trip_time_calc = 0;
	a_deque<std::tuple<int,int>> actual_round_trip_times;
	int gather_time = 10;
	a_deque<std::tuple<int,int>> actual_gather_times;
	int busy_until = 0;
	a_vector<double> income_rate, income_sum;
	int last_update_income_rate = -1;
	bool is_being_gathered = false;
	int current_complete_round_trip_time = 0;
	a_vector<int> deliveries;
	int last_find_transfer = 0;
};

a_list<resource_t> all_resources;
tsc::intrusive_list<resource_t,&resource_t::link> live_resources, dead_resources;
a_unordered_map<unit*,resource_t*> unit_resource_map;

struct gatherer_t {
	std::pair<gatherer_t*,gatherer_t*> link;
	int live_frame = 0;
	unit*u = nullptr;
	resource_t*resource = nullptr;
	bool dead = false;
	bool is_mining = false;
	bool is_mining_gas = false;
	bool is_carrying = false;
	int mining_until = 0;
	int last_round_trip = 0;
	int gather_begin = 0;
	int last_gather_end = 0;
	int last_delivery = 0;
	bool no_mineral_gas_transfer = false;
	bool round_trip_invalid = false;
	int last_find_depot = 0;
};

a_list<gatherer_t> all_gatherers;
tsc::intrusive_list<gatherer_t,&gatherer_t::link> live_gatherers, dead_gatherers;
a_unordered_map<unit*,gatherer_t*> unit_gatherer_map;

a_vector<unit*> resource_depots;

//a_map<resource_t*,std::tuple<xy,xy>> render_lines;

double global_calculated_modifier = 1.0;
double global_gas_gather_time_modifier = 1.0;
double global_minerals_gather_time_modifier = 1.0;

int no_mineral_gas_transfer_until;

void clear_deliveries(resource_t&r) {
	auto&pd = r.u->type->is_gas ? predicted_gas_delivery : predicted_mineral_delivery;
	for (int f : r.deliveries) {
		auto i = pd.find(f&-16);
		i->second -= 8;
		if (i->second==0) pd.erase(i);
	}
	r.deliveries.clear();
}
void update_deliveries(resource_t&r) {
	clear_deliveries(r);
	if (r.current_complete_round_trip_time==0) return;
	auto&pd = r.u->type->is_gas ? predicted_gas_delivery : predicted_mineral_delivery;
	for (gatherer_t*g : r.gatherers) {
		for (int f=g->last_delivery+r.current_complete_round_trip_time;f<current_frame+max_predicted_frames;f+=r.current_complete_round_trip_time) {
			if (f<current_frame) f = current_frame + r.current_complete_round_trip_time;
			pd[f&-16] += 8;
			r.deliveries.push_back(f&-16);
		}
	}
}

void process(resource_t&r) {
	unit*u = r.u;
	if (r.gatherer && (!u->game_unit->isBeingGathered() || r.gatherer->u->game_unit->isCarryingMinerals() || r.gatherer->u->game_unit->isCarryingGas())) {
		log("took %d frames\n",current_frame-r.begin_gather_frame);
		if (!r.gatherer->u->game_unit->isCarryingMinerals() && !r.gatherer->u->game_unit->isCarryingGas()) log("aborted!\n");
		r.gatherer = 0;
	}
	if (u->game_unit->isBeingGathered()) {
		if (!r.gatherer) {
			gatherer_t*g = 0;
			for (auto&v : live_gatherers) {
				if ((v.u->game_order==BWAPI::Orders::MiningMinerals || v.u->game_order==BWAPI::Orders::HarvestGas) && v.u->game_unit->getOrderTarget()==u->game_unit) {
					g = &v;
					break;
				}
			}
			if (!g) log(" !! resource gatherer not found\n");
			else {
				r.gatherer = g;
				r.begin_gather_frame = current_frame;
			}
		}
	}
	r.is_being_gathered = r.u->game_unit->isBeingGathered();

	if ((u->type->is_gas && u->owner!=players::my_player) || !u->is_completed) {
		r.depot = 0;
		return;
	}
	unit*depot = 0;
	//if (!resource_depots.empty()) depot = resource_depots.front();
	if (!resource_depots.empty()) {
		depot = get_best_score(resource_depots, [&](unit*u) {
			return units_pathing_distance(r.u, u);
		});
	}
	bool depot_changed = false;
	if (depot!=r.depot) {
		r.depot = depot;
		depot_changed = true;
	}
	//log("depot %p\n", depot);
	if ((current_frame-r.last_round_trip_time_calc>=60 || depot_changed) && depot) {
		r.last_round_trip_time_calc = current_frame;
		gatherer_t*nearest_gatherer = 0;
		if (!live_gatherers.empty()) nearest_gatherer = &live_gatherers.front();
		if (nearest_gatherer) {
			unit*u = nearest_gatherer->u;
			int round_trip_time = 0;
			xy depot_top_left = depot->pos - xy(u->type->dimension_right() + depot->type->dimension_left(),u->type->dimension_down() + depot->type->dimension_up());
			xy depot_bottom_right = depot->pos + xy(u->type->dimension_left() + depot->type->dimension_right(),u->type->dimension_up() + depot->type->dimension_down());
			xy depot_pos = nearest_spot_in_square(r.u->pos,depot_top_left,depot_bottom_right);
			xy resource_top_left = r.u->pos - xy(u->type->dimension_right() + r.u->type->dimension_left(),u->type->dimension_down() + r.u->type->dimension_up());
			xy resource_bottom_right = r.u->pos + xy(u->type->dimension_left() + r.u->type->dimension_right(),u->type->dimension_up() + r.u->type->dimension_down());
			xy resource_pos = nearest_spot_in_square(depot->pos,resource_top_left,resource_bottom_right);
			//render_lines[&r] = std::make_tuple(depot_pos,resource_pos);
			//log("frames to reach is %d, distance is %g\n",frames_to_reach(nearest_gatherer->u->stats,0.0,(depot_pos - resource_pos).length()),(depot_pos - resource_pos).length());
			double length = (depot_pos - resource_pos).length();
			if (length >= 32 * 7) length = unit_pathing_distance(u->type, resource_pos, depot_pos);
			round_trip_time = frames_to_reach(nearest_gatherer->u->stats,0.0,length) * 2;
			round_trip_time += 15 + 4;
			r.calculated_round_trip_time = round_trip_time;
			//log("round trip time is %d\n",round_trip_time);
		}
		if (depot_changed) {
			r.actual_round_trip_times.clear();
			r.actual_gather_times.clear();
		}
		{
			//while (!r.actual_round_trip_times.empty() && current_frame-std::get<0>(r.actual_round_trip_times.front())>=15*30) r.actual_round_trip_times.pop_front();
			while (r.actual_round_trip_times.size()>=16) r.actual_round_trip_times.pop_front();
			int sum = (int)(r.calculated_round_trip_time * global_calculated_modifier);
			for (auto&v : r.actual_round_trip_times) sum += std::get<1>(v);
			r.round_trip_time = sum / (r.actual_round_trip_times.size()+1);
			//log("round trip time averaged over %d counts to %d\n",r.actual_round_trip_times.size()+1,r.round_trip_time);
		}
		{
			//while (!r.actual_gather_times.empty() && current_frame-std::get<0>(r.actual_gather_times.front())>=15*30) r.actual_gather_times.pop_front();
			while (r.actual_gather_times.size()>=16) r.actual_gather_times.pop_front();
			int sum = (int)(r.u->type->is_gas ? 37*global_gas_gather_time_modifier : 84*global_minerals_gather_time_modifier);
			for (auto&v : r.actual_gather_times) sum += std::get<1>(v);
			r.gather_time = sum / (r.actual_gather_times.size()+1);
			//log("gather time averaged over %d counts to %d\n",r.actual_gather_times.size()+1,r.gather_time);
		}
	}
	if (!depot && (r.round_trip_time || !r.income_rate.empty())) {
		r.round_trip_time = 0;
		r.income_rate.clear();
		r.income_sum.clear();
	}

	if (r.last_update_income_rate<r.last_round_trip_time_calc && r.round_trip_time && r.depot) {
		r.last_update_income_rate = current_frame;
		int gather_time = std::max(r.gather_time,10);

		r.income_rate.clear();
		r.income_sum.clear();
		int t = r.round_trip_time + gather_time;
		double om = (double)t / (double)gather_time;
		double m = om;
		double sum = 0;
		while (m>0) {
			double inc = 8.0 / (double)t;
			if (m<1) inc *= m;
			m -= 1;
			sum += inc;
			r.income_rate.push_back(inc);
			r.income_sum.push_back(sum);
		}
		r.current_complete_round_trip_time = t + (int)(std::max(r.gatherers.size() - om,0.0)*gather_time);
		update_deliveries(r);
		//log("r.current_complete_round_trip_time is %d\n",r.current_complete_round_trip_time);
// 		double sum = 0;
// 		int t = r.round_trip_time + gather_time;
// 		for (int i=0;i<t;i+=gather_time) {
// 			double inc = 8.0 / (double)t;
// 			if (i>=r.round_trip_time) {
// 				int s = (i-r.round_trip_time);
// 				inc *= (double)(gather_time - s) / (double)gather_time;
// 			}
// 			sum += inc;
// 			r.income_rate.push_back(inc);
// 			r.income_sum.push_back(sum);
// 		}

// 		log("income rate is:");
// 		for (auto&v : r.income_rate) log(" %g",v);
// 		log("\n");
	}
	double next_income = 0;
	if (r.income_rate.size()>r.gatherers.size()) next_income = r.income_rate[r.gatherers.size()];

	if (my_workers.size() >= 15 && current_gas < 400) minerals_to_gas_weight = 0.25;
	if (my_workers.size() >= 30 && current_gas < 1000) minerals_to_gas_weight = 0.25;
	if (max_gas && current_gas >= max_gas) minerals_to_gas_weight = 100.0;

	if (next_income>0 && (current_frame-r.last_find_transfer>=30 || current_frame<30)) {
		r.last_find_transfer = current_frame;
		double next_income_weighted = r.u->type->is_gas ? next_income : next_income*minerals_to_gas_weight;
		gatherer_t*best = 0;
		double best_dist = std::numeric_limits<double>::infinity();
		for (auto&g : live_gatherers) {
			if (g.is_carrying) continue;
			double dist = units_pathing_distance(g.u,r.u) + units_pathing_distance(g.u->type,r.u,r.depot);
			if (dist==std::numeric_limits<double>::infinity()) continue;
			if (g.resource && g.resource->u->type->is_gas!=r.u->type->is_gas) {
				if (current_frame<no_mineral_gas_transfer_until) continue;
				if (g.no_mineral_gas_transfer) continue;
			}
			if (r.gatherers.size() >= 4) continue;
			double inc = 0;
			if (g.resource && g.resource->income_rate.size()>=g.resource->gatherers.size()) inc = g.resource->income_rate[g.resource->gatherers.size()-1];
			if (inc>0) {
				double inc_weighted = g.resource->u->type->is_gas ? inc : inc*minerals_to_gas_weight;
				//void*inc,*next_income;
				if (g.u->is_carrying_minerals_or_gas && inc_weighted>=next_income_weighted) continue;
				if (r.is_being_gathered && inc_weighted>=next_income_weighted) continue;
				if (r.gatherers.size()>=g.resource->gatherers.size() && inc_weighted>=next_income_weighted) continue;
// 				if (inc>=next_income*0.9375 && g.resource->u->resources/200>r.u->resources/200) continue;
// 				if (inc>=next_income*1.0625) continue;
				if (inc_weighted>=next_income_weighted*0.9375) continue;
// 				double dist2 = units_pathing_distance(g.u,g.resource->u) + units_pathing_distance(g.u->type,g.resource->u,g.resource->depot);
// 				//if (inc*(dist/dist2)>=next_income) continue;
// 				if (dist>=dist2) continue;
			}
			if (dist<best_dist) {
				best_dist = dist;
				best = &g;
			}
		}
		if (best) {
			log("transfer!\n");
			if (best->resource) {
				if (best->resource->u->type->is_gas!=u->type->is_gas) {
					no_mineral_gas_transfer_until = current_frame + 6;
					best->no_mineral_gas_transfer = true;
					log("transfer gas <-> minerals\n");
				}
				best->resource->gatherers.erase(std::find(best->resource->gatherers.begin(),best->resource->gatherers.end(),best));
				best->resource = 0;
			}
			r.gatherers.push_back(best);
			best->resource = &r;
			best->last_round_trip = 0;
			//best->last_delivery = 0;
			//update_deliveries(r);
		}
	}


	for (auto i=r.gatherers.begin();i!=r.gatherers.end();) {
		gatherer_t*g = *i;
		if (g->dead) {
			g->resource = 0;
			i = r.gatherers.erase(i);
		} else ++i;
	}

}

void process(gatherer_t&g) {
	unit*u = g.u;

	bool is_waiting = u->game_order==BWAPI::Orders::WaitForMinerals || u->game_order==BWAPI::Orders::WaitForGas;
	bool is_mining = u->game_order==BWAPI::Orders::MiningMinerals || u->game_order==BWAPI::Orders::HarvestGas;
	if (is_mining || is_waiting || (!u->is_carrying_minerals_or_gas && g.resource && units_distance(u,g.resource->u)<=3)) {
		if (g.last_round_trip) {
			int rt = current_frame-g.last_round_trip;
			if (g.round_trip_invalid) log("round trip (invalid) took %d frames\n", rt);
			else log("round trip took %d frames\n", rt);
			g.last_round_trip = 0;

			if (g.resource) {
				if (g.round_trip_invalid) g.round_trip_invalid = false;
				else g.resource->actual_round_trip_times.emplace_back(current_frame, rt);
			}
		}
	}
	if (is_mining!=g.is_mining) {
		if (!is_mining) g.last_round_trip = current_frame;
		g.is_mining = is_mining;
		g.is_mining_gas = u->game_order==BWAPI::Orders::HarvestGas;
		if (is_mining) {
			int f = (g.is_mining_gas ? 37 : 84);
			if (g.resource) {
				f = g.resource->gather_time;
				g.resource->busy_until = current_frame + f;
			}
			g.mining_until = current_frame + f;
			g.gather_begin = current_frame;
		} else {
			g.last_gather_end = current_frame;
			int gather_time = current_frame - g.gather_begin;
			log("gatherer took %d frames\n",gather_time);
			if (g.resource) {
				g.resource->actual_gather_times.emplace_back(current_frame,gather_time);
			}
		}
	}
	if (is_mining && g.mining_until<=current_frame) g.mining_until = current_frame + 4;
	if (is_mining) {
		u->controller->noorder_until = g.mining_until + 4;
	}

	bool is_carrying = u->game_unit->isCarryingMinerals() || u->game_unit->isCarryingGas();
	if (is_carrying!=g.is_carrying) {
		g.no_mineral_gas_transfer = false;
		g.is_carrying = is_carrying;
		if (!is_carrying) {
			g.last_delivery = current_frame;
			if (g.resource) {
				update_deliveries(*g.resource);
			}
		}
	}

	if (current_frame - g.last_find_depot >= 60) {
		g.last_find_depot = current_frame;

		unit*nearest_depot = get_best_score(resource_depots, [&](unit*du) {
			return units_pathing_distance(u, du);
		});
		if (nearest_depot != u->controller->depot) {
			u->controller->depot = nearest_depot;
			g.round_trip_invalid = true;
		}
	}

	if (g.resource && g.resource->dead) {
		g.resource->gatherers.erase(std::find(g.resource->gatherers.begin(),g.resource->gatherers.end(),&g));
		g.resource = 0;
	}
	if (g.resource) {
		if (u->controller->action == unit_controller::action_gather) {
			u->controller->target = g.resource->u;
		}
		if (!is_mining) u->controller->wait_until = g.resource->busy_until;
	} else {
		if (u->controller->action == unit_controller::action_gather) {
			u->controller->target = 0;
		}
		if (!u->is_carrying_minerals_or_gas) u->controller->depot = 0;
	}

// 	unit*u = g.u;
// 	u->game_unit->
// 	if (u->game_unit->isGatheringMinerals() || u->game_unit->isGatheringGas()) {
// 		if (g.begin_gather_frame==0) g.begin_gather_frame = current_frame;
// 	} else if (g.begin_gather_frame!=0) {
// 		log("took %d frames\n",current_frame-g.begin_gather_frame);
// 		if (!u->game_unit->isCarryingMinerals() && !u->game_unit->isCarryingGas()) log("aborted!\n");
// 		g.begin_gather_frame = 0;
// 	}

// 	log("max speed is %g, current speed is %g\n",g.u->stats->max_speed,g.u->speed);
// 
// 	log("acceleration is %g\n",g.u->stats->acceleration);
// 
// 	log("frames %g\n",frames_to_reach(g.u,128.0));
// 
// 	log("stop_distance is %g\n",g.u->stats->stop_distance);

	//log("speed %g mining %d\n",g.u->speed,g.u->game_order==BWAPI::Orders::MiningMinerals);

// 	log("speed %g distance %g latency %d remaining %d\n",g.u->speed,(g.test_goal-g.u->pos).length(),game->getLatencyFrames(),game->getRemainingLatencyFrames());
// 	if (g.u->speed==0 && (current_frame-g.test_begin>=15)) {
// 		if (g.test_end!=0) {
// 			log("expected %d, took %d\n",g.test_end-g.test_begin,current_frame-g.test_begin);
// 		}
// 		g.test_begin = current_frame;
// 		g.test_end = current_frame + (int)frames_to_reach(g.u,128.0);
// 		xy pos = g.u->pos + xy(0,128);
// 		g.test_goal = pos;
// 		g.u->game_unit->move(BWAPI::Position(pos.x,pos.y));
// 	}

// 	for (auto&r : all_resources) {
// 		r.u,pathing::get_distance(g.u->type,g.u->pos,r.u->pos);
// 		//log("%s %p is %g away\n",r.u->type->name,r.u,pathing::get_distance(g.u->type,g.u->pos,r.u->pos));
// 	}

}

void resource_gathering_task() {

	while (true) {

		resource_depots.clear();
		for (unit*u : my_units) {
			if (!u->type->game_unit_type.isResourceDepot()) continue;
			if (u->building && u->building->is_lifted) continue;
			if (!u->is_completed) continue;
			resource_depots.push_back(u);
		}

		for (unit*u : my_workers) {
			unit_controller*c = get_unit_controller(u);
			if (c->action==unit_controller::action_idle) {
				c->action = unit_controller::action_gather;
				c->target = 0;
				c->depot = 0;
			}
			if (c->action==unit_controller::action_gather) {
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
			if (u->resources==0) continue;
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

		for (auto i=live_resources.begin();i!=live_resources.end();) {
			//if (i->u->dead || i->u->resources==0) {
			if (i->live_frame!=current_frame) {
				log("resource %p dead!\n", i->u);
				i->dead = true;
				auto&v = *i;
				i = live_resources.erase(v);
				dead_resources.push_back(v);
				if (!v.deliveries.empty()) clear_deliveries(v);
			} else ++i;
		}

		for (auto i=live_gatherers.begin();i!=live_gatherers.end();) {
			//if (i->u->dead || i->u->controller->action!=unit_controller::action_gather) {
			if (i->live_frame!=current_frame) {
				log("gatherer %p dead!\n", i->u);
				i->last_round_trip = 0;
				i->dead = true;
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

		double calc_sum = 0;
		double calc_div = 0;
		for (auto&r : live_resources) {
			if (r.income_rate.empty()) continue;
			if (r.actual_round_trip_times.size()<=4) continue;
			if (r.round_trip_time==0) continue;
			calc_sum += (double)r.round_trip_time / (double)r.calculated_round_trip_time;
			calc_div += 1.0;
		}
		if (calc_div>0) {
			global_calculated_modifier = calc_sum/calc_div;
			//log("global calculated modifier is %g\n",global_calculated_modifier);
		}
		double gas_sum = 0, min_sum = 0;
		double gas_div = 0, min_div = 0;
		for (auto&r : live_resources) {
			if (r.income_rate.empty()) continue;
			if (r.actual_gather_times.size()<=4) continue;
			if (r.u->type->is_gas) {
				gas_sum += (double)r.gather_time / 37.0;
				gas_div += 1.0;
			} else {
				min_sum += (double)r.gather_time / 84.0;
				min_div += 1.0;
			}
		}
		if (gas_div>0) {
			global_gas_gather_time_modifier = gas_sum/gas_div;
			//log("global gas gather time modifier is %g\n",global_gas_gather_time_modifier);
		}
		if (min_div>0) {
			global_minerals_gather_time_modifier = min_sum/min_div;
			//log("global minerals gather time modifier is %g\n",global_minerals_gather_time_modifier);
		}

		double mpf = 0;
		double gpf = 0;
		for (auto&r : live_resources) {
			if (r.income_sum.empty()) continue;
			if (r.gatherers.empty()) continue;
			double inc = r.income_sum.back();
			if (r.gatherers.size()<=r.income_sum.size()) inc = r.income_sum[r.gatherers.size()-1];
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

	multitasking::spawn(resource_gathering_task,"resource gathering");

	render::add(render);

}

}

