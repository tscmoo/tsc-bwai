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

	struct unit_type {
		BWAPI::UnitType game_unit_type;
		int id;
		a_string name;
		// right, down, left, up
		std::array<int, 4> dimensions;
		int dimension_right() { return dimensions[0]; }
		int dimension_down() { return dimensions[1]; }
		int dimension_left() { return dimensions[2]; }
		int dimension_up() { return dimensions[3]; }
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
		unit_type* builder_type;
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
		player_t* player;

		double damage;
		int cooldown;
		double min_range;
		double max_range;
		bool targets_air;
		bool targets_ground;
		enum { damage_type_none, damage_type_concussive, damage_type_normal, damage_type_explosive };
		int damage_type;
		enum { explosion_type_none, explosion_type_radial_splash, explosion_type_enemy_splash, explosion_type_air_splash };
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
		unit* u;
		bool is_lifted;
		xy walk_squares_occupied_pos;
		a_vector<walk_square*> walk_squares_occupied;
		a_vector<build_square*> build_squares_occupied;
		xy build_pos;
		xy last_registered_pos;
		int building_addon_frame;
		bool is_liftable_wall;
		int nobuild_until;
	};

	struct units_t {

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

		void on_unit_show(BWAPI_Unit game_unit);
		void on_unit_hide(BWAPI_Unit game_unit);
		void on_unit_create(BWAPI_Unit game_unit);
		void on_unit_morph(BWAPI_Unit game_unit);
		void on_unit_destroy(BWAPI_Unit game_unit);
		void on_unit_renegade(BWAPI_Unit game_unit);
		void on_unit_complete(BWAPI_Unit game_unit);

		struct impl_t;
		std::unique_ptr<impl_t> impl;
		units_t(bot_t& bot);
		~units_t();

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

		std::array<size_t, std::extent<decltype(units_t::unit_containers)>::value> container_indexes;
	};


	namespace unit_types {
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

