#ifndef _DATA
#define _DATA
#include <cstdint>

enum class FiringMode : uint8_t
{
	AUTO = 0,
	SINGLE = 3,
	BURST = 2
};

struct R6Data
{
	uint64_t base;
	uint64_t local_player;
	uint64_t fov_manager;
	uint64_t curr_weapon;
	uint64_t weapon_info;
	uint64_t glow_manager;
	uint64_t game_manager;
	uint64_t round_manager;
};

struct ValuesUpdates
{
	bool update_cav_esp;
	bool update_no_recoil;
	bool update_no_spread;
	bool update_no_flash;
	bool update_firing_mode;
	bool update_glow;
    bool update_fov;
};

#endif