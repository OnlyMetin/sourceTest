#ifndef __INC_FISHING_H__
#define __INC_FISHING_H__

#include "item.h"

namespace fishing
{
	enum
	{
		MAX_FISH = 49,
		NUM_USE_RESULT_COUNT = 10, // 1 : DEAD 2 : BONE 3 ~ 12 : rest
		FISH_BONE_VNUM = 27799,
		SHELLFISH_VNUM = 27987,
		EARTHWORM_VNUM = 27801,
		STONEPIECE_VNUM = 27990,
		WHITE_PEARL_VNUM = 27992,
		BLUE_PEARL_VNUM = 27993,
		RED_PEARL_VNUM = 27994,
		WATER_STONE_VNUM_BEGIN = 28030,
		WATER_STONE_VNUM_END = 28043,
#if defined(__FISHING_GAME__)
		GOLDEN_TUNA_VNUM = 27797,
#endif
		FISH_NAME_MAX_LEN = 64,
		MAX_PROB = 4,
	};

	enum
	{
		USED_NONE,
		USED_SHELLFISH,
		USED_WATER_STONE,
		USED_TREASURE_MAP,
		USED_EARTHWARM,
		MAX_USED_FISH
	};

	enum
	{
		FISHING_TIME_NORMAL,
		FISHING_TIME_SLOW,
		FISHING_TIME_QUICK,
		FISHING_TIME_ALL,
		FISHING_TIME_EASY,

		FISHING_TIME_COUNT,

		MAX_FISHING_TIME_COUNT = 31,
	};

	enum
	{
		FISHING_LIMIT_NONE,
		FISHING_LIMIT_APPEAR,
	};

	enum
	{
		CAMPFIRE_MOB = 12000,
		FISHER_MOB = 9009,
		FISH_MIND_PILL_VNUM = 27610,
	};

	EVENTINFO(fishing_event_info)
	{
		DWORD pid;
		int step;
		DWORD hang_time;
		int fish_id;

		fishing_event_info()
			: pid(0)
			, step(0)
			, hang_time(0)
			, fish_id(0)
		{
		}
	};

	extern void Initialize();
	extern LPEVENT CreateFishingEvent(LPCHARACTER ch);
	extern void Take(fishing_event_info* info, LPCHARACTER ch);
	extern void Simulation(int level, int count, int map_grade, LPCHARACTER ch);
	extern void UseFish(LPCHARACTER ch, LPITEM item);
	extern void Grill(LPCHARACTER ch, LPITEM item);

	extern bool RefinableRod(LPITEM rod);
	extern int RealRefineRod(LPCHARACTER ch, LPITEM rod);
#if defined(__FISHING_GAME__)
	extern void FishingFail(LPCHARACTER ch);
#endif
}
#endif // __INC_FISHING_H__
