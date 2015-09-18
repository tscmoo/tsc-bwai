

namespace adapt {
;

a_unordered_map<a_string, double> weights;

double getweight(a_string name) {
	double&weight = weights[name];
 	if (weight == 0.0) weight = 1.0;
	return weight;
}

namespace detail {
	a_string getn(double) {
		return "";
	}
	template<typename A, typename...Ax>
	a_string getn(double val, A&&a, Ax&&...ax) {
		double w = getweight(a);
		if (val < w) return std::forward<A>(a);
		else {
			a_string next = getn(val - w, std::forward<Ax>(ax)...);
			if (next.empty()) return std::forward<A>(a);
			return next;
		}
	}
	double sum_of_weights() {
		return 0;
	}
	template<typename A, typename...Ax>
	double sum_of_weights(A&&a, Ax&&...ax) {
		return getweight(std::forward<A>(a)) + sum_of_weights(std::forward<Ax>(ax)...);
	}

	a_string concat() {
		return "";
	}
	template<typename A, typename...Ax>
	a_string concat(A&&a, Ax&&...ax) {
		a_string s = format("%s (%g)", a, getweight(a));
		a_string s2 = concat(std::forward<Ax>(ax)...);
		if (!s2.empty()) return s + ", " + s2;
		else return s;
	}
}

a_vector<a_string> all_choices;

template<typename...args_T>
a_string choose(args_T&&...args) {
	double val = tsc::rng(detail::sum_of_weights(args...));
	auto v = detail::getn(val, args...);
	log("choose [%s] -> %s = %g\n", detail::concat(args...), v, val);
	all_choices.push_back(v);
	return v;
}

bool choose_bool(a_string name) {
	return choose(name, "not " + name) == name;
}

}
