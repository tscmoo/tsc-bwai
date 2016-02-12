//
// This file contains declarations for types dealing with units, and all the
// game information contained therein.
//

#ifndef TSC_BWAI_UNITS_H
#define TSC_BWAI_UNITS_H

#include "common.h"
#include "bwapi_inc.h"

namespace tsc_bwai {

	struct unit;
	struct unit_controller;
	struct player_t;
	struct walk_square;
	struct build_square;

	// unit_type - holds all information about individual unit types.
	// Most information is retrieved from BWAPI.
	struct unit_type {
		// The BWAPI interface for this unit type.
		BWAPI::UnitType game_unit_type;
		// A unique id (as returned from BWAPI).
		int id;
		// The name of the unit type.
		a_string name;
		// Dimensions of the unit bounding box, in order right, down, left, up.
		// The actual unit bounding box is
		// [(-dimension_left(), -dimension_up()),
		//  (dimension_right(), dimension_down())]
		std::array<int, 4> dimensions;
		int dimension_right() { return dimensions[0]; }
		int dimension_down() { return dimensions[1]; }
		int dimension_left() { return dimensions[2]; }
		int dimension_up() { return dimensions[3]; }
		// Whether this is a worker.
		bool is_worker;
		// Whether this is any kind of minerals.
		bool is_minerals;
		// Whether this is any kind of gas (natural geyser, refinery, assimilator or
		// extractor).
		bool is_gas;
		// For buildings, whether it can only be built on creep.
		bool requires_creep;
		// For buildings, whether it can only be built in a power field.
		bool requires_pylon;
		// One of race_terran, race_protoss, race_zerg or race_unknown.
		int race;
		// For buildings, the dimensions it occupies in counts of build squares (32x32
		// tiles).
		int tile_width, tile_height;
		// The cost to build this unit.
		double minerals_cost, gas_cost;
		// The total cost to build this unit, including the cost of anything it morphs
		// from (e.g. for Greater Spire it would be the sum of the individual costs of
		// Drone + Spire + Greater Spire).
		double total_minerals_cost, total_gas_cost;
		// The time, in frames, it takes to build this unit.
		int build_time;
		bool is_building;
		bool is_addon;
		bool is_resource_depot;
		// True for Refinery, Assimilator and Extractor.
		bool is_refinery;
		// The unit type that directly builds this unit (e.g. Larva for most Zerg
		// units, SCV for Terran buildings, Factory for Vultures).
		unit_type* builder_type;
		// A list of unit types that this unit can directly build.
		a_vector<unit_type*> builds;
		// Not sure if this is used for anything.
		bool build_near_me;
		// A list of prerequisite units that must be built before this unit can be
		// built (e.g. Spire for Mutalisk)
		a_vector<unit_type*> required_units;
		// A list of units where this unit is a prerequisite
		// (e.g. Mutalisk for Spire).
		a_vector<unit_type*> required_for;
		// Whether this unit can build non-flying units (e.g Factory, Gateway)
		bool produces_land_units;
		// This flag is set for units that produce land units or otherwise should
		// have an available space for units to "pop out" (like a bunker).
		// Used by the building placer to make sure we don't wall it in completely.
		bool needs_neighboring_free_space;
		// The available supply that this unit requires and consumes.
		double required_supply;
		// The amount that building this unit will increase our max supply.
		double provided_supply;
		bool is_flyer;
		enum { size_none, size_small, size_medium, size_large };
		// The unit size (one of the above enum), only used for damage calculations.
		int size;
		// This flag is set for Zerglings and Scourge.
		bool is_two_units_in_one_egg;
		// The maximum of the bounding box width and height. Previously used for
		// some space estimations, but currently unused afaik.
		double width;
		// How much space this unit occupies in e.g. bunkers or dropships.
		int space_required;
		// How much space this unit can carry (e.g. for bunkers and dropships).
		int space_provided;
		bool is_biological;
		bool is_mechanical;
		bool is_robotic;
		bool is_hovering;
		// This flag is set for certain units that can not be moved and is otherwise
		// not directly usable (e.g. Spider Mines, Larva, Eggs, all spell units).
		bool is_non_usable;
		bool is_detector;
		// Whether this is a building that can lift off.
		bool is_liftable;
	};

	// weapon_stats - information for a (weapon, player) pair. Bonuses from
	// upgrades are applied and regularly updated, which is why the player
	// is needed.
	struct weapon_stats {
		// BWAPI interface for this weapon
		BWAPI::WeaponType game_weapon_type;
		// The player from which we read upgrades.
		player_t* player;

		// The damage this weapon does, per hit. The number of hits per attack is
		// tied to the unit stats, not the weapon.
		double damage;
		// The cooldown between attacks, in frames.
		int cooldown;
		// The minimum range (inclusive) of this weapon.
		double min_range;
		// The maximum range (inclusive) of this weapon.
		double max_range;
		// Whether this weapon can hit air units.
		bool targets_air;
		// Whether this weapon can hit ground units.
		bool targets_ground;
		enum { damage_type_none, damage_type_concussive, damage_type_normal, damage_type_explosive };
		// The damage type (one of the above enum), used against the target unit size
		// for damage calculations.
		int damage_type;
		enum { explosion_type_none, explosion_type_radial_splash, explosion_type_enemy_splash, explosion_type_air_splash };
		// The explosion type, one of the above enum. Exactly how damage is calculated
		// for each of these is not entirely specified here.
		int explosion_type;
		// Splash radiuses, used by the splash explosion types.
		double inner_splash_radius, median_splash_radius, outer_splash_radius;
	};

	// unit_stats - information for a (unit_type, player) pair. Bonuses from
	// upgrades are applied and regularly updated, which is why the player
	// is needed.
	struct unit_stats {
		unit_type* type;
		player_t* player;

		// The maximum movement speed of this unit in pixels per frame.
		double max_speed;
		// The acceleration of this unit, in pixels per frame per frame.
		double acceleration;
		// The stop distance is the distance in pixels this unit takes to decelerate
		// from max_speed to 0.
		double stop_distance;
		// The turn rate is, in radians, how fast the unit turns per frame.
		double turn_rate;

		// The maximum amount of energy for this unit.
		double energy;
		// The maximum amount of shields for this unit.
		double shields;
		// The maximum amount of health for this unit.
		double hp;
		// The armor of this unit.
		double armor;

		// How far (in pixels) this unit can see.
		double sight_range;

		// This units weapon for hitting ground targets (null if none).
		weapon_stats* ground_weapon;
		// The number of hits per attack with the ground weapon.
		int ground_weapon_hits;
		// This units weapon for hitting air targets (null if none);
		weapon_stats* air_weapon;
		// The number of hits per attack with the air weapon.
		int air_weapon_hits;

	};

	// unit_building - this will be set in the unit::building field,
	// it's only set for buildings.
	struct unit_building {
		// The unit this is associated with
		unit* u;
		// Whether this building is currently flying.
		bool is_lifted;
		// The position (u->pos) of the building when it was last registered as
		// occupying any squares.
		xy walk_squares_occupied_pos;
		// A list of all the walk squares this building currently occupies (by 
		// bounding box).
		a_vector<walk_square*> walk_squares_occupied;
		// A list of all the build squares this building currently occupies (by
		// tile_width & tile_height)
		a_vector<build_square*> build_squares_occupied;
		// The position of the upper-left build squares this building occupies.
		xy build_pos;
		// The build_pos of the building when it was last registered as occupying
		// any squares.
		xy last_registered_pos;
		// Used by build to indicate that the building is busy building an addon.
		int building_addon_frame;
		// Set by wall_in for buildings that are part of the wall, but can be lifted.
		// Used by square_pathing to find a path through.
		bool is_liftable_wall;
		// Indicates that this building shouldn't be used for building units until the
		// specified frame (set by unit_controls for lifted buildings that are
		// moving).
		int nobuild_until;
	};

	// The units module, whose primary purpose is to update all the unit
	// information.
	class units_module {
	public:
		a_vector<unit*> unit_containers[16];

		a_vector<unit*>& all_units_ever = unit_containers[0];
		a_vector<unit*>& live_units = unit_containers[1];
		a_vector<unit*>& visible_units = unit_containers[2];
		a_vector<unit*>& invisible_units = unit_containers[3];
		a_vector<unit*>& visible_buildings = unit_containers[4];
		a_vector<unit*>& resource_units = unit_containers[5];

		a_vector<unit*>& my_units = unit_containers[6];
		a_vector<unit*>& my_workers = unit_containers[7];
		a_vector<unit*>& my_buildings = unit_containers[8];
		a_vector<unit*>& my_resource_depots = unit_containers[9];
		a_vector<unit*>& my_detector_units = unit_containers[10];

		a_vector<unit*>& enemy_units = unit_containers[11];
		a_vector<unit*>& visible_enemy_units = unit_containers[12];
		a_vector<unit*>& enemy_buildings = unit_containers[13];
		a_vector<unit*>& enemy_detector_units = unit_containers[14];

		a_vector<unit*>& rescuable_units = unit_containers[15];

		a_unordered_map<unit_type*, a_vector<unit*>> my_units_of_type;
		a_unordered_map<unit_type*, a_vector<unit*>> my_completed_units_of_type;

		// Any module can add their functions directly to these callback lists.
		// They will be called as part of the atomic updating of all units, thus
		// they should do as little work as possible, and is the first possible
		// location where unit updates can be seen.

		// A list of callbacks that will be called when a new unit is created.
		// This will not be called when a previously unseen unit is shown, only if
		// we see it being created. It will be called for all of our own units.
		a_vector<std::function<void(unit*)>> on_create_callbacks;
		// A list of callbacks that will be called when a unit morphs to another unit type.
		a_vector<std::function<void(unit*)>> on_morph_callbacks;
		// A list of callbacks that will be called when a unit is destroyed.
		a_vector<std::function<void(unit*)>> on_destroy_callbacks;
		// A list of callbacks that will be called when we spot a new unit.
		// This will be called the first time any unit is seen.
		a_vector<std::function<void(unit*)>> on_new_unit_callbacks;
		// A list of callbacks that will be called when a unit has changed its type.
		// If we lose visibility of a unit, and that unit morphs to another type,
		// this callback will be called as soon as the unit enter visibility again.
		a_vector<std::function<void(unit*, unit_type*)>> on_type_change_callbacks;

		// Get the unit associated with the specified BWAPI unit.
		unit* get_unit(BWAPI_Unit game_unit);



		// This should be called from the BWAPI onUnitShow handler.
		void on_unit_show(BWAPI_Unit game_unit);
		// This should be called from the BWAPI onUnitHide handler.
		void on_unit_hide(BWAPI_Unit game_unit);
		// This should be called from the BWAPI onUnitCreate handler.
		void on_unit_create(BWAPI_Unit game_unit);
		// This should be called from the BWAPI onUnitMorph handler.
		void on_unit_morph(BWAPI_Unit game_unit);
		// This should be called from the BWAPI onUnitDestroy handler.
		void on_unit_destroy(BWAPI_Unit game_unit);
		// This should be called from the BWAPI onUnitRenegade handler.
		void on_unit_renegade(BWAPI_Unit game_unit);
		// This should be called from the BWAPI onUnitComplete handler.
		void on_unit_complete(BWAPI_Unit game_unit);

		class impl_t;
		std::unique_ptr<impl_t> impl;
		units_module(bot_t& bot);
		~units_module();

		// Called from bot_t::init.
		void init();

	};

	struct unit {
		BWAPI_Unit game_unit;
		player_t* owner;
		unit_type* type;
		unit_stats* stats;
		unit_building*building;
		unit_controller* controller;

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
		unit* build_unit;
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
		player_t* last_type_change_owner;

		bool force_combat_unit;
		int last_attacked;
		unit*last_attack_target;
		int prev_weapon_cooldown;

		bool is_attacking;
		bool prev_is_attacking;
		int is_attacking_frame;

		a_vector<unit*> loaded_units;
		bool is_loaded;
		unit*loaded_into;
		int high_priority_until;
		bool is_flying;
		int spider_mine_count;
		bool has_nuke;
		int lockdown_timer;
		int maelstrom_timer;
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

		std::array<size_t, std::extent<decltype(units_module::unit_containers)>::value> container_indexes;
	};


	namespace unit_types {

		unit_type* get_unit_type(BWAPI::UnitType game_unit_type);

		typedef unit_type* unit_type_pointer;
		extern unit_type_pointer unknown;
		extern unit_type_pointer cc, supply_depot, barracks, factory, science_facility, nuclear_silo, bunker, refinery, machine_shop;
		extern unit_type_pointer missile_turret, academy, comsat_station, starport, control_tower, engineering_bay, armory, covert_ops;
		extern unit_type_pointer spell_scanner_sweep;
		extern unit_type_pointer scv, marine, vulture, siege_tank_tank_mode, siege_tank_siege_mode, goliath;
		extern unit_type_pointer medic, ghost, firebat;
		extern unit_type_pointer wraith, battlecruiser, dropship, science_vessel, valkyrie;
		extern unit_type_pointer spider_mine, nuclear_missile;
		extern unit_type_pointer nexus, pylon, gateway, photon_cannon, robotics_facility, stargate, forge, citadel_of_adun, templar_archives;
		extern unit_type_pointer fleet_beacon, assimilator, observatory, cybernetics_core, shield_battery;
		extern unit_type_pointer robotics_support_bay, arbiter_tribunal;
		extern unit_type_pointer probe;
		extern unit_type_pointer zealot, dragoon, dark_templar, high_templar, reaver;
		extern unit_type_pointer archon, dark_archon;
		extern unit_type_pointer observer, shuttle, scout, carrier, interceptor, arbiter, corsair;
		extern unit_type_pointer hatchery, lair, hive, creep_colony, sunken_colony, spore_colony, nydus_canal, spawning_pool, evolution_chamber;
		extern unit_type_pointer hydralisk_den, spire, greater_spire, extractor, ultralisk_cavern, defiler_mound;
		extern unit_type_pointer drone, overlord, zergling, larva, egg, hydralisk, lurker, lurker_egg, ultralisk, defiler, broodling;
		extern unit_type_pointer mutalisk, cocoon, scourge, queen, guardian, devourer;
		extern unit_type_pointer spell_dark_swarm;
		extern unit_type_pointer vespene_geyser;
		static std::reference_wrapper<unit_type_pointer> units_that_produce_land_units[] = {
			cc, barracks, factory,
			nexus, gateway, robotics_facility,
			hatchery, lair, hive
		};

		void init_global_data(bot_t& bot);
	}
}

#endif

