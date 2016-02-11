//
// This file implements the bot interface.
// There isn't much, since everything has its own task which is run
// by multitasking.resume().
//

#include "bot.h"

void tsc_bwai::bot_t::on_frame() {
	current_frame = game->getFrameCount();
	latency_frames = game->getRemainingLatencyFrames();
	current_command_frame = current_frame + latency_frames;

	multitasking.resume();

	if (current_frame >= 30 && !send_text_queue.empty()) {
		game->sendText("%s", send_text_queue.front().c_str());
		send_text_queue.pop_front();
	}
}

void tsc_bwai::bot_t::send_text(a_string str) {

}
