
struct wraith {


	void run() {

		while (true) {

			int desired_starports = 2;

			using namespace buildpred;

			execute_build([&](state&st) {
				return nodelay(st, unit_types::scv, [&](state&st) {
					return nodelay(st, unit_types::wraith, [&](state&st) {
						if (count_units_plus_production(st, unit_types::starport) < desired_starports) return depbuild(st, state(st), unit_types::starport);
						return nodelay(st, unit_types::vulture, [](state&st) {
							return depbuild(st, state(st), unit_types::factory);
						});
					});
				});
			});

			//if (my_completed_units_of_type[unit_types::wraith].size() >= 2) buildpred::attack_now = true;

			multitasking::sleep(10);
		}

	}

};

