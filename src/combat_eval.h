
namespace combat_eval {
	;

	struct combatant {
		unit_stats*st;
		double move;
		double shields;
		double hp;
		int cooldown;
		combatant*target = nullptr;
	};

	double get_damage_type_modifier(int damage_type, int target_size) {
		if (damage_type == weapon_stats::damage_type_concussive) {
			if (target_size == unit_type::size_small) return 1.0;
			if (target_size == unit_type::size_medium) return 0.5;
			if (target_size == unit_type::size_large) return 0.25;
		}
		if (damage_type == weapon_stats::damage_type_normal) return 1.0;
		if (damage_type == weapon_stats::damage_type_explosive) {
			if (target_size == unit_type::size_small) return 0.5;
			if (target_size == unit_type::size_medium) return 0.75;
			if (target_size == unit_type::size_large) return 1.0;
		}
		return 1.0;
	}

	struct eval {

		struct team_t {
			a_vector<combatant> units;
			double start_supply = 0;
			double end_supply = 0;
			double damage_dealt = 0;
			double score = 0;
		};
		std::array<team_t, 2> teams;

		combatant&add_unit(unit_stats*st, int team) {
			auto&vec = teams[team].units;
			vec.emplace_back();
			auto&c = vec.back();
			c.st = st;
			c.move = 32 * 8;
			c.shields = st->shields;
			c.hp = st->hp;
			c.cooldown = 0;
			return c;
		}

		void run() {
			for (auto&t : teams) {
				//if (t.units.empty()) xcept("empty team");
				std::sort(t.units.begin(), t.units.end(), [&](const combatant&a, const combatant&b) {
					double ar = a.st->ground_weapon ? a.st->ground_weapon->max_range : a.st->air_weapon ? a.st->air_weapon->max_range : 0;
					double br = b.st->ground_weapon ? b.st->ground_weapon->max_range : b.st->air_weapon ? b.st->air_weapon->max_range : 0;
					return a.move + ar < b.move + br;
				});
				t.start_supply = 0;
				for (auto&v : t.units) {
					if (v.hp <= 0) continue;
					t.start_supply += v.st->type->required_supply;
				}
			}
			while (true) {
				int target_count = 0;
				for (int i = 0; i < 2; ++i) {
					team_t&my_team = teams[i];
					team_t&enemy_team = teams[i ^ 1];
					double max_width = 32 * 4;
					double acc_width = 0.0;
					for (auto&c : my_team.units) {
						if (c.hp <= 0) continue;
						//if (acc_width >= max_width) continue;
						acc_width += c.st->type->width;
						combatant*target = c.target;
						if (!target || target->hp <= 0) {
							target = nullptr;
							for (auto&ec : enemy_team.units) {
								if (ec.st->type->is_flyer && !c.st->air_weapon) continue;
								if (!ec.st->type->is_flyer && !c.st->ground_weapon) continue;
								if (ec.st->type->is_building) continue;
								if (ec.hp > 0) {
									target = &ec;
									break;
								}
							}
							c.target = target;
						}
						if (c.cooldown) --c.cooldown;
						if (target) {
							weapon_stats*w = target->st->type->is_flyer ? c.st->air_weapon : c.st->ground_weapon;
							if (c.move > w->max_range) {
								if (c.st->max_speed > 0) {
									++target_count;
									c.move -= c.st->max_speed;
									my_team.score += c.st->max_speed / 100.0;
								}
							} else {
								++target_count;
								if (c.cooldown == 0) {
									if (target->move > c.move) target->move = c.move;
									double damage = w->damage;
									if (target->shields <= 0) damage *= get_damage_type_modifier(w->damage_type, target->st->type->size);
									damage -= target->st->armor;
									if (damage <= 0) damage = 1.0;
									damage *= w == c.st->ground_weapon ? c.st->ground_weapon_hits : c.st->air_weapon_hits;
									if (target->shields > 0) {
										target->shields -= damage;
										if (target->shields < 0) {
											target->hp += target->shields;
											target->shields = 0.0;
										}
									} else target->hp -= damage;
									double damage_dealt = target->hp < 0 ? damage + target->hp : damage;
									my_team.damage_dealt += damage_dealt;
									c.cooldown = w->cooldown;
									my_team.score += damage_dealt;
								}
							}
						}
					}
				}
				if (target_count == 0) break;
			}
			for (auto&t : teams) {
				t.end_supply = 0;
				for (auto&v : t.units) {
					if (v.hp <= 0) continue;
					t.end_supply += v.st->type->required_supply;
				}
			}

		}

	};


}
