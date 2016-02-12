//
// This file defines the interface for upgrades, as well as all the available
// upgrades in upgrade_types.
//

#include "upgrades.h"
#include "common.h"
#include "units.h"
#include "bwapi_inc.h"

namespace tsc_bwai {

	namespace {

		a_list<upgrade_type> upgrade_type_container;

		a_map<std::tuple<BWAPI::UpgradeType, int>, upgrade_type*> upgrade_type_map;
		a_map<BWAPI::TechType, upgrade_type*> tech_type_map;

		upgrade_type* get_or_add_upgrade_type(BWAPI::UpgradeType game_upgrade_type, int level = 1) {
			upgrade_type*& r = upgrade_type_map[std::make_tuple(game_upgrade_type, level)];
			if (r) return r;
			upgrade_type_container.emplace_back();
			r = &upgrade_type_container.back();
			r->game_upgrade_type = game_upgrade_type;

			r->minerals_cost = game_upgrade_type.mineralPrice() + game_upgrade_type.mineralPriceFactor()*(level - 1);
			r->gas_cost = game_upgrade_type.gasPrice() + game_upgrade_type.gasPriceFactor()*(level - 1);
			r->builder_type = unit_types::get_unit_type(game_upgrade_type.whatUpgrades());
			r->build_time = game_upgrade_type.upgradeTime() + game_upgrade_type.upgradeTimeFactor()*(level - 1);
			unit_type* req = unit_types::get_unit_type(game_upgrade_type.whatsRequired(level));
			if (r->builder_type) r->required_units.push_back(r->builder_type);
			if (req) r->required_units.push_back(req);
			r->name = game_upgrade_type.getName().c_str();
			if (game_upgrade_type.maxRepeats() > 1) r->name += format(" level %d", level);
			r->level = level;
			r->prev = level == 1 ? nullptr : get_or_add_upgrade_type(game_upgrade_type, level - 1);
			for (auto& v : game_upgrade_type.whatUses()) {
				unit_type* ut = unit_types::get_unit_type(v);
				if (ut) r->what_uses.push_back(ut);
			}
			return r;
		}
		upgrade_type* get_or_add_upgrade_type(BWAPI::TechType game_tech_type) {
			upgrade_type*& r = tech_type_map[game_tech_type];
			if (r) return r;
			upgrade_type_container.emplace_back();
			r = &upgrade_type_container.back();
			r->game_tech_type = game_tech_type;

			r->minerals_cost = game_tech_type.mineralPrice();
			r->gas_cost = game_tech_type.gasPrice();
			r->builder_type = unit_types::get_unit_type(game_tech_type.whatResearches());
			r->build_time = game_tech_type.researchTime();
			if (r->builder_type) r->required_units.push_back(r->builder_type);
			if (game_tech_type == BWAPI::TechTypes::Lurker_Aspect) r->required_units.push_back(unit_types::lair);
			r->name = game_tech_type.getName().c_str();
			r->level = 1;
			r->prev = nullptr;
			for (auto& v : game_tech_type.whatUses()) {
				unit_type* ut = unit_types::get_unit_type(v);
				if (ut) r->what_uses.push_back(ut);
			}
			return r;
		}
	}

	namespace upgrade_types {
		void get(upgrade_type*&r, BWAPI::UpgradeType game_upgrade_type, int level = 1) {
			r = get_or_add_upgrade_type(game_upgrade_type, level);
		}
		void get(upgrade_type*&r, BWAPI::TechType game_tech_type) {
			r = get_or_add_upgrade_type(game_tech_type);
		}
		upgrade_type_pointer terran_infantry_weapons_1, terran_infantry_weapons_2, terran_infantry_weapons_3;
		upgrade_type_pointer terran_infantry_armor_1, terran_infantry_armor_2, terran_infantry_armor_3;
		upgrade_type_pointer terran_vehicle_weapons_1, terran_vehicle_weapons_2, terran_vehicle_weapons_3;
		upgrade_type_pointer terran_vehicle_plating_1, terran_vehicle_plating_2, terran_vehicle_plating_3;
		upgrade_type_pointer terran_ship_weapons_1, terran_ship_weapons_2, terran_ship_weapons_3;
		upgrade_type_pointer terran_ship_plating_1, terran_ship_plating_2, terran_ship_plating_3;

		upgrade_type_pointer ion_thrusters;
		upgrade_type_pointer siege_mode;
		upgrade_type_pointer u_238_shells, stim_packs;
		upgrade_type_pointer healing, restoration, optical_flare, caduceus_reactor;
		upgrade_type_pointer ocular_implants, moebius_reactor, lockdown, personal_cloaking, nuclear_strike;
		upgrade_type_pointer spider_mines;
		upgrade_type_pointer charon_boosters;
		upgrade_type_pointer defensive_matrix, emp_shockwave, irradiate, titan_reactor;
		upgrade_type_pointer cloaking_field;
		upgrade_type_pointer yamato_gun;

		upgrade_type_pointer zerg_melee_attacks_1, zerg_melee_attacks_2, zerg_melee_attacks_3;
		upgrade_type_pointer zerg_missile_attacks_1, zerg_missile_attacks_2, zerg_missile_attacks_3;
		upgrade_type_pointer zerg_carapace_1, zerg_carapace_2, zerg_carapace_3;
		upgrade_type_pointer zerg_flyer_attacks_1, zerg_flyer_attacks_2, zerg_flyer_attacks_3;
		upgrade_type_pointer zerg_flyer_carapace_1, zerg_flyer_carapace_2, zerg_flyer_carapace_3;

		upgrade_type_pointer metabolic_boost, adrenal_glands;
		upgrade_type_pointer muscular_augments, grooved_spines, lurker_aspect;
		upgrade_type_pointer anabolic_synthesis, chitinous_plating;
		upgrade_type_pointer infestation, parasite, ensnare, spawn_broodling, gamete_meiosis;
		upgrade_type_pointer ventral_sacs, antennae, pneumatized_carapace;
		upgrade_type_pointer dark_swarm, consume, plague, metasynaptic_node;
		upgrade_type_pointer burrow;

		upgrade_type_pointer protoss_ground_weapons_1, protoss_ground_weapons_2, protoss_ground_weapons_3;
		upgrade_type_pointer protoss_ground_armor_1, protoss_ground_armor_2, protoss_ground_armor_3;
		upgrade_type_pointer protoss_plasma_shields_1, protoss_plasma_shields_2, protoss_plasma_shields_3;
		upgrade_type_pointer protoss_air_weapons_1, protoss_air_weapons_2, protoss_air_weapons_3;
		upgrade_type_pointer protoss_air_armor_1, protoss_air_armor_2, protoss_air_armor_3;

		upgrade_type_pointer singularity_charge, leg_enhancements;
		upgrade_type_pointer gravitic_drive;
		upgrade_type_pointer scarab_damage, reaver_capacity;
		upgrade_type_pointer stasis_field, recall, khaydarin_core;
		upgrade_type_pointer psionic_storm, hallucination, khaydarin_amulet;
		upgrade_type_pointer feedback, maelstrom, mind_control, argus_talisman;
		upgrade_type_pointer carrier_capacity;
		upgrade_type_pointer apial_sensors, gravitic_thrusters;
		upgrade_type_pointer sensor_array, gravitic_boosters;

		void init_upgrade_types() {
			get(terran_infantry_weapons_1, BWAPI::UpgradeTypes::Terran_Infantry_Weapons, 1);
			get(terran_infantry_weapons_2, BWAPI::UpgradeTypes::Terran_Infantry_Weapons, 2);
			get(terran_infantry_weapons_3, BWAPI::UpgradeTypes::Terran_Infantry_Weapons, 3);
			get(terran_infantry_armor_1, BWAPI::UpgradeTypes::Terran_Infantry_Armor, 1);
			get(terran_infantry_armor_2, BWAPI::UpgradeTypes::Terran_Infantry_Armor, 2);
			get(terran_infantry_armor_3, BWAPI::UpgradeTypes::Terran_Infantry_Armor, 3);

			get(terran_vehicle_weapons_1, BWAPI::UpgradeTypes::Terran_Vehicle_Weapons, 1);
			get(terran_vehicle_weapons_2, BWAPI::UpgradeTypes::Terran_Vehicle_Weapons, 2);
			get(terran_vehicle_weapons_3, BWAPI::UpgradeTypes::Terran_Vehicle_Weapons, 3);
			get(terran_vehicle_plating_1, BWAPI::UpgradeTypes::Terran_Vehicle_Plating, 1);
			get(terran_vehicle_plating_2, BWAPI::UpgradeTypes::Terran_Vehicle_Plating, 2);
			get(terran_vehicle_plating_3, BWAPI::UpgradeTypes::Terran_Vehicle_Plating, 3);

			get(terran_ship_weapons_1, BWAPI::UpgradeTypes::Terran_Ship_Weapons, 1);
			get(terran_ship_weapons_2, BWAPI::UpgradeTypes::Terran_Ship_Weapons, 2);
			get(terran_ship_weapons_3, BWAPI::UpgradeTypes::Terran_Ship_Weapons, 3);
			get(terran_ship_plating_1, BWAPI::UpgradeTypes::Terran_Ship_Plating, 1);
			get(terran_ship_plating_2, BWAPI::UpgradeTypes::Terran_Ship_Plating, 2);
			get(terran_ship_plating_3, BWAPI::UpgradeTypes::Terran_Ship_Plating, 3);

			get(ion_thrusters, BWAPI::UpgradeTypes::Ion_Thrusters);
			get(siege_mode, BWAPI::TechTypes::Tank_Siege_Mode);
			get(u_238_shells, BWAPI::UpgradeTypes::U_238_Shells);
			get(stim_packs, BWAPI::TechTypes::Stim_Packs);
			get(healing, BWAPI::TechTypes::Healing);
			get(restoration, BWAPI::TechTypes::Restoration);
			get(optical_flare, BWAPI::TechTypes::Optical_Flare);
			get(caduceus_reactor, BWAPI::UpgradeTypes::Caduceus_Reactor);
			get(ocular_implants, BWAPI::UpgradeTypes::Ocular_Implants);
			get(moebius_reactor, BWAPI::UpgradeTypes::Moebius_Reactor);
			get(lockdown, BWAPI::TechTypes::Lockdown);
			get(personal_cloaking, BWAPI::TechTypes::Personnel_Cloaking);
			get(nuclear_strike, BWAPI::TechTypes::Nuclear_Strike);
			get(spider_mines, BWAPI::TechTypes::Spider_Mines);
			get(charon_boosters, BWAPI::UpgradeTypes::Charon_Boosters);
			get(defensive_matrix, BWAPI::TechTypes::Defensive_Matrix);
			get(emp_shockwave, BWAPI::TechTypes::EMP_Shockwave);
			get(irradiate, BWAPI::TechTypes::Irradiate);
			get(titan_reactor, BWAPI::UpgradeTypes::Titan_Reactor);
			get(cloaking_field, BWAPI::TechTypes::Cloaking_Field);
			get(yamato_gun, BWAPI::TechTypes::Yamato_Gun);

			get(zerg_melee_attacks_1, BWAPI::UpgradeTypes::Zerg_Melee_Attacks, 1);
			get(zerg_melee_attacks_2, BWAPI::UpgradeTypes::Zerg_Melee_Attacks, 2);
			get(zerg_melee_attacks_3, BWAPI::UpgradeTypes::Zerg_Melee_Attacks, 3);
			get(zerg_missile_attacks_1, BWAPI::UpgradeTypes::Zerg_Missile_Attacks, 1);
			get(zerg_missile_attacks_2, BWAPI::UpgradeTypes::Zerg_Missile_Attacks, 2);
			get(zerg_missile_attacks_3, BWAPI::UpgradeTypes::Zerg_Missile_Attacks, 3);
			get(zerg_carapace_1, BWAPI::UpgradeTypes::Zerg_Carapace, 1);
			get(zerg_carapace_2, BWAPI::UpgradeTypes::Zerg_Carapace, 2);
			get(zerg_carapace_3, BWAPI::UpgradeTypes::Zerg_Carapace, 3);
			get(zerg_flyer_attacks_1, BWAPI::UpgradeTypes::Zerg_Flyer_Attacks, 1);
			get(zerg_flyer_attacks_1, BWAPI::UpgradeTypes::Zerg_Flyer_Attacks, 2);
			get(zerg_flyer_attacks_1, BWAPI::UpgradeTypes::Zerg_Flyer_Attacks, 3);
			get(zerg_flyer_carapace_1, BWAPI::UpgradeTypes::Zerg_Flyer_Carapace, 1);
			get(zerg_flyer_carapace_2, BWAPI::UpgradeTypes::Zerg_Flyer_Carapace, 2);
			get(zerg_flyer_carapace_3, BWAPI::UpgradeTypes::Zerg_Flyer_Carapace, 3);

			get(metabolic_boost, BWAPI::UpgradeTypes::Metabolic_Boost);
			get(adrenal_glands, BWAPI::UpgradeTypes::Adrenal_Glands);
			get(muscular_augments, BWAPI::UpgradeTypes::Muscular_Augments);
			get(grooved_spines, BWAPI::UpgradeTypes::Grooved_Spines);
			get(lurker_aspect, BWAPI::TechTypes::Lurker_Aspect);
			get(anabolic_synthesis, BWAPI::UpgradeTypes::Anabolic_Synthesis);
			get(chitinous_plating, BWAPI::UpgradeTypes::Chitinous_Plating);
			get(infestation, BWAPI::TechTypes::Infestation);
			get(parasite, BWAPI::TechTypes::Parasite);
			get(ensnare, BWAPI::TechTypes::Ensnare);
			get(spawn_broodling, BWAPI::TechTypes::Spawn_Broodlings);
			get(gamete_meiosis, BWAPI::UpgradeTypes::Gamete_Meiosis);
			get(ventral_sacs, BWAPI::UpgradeTypes::Ventral_Sacs);
			get(antennae, BWAPI::UpgradeTypes::Antennae);
			get(pneumatized_carapace, BWAPI::UpgradeTypes::Pneumatized_Carapace);
			get(dark_swarm, BWAPI::TechTypes::Dark_Swarm);
			get(consume, BWAPI::TechTypes::Consume);
			get(plague, BWAPI::TechTypes::Plague);
			get(metasynaptic_node, BWAPI::UpgradeTypes::Metasynaptic_Node);
			get(burrow, BWAPI::TechTypes::Burrowing);

			get(protoss_ground_weapons_1, BWAPI::UpgradeTypes::Protoss_Ground_Weapons, 1);
			get(protoss_ground_weapons_2, BWAPI::UpgradeTypes::Protoss_Ground_Weapons, 2);
			get(protoss_ground_weapons_3, BWAPI::UpgradeTypes::Protoss_Ground_Weapons, 3);
			get(protoss_ground_armor_1, BWAPI::UpgradeTypes::Protoss_Ground_Armor, 1);
			get(protoss_ground_armor_2, BWAPI::UpgradeTypes::Protoss_Ground_Armor, 2);
			get(protoss_ground_armor_3, BWAPI::UpgradeTypes::Protoss_Ground_Armor, 3);
			get(protoss_plasma_shields_1, BWAPI::UpgradeTypes::Protoss_Plasma_Shields, 1);
			get(protoss_plasma_shields_2, BWAPI::UpgradeTypes::Protoss_Plasma_Shields, 2);
			get(protoss_plasma_shields_3, BWAPI::UpgradeTypes::Protoss_Plasma_Shields, 3);
			get(protoss_air_weapons_1, BWAPI::UpgradeTypes::Protoss_Air_Weapons, 1);
			get(protoss_air_weapons_2, BWAPI::UpgradeTypes::Protoss_Air_Weapons, 2);
			get(protoss_air_weapons_3, BWAPI::UpgradeTypes::Protoss_Air_Weapons, 3);
			get(protoss_air_armor_1, BWAPI::UpgradeTypes::Protoss_Air_Armor, 1);
			get(protoss_air_armor_2, BWAPI::UpgradeTypes::Protoss_Air_Armor, 2);
			get(protoss_air_armor_3, BWAPI::UpgradeTypes::Protoss_Air_Armor, 3);

			get(singularity_charge, BWAPI::UpgradeTypes::Singularity_Charge);
			get(leg_enhancements, BWAPI::UpgradeTypes::Leg_Enhancements);
			get(gravitic_drive, BWAPI::UpgradeTypes::Gravitic_Drive);
			get(scarab_damage, BWAPI::UpgradeTypes::Scarab_Damage);
			get(reaver_capacity, BWAPI::UpgradeTypes::Reaver_Capacity);
			get(stasis_field, BWAPI::TechTypes::Stasis_Field);
			get(recall, BWAPI::TechTypes::Recall);
			get(khaydarin_core, BWAPI::UpgradeTypes::Khaydarin_Core);
			get(psionic_storm, BWAPI::TechTypes::Psionic_Storm);
			get(hallucination, BWAPI::TechTypes::Hallucination);
			get(khaydarin_amulet, BWAPI::UpgradeTypes::Khaydarin_Amulet);
			get(feedback, BWAPI::TechTypes::Feedback);
			get(maelstrom, BWAPI::TechTypes::Maelstrom);
			get(mind_control, BWAPI::TechTypes::Mind_Control);
			get(argus_talisman, BWAPI::UpgradeTypes::Argus_Talisman);
			get(carrier_capacity, BWAPI::UpgradeTypes::Carrier_Capacity);
			get(apial_sensors, BWAPI::UpgradeTypes::Apial_Sensors);
			get(gravitic_thrusters, BWAPI::UpgradeTypes::Gravitic_Thrusters);
			get(sensor_array, BWAPI::UpgradeTypes::Sensor_Array);
			get(gravitic_boosters, BWAPI::UpgradeTypes::Gravitic_Boosters);
		}

		void init_global_data(bot_t& bot) {

			init_upgrade_types();
		
		}
	}

}
