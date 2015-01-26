


namespace resource_spots {
;

struct spot;
struct resource_t {
	std::pair<resource_t*,resource_t*> link, live_link;
	unit*u;
	bool dead;
	int live_frame;

	spot*s;
	std::array<double,3> full_income_workers;
	std::array<double,3> last_worker_mult;
	std::array<double,3> income_rate;
	std::array<double,3> income_sum;
};

a_list<resource_t> all_resources;
tsc::intrusive_list<resource_t,&resource_t::live_link> live_resources, dead_resources;
a_unordered_map<unit*,resource_t*> unit_resource_map;

struct spot: refcounted {

	xy pos;
	xy cc_build_pos;

	tsc::intrusive_list<resource_t,&resource_t::link> resources;

	std::array<std::vector<double>,3> min_income_rate, gas_income_rate;
	std::array<std::vector<double>,3> min_income_sum, gas_income_sum;
	std::array<double,3> total_min_income, total_gas_income;
	std::array<double,3> total_workers;
	spot() : total_min_income(), total_gas_income(), total_workers() {}
};

a_list<spot> spots;

void update_spots_pos() {

	for (auto&bs : grid::build_grid) {
		bs.reserved_for_resource_depot = false;
	}

	for (auto&s : spots) {
		if (s.cc_build_pos!=xy()) {
			s.cc_build_pos = xy();
		}
		if (s.resources.empty()) continue;
		xy pos;
		xy mpos, gpos;
		size_t mcount = 0, gcount = 0;
		for (auto&r : s.resources) {
			if (r.u->type->is_gas) ++gcount, gpos += r.u->pos;
			else ++mcount, mpos += r.u->pos;
		}
		if (mcount) mpos /= mcount;
		if (gcount) gpos /= gcount;
		if (!mcount) pos = gpos;
		else if (!gcount) pos = mpos;
		else pos = (gpos+mpos)/2;
		s.pos = pos;
		s.cc_build_pos = pos;

		a_deque<grid::build_square*> open;
		tsc::dynamic_bitset visited(grid::build_grid_width * grid::build_grid_height);

		open.push_back(&grid::get_build_square(pos));
		visited.set((unsigned)pos.x/32 + (unsigned)pos.y/32 * grid::build_grid_width);

		double best_dis;
		xy best_pos;
		while (!open.empty()) {
			grid::build_square&bs = *open.front();

			if (bs.buildable) {
				auto test = [&]() {
					if (bs.pos.x+32*4>=grid::map_width || bs.pos.y+32*3>=grid::map_height) return false;
					if (bs.building && bs.building->type->is_resource_depot && bs.building->building->build_pos == bs.pos) return true;
					for (int y=0;y<3;++y) {
						for (int x=0;x<4;++x) {
							grid::build_square&s = grid::get_build_square(bs.pos + xy(x*32,y*32));
							if (!s.buildable || s.no_resource_depot) return false;
							if (s.building) return false;
						}
					}
					return true;
				};
				if (test()) {
					double d = 0.0;
					for (auto&r : s.resources) {
						d += diag_distance(r.u->pos - (bs.pos + xy(16 * 4, 16 * 3)));
					}
					if (bs.building && bs.building->type->is_resource_depot && bs.building->building->build_pos == bs.pos && diag_distance(bs.pos - pos) < 32 * 10) {
						d /= 100;
					}
					if (best_pos==xy() || d<best_dis) {
						best_pos = bs.pos;
						best_dis = d;
					}
				}
			}

			open.pop_front();
			if (diag_distance(bs.pos - pos) >= 32 * 15) continue;
			//if (best_pos!=xy()) continue;
			auto add = [&](xy pos) {
				if (pos.x<0 || pos.y<0 || pos.x>=grid::map_width || pos.y>=grid::map_height) return;
				size_t v_idx = (unsigned)pos.x/32 + (unsigned)pos.y/32 * grid::build_grid_width;
				if (visited.test(v_idx)) return;
				visited.set(v_idx);
				auto&bs = grid::build_grid[v_idx];
				if (!bs.entirely_walkable) return;
				open.push_back(&bs);
			};
			add(bs.pos + xy(32,0));
			add(bs.pos + xy(0,32));
			add(bs.pos + xy(-32,0));
			add(bs.pos + xy(0,-32));
		}

		s.cc_build_pos = best_pos;
		s.pos = best_pos + xy(32*4,32*3)/2;

	}

	for (auto&s : spots) {
		if (s.cc_build_pos!=xy()) {
			for (int y = -1; y < 32 * (1 + 3 + 1); y += 32) {
				for (int x = -1; x < 32 * (1 + 4 + 1); x += 32) {
					grid::get_build_square(s.cc_build_pos + xy(x, y)).reserved_for_resource_depot = true;
				}
			}
		}
	}
}

void update_incomes() {

	for (auto&r : live_resources) {

		for (int i=0;i<3;++i) {

			xy depot_pos = r.s->pos;
			unit_type*depot_type = i==race_terran ? unit_types::cc : i==race_protoss ? unit_types::nexus : unit_types::hatchery;
			unit_type*worker_type = i==race_terran ? unit_types::scv : i==race_protoss ? unit_types::probe : unit_types::drone;

			int round_trip_time = 0;
			xy depot_top_left = depot_pos - xy(worker_type->dimension_right() + depot_type->dimension_left(),worker_type->dimension_down() + depot_type->dimension_up());
			xy depot_bottom_right = depot_pos + xy(worker_type->dimension_left() + depot_type->dimension_right(),worker_type->dimension_up() + depot_type->dimension_down());
			xy n_depot_pos = nearest_spot_in_square(r.u->pos,depot_top_left,depot_bottom_right);
			xy resource_top_left = r.u->pos - xy(worker_type->dimension_right() + r.u->type->dimension_left(),worker_type->dimension_down() + r.u->type->dimension_up());
			xy resource_bottom_right = r.u->pos + xy(worker_type->dimension_left() + r.u->type->dimension_right(),worker_type->dimension_up() + r.u->type->dimension_down());
			xy n_resource_pos = nearest_spot_in_square(depot_pos,resource_top_left,resource_bottom_right);
			round_trip_time = frames_to_reach(units::get_unit_stats(worker_type,players::my_player),0.0,(n_depot_pos - n_resource_pos).length()) * 2;
			round_trip_time += 15 + 4;

			round_trip_time = (int)(round_trip_time * resource_gathering::global_calculated_modifier);
			int gather_time = (int)(r.u->type->is_gas ? 37*resource_gathering::global_gas_gather_time_modifier : 84*resource_gathering::global_minerals_gather_time_modifier);

			int t = round_trip_time + gather_time;
			double om = (double)t / (double)gather_time;
			
			r.full_income_workers[i] = std::floor(om);
			r.last_worker_mult[i] = om - r.full_income_workers[i];
			r.income_rate[i] = 8.0 / (double)t;
			r.income_sum[i] = r.income_rate[i] * r.full_income_workers[i] + r.income_rate[i] * r.last_worker_mult[i];
		}

	}

	a_vector<std::pair<double,resource_t*>> tmp_vec;
	for (auto&s : spots) {
		for (int i=0;i<3;++i) {
			s.min_income_rate[i].clear();
			s.min_income_sum[i].clear();
			s.gas_income_rate[i].clear();
			s.gas_income_sum[i].clear();
			s.total_min_income[i] = 0;
			s.total_gas_income[i] = 0;
			s.total_workers[i] = 0;
			for (auto&r : s.resources) s.total_workers[i] += r.full_income_workers[i] + r.last_worker_mult[i];
			for (int i2=0;i2<2;++i2) {
				tmp_vec.clear();
				for (auto&r : s.resources) tmp_vec.emplace_back(i2==0 ? r.income_rate[i] : r.income_rate[i]*r.last_worker_mult[i],&r);
				std::sort(tmp_vec.begin(),tmp_vec.end(),[&](std::pair<double,resource_t*> a,std::pair<double,resource_t*>b) {return a.first>b.first;});
				for (auto&v : tmp_vec) {
					for (double n=0;n<v.second->full_income_workers[i];++n) {
						if (v.second->u->type->is_gas) {
							s.gas_income_rate[i].push_back(v.first);
							s.total_gas_income[i] += v.first;
							s.gas_income_sum[i].push_back(s.total_gas_income[i]);
						} else {
							s.min_income_rate[i].push_back(v.first);
							s.total_min_income[i] += v.first;
							s.min_income_sum[i].push_back(s.total_min_income[i]);
						}
					}
				}
			}
		}
	}

}

void update_spots() {

	int moves = 0;
	auto move = [&](resource_t*r,spot*to) {
		r->s->resources.erase(*r);
		to->resources.push_back(*r);
		r->s = to;
		++moves;
	};

	static int last_update = 0;
	if (moves || current_frame-last_update>=90 || current_frame<8) {
		if (true) {

			if (true) {
				auto&map = square_pathing::get_pathing_map(unit_types::scv);
				square_pathing::get_nearest_path_node(map, xy());
				if (!map.path_nodes.empty()) {
					for (auto&r : live_resources) {
						bool bad = false;
						for (auto&r2 : r.s->resources) {
							if (&r == &r2) continue;
							if (!square_pathing::unit_can_reach(unit_types::scv, r.u->pos, r2.u->pos)) {
								bad = true;
								break;
							}
						}
						if (bad) {
							spots.emplace_back();
							move(&r, &spots.back());
						}
					}
				}
			}

			a_vector<resource_t*> vec;
			for (auto&r : live_resources) vec.push_back(&r);
			std::sort(vec.begin(), vec.end(), [&](resource_t*a, resource_t*b) {
				return a->s < b->s;
			});

			a_vector<spot*> spot_at(grid::build_grid_width*grid::build_grid_height);

			tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
			a_deque<std::tuple<grid::build_square*, int>> open;
			for (auto*r : vec) {
				auto&start_bs = grid::get_build_square(r->u->building->build_pos);
				size_t start_index = grid::build_square_index(start_bs);
				bool can_move = true;
				while (true) {
					spot*s = r->s;
					visited.reset();
					open.clear();
					open.emplace_back(&start_bs, 0);
					visited.set(start_index);
					spot*move_to = nullptr;
					auto*&start_spot = spot_at[start_index];
					if (start_spot != s && can_move) move_to = start_spot;
					if (!start_spot) start_spot = s;
					while (!open.empty()) {
						grid::build_square*bs;
						int depth;
						std::tie(bs, depth) = open.front();
						open.pop_front();

						auto add = [&](int n) {
							if (move_to) return;
							auto*nbs = bs->get_neighbor(n);
							if (!nbs) return;
							if (!nbs->buildable) return;
							size_t index = grid::build_square_index(*nbs);
							if (visited.test(index)) return;
							visited.set(index);
							auto&n_spot = spot_at[index];
							if (n_spot && n_spot != s && can_move) {
								move_to = n_spot;
								return;
							}
							n_spot = s;
							if (depth < 7) open.emplace_back(nbs, depth + 1);
						};
						add(0);
						add(1);
						add(2);
						add(3);
					}
					if (move_to) {
						move(r, move_to);
						can_move = false;
						log("move resource %p to %p!\n", r, move_to);
					} else break;
				}
			}
		}
		update_spots_pos();
		update_incomes();
		last_update = current_frame;

		for (auto&s : spots) {
			for (auto&s2 : spots) {
				if (&s == &s2) continue;
				if (s.cc_build_pos.x < s2.cc_build_pos.x) continue;
				if (s.cc_build_pos.y < s2.cc_build_pos.y) continue;
				if (s.cc_build_pos.x >= s2.cc_build_pos.x + 32 * 4) continue;
				if (s.cc_build_pos.y >= s2.cc_build_pos.y + 32 * 3) continue;
				while (!s2.resources.empty()) move(&s2.resources.back(), &s);
			}
		}
	}

	for (auto i=spots.begin();i!=spots.end();) {
		if (i->resources.empty() && i->reference_count==0) i = spots.erase(i);
		else ++i;
	}
}

void resource_spots_task() {

	multitasking::sleep(2);

	bool first = true;

	while (true) {

		for (unit*u : resource_units) {
			if (u->resources==0) continue;
			resource_t*&r = unit_resource_map[u];
			bool is_new = false;
			if (!r) {
				if (u->resources<8*10) continue;
				all_resources.emplace_back();
				r = &all_resources.back();
				live_resources.push_back(*r);
				r->u = u;
				is_new = true;
			}
			if (r->dead) {
				r->dead = false;
				dead_resources.erase(*r);
				live_resources.push_back(*r);
				is_new = true;
			}
			r->live_frame = current_frame;
			if (is_new) {
				spots.emplace_back();
				spots.back().resources.push_back(*r);
				r->s = &spots.back();
			}
		}
		for (auto i=live_resources.begin();i!=live_resources.end();) {
			if (i->live_frame!=current_frame) {
				i->dead = true;
				auto&v = *i;
				i = live_resources.erase(v);
				dead_resources.push_back(v);
				v.s->resources.erase(v);
				v.s = nullptr;
			} else ++i;
		}
		if (first) {
			first = false;
			for (int i = 0; i < 100; ++i) {
				update_spots();
				multitasking::yield_point();
			}
		}

		update_spots();

		multitasking::sleep(1);

	}

}

void render() {

	for (auto&s : spots) {
		if (s.resources.empty()) continue;

		game->drawBoxMap(s.cc_build_pos.x,s.cc_build_pos.y,s.cc_build_pos.x+32*4,s.cc_build_pos.y+32*3,BWAPI::Colors::White);

		for (auto&r : s.resources) {
			game->drawLineMap(r.u->pos.x,r.u->pos.y,s.pos.x,s.pos.y,BWAPI::Colors::White);
		}

		game->drawTextMap(s.cc_build_pos.x, s.cc_build_pos.y - 16, "%p", &s);
		game->drawTextMap(s.cc_build_pos.x,s.cc_build_pos.y,"%g - %g / %g",s.total_workers[0],s.total_min_income[0],s.total_gas_income[0]);
		game->drawTextMap(s.cc_build_pos.x,s.cc_build_pos.y+16,"%g - %g / %g",s.total_workers[1],s.total_min_income[1],s.total_gas_income[1]);
		game->drawTextMap(s.cc_build_pos.x,s.cc_build_pos.y+32,"%g - %g / %g",s.total_workers[2],s.total_min_income[2],s.total_gas_income[2]);

	}

}


void init() {

	multitasking::spawn(resource_spots_task,"resource spots");

	render::add(render);

}

}


