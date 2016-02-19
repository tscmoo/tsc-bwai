//
// This file includes BWAPI and contains wrappers for compatibility with any
// version.
//

#ifndef TSC_BWAI_BWAPI_INC_H
#define TSC_BWAI_BWAPI_INC_H

#include <BWApi.h>
#include <BWAPI/Client.h>

// Unfortunately, BWAPI headers includes windows.h
#undef min
#undef max
#undef near
#undef far

#ifndef _DEBUG
#pragma comment(lib,"BWAPI.lib")
#pragma comment(lib,"BWAPIClient.lib")
#else
#pragma comment(lib,"BWAPId.lib")
#pragma comment(lib,"BWAPIClientd.lib")
#endif

namespace tsc_bwai {

	constexpr bool is_bwapi_4 = std::is_pointer<BWAPI::Player>::value;

	using BWAPI_Player = std::conditional<is_bwapi_4, BWAPI::Player, BWAPI::Player*>::type;
	using BWAPI_Unit = std::conditional<is_bwapi_4, BWAPI::Unit, BWAPI::Unit*>::type;
	using BWAPI_Bullet = std::conditional<is_bwapi_4, BWAPI::Bullet, BWAPI::Bullet*>::type;

	struct bwapi_pos {
		int x, y;
		bwapi_pos(BWAPI::Position pos) : bwapi_pos((bwapi_pos&)pos) {}
		bwapi_pos(BWAPI::TilePosition pos) : bwapi_pos((bwapi_pos&)pos) {}
		template<typename T = xy>
		operator T() const {
			return T(x, y);
		}
	};

	template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
	void bwapi_init() {
	}
	template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
	void bwapi_init() {
		BWAPI::BWAPI_init();
	}

	template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
	BWAPI::Game* bwapi_connect() {
		while (!BWAPI::BWAPIClient.connect()) std::this_thread::sleep_for(std::chrono::milliseconds(250));
		return BWAPI::BroodwarPtr;
	}
	template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
	BWAPI::Game* bwapi_connect() {
		while (!BWAPI::BWAPIClient.connect()) std::this_thread::sleep_for(std::chrono::milliseconds(250));
		return BWAPI::Broodwar;
	}

	template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
	bool bwapi_call_build(BWAPI_Unit unit, BWAPI::UnitType type, BWAPI::TilePosition pos) {
		return unit->build(type, pos);
	}
	template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
	bool bwapi_call_build(BWAPI_Unit unit, BWAPI::UnitType type, BWAPI::TilePosition pos) {
		return unit->build(pos, type);
	}

	template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
	int bwapi_tech_type_energy_cost(BWAPI::TechType type) {
		return type.energyCost();
	}
	template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
	int bwapi_tech_type_energy_cost(BWAPI::TechType type) {
		return type.energyUsed();
	}

	template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
	bool bwapi_is_powered(BWAPI_Unit unit) {
		return unit->isPowered();
	}
	template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
	bool bwapi_is_powered(BWAPI_Unit unit) {
		return !unit->isUnpowered();
	}

	template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
	bool bwapi_is_healing_order(BWAPI::Order order) {
		return order == BWAPI::Orders::MedicHeal || order == BWAPI::Orders::MedicHealToIdle;
	}
	template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
	bool bwapi_is_healing_order(BWAPI::Order order) {
		return order == BWAPI::Orders::MedicHeal1 || order == BWAPI::Orders::MedicHeal2;
	}
}

#endif
