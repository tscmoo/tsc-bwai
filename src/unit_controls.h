
struct unit_controller {
	unit*u = nullptr;
	enum { action_idle, action_move, action_gather, action_build, action_attack, action_scout, action_move_directly };
	int action = action_idle;
	xy go_to;
	unit*target = nullptr;
	unit*depot = nullptr;
	xy target_pos;
	unit_type*target_type = nullptr;
	void*flag = nullptr;

	bool can_move = false;
	int last_move = 0;
	xy last_move_to_pos;
	int last_move_to = 0;
	int move_order = 0;
	int noorder_until = 0;
	int wait_until = 0;
	int last_reached_go_to = 0;
};

namespace unit_controls {
	a_deque<unit_controller> unit_controller_container;
}

unit_controller*get_unit_controller(unit*u) {
	if (u->controller) return u->controller;
	unit_controls::unit_controller_container.emplace_back();
	unit_controller*c = &unit_controls::unit_controller_container.back();
	c->move_order = unit_controls::unit_controller_container.size();
	c->u = u;
	c->go_to = u->pos;
	c->action = unit_controller::action_idle;
	u->controller = c;
	return u->controller;
}

namespace unit_controls {
;

static const int move_resolution = 4;

void move(unit_controller*c) {

	unit*u = c->u;

	if (diag_distance(u->pos - c->go_to) <= 32)  c->last_reached_go_to = current_frame;

	xy move_to;
	if (u->type->is_flyer) {

		move_to = c->go_to;

	} else {

		move_to = square_pathing::get_move_to(u, c->go_to, c->last_reached_go_to);

		xy pos = square_pathing::get_pos_in_square(move_to, u->type);
		//if (pos == xy()) log(" !! move: get_pos_in_square for %s failed\n", u->type->name);
		if (pos != xy())  move_to = pos;
	}

	if (diag_distance(u->pos-move_to)<=8) move_to = c->go_to;
	
	//if (u->game_order != BWAPI::Orders::Move || u->game_unit->getOrderTargetPosition() != BWAPI::Position(move_to.x, move_to.y)) {
	if (c->last_move_to_pos != move_to || current_frame >= c->last_move_to + 30) {
		c->last_move_to = current_frame;
		c->last_move_to_pos = move_to;
		//if (u->game_unit->isSieged()) u->game_unit->unsiege();
		//else u->game_unit->move(BWAPI::Position(move_to.x, move_to.y));

		if (u->type->is_flyer) {
			xy relpos = move_to - u->pos;
			double a = atan2(relpos.y, relpos.x);
			double r = relpos.length();
			relpos.x = (int)(cos(a)*(r + 15 * 8));
			relpos.y = (int)(sin(a)*(r + 15 * 8));
			if ((size_t)(u->pos.x + relpos.x) < (size_t)grid::map_width && (size_t)(u->pos.y + relpos.y) < (size_t)grid::map_height) {
				move_to = u->pos + relpos;
			}
		}
		if ((u->pos - move_to).length() <= 16) {
			if (u->game_order != BWAPI::Orders::HoldPosition) {
				u->game_unit->holdPosition();
			}
		} else {
			if (u->type == unit_types::medic) {
				if (u->game_order != BWAPI::Orders::MedicHeal1 && u->game_order != BWAPI::Orders::MedicHeal2) {
					u->game_unit->useTech(upgrade_types::healing->game_tech_type, BWAPI::Position(move_to.x, move_to.y));
				}
			} else u->game_unit->move(BWAPI::Position(move_to.x, move_to.y));
		}

		c->noorder_until = current_frame + rng(8);
	}

}

void process(a_vector<unit_controller*>&controllers) {

	for (auto*c : controllers) {
		bool can_move = c->u->game_unit->canIssueCommand(BWAPI::UnitCommand::move(c->u->game_unit,c->u->game_unit->getPosition()));
		if (c->u->type->game_unit_type==BWAPI::UnitTypes::Zerg_Larva) can_move = false;
		c->can_move = can_move;
	}

	for (auto*c : controllers) {

		unit*u = c->u;
		if (u->building) continue;

		if (current_frame<c->noorder_until) continue;

// 		if (c->u->type == unit_types::barracks) {
// 			log("moo %d %d %d %d!\n", c->can_move, u->game_unit->canMove(), u->game_unit->isLifted(), u->type->game_unit_type.canMove());
// 			c->noorder_until = current_frame + 8;
// 			bool b = u->game_unit->move(BWAPI::Position(128, 128));
// 			BWAPI::Error e = game->getLastError();
// 			log("%d %d %s\n", b, e.getID(), e.getName());
// 		}

// 		if (c->action == unit_controller::action_idle) {
// 			c->last_move = current_frame;
// 			c->go_to = u->pos;
// 			move(c);
// 		}

		if (c->action==unit_controller::action_move && c->can_move && current_frame-c->last_move>=move_resolution) {
			c->last_move = current_frame;
			move(c);
		}
		if (c->action == unit_controller::action_move_directly && c->can_move && current_frame - c->last_move >= move_resolution) {
			c->last_move = current_frame;
			c->u->game_unit->move(BWAPI::Position(c->go_to.x, c->go_to.y));
			c->noorder_until = current_frame + rng(8);
		}

		if (c->action==unit_controller::action_gather) {

			auto o = u->game_order;
			if (u->game_unit->isCarryingMinerals() || u->game_unit->isCarryingGas()) {
				if (o!=BWAPI::Orders::ReturnMinerals && o!=BWAPI::Orders::ReturnGas) {
					u->game_unit->returnCargo();
					c->noorder_until = current_frame + 8;
				}
			} else {
				if (c->target && o!=BWAPI::Orders::MiningMinerals && o!=BWAPI::Orders::HarvestGas) {
					if ((o!=(c->target->type->is_gas ? BWAPI::Orders::MoveToGas : BWAPI::Orders::MoveToMinerals) && o!=(c->target->type->is_gas ? BWAPI::Orders::WaitForGas : BWAPI::Orders::WaitForMinerals)) || u->game_unit->getOrderTarget()!=c->target->game_unit) {
						if (current_frame+latency_frames<c->wait_until) {
							xy pos = u->pos;
							pos.x += (int)(u->hspeed * latency_frames);
							pos.y += (int)(u->vspeed * latency_frames);
							int f = frames_to_reach(u,units_distance(u,c->target));
							if (f>=c->wait_until || (f<c->wait_until-8)) {
								u->game_unit->gather(c->target->game_unit);
								c->noorder_until = current_frame + 3;
							}
						} else {
							if (!u->game_unit->gather(c->target->game_unit)) {
								c->go_to = c->target->pos;
								move(c);
								//u->game_unit->move(BWAPI::Position(c->target->pos.x,c->target->pos.y));
							} else log("gather failed\n");
							c->noorder_until = current_frame + 4;
							if (c->target->type->is_gas) c->noorder_until += 15;
						}
					}
				}
			}
// 			if (!c->target && !c->depot && o!=BWAPI::Orders::Move) {
// 				u->game_unit->move(BWAPI::Position(grid::map_width/2,grid::map_height/2));
// 				c->noorder_until = current_frame + 4;
// 			}

		}
		if (c->action==unit_controller::action_build) {

			if (c->target_type->is_building) {

				xy build_pos = c->target_pos;
				bool is_inside = u->pos>=build_pos && u->pos<(build_pos + xy(c->target_type->tile_width*32,c->target_type->tile_height*32));
				if (c->target_type->is_refinery) is_inside = u->pos>=build_pos-xy(32,32) && u->pos<(build_pos + xy(c->target_type->tile_width*32+32,c->target_type->tile_height*32+32));
				//if (c->target_type->is_refinery) is_inside = units_distance(u->pos,u->type,xy(c->target_type->tile_width*16,c->target_type->tile_height*16),c->target_type)<=32;
				if (c->target) {
					if (u->build_unit && u->build_unit!=c->target) {
						//log("building wrong thing, halt!\n");
						u->game_unit->haltConstruction();
						c->noorder_until = current_frame + 8;
					} else if (!u->build_unit) {
						if (units_pathing_distance(u,c->target)>=32) {
							//log("move to resume building!\n");
							c->go_to = c->target->pos;
							move(c);
							//u->game_unit->move(BWAPI::Position(c->go_to.x,c->go_to.y));
							c->noorder_until = current_frame + 5;
						} else {
							//log("right click!\n");
							u->game_unit->rightClick(c->target->game_unit);
							c->noorder_until = current_frame + 4;
						}
					}// else log("building, lalala\n");
				} else if (is_inside && current_frame+latency_frames>=c->wait_until) {
					//log("build!\n");
					bwapi_call_build(u->game_unit, c->target_type->game_unit_type, BWAPI::TilePosition(build_pos.x / 32, build_pos.y / 32));
					c->noorder_until = current_frame + 7;
				} else {
					//log("move to %d %d!\n",build_pos.x,build_pos.y);
					c->go_to = build_pos + xy(c->target_type->tile_width * 16, c->target_type->tile_height * 16);
					move(c);
					//u->game_unit->move(BWAPI::Position(c->go_to.x,c->go_to.y));
					c->noorder_until = current_frame + 5;
				}
			}
			else log("don't know how to build %s\n",c->target_type->name);

		} else if (u->build_unit && !u->is_being_constructed) {
			if (u->type==unit_types::scv) {
				log("is building something, but shouldn't be!\n");
				u->game_unit->haltConstruction();
				c->noorder_until = current_frame + 8;
			}
		}

		if (c->action == unit_controller::action_attack) {

			if (u->order_target != c->target || u->game_order != BWAPI::Orders::AttackUnit) {
				if (c->target == nullptr) xcept("attack null unit");
				weapon_stats*w = c->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
				weapon_stats*ew = u->is_flying ? c->target->stats->air_weapon : c->target->stats->ground_weapon;
				if (w) {
					double d = units_distance(u, c->target);
					if (d <= w->max_range + 32 || (ew && d <= ew->max_range + 32)) {
						if (u->type == unit_types::marine && u->owner->has_upgrade(upgrade_types::stim_packs)) {
							if (u->game_unit->getStimTimer() <= latency_frames) {
								u->game_unit->useTech(upgrade_types::stim_packs->game_tech_type);
							}
						}
					}
				}
				bool issue_attack = true;
				if (u->type == unit_types::medic) {
					issue_attack = false;
					if (u->game_order != BWAPI::Orders::MedicHeal1 && u->game_order != BWAPI::Orders::MedicHeal2) {
						if (!u->game_unit->useTech(upgrade_types::healing->game_tech_type, c->target->game_unit)) {
							c->go_to = c->target->pos;
							move(c);
						}
					}
				}
				if (issue_attack && !u->game_unit->attack(c->target->game_unit)) {
					xy order_pos(((bwapi_pos)u->game_unit->getOrderTargetPosition()).x, ((bwapi_pos)u->game_unit->getOrderTargetPosition()).y);
					if (u->game_order != BWAPI::Orders::AttackMove || diag_distance(order_pos - c->target->pos) >= 32 * 4) {
						u->game_unit->attack(BWAPI::Position(c->target->pos.x, c->target->pos.y));
					}
				}
			}

			c->noorder_until = current_frame + 4;
		}

		if (c->action == unit_controller::action_scout && c->can_move && current_frame - c->last_move >= move_resolution) {
			c->last_move = current_frame;
			move(c);
		}

	}

}

void control_units_task() {

	a_vector<unit_controller*> controllers;

	while (true) {

		controllers.clear();
		for (unit*u : my_units) {
			controllers.push_back(get_unit_controller(u));
		}

		process(controllers);


		multitasking::sleep(1);

	}

}


void render() {

	for (unit*u : my_units) {
		unit_controller*c = u->controller;
		if (!c || !c->can_move) continue;

		std::string s = "?";
		if (c->action == unit_controller::action_idle) s = "idle";
		if (c->action == unit_controller::action_move) s = "move";
		if (c->action == unit_controller::action_gather) s = "gather";
		if (c->action == unit_controller::action_build) s = "build";
		if (c->action == unit_controller::action_attack) s = "attack";
		if (c->action == unit_controller::action_scout) s = "scout";
		if (c->action == unit_controller::action_move_directly) s = "move directly";

		game->drawTextMap(u->pos.x-8,u->pos.y+8,"%s",s.c_str());
		
	}

	for (unit*u : my_units) {
		if (current_frame - u->controller->last_move_to < 15) {
			game->drawLineMap(u->pos.x, u->pos.y, u->controller->last_move_to_pos.x, u->controller->last_move_to_pos.y, BWAPI::Colors::White);
		}
	}

	for (unit*u : my_units) {
		if (current_frame - u->controller->last_move_to < 15 * 2) {
			game->drawLineMap(u->pos.x, u->pos.y, u->controller->go_to.x, u->controller->go_to.y, BWAPI::Colors::Purple);
		}
	}

// 	for (unit*u : my_units) {
// 		unit_controller*c = u->controller;
// 		if (!c || !c->can_move) continue;
// 
// 		game->drawLineMap(u->pos.x,u->pos.y,c->last_move_to_pos.x,c->last_move_to_pos.y,BWAPI::Colors::White);
// 	}

// 	for (int y=0;y<grid::walk_grid_height;++y) {
// 		for (int x=0;x<grid::walk_grid_width;++x) {
// 
// 			auto&v = force_field[x + y*grid::walk_grid_width];
// 			if (current_frame-std::get<1>(v)>60 || std::get<1>(v)==0) continue;
// 			xy force = std::get<0>(v);
// 			xy pos(x*8,y*8);
// 			xy fpos = pos + force;
// 			game->drawLineMap(pos.x+2,pos.y,fpos.x,fpos.y,BWAPI::Colors::Red);
// 			game->drawLineMap(pos.x,pos.y+2,fpos.x,fpos.y,BWAPI::Colors::Red);
// 			game->drawLineMap(pos.x-2,pos.y,fpos.x,fpos.y,BWAPI::Colors::Red);
// 			game->drawLineMap(pos.x,pos.y-2,fpos.x,fpos.y,BWAPI::Colors::Red);
// 
// 		}
// 	}

}



void init() {

	multitasking::spawn(control_units_task,"control units");

	render::add(render);

}

}



