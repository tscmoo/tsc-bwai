//
// This file declares the interface for upgrades, as well as all the available
// upgrades in upgrade_types.
//


#ifndef TSC_BWAI_UPGRADES_H
#define TSC_BWAI_UPGRADES_H

#include "common.h"
#include "bwapi_inc.h"

namespace tsc_bwai {

	struct unit_type;
	struct upgrade_type;

	struct upgrade_type {
		BWAPI::UpgradeType game_upgrade_type;
		BWAPI::TechType game_tech_type;

		double minerals_cost, gas_cost;
		unit_type* builder_type;
		int build_time;
		a_vector<unit_type*> required_units;
		a_string name;
		int level;
		upgrade_type* prev;
		a_vector<unit_type*> what_uses;
	};
	namespace upgrade_types {
		typedef upgrade_type* upgrade_type_pointer;
		extern upgrade_type_pointer terran_infantry_weapons_1, terran_infantry_weapons_2, terran_infantry_weapons_3;
		extern upgrade_type_pointer terran_infantry_armor_1, terran_infantry_armor_2, terran_infantry_armor_3;
		extern upgrade_type_pointer terran_vehicle_weapons_1, terran_vehicle_weapons_2, terran_vehicle_weapons_3;
		extern upgrade_type_pointer terran_vehicle_plating_1, terran_vehicle_plating_2, terran_vehicle_plating_3;
		extern upgrade_type_pointer terran_ship_weapons_1, terran_ship_weapons_2, terran_ship_weapons_3;
		extern upgrade_type_pointer terran_ship_plating_1, terran_ship_plating_2, terran_ship_plating_3;

		extern upgrade_type_pointer ion_thrusters;
		extern upgrade_type_pointer siege_mode;
		extern upgrade_type_pointer u_238_shells, stim_packs;
		extern upgrade_type_pointer healing, restoration, optical_flare, caduceus_reactor;
		extern upgrade_type_pointer ocular_implants, moebius_reactor, lockdown, personal_cloaking, nuclear_strike;
		extern upgrade_type_pointer spider_mines;
		extern upgrade_type_pointer charon_boosters;
		extern upgrade_type_pointer defensive_matrix, emp_shockwave, irradiate, titan_reactor;
		extern upgrade_type_pointer cloaking_field;
		extern upgrade_type_pointer yamato_gun;

		extern upgrade_type_pointer zerg_melee_attacks_1, zerg_melee_attacks_2, zerg_melee_attacks_3;
		extern upgrade_type_pointer zerg_missile_attacks_1, zerg_missile_attacks_2, zerg_missile_attacks_3;
		extern upgrade_type_pointer zerg_carapace_1, zerg_carapace_2, zerg_carapace_3;
		extern upgrade_type_pointer zerg_flyer_attacks_1, zerg_flyer_attacks_2, zerg_flyer_attacks_3;
		extern upgrade_type_pointer zerg_flyer_carapace_1, zerg_flyer_carapace_2, zerg_flyer_carapace_3;

		extern upgrade_type_pointer metabolic_boost, adrenal_glands;
		extern upgrade_type_pointer muscular_augments, grooved_spines, lurker_aspect;
		extern upgrade_type_pointer anabolic_synthesis, chitinous_plating;
		extern upgrade_type_pointer infestation, parasite, ensnare, spawn_broodling, gamete_meiosis;
		extern upgrade_type_pointer ventral_sacs, antennae, pneumatized_carapace;
		extern upgrade_type_pointer dark_swarm, consume, plague, metasynaptic_node;
		extern upgrade_type_pointer burrow;

		extern upgrade_type_pointer protoss_ground_weapons_1, protoss_ground_weapons_2, protoss_ground_weapons_3;
		extern upgrade_type_pointer protoss_ground_armor_1, protoss_ground_armor_2, protoss_ground_armor_3;
		extern upgrade_type_pointer protoss_plasma_shields_1, protoss_plasma_shields_2, protoss_plasma_shields_3;
		extern upgrade_type_pointer protoss_air_weapons_1, protoss_air_weapons_2, protoss_air_weapons_3;
		extern upgrade_type_pointer protoss_air_armor_1, protoss_air_armor_2, protoss_air_armor_3;

		extern upgrade_type_pointer singularity_charge, leg_enhancements;
		extern upgrade_type_pointer gravitic_drive;
		extern upgrade_type_pointer scarab_damage, reaver_capacity;
		extern upgrade_type_pointer stasis_field, recall, khaydarin_core;
		extern upgrade_type_pointer psionic_storm, hallucination, khaydarin_amulet;
		extern upgrade_type_pointer feedback, maelstrom, mind_control, argus_talisman;
		extern upgrade_type_pointer carrier_capacity;
		extern upgrade_type_pointer apial_sensors, gravitic_thrusters;
		extern upgrade_type_pointer sensor_array, gravitic_boosters;

		void init_global_data(bot_t& bot);
	}

}


#endif
