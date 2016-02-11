//
// This file implements the units interface,
// and initializes the unit_types global data.
//

#include "units.h"
#include "bwapi_inc.h"
#include "bot.h"

namespace tsc_bwai {

	namespace {
		struct event_t {
			enum { t_show, t_hide, t_create, t_morph, t_destroy, t_renegade, t_complete, t_refresh };
			int t;
			BWAPI_Unit game_unit;
			event_t(int t, BWAPI_Unit game_unit) : t(t), game_unit(game_unit) {}
		};
	}

	struct units_t::impl_t {

		a_deque<event_t> events;

	};

	units_t::units_t(bot_t& bot){
		impl = std::make_unique<impl_t>();
	}

	units_t::~units_t() {
	}

	void units_t::on_unit_show(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_show, game_unit);
	}
	void units_t::on_unit_hide(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_hide, game_unit);
	}
	void units_t::on_unit_create(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_create, game_unit);
	}
	void units_t::on_unit_morph(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_morph, game_unit);
	}
	void units_t::on_unit_destroy(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_destroy, game_unit);
	}
	void units_t::on_unit_renegade(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_renegade, game_unit);
	}
	void units_t::on_unit_complete(BWAPI_Unit game_unit) {
		impl->events.emplace_back(event_t::t_complete, game_unit);
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
