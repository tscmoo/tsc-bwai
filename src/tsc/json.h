

#include <string>
#include <vector>
#include <map>


struct json_value {
	enum { t_bad, t_object, t_array, t_string, t_number, t_boolean, t_null };
	int type;
	json_value() : type(t_null) {}
	//json_value(int type) : type(type) {}
	a_map<a_string,json_value> map;
	a_vector<json_value> vector;
	bool b;
	double n;
	int i;
	a_string s;
	json_value&operator[](size_t idx) {
		type = t_array;
		if (idx>=vector.size()) vector.resize(idx+1);
		return vector[idx];
	}
	json_value&operator[](int idx) {
		return (*this)[(size_t)idx];
	}
	json_value&operator[](a_string idx) {
		type = t_object;
		return map[idx];
	}
	json_value&operator[](const char*idx) {
		type = t_object;
		return map[idx];
	}
	size_t size() const {
		if (type==t_object) return map.size();
		else if (type==t_array) return vector.size();
		else return 0;
	}
	a_map<a_string,json_value>::iterator find(a_string idx) {
		return map.find(idx);
	}
	a_map<a_string,json_value>::const_iterator find(a_string idx) const {
		return map.find(idx);
	}
	operator bool() const {
		if (type!=t_boolean) return type!=t_null;
		return b;
	}
	operator int() const {
		if (type!=t_number) return 0;
		return i;
	}
	operator double() const {
		if (type!=t_number) return 0;
		return n;
	}
	operator float() const {
		return (float)(double)*this;
	}
	operator const a_string&() const {
		return s;
	}
	bool operator==(const char*str) const {
		return s==str;
	}
	bool operator==(const a_string&str) const {
		return s == str;
	}
	json_value&operator=(const a_string&str) {
		type = t_string;
		s = str;
		return *this;
	}
	json_value&operator=(double nn) {
		type = t_number;
		n = nn;
		return *this;
	}
	template<typename T>
	json_value&operator=(const a_vector<T>&v) {
		vector.reserve(v.size());
		type = t_array;
		for (size_t i=0;i<v.size();++i) {
			(*this)[i] = v[i];
		}
		return *this;
	}
	json_value&operator=(std::nullptr_t) {
		type = t_null;
		return *this;
	}
	json_value&operator=(int v) {
		type = t_number;
		n = v;
		return *this;
	}
	json_value&operator=(bool v) {
		type = t_boolean;
		b = v;
		return *this;
	}
	json_value&operator=(const char*str) {
		type = t_string;
		s = str;
		return *this;
	}
	const char*parse(const char*str,size_t len) {

		const char*e = str+len;

		auto fail = [&]() {
			size_t l = std::min<size_t>(e-str,20);
			xcept("failed to parse at %p, near '%.*s'\n",str,l,str);
			type = t_null;
			return str;
		};

		auto skip_whitespace = [&]() {
			while (str<e) {
				switch (*str) {
				case ' ': case '\t': case '\n': case '\v': case '\f': case '\r':
					++str;
					continue;
				}
				break;
			}
		};
		auto parse_string = [&]() -> a_string {
			// Note to compiler: please do not inline this function, or
			// alternatively allocate strbuf as-needed. Thank you.
			char strbuf[0x10000];
			char*o = strbuf;
			a_string r;
			auto flush = [&]() {
				if (o==strbuf) return;
				if (r.empty()) r.assign(strbuf,o-strbuf);
				else r.append(strbuf,o-strbuf);
				o = strbuf;
			};
			while (str<e) {
				char c = *str++;
				switch (c) {
				case '\\': 
					if (str==e) break;
					if (o+4>=strbuf+sizeof(strbuf)) flush();
					c = *str++;
					switch (c) {
					case 'b': *o++ = '\b'; break;
					case 'f': *o++ = '\f'; break;
					case 'n': *o++ = '\n'; break;
					case 'r': *o++ = '\r'; break;
					case 't': *o++ = '\t'; break;
					default: *o++ = c; break;
					}
					continue;
				case '"': break;
				default:
					if (o+1>=strbuf+sizeof(strbuf)) flush();
					*o++ = c;
					continue;
				}
				break;
			}
			flush();
			return r;
		};
		auto parse_as_object = [&]() -> const char* {
			map.clear();
			while (true) {
				++str;
				skip_whitespace();
				if (str==e) return fail();
				if (map.empty() && *str=='}') break;
				if (*str!='"') return fail();
				++str;
				a_string key = parse_string();
				skip_whitespace();
				if (str==e||*str!=':') return fail();
				++str;
				json_value value;
				str = value.parse(str,e-str);
				map[std::move(key)] = std::move(value);
				skip_whitespace();
				if (str==e) return fail();
				if (*str=='}') break;
				if (*str!=',') return fail();
			}
			++str;
			type = t_object;
			return str;
		};
		auto parse_as_array = [&]() -> const char* {
			vector.clear();
			while (true) {
				++str;
				skip_whitespace();
				if (str==e) return fail();
				if (vector.empty() && *str==']') break;
				json_value value;
				str = value.parse(str,e-str);
				vector.push_back(std::move(value));
				skip_whitespace();
				if (str==e) return fail();
				if (*str==']') break;
				if (*str!=',') return fail();
			}
			++str;
			type = t_array;
			return str;
		};
		auto parse_as_string = [&]() -> const char* {
			++str;
			type = t_string;
			s = parse_string();
			return str;
		};
		auto parse_as_number = [&]() -> const char* {
			char buf[0x20];
			char*o = buf;
			auto put = [&]() {
				char c = *str++;
				if (o<buf+sizeof(buf)-1) *o++ = c;
			};
			auto digit = [&]() {
				if (str==e) return false;
				char c = *str;
				return c>='0'&&c<='9' ? (put(), true) : false;
			};
			auto test = [&](char c) {
				return str==e ? false : *str==c ? (put(), true) : false;
			};
			auto done = [&]() -> const char* {
				*o = 0;
				n = strtod(buf,0);
				i = (int)n;
				type = t_number;
				return str;
			};
			if (str==e) return done();
			test('-');
			if (e >= str + 3 && str[0] == 'i'&& str[1] == 'n' && str[2] == 'f') {
				put(); put(); put();
			}
			if (*str>='1'&&*str<='9') while (digit()) ; 
			else if (!test('0')) return done();
			if (test('.')) while (digit()) ;
			if (test('e') || test('E')) {
				test('+') || test('-');
				while (digit()) ;
			}
			return done();
		};

		if (str==e) return fail();
		skip_whitespace();
		switch (*str) {
		case '{': return parse_as_object();
		case '[': return parse_as_array();
		case '"': return parse_as_string();
		case 't': return e>=str+4&&str[1]=='r'&&str[2]=='u'&&str[3]=='e' ? (type=t_boolean, b=true, str+4) : fail();
		case 'f': return e>=str+5&&str[1]=='a'&&str[2]=='l'&&str[3]=='s'&&str[4]=='e' ? (type=t_boolean, b=false, str+5) : fail();
		case 'n': return e>=str+4&&str[1]=='u'&&str[2]=='l'&&str[3]=='l' ? (type=t_null, str+4) : fail();
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case '-': case 'i':
			return parse_as_number();
		}

		return fail();
	}

	a_string dump() const {

		auto num = [&]() -> a_string {
			char buf[0x100];
			memset(buf,0,sizeof(buf));
			sprintf(buf,"%.100g",n);
			return buf;
		};
		auto str = [&](const a_string&sa) {
			// Note to compiler: please do not inline this function, or
			// alternatively allocate strbuf as-needed. Thank you.
			char strbuf[0x10000];
			char*o = strbuf;
			a_string r;
			auto flush = [&]() {
				if (o==strbuf) return;
				if (r.empty()) r.assign(strbuf,o-strbuf);
				else r.append(strbuf,o-strbuf);
				o = strbuf;
			};
			const char*strp = sa.data();
			const char*e = strp + sa.size();
			auto put = [&](char c) {
				if (o+1>=strbuf+sizeof(strbuf)) flush();
				*o++ = c;
			};
			auto pute = [&](int c) {
				if (o+2>=strbuf+sizeof(strbuf)) flush();
				*o++ = '\\';
				*o++ = c;
			};
			put('"');
			while (strp<e) {
				char c = *strp++;
				switch (c) {
				case '"': pute('"'); break;
				case '\\': pute('\\');  break;
				case '\b': pute('b'); break;
				case '\f': pute('f'); break;
				case '\n': pute('n'); break;
				case '\r': pute('r'); break;
				case '\t': pute('t'); break;
				default:
					put(c);
				}
			}
			put('"');
			flush();
			return r;
		};
		auto obj = [&]() {
			a_string r = "{";
			bool first = true;
			for (auto&v : map) {
				if (first) first=false;
				else r += ",";
				r += str(v.first);
				r += ":";
				r += v.second.dump();
			};
			return r + "}";
		};
		auto arr = [&]() {
			a_string r = "[";
			bool first = true;
			for (auto&v : vector) {
				if (first) first=false;
				else r += ",";
				r += v.dump();
			};
			return r + "]";
		};

		switch (type) {
		case t_boolean: return b ? "true" : "false";
		case t_number: return num();
		case t_string: return str(s);
		case t_object: return obj();
		case t_array: return arr();
		default: return "null";
		}

	}

};

struct json_object : json_value {
	json_object() {
		type = t_object;
	}
};

json_value json_parse(const char*str,size_t len) {
	json_value r;
	r.parse(str,len);
	return r;
}
json_value json_parse(const a_string&str) {
	return json_parse(str.data(),str.size());
}

