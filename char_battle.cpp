#include "stdafx.h"
#include "utils.h"
#include "config.h"
#include "desc.h"
#include "desc_manager.h"
#include "char_manager.h"
#include "item.h"
#include "item_manager.h"
#include "mob_manager.h"
#include "battle.h"
#include "pvp.h"
#include "skill.h"
#include "start_position.h"
#include "profiler.h"
#include "cmd.h"
#include "dungeon.h"
#include "log.h"
#include "unique_item.h"
#include "priv_manager.h"
#include "db.h"
#include "vector.h"
#include "marriage.h"
#include "arena.h"
#include "regen.h"
#include "monarch.h"
#include "exchange.h"
#include "shop_manager.h"
#include "castle.h"
#include "dev_log.h"
#include "ani.h"
#include "BattleArena.h"
#include "packet.h"
#include "party.h"
#include "affect.h"
#include "guild.h"
#include "guild_manager.h"
#include "questmanager.h"
#include "questlua.h"
#include "threeway_war.h"
#include "BlueDragon.h"
#include "DragonLair.h"
#if defined(__MT_THUNDER_DUNGEON__)
#	include "mt_thunder_dungeon.h"
#endif
#if defined(__GUILD_DRAGONLAIR_SYSTEM__)
#	include "guild_dragonlair.h"
#endif
#if defined(__DEFENSE_WAVE__)
#	include "defense_wave.h"
#endif

#if defined(__PET_LOOT__)
#	include "PetSystem.h"
#endif

DWORD AdjustExpByLevel(const LPCHARACTER ch, const DWORD exp)
{
	if (PLAYER_EXP_TABLE_MAX < ch->GetLevel())
	{
		double ret = 0.95;
		double factor = 0.1;

		for (ssize_t i = 0; i < ch->GetLevel() - 100; ++i)
		{
			if ((i % 10) == 0)
				factor /= 2.0;

			ret *= 1.0 - factor;
		}

		ret = ret * static_cast<double>(exp);

		if (ret < 1.0)
			return 1;

		return static_cast<DWORD>(ret);
	}

	return exp;
}

bool CHARACTER::CanBeginFight() const
{
	if (!CanMove())
		return false;

	return m_pointsInstant.position == POS_STANDING && !IsDead() && !IsStun();
}

void CHARACTER::BeginFight(LPCHARACTER pkVictim)
{
	SetVictim(pkVictim);
	SetNowWalking(false);
	SetPosition(POS_FIGHTING);
	SetNextStatePulse(1);
}

bool CHARACTER::CanFight() const
{
	return m_pointsInstant.position >= POS_FIGHTING ? true : false;
}

void CHARACTER::CreateFly(BYTE bType, LPCHARACTER pkVictim)
{
	TPacketGCCreateFly packFly;

	packFly.bHeader = HEADER_GC_CREATE_FLY;
	packFly.bType = bType;
	packFly.dwStartVID = GetVID();
	packFly.dwEndVID = pkVictim->GetVID();

	PacketAround(&packFly, sizeof(TPacketGCCreateFly));
}

void CHARACTER::DistributeSP(LPCHARACTER pkKiller, int iMethod)
{
	if (pkKiller->GetSP() >= pkKiller->GetMaxSP())
		return;

	bool bAttacking = (get_dword_time() - GetLastAttackTime()) < 3000;
	bool bMoving = (get_dword_time() - GetLastMoveTime()) < 3000;

	if (iMethod == 1)
	{
		int num = number(0, 3);

		if (!num)
		{
			int iLvDelta = GetLevel() - pkKiller->GetLevel();
			int iAmount = 0;

			if (iLvDelta >= 5)
				iAmount = 10;
			else if (iLvDelta >= 0)
				iAmount = 6;
			else if (iLvDelta >= -3)
				iAmount = 2;

			if (iAmount != 0)
			{
				iAmount += (iAmount * pkKiller->GetPoint(POINT_SP_REGEN)) / 100;

				if (iAmount >= 11)
					CreateFly(FLY_SP_BIG, pkKiller);
				else if (iAmount >= 7)
					CreateFly(FLY_SP_MEDIUM, pkKiller);
				else
					CreateFly(FLY_SP_SMALL, pkKiller);

				pkKiller->PointChange(POINT_SP, iAmount);
			}
		}
	}
	else
	{
		if (pkKiller->GetJob() == JOB_SHAMAN || (pkKiller->GetJob() == JOB_SURA && pkKiller->GetSkillGroup() == 2))
		{
			int iAmount;

			if (bAttacking)
				iAmount = 2 + GetMaxSP() / 100;
			else if (bMoving)
				iAmount = 3 + GetMaxSP() * 2 / 100;
			else
				iAmount = 10 + GetMaxSP() * 3 / 100; // 평상시

			iAmount += (iAmount * pkKiller->GetPoint(POINT_SP_REGEN)) / 100;
			pkKiller->PointChange(POINT_SP, iAmount);
		}
		else
		{
			int iAmount;

			if (bAttacking)
				iAmount = 2 + pkKiller->GetMaxSP() / 200;
			else if (bMoving)
				iAmount = 2 + pkKiller->GetMaxSP() / 100;
			else
			{
				// 평상시
				if (pkKiller->GetHP() < pkKiller->GetMaxHP())
					iAmount = 2 + (pkKiller->GetMaxSP() / 100); // 피 다 안찼을때
				else
					iAmount = 9 + (pkKiller->GetMaxSP() / 100); // 기본
			}

			iAmount += (iAmount * pkKiller->GetPoint(POINT_SP_REGEN)) / 100;
			pkKiller->PointChange(POINT_SP, iAmount);
		}
	}
}

bool CHARACTER::Attack(LPCHARACTER pkVictim, BYTE bType)
{
	if (test_server)
		sys_log(0, "[TEST_SERVER] Attack : %s type %d, MobBattleType %d", GetName(), bType, !GetMobBattleType() ? 0 : GetMobAttackRange());

	//PROF_UNIT puAttack("Attack");
	if (!CanMove() || IsObserverMode())
		return false;

	// CASTLE
	if (IS_CASTLE_MAP(GetMapIndex()) && false == castle_can_attack(this, pkVictim))
		return false;
	// CASTLE

#if defined(__DEFENSE_WAVE__)
	if (GetDefenseWave() && CDefenseWaveManager::Instance().CanAttack(this, pkVictim) == false)
		return false;
#endif

	if (!battle_is_attackable(this, pkVictim))
		return false;

	DWORD dwCurrentTime = get_dword_time();

	if (IsPC())
	{
		if (IS_SPEED_HACK(this, pkVictim, dwCurrentTime))
			return false;

		if (bType == 0 && dwCurrentTime < GetSkipComboAttackByTime())
			return false;
	}
	else
	{
		MonsterChat(MONSTER_CHAT_ATTACK);
	}

	pkVictim->SetSyncOwner(this);

	if (pkVictim->CanBeginFight())
		pkVictim->BeginFight(this);

	int iRet;

	if (bType == 0)
	{
		//
		// 일반 공격
		//
		switch (GetMobBattleType())
		{
			case BATTLE_TYPE_MELEE:
			case BATTLE_TYPE_POWER:
			case BATTLE_TYPE_TANKER:
			case BATTLE_TYPE_SUPER_POWER:
			case BATTLE_TYPE_SUPER_TANKER:
				iRet = battle_melee_attack(this, pkVictim);
				break;

			case BATTLE_TYPE_RANGE:
				FlyTarget(pkVictim->GetVID(), pkVictim->GetX(), pkVictim->GetY(), HEADER_CG_FLY_TARGETING);
				iRet = Shoot(0) ? BATTLE_DAMAGE : BATTLE_NONE;
				break;

			case BATTLE_TYPE_MAGIC:
				FlyTarget(pkVictim->GetVID(), pkVictim->GetX(), pkVictim->GetY(), HEADER_CG_FLY_TARGETING);
				iRet = Shoot(1) ? BATTLE_DAMAGE : BATTLE_NONE;
				break;

			default:
				sys_err("Unhandled battle type %d", GetMobBattleType());
				iRet = BATTLE_NONE;
				break;
		}

#if defined(__SOUL_SYSTEM__)
		if (IsPC() && iRet != BATTLE_NONE)
			SoulItemProcess(RED_SOUL);
#endif
	}
	else
	{
		if (IsPC() == true)
		{
			if (dwCurrentTime - m_dwLastSkillTime > 1500)
			{
				sys_log(1, "HACK: Too long skill using term. Name(%s) PID(%u) delta(%u)",
					GetName(), GetPlayerID(), (dwCurrentTime - m_dwLastSkillTime));
				return false;
			}
		}

		sys_log(1, "Attack call ComputeSkill %d %s", bType, pkVictim ? pkVictim->GetName() : "");
		iRet = ComputeSkill(bType, pkVictim);

#if defined(__SOUL_SYSTEM__)
		if (IsPC() && iRet != BATTLE_NONE)
			SoulItemProcess(BLUE_SOUL);
#endif
	}

#if defined(__ATTR_6TH_7TH__)
	if (IsPC() && iRet != BATTLE_NONE)
		this->StartHitBuffEvent();
#endif

	//if (test_server && IsPC())
		//sys_log(0, "%s Attack %s type %u ret %d", GetName(), pkVictim->GetName(), bType, iRet);

	if (iRet == BATTLE_DAMAGE || iRet == BATTLE_DEAD)
	{
		OnMove(true);
		pkVictim->OnMove();

		// Only pc sets victim null. For npc, state machine will reset this.
		if (BATTLE_DEAD == iRet && IsPC())
			SetVictim(NULL);

		return true;
	}

	return false;
}

void CHARACTER::DeathPenalty(BYTE bTown)
{
	sys_log(1, "DEATH_PERNALY_CHECK(%s) town(%d)", GetName(), bTown);

#if defined(__CUBE_RENEWAL__)
	CCubeManager::Instance().CloseCube(this);
#else
	Cube_close(this);
#endif

	if (CBattleArena::instance().IsBattleArenaMap(GetMapIndex()) == true)
	{
		return;
	}

	if (GetLevel() < 10)
	{
		sys_log(0, "NO_DEATH_PENALTY_LESS_LV10(%s)", GetName());
		ChatPacket(CHAT_TYPE_INFO, LC_STRING("용신의 가호로 경험치가 떨어지지 않았습니다."));
		return;
	}

	if (number(0, 2))
	{
		sys_log(0, "NO_DEATH_PENALTY_LUCK(%s)", GetName());
		ChatPacket(CHAT_TYPE_INFO, LC_STRING("용신의 가호로 경험치가 떨어지지 않았습니다."));
		return;
	}

	if (IS_SET(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY))
	{
		REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);

		// NO_DEATH_PENALTY_BUG_FIX 
		if (LC_IsYMIR()) // 천마 버전에서는 언제나 용신의 가호 아이템을 체크한다.
		{
			if (FindAffect(AFFECT_NO_DEATH_PENALTY))
			{
				sys_log(0, "NO_DEATH_PENALTY_AFFECT(%s)", GetName());
				ChatPacket(CHAT_TYPE_INFO, LC_STRING("용신의 가호로 경험치가 떨어지지 않았습니다."));
				RemoveAffect(AFFECT_NO_DEATH_PENALTY);
				return;
			}
		}
		else if (!bTown) // 국제 버전에서는 제자리 부활시만 용신의 가호를 사용한다. (마을 복귀시는 경험치 패널티 없음)
		{
			if (FindAffect(AFFECT_NO_DEATH_PENALTY))
			{
				sys_log(0, "NO_DEATH_PENALTY_AFFECT(%s)", GetName());
				ChatPacket(CHAT_TYPE_INFO, LC_STRING("용신의 가호로 경험치가 떨어지지 않았습니다."));
				RemoveAffect(AFFECT_NO_DEATH_PENALTY);
				return;
			}
		}
		// END_OF_NO_DEATH_PENALTY_BUG_FIX

		int iLoss = ((GetNextExp() * aiExpLossPercents[MINMAX(1, GetLevel(), PLAYER_EXP_TABLE_MAX)]) / 100);

		if (true == LC_IsYMIR())
		{
			if (PLAYER_EXP_TABLE_MAX < GetLevel())
			{
				iLoss = MIN(500000, iLoss);
			}
			else
			{
				iLoss = MIN(200000, iLoss);
			}
		}
		else if (true == LC_IsEurope())
		{
			iLoss = MIN(800000, iLoss);
		}

		if (bTown)
		{
			if (g_iUseLocale)
			{
				iLoss = 0;
			}
			else
			{
				iLoss -= iLoss / 3;
			}
		}

		if (IsEquipUniqueItem(UNIQUE_ITEM_TEARDROP_OF_GODNESS))
			iLoss /= 2;

		sys_log(0, "DEATH_PENALTY(%s) EXP_LOSS: %d percent %d%%", GetName(), iLoss, aiExpLossPercents[MIN(gPlayerMaxLevel, GetLevel())]);

		PointChange(POINT_EXP, -iLoss, true);
	}
}

bool CHARACTER::IsStun() const
{
	if (IS_SET(m_pointsInstant.instant_flag, INSTANT_FLAG_STUN))
		return true;

	return false;
}

EVENTFUNC(StunEvent)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);

	if (info == NULL)
	{
		sys_err("StunEvent> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER ch = info->ch;

	if (ch == NULL) // <Factor>
		return 0;

	ch->m_pkStunEvent = NULL;
	ch->Dead();
	return 0;
}

void CHARACTER::Stun(bool bImmediate)
{
	if (IsStun())
		return;

	if (IsDead())
		return;

	if (!IsPC() && m_pkParty)
	{
		m_pkParty->SendMessage(this, PM_ATTACKED_BY, 0, 0);
	}

	sys_log(1, "%s: Stun %p", GetName(), this);

	PointChange(POINT_HP_RECOVERY, -GetPoint(POINT_HP_RECOVERY));
	PointChange(POINT_SP_RECOVERY, -GetPoint(POINT_SP_RECOVERY));

	CloseMyShop();

	event_cancel(&m_pkRecoveryEvent); // 회복 이벤트를 죽인다.

	TPacketGCStun pack;
	pack.header = HEADER_GC_STUN;
	pack.vid = m_vid;
	PacketAround(&pack, sizeof(pack));

	SET_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_STUN);

	if (m_pkStunEvent)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();

	info->ch = this;

	m_pkStunEvent = event_create(StunEvent, info, PASSES_PER_SEC(bImmediate ? 1 : 3));
}

EVENTINFO(SCharDeadEventInfo)
{
	bool isPC;
	uint32_t dwID;

	SCharDeadEventInfo()
		: isPC(0)
		, dwID(0)
	{
	}
};

EVENTFUNC(dead_event)
{
	const SCharDeadEventInfo* info = dynamic_cast<SCharDeadEventInfo*>(event->info);

	if (info == NULL)
	{
		sys_err("dead_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER ch = NULL;

	if (true == info->isPC)
	{
		ch = CHARACTER_MANAGER::instance().FindByPID(info->dwID);
	}
	else
	{
		ch = CHARACTER_MANAGER::instance().Find(info->dwID);
	}

	if (NULL == ch)
	{
		sys_err("DEAD_EVENT: cannot find char pointer with %s id(%d)", info->isPC ? "PC" : "MOB", info->dwID);
		return 0;
	}

	ch->m_pkDeadEvent = NULL;

	if (ch->GetDesc())
	{
		ch->GetDesc()->SetPhase(PHASE_GAME);
		ch->SetPosition(POS_STANDING);

		long lMapIndex = ch->GetMapIndex();

#if defined(__GUILD_DRAGONLAIR_SYSTEM__)
		if (CGuildDragonLairManager::Instance().IsRedDragonLair(lMapIndex))
		{
			CGuildDragonLairManager::Instance().Exit(ch);
			return 0;
		}
#endif

#if defined(__ELEMENTAL_DUNGEON__)
		if (IS_ELEMENTAL_DUNGEON(ch->GetMapIndex()))
		{
			PIXEL_POSITION WarpPos;
			if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(MAP_DEFENSEWAVE_PORT, ch->GetEmpire(), WarpPos))
				ch->WarpSet(WarpPos.x, WarpPos.y);
			else
			{
				sys_err("cannot find spawn position (name %s)", ch->GetName());
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
			}

			ch->PointChange(POINT_HP, (ch->GetMaxHP() / 2) - ch->GetHP(), true);
			ch->DeathPenalty(0);
			ch->StartRecoveryEvent();
			ch->ChatPacket(CHAT_TYPE_COMMAND, "CloseRestartWindow");

			return 0;
		}
#endif

		PIXEL_POSITION pos;
		if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(lMapIndex, ch->GetEmpire(), pos))
			ch->WarpSet(pos.x, pos.y);
		else
		{
			sys_err("cannot find spawn position (name %s)", ch->GetName());
			ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
		}

		ch->PointChange(POINT_HP, (ch->GetMaxHP() / 2) - ch->GetHP(), true);
		ch->DeathPenalty(0);
		ch->StartRecoveryEvent();
		ch->ChatPacket(CHAT_TYPE_COMMAND, "CloseRestartWindow");
	}
	else
	{
		if (ch->IsMonster())
		{
			if (!ch->IsRevive() && ch->HasReviverInParty())
			{
				ch->SetPosition(POS_STANDING);
				ch->SetHP(ch->GetMaxHP());

				ch->ViewReencode();

				ch->SetAggressive();
				ch->SetRevive(true);

				return 0;
			}
		}

		M2_DESTROY_CHARACTER(ch);
	}

	return 0;
}

bool CHARACTER::IsDead() const
{
	if (m_pointsInstant.position == POS_DEAD)
		return true;

	return false;
}

#define GetGoldMultipler() (distribution_test_server ? 3 : 1)

void CHARACTER::RewardGold(LPCHARACTER pkAttacker)
{
	// ADD_PREMIUM
	bool isAutoLoot =
		(pkAttacker->GetPremiumRemainSeconds(PREMIUM_AUTOLOOT) > 0 ||
			pkAttacker->IsEquipUniqueGroup(UNIQUE_GROUP_AUTO_LOOT))
		? true : false; // 제3의 손
	// END_OF_ADD_PREMIUM

	PIXEL_POSITION pos;

	if (!isAutoLoot)
		if (!SECTREE_MANAGER::instance().GetMovablePosition(GetMapIndex(), GetX(), GetY(), pos))
			return;

	int iTotalGold = 0;
	//
	// --------- 돈 드롭 확률 계산 ----------
	//
	int iGoldPercent = MobRankStats[GetMobRank()].iGoldPercent;

	if (pkAttacker->IsPC())
		iGoldPercent = iGoldPercent * (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD_DROP)) / 100;

	if (pkAttacker->GetPoint(POINT_MALL_GOLDBONUS))
		iGoldPercent += (iGoldPercent * pkAttacker->GetPoint(POINT_MALL_GOLDBONUS) / 100);

	iGoldPercent = iGoldPercent * CHARACTER_MANAGER::instance().GetMobGoldDropRate(pkAttacker) / 100;

	// ADD_PREMIUM
	if (pkAttacker->GetPremiumRemainSeconds(PREMIUM_GOLD) > 0 ||
		pkAttacker->IsEquipUniqueGroup(UNIQUE_GROUP_LUCKY_GOLD))
		iGoldPercent += iGoldPercent;
	// END_OF_ADD_PREMIUM

	if (iGoldPercent > 100)
		iGoldPercent = 100;

	int iPercent;

	if (GetMobRank() >= MOB_RANK_BOSS)
		iPercent = ((iGoldPercent * PERCENT_LVDELTA_BOSS(pkAttacker->GetLevel(), GetLevel())) / 100);
	else
		iPercent = ((iGoldPercent * PERCENT_LVDELTA(pkAttacker->GetLevel(), GetLevel())) / 100);
	//int iPercent = CALCULATE_VALUE_LVDELTA(pkAttacker->GetLevel(), GetLevel(), iGoldPercent);

	if (number(1, 100) > iPercent)
		return;

	int iGoldMultipler = GetGoldMultipler();

	if (1 == number(1, 50000)) // 1 / 50000 확률로 돈이 10배
		iGoldMultipler *= 10;
	else if (1 == number(1, 10000)) // 1 / 10000 확률로 돈이 5배
		iGoldMultipler *= 5;

	// 개인 적용
	if (pkAttacker->GetPoint(POINT_GOLD_DOUBLE_BONUS))
		if (number(1, 100) <= pkAttacker->GetPoint(POINT_GOLD_DOUBLE_BONUS))
			iGoldMultipler *= 2;

	//
	// --------- 돈 드롭 배수 결정 ----------
	//
	if (test_server)
		pkAttacker->ChatPacket(CHAT_TYPE_PARTY, "gold_mul %d rate %d", iGoldMultipler, CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker));

	//
	// --------- 실제 드롭 처리 -------------
	//
	LPITEM item;

	int iGold10DropPct = 100;
	iGold10DropPct = (iGold10DropPct * 100) / (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD10_DROP));

	// MOB_RANK가 BOSS보다 높으면 무조건 돈폭탄
	if (GetMobRank() >= MOB_RANK_BOSS && !IsStone() && GetMobTable().dwGoldMax != 0)
	{
		if (1 == number(1, iGold10DropPct))
			iGoldMultipler *= 10; // 1% 확률로 돈 10배

		int iSplitCount = number(25, 35);

		for (int i = 0; i < iSplitCount; ++i)
		{
			int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax) / iSplitCount;
			if (test_server)
				sys_log(0, "iGold %d", iGold);
			iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker) / 100;
			iGold *= iGoldMultipler;

			if (iGold == 0)
			{
				continue;
			}

			if (test_server)
			{
				sys_log(0, "Drop Moeny MobGoldAmountRate %d %d", CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker), iGoldMultipler);
				sys_log(0, "Drop Money gold %d GoldMin %d GoldMax %d", iGold, GetMobTable().dwGoldMax, GetMobTable().dwGoldMax);
			}

			// NOTE: 돈 폭탄은 제 3의 손 처리를 하지 않음
			if ((item = ITEM_MANAGER::instance().CreateItem(1, iGold)))
			{
				pos.x = GetX() + ((number(-14, 14) + number(-14, 14)) * 23);
				pos.y = GetY() + ((number(-14, 14) + number(-14, 14)) * 23);

				item->AddToGround(GetMapIndex(), pos);
				item->StartDestroyEvent();

				iTotalGold += iGold; // Total gold
			}
		}
	}
	// 1% 확률로 돈을 10개 떨어 뜨린다. (10배 드롭임)
	else if (1 == number(1, iGold10DropPct))
	{
		//
		// 돈 폭탄식 드롭
		//
		for (int i = 0; i < 10; ++i)
		{
			int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax);
			iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker) / 100;
			iGold *= iGoldMultipler;

			if (iGold == 0)
			{
				continue;
			}

			// NOTE : 돈 폭탄은 제 3의 손 처리를 하지 않음
			if ((item = ITEM_MANAGER::instance().CreateItem(1, iGold)))
			{
				pos.x = GetX() + (number(-7, 7) * 20);
				pos.y = GetY() + (number(-7, 7) * 20);

				item->AddToGround(GetMapIndex(), pos);
				item->StartDestroyEvent();

				iTotalGold += iGold; // Total gold
			}
		}
	}
	else
	{
		//
		// 일반적인 방식의 돈 드롭
		//
		int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax);
		iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker) / 100;
		iGold *= iGoldMultipler;

		int iSplitCount;

		if (iGold >= 3 && !LC_IsYMIR())
			iSplitCount = number(1, 3);
		else if (GetMobRank() >= MOB_RANK_BOSS)
		{
			iSplitCount = number(3, 10);

			if ((iGold / iSplitCount) == 0)
				iSplitCount = 1;
		}
		else
			iSplitCount = 1;

		if (iGold != 0)
		{
			iTotalGold += iGold; // Total gold

			for (int i = 0; i < iSplitCount; ++i)
			{
				if (isAutoLoot)
				{
					pkAttacker->GiveGold(iGold / iSplitCount);
				}
				else if ((item = ITEM_MANAGER::instance().CreateItem(1, iGold / iSplitCount)))
				{
					pos.x = GetX() + (number(-7, 7) * 20);
					pos.y = GetY() + (number(-7, 7) * 20);

					item->AddToGround(GetMapIndex(), pos);
					item->StartDestroyEvent();
				}
			}
		}
	}

	DBManager::instance().SendMoneyLog(MONEY_LOG_MONSTER, GetRaceNum(), iTotalGold);
}

void CHARACTER::Reward(bool bItemDrop)
{
	if (GetRaceNum() == 5001) // 왜구는 돈을 무조건 드롭
	{
		PIXEL_POSITION pos;

		if (!SECTREE_MANAGER::instance().GetMovablePosition(GetMapIndex(), GetX(), GetY(), pos))
			return;

		LPITEM item;
		int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax);
		iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(NULL) / 100;
		iGold *= GetGoldMultipler();
		int iSplitCount = number(25, 35);

		sys_log(0, "WAEGU Dead gold %d split %d", iGold, iSplitCount);

		for (int i = 1; i <= iSplitCount; ++i)
		{
			if ((item = ITEM_MANAGER::instance().CreateItem(1, iGold / iSplitCount)))
			{
				if (i != 0)
				{
					pos.x = number(-7, 7) * 20;
					pos.y = number(-7, 7) * 20;

					pos.x += GetX();
					pos.y += GetY();
				}

				item->AddToGround(GetMapIndex(), pos);
				item->StartDestroyEvent();
			}
		}
		return;
	}

	//PROF_UNIT puReward("Reward");
	LPCHARACTER pkAttacker = DistributeExp();

	if (!pkAttacker)
		return;

	//PROF_UNIT pu1("r1");
	if (pkAttacker->IsPC())
	{
		if (GetLevel() - pkAttacker->GetLevel() >= -10)
		{
			if (pkAttacker->GetRealAlignment() < 0)
			{
				if (pkAttacker->IsEquipUniqueItem(UNIQUE_ITEM_FASTER_ALIGNMENT_UP_BY_KILL))
					pkAttacker->UpdateAlignment(14);
				else
					pkAttacker->UpdateAlignment(7);
			}
			else
				pkAttacker->UpdateAlignment(2);
		}

		pkAttacker->SetQuestNPCID(GetVID());
		quest::CQuestManager::instance().Kill(pkAttacker->GetPlayerID(), GetRaceNum());
		CHARACTER_MANAGER::instance().KillLog(GetRaceNum());

#if defined(__PARTY_KILL_RENEWAL__)
		if (pkAttacker->GetParty())
		{
			quest::CQuestManager::instance().PartyKill(pkAttacker->GetPlayerID(), GetRaceNum());

			FPartyKill f(this, pkAttacker);
			pkAttacker->GetParty()->ForEachNearMember(f);
		}
#endif

		if (!number(0, 9))
		{
			if (pkAttacker->GetPoint(POINT_KILL_HP_RECOVERY))
			{
				int iHP = pkAttacker->GetMaxHP() * pkAttacker->GetPoint(POINT_KILL_HP_RECOVERY) / 100;
				pkAttacker->PointChange(POINT_HP, iHP);
				CreateFly(FLY_HP_SMALL, pkAttacker);
			}

			if (pkAttacker->GetPoint(POINT_KILL_SP_RECOVER))
			{
				int iSP = pkAttacker->GetMaxSP() * pkAttacker->GetPoint(POINT_KILL_SP_RECOVER) / 100;
				pkAttacker->PointChange(POINT_SP, iSP);
				CreateFly(FLY_SP_SMALL, pkAttacker);
			}
		}
	}
	//pu1.Pop();

	if (!bItemDrop)
		return;

	PIXEL_POSITION pos = GetXYZ();

	if (!SECTREE_MANAGER::instance().GetMovablePosition(GetMapIndex(), pos.x, pos.y, pos))
		return;

	//
	// 돈 드롭
	//
	//PROF_UNIT pu2("r2");
	if (test_server)
		sys_log(0, "Drop money : Attacker %s", pkAttacker->GetName());
	RewardGold(pkAttacker);
	//pu2.Pop();

	//
	// 아이템 드롭
	//
	//PROF_UNIT pu3("r3");
	LPITEM item;

	static std::vector<LPITEM> s_vec_item;
	s_vec_item.clear();

	if (ITEM_MANAGER::instance().CreateDropItem(this, pkAttacker, s_vec_item))
	{
		if (s_vec_item.size() == 0);
		else if (s_vec_item.size() == 1)
		{
			item = s_vec_item[0];

#if defined(__DICE_SYSTEM__)
			FPartyDropDiceRoll f(item, pkAttacker);
			f.Process(this);
#endif

#if defined (__PET_SYSTEM__) && defined(__PET_LOOT__)
			if ((f.GetItemOwner() == pkAttacker) && (pkAttacker->GetPetSystem() && pkAttacker->GetPetSystem()->LootItem(item)))
				pkAttacker->AutoGiveItem(item, true, true
#if defined(__WJ_PICKUP_ITEM_EFFECT__)
					, true
#endif
				);
			else
#endif
			{
				item->AddToGround(GetMapIndex(), pos);
				if (CBattleArena::instance().IsBattleArenaMap(pkAttacker->GetMapIndex()) == false)
#if defined(__DICE_SYSTEM__)
					item->SetOwnership(f.GetItemOwner());
#else
					item->SetOwnership(pkAttacker);
#endif
				item->StartDestroyEvent();

				pos.x = number(-7, 7) * 20;
				pos.y = number(-7, 7) * 20;
				pos.x += GetX();
				pos.y += GetY();

				sys_log(0, "DROP_ITEM: %s %d %d from %s", item->GetName(), pos.x, pos.y, GetName());
			}
		}
		else
		{
			int iItemIdx = s_vec_item.size() - 1;

			std::priority_queue<std::pair<int, LPCHARACTER>> pq;

			int total_dam = 0;

			for (TDamageMap::iterator it = m_map_kDamage.begin(); it != m_map_kDamage.end(); ++it)
			{
				int iDamage = it->second.iTotalDamage;
				if (iDamage > 0)
				{
					LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(it->first);

					if (ch)
					{
						pq.push(std::make_pair(iDamage, ch));
						total_dam += iDamage;
					}
				}
			}

			std::vector<LPCHARACTER> v;

			while (!pq.empty() && pq.top().first * 10 >= total_dam)
			{
				v.push_back(pq.top().second);
				pq.pop();
			}

			if (v.empty())
			{
				// 데미지를 특별히 많이 준 사람이 없으니 소유권 없음
				while (iItemIdx >= 0)
				{
					item = s_vec_item[iItemIdx--];

					if (!item)
					{
						sys_err("item null in vector idx %d", iItemIdx + 1);
						continue;
					}

					item->AddToGround(GetMapIndex(), pos);
					// 10% 이하 데미지 준 사람끼리는 소유권없음
					//item->SetOwnership(pkAttacker);
					item->StartDestroyEvent();

					pos.x = number(-7, 7) * 20;
					pos.y = number(-7, 7) * 20;
					pos.x += GetX();
					pos.y += GetY();

					sys_log(0, "DROP_ITEM: %s %d %d by %s", item->GetName(), pos.x, pos.y, GetName());
				}
			}
			else
			{
				// 데미지 많이 준 사람들 끼리만 소유권 나눠가짐
				std::vector<LPCHARACTER>::iterator it = v.begin();

				while (iItemIdx >= 0)
				{
					item = s_vec_item[iItemIdx--];

					if (!item)
					{
						sys_err("item null in vector idx %d", iItemIdx + 1);
						continue;
					}

					LPCHARACTER ch = *it;

					if (ch->GetParty())
						ch = ch->GetParty()->GetNextOwnership(ch, GetX(), GetY());

					++it;

					if (it == v.end())
						it = v.begin();

#if defined(__DICE_SYSTEM__)
					FPartyDropDiceRoll f(item, ch);
					f.Process(this);
#endif

#if defined(__PET_SYSTEM__) && defined(__PET_LOOT__)
					if ((f.GetItemOwner() == pkAttacker) && (pkAttacker->GetPetSystem() && pkAttacker->GetPetSystem()->LootItem(item)))
						pkAttacker->AutoGiveItem(item, true, true
#if defined(__WJ_PICKUP_ITEM_EFFECT__)
							, true
#endif
						);
					else
#endif
					{
						item->AddToGround(GetMapIndex(), pos);
						if (CBattleArena::instance().IsBattleArenaMap(ch->GetMapIndex()) == false)
						{
#if defined(__DICE_SYSTEM__)
							item->SetOwnership(f.GetItemOwner());
#else
							item->SetOwnership(ch);
#endif
						}
						item->StartDestroyEvent();

						pos.x = number(-7, 7) * 20;
						pos.y = number(-7, 7) * 20;
						pos.x += GetX();
						pos.y += GetY();

						sys_log(0, "DROP_ITEM: %s %d %d by %s", item->GetName(), pos.x, pos.y, GetName());
					}
				}
			}
		}
	}

	m_map_kDamage.clear();
}

struct TItemDropPenalty
{
	int iInventoryPct; // Range: 1 ~ 1000
	int iInventoryQty; // Range: --
	int iEquipmentPct; // Range: 1 ~ 100
	int iEquipmentQty; // Range: --
};

TItemDropPenalty aItemDropPenalty_kor[9] =
{
	{ 0, 0, 0, 0 }, // 선왕
	{ 0, 0, 0, 0 }, // 영웅
	{ 0, 0, 0, 0 }, // 성자
	{ 0, 0, 0, 0 }, // 지인
	{ 0, 0, 0, 0 }, // 양민
	{ 20, 5, 5, 1 }, // 낭인
	{ 30, 10, 10, 2 }, // 악인
	{ 40, 20, 35, 3 }, // 마두
	{ 80, 40, 40, 4 }, // 패왕
};

void CHARACTER::ItemDropPenalty(LPCHARACTER pkKiller)
{
	// 개인상점을 연 상태에서는 아이템을 드롭하지 않는다.
	if (GetMyShop())
		return;

	if (false == LC_IsYMIR())
	{
		if (GetLevel() < 50)
			return;
	}

	if (CArenaManager::Instance().IsArenaMap(GetMapIndex()))
		return;

	if (CBattleArena::instance().IsBattleArenaMap(GetMapIndex()) == true)
		return;

	struct TItemDropPenalty* table = &aItemDropPenalty_kor[0];

	if (GetLevel() < 10)
		return;

	int iAlignIndex;
	if (GetRealAlignment() >= 120000)
		iAlignIndex = 0;
	else if (GetRealAlignment() >= 80000)
		iAlignIndex = 1;
	else if (GetRealAlignment() >= 40000)
		iAlignIndex = 2;
	else if (GetRealAlignment() >= 10000)
		iAlignIndex = 3;
	else if (GetRealAlignment() >= 0)
		iAlignIndex = 4;
	else if (GetRealAlignment() > -40000)
		iAlignIndex = 5;
	else if (GetRealAlignment() > -80000)
		iAlignIndex = 6;
	else if (GetRealAlignment() > -120000)
		iAlignIndex = 7;
	else
		iAlignIndex = 8;

	std::vector<std::pair<LPITEM, int>> vec_item;
	LPITEM pkItem;
	int i;
	bool isDropAllEquipments = false;

	TItemDropPenalty& r = table[iAlignIndex];
	sys_log(0, "%s align %d inven_pct %d equip_pct %d", GetName(), iAlignIndex, r.iInventoryPct, r.iEquipmentPct);

	bool bDropInventory = r.iInventoryPct >= number(1, 1000);
	bool bDropEquipment = r.iEquipmentPct >= number(1, 100);
	bool bDropAntiDropUniqueItem = false;

	if ((bDropInventory || bDropEquipment) && IsEquipUniqueItem(UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY))
	{
		bDropInventory = false;
		bDropEquipment = false;
		bDropAntiDropUniqueItem = true;
	}

	if (bDropInventory) // Drop Inventory
	{
		const bool bIsRunningQuest = IsRunningQuest();
		std::vector<BYTE> vec_bSlots;

		for (i = 0; i < INVENTORY_MAX_NUM; ++i)
		{
			pkItem = GetInventoryItem(i);
			if (pkItem && !(bIsRunningQuest && pkItem->GetType() == ITEM_QUEST))
				vec_bSlots.push_back(i);
		}

		if (!vec_bSlots.empty())
		{
			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(vec_bSlots.begin(), vec_bSlots.end(), g);

			int iQty = MIN(vec_bSlots.size(), r.iInventoryQty);

			if (iQty)
				iQty = number(1, iQty);

			for (i = 0; i < iQty; ++i)
			{
				pkItem = GetInventoryItem(vec_bSlots[i]);

#if defined(__MAILBOX__)
				if (IS_SET(pkItem->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP | ITEM_ANTIFLAG_MYSHOP | ITEM_ANTIFLAG_MAIL))
#else
				if (IS_SET(pkItem->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP | ITEM_ANTIFLAG_MYSHOP))
#endif
					continue;

#if defined(__SOUL_BIND_SYSTEM__)
				if (pkItem->IsSealed())
					continue;
#endif

				SyncQuickslot(SLOT_TYPE_INVENTORY, vec_bSlots[i], WORD_MAX);
				vec_item.push_back(std::make_pair(pkItem->RemoveFromCharacter(), INVENTORY));
			}
		}
		else if (iAlignIndex == 8)
			isDropAllEquipments = true;
	}

	if (bDropEquipment) // Drop Equipment
	{
		std::vector<BYTE> vec_bSlots;

		for (i = 0; i < EQUIPMENT_MAX_NUM; ++i)
			if (GetEquipmentItem(i))
				vec_bSlots.push_back(i);

		if (!vec_bSlots.empty())
		{
			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(vec_bSlots.begin(), vec_bSlots.end(), g);
			int iQty;

			if (isDropAllEquipments)
				iQty = vec_bSlots.size();
			else
				iQty = MIN(vec_bSlots.size(), number(1, r.iEquipmentQty));

			if (iQty)
				iQty = number(1, iQty);

			for (i = 0; i < iQty; ++i)
			{
				pkItem = GetEquipmentItem(vec_bSlots[i]);

#if defined(__MAILBOX__)
				if (IS_SET(pkItem->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP | ITEM_ANTIFLAG_MYSHOP | ITEM_ANTIFLAG_MAIL))
#else
				if (IS_SET(pkItem->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP | ITEM_ANTIFLAG_MYSHOP))
#endif
					continue;

#if defined(__DRAGON_SOUL_SYSTEM__)
				if (pkItem->IsDragonSoul())
					continue;
#endif

#if defined(__SOUL_BIND_SYSTEM__)
				if (pkItem->IsSealed())
					continue;
#endif

				SyncQuickslot(SLOT_TYPE_INVENTORY, vec_bSlots[i], WORD_MAX);
				vec_item.push_back(std::make_pair(pkItem->RemoveFromCharacter(), EQUIPMENT));
			}
		}
	}

	if (bDropAntiDropUniqueItem)
	{
		LPITEM pkItem;

		pkItem = GetWear(WEAR_UNIQUE1);

		if (pkItem && pkItem->GetVnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
		{
			SyncQuickslot(SLOT_TYPE_INVENTORY, WEAR_UNIQUE1, WORD_MAX);
			vec_item.push_back(std::make_pair(pkItem->RemoveFromCharacter(), EQUIPMENT));
		}

		pkItem = GetWear(WEAR_UNIQUE2);

		if (pkItem && pkItem->GetVnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
		{
			SyncQuickslot(SLOT_TYPE_INVENTORY, WEAR_UNIQUE2, WORD_MAX);
			vec_item.push_back(std::make_pair(pkItem->RemoveFromCharacter(), EQUIPMENT));
		}
	}

	{
		PIXEL_POSITION pos;
		pos.x = GetX();
		pos.y = GetY();

		unsigned int i;

		for (i = 0; i < vec_item.size(); ++i)
		{
			LPITEM item = vec_item[i].first;
			int window = vec_item[i].second;

			item->AddToGround(GetMapIndex(), pos);
			item->StartDestroyEvent();

			sys_log(0, "DROP_ITEM_PK: %s %d %d from %s", item->GetName(), pos.x, pos.y, GetName());
			LogManager::instance().ItemLog(this, item, "DEAD_DROP", (window == INVENTORY) ? "INVENTORY" : ((window == EQUIPMENT) ? "EQUIPMENT" : ""));

			pos.x = GetX() + number(-7, 7) * 20;
			pos.y = GetY() + number(-7, 7) * 20;
		}
	}
}

class FPartyAlignmentCompute
{
public:
	FPartyAlignmentCompute(int iAmount, int x, int y)
	{
		m_iAmount = iAmount;
		m_iCount = 0;
		m_iStep = 0;
		m_iKillerX = x;
		m_iKillerY = y;
	}

	void operator () (LPCHARACTER pkChr)
	{
		if (DISTANCE_APPROX(pkChr->GetX() - m_iKillerX, pkChr->GetY() - m_iKillerY) < PARTY_DEFAULT_RANGE)
		{
			if (m_iStep == 0)
			{
				++m_iCount;
			}
			else
			{
				pkChr->UpdateAlignment(m_iAmount / m_iCount);
			}
		}
	}

	int m_iAmount;
	int m_iCount;
	int m_iStep;

	int m_iKillerX;
	int m_iKillerY;
};

void CHARACTER::Dead(LPCHARACTER pkKiller, bool bImmediateDead)
{
	if (IsDead())
		return;

#if !defined(__MOUNT_COSTUME_SYSTEM__)
	if (IsHorseRiding())
	{
		StopRiding();
	}
	else if (GetMountVnum())
	{
		RemoveAffect(AFFECT_MOUNT_BONUS);

		m_dwMountVnum = 0;
		UnEquipSpecialRideUniqueItem();

		UpdatePacket();
	}
#endif

	if (!pkKiller && m_dwKillerPID)
		pkKiller = CHARACTER_MANAGER::instance().FindByPID(m_dwKillerPID);

	m_dwKillerPID = 0; // 반드시 초기화 해야함 DO NOT DELETE THIS LINE UNLESS YOU ARE 1000000% SURE

#if defined(__SKILL_COOLTIME_UPDATE__)
	if (IsPC())
		ResetSkillCoolTimes();
#endif

	// 20200716.Owsap : Remove bad affects on death.
	if (IsPC())
		RemoveBadAffect();

	ForgetMyAttacker(false);

	bool isAgreedPVP = false;
	bool isUnderGuildWar = false;
	bool isDuel = false;
	bool isForked = false;

	if (pkKiller && pkKiller->IsPC())
	{
		if (pkKiller->m_pkChrTarget == this)
			pkKiller->SetTarget(NULL);

		if (!IsPC() && pkKiller->GetDungeon())
			pkKiller->GetDungeon()->IncKillCount(pkKiller, this);

		isAgreedPVP = CPVPManager::instance().Dead(this, pkKiller->GetPlayerID());
		isDuel = CArenaManager::instance().OnDead(pkKiller, this);

		if (IsPC())
		{
			CGuild* g1 = GetGuild();
			CGuild* g2 = pkKiller->GetGuild();

			if (g1 && g2)
				if (g1->UnderWar(g2->GetID()))
					isUnderGuildWar = true;

			pkKiller->SetQuestNPCID(GetVID());
			quest::CQuestManager::instance().Kill(pkKiller->GetPlayerID(), quest::QUEST_NO_NPC);
			CGuildManager::instance().Kill(pkKiller, this);
		}
	}

	if (IsPC())
	{
#if defined(__QUEST_EVENT_DEAD__)
		if (pkKiller)
			SetQuestNPCID(pkKiller->GetVID());

		quest::CQuestManager::Instance().Dead(GetPlayerID(), (pkKiller) ? pkKiller->GetRaceNum() : quest::QUEST_NO_NPC);
#endif

		// CHECK_FORKEDROAD_WAR
		if (CThreeWayWar::instance().IsThreeWayWarMapIndex(GetMapIndex()))
			isForked = true;
		// END_CHECK_FORKEDROAD_WAR
	}

	if (pkKiller &&
		!isAgreedPVP &&
		!isUnderGuildWar &&
		IsPC() &&
		!isDuel &&
		!isForked &&
		!IS_CASTLE_MAP(GetMapIndex()))
	{
		if (GetGMLevel() == GM_PLAYER || test_server)
		{
			ItemDropPenalty(pkKiller);
		}
	}

	// CASTLE_SIEGE
	if (IS_CASTLE_MAP(GetMapIndex()))
	{
		if (CASTLE_FROG_VNUM == GetRaceNum())
			castle_frog_die(this, pkKiller);
		else if (castle_is_guard_vnum(GetRaceNum()))
			castle_guard_die(this, pkKiller);
		else if (castle_is_tower_vnum(GetRaceNum()))
			castle_tower_die(this, pkKiller);
	}
	// CASTLE_SIEGE

	if (true == isForked)
	{
		CThreeWayWar::instance().onDead(this, pkKiller);
	}

	SetPosition(POS_DEAD);
	ClearAffect(true);

	if (pkKiller && IsPC())
	{
		if (!pkKiller->IsPC())
		{
			if (!isForked)
			{
				sys_log(1, "DEAD: %s %p WITH PENALTY", GetName(), this);
				SET_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);
				LogManager::instance().CharLog(this, pkKiller->GetRaceNum(), "DEAD_BY_NPC", pkKiller->GetName());
			}
		}
		else
		{
			sys_log(1, "DEAD_BY_PC: %s %p KILLER %s %p", GetName(), this, pkKiller->GetName(), get_pointer(pkKiller));
			REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);

			if (GetEmpire() != pkKiller->GetEmpire())
			{
				int iEP = MIN(GetPoint(POINT_EMPIRE_POINT), pkKiller->GetPoint(POINT_EMPIRE_POINT));

				PointChange(POINT_EMPIRE_POINT, -(iEP / 10));
				pkKiller->PointChange(POINT_EMPIRE_POINT, iEP / 5);

				if (GetPoint(POINT_EMPIRE_POINT) < 10)
				{
					// TODO : 입구로 날리는 코드를 넣어야 한다.
				}

				char buf[256];
				snprintf(buf, sizeof(buf),
					"%d %d %d %s %d %d %d %s",
					GetEmpire(), GetAlignment(), GetPKMode(), GetName(),
					pkKiller->GetEmpire(), pkKiller->GetAlignment(), pkKiller->GetPKMode(), pkKiller->GetName());

				LogManager::instance().CharLog(this, pkKiller->GetPlayerID(), "DEAD_BY_PC", buf);
			}
			else
			{
				if (!isAgreedPVP && !isUnderGuildWar && !IsKillerMode() && GetAlignment() >= 0 && !isDuel && !isForked)
				{
					int iNoPenaltyProb = 0;

					if (g_iUseLocale)
					{
						if (pkKiller->GetAlignment() >= 0) // 1 / 3 percent down
							iNoPenaltyProb = 33;
						else // 4 / 5 percent down
							iNoPenaltyProb = 20;
					}

					if (number(1, 100) < iNoPenaltyProb)
						pkKiller->ChatPacket(CHAT_TYPE_INFO, LC_STRING("용신의 보호로 아이템이 떨어지지 않았습니다."));
					else
					{
						if (g_iUseLocale && pkKiller->GetParty())
						{
							FPartyAlignmentCompute f(-20000, pkKiller->GetX(), pkKiller->GetY());
							pkKiller->GetParty()->ForEachOnMapMember(f, pkKiller->GetMapIndex());

							if (f.m_iCount == 0)
								pkKiller->UpdateAlignment(-20000);
							else
							{
								sys_log(0, "ALIGNMENT PARTY count %d amount %d", f.m_iCount, f.m_iAmount);

								f.m_iStep = 1;
								pkKiller->GetParty()->ForEachOnMapMember(f, pkKiller->GetMapIndex());
							}
						}
						else
							pkKiller->UpdateAlignment(-20000);
					}
				}

				char buf[256];
				snprintf(buf, sizeof(buf),
					"%d %d %d %s %d %d %d %s",
					GetEmpire(), GetAlignment(), GetPKMode(), GetName(),
					pkKiller->GetEmpire(), pkKiller->GetAlignment(), pkKiller->GetPKMode(), pkKiller->GetName());

				LogManager::instance().CharLog(this, pkKiller->GetPlayerID(), "DEAD_BY_PC", buf);
			}
		}
	}
	else
	{
		sys_log(1, "DEAD: %s %p", GetName(), this);
		REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);
	}

	ClearSync();

	//sys_log(1, "stun cancel %s[%d]", GetName(), (DWORD)GetVID());
	event_cancel(&m_pkStunEvent); // 기절 이벤트는 죽인다.

	if (IsPC())
	{
		m_dwLastDeadTime = get_dword_time();
		SetKillerMode(false);
		GetDesc()->SetPhase(PHASE_DEAD);
	}
	else
	{
		// 가드에게 공격받은 몬스터는 보상이 없어야 한다.
		if (!IS_SET(m_pointsInstant.instant_flag, INSTANT_FLAG_NO_REWARD))
		{
			if (!(pkKiller && pkKiller->IsPC() && pkKiller->GetGuild() && pkKiller->GetGuild()->UnderAnyWar(GUILD_WAR_TYPE_FIELD)))
			{
				// 부활하는 몬스터는 보상을 주지 않는다.
				if (GetMobTable().dwResurrectionVnum)
				{
					// DUNGEON_MONSTER_REBIRTH_BUG_FIX
					LPCHARACTER chResurrect = CHARACTER_MANAGER::instance().SpawnMob(GetMobTable().dwResurrectionVnum, GetMapIndex(), GetX(), GetY(), GetZ(), true, (int)GetRotation());
					if (GetDungeon() && chResurrect)
					{
						chResurrect->SetDungeon(GetDungeon());
					}
					// END_OF_DUNGEON_MONSTER_REBIRTH_BUG_FIX

					Reward(false);
				}
				else if (IsRevive() == true)
				{
					Reward(false);
				}
				else
				{
					Reward(true); // Drops gold, item, etc..
				}
			}
			else
			{
				if (pkKiller->m_dwUnderGuildWarInfoMessageTime < get_dword_time())
				{
					pkKiller->m_dwUnderGuildWarInfoMessageTime = get_dword_time() + 60000;
					pkKiller->ChatPacket(CHAT_TYPE_INFO, LC_STRING("<길드> 길드전중에는 사냥에 따른 이익이 없습니다."));
				}
			}
		}
	}

	// BOSS_KILL_LOG
	if (GetMobRank() >= MOB_RANK_BOSS && pkKiller && pkKiller->IsPC())
	{
		char buf[51];
		snprintf(buf, sizeof(buf), "%d %ld", g_bChannel, pkKiller->GetMapIndex());
		if (IsStone())
			LogManager::instance().CharLog(pkKiller, GetRaceNum(), "STONE_KILL", buf);
		else
			LogManager::instance().CharLog(pkKiller, GetRaceNum(), "BOSS_KILL", buf);
	}
	// END_OF_BOSS_KILL_LOG

	TPacketGCDead pack;
	pack.header = HEADER_GC_DEAD;
	pack.vid = m_vid;
	pack.map_index = GetMapIndex();
	pack.dialog_type = DEAD_DIALOG_NORMAL;

#if defined(__ELEMENTAL_DUNGEON__)
	if (IS_ELEMENTAL_DUNGEON(GetMapIndex()))
		pack.dialog_type = DEAD_DIALOG_GIVE_UP;
#endif

	PacketAround(&pack, sizeof(pack));

	REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_STUN);

	// 플레이어 캐릭터이면
	if (GetDesc() != NULL)
	{
		//
		// 클라이언트에 에펙트 패킷을 다시 보낸다.
		//
		auto it = m_list_pkAffect.begin();
		while (it != m_list_pkAffect.end())
			SendAffectAddPacket(GetDesc(), *it++);
	}

	//
	// Dead 이벤트 생성,
	//
	// Dead 이벤트에서는 몬스터의 경우 몇초 후에 Destroy 되도록 해주며,
	// PC의 경우 3분 있다가 마을에서 나오도록 해 준다. 3분 내에는 유저로부터
	// 마을에서 시작할 건지, 여기서 시작할 건지 결정을 받는다.
	if (isDuel == false)
	{
		if (m_pkDeadEvent)
		{
			sys_log(1, "DEAD_EVENT_CANCEL: %s %p %p", GetName(), this, get_pointer(m_pkDeadEvent));
			event_cancel(&m_pkDeadEvent);
		}

		if (IsStone())
			ClearStone();

		if (GetDungeon())
			GetDungeon()->DeadCharacter(this);

		SCharDeadEventInfo* pEventInfo = AllocEventInfo<SCharDeadEventInfo>();

		if (IsPC())
		{
			pEventInfo->isPC = true;
			pEventInfo->dwID = this->GetPlayerID();

			m_pkDeadEvent = event_create(dead_event, pEventInfo, PASSES_PER_SEC(180));
		}
		else
		{
			pEventInfo->isPC = false;
			pEventInfo->dwID = this->GetVID();

			if (IsRevive() == false && HasReviverInParty() == true)
			{
				m_pkDeadEvent = event_create(dead_event, pEventInfo, bImmediateDead ? 1 : PASSES_PER_SEC(1));
			}
			else
			{
				BYTE sec = 3;
				if (!IsMonster())
					sec = 10;

				m_pkDeadEvent = event_create(dead_event, pEventInfo, bImmediateDead ? 1 : PASSES_PER_SEC(sec));
			}
		}

		sys_log(1, "DEAD_EVENT_CREATE: %s %p %p", GetName(), this, get_pointer(m_pkDeadEvent));
	}

	if (m_pkExchange != NULL)
	{
		m_pkExchange->Cancel();
	}

	if (IsCubeOpen() == true)
	{
#if defined(__CUBE_RENEWAL__)
		CCubeManager::Instance().CloseCube(this);
#else
		Cube_close(this);
#endif
	}

	CShopManager::instance().StopShopping(this);
	CloseMyShop();
	CloseSafebox();

#if defined(__ACCE_COSTUME_SYSTEM__)
	if (IsAcceRefineWindowOpen())
		AcceRefineWindowClose(true);
#endif

#if defined(__AURA_COSTUME_SYSTEM__)
	if (IsAuraRefineWindowOpen())
		AuraRefineWindowClose(true);
#endif

#if defined(__MT_THUNDER_DUNGEON__)
	if (IsMonster() && GetMobTable().dwVnum == CMTThunderDungeon::GUARDIAN_VNUM)
		if ((pkKiller) && (pkKiller->GetMapIndex() == CMTThunderDungeon::MAP_INDEX))
			CMTThunderDungeon::Instance().SpawnPortal(this);
#endif

	if (IsMonster() && BlueDragon::BossVnum == GetMobTable().dwVnum)
	{
		if (NULL != pkKiller && NULL != pkKiller->GetGuild())
		{
			CDragonLairManager::instance().OnDragonDead(this, pkKiller->GetGuild()->GetID());
		}
		else
		{
			sys_err("DragonLair: Dragon killed by nobody");
		}
	}

#if defined(__GUILD_DRAGONLAIR_SYSTEM__)
	if (pkKiller && CGuildDragonLairManager::Instance().IsRedDragonLair(pkKiller->GetMapIndex()))
	{
		if (IsMonster() || IsStone())
		{
			CGuildDragonLair* pGuildDragonLair = pkKiller->GetGuildDragonLair();
			if (pGuildDragonLair)
				pGuildDragonLair->Kill(this);
		}
	}
#endif
}

struct FuncSetLastAttacked
{
	FuncSetLastAttacked(DWORD dwTime) : m_dwTime(dwTime)
	{
	}

	void operator () (LPCHARACTER ch)
	{
		ch->SetLastAttacked(m_dwTime);
	}

	DWORD m_dwTime;
};

void CHARACTER::SetLastAttacked(DWORD dwTime)
{
	assert(m_pkMobInst != NULL);

	m_pkMobInst->m_dwLastAttackedTime = dwTime;
	m_pkMobInst->m_posLastAttacked = GetXYZ();
}

void CHARACTER::SendDamagePacket(LPCHARACTER pAttacker, int Damage, BYTE DamageFlag)
{
	{
		TPacketGCDamageInfo damageInfo;
		memset(&damageInfo, 0, sizeof(TPacketGCDamageInfo));

		damageInfo.header = HEADER_GC_DAMAGE_INFO;
		damageInfo.dwVID = (DWORD)GetVID();
		damageInfo.flag = DamageFlag;
		damageInfo.damage = Damage;

		if (GetDesc() != NULL)
		{
			GetDesc()->Packet(&damageInfo, sizeof(TPacketGCDamageInfo));
		}

		if (pAttacker && pAttacker->GetDesc())
		{
			if (pAttacker->GetTarget() == this)
				pAttacker->GetDesc()->Packet(&damageInfo, sizeof(TPacketGCDamageInfo));
		}

		/*
		if (GetArenaObserverMode() == false && GetArena() != NULL)
		{
			GetArena()->SendPacketToObserver(&damageInfo, sizeof(TPacketGCDamageInfo));
		}
		*/
	}
}

//
// CHARACTER::Damage 메소드는 this가 데미지를 입게 한다.
//
// Arguments
//	pAttacker : 공격자
//	dam : 데미지
//	EDamageType : 어떤 형식의 공격인가?
//
// Return value
//	true : dead
//	false : not dead yet
//
bool CHARACTER::Damage(LPCHARACTER pAttacker, int dam, EDamageType type /*= DAMAGE_TYPE_NORMAL*/) // returns true if dead
{
	if (IsPC() && IsDead())
		return false;

	if (!GetSectree() || GetSectree()->IsAttr(GetX(), GetY(), ATTR_BANPK))
		return false;

	// 20190823.Owsap : Check skill affect without weapon.
	if (pAttacker && this)
	{
		if (pAttacker->IsAffectFlag(AFF_GWIGUM) && !pAttacker->GetWear(WEAR_WEAPON))
		{
			pAttacker->RemoveAffect(SKILL_GWIGEOM);
			return false;
		}

		if (pAttacker->IsAffectFlag(AFF_GEOMGYEONG) && !pAttacker->GetWear(WEAR_WEAPON))
		{
			pAttacker->RemoveAffect(SKILL_GEOMKYUNG);
			return false;
		}
	}

#if defined(__9TH_SKILL__)
	if (pAttacker && FindAffect(AFFECT_CHEONUN_INVINCIBILITY))
	{
		SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
		return false;
	}
#endif

#if defined(__CONQUEROR_LEVEL__)
	if (pAttacker && pAttacker->IsNewWorldMapIndex() && pAttacker->IsPC() && !IsStone())
	{
		if (pAttacker->IsSungMaCursed(POINT_SUNGMA_STR))
			dam /= 2;

		int iHitPct = pAttacker->GetNewWorldSungMa(POINT_HIT_PCT);
		if (iHitPct > 0 && iHitPct > pAttacker->GetPoint(POINT_HIT_PCT))
		{
			int iMissHitPct = ((iHitPct - pAttacker->GetPoint(POINT_HIT_PCT)) * 100.0) / iHitPct;
			if (number(1, 100) <= iMissHitPct)
			{
				SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
				return false;
			}
		}
	}
#endif

#if defined(__SNOW_DUNGEON__)
	if (pAttacker && IS_SNOW_DUNGEON(pAttacker->GetMapIndex()))
	{
		LPDUNGEON pDungeon = pAttacker->GetDungeon();
		if (pDungeon)
		{
			switch (GetRaceNum())
			{
				case 8058: // Metin of Cold
#if defined(__LABYRINTH_DUNGEON__)
				case 8125: // TR: Metin of Cold
				case 8135: // RX: Metin of Cold
#endif
				{
					if (pDungeon->GetFlag("level") == 6)
					{
						if (pAttacker->GetJob() != JOB_SHAMAN)
						{
							SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
							return false;
						}
					}
				}
				break;

				case 20399: // North Dragon Pillar
#if defined(__LABYRINTH_DUNGEON__)
				case 20518: // TR: North Dragon Pillar
				case 20538: // RX: North Dragon Pillar
#endif
				{
					if (pDungeon->GetFlag("level") == 9)
					{
						if (pAttacker->GetJob() != JOB_SHAMAN && pAttacker->GetJob() != JOB_ASSASSIN)
						{
							SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
							return false;
						}
					}
				}
				break;

				case 6151: // Szel
#if defined(__LABYRINTH_DUNGEON__)
				case 4249: // TR: Szel
				case 4329: // RX: Szel
#endif
				{
					if (pDungeon->GetFlag("level") == 4)
					{
						if (!pAttacker->IsAffectFlag(AFF_HOSIN))
						{
							SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
							return false;
						}
					}
					else if (pDungeon->GetFlag("level") == 7)
					{
						if (!pAttacker->IsAffectFlag(AFF_GICHEON))
						{
							SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
							return false;
						}
					}
				}
				break;

			}
		}
	}
#endif

#if defined(__BLUE_DRAGON_RENEWAL__)
	if (BlueDragon_IsBoss(GetRaceNum()))
	{
		if (BlueDragon_Block(pAttacker->GetMapIndex()))
		{
			SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
			return false;
		}
	}
#endif

#if defined(__DEFENSE_WAVE__)
	if (pAttacker && GetDefenseWave())
	{
		if (GetDefenseWave()->Damage(this) == false)
		{
			SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
			return false;
		}
	}
#endif

#if defined(__GUILD_DRAGONLAIR_SYSTEM__)
	if (pAttacker && CGuildDragonLairManager::Instance().IsRedDragonLair(pAttacker->GetMapIndex()))
	{
		if (CGuildDragonLairManager::Instance().IsStone(GetRaceNum()))
		{
			CGuildDragonLair* pGuildDragonLair = pAttacker->GetGuildDragonLair();
			if (pGuildDragonLair && pGuildDragonLair->Damage(this) == false)
			{
				SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
				return false;
			}
		}
		else if (CGuildDragonLairManager::Instance().IsKing(GetRaceNum()))
		{
			SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
			return false;
		}
	}
#endif

	if (EDamageType::DAMAGE_TYPE_MAGIC == type)
	{
		dam = (int)((float)dam * (100 + (pAttacker->GetPoint(POINT_MAGIC_ATT_BONUS_PER) + pAttacker->GetPoint(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100.f + 0.5f);
		
		int iMtkBonus = pAttacker->GetPoint(POINT_MAGIC_ATT_GRADE_BONUS);
		if (iMtkBonus > 0)
		{
			int iRawMtkBonus = dam * iMtkBonus / 100;
			float fAdjFactor = iMtkBonus / 10.0f - 2.5f;
			int iFinalMtkBonus = int(iRawMtkBonus / 100.0f * fAdjFactor + 0.5f);
			dam += iFinalMtkBonus;
		}
	}

	if (GetRaceNum() == 5001)
	{
		bool bDropMoney = false;
		int iPercent = 0;
		if (GetMaxHP() >= 0)
			iPercent = (GetHP() * 100) / GetMaxHP();

		if (iPercent <= 10 && GetMaxSP() < 5)
		{
			SetMaxSP(5);
			bDropMoney = true;
		}
		else if (iPercent <= 20 && GetMaxSP() < 4)
		{
			SetMaxSP(4);
			bDropMoney = true;
		}
		else if (iPercent <= 40 && GetMaxSP() < 3)
		{
			SetMaxSP(3);
			bDropMoney = true;
		}
		else if (iPercent <= 60 && GetMaxSP() < 2)
		{
			SetMaxSP(2);
			bDropMoney = true;
		}
		else if (iPercent <= 80 && GetMaxSP() < 1)
		{
			SetMaxSP(1);
			bDropMoney = true;
		}

		if (bDropMoney)
		{
			DWORD dwGold = 1000;
			int iSplitCount = number(10, 13);

			sys_log(0, "WAEGU DropGoldOnHit %d times", GetMaxSP());

			for (int i = 1; i <= iSplitCount; ++i)
			{
				PIXEL_POSITION pos;
				LPITEM item;

				if ((item = ITEM_MANAGER::instance().CreateItem(1, dwGold / iSplitCount)))
				{
					if (i != 0)
					{
						pos.x = (number(-14, 14) + number(-14, 14)) * 20;
						pos.y = (number(-14, 14) + number(-14, 14)) * 20;

						pos.x += GetX();
						pos.y += GetY();
					}

					item->AddToGround(GetMapIndex(), pos);
					item->StartDestroyEvent();
				}
			}
		}
	}

	// 평타가 아닐 때는 공포 처리
	if (type != EDamageType::DAMAGE_TYPE_NORMAL && type != EDamageType::DAMAGE_TYPE_NORMAL_RANGE)
	{
		if (IsAffectFlag(AFF_TERROR))
		{
			int pct = GetSkillPower(SKILL_TERROR) / 400;

			if (number(1, 100) <= pct)
				return false;
		}
	}

	int iCurHP = GetHP();
	int iCurSP = GetSP();

	bool IsCritical = false;
	bool IsPenetrate = false;
	bool IsDeathBlow = false;

	//PROF_UNIT puAttr("Attr");

	//
	// 마법형 스킬과, 레인지형 스킬은(궁자객) 크리티컬과, 관통공격 계산을 한다.
	// 원래는 하지 않아야 하는데 Nerf(다운밸런스)패치를 할 수 없어서 크리티컬과
	// 관통공격의 원래 값을 쓰지 않고, /2 이상하여 적용한다.
	//
	// 무사 이야기가 많아서 밀리 스킬도 추가
	//
	// 20091109 : 무사가 결과적으로 엄청나게 강해진 것으로 결론남, 독일 기준 무사 비율 70% 육박
	//
	if (type == EDamageType::DAMAGE_TYPE_MELEE || type == EDamageType::DAMAGE_TYPE_RANGE || type == EDamageType::DAMAGE_TYPE_MAGIC)
	{
		if (pAttacker)
		{
			// 크리티컬
			int iCriticalPct = pAttacker->GetPoint(POINT_CRITICAL_PCT);

			if (!IsPC())
				iCriticalPct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);

			if (iCriticalPct)
			{
				if (iCriticalPct >= 10) // 10보다 크면 5% + (4마다 1%씩 증가), 따라서 수치가 50이면 20%
					iCriticalPct = 5 + (iCriticalPct - 10) / 4;
				else // 10보다 작으면 단순히 반으로 깎음, 10 = 5%
					iCriticalPct /= 2;

				// 크리티컬 저항 값 적용.
				iCriticalPct -= GetPoint(POINT_RESIST_CRITICAL);

				if (number(1, 100) <= iCriticalPct)
				{
					IsCritical = true;

					// NOTE : According to the planner, in 2018 the critical damage multiplier in PvP
					// was adjusted from 2 to 1.5 in order to balance the damage.
					dam *= 1.5; // 2.0

					EffectPacket(SE_CRITICAL);

					if (IsAffectFlag(AFF_MANASHIELD))
					{
						RemoveAffect(AFF_MANASHIELD);
					}
				}
			}

			// 관통공격
			int iPenetratePct = pAttacker->GetPoint(POINT_PENETRATE_PCT);

			if (!IsPC())
				iPenetratePct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);

			if (iPenetratePct)
			{
				{
					CSkillProto* pkSk = CSkillManager::instance().Get(SKILL_RESIST_PENETRATE);

					if (NULL != pkSk)
					{
						pkSk->SetPointVar("k", 1.0f * GetSkillPower(SKILL_RESIST_PENETRATE) / 100.0f);

						iPenetratePct -= static_cast<int>(pkSk->kPointPoly.Eval());
					}
				}

				if (iPenetratePct >= 10)
				{
					// 10보다 크면 5% + (4마다 1%씩 증가), 따라서 수치가 50이면 20%
					iPenetratePct = 5 + (iPenetratePct - 10) / 4;
				}
				else
				{
					// 10보다 작으면 단순히 반으로 깎음, 10 = 5%
					iPenetratePct /= 2;
				}

				// 관통타격 저항 값 적용.
				iPenetratePct -= GetPoint(POINT_RESIST_PENETRATE);

				if (number(1, 100) <= iPenetratePct)
				{
					IsPenetrate = true;

					if (test_server)
						ChatPacket(CHAT_TYPE_INFO, LC_STRING("관통 추가 데미지 %d", GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100));

					dam += GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100;
					if (iPenetratePct > 100)
						dam += dam * (iPenetratePct - 100) / 200;

					EffectPacket(SE_PENETRATE);

					if (IsAffectFlag(AFF_MANASHIELD))
					{
						RemoveAffect(AFF_MANASHIELD);
					}
				}
			}
		}
	}
	//
	// 콤보 공격, 활 공격, 즉 평타 일 때만 속성값들을 계산을 한다.
	//
	else if (type == EDamageType::DAMAGE_TYPE_NORMAL || type == EDamageType::DAMAGE_TYPE_NORMAL_RANGE)
	{
		if (type == EDamageType::DAMAGE_TYPE_NORMAL)
		{
			// 근접 평타일 경우 막을 수 있음
			if (GetPoint(POINT_BLOCK) && number(1, 100) <= GetPoint(POINT_BLOCK))
			{
				if (test_server)
				{
					pAttacker->ChatPacket(CHAT_TYPE_INFO, LC_STRING("%s 블럭! (%d%%)", GetName(), GetPoint(POINT_BLOCK)));
					ChatPacket(CHAT_TYPE_INFO, LC_STRING("%s 블럭! (%d%%)", GetName(), GetPoint(POINT_BLOCK)));
				}

				SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
				return false;
			}

			if (GetPoint(POINT_NORMAL_DAMAGE_GUARD) && number(1, 100) <= GetPoint(POINT_NORMAL_DAMAGE_GUARD))
			{
				SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
				return false;
			}
		}
		else if (type == EDamageType::DAMAGE_TYPE_NORMAL_RANGE)
		{
			// 원거리 평타의 경우 피할 수 있음
			if (GetPoint(POINT_DODGE) && number(1, 100) <= GetPoint(POINT_DODGE))
			{
				if (test_server)
				{
					pAttacker->ChatPacket(CHAT_TYPE_INFO, LC_STRING("%s 회피! (%d%%)", GetName(), GetPoint(POINT_DODGE)));
					ChatPacket(CHAT_TYPE_INFO, LC_STRING("%s 회피! (%d%%)", GetName(), GetPoint(POINT_DODGE)));
				}

				SendDamagePacket(pAttacker, 0, DAMAGE_DODGE);
				return false;
			}
		}

		if (IsAffectFlag(AFF_JEONGWIHON))
			dam = (int)(dam * (100 + GetSkillPower(SKILL_JEONGWI) * 25 / 100) / 100);

		if (IsAffectFlag(AFF_TERROR))
			dam = (int)(dam * (95 - GetSkillPower(SKILL_TERROR) / 5) / 100);

		if (IsAffectFlag(AFF_HOSIN))
			dam = dam * (100 - GetPoint(POINT_RESIST_NORMAL_DAMAGE)) / 100;

		//
		// 공격자 속성 적용
		//
		if (pAttacker)
		{
			if (type == EDamageType::DAMAGE_TYPE_NORMAL)
			{
				// 반사
				if (GetPoint(POINT_REFLECT_MELEE))
				{
					int reflectDamage = dam * GetPoint(POINT_REFLECT_MELEE) / 100;

					// NOTE: 공격자가 IMMUNE_REFLECT 속성을 갖고있다면 반사를 안 하는 게 
					// 아니라 1/3 데미지로 고정해서 들어가도록 기획에서 요청.
					if (pAttacker->IsImmune(IMMUNE_REFLECT))
						reflectDamage = int(reflectDamage / 3.0f + 0.5f);

					if (reflectDamage > 0)
						pAttacker->Damage(this, reflectDamage, DAMAGE_TYPE_SPECIAL);
				}
			}

			// 크리티컬
			int iCriticalPct = pAttacker->GetPoint(POINT_CRITICAL_PCT);

			if (!IsPC())
				iCriticalPct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);

			if (iCriticalPct)
			{
				//크리티컬 저항 값 적용.
				iCriticalPct -= GetPoint(POINT_RESIST_CRITICAL);

				if (number(1, 100) <= iCriticalPct)
				{
					IsCritical = true;
					dam *= 2;
					EffectPacket(SE_CRITICAL);
				}
			}

			// 관통공격
			int iPenetratePct = pAttacker->GetPoint(POINT_PENETRATE_PCT);

			if (!IsPC())
				iPenetratePct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);

			{
				CSkillProto* pkSk = CSkillManager::instance().Get(SKILL_RESIST_PENETRATE);

				if (NULL != pkSk)
				{
					pkSk->SetPointVar("k", 1.0f * GetSkillPower(SKILL_RESIST_PENETRATE) / 100.0f);

					iPenetratePct -= static_cast<int>(pkSk->kPointPoly.Eval());
				}
			}

			if (iPenetratePct)
			{
				// 관통타격 저항 값 적용.
				iPenetratePct -= GetPoint(POINT_RESIST_PENETRATE);

				if (number(1, 100) <= iPenetratePct)
				{
					IsPenetrate = true;

					if (test_server)
						ChatPacket(CHAT_TYPE_INFO, LC_STRING("관통 추가 데미지 %d", GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100));

					dam += GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100;
					if (iPenetratePct > 100)
						dam += dam * (iPenetratePct - 100) / 200;

					EffectPacket(SE_PENETRATE);
				}
			}

			// HP 스틸
			if (pAttacker->GetPoint(POINT_STEAL_HP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iHP = MIN(dam, MAX(0, iCurHP)) * pAttacker->GetPoint(POINT_STEAL_HP) / 100;

					if (iHP > 0 && GetHP() >= iHP)
					{
						CreateFly(FLY_HP_SMALL, pAttacker);
						pAttacker->PointChange(POINT_HP, iHP);
						PointChange(POINT_HP, -iHP);
					}
				}
			}

			// SP 스틸
			if (pAttacker->GetPoint(POINT_STEAL_SP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iCur;

					if (IsPC())
						iCur = iCurSP;
					else
						iCur = iCurHP;

					int iSP = MIN(dam, MAX(0, iCur)) * pAttacker->GetPoint(POINT_STEAL_SP) / 100;

					if (iSP > 0 && iCur >= iSP)
					{
						CreateFly(FLY_SP_SMALL, pAttacker);
						pAttacker->PointChange(POINT_SP, iSP);

						if (IsPC())
							PointChange(POINT_SP, -iSP);
					}
				}
			}

			// 돈 스틸
			if (pAttacker->GetPoint(POINT_STEAL_GOLD))
			{
				if (number(1, 100) <= pAttacker->GetPoint(POINT_STEAL_GOLD))
				{
					long llAmount = number(1, GetLevel());
					pAttacker->PointChange(POINT_GOLD, llAmount);
					DBManager::instance().SendMoneyLog(MONEY_LOG_MISC, 1, llAmount);
				}
			}

			// 칠 때마다 HP회복
			if (pAttacker->GetPoint(POINT_HIT_HP_RECOVERY) && number(0, 4) > 0) // 80% 확률
			{
				int i = ((iCurHP >= 0) ? MIN(dam, iCurHP) : dam) * pAttacker->GetPoint(POINT_HIT_HP_RECOVERY) / 100;

				if (i)
				{
					CreateFly(FLY_HP_SMALL, pAttacker);
					pAttacker->PointChange(POINT_HP, i);
				}
			}

			// 칠 때마다 SP회복
			if (pAttacker->GetPoint(POINT_HIT_SP_RECOVERY) && number(0, 4) > 0) // 80% 확률
			{
				int i = ((iCurHP >= 0) ? MIN(dam, iCurHP) : dam) * pAttacker->GetPoint(POINT_HIT_SP_RECOVERY) / 100;

				if (i)
				{
					CreateFly(FLY_SP_SMALL, pAttacker);
					pAttacker->PointChange(POINT_SP, i);
				}
			}

			// 상대방의 마나를 없앤다.
			if (pAttacker->GetPoint(POINT_MANA_BURN_PCT))
			{
				if (number(1, 100) <= pAttacker->GetPoint(POINT_MANA_BURN_PCT))
					PointChange(POINT_SP, -50);
			}
		}
	}

	//
	// 평타 또는 스킬로 인한 보너스 피해/방어 계산
	//
	switch (type)
	{
		case EDamageType::DAMAGE_TYPE_NORMAL:
		case EDamageType::DAMAGE_TYPE_NORMAL_RANGE:
		{
			if (pAttacker)
			{
				if (pAttacker->GetPoint(POINT_NORMAL_HIT_DAMAGE_BONUS))
					dam = dam * (100 + pAttacker->GetPoint(POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;

#if defined(__SOUL_SYSTEM__) && !defined(__SOUL_SYSTEM_CALC_FINAL_DAMAGE__)
				if (pAttacker->FindAffect(AFFECT_SOUL, AFF_SOUL_RED))
					dam = dam * (100 + pAttacker->GetSoulDamage(RED_SOUL)) / 100;
#endif
			}

			dam = dam * (100 - MIN(99, GetPoint(POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;

			{
				DWORD dwDmgHPRecover = GetPoint(POINT_DAMAGE_HP_RECOVERY);
				if (dwDmgHPRecover != 0)
				{
					int iHPRecover = (float(dwDmgHPRecover) / 100) * dam;
					PointChange(POINT_HP, iHPRecover);
					CreateFly(FLY_HP_SMALL, this);
				}

				DWORD dwDmgSPRecover = GetPoint(POINT_DAMAGE_SP_RECOVERY);
				if (dwDmgSPRecover != 0)
				{
					int iSPRecover = (float(dwDmgSPRecover) / 100) * dam;
					PointChange(POINT_SP, iSPRecover);
					CreateFly(FLY_SP_SMALL, this);
				}
			}
		}
		break;

		case EDamageType::DAMAGE_TYPE_MELEE:
		case EDamageType::DAMAGE_TYPE_RANGE:
		case EDamageType::DAMAGE_TYPE_FIRE:
		case EDamageType::DAMAGE_TYPE_ICE:
		case EDamageType::DAMAGE_TYPE_ELEC:
		case EDamageType::DAMAGE_TYPE_MAGIC:
		{
			if (pAttacker)
			{
				if (pAttacker->GetPoint(POINT_SKILL_DAMAGE_BONUS))
					dam = dam * (100 + pAttacker->GetPoint(POINT_SKILL_DAMAGE_BONUS)) / 100;

#if defined(__SOUL_SYSTEM__) && !defined(__SOUL_SYSTEM_CALC_FINAL_DAMAGE__)
				if (pAttacker->FindAffect(AFFECT_SOUL, AFF_SOUL_BLUE))
					dam = dam * (100 + pAttacker->GetSoulDamage(BLUE_SOUL)) / 100;
#endif

#if defined(__ATTR_6TH_7TH__)
				if (pAttacker->GetPoint(POINT_SKILL_DAMAGE_BONUS_BOSS_OR_MORE))
					if (IsMonster() && GetMobRank() >= MOB_RANK_BOSS)
						dam = dam * (100 + pAttacker->GetPoint(POINT_SKILL_DAMAGE_BONUS_BOSS_OR_MORE)) / 100;

				if (GetPoint(POINT_SKILL_DEFEND_BONUS_BOSS_OR_MORE))
					if (pAttacker->IsMonster() && pAttacker->GetMobRank() >= MOB_RANK_BOSS)
						dam = dam * (100 - MIN(99, GetPoint(POINT_SKILL_DEFEND_BONUS_BOSS_OR_MORE))) / 100;
#endif
			}

			dam = dam * (100 - MIN(99, GetPoint(POINT_SKILL_DEFEND_BONUS))) / 100;
		}
		break;

		default:
			break;
	}

	//
	// 마나쉴드(흑신수호)
	//
	if (IsAffectFlag(AFF_MANASHIELD))
	{
		// POINT_MANASHIELD 는 작아질수록 좋다
		int iDamageSPPart = dam / 3;
		int iDamageToSP = iDamageSPPart * GetPoint(POINT_MANASHIELD) / 100;
		int iSP = GetSP();

		// SP가 있으면 무조건 데미지 절반 감소
		if (iDamageToSP <= iSP)
		{
			PointChange(POINT_SP, -iDamageToSP);
			dam -= iDamageSPPart;
		}
		else
		{
			// 정신력이 모자라서 피가 더 깍여야할떄
			PointChange(POINT_SP, -GetSP());
			dam -= iSP * 100 / MAX(GetPoint(POINT_MANASHIELD), 1);
		}
	}

	//
	// 전체 방어력 상승 (몰 아이템)
	//
	if (GetPoint(POINT_MALL_DEFBONUS) > 0)
	{
		int dec_dam = MIN(200, dam * GetPoint(POINT_MALL_DEFBONUS) / 100);
		dam -= dec_dam;
	}

	if (pAttacker)
	{
		//
		// 전체 공격력 상승 (몰 아이템)
		//
		if (pAttacker->GetPoint(POINT_MALL_ATTBONUS) > 0)
		{
			int add_dam = MIN(300, dam * pAttacker->GetLimitPoint(POINT_MALL_ATTBONUS) / 100);
			dam += add_dam;
		}

		//
		// 제국으로 인한 보너스 (한국 올드 버전만 적용)
		//
		int iEmpire = GetEmpire();
		long lMapIndex = GetMapIndex();
		int iMapEmpire = SECTREE_MANAGER::instance().GetEmpireFromMapIndex(lMapIndex);

		// Damage increased by 10% for pc empire
		if (iEmpire && iMapEmpire && iEmpire != iMapEmpire)
			dam += (dam * 10) / 100;

		if (pAttacker->IsPC())
		{
			iEmpire = pAttacker->GetEmpire();
			lMapIndex = pAttacker->GetMapIndex();
			iMapEmpire = SECTREE_MANAGER::instance().GetEmpireFromMapIndex(lMapIndex);

			// Damage reduced by 10% for other empires
			/*
			if (iEmpire && iMapEmpire && iEmpire != iMapEmpire)
			{
				int percent = 10;

				if (184 <= lMapIndex && lMapIndex <= 189)
					percent = 9;
				else
					percent = 9;

				dam = dam * percent / 10;
			}
			*/

			if (!IsPC() && GetMonsterDrainSPPoint())
			{
				int iDrain = GetMonsterDrainSPPoint();

				if (iDrain <= pAttacker->GetSP())
					pAttacker->PointChange(POINT_SP, -iDrain);
				else
				{
					int iSP = pAttacker->GetSP();
					pAttacker->PointChange(POINT_SP, -iSP);
				}
			}
		}
		else if (pAttacker->IsGuardNPC())
		{
			SET_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_NO_REWARD);
			Stun();
			return true;
		}

		//
		// 군주의 금강권 & 사자후 
		//
		if (pAttacker->IsPC() && CMonarch::instance().IsPowerUp(pAttacker->GetEmpire()))
		{
			// 10% 피해 증가
			dam += dam / 10;
		}

		if (IsPC() && CMonarch::instance().IsDefenceUp(GetEmpire()))
		{
			// 10% 피해 감소
			dam -= dam / 10;
		}

#if defined(__SOUL_SYSTEM__) && defined(__SOUL_SYSTEM_CALC_FINAL_DAMAGE__)
		switch (type)
		{
			case EDamageType::DAMAGE_TYPE_NORMAL:
			case EDamageType::DAMAGE_TYPE_NORMAL_RANGE:
			{
				if (pAttacker->FindAffect(AFFECT_SOUL, AFF_SOUL_RED))
				{
					if (!pAttacker->IsPolymorphed() && !IsPC())
					{
						float fDamMultiplier = static_cast<float>(100 * pAttacker->GetSoulDamage(RED_SOUL)) / 1000;
						if (fDamMultiplier != 0)
							dam = (int)(dam * fDamMultiplier);
					}
				}
			}
			break;

			case EDamageType::DAMAGE_TYPE_MELEE:
			case EDamageType::DAMAGE_TYPE_RANGE:
			case EDamageType::DAMAGE_TYPE_FIRE:
			case EDamageType::DAMAGE_TYPE_ICE:
			case EDamageType::DAMAGE_TYPE_ELEC:
			case EDamageType::DAMAGE_TYPE_MAGIC:
			{
				if (pAttacker->FindAffect(AFFECT_SOUL, AFF_SOUL_BLUE))
				{
					if (!pAttacker->IsPolymorphed() && !IsPC())
					{
						float fDamMultiplier = static_cast<float>(100 * pAttacker->GetSoulDamage(BLUE_SOUL)) / 1000;
						if (fDamMultiplier != 0)
							dam = (int)(dam * fDamMultiplier);
					}
				}
			}
			break;
		}
#endif
	}
	//puAttr.Pop();

	if (!GetSectree() || GetSectree()->IsAttr(GetX(), GetY(), ATTR_BANPK))
		return false;

	if (!IsPC())
	{
		if (m_pkParty && m_pkParty->GetLeader())
			m_pkParty->GetLeader()->SetLastAttacked(get_dword_time());
		else
			SetLastAttacked(get_dword_time());

		// 몬스터 대사 : 맞을 때
		MonsterChat(MONSTER_CHAT_ATTACKED);
	}

	if (IsStun())
	{
		Dead(pAttacker);
		return true;
	}

	if (IsDead())
		return true;

	// 독 공격으로 죽지 않도록 함.
	if (type == DAMAGE_TYPE_POISON)
	{
		if (GetHP() - dam <= 0)
		{
			dam = GetHP() - 1;
		}
	}

	else if (type == DAMAGE_TYPE_BLEEDING)
	{
		if (GetHP() - dam <= 0)
		{
			dam = GetHP();
		}
	}

	// ------------------------
	// 독일 프리미엄 모드 
	// -----------------------
	if (pAttacker && pAttacker->IsPC())
	{
		int iDmgPct = CHARACTER_MANAGER::instance().GetUserDamageRate(pAttacker);
		dam = dam * iDmgPct / 100;
	}

	// STONE SKIN : 피해 반으로 감소
	if (IsMonster() && IsStoneSkinner())
	{
		if (GetHPPct() < GetMobTable().bStoneSkinPoint)
			dam /= 2;
	}

	//PROF_UNIT puRest1("Rest1");
	if (pAttacker)
	{
		// DEATH BLOW : 확률 적으로 4배 피해 (!? 현재 이벤트나 공성전용 몬스터만 사용함)
		if (pAttacker->IsMonster() && pAttacker->IsDeathBlower())
		{
			if (pAttacker->IsDeathBlow())
			{
				int rate = 2 * MINMAX(1, 51 - pAttacker->GetHPPct(), 50);
				if (GetPoint(POINT_BLOCK) > 0 && number(1, 100) <= GetPoint(POINT_BLOCK))
				{
					rate -= MINMAX(1, GetPoint(POINT_BLOCK), 75);
				}

				IsDeathBlow = true;
				dam *= (int)(1.5 + 1.5 * rate / 100.0);
			}
		}

		// MOUNT_FALL
		if (pAttacker->IsMonster() && pAttacker->IsFaller())
		{
			unsigned int iUnMountChance = 30 - GetPoint(POINT_RESIST_MOUNT_FALL);
			if (number(1, 100) <= iUnMountChance)
			{
				if (!FindAffect(AFFECT_MOUNT_FALL))
					AddAffect(AFFECT_MOUNT_FALL, POINT_NONE, 0, AFF_NONE, 5, 0, true);
			}
		}
		// END_OF_MOUNT_FALL

		dam = BlueDragon_Damage(this, pAttacker, dam);
	}

	BYTE damageFlag = 0;

	if (type == DAMAGE_TYPE_POISON)
		damageFlag = DAMAGE_POISON;
	else if (type == DAMAGE_TYPE_BLEEDING)
		damageFlag = DAMAGE_BLEEDING;
	else
		damageFlag = DAMAGE_NORMAL;

	if (IsCritical == true)
		damageFlag |= DAMAGE_CRITICAL;

	if (IsPenetrate == true)
		damageFlag |= DAMAGE_PENETRATE;

	// 최종 데미지 보정
	float damMul = this->GetDamMul();
	float tempDam = dam;
	dam = tempDam * damMul + 0.5f;

	//if (pAttacker)
	SendDamagePacket(pAttacker, dam, damageFlag);

	if (test_server && g_bTestMobLog)
	{
		int iPercent = 0;
		if (GetMaxHP() >= 0)
			iPercent = (GetHP() * 100) / GetMaxHP();

		if (pAttacker)
		{
			pAttacker->ChatPacket(CHAT_TYPE_INFO, "-> %s, DAM %d HP %d(%d%%) %s%s",
				GetName(),
				dam,
				GetHP(),
				iPercent,
				IsCritical ? "crit " : "",
				IsPenetrate ? "pene " : "",
				IsDeathBlow ? "deathblow " : "");
		}

		ChatPacket(CHAT_TYPE_PARTY, "<- %s, DAM %d HP %d(%d%%) %s%s",
			pAttacker ? pAttacker->GetName() : 0,
			dam,
			GetHP(),
			iPercent,
			IsCritical ? "crit " : "",
			IsPenetrate ? "pene " : "",
			IsDeathBlow ? "deathblow " : "");
	}

	if (pAttacker)
	{
		if (m_bDetailLog)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_STRING("%s[%d]가 공격 위치: %d %d", pAttacker->GetName(), (DWORD)pAttacker->GetVID(), pAttacker->GetX(), pAttacker->GetY()));
		}
	}

	//
	// !!!!!!!!! 실제 HP를 줄이는 부분 !!!!!!!!!
	//
	if (!cannot_dead)
	{
		dam = (GetHP() - dam <= 0) ? GetHP() : dam;

#if defined(__QUEST_EVENT_DAMAGE__)
		if (pAttacker && pAttacker->IsPC())
		{
			pAttacker->SetQuestNPCID(GetVID());
			quest::CQuestManager::instance().Damage(pAttacker->GetPlayerID(), GetRaceNum());
		}
#endif

		PointChange(POINT_HP, -dam, false);

#if defined(__GUILD_DRAGONLAIR_SYSTEM__)
		if (pAttacker && CGuildDragonLairManager::Instance().IsRedDragonLair(pAttacker->GetMapIndex()))
		{
			if (GetHP() == 0 && CGuildDragonLairManager::Instance().IsStone(GetRaceNum()))
			{
				PointChange(POINT_HP, 1, false);
			}
		}
#endif
	}

	//puRest1.Pop();

	//PROF_UNIT puRest2("Rest2");
	if (pAttacker && dam > 0 && IsNPC())
	{
		//PROF_UNIT puRest20("Rest20");
		TDamageMap::iterator it = m_map_kDamage.find(pAttacker->GetVID());

		if (it == m_map_kDamage.end())
		{
			m_map_kDamage.insert(TDamageMap::value_type(pAttacker->GetVID(), TBattleInfo(dam, 0)));
			it = m_map_kDamage.find(pAttacker->GetVID());
		}
		else
		{
			it->second.iTotalDamage += dam;
		}
		//puRest20.Pop();

		//PROF_UNIT puRest21("Rest21");
		StartRecoveryEvent(); // 몬스터는 데미지를 입으면 회복을 시작한다.
		//puRest21.Pop();

		//PROF_UNIT puRest22("Rest22");
		UpdateAggrPointEx(pAttacker, type, dam, it->second);
		//puRest22.Pop();

		pAttacker->SetAccumulatedDamage(dam);
	}
	//puRest2.Pop();

	//PROF_UNIT puRest3("Rest3");
	if (GetHP() <= 0)
	{
		if (IsMonster())
			Dead(pAttacker);
		else
			Stun(true);

		if (pAttacker && !pAttacker->IsNPC())
			m_dwKillerPID = pAttacker->GetPlayerID();
		else
			m_dwKillerPID = 0;
	}

	return false;
}

void CHARACTER::DistributeHP(LPCHARACTER pkKiller)
{
	if (pkKiller->GetDungeon()) // 던젼내에선 만두가나오지않는다
		return;
}

#if defined(__CONQUEROR_LEVEL__)
static void GiveExp(LPCHARACTER from, LPCHARACTER to, int iExp, int iConquerorExp)
#else
static void GiveExp(LPCHARACTER from, LPCHARACTER to, int iExp)
#endif
{
	{
		LPITEM pkItem;

		pkItem = to->GetWear(WEAR_UNIQUE1);

		if (pkItem && pkItem->GetVnum() == UNIQUE_ITEM_ANTI_EXP)
		{
			if (test_server)
				to->ChatPacket(CHAT_TYPE_INFO, "exp blocked by ring %d", UNIQUE_ITEM_ANTI_EXP);
			return;
		}

		pkItem = to->GetWear(WEAR_UNIQUE2);
		if (pkItem && pkItem->GetVnum() == UNIQUE_ITEM_ANTI_EXP)
		{
			if (test_server)
				to->ChatPacket(CHAT_TYPE_INFO, "exp blocked by ring %d", UNIQUE_ITEM_ANTI_EXP);
			return;
		}
	}

	// 레벨차 경험치 가감비율
	iExp = CALCULATE_VALUE_LVDELTA(to->GetLevel(), from->GetLevel(), iExp);
	const int iBaseExp = iExp;

#if defined(__CONQUEROR_LEVEL__)
	iConquerorExp = CALCULATE_VALUE_LVDELTA(to->GetLevel(), from->GetLevel(), iConquerorExp);
	const int iBaseConquerorExp = iConquerorExp;
#endif

	int iRate = 100;

	// 점술, 회사 경험치 이벤트 적용
	iRate += CPrivManager::instance().GetPriv(to, PRIV_EXP_PCT);

	// 게임내 기본 제공되는 경험치 보너스
	{
		// 노동절 메달
		if (to->IsEquipUniqueItem(UNIQUE_ITEM_LARBOR_MEDAL))
			iRate += 20;

		// Devil tower experience bonus
		if (to->GetMapIndex() >= 660000 && to->GetMapIndex() < 670000)
			iRate += 20; // 1.2배 (20%)

		// 아이템 경험치 두배 속성
		if (to->GetPoint(POINT_EXP_DOUBLE_BONUS))
			if (number(1, 100) <= to->GetPoint(POINT_EXP_DOUBLE_BONUS))
				iRate += 30; // 1.3배 (30%)

		// 경험의 반지 (2시간짜리)
		// NOTE: Experience rings now work like special rings.
		/*
		if (to->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_EXP))
		{
			iRate += 50; // (50%)
			if (test_server)
				to->ChatPacket(CHAT_TYPE_INFO, "exp bonus + 50% (using ring)");
		}
		*/

		// 아이템 몰 판매 경험치 보너스
		if (to->GetPremiumRemainSeconds(PREMIUM_EXP) > 0) // 아이템 몰: 경험치 결제
			iRate += 50;

		iRate += to->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_EXP_BONUS); // 결혼 보너스
		iRate += to->GetPoint(POINT_RAMADAN_CANDY_BONUS_EXP);
		iRate += to->GetPoint(POINT_MALL_EXPBONUS);
	}

	if (test_server)
	{
		sys_log(0, "Bonus Exp : Ramadan Candy: %d MallExp: %d PointExp: %d",
			to->GetPoint(POINT_RAMADAN_CANDY_BONUS_EXP),
			to->GetPoint(POINT_MALL_EXPBONUS),
			to->GetPoint(POINT_EXP)
		);
	}

	// 기획측 조정값 2005.04.21 현재 85%
	iRate *= CHARACTER_MANAGER::instance().GetMobExpRate(to) / 100;

	iExp *= (iRate / 100.0);
#if defined(__CONQUEROR_LEVEL__)
	iConquerorExp *= (iRate / 100.0);
#endif

	// 경험치 한번 획득량 제한
	//iExp = MIN(to->GetNextExp() / 10, iExp);

	//iExp = AdjustExpByLevel(to, iExp);

	if (iBaseExp)
	{
		to->PointChange(POINT_EXP, iExp, true);
		from->CreateFly(FLY_EXP, to);
	}

#if defined(__CONQUEROR_LEVEL__)
	if (iBaseConquerorExp && to->GetConquerorLevel() > 0)
	{
		to->PointChange(POINT_CONQUEROR_EXP, iConquerorExp, true);
		from->CreateFly(FLY_CONQUEROR_EXP, to);
	}
#endif

	{
		LPCHARACTER you = to->GetMarryPartner();
		// 부부가 서로 파티중이면 금슬이 오른다
		if (you)
		{
			// 1억이 100%
			DWORD dwUpdatePoint = 2000 * iExp / to->GetLevel() / to->GetLevel() / 3;

			if (to->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0 ||
				you->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0)
				dwUpdatePoint *= 3;

			marriage::TMarriage* pMarriage = marriage::CManager::Instance().Get(to->GetPlayerID());

			// DIVORCE_NULL_BUG_FIX
			if (pMarriage && pMarriage->IsNear())
				pMarriage->Update(dwUpdatePoint);
			// END_OF_DIVORCE_NULL_BUG_FIX
		}
	}
}

namespace NPartyExpDistribute
{
	struct FPartyTotaler
	{
		int total;
		int member_count;
		int x, y;

		FPartyTotaler(LPCHARACTER center)
			: total(0), member_count(0), x(center->GetX()), y(center->GetY())
		{};

		void operator () (LPCHARACTER ch)
		{
			if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
			{
				total += party_exp_distribute_table[ch->GetLevel()];
				++member_count;
			}
		}
	};

	struct FPartyDistributor
	{
		int total;
		LPCHARACTER c;
		int x, y;
		DWORD _iExp;
#if defined(__CONQUEROR_LEVEL__)
		DWORD _iConquerorExp;
#endif
		int m_iMode;
		int m_iMemberCount;

#if defined(__CONQUEROR_LEVEL__)
		FPartyDistributor(LPCHARACTER center, int member_count, int total, DWORD iExp, DWORD iConquerorExp, int iMode)
			: total(total), c(center), x(center->GetX()), y(center->GetY()), _iExp(iExp), _iConquerorExp(iConquerorExp), m_iMode(iMode), m_iMemberCount(member_count)
#else
		FPartyDistributor(LPCHARACTER center, int member_count, int total, DWORD iExp, int iMode)
			: total(total), c(center), x(center->GetX()), y(center->GetY()), _iExp(iExp), m_iMode(iMode), m_iMemberCount(member_count)
#endif
		{
			if (m_iMemberCount == 0)
				m_iMemberCount = 1;
		};

		void operator () (LPCHARACTER ch)
		{
			if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
			{
				DWORD iExp = 0;
#if defined(__CONQUEROR_LEVEL__)
				DWORD iConquerorExp = 0;
#endif

				switch (m_iMode)
				{
					case PARTY_EXP_DISTRIBUTION_NON_PARITY:
						iExp = (DWORD)(_iExp * (float)party_exp_distribute_table[ch->GetLevel()] / total);
#if defined(__CONQUEROR_LEVEL__)
						iConquerorExp = (DWORD)(_iConquerorExp * (float)party_exp_distribute_table[ch->GetLevel()] / total);
#endif
						break;

					case PARTY_EXP_DISTRIBUTION_PARITY:
						iExp = _iExp / m_iMemberCount;
#if defined(__CONQUEROR_LEVEL__)
						iConquerorExp = _iConquerorExp / m_iMemberCount;
#endif
						break;

					default:
						sys_err("Unknown party exp distribution mode %d", m_iMode);
						return;
				}

#if defined(__CONQUEROR_LEVEL__)
				GiveExp(c, ch, iExp, iConquerorExp);
#else
				GiveExp(c, ch, iExp);
#endif
			}
		}
	};
}

typedef struct SDamageInfo
{
	int iDam;
	LPCHARACTER pAttacker;
	LPPARTY pParty;

	void Clear()
	{
		pAttacker = NULL;
		pParty = NULL;
	}

#if defined(__CONQUEROR_LEVEL__)
	inline void Distribute(LPCHARACTER ch, int iExp, int iConquerorExp)
#else
	inline void Distribute(LPCHARACTER ch, int iExp)
#endif
	{
		if (pAttacker)
		{
#if defined(__CONQUEROR_LEVEL__)
			GiveExp(ch, pAttacker, iExp, iConquerorExp);
#else
			GiveExp(ch, pAttacker, iExp);
#endif
		}
		else if (pParty)
		{
			NPartyExpDistribute::FPartyTotaler f(ch);
			pParty->ForEachOnMapMember(f, ch->GetMapIndex());

			if (pParty->IsPositionNearLeader(ch))
				iExp = iExp * (100 + pParty->GetExpBonusPercent()) / 100;

			if (test_server)
			{
				if (quest::CQuestManager::instance().GetEventFlag("exp_bonus_log") && pParty->GetExpBonusPercent())
					pParty->ChatPacketToAllMember(CHAT_TYPE_INFO, "exp party bonus %d%%", pParty->GetExpBonusPercent());
			}

			// 경험치 몰아주기 (파티가 획득한 경험치를 5% 빼서 먼저 줌)
			if (pParty->GetExpCentralizeCharacter())
			{
				LPCHARACTER tch = pParty->GetExpCentralizeCharacter();

				if (DISTANCE_APPROX(ch->GetX() - tch->GetX(), ch->GetY() - tch->GetY()) <= PARTY_DEFAULT_RANGE)
				{
					int iExpCenteralize = (int)(iExp * 0.05f);
					iExp -= iExpCenteralize;

#if defined(__CONQUEROR_LEVEL__)
					int iConquerorExpCenteralize = (int)(iExp * 0.05f);
					iConquerorExp -= iConquerorExpCenteralize;

					GiveExp(ch, pParty->GetExpCentralizeCharacter(), iExpCenteralize, iConquerorExpCenteralize);
#else
					GiveExp(ch, pParty->GetExpCentralizeCharacter(), iExpCenteralize);
#endif
				}
			}

#if defined(__CONQUEROR_LEVEL__)
			NPartyExpDistribute::FPartyDistributor fDist(ch, f.member_count, f.total, iExp, iConquerorExp, pParty->GetExpDistributionMode());
#else
			NPartyExpDistribute::FPartyDistributor fDist(ch, f.member_count, f.total, iExp, pParty->GetExpDistributionMode());
#endif
			pParty->ForEachOnMapMember(fDist, ch->GetMapIndex());
		}
	}
} TDamageInfo;

LPCHARACTER CHARACTER::DistributeExp()
{
#if defined(__CONQUEROR_LEVEL__)
	int iExpToDistribute = GetExp();
	if (iExpToDistribute <= 0)
		iExpToDistribute = 0;

	int iConquerorExpToDistribute = GetConquerorExp();
	if (iConquerorExpToDistribute <= 0)
		iConquerorExpToDistribute = 0;
#else
	int iExpToDistribute = GetExp();
	if (iExpToDistribute <= 0)
		return NULL;
#endif

	int iTotalDam = 0;
	LPCHARACTER pkChrMostAttacked = NULL;
	int iMostDam = 0;

	typedef std::vector<TDamageInfo> TDamageInfoTable;
	TDamageInfoTable damage_info_table;
	std::map<LPPARTY, TDamageInfo> map_party_damage;

	damage_info_table.reserve(m_map_kDamage.size());

	TDamageMap::iterator it = m_map_kDamage.begin();

	// 일단 주위에 없는 사람을 걸러 낸다. (50m)
	while (it != m_map_kDamage.end())
	{
		const VID& c_VID = it->first;
		int iDam = it->second.iTotalDamage;

		++it;

		LPCHARACTER pAttacker = CHARACTER_MANAGER::instance().Find(c_VID);

		// NPC가 때리기도 하나? -.-;
		if (!pAttacker || pAttacker->IsNPC() || DISTANCE_APPROX(GetX() - pAttacker->GetX(), GetY() - pAttacker->GetY()) > 5000)
			continue;

		iTotalDam += iDam;
		if (!pkChrMostAttacked || iDam > iMostDam)
		{
			pkChrMostAttacked = pAttacker;
			iMostDam = iDam;
		}

		if (pAttacker->GetParty())
		{
			std::map<LPPARTY, TDamageInfo>::iterator it = map_party_damage.find(pAttacker->GetParty());
			if (it == map_party_damage.end())
			{
				TDamageInfo di;
				di.iDam = iDam;
				di.pAttacker = NULL;
				di.pParty = pAttacker->GetParty();
				map_party_damage.insert(std::make_pair(di.pParty, di));
			}
			else
			{
				it->second.iDam += iDam;
			}
		}
		else
		{
			TDamageInfo di;

			di.iDam = iDam;
			di.pAttacker = pAttacker;
			di.pParty = NULL;

			//sys_log(0, "__ pq_damage %s %d", pAttacker->GetName(), iDam);
			//pq_damage.push(di);
			damage_info_table.push_back(di);
		}
	}

	for (std::map<LPPARTY, TDamageInfo>::iterator it = map_party_damage.begin(); it != map_party_damage.end(); ++it)
	{
		damage_info_table.push_back(it->second);
		//sys_log(0, "__ pq_damage_party [%u] %d", it->second.pParty->GetLeaderPID(), it->second.iDam);
	}

	SetExp(0);
#if defined(__CONQUEROR_LEVEL__)
	SetConquerorExp(0);
#endif
	//m_map_kDamage.clear();

	if (iTotalDam == 0) // 데미지 준게 0이면 리턴
		return NULL;

	if (m_pkChrStone)	// 돌이 있을 경우 경험치의 반을 돌에게 넘긴다.
	{
		//sys_log(0, "__ Give half to Stone : %d", iExpToDistribute>>1);
		int iExp = iExpToDistribute >> 1;
		m_pkChrStone->SetExp(m_pkChrStone->GetExp() + iExp);
		iExpToDistribute -= iExp;
	}

	sys_log(1, "%s total exp: %d, damage_info_table.size() == %d, TotalDam %d",
		GetName(), iExpToDistribute, damage_info_table.size(), iTotalDam);
	//sys_log(1, "%s total exp: %d, pq_damage.size() == %d, TotalDam %d",
	//GetName(), iExpToDistribute, pq_damage.size(), iTotalDam);

	if (damage_info_table.empty())
		return NULL;

	// 제일 데미지를 많이 준 사람이 HP 회복을 한다.
	DistributeHP(pkChrMostAttacked); // 만두 시스템

	{
		// 제일 데미지를 많이 준 사람이나 파티가 총 경험치의 20% + 자기가 때린만큼의 경험치를 먹는다.
		TDamageInfoTable::iterator di = damage_info_table.begin();
		{
			TDamageInfoTable::iterator it;

			for (it = damage_info_table.begin(); it != damage_info_table.end(); ++it)
			{
				if (it->iDam > di->iDam)
					di = it;
			}
		}

		int iExp = iExpToDistribute / 5;
		iExpToDistribute -= iExp;

#if defined(__CONQUEROR_LEVEL__)
		int	iConquerorExp = iConquerorExpToDistribute / 5;
		iConquerorExpToDistribute -= iConquerorExp;
#endif

		float fPercent = (float)di->iDam / iTotalDam;

		if (fPercent > 1.0f)
		{
			sys_err("DistributeExp percent over 1.0 (fPercent %f name %s)", fPercent, di->pAttacker->GetName());
			fPercent = 1.0f;
		}

		iExp += (int)(iExpToDistribute * fPercent);
#if defined(__CONQUEROR_LEVEL__)
		iConquerorExp += (int)(iConquerorExpToDistribute * fPercent);
#endif

		//sys_log(0, "%s given exp percent %.1f + 20 dam %d", GetName(), fPercent * 100.0f, di.iDam);

#if defined(__CONQUEROR_LEVEL__)
		di->Distribute(this, iExp, iConquerorExp);
#else
		di->Distribute(this, iExp);
#endif

		// 100% 다 먹었으면 리턴한다.
		if (fPercent == 1.0f)
			return pkChrMostAttacked;

		di->Clear();
	}

	{
		// 남은 80%의 경험치를 분배한다.
		TDamageInfoTable::iterator it;

		for (it = damage_info_table.begin(); it != damage_info_table.end(); ++it)
		{
			TDamageInfo& di = *it;

			float fPercent = (float)di.iDam / iTotalDam;

			if (fPercent > 1.0f)
			{
				sys_err("DistributeExp percent over 1.0 (fPercent %f name %s)", fPercent, di.pAttacker->GetName());
				fPercent = 1.0f;
			}

			//sys_log(0, "%s given exp percent %.1f dam %d", GetName(), fPercent * 100.0f, di.iDam);
#if defined(__CONQUEROR_LEVEL__)
			di.Distribute(this, (int)(iExpToDistribute * fPercent), (int)(iConquerorExpToDistribute * fPercent));
#else
			di.Distribute(this, (int)(iExpToDistribute * fPercent));
#endif
		}
	}

	return pkChrMostAttacked;
}

// 화살 개수를 리턴해 줌
int CHARACTER::GetArrowAndBow(LPITEM* ppkBow, LPITEM* ppkArrow, int iArrowCount/* = 1 */)
{
	LPITEM pkBow;
	if (!(pkBow = GetWear(WEAR_WEAPON)) || pkBow->GetSubType() != WEAPON_BOW)
		return 0;

	LPITEM pkArrow;
#if defined(__QUIVER_SYSTEM__)
	if (!(pkArrow = GetWear(WEAR_ARROW)) || pkArrow->GetType() != ITEM_WEAPON || (pkArrow->GetSubType() != WEAPON_ARROW && pkArrow->GetSubType() != WEAPON_QUIVER))
#else
	if (!(pkArrow = GetWear(WEAR_ARROW)) || pkArrow->GetType() != ITEM_WEAPON || pkArrow->GetSubType() != WEAPON_ARROW)
#endif
	{
		return 0;
	}

#if defined(__QUIVER_SYSTEM__)
	if (pkArrow->GetSubType() == WEAPON_QUIVER)
		iArrowCount = MIN(iArrowCount, pkArrow->GetSocket(0) - time(0));
	else
		iArrowCount = MIN(iArrowCount, pkArrow->GetCount());
#else
	iArrowCount = MIN(iArrowCount, pkArrow->GetCount());
#endif

	* ppkBow = pkBow;
	*ppkArrow = pkArrow;

	return iArrowCount;
}

void CHARACTER::UseArrow(LPITEM pkArrow, DWORD dwArrowCount)
{
#if defined(__QUIVER_SYSTEM__)
	if (pkArrow->GetSubType() == WEAPON_QUIVER)
		return;
#endif

	int iCount = pkArrow->GetCount();
	DWORD dwVnum = pkArrow->GetVnum();
	iCount = iCount - MIN(iCount, dwArrowCount);
	pkArrow->SetCount(iCount);

	if (iCount == 0)
	{
		LPITEM pkNewArrow = FindSpecifyItem(dwVnum);

		sys_log(0, "UseArrow : FindSpecifyItem %u %p", dwVnum, get_pointer(pkNewArrow));

		if (pkNewArrow)
			EquipItem(pkNewArrow);
	}
}

class CFuncShoot
{
public:
	LPCHARACTER m_me;
	BYTE m_bType;
	bool m_bSucceed;

	CFuncShoot(LPCHARACTER ch, BYTE bType) : m_me(ch), m_bType(bType), m_bSucceed(FALSE)
	{
	}

	void operator () (DWORD dwTargetVID)
	{
		if (m_bType > 1)
		{
			if (g_bSkillDisable)
				return;

			m_me->m_SkillUseInfo[m_bType].SetMainTargetVID(dwTargetVID);
			/*
			if (m_bType == SKILL_BIPABU || m_bType == SKILL_KWANKYEOK)
				m_me->m_SkillUseInfo[m_bType].ResetHitCount();
			*/
		}

		LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find(dwTargetVID);

		if (!pkVictim)
			return;

		// 공격 불가
		if (!battle_is_attackable(m_me, pkVictim))
			return;

		if (m_me->IsNPC())
		{
			if (DISTANCE_APPROX(m_me->GetX() - pkVictim->GetX(), m_me->GetY() - pkVictim->GetY()) > 5000)
				return;
		}

		LPITEM pkBow, pkArrow;

		switch (m_bType)
		{
			case 0: // 일반활
			{
				int iDam = 0;

				if (m_me->IsPC())
				{
					if (m_me->GetJob() != JOB_ASSASSIN)
						return;

					if (0 == m_me->GetArrowAndBow(&pkBow, &pkArrow))
						return;

					if (m_me->GetSkillGroup() != 0)
						if (!m_me->IsNPC() && m_me->GetSkillGroup() != 2)
						{
							if (m_me->GetSP() < 5)
								return;

							m_me->PointChange(POINT_SP, -5);
						}

					iDam = CalcArrowDamage(m_me, pkVictim, pkBow, pkArrow);
					m_me->UseArrow(pkArrow, 1);

					// Check speed hack
					DWORD dwCurrentTime = get_dword_time();
					if (IS_SPEED_HACK(m_me, pkVictim, dwCurrentTime))
					{
						iDam = 0;
						return;
					}
				}
				else
					iDam = CalcMeleeDamage(m_me, pkVictim);

				NormalAttackAffect(m_me, pkVictim);

				// 데미지 계산
				iDam = iDam * (100 - pkVictim->GetPoint(POINT_RESIST_BOW)) / 100;

				//sys_log(0, "%s arrow %s dam %d", m_me->GetName(), pkVictim->GetName(), iDam);

				m_me->OnMove(true);
				pkVictim->OnMove();

				if (pkVictim->CanBeginFight())
					pkVictim->BeginFight(m_me);

				pkVictim->Damage(m_me, iDam, EDamageType::DAMAGE_TYPE_NORMAL_RANGE);
				// 타격치 계산부 끝
			}
			break;

			case 1: // 일반 마법
			{
				int iDam;

				if (m_me->IsPC())
					return;

				iDam = CalcMagicDamage(m_me, pkVictim);

				NormalAttackAffect(m_me, pkVictim);

				// 데미지 계산
#if defined(__MAGIC_REDUCTION__)
				const int c_iResMagic = MINMAX(0, pkVictim->GetPoint(POINT_RESIST_MAGIC), 100);
				const int c_iResMagicReduction = MINMAX(0, (m_me->GetJob() == JOB_SURA) ? m_me->GetPoint(POINT_RESIST_MAGIC_REDUCTION) / 2 : m_me->GetPoint(POINT_RESIST_MAGIC_REDUCTION), 50);
				const int c_iTotalMagicRes = MINMAX(0, c_iResMagic - c_iResMagicReduction, 100);
				iDam = iDam * (100 - c_iTotalMagicRes) / 100;
#else
				iDam = iDam * (100 - pkVictim->GetPoint(POINT_RESIST_MAGIC)) / 100;
#endif

				//sys_log(0, "%s arrow %s dam %d", m_me->GetName(), pkVictim->GetName(), iDam);

				m_me->OnMove(true);
				pkVictim->OnMove();

				if (pkVictim->CanBeginFight())
					pkVictim->BeginFight(m_me);

				pkVictim->Damage(m_me, iDam, DAMAGE_TYPE_MAGIC);
				// 타격치 계산부 끝
			}
			break;

			case SKILL_YEONSA: // 연사
			{
				//int iUseArrow = 2 + (m_me->GetSkillPower(SKILL_YEONSA) *6/100);
				int iUseArrow = 1;

				// 토탈만 계산하는경우
				{
					if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
					{
						m_me->OnMove(true);
						pkVictim->OnMove();

						if (pkVictim->CanBeginFight())
							pkVictim->BeginFight(m_me);

						m_me->ComputeSkill(m_bType, pkVictim);
						m_me->UseArrow(pkArrow, iUseArrow);

						if (pkVictim->IsDead())
							break;
					}
					else
						break;
				}
			}
			break;

			case SKILL_KWANKYEOK:
			{
				int iUseArrow = 1;

				if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
				{
					m_me->OnMove(true);
					pkVictim->OnMove();

					if (pkVictim->CanBeginFight())
						pkVictim->BeginFight(m_me);

					sys_log(0, "%s kwankeyok %s", m_me->GetName(), pkVictim->GetName());
					m_me->ComputeSkill(m_bType, pkVictim);
					m_me->UseArrow(pkArrow, iUseArrow);
				}
			}
			break;

			case SKILL_GIGUNG:
#if defined(__9TH_SKILL__)
			case SKILL_PUNGLOEPO:
#endif
			{
				int iUseArrow = 1;
				if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
				{
					m_me->OnMove(true);
					pkVictim->OnMove();

					if (pkVictim->CanBeginFight())
						pkVictim->BeginFight(m_me);

#if defined(__9TH_SKILL__)
					if (m_bType == SKILL_PUNGLOEPO)
						sys_log(0, "%s pungloepo %s", m_me->GetName(), pkVictim->GetName());
					else
						sys_log(0, "%s gigung %s", m_me->GetName(), pkVictim->GetName());
#else
					sys_log(0, "%s gigung %s", m_me->GetName(), pkVictim->GetName());
#endif

					m_me->ComputeSkill(m_bType, pkVictim);
					m_me->UseArrow(pkArrow, iUseArrow);
				}
			}
			break;

			case SKILL_HWAJO:
			{
				int iUseArrow = 1;
				if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
				{
					m_me->OnMove(true);
					pkVictim->OnMove();

					if (pkVictim->CanBeginFight())
						pkVictim->BeginFight(m_me);

					sys_log(0, "%s hwajo %s", m_me->GetName(), pkVictim->GetName());
					m_me->ComputeSkill(m_bType, pkVictim);
					m_me->UseArrow(pkArrow, iUseArrow);
				}
			}
			break;

			case SKILL_HORSE_WILDATTACK_RANGE:
			{
				int iUseArrow = 1;
				if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
				{
					m_me->OnMove(true);
					pkVictim->OnMove();

					if (pkVictim->CanBeginFight())
						pkVictim->BeginFight(m_me);

					sys_log(0, "%s horse_wildattack %s", m_me->GetName(), pkVictim->GetName());
					m_me->ComputeSkill(m_bType, pkVictim);
					m_me->UseArrow(pkArrow, iUseArrow);
				}
			}
			break;

			case SKILL_MARYUNG:
			case SKILL_TUSOK:
			case SKILL_BIPABU:
#if defined(__9TH_SKILL__)
			case SKILL_ILGWANGPYO:
			case SKILL_MABEOBAGGWI:
#endif
			case SKILL_NOEJEON:
			case SKILL_GEOMPUNG:
			case SKILL_SANGONG:
			case SKILL_MAHWAN:
			case SKILL_PABEOB:
			{
				m_me->OnMove(true);
				pkVictim->OnMove();

				if (pkVictim->CanBeginFight())
					pkVictim->BeginFight(m_me);

				sys_log(0, "%s - Skill %d -> %s", m_me->GetName(), m_bType, pkVictim->GetName());
				m_me->ComputeSkill(m_bType, pkVictim);
			}
			break;

			case SKILL_CHAIN:
			{
				m_me->OnMove(true);
				pkVictim->OnMove();

				if (pkVictim->CanBeginFight())
					pkVictim->BeginFight(m_me);

				sys_log(0, "%s - Skill %d -> %s", m_me->GetName(), m_bType, pkVictim->GetName());
				m_me->ComputeSkill(m_bType, pkVictim);

				// TODO 여러명에게 슉 슉 슉 하기
			}
			break;

			/*
			case SKILL_BUDONG:
			{
				m_me->OnMove(true);
				pkVictim->OnMove();

				DWORD * pdw;
				DWORD dwEI = AllocEventInfo(sizeof(DWORD) * 2, &pdw);
				pdw[0] = m_me->GetVID();
				pdw[1] = pkVictim->GetVID();

				event_create(budong_event_func, dwEI, PASSES_PER_SEC(1));
			}
			break;
			*/

			default:
				sys_err("CFuncShoot: I don't know this type [%d] of range attack.", (int)m_bType);
				break;
		}

		m_bSucceed = TRUE;
	}
};

bool CHARACTER::Shoot(BYTE bType)
{
	sys_log(1, "Shoot %s type %u flyTargets.size %zu", GetName(), bType, m_vec_dwFlyTargets.size());

	if (!CanMove())
		return false;

	if (IsPC() && IsPolymorphed())
		return false;

	CFuncShoot f(this, bType);

	if (m_dwFlyTargetID != 0)
	{
		f(m_dwFlyTargetID);
		m_dwFlyTargetID = 0;
	}

	f = std::for_each(m_vec_dwFlyTargets.begin(), m_vec_dwFlyTargets.end(), f);
	m_vec_dwFlyTargets.clear();

	return f.m_bSucceed;
}

void CHARACTER::FlyTarget(DWORD dwTargetVID, long x, long y, BYTE bHeader)
{
	LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find(dwTargetVID);
	TPacketGCFlyTargeting pack;

	//pack.bHeader = HEADER_GC_FLY_TARGETING;
	pack.bHeader = (bHeader == HEADER_CG_FLY_TARGETING) ? HEADER_GC_FLY_TARGETING : HEADER_GC_ADD_FLY_TARGETING;
	pack.dwShooterVID = GetVID();

	if (pkVictim)
	{
		pack.dwTargetVID = pkVictim->GetVID();
		pack.x = pkVictim->GetX();
		pack.y = pkVictim->GetY();

		if (bHeader == HEADER_CG_FLY_TARGETING)
			m_dwFlyTargetID = dwTargetVID;
		else
			m_vec_dwFlyTargets.push_back(dwTargetVID);
	}
	else
	{
		pack.dwTargetVID = 0;
		pack.x = x;
		pack.y = y;
	}

	sys_log(1, "FlyTarget %s vid %d x %d y %d", GetName(), pack.dwTargetVID, pack.x, pack.y);
	PacketAround(&pack, sizeof(pack), this);
}

LPCHARACTER CHARACTER::GetNearestVictim(LPCHARACTER pkChr)
{
	if (NULL == pkChr)
		pkChr = this;

	float fMinDist = 99999.0f;
	LPCHARACTER pkVictim = NULL;

	TDamageMap::iterator it = m_map_kDamage.begin();

	// 일단 주위에 없는 사람을 걸러 낸다.
	while (it != m_map_kDamage.end())
	{
		const VID& c_VID = it->first;
		++it;

		LPCHARACTER pAttacker = CHARACTER_MANAGER::instance().Find(c_VID);

		if (!pAttacker)
			continue;

		if (pAttacker->IsAffectFlag(AFF_EUNHYUNG) ||
			pAttacker->IsAffectFlag(AFF_INVISIBILITY) ||
			pAttacker->IsAffectFlag(AFF_REVIVE_INVISIBLE))
			continue;

		float fDist = DISTANCE_APPROX(pAttacker->GetX() - pkChr->GetX(), pAttacker->GetY() - pkChr->GetY());

		if (fDist < fMinDist)
		{
			pkVictim = pAttacker;
			fMinDist = fDist;
		}
	}

	return pkVictim;
}

void CHARACTER::SetVictim(LPCHARACTER pkVictim)
{
	if (!pkVictim)
	{
		if (0 != (DWORD)m_kVIDVictim)
			MonsterLog("Can't find victim");

		m_kVIDVictim.Reset();
		battle_end(this);
	}
	else
	{
		if (m_kVIDVictim != pkVictim->GetVID())
			MonsterLog("Set victim: %s", pkVictim->GetName());

		m_kVIDVictim = pkVictim->GetVID();
		m_dwLastVictimSetTime = get_dword_time();
	}
}

LPCHARACTER CHARACTER::GetVictim() const
{
	return CHARACTER_MANAGER::instance().Find(m_kVIDVictim);
}

LPCHARACTER CHARACTER::GetProtege() const // 보호해야 할 대상을 리턴
{
	if (m_pkChrStone)
		return m_pkChrStone;

	if (m_pkParty)
		return m_pkParty->GetLeader();

	return NULL;
}

int CHARACTER::GetAlignment() const
{
	return m_iAlignment;
}

int CHARACTER::GetRealAlignment() const
{
	return m_iRealAlignment;
}

void CHARACTER::ShowAlignment(bool bShow)
{
	if (bShow)
	{
		if (m_iAlignment != m_iRealAlignment)
		{
			m_iAlignment = m_iRealAlignment;
			UpdatePacket();
		}
	}
	else
	{
		if (m_iAlignment != 0)
		{
			m_iAlignment = 0;
			UpdatePacket();
		}
	}
}

void CHARACTER::UpdateAlignment(int iAmount)
{
	bool bShow = false;

	if (m_iAlignment == m_iRealAlignment)
		bShow = true;

	int i = m_iAlignment / 10;

	m_iRealAlignment = MINMAX(-200000, m_iRealAlignment + iAmount, 200000);

	if (bShow)
	{
		m_iAlignment = m_iRealAlignment;

		if (i != m_iAlignment / 10)
			UpdatePacket();
	}
}

UINT CHARACTER::GetAlignmentGrade() const
{
	const int iAlignment = GetRealAlignment();

	if (iAlignment >= 12000)
		return ALIGN_GRADE_GOOD_4;
	else if (iAlignment >= 8000)
		return ALIGN_GRADE_GOOD_3;
	else if (iAlignment >= 4000)
		return ALIGN_GRADE_GOOD_2;
	else if (iAlignment >= 1000)
		return ALIGN_GRADE_GOOD_1;
	else if (iAlignment >= 0)
		return ALIGN_GRADE_NORMAL;
	else if (iAlignment > -4000)
		return ALIGN_GRADE_EVIL_1;
	else if (iAlignment > -8000)
		return ALIGN_GRADE_EVIL_2;
	else if (iAlignment > -12000)
		return ALIGN_GRADE_EVIL_3;
	else
		return ALIGN_GRADE_EVIL_4;

	return 0;
}

void CHARACTER::SetKillerMode(bool isOn)
{
	if ((isOn ? ADD_CHARACTER_STATE_KILLER : 0) == IS_SET(m_bAddChrState, ADD_CHARACTER_STATE_KILLER))
		return;

	if (isOn)
		SET_BIT(m_bAddChrState, ADD_CHARACTER_STATE_KILLER);
	else
		REMOVE_BIT(m_bAddChrState, ADD_CHARACTER_STATE_KILLER);

	m_iKillerModePulse = thecore_pulse();
	UpdatePacket();
	sys_log(0, "SetKillerMode Update %s[%d]", GetName(), GetPlayerID());
}

bool CHARACTER::IsKillerMode() const
{
	return IS_SET(m_bAddChrState, ADD_CHARACTER_STATE_KILLER);
}

void CHARACTER::UpdateKillerMode()
{
	if (!IsKillerMode())
		return;

	int iKillerSeconds = 60;

	if (thecore_pulse() - m_iKillerModePulse >= PASSES_PER_SEC(iKillerSeconds))
		SetKillerMode(false);
}

void CHARACTER::SetPKMode(BYTE bPKMode)
{
	if (bPKMode >= PK_MODE_MAX_NUM)
		return;

	if (m_bPKMode == bPKMode)
		return;

	if (bPKMode == PK_MODE_GUILD && !GetGuild())
		bPKMode = PK_MODE_FREE;

	m_bPKMode = bPKMode;
	UpdatePacket();

	sys_log(0, "PK_MODE: %s %d", GetName(), m_bPKMode);
}

BYTE CHARACTER::GetPKMode() const
{
	return m_bPKMode;
}

struct FuncForgetMyAttacker
{
	LPCHARACTER m_ch;
	FuncForgetMyAttacker(LPCHARACTER ch)
	{
		m_ch = ch;
	}
	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER)ent;
			if (ch->IsPC())
				return;
			if (ch->m_kVIDVictim == m_ch->GetVID())
				ch->SetVictim(NULL);
		}
	}
};

struct FuncAggregateMonster
{
	LPCHARACTER m_ch;
	FuncAggregateMonster(LPCHARACTER ch)
	{
		m_ch = ch;
	}
	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER)ent;
			if (ch->IsPC())
				return;
			if (!ch->IsMonster())
				return;
			if (ch->GetVictim())
				return;

			if (number(1, 100) <= 50) // 임시로 50% 확률로 적을 끌어온다
				if (DISTANCE_APPROX(ch->GetX() - m_ch->GetX(), ch->GetY() - m_ch->GetY()) < 5000)
					if (ch->CanBeginFight())
						ch->BeginFight(m_ch);
		}
	}
};

struct FuncAttractRanger
{
	LPCHARACTER m_ch;
	FuncAttractRanger(LPCHARACTER ch)
	{
		m_ch = ch;
	}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER)ent;
			if (ch->IsPC())
				return;
			if (!ch->IsMonster())
				return;
			if (ch->GetVictim() && ch->GetVictim() != m_ch)
				return;
			if (ch->GetMobAttackRange() > 150)
			{
				int iNewRange = 150; //(int)(ch->GetMobAttackRange() * 0.2);
				if (iNewRange < 150)
					iNewRange = 150;

				ch->AddAffect(AFFECT_BOW_DISTANCE, POINT_BOW_DISTANCE, iNewRange - ch->GetMobAttackRange(), AFF_NONE, 3 * 60, 0, false);
			}
		}
	}
};

struct FuncPullMonster
{
	LPCHARACTER m_ch;
	int m_iLength;
	FuncPullMonster(LPCHARACTER ch, int iLength = 300)
	{
		m_ch = ch;
		m_iLength = iLength;
	}

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER)ent;
			if (ch->IsPC())
				return;
			if (!ch->IsMonster())
				return;
			/*
			if (ch->GetVictim() && ch->GetVictim() != m_ch)
				return;
			*/
			float fDist = DISTANCE_APPROX(m_ch->GetX() - ch->GetX(), m_ch->GetY() - ch->GetY());
			if (fDist > 3000 || fDist < 100)
				return;

			float fNewDist = fDist - m_iLength;
			if (fNewDist < 100)
				fNewDist = 100;

			float degree = GetDegreeFromPositionXY(ch->GetX(), ch->GetY(), m_ch->GetX(), m_ch->GetY());
			float fx;
			float fy;

			GetDeltaByDegree(degree, fDist - fNewDist, &fx, &fy);
			long tx = (long)(ch->GetX() + fx);
			long ty = (long)(ch->GetY() + fy);

			ch->Sync(tx, ty);
			ch->Goto(tx, ty);
			ch->CalculateMoveDuration();

			ch->SyncPacket();
		}
	}
};

void CHARACTER::ForgetMyAttacker(bool bRevive)
{
	LPSECTREE pSec = GetSectree();
	if (pSec)
	{
		FuncForgetMyAttacker f(this);
		pSec->ForEachAround(f);
	}

	if (bRevive)
		ReviveInvisible(5);
}

void CHARACTER::AggregateMonster()
{
	LPSECTREE pSec = GetSectree();
	if (pSec)
	{
		FuncAggregateMonster f(this);
		pSec->ForEachAround(f);
		EffectPacket(SE_CAPE_OF_COURAGE);
	}
}

void CHARACTER::AttractRanger()
{
	LPSECTREE pSec = GetSectree();
	if (pSec)
	{
		FuncAttractRanger f(this);
		pSec->ForEachAround(f);
	}
}

void CHARACTER::PullMonster()
{
	LPSECTREE pSec = GetSectree();
	if (pSec)
	{
		FuncPullMonster f(this);
		pSec->ForEachAround(f);
	}
}

void CHARACTER::UpdateAggrPointEx(LPCHARACTER pAttacker, EDamageType type, int dam, CHARACTER::TBattleInfo& info)
{
#if defined(__DEFENSE_WAVE__)
	if (GetDefenseWave())
	{
		if (CDefenseWaveManager::Instance().IsHydraSpawn(GetRaceNum()))
			return;

		if (CDefenseWaveManager::Instance().IsHydra(GetRaceNum()))
			return;
	}
#endif

	// 특정 공격타입에 따라 더 올라간다
	switch (type)
	{
		case EDamageType::DAMAGE_TYPE_NORMAL_RANGE:
			dam = (int)(dam * 1.2f);
			break;

		case EDamageType::DAMAGE_TYPE_RANGE:
			dam = (int)(dam * 1.5f);
			break;

		case EDamageType::DAMAGE_TYPE_MAGIC:
			dam = (int)(dam * 1.2f);
			break;

		default:
			break;
	}

	// 공격자가 현재 대상인 경우 보너스를 준다.
	if (pAttacker == GetVictim())
		dam = (int)(dam * 1.2f);

	info.iAggro += dam;

	if (info.iAggro < 0)
		info.iAggro = 0;

	//sys_log(0, "UpdateAggrPointEx for %s by %s dam %d total %d", GetName(), pAttacker->GetName(), dam, total);
	if (GetParty() && dam > 0 && type != DAMAGE_TYPE_SPECIAL)
	{
		LPPARTY pParty = GetParty();

		// 리더인 경우 영향력이 좀더 강하다
		int iPartyAggroDist = dam;

		if (pParty->GetLeaderPID() == GetVID())
			iPartyAggroDist /= 2;
		else
			iPartyAggroDist /= 3;

		pParty->SendMessage(this, PM_AGGRO_INCREASE, iPartyAggroDist, pAttacker->GetVID());
	}

	ChangeVictimByAggro(info.iAggro, pAttacker);
}

void CHARACTER::UpdateAggrPoint(LPCHARACTER pAttacker, EDamageType type, int dam)
{
	if (IsDead() || IsStun())
		return;

	TDamageMap::iterator it = m_map_kDamage.find(pAttacker->GetVID());
	if (it == m_map_kDamage.end())
	{
		m_map_kDamage.insert(TDamageMap::value_type(pAttacker->GetVID(), TBattleInfo(0, dam)));
		it = m_map_kDamage.find(pAttacker->GetVID());
	}

	UpdateAggrPointEx(pAttacker, type, dam, it->second);
}

void CHARACTER::ChangeVictimByAggro(int iNewAggro, LPCHARACTER pNewVictim)
{
	if (get_dword_time() - m_dwLastVictimSetTime < 3000) // 3초는 기다려야한다
		return;

	if (pNewVictim == GetVictim())
	{
		if (m_iMaxAggro < iNewAggro)
		{
			m_iMaxAggro = iNewAggro;
			return;
		}

		// Aggro가 감소한 경우
		TDamageMap::iterator it;
		TDamageMap::iterator itFind = m_map_kDamage.end();

		for (it = m_map_kDamage.begin(); it != m_map_kDamage.end(); ++it)
		{
			if (it->second.iAggro > iNewAggro)
			{
				LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(it->first);

				if (ch && !ch->IsDead() && DISTANCE_APPROX(ch->GetX() - GetX(), ch->GetY() - GetY()) < 5000)
				{
					itFind = it;
					iNewAggro = it->second.iAggro;
				}
			}
		}

		if (itFind != m_map_kDamage.end())
		{
			m_iMaxAggro = iNewAggro;
			SetVictim(CHARACTER_MANAGER::Instance().Find(itFind->first));
			m_dwStateDuration = 1;
		}
	}
	else
	{
		if (m_iMaxAggro < iNewAggro)
		{
			m_iMaxAggro = iNewAggro;
			SetVictim(pNewVictim);
			m_dwStateDuration = 1;
		}
	}
}
