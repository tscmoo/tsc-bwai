
#include "common.h"
#include "log.h"
#include "multitasking.h"
#include "multitasking_sync.h"
#include "bwapi_inc.h"
#include "bot.h"
using namespace tsc_bwai;

namespace tsc_bwai {

	struct module : BWAPI::AIModule {

		bot_t bot;

		virtual void onStart() override {
			bot.log(log_level_info, "game start\n");

			bot.current_frame = bot.game->getFrameCount();

			bot.game->enableFlag(BWAPI::Flag::UserInput);
			bot.game->setCommandOptimizationLevel(2);
			bot.game->setLocalSpeed(0);

			bot.send_text("tsc-bwai v0.6.7");

			bot.init();

		}

		virtual void onEnd(bool is_winner) override {
			bot.multitasking.stop();
		}

		virtual void onFrame() override {
			bot.on_frame();
		}


		virtual void onUnitShow(BWAPI_Unit gu) override {
			bot.units.on_unit_show(gu);
		};

		virtual void onUnitHide(BWAPI_Unit gu) override {
			bot.units.on_unit_hide(gu);
		};

		virtual void onUnitCreate(BWAPI_Unit gu) override {
			bot.units.on_unit_create(gu);
		}

		virtual void onUnitMorph(BWAPI_Unit gu) override {
			bot.units.on_unit_morph(gu);
		}

		virtual void onUnitDestroy(BWAPI_Unit gu) override {
			bot.units.on_unit_destroy(gu);
		}

		virtual void onUnitRenegade(BWAPI_Unit gu) override {
			bot.units.on_unit_renegade(gu);
		}

		virtual void onUnitComplete(BWAPI_Unit gu) override {
			bot.units.on_unit_complete(gu);
		}

	};
}


int main() {

	module m;
	m.bot.test_mode = true;
	m.bot.log.current_log_level = m.bot.test_mode ? log_level_all : log_level_info;
	if (m.bot.test_mode) m.bot.log.set_output_file("log.txt");

	try {

		bwapi_init();

		m.bot.log(log_level_info, "connecting\n");
		m.bot.game = bwapi_connect();

		m.bot.log(log_level_info, "waiting for game start\n");
		while (BWAPI::BWAPIClient.isConnected() && !m.bot.game->isInGame()) {
			BWAPI::BWAPIClient.update();
		}
		while (BWAPI::BWAPIClient.isConnected() && m.bot.game->isInGame()) {
			for (auto& e : m.bot.game->getEvents()) {
				switch (e.getType()) {
				case BWAPI::EventType::MatchStart:
					m.onStart();
					break;
				case BWAPI::EventType::MatchEnd:
					m.onEnd(e.isWinner());
					break;
				case BWAPI::EventType::MatchFrame:
					m.onFrame();
					break;
				case BWAPI::EventType::UnitShow:
					m.onUnitShow(e.getUnit());
					break;
				case BWAPI::EventType::UnitHide:
					m.onUnitHide(e.getUnit());
					break;
				case BWAPI::EventType::UnitCreate:
					m.onUnitCreate(e.getUnit());
					break;
				case BWAPI::EventType::UnitMorph:
					m.onUnitMorph(e.getUnit());
					break;
				case BWAPI::EventType::UnitDestroy:
					m.onUnitDestroy(e.getUnit());
					break;
				case BWAPI::EventType::UnitRenegade:
					m.onUnitRenegade(e.getUnit());
					break;
				case BWAPI::EventType::UnitComplete:
					m.onUnitComplete(e.getUnit());
					break;
				case BWAPI::EventType::SendText:
					m.bot.game->sendText("%s", e.getText().c_str());
					break;
				}
			}
			BWAPI::BWAPIClient.update();
			if (!BWAPI::BWAPIClient.isConnected()) {
				m.bot.log(log_level_info, "disconnected\n");
				break;
			}
		}
		m.bot.log(log_level_info, "game over\n");
	} catch (const std::exception& e) {
		m.bot.log(log_level_info, "main [exception] %s\n", e.what());
	}

	return 0;
}


