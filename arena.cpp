#include "stdafx.h"
#include "constants.h"
#include "config.h"
#include "packet.h"
#include "desc.h"
#include "buffer_manager.h"
#include "start_position.h"
#include "questmanager.h"
#include "char.h"
#include "char_manager.h"
#include "arena.h"

CArena::CArena(WORD startA_X, WORD startA_Y, WORD startB_X, WORD startB_Y)
{
	m_StartPointA.x = startA_X;
	m_StartPointA.y = startA_Y;
	m_StartPointA.z = 0;

	m_StartPointB.x = startB_X;
	m_StartPointB.y = startB_Y;
	m_StartPointB.z = 0;

	m_ObserverPoint.x = (startA_X + startB_X) / 2;
	m_ObserverPoint.y = (startA_Y + startB_Y) / 2;
	m_ObserverPoint.z = 0;

	m_pEvent = NULL;
	m_pTimeOutEvent = NULL;

	Clear();
}

void CArena::Clear()
{
	m_dwPIDA = 0;
	m_dwPIDB = 0;

	if (m_pEvent != NULL)
	{
		event_cancel(&m_pEvent);
	}

	if (m_pTimeOutEvent != NULL)
	{
		event_cancel(&m_pTimeOutEvent);
	}

	m_dwSetCount = 0;
	m_dwSetPointOfA = 0;
	m_dwSetPointOfB = 0;
}

bool CArenaManager::AddArena(DWORD mapIdx, WORD startA_X, WORD startA_Y, WORD startB_X, WORD startB_Y)
{
	CArenaMap* pArenaMap = NULL;

	auto iter = m_mapArenaMap.find(mapIdx);
	if (iter == m_mapArenaMap.end())
	{
		pArenaMap = M2_NEW CArenaMap;
		m_mapArenaMap.insert(std::make_pair(mapIdx, pArenaMap));
	}
	else
	{
		pArenaMap = iter->second;
	}

	if (pArenaMap->AddArena(mapIdx, startA_X, startA_Y, startB_X, startB_Y) == false)
	{
		sys_log(0, "CArenaManager::AddArena - AddMap Error MapID: %d", mapIdx);
		return false;
	}

	return true;
}

bool CArenaMap::AddArena(DWORD mapIdx, WORD startA_X, WORD startA_Y, WORD startB_X, WORD startB_Y)
{
	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); iter++)
	{
		if ((*iter)->CheckArea(startA_X, startA_Y, startB_X, startB_Y) == false)
		{
			sys_log(0, "CArenaMap::AddArena - Same Start Position set. stA(%d, %d) stB(%d, %d)", startA_X, startA_Y, startB_X, startB_Y);
			return false;
		}
	}

	m_dwMapIndex = mapIdx;

	CArena* pArena = M2_NEW CArena(startA_X, startA_Y, startB_X, startB_Y);
	m_listArena.push_back(pArena);

	return true;
}

void CArenaManager::Destroy()
{
	auto iter = m_mapArenaMap.begin();
	for (; iter != m_mapArenaMap.end(); iter++)
	{
		CArenaMap* pArenaMap = iter->second;
		pArenaMap->Destroy();

		M2_DELETE(pArenaMap);
	}
	m_mapArenaMap.clear();
}

void CArenaMap::Destroy()
{
	sys_log(0, "ARENA: ArenaMap will be destroy. mapIndex(%d)", m_dwMapIndex);

	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); iter++)
	{
		CArena* pArena = *iter;
		pArena->EndDuel();

		M2_DELETE(pArena);
	}
	m_listArena.clear();
}

bool CArena::CheckArea(WORD startA_X, WORD startA_Y, WORD startB_X, WORD startB_Y)
{
	if (m_StartPointA.x == startA_X && m_StartPointA.y == startA_Y &&
		m_StartPointB.x == startB_X && m_StartPointB.y == startB_Y)
		return false;

	return true;
}

void CArenaManager::SendArenaMapListTo(LPCHARACTER pChar)
{
	auto iter = m_mapArenaMap.begin();
	for (; iter != m_mapArenaMap.end(); iter++)
	{
		CArenaMap* pArena = iter->second;
		pArena->SendArenaMapListTo(pChar, (iter->first));
	}
}

void CArenaMap::SendArenaMapListTo(LPCHARACTER pChar, DWORD mapIdx)
{
	if (pChar == NULL) return;

	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); iter++)
	{
		pChar->ChatPacket(CHAT_TYPE_INFO, "ArenaMapInfo Map: %d stA(%d, %d) stB(%d, %d)", mapIdx,
			(CArena*)(*iter)->GetStartPointA().x, (CArena*)(*iter)->GetStartPointA().y,
			(CArena*)(*iter)->GetStartPointB().x, (CArena*)(*iter)->GetStartPointB().y
		);
	}
}

bool CArenaManager::StartDuel(LPCHARACTER pCharFrom, LPCHARACTER pCharTo, int nSetPoint, int nMinute)
{
	if (pCharFrom == NULL || pCharTo == NULL) return false;

	auto iter = m_mapArenaMap.begin();
	for (; iter != m_mapArenaMap.end(); iter++)
	{
		CArenaMap* pArenaMap = iter->second;
		if (pArenaMap->StartDuel(pCharFrom, pCharTo, nSetPoint, nMinute) == true)
		{
			return true;
		}
	}

	return false;
}

bool CArenaMap::StartDuel(LPCHARACTER pCharFrom, LPCHARACTER pCharTo, int nSetPoint, int nMinute)
{
	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); iter++)
	{
		CArena* pArena = *iter;
		if (pArena->IsEmpty() == true)
		{
			return pArena->StartDuel(pCharFrom, pCharTo, nSetPoint, nMinute);
		}
	}

	return false;
}

EVENTINFO(TArenaEventInfo)
{
	CArena* pArena;
	BYTE state;

	TArenaEventInfo()
		: pArena(0)
		, state(0)
	{
	}
};

EVENTFUNC(ready_to_start_event)
{
	if (event == NULL)
		return 0;

	if (event->info == NULL)
		return 0;

	TArenaEventInfo* info = dynamic_cast<TArenaEventInfo*>(event->info);

	if (info == NULL)
	{
		sys_err("ready_to_start_event> <Factor> Null pointer");
		return 0;
	}

	CArena* pArena = info->pArena;

	if (pArena == NULL)
	{
		sys_err("ARENA: Arena start event info is null.");
		return 0;
	}

	LPCHARACTER chA = pArena->GetPlayerA();
	LPCHARACTER chB = pArena->GetPlayerB();

	if (chA == NULL || chB == NULL)
	{
		sys_err("ARENA: Player err in event func ready_start_event");

		if (chA != NULL)
		{
			chA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("대련 상대가 사라져 대련을 종료합니다."));
			sys_log(0, "ARENA: Oppernent is disappered. MyPID(%d) OppPID(%d)", pArena->GetPlayerAPID(), pArena->GetPlayerBPID());
		}

		if (chB != NULL)
		{
			chB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("대련 상대가 사라져 대련을 종료합니다."));
			sys_log(0, "ARENA: Oppernent is disappered. MyPID(%d) OppPID(%d)", pArena->GetPlayerBPID(), pArena->GetPlayerAPID());
		}

		pArena->SendChatPacketToObserver(CHAT_TYPE_NOTICE, LC_STRING("대련 상대가 사라져 대련을 종료합니다."));

		pArena->EndDuel();
		return 0;
	}

	switch (info->state)
	{
		case 0:
		{
			chA->SetArena(pArena);
			chB->SetArena(pArena);

			int count = quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count");

			if (count > 10000)
			{
				chA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("물약 제한이 없습니다."));
				chB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("물약 제한이 없습니다."));
			}
			else
			{
				chA->SetPotionLimit(count);
				chB->SetPotionLimit(count);

				chA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("물약을 %d 개 까지 사용 가능합니다.", chA->GetPotionLimit()));
				chB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("물약을 %d 개 까지 사용 가능합니다.", chB->GetPotionLimit()));
			}

			chA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("10초뒤 대련이 시작됩니다."));
			chB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("10초뒤 대련이 시작됩니다."));
			pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, LC_STRING("10초뒤 대련이 시작됩니다."));

			info->state++;
			return PASSES_PER_SEC(10);
		}
		break;

		case 1:
		{
			chA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("대련이 시작되었습니다."));
			chB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("대련이 시작되었습니다."));
			pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, LC_STRING("대련이 시작되었습니다."));

			TPacketGCDuelStart duelStart;
			duelStart.header = HEADER_GC_DUEL_START;
			duelStart.wSize = sizeof(TPacketGCDuelStart) + 4;

			DWORD dwOppList[8]; // 최대 파티원 8명 이므로.. eng. maximum members for arena (2x4)

			dwOppList[0] = (DWORD)chB->GetVID();
			TEMP_BUFFER buf;

			buf.write(&duelStart, sizeof(TPacketGCDuelStart));
			buf.write(&dwOppList[0], 4);
			chA->GetDesc()->Packet(buf.read_peek(), buf.size());

			dwOppList[0] = (DWORD)chA->GetVID();
			TEMP_BUFFER buf2;

			buf2.write(&duelStart, sizeof(TPacketGCDuelStart));
			buf2.write(&dwOppList[0], 4);
			chB->GetDesc()->Packet(buf2.read_peek(), buf2.size());

			return 0;
		}
		break;

		case 2:
		{
			pArena->EndDuel();
			return 0;
		}
		break;

		case 3:
		{
			chA->Show(chA->GetMapIndex(), pArena->GetStartPointA().x * 100, pArena->GetStartPointA().y * 100);
			chB->Show(chB->GetMapIndex(), pArena->GetStartPointB().x * 100, pArena->GetStartPointB().y * 100);

			chA->GetDesc()->SetPhase(PHASE_GAME);
			chA->StartRecoveryEvent();
			chA->SetPosition(POS_STANDING);
			chA->PointChange(POINT_HP, chA->GetMaxHP() - chA->GetHP());
			chA->PointChange(POINT_SP, chA->GetMaxSP() - chA->GetSP());
			chA->ViewReencode();

			chB->GetDesc()->SetPhase(PHASE_GAME);
			chB->StartRecoveryEvent();
			chB->SetPosition(POS_STANDING);
			chB->PointChange(POINT_HP, chB->GetMaxHP() - chB->GetHP());
			chB->PointChange(POINT_SP, chB->GetMaxSP() - chB->GetSP());
			chB->ViewReencode();

			TEMP_BUFFER buf;
			TEMP_BUFFER buf2;
			DWORD dwOppList[8]; // 최대 파티원 8명 이므로.. eng. maximum members for arena (2x4)
			TPacketGCDuelStart duelStart;
			duelStart.header = HEADER_GC_DUEL_START;
			duelStart.wSize = sizeof(TPacketGCDuelStart) + 4;

			dwOppList[0] = (DWORD)chB->GetVID();
			buf.write(&duelStart, sizeof(TPacketGCDuelStart));
			buf.write(&dwOppList[0], 4);
			chA->GetDesc()->Packet(buf.read_peek(), buf.size());

			dwOppList[0] = (DWORD)chA->GetVID();
			buf2.write(&duelStart, sizeof(TPacketGCDuelStart));
			buf2.write(&dwOppList[0], 4);
			chB->GetDesc()->Packet(buf2.read_peek(), buf2.size());

			chA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("대련이 시작되었습니다."));
			chB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("대련이 시작되었습니다."));
			pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, LC_STRING("대련이 시작되었습니다."));

			pArena->ClearEvent();

			return 0;
		}
		break;

		default:
		{
			chA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("대련장 문제로 인하여 대련을 종료합니다."));
			chB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("대련장 문제로 인하여 대련을 종료합니다."));
			pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, LC_STRING("대련장 문제로 인하여 대련을 종료합니다."));

			sys_log(0, "ARENA: Something wrong in event func. info->state(%d)", info->state);

			pArena->EndDuel();

			return 0;
		}
	}
}

EVENTFUNC(duel_time_out)
{
	if (event == NULL) return 0;
	if (event->info == NULL) return 0;

	TArenaEventInfo* info = dynamic_cast<TArenaEventInfo*>(event->info);

	if (info == NULL)
	{
		sys_err("duel_time_out> <Factor> Null pointer");
		return 0;
	}

	CArena* pArena = info->pArena;

	if (pArena == NULL)
	{
		sys_err("ARENA: Time out event error");
		return 0;
	}

	LPCHARACTER chA = pArena->GetPlayerA();
	LPCHARACTER chB = pArena->GetPlayerB();

	if (chA == NULL || chB == NULL)
	{
		if (chA != NULL)
		{
			chA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("대련 상대가 사라져 대련을 종료합니다."));
			sys_log(0, "ARENA: Oppernent is disappered. MyPID(%d) OppPID(%d)", pArena->GetPlayerAPID(), pArena->GetPlayerBPID());
		}

		if (chB != NULL)
		{
			chB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("대련 상대가 사라져 대련을 종료합니다."));
			sys_log(0, "ARENA: Oppernent is disappered. MyPID(%d) OppPID(%d)", pArena->GetPlayerBPID(), pArena->GetPlayerAPID());
		}

		pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, LC_STRING("대련 상대가 사라져 대련을 종료합니다."));

		pArena->EndDuel();
		return 0;
	}
	else
	{
		switch (info->state)
		{
			case 0:
				pArena->SendChatPacketToObserver(CHAT_TYPE_NOTICE, LC_STRING("대련 시간 초과로 대련을 중단합니다."));
				pArena->SendChatPacketToObserver(CHAT_TYPE_NOTICE, LC_STRING("10초뒤 마을로 이동합니다."));

				chA->ChatPacket(CHAT_TYPE_NOTICE, LC_STRING("대련 시간 초과로 대련을 중단합니다."));
				chA->ChatPacket(CHAT_TYPE_NOTICE, LC_STRING("10초뒤 마을로 이동합니다."));

				chB->ChatPacket(CHAT_TYPE_NOTICE, LC_STRING("대련 시간 초과로 대련을 중단합니다."));
				chB->ChatPacket(CHAT_TYPE_NOTICE, LC_STRING("10초뒤 마을로 이동합니다."));

				TPacketGCDuelStart duelStart;
				duelStart.header = HEADER_GC_DUEL_START;
				duelStart.wSize = sizeof(TPacketGCDuelStart);

				chA->GetDesc()->Packet(&duelStart, sizeof(TPacketGCDuelStart));
				chA->GetDesc()->Packet(&duelStart, sizeof(TPacketGCDuelStart));

				info->state++;

				sys_log(0, "ARENA: Because of time over, duel is end. PIDA(%d) vs PIDB(%d)", pArena->GetPlayerAPID(), pArena->GetPlayerBPID());

				return PASSES_PER_SEC(10);
				break;

			case 1:
				pArena->EndDuel();
				break;
		}
	}

	return 0;
}

bool CArena::StartDuel(LPCHARACTER pCharFrom, LPCHARACTER pCharTo, int nSetPoint, int nMinute)
{
	this->m_dwPIDA = pCharFrom->GetPlayerID();
	this->m_dwPIDB = pCharTo->GetPlayerID();
	this->m_dwSetCount = nSetPoint;

	pCharFrom->WarpSet(GetStartPointA().x * 100, GetStartPointA().y * 100);
	pCharTo->WarpSet(GetStartPointB().x * 100, GetStartPointB().y * 100);

	if (m_pEvent != NULL)
	{
		event_cancel(&m_pEvent);
	}

	TArenaEventInfo* info = AllocEventInfo<TArenaEventInfo>();

	info->pArena = this;
	info->state = 0;

	m_pEvent = event_create(ready_to_start_event, info, PASSES_PER_SEC(10));

	if (m_pTimeOutEvent != NULL)
	{
		event_cancel(&m_pTimeOutEvent);
	}

	info = AllocEventInfo<TArenaEventInfo>();

	info->pArena = this;
	info->state = 0;

	m_pTimeOutEvent = event_create(duel_time_out, info, PASSES_PER_SEC(nMinute * 60));

	pCharFrom->PointChange(POINT_HP, pCharFrom->GetMaxHP() - pCharFrom->GetHP());
	pCharFrom->PointChange(POINT_SP, pCharFrom->GetMaxSP() - pCharFrom->GetSP());

	pCharTo->PointChange(POINT_HP, pCharTo->GetMaxHP() - pCharTo->GetHP());
	pCharTo->PointChange(POINT_SP, pCharTo->GetMaxSP() - pCharTo->GetSP());

	sys_log(0, "ARENA: Start Duel with PID_A(%d) vs PID_B(%d)", GetPlayerAPID(), GetPlayerBPID());
	return true;
}

void CArenaManager::EndAllDuel()
{
	auto iter = m_mapArenaMap.begin();
	for (; iter != m_mapArenaMap.end(); iter++)
	{
		CArenaMap* pArenaMap = iter->second;
		if (pArenaMap != NULL)
			pArenaMap->EndAllDuel();
	}

	return;
}

void CArenaMap::EndAllDuel()
{
	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); iter++)
	{
		CArena* pArena = *iter;
		if (pArena != NULL)
			pArena->EndDuel();
	}
}

void CArena::EndDuel()
{
	if (m_pEvent != NULL)
	{
		event_cancel(&m_pEvent);
	}

	if (m_pTimeOutEvent != NULL)
	{
		event_cancel(&m_pTimeOutEvent);
	}

	LPCHARACTER playerA = GetPlayerA();
	LPCHARACTER playerB = GetPlayerB();

	if (playerA != NULL)
	{
		playerA->SetPKMode(PK_MODE_PEACE);
		playerA->StartRecoveryEvent();
		playerA->SetPosition(POS_STANDING);
		playerA->PointChange(POINT_HP, playerA->GetMaxHP() - playerA->GetHP());
		playerA->PointChange(POINT_SP, playerA->GetMaxSP() - playerA->GetSP());

		playerA->SetArena(NULL);

		playerA->WarpSet(ARENA_RETURN_POINT_X(playerA->GetEmpire()), ARENA_RETURN_POINT_Y(playerA->GetEmpire()));
	}

	if (playerB != NULL)
	{
		playerB->SetPKMode(PK_MODE_PEACE);
		playerB->StartRecoveryEvent();
		playerB->SetPosition(POS_STANDING);
		playerB->PointChange(POINT_HP, playerB->GetMaxHP() - playerB->GetHP());
		playerB->PointChange(POINT_SP, playerB->GetMaxSP() - playerB->GetSP());

		playerB->SetArena(NULL);

		playerB->WarpSet(ARENA_RETURN_POINT_X(playerB->GetEmpire()), ARENA_RETURN_POINT_Y(playerB->GetEmpire()));
	}

	auto iter = m_mapObserver.begin();
	for (; iter != m_mapObserver.end(); iter++)
	{
		LPCHARACTER pChar = CHARACTER_MANAGER::instance().FindByPID(iter->first);
		if (pChar != NULL)
		{
			pChar->WarpSet(ARENA_RETURN_POINT_X(pChar->GetEmpire()), ARENA_RETURN_POINT_Y(pChar->GetEmpire()));
		}
	}

	m_mapObserver.clear();

	sys_log(0, "ARENA: End Duel PID_A(%d) vs PID_B(%d)", GetPlayerAPID(), GetPlayerBPID());

	Clear();
}

void CArenaManager::GetDuelList(lua_State* L)
{
	int index = 1;
	lua_newtable(L);

	auto iter = m_mapArenaMap.begin();
	for (; iter != m_mapArenaMap.end(); iter++)
	{
		CArenaMap* pArenaMap = iter->second;
		if (pArenaMap != NULL)
			index = pArenaMap->GetDuelList(L, index);
	}
}

int CArenaMap::GetDuelList(lua_State* L, int index)
{
	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); iter++)
	{
		CArena* pArena = *iter;

		if (pArena == NULL) continue;

		if (pArena->IsEmpty() == false)
		{
			LPCHARACTER chA = pArena->GetPlayerA();
			LPCHARACTER chB = pArena->GetPlayerB();

			if (chA != NULL && chB != NULL)
			{
				lua_newtable(L);

				lua_pushstring(L, chA->GetName());
				lua_rawseti(L, -2, 1);

				lua_pushstring(L, chB->GetName());
				lua_rawseti(L, -2, 2);

				lua_pushnumber(L, m_dwMapIndex);
				lua_rawseti(L, -2, 3);

				lua_pushnumber(L, pArena->GetObserverPoint().x);
				lua_rawseti(L, -2, 4);

				lua_pushnumber(L, pArena->GetObserverPoint().y);
				lua_rawseti(L, -2, 5);

				lua_rawseti(L, -2, index++);
			}
		}
	}

	return index;
}

bool CArenaManager::CanAttack(LPCHARACTER pCharAttacker, LPCHARACTER pCharVictim)
{
	if (pCharAttacker == NULL || pCharVictim == NULL) return false;

	if (pCharAttacker == pCharVictim) return false;

	long mapIndex = pCharAttacker->GetMapIndex();
	if (mapIndex != pCharVictim->GetMapIndex()) return false;

	auto iter = m_mapArenaMap.find(mapIndex);
	if (iter == m_mapArenaMap.end()) return false;

	CArenaMap* pArenaMap = (CArenaMap*)(iter->second);
	return pArenaMap->CanAttack(pCharAttacker, pCharVictim);
}

bool CArenaMap::CanAttack(LPCHARACTER pCharAttacker, LPCHARACTER pCharVictim)
{
	if (pCharAttacker == NULL || pCharVictim == NULL) return false;

	DWORD dwPIDA = pCharAttacker->GetPlayerID();
	DWORD dwPIDB = pCharVictim->GetPlayerID();

	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); iter++)
	{
		CArena* pArena = *iter;
		if (pArena->CanAttack(dwPIDA, dwPIDB) == true)
		{
			return true;
		}
	}
	return false;
}

bool CArena::CanAttack(DWORD dwPIDA, DWORD dwPIDB)
{
	// 1:1 전용 다대다 할 경우 수정 필요 eng. Modification required only if many-to-many
	if (m_dwPIDA == dwPIDA && m_dwPIDB == dwPIDB) return true;
	if (m_dwPIDA == dwPIDB && m_dwPIDB == dwPIDA) return true;

	return false;
}

bool CArenaManager::OnDead(LPCHARACTER pCharKiller, LPCHARACTER pCharVictim)
{
	if (pCharKiller == NULL || pCharVictim == NULL) return false;

	long mapIndex = pCharKiller->GetMapIndex();
	if (mapIndex != pCharVictim->GetMapIndex()) return false;

	auto iter = m_mapArenaMap.find(mapIndex);
	if (iter == m_mapArenaMap.end()) return false;

	CArenaMap* pArenaMap = (CArenaMap*)(iter->second);
	return pArenaMap->OnDead(pCharKiller, pCharVictim);
}

bool CArenaMap::OnDead(LPCHARACTER pCharKiller, LPCHARACTER pCharVictim)
{
	DWORD dwPIDA = pCharKiller->GetPlayerID();
	DWORD dwPIDB = pCharVictim->GetPlayerID();

	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); iter++)
	{
		CArena* pArena = *iter;

		if (pArena->IsMember(dwPIDA) == true && pArena->IsMember(dwPIDB) == true)
		{
			pArena->OnDead(dwPIDA, dwPIDB);
			return true;
		}
	}
	return false;
}

bool CArena::OnDead(DWORD dwPIDA, DWORD dwPIDB)
{
	bool restart = false;

	LPCHARACTER pCharA = GetPlayerA();
	LPCHARACTER pCharB = GetPlayerB();

	if (pCharA == NULL && pCharB == NULL)
	{
		// 둘다 접속이 끊어졌다 ?! eng. Both were disconnected ?!
		SendChatPacketToObserver(CHAT_TYPE_NOTICE, LC_STRING("대련자 문제로 인하여 대련을 중단합니다."));
		restart = false;
	}
	else if (pCharA == NULL && pCharB != NULL)
	{
		pCharB->ChatPacket(CHAT_TYPE_NOTICE, LC_STRING("상대방 캐릭터의 문제로 인하여 대련을 종료합니다."));
		SendChatPacketToObserver(CHAT_TYPE_NOTICE, LC_STRING("대련자 문제로 인하여 대련을 종료합니다."));
		restart = false;
	}
	else if (pCharA != NULL && pCharB == NULL)
	{
		pCharA->ChatPacket(CHAT_TYPE_NOTICE, LC_STRING("상대방 캐릭터의 문제로 인하여 대련을 종료합니다."));
		SendChatPacketToObserver(CHAT_TYPE_NOTICE, LC_STRING("대련자 문제로 인하여 대련을 종료합니다."));
		restart = false;
	}
	else if (pCharA != NULL && pCharB != NULL)
	{
		if (m_dwPIDA == dwPIDA)
		{
			m_dwSetPointOfA++;

			if (m_dwSetPointOfA >= m_dwSetCount)
			{
				pCharA->ChatPacket(CHAT_TYPE_NOTICE, LC_STRING("%s 님이 대련에서 승리하였습니다.", pCharA->GetName()));
				pCharB->ChatPacket(CHAT_TYPE_NOTICE, LC_STRING("%s 님이 대련에서 승리하였습니다.", pCharA->GetName()));
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, LC_STRING("%s 님이 대련에서 승리하였습니다.", pCharA->GetName()));

				sys_log(0, "ARENA: Duel is end. Winner %s(%d) Loser %s(%d)",
					pCharA->GetName(), GetPlayerAPID(), pCharB->GetName(), GetPlayerBPID());
			}
			else
			{
				restart = true;
				pCharA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("%s 님이 승리하였습니다.", pCharA->GetName()));
				pCharA->ChatPacket(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->GetName(), m_dwSetPointOfA, m_dwSetPointOfB, pCharB->GetName());

				pCharB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("%s 님이 승리하였습니다.", pCharA->GetName()));
				pCharB->ChatPacket(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->GetName(), m_dwSetPointOfA, m_dwSetPointOfB, pCharB->GetName());

				SendChatPacketToObserver(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->GetName(), m_dwSetPointOfA, m_dwSetPointOfB, pCharB->GetName());

				sys_log(0, "ARENA: %s(%d) won a round vs %s(%d)",
					pCharA->GetName(), GetPlayerAPID(), pCharB->GetName(), GetPlayerBPID());
			}
		}
		else if (m_dwPIDB == dwPIDA)
		{
			m_dwSetPointOfB++;
			if (m_dwSetPointOfB >= m_dwSetCount)
			{
				pCharA->ChatPacket(CHAT_TYPE_NOTICE, LC_STRING("%s 님이 대련에서 승리하였습니다.", pCharB->GetName()));
				pCharB->ChatPacket(CHAT_TYPE_NOTICE, LC_STRING("%s 님이 대련에서 승리하였습니다.", pCharB->GetName()));
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, LC_STRING("%s 님이 대련에서 승리하였습니다.", pCharB->GetName()));

				sys_log(0, "ARENA: Duel is end. Winner(%d) Loser(%d)", GetPlayerBPID(), GetPlayerAPID());
			}
			else
			{
				restart = true;
				pCharA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("%s 님이 승리하였습니다.", pCharB->GetName()));
				pCharA->ChatPacket(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->GetName(), m_dwSetPointOfA, m_dwSetPointOfB, pCharB->GetName());

				pCharB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("%s 님이 승리하였습니다.", pCharB->GetName()));
				pCharB->ChatPacket(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->GetName(), m_dwSetPointOfA, m_dwSetPointOfB, pCharB->GetName());

				SendChatPacketToObserver(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->GetName(), m_dwSetPointOfA, m_dwSetPointOfB, pCharB->GetName());

				sys_log(0, "ARENA : PID(%d) won a round. Opp(%d)", GetPlayerBPID(), GetPlayerAPID());
			}
		}
		else
		{
			// wtf
			sys_log(0, "ARENA : OnDead Error (%d, %d) (%d, %d)", m_dwPIDA, m_dwPIDB, dwPIDA, dwPIDB);
		}

		int potion = quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count");
		pCharA->SetPotionLimit(potion);
		pCharB->SetPotionLimit(potion);
	}
	else
	{
		// 오면 안된다 ?! eng. Shouldn't you come?!
	}

	if (restart == false)
	{
		if (pCharA != NULL)
			pCharA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("10초뒤 마을로 되돌아갑니다."));

		if (pCharB != NULL)
			pCharB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("10초뒤 마을로 되돌아갑니다."));

		SendChatPacketToObserver(CHAT_TYPE_INFO, LC_STRING("10초뒤 마을로 되돌아갑니다."));

		if (m_pEvent != NULL)
		{
			event_cancel(&m_pEvent);
		}

		TArenaEventInfo* info = AllocEventInfo<TArenaEventInfo>();

		info->pArena = this;
		info->state = 2;

		m_pEvent = event_create(ready_to_start_event, info, PASSES_PER_SEC(10));
	}
	else
	{
		if (pCharA != NULL)
			pCharA->ChatPacket(CHAT_TYPE_INFO, LC_STRING("10초뒤 다음 판을 시작합니다."));

		if (pCharB != NULL)
			pCharB->ChatPacket(CHAT_TYPE_INFO, LC_STRING("10초뒤 다음 판을 시작합니다."));

		SendChatPacketToObserver(CHAT_TYPE_INFO, LC_STRING("10초뒤 다음 판을 시작합니다."));

		if (m_pEvent != NULL)
		{
			event_cancel(&m_pEvent);
		}

		TArenaEventInfo* info = AllocEventInfo<TArenaEventInfo>();

		info->pArena = this;
		info->state = 3;

		m_pEvent = event_create(ready_to_start_event, info, PASSES_PER_SEC(10));
	}

	return true;
}

bool CArenaManager::AddObserver(LPCHARACTER pChar, DWORD mapIdx, WORD ObserverX, WORD ObserverY)
{
	auto iter = m_mapArenaMap.find(mapIdx);
	if (iter == m_mapArenaMap.end()) return false;

	CArenaMap* pArenaMap = iter->second;
	return pArenaMap->AddObserver(pChar, ObserverX, ObserverY);
}

bool CArenaMap::AddObserver(LPCHARACTER pChar, WORD ObserverX, WORD ObserverY)
{
	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); iter++)
	{
		CArena* pArena = *iter;

		if (pArena->IsMyObserver(ObserverX, ObserverY) == true)
		{
			pChar->SetArena(pArena);
			return pArena->AddObserver(pChar);
		}
	}

	return false;
}

bool CArena::IsMyObserver(WORD ObserverX, WORD ObserverY)
{
	return ((ObserverX == m_ObserverPoint.x) && (ObserverY == m_ObserverPoint.y));
}

bool CArena::AddObserver(LPCHARACTER pChar)
{
	DWORD pid = pChar->GetPlayerID();

	m_mapObserver.insert(std::make_pair(pid, (LPCHARACTER)NULL));

	pChar->SaveExitLocation();
	pChar->WarpSet(m_ObserverPoint.x * 100, m_ObserverPoint.y * 100);

	return true;
}

bool CArenaManager::IsArenaMap(DWORD dwMapIndex)
{
	return m_mapArenaMap.find(dwMapIndex) != m_mapArenaMap.end();
}

MEMBER_IDENTITY CArenaManager::IsMember(DWORD dwMapIndex, DWORD PID)
{
	auto iter = m_mapArenaMap.find(dwMapIndex);
	if (iter != m_mapArenaMap.end())
	{
		CArenaMap* pArenaMap = iter->second;
		return pArenaMap->IsMember(PID);
	}
	return MEMBER_NO;
}

MEMBER_IDENTITY CArenaMap::IsMember(DWORD PID)
{
	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); iter++)
	{
		CArena* pArena = *iter;

		if (pArena->IsObserver(PID) == true) return MEMBER_OBSERVER;
		if (pArena->IsMember(PID) == true) return MEMBER_DUELIST;
	}
	return MEMBER_NO;
}

bool CArena::IsObserver(DWORD PID)
{
	auto iter = m_mapObserver.find(PID);
	return iter != m_mapObserver.end();
}

void CArena::OnDisconnect(DWORD pid)
{
	if (m_dwPIDA == pid)
	{
		if (GetPlayerB() != NULL)
			GetPlayerB()->ChatPacket(CHAT_TYPE_INFO, LC_STRING("상대방 캐릭터가 접속을 종료하여 대련을 중지합니다."));

		sys_log(0, "ARENA : Duel is end because of Opp(%d) is disconnect. MyPID(%d)", GetPlayerAPID(), GetPlayerBPID());
		EndDuel();
	}
	else if (m_dwPIDB == pid)
	{
		if (GetPlayerA() != NULL)
			GetPlayerA()->ChatPacket(CHAT_TYPE_INFO, LC_STRING("상대방 캐릭터가 접속을 종료하여 대련을 중지합니다."));

		sys_log(0, "ARENA : Duel is end because of Opp(%d) is disconnect. MyPID(%d)", GetPlayerBPID(), GetPlayerAPID());
		EndDuel();
	}
}

void CArena::RemoveObserver(DWORD pid)
{
	auto iter = m_mapObserver.find(pid);
	if (iter != m_mapObserver.end())
	{
		m_mapObserver.erase(iter);
	}
}

void CArena::SendPacketToObserver(const void* c_pvData, int iSize)
{
	/*
	auto iter = m_mapObserver.begin();
	for (; iter != m_mapObserver.end(); iter++)
	{
		LPCHARACTER pChar = iter->second;

		if (pChar != NULL)
		{
			if (pChar->GetDesc() != NULL)
			{
				pChar->GetDesc()->Packet(c_pvData, iSize);
			}
		}
	}
	*/
}

void CArena::SendChatPacketToObserver(BYTE type, const char* format, ...)
{
	/*
	char chatbuf[CHAT_MAX_LEN + 1];
	va_list args;

	va_start(args, format);
	vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	auto iter = m_mapObserver.begin();
	for (; iter != m_mapObserver.end(); iter++)
	{
		LPCHARACTER pChar = iter->second;

		if (pChar != NULL)
		{
			if (pChar->GetDesc() != NULL)
			{
				pChar->ChatPacket(type, chatbuf);
			}
		}
	}
	*/
}

bool CArenaManager::EndDuel(DWORD pid)
{
	auto iter = m_mapArenaMap.begin();
	for (; iter != m_mapArenaMap.end(); iter++)
	{
		CArenaMap* pArenaMap = iter->second;
		if (pArenaMap->EndDuel(pid) == true) return true;
	}
	return false;
}

bool CArenaMap::EndDuel(DWORD pid)
{
	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); iter++)
	{
		CArena* pArena = *iter;
		if (pArena->IsMember(pid) == true)
		{
			pArena->EndDuel();
			return true;
		}
	}
	return false;
}

bool CArenaManager::RegisterObserverPtr(LPCHARACTER pChar, DWORD mapIdx, WORD ObserverX, WORD ObserverY)
{
	if (pChar == NULL) return false;

	auto iter = m_mapArenaMap.find(mapIdx);
	if (iter == m_mapArenaMap.end())
	{
		sys_log(0, "ARENA : Cannot find ArenaMap. %d %d %d", mapIdx, ObserverX, ObserverY);
		return false;
	}

	CArenaMap* pArenaMap = iter->second;
	return pArenaMap->RegisterObserverPtr(pChar, mapIdx, ObserverX, ObserverY);
}

bool CArenaMap::RegisterObserverPtr(LPCHARACTER pChar, DWORD mapIdx, WORD ObserverX, WORD ObserverY)
{
	auto iter = m_listArena.begin();
	for (; iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;

		if (pArena->IsMyObserver(ObserverX, ObserverY) == true)
		{
			return pArena->RegisterObserverPtr(pChar);
		}
	}

	return false;
}

bool CArena::RegisterObserverPtr(LPCHARACTER pChar)
{
	DWORD pid = pChar->GetPlayerID();

	auto iter = m_mapObserver.find(pid);
	if (iter == m_mapObserver.end())
	{
		sys_log(0, "ARENA : not in ob list");
		return false;
	}

	m_mapObserver[pid] = pChar;
	return true;
}

bool CArenaManager::IsLimitedItem(long lMapIndex, DWORD dwVnum)
{
	if (IsArenaMap(lMapIndex) == true)
	{
		switch (dwVnum)
		{
			case 50020: // Bean Cake
			case 50021: // Sugar Cake
			case 50022: // Fruit Cake
			case 50801: // Peach Blossom Juice
			case 50802: // Bellflower Juice
			case 50813: // Sim Water
			case 50814: // Dok Water
			case 50817: // Zin Water
			case 50818: // SamBo Water
			case 50819: // Mong Water
			case 50820: // Hwal Water
			case 50821: // Red Dew
			case 50822: // Pink Dew
			case 50823: // Yellow Dew
			case 50824: // Green Dew
			case 50825: // Blue Dew
			case 50826: // White Dew
			case 71044: // Critical Strike
			case 71055: // Tincture of the Name
				return true;
		}
	}

	return false;
}