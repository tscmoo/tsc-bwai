//
// This file defines the log function, which is used for all console output,
// printed to log.txt if test_mode is enabled.
//

#ifndef TSC_BWAI_LOG_H
#define TSC_BWAI_LOG_H

#include "common.h"
#include <mutex>
#include <string>

namespace tsc_bwai {

	class bot_t;

	struct simple_logger {
		std::mutex mut;
		a_string str, str2;
		bool newline = true;
		FILE*f = nullptr;
		simple_logger() {
		}
		~simple_logger() {
			if (f) fclose(f);
		}
		void set_output_file(const char* filename) {
			if (f) fclose(f);
			f = fopen(filename, "w");
		}
		template<typename...T>
		void operator()(int frame, const char*fmt, T&&...args) {
			std::lock_guard<std::mutex> lock(mut);
			try {
				tsc::strf::format(str, fmt, std::forward<T>(args)...);
			} catch (const std::exception&) {
				str = fmt;
			}
			if (newline) tsc::strf::format(str2, "%5d: %s", frame, str);
			const char*out_str = newline ? str2.c_str() : str.c_str();
			newline = strchr(out_str, '\n') ? true : false;
			if (f) {
				fputs(out_str, f);
				fflush(f);
			}
			fputs(out_str,stdout);
		}
	};

	enum log_level {
		log_level_all,
		log_level_debug,
		log_level_info
	};

	class log_t {
		bot_t& bot;
		simple_logger logger;
	public:
		log_t(bot_t& bot) : bot(bot) {}
		int current_log_level;

		void set_output_file(const char* filename) {
			logger.set_output_file(filename);
		}

		template<typename...T>
		void operator()(int level, const char*fmt, T&&...args) {
			if (current_log_level <= level) logger(bot.current_frame, fmt, std::forward<T>(args)...);
		}

		template<typename...T>
		void operator()(const char*fmt, T&&...args) {
			(*this)(log_level_debug, fmt, std::forward<T>(args)...);
		}
	};
}

#endif
