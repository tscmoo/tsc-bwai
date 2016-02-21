//
// This file implements the units interface,
// and initializes the unit_types global data.
//

#include "units.h"
#include "bwapi_inc.h"
#include "bot.h"

namespace tsc_bwai {

	namespace {
		// Copy of a BWAPI event.
		struct event_t {
			enum { t_show, t_hide, t_create, t_morph, t_destroy, t_renegade, t_complete, t_refresh };
			int t;
			BWAPI_Unit game_unit;
			event_t(int t, BWAPI_Unit game_unit) : t(t), game_unit(game_unit) {}
		};
	}

	// The hidden implementation for the units module, which does all the work.
	class units_impl {
		bot_t& bot;

		a_deque<unit> unit_container;
		a_deque<unit_building> unit_building_container;

		a_deque<weapon_stats> weapon_stats_container;
		a_map<std::tuple<BWAPI::WeaponType, player_t*>, weapon_stats*> weapon_stats_map;

		a_deque<unit_stats> unit_stats_container;
		a_map<std::tuple<unit_type*, player_t*>, unit_stats*> unit_stats_map;

		a_vector<unit_building*> registered_buildings;

		unit* new_unit(BWAPI_Unit game_unit);

		void update_unit_container(unit* u, a_vector<unit*>& cont, bool contain);

		void update_unit_owner(unit* u);
		void update_unit_value(unit* u, unit_type* from, unit_type* to);
		void update_unit_type(unit* u);
		void update_weapon_stats(weapon_stats*st);
		void update_stats(unit_stats* st);
		void update_unit_stuff(unit* u);
		void update_building_squares();
		void update_all_stats();

		a_vector<unit*> new_units_queue;
		a_vector<std::tuple<unit*, unit_type*>> unit_type_changes_queue;

		int last_eval_gg = 0;

		a_unordered_map<BWAPI_Bullet, int> bullet_timestamps;

		// This task is responsible for updating all information about units, unit types,
		// unit stats and weapon stats (and unit containers).
		// It also handles events, since that ties into updating unit information.
		void update_units_task();

		// This task updates information in build_squares to reflect landed buildings,
		// used by pathfinding.
		void update_buildings_squares_task();

		// This task handles projectile thingies, which currently is only recognizing
		// bullets that come out of bunkers to identify how many marines are loaded
		// in them.
		void update_projectile_stuff_task();

	public:
		// All (BWAPI) events that are queued up for processing.
		a_deque<event_t> events;

		weapon_stats* get_weapon_stats(BWAPI::WeaponType type, player_t*player);
		unit_stats* get_unit_stats(unit_type* type, player_t* player);
		unit* get_unit(BWAPI_Unit game_unit);

		units_impl(bot_t& bot) : bot(bot) {}
		void init() {
			bot.multitasking.spawn(0.0, std::bind(&units_impl::update_units_task, this), "update units");
			bot.multitasking.spawn(std::bind(&units_impl::update_buildings_squares_task, this), "update buildings squares");

			bot.multitasking.spawn(std::bind(&units_impl::update_projectile_stuff_task, this), "update projectile stuff");

			//bot.render.add(std::bind(&units_module::render, this));
		}

	};

	class units_module::impl_t : public units_impl {
	public:
		impl_t(bot_t& bot) : units_impl(bot) {}
	};

	unit* units_module::get_unit(BWAPI_Unit game_unit) {
		return impl->get_unit(game_unit);
	}

	unit_stats* units_module::get_unit_stats(unit_type* type, player_t* player) {
		return impl->get_unit_stats(type, player);
	}

	units_module::units_module(bot_t& bot){
		impl = std::make_unique<impl_t>(bot);
	}

	units_module::~units_module() {
	}

	void units_module::on_unit_show(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_show, game_unit);
	}
	void units_module::on_unit_hide(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_hide, game_unit);
	}
	void units_module::on_unit_create(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_create, game_unit);
	}
	void units_module::on_unit_morph(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_morph, game_unit);
	}
	void units_module::on_unit_destroy(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_destroy, game_unit);
	}
	void units_module::on_unit_renegade(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_renegade, game_unit);
	}
	void units_module::on_unit_complete(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_complete, game_unit);
	}

	void units_module::init() {
		impl->init();
	}

	namespace {

		std::tuple<double, double> get_unit_value(bot_t& bot, unit_type* from, unit_type* to) {
			double minerals = 0.0;
			double gas = 0.0;
			if (from == unit_types::siege_tank_tank_mode && to == unit_types::siege_tank_siege_mode) return std::make_tuple(minerals, gas);
			if (from == unit_types::siege_tank_siege_mode && to == unit_types::siege_tank_tank_mode) return std::make_tuple(minerals, gas);
			bot.log("update unit value for %s -> %s\n", from ? from->name : "null", to->name);
			if (to->race == race_zerg) {
				if (from == unit_types::larva) from = nullptr;
				auto trace_back = [&]() {
					for (unit_type*t = to; t->builder_type; t = t->builder_type) {
						if (t == from) {
							from = nullptr;
							break;
						}
						if (t == unit_types::larva) break;
						bot.log(" +> %s - %g %g\n", t->name, t->minerals_cost, t->gas_cost);
						double mult = t->is_two_units_in_one_egg ? 0.5 : 1.0;
						minerals += t->minerals_cost * mult;
						gas += t->gas_cost * mult;
					}
				};
				trace_back();
				if (from) {
					bot.log("maybe canceled?\n");
					unit_type*prev_from = from;
					double mult = from->is_building ? 0.75 : 1.0;
					minerals = -from->minerals_cost * mult;
					gas = -from->gas_cost * mult;
					from = from->builder_type;
					trace_back();
					if (from) from = prev_from;
				}
			} else {
				if (from == unit_types::siege_tank_siege_mode) from = unit_types::siege_tank_tank_mode;
				if (to == unit_types::archon) {
					minerals = unit_types::high_templar->minerals_cost * 2;
					gas = unit_types::high_templar->gas_cost * 2;
				} else if (to == unit_types::dark_archon) {
					minerals = unit_types::dark_templar->minerals_cost * 2;
					gas = unit_types::dark_templar->gas_cost * 2;
				} else {
					minerals = to->minerals_cost;
					gas = to->gas_cost;
				}
			}
			// TODO: display an error on screen or something when this occurs
			//if (from != nullptr) xcept("unable to trace how %s changed into %s\n", from->name, to->name);
			if (from != nullptr) bot.log(" !! error: unable to trace how %s changed into %s\n", from->name, to->name);
			bot.log("final cost: %g %g\n", minerals, gas);
			return std::make_tuple(minerals, gas);
		}

		a_deque<unit_type> unit_type_container;
		a_unordered_map<int, unit_type*> unit_type_map;

		unit_type* get_unit_type(BWAPI::UnitType game_unit_type) {
			auto i = unit_type_map.find(game_unit_type.getID());
			if (i == unit_type_map.end()) return nullptr;
			return i->second;
		}
		unit_type*new_unit_type(BWAPI::UnitType game_unit_type, unit_type* ut) {
			ut->game_unit_type = game_unit_type;
			ut->name = game_unit_type.getName().c_str();
			ut->id = game_unit_type.getID();
			ut->dimensions[0] = game_unit_type.dimensionRight();
			ut->dimensions[1] = game_unit_type.dimensionDown();
			ut->dimensions[2] = game_unit_type.dimensionLeft();
			ut->dimensions[3] = game_unit_type.dimensionUp();
			ut->is_worker = game_unit_type.isWorker();
			ut->is_minerals = game_unit_type.isMineralField();
			ut->is_gas = game_unit_type == BWAPI::UnitTypes::Resource_Vespene_Geyser || game_unit_type == BWAPI::UnitTypes::Terran_Refinery || game_unit_type == BWAPI::UnitTypes::Protoss_Assimilator || game_unit_type == BWAPI::UnitTypes::Zerg_Extractor;
			ut->requires_creep = game_unit_type.requiresCreep();
			ut->requires_pylon = game_unit_type.requiresPsi();
			ut->race = game_unit_type.getRace() == BWAPI::Races::Terran ? race_terran : game_unit_type.getRace() == BWAPI::Races::Protoss ? race_protoss : game_unit_type.getRace() == BWAPI::Races::Zerg ? race_zerg : race_unknown;
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
			ut->build_near_me = ut->is_resource_depot || ut == unit_types::supply_depot || ut == unit_types::pylon || ut == unit_types::creep_colony || ut == unit_types::sunken_colony || ut == unit_types::spore_colony;
			for (auto&v : game_unit_type.requiredUnits()) {
				unit_type* t = get_unit_type(v.first);
				if (!t) xcept("%d has invalid required unit %d", game_unit_type.getID(), v.first.getID());
				t->required_for.push_back(ut);
				ut->required_units.push_back(t);
				//if (v.second!=1) xcept("%s requires %d %s\n",ut->name,v.second,t->name);
			}
			ut->produces_land_units = false;
			for (auto&v : unit_types::units_that_produce_land_units) {
				if (v == ut) ut->produces_land_units = true;
			}
			ut->needs_neighboring_free_space = ut->is_building && (ut->produces_land_units || ut == unit_types::nydus_canal || ut == unit_types::bunker);
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
			ut->is_non_usable = ut == unit_types::spider_mine || ut == unit_types::nuclear_missile || ut == unit_types::larva || ut == unit_types::egg || ut == unit_types::lurker_egg || ut == unit_types::cocoon;
			ut->is_non_usable |= game_unit_type.maxHitPoints() <= 1;
			ut->is_detector = game_unit_type.isDetector();
			ut->is_liftable = game_unit_type.isFlyingBuilding();
			return ut;
		}
		unit_type* get_unit_type_ptr(BWAPI::UnitType game_unit_type) {
			unit_type*& ut = unit_type_map[game_unit_type.getID()];
			if (ut) return ut;
			if (game_unit_type == BWAPI::UnitTypes::None) return nullptr;
			unit_type_container.emplace_back();
			ut = &unit_type_container.back();
			ut->game_unit_type = game_unit_type;
			return ut;
		}
	
	}

	namespace unit_types {

		unit_type* get_unit_type(BWAPI::UnitType game_unit_type) {
			auto i = unit_type_map.find(game_unit_type.getID());
			if (i == unit_type_map.end()) return nullptr;
			return i->second;
		}

		void get(unit_type*& rv, BWAPI::UnitType game_unit_type) {
			rv = get_unit_type_ptr(game_unit_type);
			if (!rv) xcept("while initializing global unit types data: got null unit_type for '%s' (id %d)", game_unit_type.getName(), game_unit_type.getID());
		}
		unit_type_pointer unknown;
		unit_type_pointer cc, supply_depot, barracks, factory, science_facility, nuclear_silo, bunker, refinery, machine_shop;
		unit_type_pointer missile_turret, academy, comsat_station, starport, control_tower, engineering_bay, armory, covert_ops;
		unit_type_pointer spell_scanner_sweep;
		unit_type_pointer scv, marine, vulture, siege_tank_tank_mode, siege_tank_siege_mode, goliath;
		unit_type_pointer medic, ghost, firebat;
		unit_type_pointer wraith, battlecruiser, dropship, science_vessel, valkyrie;
		unit_type_pointer spider_mine, nuclear_missile;
		unit_type_pointer nexus, pylon, gateway, photon_cannon, robotics_facility, stargate, forge, citadel_of_adun, templar_archives;
		unit_type_pointer fleet_beacon, assimilator, observatory, cybernetics_core, shield_battery;
		unit_type_pointer robotics_support_bay, arbiter_tribunal;
		unit_type_pointer probe;
		unit_type_pointer zealot, dragoon, dark_templar, high_templar, reaver;
		unit_type_pointer archon, dark_archon;
		unit_type_pointer observer, shuttle, scout, carrier, interceptor, arbiter, corsair;
		unit_type_pointer hatchery, lair, hive, creep_colony, sunken_colony, spore_colony, nydus_canal, spawning_pool, evolution_chamber;
		unit_type_pointer hydralisk_den, spire, greater_spire, extractor, ultralisk_cavern, defiler_mound;
		unit_type_pointer drone, overlord, zergling, larva, egg, hydralisk, lurker, lurker_egg, ultralisk, defiler, broodling;
		unit_type_pointer mutalisk, cocoon, scourge, queen, guardian, devourer;
		unit_type_pointer spell_dark_swarm;
		unit_type_pointer vespene_geyser;
		void init_unit_types() {
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
			get(shield_battery, BWAPI::UnitTypes::Protoss_Shield_Battery);
			get(robotics_support_bay, BWAPI::UnitTypes::Protoss_Robotics_Support_Bay);
			get(arbiter_tribunal, BWAPI::UnitTypes::Protoss_Arbiter_Tribunal);

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
			get(broodling, BWAPI::UnitTypes::Zerg_Broodling);

			get(mutalisk, BWAPI::UnitTypes::Zerg_Mutalisk);
			get(cocoon, BWAPI::UnitTypes::Zerg_Cocoon);
			get(scourge, BWAPI::UnitTypes::Zerg_Scourge);
			get(queen, BWAPI::UnitTypes::Zerg_Queen);
			get(guardian, BWAPI::UnitTypes::Zerg_Guardian);
			get(devourer, BWAPI::UnitTypes::Zerg_Devourer);

			get(spell_dark_swarm, BWAPI::UnitTypes::Spell_Dark_Swarm);

			get(vespene_geyser, BWAPI::UnitTypes::Resource_Vespene_Geyser);
		}

		void init_global_data(bot_t& bot) {

			init_unit_types();

			for (auto& game_unit_type : BWAPI::UnitTypes::allUnitTypes()) {
				get_unit_type_ptr(game_unit_type);
			}

			for (unit_type& ut : unit_type_container) {
				new_unit_type(ut.game_unit_type, &ut);
			}
			for (unit_type& ut : unit_type_container) {
				std::tie(ut.total_minerals_cost, ut.total_gas_cost) = get_unit_value(bot, nullptr, &ut);
			}

		}
	}
}

namespace {
	static const size_t npos = ~(size_t)0;
}

using namespace tsc_bwai;

unit* units_impl::new_unit(BWAPI_Unit game_unit) {
	unit_container.emplace_back();
	unit* u = &unit_container.back();
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
	u->creation_frame = bot.current_frame;
	u->last_seen = bot.current_frame;
	u->gone = false;
	u->last_shown = bot.current_frame;

	u->force_combat_unit = false;
	//u->last_attacking = 0;
	u->last_attacked = 0;
	u->last_attack_target = nullptr;
	u->prev_weapon_cooldown = 0;

	u->is_attacking = false;
	u->prev_is_attacking = false;
	u->is_attacking_frame = 0;

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
	u->visible = game_unit->isVisible(bot.players.self);
	update_unit_stuff(u);

	return u;
}

unit* units_impl::get_unit(BWAPI_Unit game_unit) {
	unit*u = (unit*)game_unit->getClientInfo();
	if (!u) {
		if (!game_unit->exists()) return nullptr;
		u = new_unit(game_unit);
	}
	return u;
}


weapon_stats* units_impl::get_weapon_stats(BWAPI::WeaponType type, player_t*player) {
	weapon_stats*& st = weapon_stats_map[std::make_tuple(type, player)];
	if (st) return st;
	weapon_stats_container.emplace_back();
	st = &weapon_stats_container.back();
	st->game_weapon_type = type;
	st->player = player;
	update_weapon_stats(st);
	return st;
};

unit_stats* units_impl::get_unit_stats(unit_type* type, player_t* player) {
	unit_stats*& st = unit_stats_map[std::make_tuple(type, player)];
	if (st) return st;
	unit_stats_container.emplace_back();
	st = &unit_stats_container.back();
	st->type = type;
	st->player = player;
	st->ground_weapon = nullptr;
	st->air_weapon = nullptr;
	auto gw = st->type->game_unit_type.groundWeapon();
	if (gw != BWAPI::WeaponTypes::None) st->ground_weapon = get_weapon_stats(gw, player);
	if (st->type->game_unit_type == BWAPI::UnitTypes::Protoss_Scarab) st->ground_weapon = nullptr;
	if (st->type->game_unit_type == BWAPI::UnitTypes::Protoss_Reaver) {
		st->ground_weapon = get_weapon_stats(BWAPI::WeaponTypes::Scarab, player);
		st->ground_weapon->cooldown = 60;
		st->ground_weapon->max_range = 32 * 8;
	}
	auto aw = st->type->game_unit_type.airWeapon();
	if (aw != BWAPI::WeaponTypes::None) st->air_weapon = get_weapon_stats(aw, player);
	update_stats(st);
	return st;
}


void units_impl::update_unit_container(unit* u, a_vector<unit*>& cont, bool contain) {
	size_t idx = &cont - &bot.units.unit_containers[0];
	size_t&u_idx = u->container_indexes[idx];
	if (contain && u_idx == npos) {
		u_idx = cont.size();
		cont.push_back(u);
	}
	if (!contain && u_idx != npos) {
		if (u_idx != cont.size() - 1) {
			cont[cont.size() - 1]->container_indexes[idx] = u_idx;
			std::swap(cont[cont.size() - 1], cont[u_idx]);
		}
		cont.pop_back();
		u_idx = npos;
	}
}

void units_impl::update_unit_owner(unit* u) {
	u->owner = bot.players.get_player(u->game_unit->getPlayer());
	if (u->type) u->stats = get_unit_stats(u->type, u->owner);
}

void units_impl::update_unit_value(unit* u, unit_type* from, unit_type* to) {
	if (from == to) return;
	double min, gas;
	std::tie(min, gas) = get_unit_value(bot, from, to);
	double free_min = 0;
	if (from == nullptr) {
		if (to->is_resource_depot && ++u->owner->resource_depots_seen <= 1) {
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

void units_impl::update_unit_type(unit* u) {
	unit_type* ut = get_unit_type(u->game_unit->getType());
	if (ut == u->type) return;
	if (u->last_type_change_owner && u->owner != u->last_type_change_owner && u->type != unit_types::vespene_geyser) {
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

	if (u->owner->random) {
		if (u->type->race != race_unknown) {
			u->owner->race = u->type->race;
			u->owner->random = false;
		}
	}
}
void units_impl::update_weapon_stats(weapon_stats*st) {

	auto& gw = st->game_weapon_type;
	auto& gp = st->player->game_player;

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
	if (gw.explosionType() == BWAPI::ExplosionTypes::Air_Splash) st->explosion_type = weapon_stats::explosion_type_air_splash;
	st->inner_splash_radius = gw.innerSplashRadius();
	st->median_splash_radius = gw.medianSplashRadius();
	st->outer_splash_radius = gw.outerSplashRadius();
}
void units_impl::update_stats(unit_stats* st) {

	auto& gp = st->player->game_player;
	auto& gu = st->type->game_unit_type;

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
	if (gu == BWAPI::UnitTypes::Protoss_Reaver) st->ground_weapon_hits = 1;
	if (st->ground_weapon_hits == 0 && st->ground_weapon) {
		bot.log("warning: %s.maxGroundHits() returned 0 (setting to 1)\n", st->type->name);
		st->ground_weapon_hits = 1;
	}
	st->air_weapon_hits = gu.maxAirHits();
	if (st->air_weapon_hits == 0 && st->air_weapon) {
		bot.log("warning: %s.maxAirHits() returned 0 (setting to 1)\n", st->type->name);
		st->air_weapon_hits = 1;
	}
	if (st->ground_weapon) update_weapon_stats(st->ground_weapon);
	if (st->air_weapon) update_weapon_stats(st->air_weapon);
}
void units_impl::update_unit_stuff(unit* u) {
	bwapi_pos position = u->game_unit->getPosition();
	if (!u->game_unit->exists()) xcept("attempt to update inaccessible unit");
	if (u->game_unit->getID() < 0) xcept("(update) %s->getId() is %d\n", u->type->name, u->game_unit->getID());
	if (u->visible != u->game_unit->isVisible(bot.players.self)) xcept("visible mismatch in unit %s (%d vs %d)", u->type->name, u->visible, u->game_unit->isVisible(bot.players.self));
	if ((size_t)position.x >= (size_t)bot.grid.map_width || (size_t)position.y >= (size_t)bot.grid.map_height) xcept("unit %s is outside map", u->type->name);
	u->pos.x = position.x;
	u->pos.y = position.y;
	u->hspeed = u->game_unit->getVelocityX();
	u->vspeed = u->game_unit->getVelocityY();
	u->speed = xy_typed<double>(u->hspeed, u->vspeed).length();
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
	u->remaining_whatever_time = std::max(u->remaining_build_time, std::max(u->remaining_train_time, std::max(u->remaining_research_time, u->remaining_upgrade_time)));
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
		if (!gu->isVisible(bot.players.self)) return nullptr;
		if (gu->getID() < 0) xcept("(target) %s->getId() is %d\n", gu->getType().getName(), gu->getID());
		return get_unit(gu);
	};
	u->target = targetunit(u->game_unit->getTarget());
	u->order_target = targetunit(u->game_unit->getOrderTarget());

	if (u->weapon_cooldown > u->prev_weapon_cooldown) u->last_attacked = bot.current_frame;
	if (u->game_order == BWAPI::Orders::AttackUnit || u->last_attacked == bot.current_frame) u->last_attack_target = u->order_target;

	u->prev_is_attacking = u->is_attacking;
	u->is_attacking = u->game_unit->isAttacking();
	if (u->is_attacking != u->prev_is_attacking) u->is_attacking_frame = u->is_attacking ? bot.current_frame : 0;

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
	u->maelstrom_timer = u->game_unit->getMaelstromTimer() * 8;
	u->stasis_timer = u->game_unit->getStasisTimer() * 8;
	u->defensive_matrix_timer = u->game_unit->getDefenseMatrixTimer() * 8;
	u->defensive_matrix_hp = u->game_unit->getDefenseMatrixPoints();
	u->irradiate_timer = u->game_unit->getIrradiateTimer() * 8;
	u->stim_timer = u->game_unit->getStimTimer() * 8;
	u->plague_timer = u->game_unit->getPlagueTimer() * 8;
	u->is_blind = u->game_unit->isBlind();

	u->is_powered = bwapi_is_powered(u->game_unit);

	bool is_being_gathered = u->game_unit->isBeingGathered();
	if (is_being_gathered && !u->is_being_gathered) u->is_being_gathered_begin_frame = bot.current_frame;
	u->is_being_gathered = is_being_gathered;

	unit_building*b = u->building;
	if (b) {
		b->is_lifted = u->game_unit->isLifted();
		bwapi_pos tile_pos = u->game_unit->getTilePosition();
		if ((size_t)tile_pos.x >= (size_t)bot.grid.build_grid_width || (size_t)tile_pos.y >= (size_t)bot.grid.build_grid_height) {
			bot.log(log_level_info, "error: unit %s has tile pos outside map (at %d %d)", u->type->name, tile_pos.x, tile_pos.y);
			tile_pos.x = 0;
			tile_pos.y = 0;
		}
		if ((size_t)tile_pos.x + u->type->tile_width > (size_t)bot.grid.build_grid_width || (size_t)tile_pos.y + u->type->tile_height > (size_t)bot.grid.build_grid_height) {
			bot.log(log_level_info, "error: unit %s has tile pos outside map (at %d %d)", u->type->name, tile_pos.x, tile_pos.y);
			tile_pos.x = 0;
			tile_pos.y = 0;
		}
		b->build_pos.x = tile_pos.x * 32;
		b->build_pos.y = tile_pos.y * 32;
		if (b->last_registered_pos == xy()) b->last_registered_pos = b->build_pos;
	}
}
void units_impl::update_building_squares() {
	// This function is carefully written to recognize whenever a building
	// is created, moved or destroyed in order to update build squares
	// and pathing information.

	// Check if any registered buildings have been moved or destroyed
	for (size_t i = 0; i < registered_buildings.size(); ++i) {
		unit_building* b = registered_buildings[i];
		unit* u = b->u;
		// b->u->building can change if a building morphs to a non-building, then back
		// to a building before this code has a chance to run.
		if (b->u->building != b || b->is_lifted || u->pos != b->walk_squares_occupied_pos || u->gone || u->dead) {
			bot.log("unregistered %s, because %s, %d squares\n", u->type->name, b->u->building != b ? "unit changed" : b->is_lifted ? "lifted" : u->pos != b->walk_squares_occupied_pos ? "moved" : u->gone ? "gone" : u->dead ? "dead" : "?", b->walk_squares_occupied.size());
			// walk squares contains a list of all buildings, so just erase this building.
			for (auto* ws_p : b->walk_squares_occupied) {
				find_and_erase(ws_p->buildings, u);
			}
			b->walk_squares_occupied.clear();
			// build squares only contain one pointer, so only reset it if it's this building.
			for (auto* bs_p : b->build_squares_occupied) {
				if (bs_p->building == u) bs_p->building = nullptr;
			}
			b->build_squares_occupied.clear();
			xy upper_left = u->pos - xy(u->type->dimension_left(), u->type->dimension_up());
			xy bottom_right = u->pos + xy(u->type->dimension_right(), u->type->dimension_down());
			// square_pathing needs to be told that something has changed in this area,
			// so it can recalculate pathing information.
			bot.square_pathing.invalidate_area(upper_left, bottom_right);
			// Remove the building from registered_buildings.
			if (i != registered_buildings.size() - 1) std::swap(registered_buildings[registered_buildings.size() - 1], registered_buildings[i]);
			registered_buildings.pop_back();
			--i;
		} else {
			// Even if nothing has changed, the pointer in build_square can be set
			// by other buildings, so change it back! :D
			for (auto* bs_p : b->build_squares_occupied) {
				if (bs_p->building != u) bs_p->building = u;
			}
		}
	}

	// Check if any buildings should be registered.
	for (unit* u : bot.units.visible_buildings) {
		unit_building* b = u->building;
		if (!b) continue;
		if (b->is_lifted) continue;

		if (b->walk_squares_occupied.empty()) {
			xy upper_left = u->pos - xy(u->type->dimension_left(), u->type->dimension_up());
			xy bottom_right = u->pos + xy(u->type->dimension_right(), u->type->dimension_down());
			for (int y = upper_left.y&-8; y <= bottom_right.y; y += 8) {
				if ((size_t)y >= (size_t)bot.grid.map_height) continue;
				for (int x = upper_left.x&-8; x <= bottom_right.x; x += 8) {
					if ((size_t)x >= (size_t)bot.grid.map_width) continue;
					auto& ws = bot.grid.get_walk_square(xy(x, y));
					b->walk_squares_occupied.push_back(&ws);
					ws.buildings.push_back(u);
				}
			}
			for (int y = 0; y < u->type->tile_height; ++y) {
				for (int x = 0; x < u->type->tile_width; ++x) {
					auto&bs = bot.grid.get_build_square(u->building->build_pos + xy(x * 32, y * 32));
					b->build_squares_occupied.push_back(&bs);
					bs.building = u;
				}
			}
			bot.square_pathing.invalidate_area(upper_left, bottom_right);
			registered_buildings.push_back(b);
			b->walk_squares_occupied_pos = u->pos;
			b->last_registered_pos = b->build_pos;
			bot.log("registered %s, %d squares\n", u->type->name, b->walk_squares_occupied.size());
		}
	}
}
void units_impl::update_all_stats() {
	for (auto&v : unit_stats_container) update_stats(&v);
}

void units_impl::update_buildings_squares_task() {
	while (true) {
		update_building_squares();

		bot.multitasking.sleep(4);
	}
}

void units_impl::update_projectile_stuff_task() {
	while (true) {
		a_vector<unit*> bunkers;
		for (unit* u : bot.units.enemy_buildings) {
			if (!u->is_completed) continue;
			if (u->type == unit_types::bunker) {
				double d = get_best_score_value(bot.units.my_units, [&](unit*mu) {
					return units_distance(mu, u);
				}, std::numeric_limits<double>::infinity());
				if (d <= 32 * 5) {
					bunkers.push_back(u);
					u->marines_loaded = 0;
				}
			}
		}
		for (BWAPI_Bullet b : bot.game->getBullets()) {
			if (b->getType() == BWAPI::BulletTypes::Gauss_Rifle_Hit && b->getRemoveTimer() > 4) {
				int&ts = bullet_timestamps[b];
				if (!ts || bot.current_frame - ts > 15) ts = bot.current_frame;
				if (b->getTarget() && b->getTarget()->getPlayer() == bot.players.my_player->game_player && bot.current_frame - ts < 15) {
					unit* target = get_unit(b->getTarget());
					if (target) {
						unit* bunker = get_best_score(bunkers, [&](unit*u) {
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
		for (unit* u : bot.units.enemy_buildings) {
			if (!u->is_completed) continue;
			if (u->type == unit_types::bunker) {
				bot.log("bunker %p has %d marines\n", u, u->marines_loaded);
			}
		}

		bot.multitasking.sleep(1);
	}
}

void units_impl::update_units_task() {
	// This is a high priority task, and the order things are done is important to
	// maintain consistency in the various unit fields.

	int last_update_stats = 0;
	int last_refresh = 0;

	a_vector<unit*> created_units, morphed_units, destroyed_units;

	auto update_groups = [this](unit*u) {
		update_unit_container(u, bot.units.all_units_ever, true);
		update_unit_container(u, bot.units.live_units, !u->dead);
		update_unit_container(u, bot.units.visible_units, u->visible);
		update_unit_container(u, bot.units.invisible_units, !u->dead && !u->visible);
		update_unit_container(u, bot.units.visible_buildings, u->visible && u->building);
		update_unit_container(u, bot.units.resource_units, !u->dead && !u->gone && (u->type->is_minerals || u->type->is_gas));

		bool is_unpowered = u->type->requires_pylon && !u->is_powered;
		update_unit_container(u, bot.units.my_units, u->visible && u->owner == bot.players.my_player && !is_unpowered);
		update_unit_container(u, bot.units.my_workers, u->visible && u->owner == bot.players.my_player && u->type->is_worker && u->is_completed && !u->is_morphing && !is_unpowered);
		update_unit_container(u, bot.units.my_buildings, u->visible && u->owner == bot.players.my_player && u->building && !is_unpowered);
		update_unit_container(u, bot.units.my_resource_depots, u->visible && u->owner == bot.players.my_player && u->type->is_resource_depot && !is_unpowered);
		update_unit_container(u, bot.units.my_detector_units, u->visible && u->owner == bot.players.my_player && u->type->is_detector && !is_unpowered);

		// What was this used for? Might've been analyzing replays..
		//bool has_ever_been_shown = u->last_shown != 0; // Fails when the unit is *actually* shown on frame 0 though.
		bool has_ever_been_shown = true;

		update_unit_container(u, bot.units.enemy_units, !u->dead && u->owner->is_enemy && has_ever_been_shown);
		update_unit_container(u, bot.units.visible_enemy_units, u->visible && u->owner->is_enemy && has_ever_been_shown);
		update_unit_container(u, bot.units.enemy_buildings, !u->dead && !u->gone && u->building && u->owner->is_enemy && has_ever_been_shown);
		update_unit_container(u, bot.units.enemy_detector_units, !u->dead && u->owner->is_enemy && u->type->is_detector && has_ever_been_shown);

		update_unit_container(u, bot.units.rescuable_units, !u->dead && u->owner->rescuable);

		bot.units.my_units_of_type.clear();
		for (unit* u : bot.units.my_units) {
			bot.units.my_units_of_type[u->type].push_back(u);
		}
		bot.units.my_completed_units_of_type.clear();
		for (unit* u : bot.units.my_units) {
			if (!u->is_completed) continue;
			bot.units.my_completed_units_of_type[u->type].push_back(u);
		}

		// Calling get_unit_controller will set the u->controller field, which is
		// required for all of our units.
		if (u->owner == bot.players.my_player && !u->controller) bot.unit_controls.get_unit_controller(u);
	};

	while (true) {

		// This loop must execute atomically with respect to other tasks
		// (no yields).

		// Because we need up-to-date visibility information, the build grid is
		// updated from here.
		bot.grid.update_build_grid();

		// In replays, we generate the show and hide events manually.
		if (bot.game->isReplay()) {
			for (auto& game_unit : bot.game->getAllUnits()) {
				unit* u = bot.units.get_unit(game_unit);
				if (!u) continue;
				if (!u->visible) {
					if (game_unit->isVisible(bot.players.self)) events.emplace_back(event_t::t_show, game_unit);
				} else {
					if (!game_unit->isVisible(bot.players.self)) events.emplace_back(event_t::t_hide, game_unit);
				}
			}
		}

		while (!events.empty()) {
			auto e = events.front();
			events.pop_front();
			unit* u = bot.units.get_unit(e.game_unit);
			if (!u) {
				bot.log("missed new unit %s\n", e.game_unit->getType().getName());
				continue;
			}
			// These events are delayed, so e.g. a unit is not necessarily visible even though we are processing a show event.
			u->visible = u->game_unit->isVisible(bot.players.self);
			u->visible &= u->game_unit->exists(); // BWAPI bug: destroyed units are visible
			auto* b = u->building;
			switch (e.t) {
			case event_t::t_show:
				bot.log("show %s\n", u->type->name);
				if (u->visible) update_unit_owner(u);
				if (u->visible) update_unit_type(u);
				u->last_shown = bot.current_frame;
				update_stats(u->stats);
				break;
			case event_t::t_hide:
				bot.log("hide %s\n", u->type->name);
				// the unit is still visible, but will be invisible the next frame
				if (u->visible) update_unit_stuff(u);
				u->visible = false;
				break;
			case event_t::t_create:
				created_units.push_back(u);
				break;
			case event_t::t_morph:
				bot.log("morph %s -> %s\n", u->type->name, u->game_unit->getType().getName());
				if (u->visible) update_unit_owner(u);
				if (u->visible) update_unit_type(u);
				morphed_units.push_back(u);
				if (u->type == unit_types::extractor) u->owner->minerals_lost -= 50;
				break;
			case event_t::t_destroy:
				bot.log("destroy %s\n", u->type->name);
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

		if (bot.current_frame - last_refresh >= 90) {
			last_refresh = bot.current_frame;
			for (unit* u : bot.units.live_units) {
				events.emplace_back(event_t::t_refresh, u->game_unit);
			}
		}

		for (unit* u : bot.units.visible_units) {
			if (!u->game_unit->exists()) {
				// If BWAPI sends the hide and destroy events properly, then this should never trigger.
				events.emplace_back(event_t::t_refresh, u->game_unit);
				continue;
			}
			u->last_seen = bot.current_frame;
			bool prev_is_completed = u->is_completed;
			update_unit_stuff(u);

			if (u->is_completed != prev_is_completed) update_groups(u);

			if (u->gone) {
				bot.log("%s is no longer gone\n", u->type->name);
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

		for (unit* u : bot.units.all_units_ever) {
			//update_unit_container(u,my_workers,u->visible && u->owner==players::my_player && u->type->is_worker && u->is_completed && !u->is_morphing);

			if (u->game_unit->getID() < 0) {
				xcept("unit %p has invalid id %d\n", u->game_unit->getID());
			}
		}

		auto is_visible_around = [&](xy pos) {
			if (!bot.grid.is_visible(pos)) return false;
			if (!bot.grid.is_visible(pos + xy(32, 0))) return false;
			if (!bot.grid.is_visible(pos + xy(0, 32))) return false;
			if (!bot.grid.is_visible(pos + xy(-32, 0))) return false;
			if (!bot.grid.is_visible(pos + xy(0, -32))) return false;
			return true;
		};
		auto has_detection_at = [&](xy pos) {
			for (unit* u : bot.units.my_detector_units) {
				if ((pos - u->pos).length() <= u->stats->sight_range) return true;
			}
			return false;
		};

		if (!bot.game->isReplay()) {
			for (unit* u : bot.units.invisible_units) {
				if (!u->gone) {
					if (is_visible_around(u->pos) && (!u->burrowed || has_detection_at(u->pos))) {
						bot.log("%s is gone\n", u->type->name);
						u->gone = true;
						events.emplace_back(event_t::t_refresh, u->game_unit);
					}
				}
			}
		}

		if (bot.current_frame - last_update_stats >= 15 * 5) {
			update_all_stats();
		}

		for (unit* u : created_units) {
			for (auto& f : bot.units.on_create_callbacks) f(u);
		}
		created_units.clear();
		for (unit* u : new_units_queue) {
			for (auto& f : bot.units.on_new_unit_callbacks) f(u);
		}
		new_units_queue.clear();
		for (unit* u : morphed_units) {
			for (auto& f : bot.units.on_morph_callbacks) f(u);
		}
		morphed_units.clear();
		for (auto& v : unit_type_changes_queue) {
			for (auto& f : bot.units.on_type_change_callbacks) f(std::get<0>(v), std::get<1>(v));
		}
		unit_type_changes_queue.clear();
		for (unit* u : destroyed_units) {
			for (auto& f : bot.units.on_destroy_callbacks) f(u);
		}
		destroyed_units.clear();

		// This code does not belong here.
		// TODO: find a better spot to put this.
		if (bot.current_frame - last_eval_gg >= 15 * 10 && !bot.dont_call_gg) {
			last_eval_gg = bot.current_frame;
			double enemy_army_supply = 0;
			for (unit* u : bot.units.enemy_units) {
				if (u->gone || u->type->is_worker) continue;
				enemy_army_supply += u->type->required_supply;
			}
			static int gg_frame = 0;
			bool call_gg = false;
			if (bot.current_used_total_supply <= 5 && enemy_army_supply >= 20 && bot.current_minerals < 500) call_gg = true;
			if (bot.current_used_total_supply <= 1 && enemy_army_supply >= 5 && bot.current_minerals < 50) call_gg = true;
			if (!gg_frame && call_gg) {
				gg_frame = bot.current_frame;
				bot.game->sendText("gg");
				bot.log("gg, i lost :(\n");
			}
			if (gg_frame && bot.current_frame >= gg_frame + 15 * 10) {
				bot.game->leaveGame();
			}
		}

		bot.multitasking.sleep(1);
	}

}


