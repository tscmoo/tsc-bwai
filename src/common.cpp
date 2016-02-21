//
// This file implements the common globals, and maybe some other common things.
//


#include "common.h"
#include "units.h"
#include "bot.h"
using namespace tsc_bwai;

int tsc_bwai::frames_to_reach(double initial_speed, double acceleration, double max_speed, double stop_distance, double distance) {
	int f = 0;
	double s = initial_speed;
	double d = distance;
	if (d / max_speed >= 15 * 600) return 15 * 600;
	while (d > 0) {
		if (d < stop_distance*((s - acceleration) / max_speed)) s -= acceleration;
		else if (s < max_speed) s += acceleration;
		if (s > max_speed) s = max_speed;
		if (s < acceleration) s = acceleration;
		if (s >= max_speed) {
			double dd = std::max(d, stop_distance) - stop_distance;
			if (dd == 0) {
				d -= s;
				++f;
			} else {
				int t = std::max((int)(dd / s), 1);
				d -= t*s;
				f += t;
			}
		} else {
			d -= s;
			++f;
		}
	}

	return f;
}
int tsc_bwai::frames_to_reach(const unit_stats* stats, double initial_speed, double distance) {
	return frames_to_reach(initial_speed, stats->acceleration, stats->max_speed, stats->stop_distance, distance);
}
int tsc_bwai::frames_to_reach(const unit* u, double distance) {
	return frames_to_reach(u->stats, u->speed, distance);
}

double tsc_bwai::diag_distance(xy pos) {
	double d = std::min(std::abs(pos.x), std::abs(pos.y));
	double s = std::abs(pos.x) + std::abs(pos.y);

	return d*1.4142135623730950488016887242097 + (s - d * 2);
}

double tsc_bwai::manhattan_distance(xy pos) {
	return std::abs(pos.x) + std::abs(pos.y);
}

xy tsc_bwai::units_difference(xy a_pos, std::array<int, 4> a_dimensions, xy b_pos, std::array<int, 4> b_dimensions) {
	xy a0 = a_pos + xy(-a_dimensions[2], -a_dimensions[3]);
	xy a1 = a_pos + xy(a_dimensions[0], a_dimensions[1]);
	xy b0 = b_pos + xy(-b_dimensions[2], -b_dimensions[3]);
	xy b1 = b_pos + xy(b_dimensions[0], b_dimensions[1]);

	int x, y;
	if (a0.x > b1.x) x = a0.x - b1.x;
	else if (b0.x > a1.x) x = b0.x - a1.x;
	else x = 0;
	if (a0.y > b1.y) y = a0.y - b1.y;
	else if (b0.y > a1.y) y = b0.y - a1.y;
	else y = 0;

	return xy(x, y);
}

double tsc_bwai::units_distance(const unit* a, const unit* b) {
	return units_difference(a->pos, a->type->dimensions, b->pos, b->type->dimensions).length();
}

double tsc_bwai::units_distance(xy a_pos, const unit* a, xy b_pos, const unit* b) {
	return units_difference(a_pos, a->type->dimensions, b_pos, b->type->dimensions).length();
}
double tsc_bwai::units_distance(xy a_pos, const unit_type* at, xy b_pos, const unit_type* bt) {
	return units_difference(a_pos, at->dimensions, b_pos, bt->dimensions).length();
}

xy tsc_bwai::nearest_spot_in_square(xy pos, xy top_left, xy bottom_right) {
	if (top_left.x > pos.x) pos.x = top_left.x;
	else if (bottom_right.x < pos.x) pos.x = bottom_right.x;
	if (top_left.y > pos.y) pos.y = top_left.y;
	else if (bottom_right.y < pos.y) pos.y = bottom_right.y;
	return pos;
}

a_string tsc_bwai::short_type_name(const unit_type* type) {
	if (type->game_unit_type == BWAPI::UnitTypes::Terran_Command_Center) return "CC";
	if (type->game_unit_type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) return "Tank";
	const a_string& s = type->name;
	if (s.find("Terran ") == 0) return s.substr(7);
	if (s.find("Protoss ") == 0) return s.substr(8);
	if (s.find("Zerg ") == 0) return s.substr(5);
	if (s.find("Terran_") == 0) return s.substr(7);
	if (s.find("Protoss_") == 0) return s.substr(8);
	if (s.find("Zerg_") == 0) return s.substr(5);
	return s;
}



double common_module::unit_pathing_distance(const unit* u, xy to) {
	return unit_pathing_distance(u->type, u->pos, to);
}

double common_module::unit_pathing_distance(const unit_type* ut, xy from, xy to) {
	if (ut->is_flyer) return (to - from).length();
	auto& map = bot.square_pathing.get_pathing_map(ut);
	return bot.square_pathing.get_distance(map, from, to);
}

double common_module::units_pathing_distance(const unit* a, const unit* b) {
	if (a->type->is_flyer) return units_difference(a->pos, a->type->dimensions, b->pos, b->type->dimensions).length();
	double ud = units_difference(a->pos, a->type->dimensions, b->pos, b->type->dimensions).length();
	if (ud <= 32) return ud;
	auto& map = bot.square_pathing.get_pathing_map(a->type);
	double d = bot.square_pathing.get_distance(map, a->pos, b->pos);
	if (d > 32 * 4) return d;
	return ud;
}

double common_module::units_pathing_distance(const unit_type* ut, const unit* a, const unit* b) {
	if (ut->is_flyer) return units_difference(a->pos, a->type->dimensions, b->pos, b->type->dimensions).length();
	double ud = units_difference(a->pos, a->type->dimensions, b->pos, b->type->dimensions).length();
	if (ud <= 32) return ud;
	auto& map = bot.square_pathing.get_pathing_map(ut);
	double d = bot.square_pathing.get_distance(map, a->pos, b->pos);
	if (d > 32 * 4) return d;
	return ud;
}
