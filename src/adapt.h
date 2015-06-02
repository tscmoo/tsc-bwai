

namespace adapt {
;

a_unordered_map<a_string, double> weights;

double getweight(a_string name) {
	double&weight = weights[name];
	if (weight == 0.0) weight = 1.0;
	if (weight < 1.0 / 32) weight = 1.0 / 32;
	if (weight > 8.0) weight = 8.0;
	return weight;
}

double getval(a_string name) {
	return tsc::rng(getweight(name));
}

namespace detail {
	std::tuple<a_string, double> getn() {
		return std::make_tuple("", -std::numeric_limits<double>::infinity());
	}
	template<typename A, typename...Ax>
	std::tuple<a_string, double> getn(A&&a, Ax&&...ax) {
		double val = getval(a);
		auto next = getn(std::forward<Ax>(ax)...);
		if (val > std::get<1>(next)) return std::make_tuple(a, val);
		else return next;
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

a_vector<std::tuple<a_string, double>> all_choices;

template<typename...args_T>
a_string choose(args_T&&...args) {
	auto v = detail::getn(std::forward<args_T>(args)...);
	log("choose [%s] -> %s = %g\n", detail::concat(std::forward<args_T>(args)...), std::get<0>(v), std::get<1>(v));
	all_choices.push_back(v);
	return std::get<0>(v);
}

bool choose_bool(a_string name) {
	return choose(name, "not " + name) == name;
}

// void won() {
// 	for (auto&v : all_choices) {
// 		double&val = weights[std::get<0>(v)];
// 		double oldval = val;
// 		val += 1.0;
// 		log("weight %s - %g -> %g\n", std::get<0>(v), oldval, val);
// 	}
// }
// void lost() {
// 	for (auto&v : all_choices) {
// 		double&val = weights[std::get<0>(v)];
// 		double oldval = val;
// 		val /= 2.0;
// 		log("weight %s - %g -> %g\n", std::get<0>(v), oldval, val);
// 	}
// }
// 
// void load_weights(a_string data) {
// 	const char*s = data.data();
// 	while (*s) {
// 		while (*s && *s == '\r' || *s == '\n' || *s == ' ') ++s;
// 		const char*n = s;
// 		while (*s && *s != ':') ++s;
// 		a_string name(n, s - n);
// 		if (*s == ':') ++s;
// 		char*e = (char*)s;
// 		double val = std::strtod(s, &e);
// 		s = e;
// 		if (!name.empty()) weights[name] = val;
// 		log("weight %s -> %g\n", name, val);
// 	}
// }
// 
// a_string save_weights() {
// 	a_string str;
// 	for (auto&v : weights) {
// 		str += format("%s: %g\n", std::get<0>(v), std::get<1>(v));
// 	}
// 	return str;
// }

}
