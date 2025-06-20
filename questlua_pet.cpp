#include "stdafx.h"

#include "questlua.h"
#include "questmanager.h"
#include "horsename_manager.h"
#include "char.h"
#include "affect.h"
#include "config.h"
#include "utils.h"
#include "sectree_manager.h"

#include "PetSystem.h"

#undef sys_err
#ifndef __WIN32__
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, __VA_ARGS__)
#endif

extern int (*check_name) (const char* str);

namespace quest
{
#if defined(__PET_SYSTEM__)
	// syntax in LUA: pet.summon(mob_vnum, pet's name, (bool)run to me from far away, (bool)loot items from ground)
	int pet_summon(lua_State* L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();
		LPITEM pItem = CQuestManager::instance().GetCurrentItem();
		if (!ch || !petSystem || !pItem)
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		if (0 == petSystem)
		{
			lua_pushnumber(L, 0);
			return 1;
		}

#if defined(__PET_LOOT__)
		DWORD options = CPetActor::EPetOption_Followable | CPetActor::EPetOption_Summonable;
#endif

		// 소환수의 vnum
		DWORD mobVnum = lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0;

		// 소환수의 이름
		const char* petName = lua_isstring(L, 2) ? lua_tostring(L, 2) : 0;

		// 소환하면 멀리서부터 달려오는지 여부
		bool bFromFar = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : false;

#if defined(__PET_LOOT__)
		// Loot items from ground
		if (lua_isboolean(L, 4) && lua_toboolean(L, 4) != false)
			options |= CPetActor::EPetOption_Lootable;
#endif

		CPetActor* pet = petSystem->Summon(mobVnum, pItem, petName, bFromFar, options);

		if (pet != NULL)
			lua_pushnumber(L, pet->GetVID());
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	// syntax in LUA: pet.unsummon(mob_vnum)
	int pet_unsummon(lua_State* L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();

		if (0 == petSystem)
			return 0;

		// 소환수의 vnum
		DWORD mobVnum = lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0;

		petSystem->Unsummon(mobVnum);
		return 1;
	}

	int pet_count_summoned(lua_State* L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();

		lua_Number count = 0;

		if (0 != petSystem)
			count = (lua_Number)petSystem->CountSummoned();

		lua_pushnumber(L, count);

		return 1;
	}

	// syntax in LUA: pet.is_summon(mob_vnum)
	int pet_is_summon(lua_State* L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();

		if (0 == petSystem)
			return 0;

		// 소환수의 vnum
		DWORD mobVnum = lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0;

		CPetActor* petActor = petSystem->GetByVnum(mobVnum);

		if (NULL == petActor)
			lua_pushboolean(L, false);
		else
			lua_pushboolean(L, petActor->IsSummoned());

		return 1;
	}

	int pet_spawn_effect(lua_State* L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();

		if (0 == petSystem)
			return 0;

		DWORD mobVnum = lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0;

		CPetActor* petActor = petSystem->GetByVnum(mobVnum);
		if (NULL == petActor)
			return 0;
		LPCHARACTER pet_ch = petActor->GetCharacter();
		if (NULL == pet_ch)
			return 0;

		if (lua_isstring(L, 2))
		{
			pet_ch->SpecificEffectPacket(lua_tostring(L, 2));
		}
		return 0;
	}

	int pet_can_spawn(lua_State* L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();

		if (0 == petSystem)
			return 0;

		if (!SECTREE_MANAGER::Instance().IsBlockFilterMapIndex(SECTREE_MANAGER::PET_BLOCK_MAP_INDEX, ch->GetMapIndex()))
			lua_pushboolean(L, true);
		else
			lua_pushboolean(L, false);

		return 1;
	}

#if defined(__PET_LOOT__)
	int pet_is_summoned_looter(lua_State* L)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();
		if (NULL == petSystem)
			return 0;

		lua_pushboolean(L, petSystem->SummonedLooter());
		return 1;
	}
#endif

	void RegisterPetFunctionTable()
	{
		luaL_reg pet_functions[] =
		{
			{ "summon", pet_summon },
			{ "unsummon", pet_unsummon },
			{ "is_summon", pet_is_summon },
			{ "count_summoned", pet_count_summoned },
			{ "spawn_effect", pet_spawn_effect },
			{ "can_spawn", pet_can_spawn },
#if defined(__PET_LOOT__)
			{ "is_summoned_looter", pet_is_summoned_looter },
#endif
			{ NULL, NULL }
		};

		CQuestManager::instance().AddLuaFunctionTable("pet", pet_functions);
	}
#endif
}
