


struct unit_type {
	BWAPI::UnitType game_unit_type;
	a_string name;
	// right, down, left, up
	std::array<int,4> dimensions;
	int dimension_right() {return dimensions[0];}
	int dimension_down() {return dimensions[1];}
	int dimension_left() {return dimensions[2];}
	int dimension_up() {return dimensions[3];}
	bool is_worker;
	bool is_minerals;
	bool is_gas;
	bool requires_creep, requires_pylon;
	int race;
	int tile_width, tile_height;
	double minerals_cost, gas_cost;
	double total_minerals_cost, total_gas_cost;
	int build_time;
	bool is_building;
	bool is_addon;
	bool is_resource_depot;
	bool is_refinery;
	unit_type*builder_type;
	a_vector<unit_type*> builds;
	bool build_near_me;
	a_vector<unit_type*> required_units, required_for;
	bool produces_land_units;
	bool needs_neighboring_free_space;
	double required_supply;
	double provided_supply;
	bool is_flyer;
	enum { size_none, size_small, size_medium, size_large };
	int size;
	bool is_two_units_in_one_egg;
	double width;
	int space_required;
	int space_provided;
	bool is_biological;
	bool is_mechanical;
	bool is_robotic;
	bool is_hovering;
	bool is_non_usable;
	bool is_detector;
	bool is_liftable;
};

struct weapon_stats {
	BWAPI::WeaponType game_weapon_type;
	player_t*player;

	double damage;
	int cooldown;
	double min_range;
	double max_range;
	bool targets_air;
	bool targets_ground;
	enum { damage_type_none, damage_type_concussive, damage_type_normal, damage_type_explosive };
	int damage_type;
	enum { explosion_type_none, explosion_type_radial_splash, explosion_type_enemy_splash };
	int explosion_type;
	double inner_splash_radius, median_splash_radius, outer_splash_radius;
};

struct unit_stats {
	unit_type*type;
	player_t*player;

	double max_speed;
	double acceleration;
	double stop_distance;
	double turn_rate;

	double energy;
	double shields;
	double hp;
	double armor;

	double sight_range;

	weapon_stats*ground_weapon;
	int ground_weapon_hits;
	weapon_stats*air_weapon;
	int air_weapon_hits;

};

struct unit_building {
	unit*u;
	bool is_lifted;
	xy walk_squares_occupied_pos;
	a_vector<grid::walk_square*> walk_squares_occupied;
	a_vector<grid::sixtyfour_square*> sixtyfour_squares_occupied;
	a_vector<grid::build_square*> build_squares_occupied;
	xy build_pos;
	xy last_registered_pos;
	int building_addon_frame;
	bool is_liftable_wall;
	int nobuild_until;
};

struct unit;
namespace units {
	a_vector<unit*> unit_containers[16];
}

a_vector<unit*>&all_units_ever = units::unit_containers[0];
a_vector<unit*>&live_units = units::unit_containers[1];
a_vector<unit*>&visible_units = units::unit_containers[2];
a_vector<unit*>&invisible_units = units::unit_containers[3];
a_vector<unit*>&visible_buildings = units::unit_containers[4];
a_vector<unit*>&resource_units = units::unit_containers[5];

a_vector<unit*>&my_units = units::unit_containers[6];
a_vector<unit*>&my_workers = units::unit_containers[7];
a_vector<unit*>&my_buildings = units::unit_containers[8];
a_vector<unit*>&my_resource_depots = units::unit_containers[9];
a_vector<unit*>&my_detector_units = units::unit_containers[10];

a_vector<unit*>&enemy_units = units::unit_containers[11];
a_vector<unit*>&visible_enemy_units = units::unit_containers[12];
a_vector<unit*>&enemy_buildings = units::unit_containers[13];
a_vector<unit*>&enemy_detector_units = units::unit_containers[14];

a_vector<unit*>&rescuable_units= units::unit_containers[15];

a_unordered_map<unit_type*, a_vector<unit*>> my_units_of_type;
a_unordered_map<unit_type*, a_vector<unit*>> my_completed_units_of_type;

struct unit_controller;
struct unit {
	BWAPI_Unit game_unit;
	player_t*owner;
	unit_type*type;
	unit_stats*stats;
	unit_building*building;
	unit_controller*controller;

	bool visible, dead;
	int last_seen, creation_frame;
	int last_shown;
	xy pos;
	bool gone;
	double speed, hspeed, vspeed;
	double heading;
	int resources, initial_resources;
	BWAPI::Order game_order;
	bool is_carrying_minerals_or_gas;
	bool is_being_constructed;
	bool is_completed;
	bool is_morphing;
	int remaining_build_time;
	int remaining_train_time;
	int remaining_research_time;
	int remaining_upgrade_time;
	int remaining_whatever_time;
	unit*build_unit;
	a_vector<unit_type*> train_queue;
	unit*addon;
	bool cloaked, detected;
	bool burrowed;
	bool invincible;

	double energy;
	double shields;
	double hp;
	int weapon_cooldown;

	unit*target;
	unit*order_target;

	double minerals_value;
	double gas_value;
	player_t*last_type_change_owner;

	bool force_combat_unit;
	int last_attacked;
	unit*last_attack_target;
	int prev_weapon_cooldown;

	a_vector<unit*> loaded_units;
	bool is_loaded;
	unit*loaded_into;
	int high_priority_until;
	bool is_flying;
	int spider_mine_count;
	bool has_nuke;
	int lockdown_timer;
	int stasis_timer;
	int defensive_matrix_timer;
	double defensive_matrix_hp;
	int irradiate_timer;
	int stim_timer;
	int plague_timer;
	int is_blind;
	int scan_me_until;
	bool is_powered;
	int marines_loaded;
	int strategy_high_priority_until;
	bool is_being_gathered;
	int is_being_gathered_begin_frame;

	std::array<size_t, std::extent<decltype(units::unit_containers)>::value> container_indexes;
};

namespace units {
	unit_type*get_unit_type(unit_type*&,BWAPI::UnitType);
}
namespace unit_types {
	void get(unit_type*&rv,BWAPI::UnitType game_unit_type) {
		units::get_unit_type(rv,game_unit_type);
	}
	typedef unit_type*unit_type_pointer;
	unit_type_pointer unknown;
	unit_type_pointer cc, supply_depot, barracks, factory, science_facility, nuclear_silo, bunker, refinery, machine_shop;
	unit_type_pointer missile_turret, academy, comsat_station, starport, control_tower, engineering_bay, armory, covert_ops;
	unit_type_pointer spell_scanner_sweep;
	unit_type_pointer scv, marine, vulture, siege_tank_tank_mode, siege_tank_siege_mode, goliath;
	unit_type_pointer medic, ghost, firebat;
	unit_type_pointer wraith, battlecruiser, dropship, science_vessel, valkyrie;
	unit_type_pointer spider_mine, nuclear_missile;
	unit_type_pointer nexus, pylon, gateway, photon_cannon, robotics_facility, stargate, forge, citadel_of_adun, templar_archives;
	unit_type_pointer fleet_beacon, assimilator, observatory, cybernetics_core;
	unit_type_pointer probe;
	unit_type_pointer zealot, dragoon, dark_templar, high_templar, reaver;
	unit_type_pointer archon, dark_archon;
	unit_type_pointer observer, shuttle, scout, carrier, interceptor, arbiter, corsair;
	unit_type_pointer hatchery, lair, hive, creep_colony, sunken_colony, spore_colony, nydus_canal, spawning_pool, evolution_chamber;
	unit_type_pointer hydralisk_den, spire, greater_spire, extractor, ultralisk_cavern, defiler_mound;
	unit_type_pointer drone, overlord, zergling, larva, egg, hydralisk, lurker, lurker_egg, ultralisk, defiler;
	unit_type_pointer mutalisk, cocoon, scourge, queen, guardian, devourer;
	unit_type_pointer spell_dark_swarm;
	unit_type_pointer vespene_geyser;
	std::reference_wrapper<unit_type_pointer> units_that_produce_land_units[] = {
		cc, barracks, factory,
		nexus, gateway, robotics_facility,
		hatchery, lair, hive
	};
	void init() {
		get(unknown, BWAPI::UnitTypes::Unknown);

		get(cc, BWAPI::UnitTypes::Terran_Command_Center);
		get(supply_depot, BWAPI::UnitTypes::Terran_Supply_Depot);
		get(barracks, BWAPI::UnitTypes::Terran_Barracks);
		get(factory, BWAPI::UnitTypes::Terran_Factory);
		get(science_facility, BWAPI::UnitTypes::Terran_Science_Facility);
		get(nuclear_silo, BWAPI::UnitTypes::Terran_Nuclear_Silo);
		get(bunker, BWAPI::UnitTypes::Terran_Bunker);
		get(refinery, BWAPI::UnitTypes::Terran_Refinery);
		get(machine_shop, BWAPI::UnitTypes::Terran_Machine_Shop);
		get(missile_turret, BWAPI::UnitTypes::Terran_Missile_Turret);
		get(academy, BWAPI::UnitTypes::Terran_Academy);
		get(comsat_station, BWAPI::UnitTypes::Terran_Comsat_Station);
		get(spell_scanner_sweep, BWAPI::UnitTypes::Spell_Scanner_Sweep);
		get(starport, BWAPI::UnitTypes::Terran_Starport);
		get(control_tower, BWAPI::UnitTypes::Terran_Control_Tower);
		get(engineering_bay, BWAPI::UnitTypes::Terran_Engineering_Bay);
		get(armory, BWAPI::UnitTypes::Terran_Armory);
		get(covert_ops, BWAPI::UnitTypes::Terran_Covert_Ops);

		get(scv, BWAPI::UnitTypes::Terran_SCV);
		get(marine, BWAPI::UnitTypes::Terran_Marine);
		get(medic, BWAPI::UnitTypes::Terran_Medic);
		get(ghost, BWAPI::UnitTypes::Terran_Ghost);
		get(firebat, BWAPI::UnitTypes::Terran_Firebat);
		get(vulture, BWAPI::UnitTypes::Terran_Vulture);
		get(siege_tank_tank_mode, BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);
		get(siege_tank_siege_mode, BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode);
		get(goliath, BWAPI::UnitTypes::Terran_Goliath);

		get(wraith, BWAPI::UnitTypes::Terran_Wraith);
		get(battlecruiser, BWAPI::UnitTypes::Terran_Battlecruiser);
		get(dropship, BWAPI::UnitTypes::Terran_Dropship);
		get(science_vessel, BWAPI::UnitTypes::Terran_Science_Vessel);
		get(valkyrie, BWAPI::UnitTypes::Terran_Valkyrie);

		get(spider_mine, BWAPI::UnitTypes::Terran_Vulture_Spider_Mine);
		get(nuclear_missile, BWAPI::UnitTypes::Terran_Nuclear_Missile);

		get(nexus, BWAPI::UnitTypes::Protoss_Nexus);
		get(pylon, BWAPI::UnitTypes::Protoss_Pylon);
		get(gateway, BWAPI::UnitTypes::Protoss_Gateway);
		get(photon_cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon);
		get(robotics_facility, BWAPI::UnitTypes::Protoss_Robotics_Facility);
		get(stargate, BWAPI::UnitTypes::Protoss_Stargate);
		get(forge, BWAPI::UnitTypes::Protoss_Forge);
		get(citadel_of_adun, BWAPI::UnitTypes::Protoss_Citadel_of_Adun);
		get(templar_archives, BWAPI::UnitTypes::Protoss_Templar_Archives);
		get(fleet_beacon, BWAPI::UnitTypes::Protoss_Fleet_Beacon);
		get(assimilator, BWAPI::UnitTypes::Protoss_Assimilator);
		get(observatory, BWAPI::UnitTypes::Protoss_Observatory);
		get(cybernetics_core, BWAPI::UnitTypes::Protoss_Cybernetics_Core);

		get(probe, BWAPI::UnitTypes::Protoss_Probe);
		get(zealot, BWAPI::UnitTypes::Protoss_Zealot);
		get(dragoon, BWAPI::UnitTypes::Protoss_Dragoon);
		get(dark_templar, BWAPI::UnitTypes::Protoss_Dark_Templar);
		get(high_templar, BWAPI::UnitTypes::Protoss_High_Templar);
		get(reaver, BWAPI::UnitTypes::Protoss_Reaver);
		get(archon, BWAPI::UnitTypes::Protoss_Archon);
		get(dark_archon, BWAPI::UnitTypes::Protoss_Dark_Archon);

		get(observer, BWAPI::UnitTypes::Protoss_Observer);
		get(shuttle, BWAPI::UnitTypes::Protoss_Shuttle);
		get(scout, BWAPI::UnitTypes::Protoss_Scout);
		get(carrier, BWAPI::UnitTypes::Protoss_Carrier);
		get(interceptor, BWAPI::UnitTypes::Protoss_Interceptor);
		get(arbiter, BWAPI::UnitTypes::Protoss_Arbiter);
		get(corsair, BWAPI::UnitTypes::Protoss_Corsair);

		get(larva, BWAPI::UnitTypes::Zerg_Larva);
		get(hatchery, BWAPI::UnitTypes::Zerg_Hatchery);
		get(lair, BWAPI::UnitTypes::Zerg_Lair);
		get(hive, BWAPI::UnitTypes::Zerg_Hive);
		get(creep_colony, BWAPI::UnitTypes::Zerg_Creep_Colony);
		get(sunken_colony, BWAPI::UnitTypes::Zerg_Sunken_Colony);
		get(spore_colony, BWAPI::UnitTypes::Zerg_Spore_Colony);
		get(nydus_canal, BWAPI::UnitTypes::Zerg_Nydus_Canal);
		get(spawning_pool, BWAPI::UnitTypes::Zerg_Spawning_Pool);
		get(evolution_chamber, BWAPI::UnitTypes::Zerg_Evolution_Chamber);
		get(hydralisk_den, BWAPI::UnitTypes::Zerg_Hydralisk_Den);
		get(spire, BWAPI::UnitTypes::Zerg_Spire);
		get(greater_spire, BWAPI::UnitTypes::Zerg_Greater_Spire);
		get(extractor, BWAPI::UnitTypes::Zerg_Extractor);
		get(ultralisk_cavern, BWAPI::UnitTypes::Zerg_Ultralisk_Cavern);
		get(defiler_mound, BWAPI::UnitTypes::Zerg_Defiler_Mound);

		get(drone, BWAPI::UnitTypes::Zerg_Drone);
		get(egg, BWAPI::UnitTypes::Zerg_Egg);
		get(overlord, BWAPI::UnitTypes::Zerg_Overlord);
		get(zergling, BWAPI::UnitTypes::Zerg_Zergling);
		get(hydralisk, BWAPI::UnitTypes::Zerg_Hydralisk);
		get(lurker, BWAPI::UnitTypes::Zerg_Lurker);
		get(lurker_egg, BWAPI::UnitTypes::Zerg_Lurker_Egg);
		get(ultralisk, BWAPI::UnitTypes::Zerg_Ultralisk);
		get(defiler, BWAPI::UnitTypes::Zerg_Defiler);

		get(mutalisk, BWAPI::UnitTypes::Zerg_Mutalisk);
		get(cocoon, BWAPI::UnitTypes::Zerg_Cocoon);
		get(scourge, BWAPI::UnitTypes::Zerg_Scourge);
		get(queen, BWAPI::UnitTypes::Zerg_Queen);
		get(guardian, BWAPI::UnitTypes::Zerg_Guardian);
		get(devourer, BWAPI::UnitTypes::Zerg_Devourer);

		get(spell_dark_swarm, BWAPI::UnitTypes::Spell_Dark_Swarm);

		get(vespene_geyser, BWAPI::UnitTypes::Resource_Vespene_Geyser);
	}
}

struct unit_controller;
unit_controller*get_unit_controller(unit*);
namespace units {
;

static const size_t npos = ~(size_t)0;

a_vector<unit*> new_units_queue;
a_vector<std::tuple<unit*, unit_type*>> unit_type_changes_queue;

void update_unit_container(unit*u,a_vector<unit*>&cont,bool contain) {
	size_t idx = &cont - &units::unit_containers[0];
	size_t&u_idx = u->container_indexes[idx];
	if (contain && u_idx==npos) {
		u_idx = cont.size();
		cont.push_back(u);
	}
	if (!contain && u_idx!=npos) {
		if (u_idx!=cont.size()-1) {
			cont[cont.size()-1]->container_indexes[idx] = u_idx;
			std::swap(cont[cont.size()-1],cont[u_idx]);
		}
		cont.pop_back();
		u_idx = npos;
	}
}


a_deque<unit> unit_container;
a_deque<unit_building> unit_building_container;

unit_stats*get_unit_stats(unit_type*type, player_t*player);
void update_unit_owner(unit*u) {
	u->owner = players::get_player(u->game_unit->getPlayer());
	if (u->type) u->stats = get_unit_stats(u->type, u->owner);
}

a_deque<unit_type> unit_type_container;
a_unordered_map<int,unit_type*> unit_type_map;

unit_type*get_unit_type(unit_type*&rv,BWAPI::UnitType game_unit_type);
unit_type*get_unit_type(BWAPI::UnitType game_unit_type) {
	unit_type*r;
	get_unit_type(r,game_unit_type);
	return r;
}
std::tuple<double, double> get_unit_value(unit_type*from, unit_type*to);
unit_type*new_unit_type(BWAPI::UnitType game_unit_type,unit_type*ut) {
	ut->game_unit_type = game_unit_type;
	ut->name = game_unit_type.getName().c_str();
	ut->dimensions[0] = game_unit_type.dimensionRight();
	ut->dimensions[1] = game_unit_type.dimensionDown();
	ut->dimensions[2] = game_unit_type.dimensionLeft();
	ut->dimensions[3] = game_unit_type.dimensionUp();
	ut->is_worker = game_unit_type.isWorker();
	ut->is_minerals = game_unit_type.isMineralField();
	ut->is_gas = game_unit_type==BWAPI::UnitTypes::Resource_Vespene_Geyser || game_unit_type==BWAPI::UnitTypes::Terran_Refinery || game_unit_type==BWAPI::UnitTypes::Protoss_Assimilator || game_unit_type==BWAPI::UnitTypes::Zerg_Extractor;
	ut->requires_creep = game_unit_type.requiresCreep();
	ut->requires_pylon = game_unit_type.requiresPsi();
	ut->race = game_unit_type.getRace()==BWAPI::Races::Terran ? race_terran : game_unit_type.getRace()==BWAPI::Races::Protoss ? race_protoss : game_unit_type.getRace()==BWAPI::Races::Zerg ? race_zerg : race_unknown;
	ut->tile_width = game_unit_type.tileWidth();
	ut->tile_height = game_unit_type.tileHeight();
	ut->minerals_cost = (double)game_unit_type.mineralPrice();
	ut->gas_cost = (double)game_unit_type.gasPrice();
	ut->build_time = game_unit_type.buildTime();
	ut->is_building = game_unit_type.isBuilding();
	ut->is_addon = game_unit_type.isAddon();
	ut->is_resource_depot = game_unit_type.isResourceDepot();
	ut->is_refinery = game_unit_type.isRefinery();
	ut->builder_type = get_unit_type(game_unit_type.whatBuilds().first);
	if (ut->builder_type) ut->builder_type->builds.push_back(ut);
	ut->is_two_units_in_one_egg = game_unit_type.isTwoUnitsInOneEgg();
	std::tie(ut->total_minerals_cost, ut->total_gas_cost) = get_unit_value(nullptr, ut);
	ut->build_near_me = ut->is_resource_depot || ut==unit_types::supply_depot || ut==unit_types::pylon || ut==unit_types::creep_colony || ut==unit_types::sunken_colony || ut==unit_types::spore_colony;
	for (auto&v : game_unit_type.requiredUnits()) {
		unit_type*t = get_unit_type(v.first);
		t->required_for.push_back(ut);
		ut->required_units.push_back(t);
		//if (v.second!=1) xcept("%s requires %d %s\n",ut->name,v.second,t->name);
	}
	ut->produces_land_units = false;
	for (auto&v : unit_types::units_that_produce_land_units) {
		if (v==ut) ut->produces_land_units = true;
	}
	if (ut->produces_land_units) log("%s produces land units!\n",ut->name);
	ut->needs_neighboring_free_space = ut->is_building && (ut->produces_land_units || ut==unit_types::nydus_canal || ut==unit_types::bunker);
	ut->required_supply = (double)game_unit_type.supplyRequired() / 2.0;
	ut->provided_supply = (double)game_unit_type.supplyProvided() / 2.0;
	ut->is_flyer = game_unit_type.isFlyer();
	ut->size = unit_type::size_none;
	if (game_unit_type.size().getID() == BWAPI::UnitSizeTypes::Small) ut->size = unit_type::size_small;
	if (game_unit_type.size().getID() == BWAPI::UnitSizeTypes::Medium) ut->size = unit_type::size_medium;
	if (game_unit_type.size().getID() == BWAPI::UnitSizeTypes::Large) ut->size = unit_type::size_large;
	ut->width = std::max(ut->dimensions[0] + 1 + ut->dimensions[2], ut->dimensions[1] + 1 + ut->dimensions[3]);
	ut->space_required = game_unit_type.spaceRequired();
	ut->space_provided = game_unit_type.spaceProvided();
	ut->is_biological = game_unit_type.isOrganic();
	ut->is_mechanical = game_unit_type.isMechanical();
	ut->is_robotic = game_unit_type.isRobotic();
	ut->is_hovering = ut->is_worker || ut == unit_types::vulture || ut == unit_types::archon || ut == unit_types::dark_archon;
	ut->is_non_usable = ut == unit_types::spider_mine || ut == unit_types::nuclear_missile || ut == unit_types::larva || ut == unit_types::egg || ut == unit_types::lurker_egg;
	ut->is_non_usable |= game_unit_type.maxHitPoints() <= 1;
	ut->is_detector = game_unit_type.isDetector();
	ut->is_liftable = game_unit_type.isFlyingBuilding();
	return ut;
}
unit_type*get_unit_type(unit_type*&rv,BWAPI::UnitType game_unit_type) {
	unit_type*&ut = unit_type_map[game_unit_type.getID()];
	if (ut) return rv=ut;
	if (game_unit_type == BWAPI::UnitTypes::None) return rv = nullptr;
	unit_type_container.emplace_back();
	rv = ut = &unit_type_container.back();
	return new_unit_type(game_unit_type,ut);
}

std::tuple<double, double> get_unit_value(unit_type*from, unit_type*to) {
	double minerals = 0.0;
	double gas = 0.0;
	if (from == unit_types::siege_tank_tank_mode && to == unit_types::siege_tank_siege_mode) return std::make_tuple(minerals, gas);
	if (from == unit_types::siege_tank_siege_mode && to == unit_types::siege_tank_tank_mode) return std::make_tuple(minerals, gas);
	log("update unit value for %s -> %s\n", from ? from->name : "null", to->name);
	if (to->race == race_zerg) {
		if (from == unit_types::larva) from = nullptr;
		auto trace_back = [&]() {
			for (unit_type*t = to; t->builder_type; t = t->builder_type) {
				if (t == from) {
					from = nullptr;
					break;
				}
				if (t == unit_types::larva) break;
				log(" +> %s - %g %g\n", t->name, t->minerals_cost, t->gas_cost);
				double mult = t->is_two_units_in_one_egg ? 0.5 : 1.0;
				minerals += t->minerals_cost * mult;
				gas += t->gas_cost * mult;
			}
		};
		trace_back();
		if (from) {
			log("maybe canceled?\n");
			unit_type*prev_from = from;
			double mult = from->is_building ? 0.75 : 1.0;
			minerals = -from->minerals_cost * mult;
			gas = -from->gas_cost * mult;
			from = from->builder_type;
			trace_back();
			if (from) from = prev_from;
		}
	} else {
		// TODO: archons, possibly other units?
		if (from == unit_types::siege_tank_siege_mode) from = unit_types::siege_tank_tank_mode;
		minerals = to->minerals_cost;
		gas = to->gas_cost;
	}
	// TODO: display an error on screen or something when this occurs
	//if (from != nullptr) xcept("unable to trace how %s changed into %s\n", from->name, to->name);
	if (from != nullptr) log(" !! error: unable to trace how %s changed into %s\n", from->name, to->name);
	log("final cost: %g %g\n", minerals, gas);
	return std::make_tuple(minerals, gas);
}

void update_unit_value(unit*u, unit_type*from, unit_type*to) {
	if (from == to) return;
	double min, gas;
	std::tie(min, gas) = get_unit_value(from, to);
	double free_min = 0;
	if (from == nullptr) {
		if (to->is_resource_depot && ++u->owner->resource_depots_seen<=1) {
			if (to->race == race_zerg) free_min += 300;
			else free_min += 400;
		}
		if (to->is_worker && ++u->owner->workers_seen <= 4) {
			free_min += 50;
		}
	}
	u->owner->minerals_spent += (min - free_min) - u->minerals_value;
	u->owner->gas_spent += gas - u->gas_value;
	u->minerals_value += (min - free_min);
	u->gas_value += gas;
}

void update_unit_type(unit*u) {
	unit_type*ut = get_unit_type(u->game_unit->getType());
	if (ut==u->type) return;
	if (u->last_type_change_owner && u->owner != u->last_type_change_owner && u->type!=unit_types::vespene_geyser) {
		u->minerals_value = 0;
		u->gas_value = 0;
	} else {
		if (ut == unit_types::vespene_geyser) {
			u->minerals_value = 0;
			u->gas_value = 0;
		} else update_unit_value(u, u->type == unit_types::vespene_geyser ? nullptr : u->type, ut);
	}
	if (u->type) unit_type_changes_queue.emplace_back(u, u->type);
	u->type = ut;
	u->stats = get_unit_stats(u->type, u->owner);
	if (ut->is_building) {
		if (!u->building) {
			unit_building_container.emplace_back();
			u->building = &unit_building_container.back();
			u->building->u = u;
			u->building->building_addon_frame = 0;
			u->building->is_liftable_wall = false;
			u->building->nobuild_until = 0;
		}
	} else u->building = nullptr;
}

a_deque<weapon_stats> weapon_stats_container;
a_map<std::tuple<BWAPI::WeaponType, player_t*>, weapon_stats*> weapon_stats_map;

void update_weapon_stats(weapon_stats*st) {

	auto&gw = st->game_weapon_type;
	auto&gp = st->player->game_player;

	//st->damage = gp->damage(gw);
	st->damage = gw.damageAmount() + (gp->getUpgradeLevel(gw.upgradeType())*gw.damageBonus())*gw.damageFactor();
	st->cooldown = gw.damageCooldown(); // FIXME: adrenal glands
	st->min_range = gw.minRange();
	st->max_range = gp->weaponMaxRange(gw);
	st->targets_air = gw.targetsAir();
	st->targets_ground = gw.targetsGround();
	st->damage_type = weapon_stats::damage_type_none;
	if (gw.damageType() == BWAPI::DamageTypes::Concussive) st->damage_type = weapon_stats::damage_type_concussive;
	if (gw.damageType() == BWAPI::DamageTypes::Normal) st->damage_type = weapon_stats::damage_type_normal;
	if (gw.damageType() == BWAPI::DamageTypes::Explosive) st->damage_type = weapon_stats::damage_type_explosive;
	st->explosion_type = weapon_stats::explosion_type_none;
	if (gw.explosionType() == BWAPI::ExplosionTypes::Radial_Splash) st->explosion_type = weapon_stats::explosion_type_radial_splash;
	if (gw.explosionType() == BWAPI::ExplosionTypes::Enemy_Splash) st->explosion_type = weapon_stats::explosion_type_enemy_splash;
	st->inner_splash_radius = gw.innerSplashRadius();
	st->median_splash_radius = gw.medianSplashRadius();
	st->outer_splash_radius = gw.outerSplashRadius();
}

weapon_stats*get_weapon_stats(BWAPI::WeaponType type, player_t*player) {
	weapon_stats*&st = weapon_stats_map[std::make_tuple(type, player)];
	if (st) return st;
	weapon_stats_container.emplace_back();
	st = &weapon_stats_container.back();
	st->game_weapon_type = type;
	st->player = player;
	update_weapon_stats(st);
	return st;
};

a_deque<unit_stats> unit_stats_container;
a_map<std::tuple<unit_type*,player_t*>,unit_stats*> unit_stats_map;

void update_stats(unit_stats*st) {

	auto&gp = st->player->game_player;
	auto&gu = st->type->game_unit_type;

	st->max_speed = std::ceil(gp->topSpeed(gu) * 256) / 256.0;
	st->max_speed += 20.0 / 256.0; // Verify this: some(all?) speeds in BWAPI seem to be too low by this amount.
	int acc = gu.acceleration();
	st->acceleration = (double)acc / 256.0;
	if (st->acceleration == 0 || acc == 1) st->acceleration = st->max_speed;
	st->stop_distance = (double)gu.haltDistance() / 256.0;
	st->turn_rate = (double)gu.turnRadius() / 128.0 * PI;

	st->energy = (double)gp->maxEnergy(gu);
	st->shields = (double)gu.maxShields();
	st->hp = (double)gu.maxHitPoints();
	st->armor = (double)gp->armor(gu);

	st->sight_range = (double)gp->sightRange(gu);

	st->ground_weapon_hits = gu.maxGroundHits();
	if (st->ground_weapon_hits == 0 && st->ground_weapon) {
		log("warning: %s.maxGroundHits() returned 0 (setting to 1)\n", st->type->name);
		st->ground_weapon_hits = 1;
	}
	st->air_weapon_hits = gu.maxAirHits();
	if (st->air_weapon_hits == 0 && st->air_weapon) {
		log("warning: %s.maxAirHits() returned 0 (setting to 1)\n", st->type->name);
		st->air_weapon_hits = 1;
	}
	if (st->ground_weapon) update_weapon_stats(st->ground_weapon);
	if (st->air_weapon) update_weapon_stats(st->air_weapon);
}

void update_all_stats() {
	for (auto&v : unit_stats_container) update_stats(&v);
}

unit_stats*get_unit_stats(unit_type*type,player_t*player) {
	unit_stats*&st = unit_stats_map[std::make_tuple(type,player)];
	if (st) return st;
	unit_stats_container.emplace_back();
	st = &unit_stats_container.back();
	st->type = type;
	st->player = player;
	st->ground_weapon = nullptr;
	st->air_weapon = nullptr;
	auto gw = st->type->game_unit_type.groundWeapon();
	if (gw != BWAPI::WeaponTypes::None) st->ground_weapon = get_weapon_stats(gw, player);
	auto aw = st->type->game_unit_type.airWeapon();
	if (aw != BWAPI::WeaponTypes::None) st->air_weapon = get_weapon_stats(aw, player);
	update_stats(st);
	return st;
}

unit*get_unit(BWAPI_Unit game_unit);
void update_unit_stuff(unit*u) {
	bwapi_pos position = u->game_unit->getPosition();
	if (!u->game_unit->exists()) xcept("attempt to update inaccessible unit");
	if (u->game_unit->getID() < 0) xcept("(update) %s->getId() is %d\n", u->type->name, u->game_unit->getID());
	if (u->visible != u->game_unit->isVisible()) xcept("visible mismatch in unit %s (%d vs %d)", u->type->name, u->visible, u->game_unit->isVisible());
	if ((size_t)position.x >= (size_t)grid::map_width || (size_t)position.y >= (size_t)grid::map_height) xcept("unit %s is outside map", u->type->name);
	u->pos.x = position.x;
	u->pos.y = position.y;
	u->hspeed = u->game_unit->getVelocityX();
	u->vspeed = u->game_unit->getVelocityY();
	u->speed = xy_typed<double>(u->hspeed,u->vspeed).length();
	u->heading = u->game_unit->getAngle();
	u->resources = u->game_unit->getResources();
	u->initial_resources = u->game_unit->getInitialResources();
	u->game_order = u->game_unit->getOrder();
	u->is_carrying_minerals_or_gas = u->game_unit->isCarryingMinerals() || u->game_unit->isCarryingGas();
	u->is_being_constructed = u->game_unit->isBeingConstructed();
	u->is_completed = u->game_unit->isCompleted();
	u->is_morphing = u->game_unit->isMorphing();
	u->remaining_build_time = u->game_unit->getRemainingBuildTime();
	u->remaining_train_time = u->game_unit->getRemainingTrainTime();
	u->remaining_research_time = u->game_unit->getRemainingResearchTime();
	u->remaining_upgrade_time = u->game_unit->getRemainingUpgradeTime();
	u->remaining_whatever_time = std::max(u->remaining_build_time,std::max(u->remaining_train_time,std::max(u->remaining_research_time,u->remaining_upgrade_time)));
	u->build_unit = u->game_unit->getBuildUnit() ? (unit*)u->game_unit->getBuildUnit()->getClientInfo() : nullptr;
	u->train_queue.clear();
	for (auto&v : u->game_unit->getTrainingQueue()) {
		u->train_queue.push_back(get_unit_type(v));
	}
	u->addon = u->game_unit->getAddon() ? get_unit(u->game_unit->getAddon()) : nullptr;
	
	u->cloaked = u->game_unit->isCloaked() || u->game_unit->isBurrowed();
	u->burrowed = u->game_unit->isBurrowed();
	u->detected = u->game_unit->isDetected();
	u->invincible = u->game_unit->isInvincible();

	if (!u->cloaked || u->detected) {
		u->energy = u->game_unit->getEnergy();
		u->shields = u->game_unit->getShields();
		u->hp = u->game_unit->getHitPoints();
	}
	u->prev_weapon_cooldown = u->weapon_cooldown;
	u->weapon_cooldown = std::max(u->game_unit->getGroundWeaponCooldown(), u->game_unit->getAirWeaponCooldown());

	auto targetunit = [&](BWAPI_Unit gu) -> unit* {
		if (!gu) return nullptr;
		if (!gu->isVisible()) return nullptr;
		if (gu->getID() < 0) xcept("(target) %s->getId() is %d\n", gu->getType().getName(), gu->getID());
		return get_unit(gu);
	};
	u->target = targetunit(u->game_unit->getTarget());
	u->order_target = targetunit(u->game_unit->getOrderTarget());

	if (u->weapon_cooldown > u->prev_weapon_cooldown) u->last_attacked = current_frame;
	if (u->game_order == BWAPI::Orders::AttackUnit || u->last_attacked == current_frame) u->last_attack_target = u->order_target;

	u->loaded_units.clear();
	for (auto&gu : u->game_unit->getLoadedUnits()) {
		u->loaded_units.push_back(get_unit(gu));
	}
	u->is_loaded = u->game_unit->isLoaded();
	u->loaded_into = u->game_unit->getTransport() ? get_unit(u->game_unit->getTransport()) : nullptr;
	if (u->loaded_into) u->pos = u->loaded_into->pos;
	u->is_flying = u->type->is_flyer || u->game_unit->isLifted();
	u->spider_mine_count = u->game_unit->getSpiderMineCount();
	u->has_nuke = u->game_unit->hasNuke();

	// These timer functions seem to return a value in multiple of 8 frames
	// (the timer is decremented every 8 frames)
	u->lockdown_timer = u->game_unit->getLockdownTimer() * 8;
	u->stasis_timer = u->game_unit->getStasisTimer() * 8;
	u->defensive_matrix_timer = u->game_unit->getDefenseMatrixTimer() * 8;
	u->defensive_matrix_hp = u->game_unit->getDefenseMatrixPoints();
	u->irradiate_timer = u->game_unit->getIrradiateTimer() * 8;
	u->stim_timer = u->game_unit->getStimTimer() * 8;
	u->plague_timer = u->game_unit->getPlagueTimer() * 8;
	u->is_blind = u->game_unit->isBlind();
	
	u->is_powered = bwapi_is_powered(u->game_unit);

	bool is_being_gathered = u->game_unit->isBeingGathered();
	if (is_being_gathered && !u->is_being_gathered) u->is_being_gathered_begin_frame = current_frame;
	u->is_being_gathered = is_being_gathered;

	unit_building*b = u->building;
	if (b) {
		b->is_lifted = u->game_unit->isLifted();
		bwapi_pos tile_pos = u->game_unit->getTilePosition();
		if ((size_t)tile_pos.x >= (size_t)grid::build_grid_width || (size_t)tile_pos.y >= (size_t)grid::build_grid_height) xcept("unit %s has tile pos outside map", u->type->name);
		if ((size_t)tile_pos.x + u->type->tile_width > (size_t)grid::build_grid_width || (size_t)tile_pos.y + u->type->tile_height > (size_t)grid::build_grid_height) xcept("unit %s has tile pos outside map", u->type->name);
		b->build_pos.x = tile_pos.x*32;
		b->build_pos.y = tile_pos.y*32;
		if (b->last_registered_pos == xy()) b->last_registered_pos = b->build_pos;
	}
}

unit*new_unit(BWAPI_Unit game_unit) {
	unit_container.emplace_back();
	unit*u = &unit_container.back();
	game_unit->setClientInfo(u);

	new_units_queue.push_back(u);

	u->game_unit = game_unit;
	u->owner = nullptr;
	u->type = nullptr;
	u->stats = nullptr;
	u->building = nullptr;
	u->controller = nullptr;
	u->last_type_change_owner = nullptr;

	u->visible = false;
	u->dead = false;
	u->creation_frame = current_frame;
	u->last_seen = current_frame;
	u->gone = false;
	u->last_shown = current_frame;

	u->force_combat_unit = false;
	//u->last_attacking = 0;
	u->last_attacked = 0;
	u->last_attack_target = nullptr;
	u->prev_weapon_cooldown = 0;

	u->container_indexes.fill(npos);

	u->minerals_value = 0;
	u->gas_value = 0;
	u->high_priority_until = 0;
	u->scan_me_until = 0;
	u->strategy_high_priority_until = 0;
	u->is_being_gathered = false;
	u->is_being_gathered_begin_frame = 0;

	update_unit_owner(u);
	update_unit_type(u);

	u->energy = u->stats->energy;
	u->shields = u->stats->shields;
	u->hp = u->stats->hp;
	u->visible = game_unit->isVisible();
	update_unit_stuff(u);

	return u;
}

unit*get_unit(BWAPI_Unit game_unit) {
	unit*u = (unit*)game_unit->getClientInfo();
	if (!u) {
		if (!game_unit->exists()) return nullptr;
		u = new_unit(game_unit);
	}
	return u;
}

struct event_t {
	enum {t_show, t_hide, t_create, t_morph, t_destroy, t_renegade, t_complete, t_refresh};
	int t;
	BWAPI_Unit game_unit;
	event_t(int t, BWAPI_Unit game_unit) : t(t), game_unit(game_unit) {}
};
a_deque<event_t> events;

void on_unit_show(BWAPI_Unit game_unit) {
	events.emplace_back(event_t::t_show,game_unit);
}
void on_unit_hide(BWAPI_Unit game_unit) {
	events.emplace_back(event_t::t_hide,game_unit);
}
void on_unit_create(BWAPI_Unit game_unit) {
	events.emplace_back(event_t::t_create,game_unit);
}
void on_unit_morph(BWAPI_Unit game_unit) {
	events.emplace_back(event_t::t_morph,game_unit);
}
void on_unit_destroy(BWAPI_Unit game_unit) {
	events.emplace_back(event_t::t_destroy,game_unit);
}
void on_unit_renegade(BWAPI_Unit game_unit) {
	events.emplace_back(event_t::t_renegade,game_unit);
}
void on_unit_complete(BWAPI_Unit game_unit) {
	events.emplace_back(event_t::t_complete,game_unit);
}

a_vector<unit_building*> registered_buildings;
void update_building_squares() {
	for (size_t i = 0; i < registered_buildings.size(); ++i) {
		unit_building*b = registered_buildings[i];
		unit*u = b->u;
		if (b->u->building != b || b->is_lifted || u->pos != b->walk_squares_occupied_pos || u->gone || u->dead) {
			//++pathing::update_spaces;
			log("unregistered %s, because %s, %d squares\n", u->type->name, b->u->building != b ? "unit changed" : b->is_lifted ? "lifted" : u->pos != b->walk_squares_occupied_pos ? "moved" : u->gone ? "gone" : u->dead ? "dead" : "?", b->walk_squares_occupied.size());
			for (auto*ws_p : b->walk_squares_occupied) {
				find_and_erase(ws_p->buildings, u);
				//if (ws_p->building==u) ws_p->building = 0;
			}
			b->walk_squares_occupied.clear();
			for (auto*sfs_p : b->sixtyfour_squares_occupied) {
				//while (!sfs_p->distance_map_used.empty()) pathing::invalidate_distance_map(sfs_p->distance_map_used.front());
			}
			b->sixtyfour_squares_occupied.clear();
			for (auto*bs_p : b->build_squares_occupied) {
				if (bs_p->building == u) bs_p->building = 0;
			}
			b->build_squares_occupied.clear();
			xy upper_left = u->pos - xy(u->type->dimension_left(), u->type->dimension_up());
			xy bottom_right = u->pos + xy(u->type->dimension_right(), u->type->dimension_down());
			square_pathing::invalidate_area(upper_left, bottom_right);
			if (i != registered_buildings.size() - 1) std::swap(registered_buildings[registered_buildings.size() - 1], registered_buildings[i]);
			registered_buildings.pop_back();
			--i;
		} else {
			for (auto*ws_p : b->walk_squares_occupied) {
				//if (ws_p->building!=u) ws_p->building = u;
			}
			for (auto*bs_p : b->build_squares_occupied) {
				if (bs_p->building != u) bs_p->building = u;
			}
		}
	}

	for (unit*u : visible_buildings) {
		unit_building*b = u->building;
		if (!b) continue;
		if (b->is_lifted) continue;

		if (b->walk_squares_occupied.empty()) {
			//++pathing::update_spaces;
			xy upper_left = u->pos - xy(u->type->dimension_left(), u->type->dimension_up());
			xy bottom_right = u->pos + xy(u->type->dimension_right(), u->type->dimension_down());
			for (int y = upper_left.y&-8; y <= bottom_right.y; y += 8) {
				if ((size_t)y >= (size_t)grid::map_height) continue;
				for (int x = upper_left.x&-8; x <= bottom_right.x; x += 8) {
					if ((size_t)x >= (size_t)grid::map_width) continue;
					auto&ws = grid::get_walk_square(xy(x, y));
					b->walk_squares_occupied.push_back(&ws);
					//ws.building = u;
					ws.buildings.push_back(u);
				}
			}
			for (int y = upper_left.y&-64 - 64; y <= bottom_right.y + 64; y += 64) {
				if ((size_t)y >= (size_t)grid::map_height) continue;
				for (int x = upper_left.x&-64 - 64; x <= bottom_right.x + 64; x += 64) {
					if ((size_t)x >= (size_t)grid::map_width) continue;
					auto&sfs = grid::get_sixtyfour_square(xy(x, y));
					b->sixtyfour_squares_occupied.push_back(&sfs);
					//while (!sfs.distance_map_used.empty()) pathing::invalidate_distance_map(sfs.distance_map_used.front());
				}
			}
			for (int y = 0; y < u->type->tile_height; ++y) {
				for (int x = 0; x < u->type->tile_width; ++x) {
					auto&bs = grid::get_build_square(u->building->build_pos + xy(x * 32, y * 32));
					b->build_squares_occupied.push_back(&bs);
					bs.building = u;
				}
			}
			square_pathing::invalidate_area(upper_left, bottom_right);
			registered_buildings.push_back(b);
			b->walk_squares_occupied_pos = u->pos;
			b->last_registered_pos = b->build_pos;
			log("registered %s, %d squares\n", u->type->name, b->walk_squares_occupied.size());
		}
	}
}

void update_buildings_squares_task() {

	while (true) {

		update_building_squares();

		multitasking::sleep(4);
	}

}

a_unordered_map<BWAPI_Bullet, int> bullet_timestamps;
void update_projectile_stuff_task() {
	while (true) {
		a_vector<unit*> bunkers;
		for (unit*u : enemy_buildings) {
			if (!u->is_completed) continue;
			if (u->type == unit_types::bunker) {
				double d = get_best_score_value(my_units, [&](unit*mu) {
					return units_distance(mu, u);
				}, std::numeric_limits<double>::infinity());
				if (d <= 32 * 5) {
					bunkers.push_back(u);
					u->marines_loaded = 0;
				}
			}
		}
		for (BWAPI_Bullet b : game->getBullets()) {
			if (b->getType() == BWAPI::BulletTypes::Gauss_Rifle_Hit && b->getRemoveTimer() > 4) {
				int&ts = bullet_timestamps[b];
				if (!ts || current_frame - ts > 15) ts = current_frame;
				if (b->getTarget() && b->getTarget()->getPlayer() == players::my_player->game_player && current_frame - ts < 15) {
					unit*target = get_unit(b->getTarget());
					if (target) {
						unit*bunker = get_best_score(bunkers, [&](unit*u) {
							if (u->marines_loaded >= 4) return std::numeric_limits<double>::infinity();
							double d = units_distance(target, u);
							if (d >= 32 * 5 + 32) return std::numeric_limits<double>::infinity();
							return d;
						}, std::numeric_limits<double>::infinity());
						if (bunker) {
							++bunker->marines_loaded;
						}
					}
				}
			}
		}
		for (unit*u : enemy_buildings) {
			if (!u->is_completed) continue;
			if (u->type == unit_types::bunker) {
				log("bunker %p has %d marines\n", u, u->marines_loaded);
			}
		}

		multitasking::sleep(1);
	}
}

a_vector<std::function<void(unit*)>> on_create_callbacks, on_morph_callbacks, on_destroy_callbacks;

a_vector<std::function<void(unit*)>> on_new_unit_callbacks;
a_vector<std::function<void(unit*,unit_type*)>> on_type_change_callbacks;

bool dont_call_gg = false;

void update_units_task() {

	int last_update_stats = 0;
	int last_refresh = 0;

	a_vector<unit*> created_units, morphed_units, destroyed_units;

	auto update_groups = [](unit*u) {
		update_unit_container(u, all_units_ever, true);
		update_unit_container(u, live_units, !u->dead);
		update_unit_container(u, visible_units, u->visible);
		update_unit_container(u, invisible_units, !u->dead && !u->visible);
		update_unit_container(u, visible_buildings, u->visible && u->building);
		update_unit_container(u, resource_units, !u->dead && !u->gone && (u->type->is_minerals || u->type->is_gas));

		update_unit_container(u, my_units, u->visible && u->owner == players::my_player);
		update_unit_container(u, my_workers, u->visible && u->owner == players::my_player && u->type->is_worker && u->is_completed && !u->is_morphing);
		update_unit_container(u, my_buildings, u->visible && u->owner == players::my_player && u->building);
		update_unit_container(u, my_resource_depots, u->visible && u->owner == players::my_player && u->type->is_resource_depot);
		update_unit_container(u, my_detector_units, u->visible && u->owner == players::my_player && u->type->is_detector);

		update_unit_container(u, enemy_units, !u->dead && u->owner->is_enemy);
		update_unit_container(u, visible_enemy_units, u->visible && u->owner->is_enemy);
		update_unit_container(u, enemy_buildings, !u->dead && !u->gone && u->building && u->owner->is_enemy);
		update_unit_container(u, enemy_detector_units, !u->dead && u->owner->is_enemy && u->type->is_detector);

		update_unit_container(u, rescuable_units, !u->dead && u->owner->rescuable);

		my_units_of_type.clear();
		for (unit*u : my_units) {
			my_units_of_type[u->type].push_back(u);
		}
		my_completed_units_of_type.clear();
		for (unit*u : my_units) {
			if (!u->is_completed) continue;
			my_completed_units_of_type[u->type].push_back(u);
		}

		if (u->owner == players::my_player && !u->controller) get_unit_controller(u);
	};

	while (true) {

		grid::update_build_grid();

		while (!events.empty()) {
			auto e = events.front();
			events.pop_front();
			unit*u = get_unit(e.game_unit);
			if (!u) {
				log("missed new unit %s\n", e.game_unit->getType().getName());
				continue;
			}
			// These events are delayed, so e.g. a unit is not necessarily visible even though we are processing a show event.
			u->visible = u->game_unit->isVisible();
			u->visible &= u->game_unit->exists(); // BWAPI bug: destroyed units are visible
			auto*b = u->building;
			switch (e.t) {
			case event_t::t_show:
				log("show %s\n",u->type->name);
				if (u->visible) update_unit_owner(u);
				if (u->visible) update_unit_type(u);
				u->last_shown = current_frame;
				update_stats(u->stats);
				break;
			case event_t::t_hide:
				log("hide %s\n",u->type->name);
				// the unit is still visible, but will be invisible the next frame
				if (u->visible) update_unit_stuff(u);
				u->visible = false;
				break;
			case event_t::t_create:
				created_units.push_back(u);
				break;
			case event_t::t_morph:
				log("morph %s -> %s\n", u->type->name, u->game_unit->getType().getName());
				if (u->visible) update_unit_owner(u);
				if (u->visible) update_unit_type(u);
				morphed_units.push_back(u);
				if (u->type == unit_types::extractor) u->owner->minerals_lost -= 50;
				break;
			case event_t::t_destroy:
				log("destroy %s\n", u->type->name);
				if (u->visible) update_unit_stuff(u);
				u->visible = false;
				u->dead = true;
				u->owner->minerals_lost += u->minerals_value;
				u->owner->gas_lost += u->gas_value;
				destroyed_units.push_back(u);
				break;
			case event_t::t_renegade:
				if (u->visible) update_unit_owner(u);
				break;
// 			case event_t::t_complete:
// 				log("%s completed\n",u->type->name);
// 				u->is_completed = true;
// 				break;
			case event_t::t_refresh:
				if (u->visible) update_unit_owner(u);
				if (u->visible) update_unit_type(u);
				if (u->visible) update_unit_stuff(u);
				break;
			}
			//log("event %d - %s visible ? %d\n", e.t, u->type->name, u->visible);
			if (b && u->building != b) update_building_squares();

			update_groups(u);
		}

		if (current_frame - last_refresh >= 90) {
			last_refresh = current_frame;
			for (unit*u : live_units) {
				events.emplace_back(event_t::t_refresh, u->game_unit);
			}
		}

		for (unit*u : visible_units) {
			if (!u->game_unit->exists()) {
				// If BWAPI sends the hide and destroy events properly, then this should never trigger.
				events.emplace_back(event_t::t_refresh, u->game_unit);
				continue;
			}
			u->last_seen = current_frame;
			bool prev_is_completed = u->is_completed;
			update_unit_stuff(u);

			if (u->is_completed != prev_is_completed) update_groups(u);

			if (u->gone) {
				log("%s is no longer gone\n",u->type->name);
				u->gone = false;
				//events.emplace_back(event_t::t_refresh, u->game_unit);
				update_groups(u);

// 				if (u->type == unit_types::marine) {
// 					for (unit*e : enemy_buildings) {
// 						if (!e->is_completed || e->type != unit_types::bunker || e->owner != u->owner) continue;
// 						if (diag_distance(e->pos - u->pos) >= 32 * 4) continue;
// 						if (e->marines_loaded) --e->marines_loaded;
// 					}
// 				}
			}
		}

		for (unit*u : all_units_ever) {
			//update_unit_container(u,my_workers,u->visible && u->owner==players::my_player && u->type->is_worker && u->is_completed && !u->is_morphing);

			if (u->game_unit->getID() < 0) {
				xcept("unit %p has invalid id %d\n", u->game_unit->getID());
			}
		}

		auto is_visible_around = [&](xy pos) {
			if (!grid::is_visible(pos)) return false;
			if (!grid::is_visible(pos + xy(32, 0))) return false;
			if (!grid::is_visible(pos + xy(0, 32))) return false;
			if (!grid::is_visible(pos + xy(-32, 0))) return false;
			if (!grid::is_visible(pos + xy(0, -32))) return false;
			return true;
		};
		auto has_detection_at = [&](xy pos) {
			for (unit*u : my_detector_units) {
				if ((pos - u->pos).length() <= u->stats->sight_range) return true;
			}
			return false;
		};

		for (unit*u : invisible_units) {
			if (!u->gone) {
				if (is_visible_around(u->pos) && (!u->burrowed || has_detection_at(u->pos))) {
					log("%s is gone\n",u->type->name);
					u->gone = true;
					events.emplace_back(event_t::t_refresh, u->game_unit);
				}
			}
		}

		if (current_frame-last_update_stats>=15*5) {
			update_all_stats();
		}

		for (unit*u : created_units) {
			for (auto&f : on_create_callbacks) f(u);
		}
		created_units.clear();
		for (unit*u : new_units_queue) {
			for (auto&f : on_new_unit_callbacks) f(u);
		}
		new_units_queue.clear();
		for (unit*u : morphed_units) {
			for (auto&f : on_morph_callbacks) f(u);
		}
		morphed_units.clear();
		for (auto&v : unit_type_changes_queue) {
			for (auto&f : on_type_change_callbacks) f(std::get<0>(v), std::get<1>(v));
		}
		unit_type_changes_queue.clear();
		for (unit*u : destroyed_units) {
			for (auto&f : on_destroy_callbacks) f(u);
		}
		destroyed_units.clear();

		static int last_eval_gg = 0;
		if (current_frame - last_eval_gg >= 15 * 10 && !dont_call_gg) {
			last_eval_gg = current_frame;
			double enemy_army_supply = 0;
			for (unit*u : enemy_units) {
				if (u->gone || u->type->is_worker) continue;
				enemy_army_supply += u->type->required_supply;
			}
			static int gg_frame = 0;
			bool call_gg = false;
			if (current_used_total_supply <= 5 && enemy_army_supply >= 20 && current_minerals < 500) call_gg = true;
			if (current_used_total_supply <= 1 && enemy_army_supply >= 5 && current_minerals < 50) call_gg = true;
			if (!gg_frame && call_gg) {
				gg_frame = current_frame;
				game->sendText("gg");
				log("gg, i lost :(\n");
			}
			if (gg_frame && current_frame >= gg_frame + 15 * 10) {
				game->leaveGame();
			}
		}

		multitasking::sleep(1);
	}

}

void render() {

// 	for (unit*u : my_buildings) {
// 		game->drawBoxMap(u->building->pos.x, u->building->pos.y, u->building->pos.x + 32, u->building->pos.y + 32, BWAPI::Colors::White);
// 	}

	render::draw_screen_stacked_text(260, 20, format("actual spent minerals: %d  gas: %d", players::my_player->game_player->spentMinerals(), players::my_player->game_player->spentGas()));
	render::draw_screen_stacked_text(260, 20, format("  calc spent minerals: %g  gas: %g", players::my_player->minerals_spent, players::my_player->gas_spent));
	render::draw_screen_stacked_text(260, 20, format("    op spent minerals: %g  gas: %g", players::opponent_player->minerals_spent, players::opponent_player->gas_spent));

	render::draw_screen_stacked_text(460, 20, format("my lost minerals: %g  gas: %g", players::my_player->minerals_lost, players::my_player->gas_lost));
	render::draw_screen_stacked_text(460, 20, format("op lost minerals: %g  gas: %g", players::opponent_player->minerals_lost, players::opponent_player->gas_lost));

}

void init() {

	unit_types::init();

	multitasking::spawn(0,update_units_task,"update units");
	multitasking::spawn(update_buildings_squares_task,"update buildings squares");

	multitasking::spawn(update_projectile_stuff_task, "update projectile stuff");

	render::add(render);

}


}



