
#include "common.h"
#include "global.h"
#include "log.h"
using namespace tsc_bwai;

void main() {
	globals_container gc;
	set_globals_container(gc);
	if (test_mode) logger->set_output_file("log.txt");
	current_log_level = test_mode ? log_level_all : log_level_info;
	log("hello world\n");
}
