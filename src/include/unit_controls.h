//
// This file defines the unit_controls module, which is responsible for issuing
// orders to units, especially as it regards movement and attacking. Some orders
// (like spell usage and building things) are also issued by build and combat.
//

#ifndef TSC_BWAI_UNIT_CONTROLS_H
#define TSC_BWAI_UNIT_CONTROLS_H

#include "common.h"

namespace tsc_bwai {

	struct unit;
	struct unit_type;
	struct upgrade_type;

	// unit_controller - This structure exists for all controllable units,
	// and keeps track of related state information.
	// Some of the fields are set by other modules (mostly by combat or build),
	// but most are used internally by unit_controls.
	struct unit_controller {
		// The associated unit.
		unit* u = nullptr;
		enum { action_idle, action_move, action_gather, action_build, action_attack, action_kite, action_scout, action_move_directly, action_use_ability, action_repair, action_building_movement, action_recharge, action_worker_micro };
		// The current action the unit is performing, which is the lowest level at
		// which units can be told what to do (other than issuing orders directly).
		// It is one of the action_* enums that live in unit_controller, described as follows:
		//   action_idle   - Does nothing.
		//   action_move   - Move command with pathfinding.
		//     go_to        - The destination.
		//   action_gather - Gather resources.
		//     target       - The target minerals or gas that should be gathered from (will not gather if null).
		//     depot        - The depot that resources should be returned to (will not return if null)
		//   action_build  - Specifically for workers to build a building.
		//     target_type  - The type of the building to be built.
		//     target_pos   - The position of the build square in the upper left corner where we want to build.
		//     wait_until   - Wait at the build site, and don't build until frame wait_until.
		//   action_attack - Attack a target. If the target is dead, action will reset to action_idle.
		//                   This action can also be set for medics, who will use their healing.
		//     target       - The target to attack.
		//   action_kite   - Attack while kiting. combat will set either attack or kite for units depending
		//                   on attack/retreat and weapon ranges. For some unit types they behave very similar
		//                   or identical.
		//     target       - The unit we are kiting from (why do we need to know this?).
		//     kite_attack_target - The unit to actually attack (couldn't this be target?).
		//     target_pos   - The destination to move to when kiting. This should be calculated as some location
		//                    away from the enemy units.
		//   action_scout  - This is set by scouting, and behaves the same as action_move except that if the unit
		//                   is carrying any resources, then it will return them to a resource depot before moving.
		//                   It mostly exists as a priority action that other modules check for and make sure they
		//                   don't try to use the unit for anything else (could be handled better).
		//     go_to        - The destination.
		//   action_move_directly - Move command without pathfinding (Broodwar still still do pathfinding).
		//     go_to        - The destination.
		//   action_use_ability - Deprecated.
		//   action_repair  - Repair the target. Will pathfind to the target if it's not very close.
		//     target        - The target unit to repair.
		//   action_building_movement - Not entirely fleshed out.
		//   action_recharge - Orders the unit to recharge shields at the nearest shield battery.
		//   action_worker_micro - Enables some special micro where units stack up at mineral fields to defend
		//                         very early rushes like a 4 pool.
		int action = action_idle;
		xy go_to;
		unit* target = nullptr;
		unit* kite_attack_target = nullptr;
		unit* depot = nullptr;
		xy target_pos;
		unit_type* target_type = nullptr;
		upgrade_type* ability = nullptr;

		// Not used directly by unit_controls, the idea was that it should be set
		// by other modules to gain ownership of this unit, but I believe it is
		// currently unused.
		void* flag = nullptr;

		// Whether this unit can be issued movement commands (and it will respond).
		bool can_move = false;
		// The most recent frame the move function was called for this unit.
		int last_move = 0;
		// The most recent position this unit was issued a move command to.
		xy last_move_to_pos;
		// The most recent frame this unit was issued a move command.
		int last_move_to = 0;
		// Deprecated.
		int move_order = 0;
		// Specifies that this unit should be issued no orders until the specified
		// frame. Not respected by attack micro, since it requires very specific
		// timing. This field should be checked and set whenever a direct unit order
		// is issued, to prevent apm spam.
		int noorder_until = 0;
		// This is used in addition to noorder_until, for "special" orders.
		// It is currently only used for stim, to ensure that we do not stim twice
		// because of high latency.
		int nospecial_until = 0;
		// Used by action_build.
		int wait_until = 0;
		// The most recent frame at which this unit reached go_to. This is used
		// in pathfinding to resolve collisions or bottlenecks.
		int last_reached_go_to = 0;
		// This is incremented by action_build when it fails to build but we don't see
		// anything blocking it. It will eventually cause scouting to scan or send
		// detection since it's likely there is a burrowed or cloaked unit blocking it.
		int fail_build_count = 0;
		// This will be set for units that are blocking a location that we want to
		// build something, and will cause them to move away. Takes priority over most
		// actions.
		int move_away_until = 0;
		// The position to move away from.
		xy move_away_from;
		// Deprecated.
		int at_go_to_counter = 0;
		// The most recent frame u->game_unit->siege() was called.
		// Used to help decided when to siege and unsiege.
		int last_siege = 0;
		// The most recent frame u->game_unit->returnCargo() was issued.
		// Used to re-issue the returnCargo command if it has been active
		// for a while, since there might be a new nearest resource depot.
		int last_return_cargo = 0;
		// Used by action_building_movement.
		bool lift = false;
		// Used by action_building_movement.
		int timeout = 0;
		// Used internally by attack micro.
		int attack_state = 0;
		// Used internally by attack micro.
		int attack_timer = 0;
		// Used internally by attack micro.
		int attack_state2 = 0;
		// Used for debugging how long a weapon was off cooldown when attacking
		// (wasted frames).
		int weapon_ready_frames = 0;
		// The most recent frame this controller was processed. Used to ensure
		// we process each controller only once per frame, since there's a few
		// stages that handle different actions.
		int last_process = 0;
		// Used for detecting when a unit has a move order, but is not moving.
		// Sometimes you need to issue a stop order to get it to respond again.
		int not_moving_counter = 0;
		// The last command frame (current_command_frame) this unit was issued
		// an order. Issuing multiple orders in the same command frame will invalidate
		// the earlier issued ones, so it should be avoided.
		int last_command_frame = 0;
		// Used by re_evaluate_target to cache results.
		int last_re_evaluate_target = 0;
		// Used by re_evaluate_target to cache results.
		unit* re_evaluate_target = nullptr;
		// Used by re_evaluate_target to cache results.
		xy re_evalute_move_to;
		// Set by combat to have units form a concave formation.
		xy defensive_concave_position;
		// Used by wait_for_return_resources as a timeout.
		int return_minerals_frame = 0;
		// The most recent target we issued a gather order to.
		unit* last_gather_target = nullptr;
		// The most recent frame we issued a gather order.
		int last_gather_issued_frame = 0;
		// The most recent frame a burrow order was issued.
		int last_burrow = 0;
		// These fields are used by shuttle_micro.
 		int last_pickup = 0;
		int last_unload = 0;
		double incoming_damage;
		unit* pickup_to = nullptr;
		int no_unload_until = 0;
	};

	class unit_controls_module {
		bot_t& bot;

		a_deque<unit_controller> unit_controller_container;

		void move(unit_controller* c);
		void move_away(unit_controller* c);
		bool move_stuff_away_from_build_pos(xy build_pos, const unit_type* ut, unit_controller* builder);
		void shuttle_micro(unit_controller* c);
		void issue_gather_order(unit_controller* c);
		void worker_micro(const a_vector<unit_controller*>& controllers);
		void process(const a_vector<unit_controller*>& controllers);
		void control_units_task();

	public:
		unit_controls_module(bot_t& bot) : bot(bot) {}

		unit_controller* get_unit_controller(unit* u);

		void init();
	};

}
#endif
