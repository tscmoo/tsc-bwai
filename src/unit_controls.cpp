
#include "unit_controls.h"
#include "bot.h"
using namespace tsc_bwai;

unit_controller* unit_controls_module::get_unit_controller(unit* u) {
	if (u->controller) return u->controller;
	unit_controller_container.emplace_back();
	unit_controller* c = &unit_controller_container.back();
	c->move_order = unit_controller_container.size();
	c->u = u;
	c->go_to = u->pos;
	c->action = unit_controller::action_idle;
	u->controller = c;
	return u->controller;
}

void unit_controls_module::control_units_task() {
	xcept("fixme: control_units_task");
}

void unit_controls_module::init() {
	if (bot.game->isReplay()) return;

	bot.multitasking.spawn(std::bind(&unit_controls_module::control_units_task, this), "control units");

	//bot.render.add(std::bind(&unit_controls_module::render, this));
	//render::add(render);
}

