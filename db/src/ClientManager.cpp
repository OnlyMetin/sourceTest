#include "stdafx.h"

#include "../../common/building.h"
#include "../../common/VnumHelper.h"
#include "../../libgame/include/grid.h"

#include "ClientManager.h"

#include "Main.h"
#include "Config.h"
#include "DBManager.h"
#include "QID.h"
#include "GuildManager.h"
#include "PrivManager.h"
#include "MoneyLog.h"
#include "ItemAwardManager.h"
#include "Marriage.h"
#include "Monarch.h"
#include "BlockCountry.h"
#include "ItemIDRangeManager.h"
#include "Cache.h"

extern int g_iPlayerCacheFlushSeconds;
extern int g_iItemCacheFlushSeconds;
extern int g_test_server;
extern int g_log;
extern std::string g_stLocale;
extern std::string g_stLocaleNameColumn;
bool CreateItemTableFromRes(MYSQL_RES* res, std::vector<TPlayerItem>* pVec, DWORD dwPID);

DWORD g_dwUsageMax = 0;
DWORD g_dwUsageAvg = 0;

CPacketInfo g_query_info;
CPacketInfo g_item_info;

int g_item_count = 0;
int g_query_count[2];

CClientManager::CClientManager() :
	m_pkAuthPeer(NULL),
	m_iPlayerIDStart(0),
	m_iPlayerDeleteLevelLimit(0),
	m_iPlayerDeleteLevelLimitLower(0),
	m_bChinaEventServer(false),
	m_iShopTableSize(0),
	m_pShopTable(NULL),
	m_iRefineTableSize(0),
	m_pRefineTable(NULL),
	m_bShutdowned(false),
	m_iCacheFlushCount(0),
	m_iCacheFlushCountLimit(200)
#if defined(__EXPRESSING_EMOTIONS__)
	, m_iEmoteDumpDelay(3600)
#endif
#if defined(__MAILBOX__)
	, m_iMailBoxBackupSec(3600)
#endif
#if defined(__GEM_SHOP__)
	, m_iGemShopFlushDelay(3600)
#endif
{
	m_itemRange.dwMin = 0;
	m_itemRange.dwMax = 0;
	m_itemRange.dwUsableItemIDMin = 0;

	memset(g_query_count, 0, sizeof(g_query_count));
}

CClientManager::~CClientManager()
{
	Destroy();
}

void CClientManager::SetPlayerIDStart(int iIDStart)
{
	m_iPlayerIDStart = iIDStart;
}

void CClientManager::Destroy()
{
	m_mChannelStatus.clear();
	//for (TPeerList::const_iterator i = m_peerList.begin(); i != m_peerList.end(); ++i)
	for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
		(*i)->Destroy();

	m_peerList.clear();

	if (m_fdAccept > 0)
	{
		socket_close(m_fdAccept);
		m_fdAccept = -1;
	}
}

bool CClientManager::Initialize()
{
	int tmpValue;

	// BOOT_LOCALIZATION
	if (!InitializeLocalization())
	{
		fprintf(stderr, "Failed Localization Infomation so exit\n");
		return false;
	}
	// END_BOOT_LOCALIZATION

	// ITEM_UNIQUE_ID
	if (!InitializeNowItemID())
	{
		fprintf(stderr, " Item range Initialize Failed. Exit DBCache Server\n");
		return false;
	}
	// END_ITEM_UNIQUE_ID

	m_bWolfmanCharacter = true;
	int iWolfmanCharacter = 0;
	if (CConfig::instance().GetValue("WOLFMAN_CHARACTER", &iWolfmanCharacter))
		m_bWolfmanCharacter = (iWolfmanCharacter);
	sys_log(0, "WOLFMAN_CHARACTER %s", m_bWolfmanCharacter ? "true" : "false");

	m_bDelayedCharacterCreation = true;
	int iDelayedCharacterCreation = 0;
	if (CConfig::instance().GetValue("DELAYED_CHARACTER_CREATION", &iDelayedCharacterCreation))
		m_bDelayedCharacterCreation = (iDelayedCharacterCreation);
	sys_log(0, "DELAYED_CHARACTER_CREATION %s", m_bDelayedCharacterCreation ? "true" : "false");

	if (!InitializeTables())
	{
		sys_err("Table Initialize FAILED");
		return false;
	}

	CGuildManager::instance().BootReserveWar();

	if (!CConfig::instance().GetValue("BIND_PORT", &tmpValue))
		tmpValue = 5300;

	char szBindIP[128];

	if (!CConfig::instance().GetValue("BIND_IP", szBindIP, 128))
		strlcpy(szBindIP, "0", sizeof(szBindIP));

	m_fdAccept = socket_tcp_bind(szBindIP, tmpValue);

	if (m_fdAccept < 0)
	{
		perror("socket");
		return false;
	}

	sys_log(0, "ACCEPT_HANDLE: %u", m_fdAccept);
	fdwatch_add_fd(m_fdWatcher, m_fdAccept, NULL, FDW_READ, false);

#if defined(__EXPRESSING_EMOTIONS__)
	CConfig::instance().GetValue("EMOTE_DUMP_DELAY", &m_iEmoteDumpDelay);
#endif

#if defined(__MAILBOX__)
	CConfig::instance().GetValue("MAILBOX_BACKUP_SEC", &m_iMailBoxBackupSec);
#endif

#if defined(__GEM_SHOP__)
	CConfig::instance().GetValue("GEM_SHOP_FLUSH_DELAY", &m_iGemShopFlushDelay);
#endif

	if (!CConfig::instance().GetValue("BACKUP_LIMIT_SEC", &tmpValue))
		tmpValue = 600;

	m_looping = true;

	if (!CConfig::instance().GetValue("PLAYER_DELETE_LEVEL_LIMIT", &m_iPlayerDeleteLevelLimit))
	{
		sys_err("config.txt: Cannot find PLAYER_DELETE_LEVEL_LIMIT, use default level %d", PLAYER_MAX_LEVEL_CONST + 1);
		m_iPlayerDeleteLevelLimit = PLAYER_MAX_LEVEL_CONST + 1;
	}

	if (!CConfig::instance().GetValue("PLAYER_DELETE_LEVEL_LIMIT_LOWER", &m_iPlayerDeleteLevelLimitLower))
	{
		m_iPlayerDeleteLevelLimitLower = 0;
	}

	sys_log(0, "PLAYER_DELETE_LEVEL_LIMIT set to %d", m_iPlayerDeleteLevelLimit);
	sys_log(0, "PLAYER_DELETE_LEVEL_LIMIT_LOWER set to %d", m_iPlayerDeleteLevelLimitLower);

	m_bChinaEventServer = false;

	int iChinaEventServer = 0;

	if (CConfig::instance().GetValue("CHINA_EVENT_SERVER", &iChinaEventServer))
		m_bChinaEventServer = (iChinaEventServer);

	sys_log(0, "CHINA_EVENT_SERVER %s", CClientManager::instance().IsChinaEventServer() ? "true" : "false");

	LoadEventFlag();
#if defined(__GUILD_EVENT_FLAG__) //&& defined(__GUILD_RENEWAL__)
	LoadGuildEventFlag();
#endif

	// database character-set을 강제로 맞춤
	if (g_stLocale == "big5" || g_stLocale == "sjis")
		CDBManager::instance().QueryLocaleSet();

	return true;
}

void CClientManager::MainLoop()
{
	SQLMsg* tmp;

	sys_log(0, "ClientManager pointer is %p", this);

	// 메인루프
	while (!m_bShutdowned)
	{
		while ((tmp = CDBManager::instance().PopResult()))
		{
			AnalyzeQueryResult(tmp);
			delete tmp;
		}

		if (!Process())
			break;

		log_rotate();
	}

	//
	// 메인루프 종료처리
	//
	sys_log(0, "MainLoop exited, Starting cache flushing");

#if defined(__EXPRESSING_EMOTIONS__)
	CClientManager::instance().QUERY_EMOTE_DUMP();
#endif

#if defined(__MAILBOX__)
	CClientManager::instance().MAILBOX_BACKUP();
#endif

#if defined(__GEM_SHOP__)
	// Gem Shop Item Data List Flush
	CClientManager::instance().FlushGemShop();
#endif

	signal_timer_disable();

	// 플레이어 테이블 캐쉬 플러쉬
	auto it = m_map_playerCache.begin();
	while (it != m_map_playerCache.end())
	{
		CPlayerTableCache* c = (it++)->second;

		c->Flush();
		delete c;
	}
	m_map_playerCache.clear();

	// 아이템 플러쉬
	auto it2 = m_map_itemCache.begin();
	while (it2 != m_map_itemCache.end())
	{
		CItemCache* c = (it2++)->second;

		c->Flush();
		delete c;
	}
	m_map_itemCache.clear();

	// MYSHOP_PRICE_LIST
	//
	// 개인상점 아이템 가격 리스트 Flush
	//
	for (auto itPriceList = m_mapItemPriceListCache.begin(); itPriceList != m_mapItemPriceListCache.end(); ++itPriceList)
	{
		CItemPriceListTableCache* pCache = itPriceList->second;
		pCache->Flush();
		delete pCache;
	}

	m_mapItemPriceListCache.clear();
	// END_OF_MYSHOP_PRICE_LIST
}

void CClientManager::Quit()
{
	m_bShutdowned = true;
}

void CClientManager::QUERY_BOOT(CPeer* peer, TPacketGDBoot* p)
{
	const BYTE bPacketVersion = 6; // BOOT 패킷이 바뀔때마다 번호를 올리도록 한다.

	std::vector<tAdminInfo> vAdmin;
	std::vector<std::string> vHost;

	__GetHostInfo(vHost);
	__GetAdminInfo(p->szIP, vAdmin);

	sys_log(0, "QUERY_BOOT : AdminInfo (Request ServerIp %s) ", p->szIP);

	DWORD dwPacketSize =
		sizeof(DWORD) +
		sizeof(BYTE) +
		sizeof(WORD) + sizeof(WORD) + sizeof(TMobTable) * m_vec_mobTable.size() +
		sizeof(WORD) + sizeof(WORD) + sizeof(TItemTable) * m_vec_itemTable.size() +
		sizeof(WORD) + sizeof(WORD) + sizeof(TShopTable) * m_iShopTableSize +
		sizeof(WORD) + sizeof(WORD) + sizeof(TSkillTable) * m_vec_skillTable.size() +
		sizeof(WORD) + sizeof(WORD) + sizeof(TRefineTable) * m_iRefineTableSize +
		sizeof(WORD) + sizeof(WORD) + sizeof(TItemAttrTable) * m_vec_itemAttrTable.size() +
		sizeof(WORD) + sizeof(WORD) + sizeof(TItemAttrTable) * m_vec_itemRareTable.size() +
		sizeof(WORD) + sizeof(WORD) + sizeof(TBanwordTable) * m_vec_banwordTable.size() +
		sizeof(WORD) + sizeof(WORD) + sizeof(building::TLand) * m_vec_kLandTable.size() +
		sizeof(WORD) + sizeof(WORD) + sizeof(building::TObjectProto) * m_vec_kObjectProto.size() +
		sizeof(WORD) + sizeof(WORD) + sizeof(building::TObject) * m_map_pkObjectTable.size() +

		sizeof(time_t) +
		sizeof(WORD) + sizeof(WORD) + sizeof(TItemIDRangeTable) * 2 +
		// ADMIN_MANAGER
		sizeof(WORD) + sizeof(WORD) + 16 * vHost.size() +
		sizeof(WORD) + sizeof(WORD) + sizeof(tAdminInfo) * vAdmin.size() +
		// END_ADMIN_MANAGER
		sizeof(WORD) + sizeof(WORD) + sizeof(TMonarchInfo) +
		sizeof(WORD) + sizeof(WORD) + sizeof(MonarchCandidacy) * CMonarch::instance().MonarchCandidacySize() +
		sizeof(WORD);

	peer->EncodeHeader(HEADER_DG_BOOT, 0, dwPacketSize);
	peer->Encode(&dwPacketSize, sizeof(DWORD));
	peer->Encode(&bPacketVersion, sizeof(BYTE));

	sys_log(0, "BOOT: PACKET: %d", dwPacketSize);
	sys_log(0, "BOOT: VERSION: %d", bPacketVersion);

	sys_log(0, "sizeof(TMobTable) = %d", sizeof(TMobTable));
	sys_log(0, "sizeof(TItemTable) = %d", sizeof(TItemTable));
	sys_log(0, "sizeof(TShopTable) = %d", sizeof(TShopTable));
	sys_log(0, "sizeof(TSkillTable) = %d", sizeof(TSkillTable));
	sys_log(0, "sizeof(TRefineTable) = %d", sizeof(TRefineTable));
	sys_log(0, "sizeof(TItemAttrTable) = %d", sizeof(TItemAttrTable));
	sys_log(0, "sizeof(TItemRareTable) = %d", sizeof(TItemAttrTable));
	sys_log(0, "sizeof(TBanwordTable) = %d", sizeof(TBanwordTable));
	sys_log(0, "sizeof(TLand) = %d", sizeof(building::TLand));
	sys_log(0, "sizeof(TObjectProto) = %d", sizeof(building::TObjectProto));
	sys_log(0, "sizeof(TObject) = %d", sizeof(building::TObject));
	// ADMIN_MANAGER
	sys_log(0, "sizeof(tAdminInfo) = %d * %d ", sizeof(tAdminInfo) * vAdmin.size());
	// END_ADMIN_MANAGER
	sys_log(0, "sizeof(TMonarchInfo) = %d * %d", sizeof(TMonarchInfo));

	peer->EncodeWORD(sizeof(TMobTable));
	peer->EncodeWORD(static_cast<WORD>(m_vec_mobTable.size()));
	peer->Encode(&m_vec_mobTable[0], sizeof(TMobTable) * m_vec_mobTable.size());

	peer->EncodeWORD(sizeof(TItemTable));
	peer->EncodeWORD(static_cast<WORD>(m_vec_itemTable.size()));
	peer->Encode(&m_vec_itemTable[0], sizeof(TItemTable) * m_vec_itemTable.size());

	peer->EncodeWORD(sizeof(TShopTable));
	peer->EncodeWORD(m_iShopTableSize);
	peer->Encode(m_pShopTable, sizeof(TShopTable) * m_iShopTableSize);

	peer->EncodeWORD(sizeof(TSkillTable));
	peer->EncodeWORD(static_cast<WORD>(m_vec_skillTable.size()));
	peer->Encode(&m_vec_skillTable[0], sizeof(TSkillTable) * m_vec_skillTable.size());

	peer->EncodeWORD(sizeof(TRefineTable));
	peer->EncodeWORD(m_iRefineTableSize);
	peer->Encode(m_pRefineTable, sizeof(TRefineTable) * m_iRefineTableSize);

	peer->EncodeWORD(sizeof(TItemAttrTable));
	peer->EncodeWORD(static_cast<WORD>(m_vec_itemAttrTable.size()));
	peer->Encode(&m_vec_itemAttrTable[0], sizeof(TItemAttrTable) * m_vec_itemAttrTable.size());

	peer->EncodeWORD(sizeof(TItemAttrTable));
	peer->EncodeWORD(static_cast<WORD>(m_vec_itemRareTable.size()));
	peer->Encode(&m_vec_itemRareTable[0], sizeof(TItemAttrTable) * m_vec_itemRareTable.size());

	peer->EncodeWORD(sizeof(TBanwordTable));
	peer->EncodeWORD(static_cast<WORD>(m_vec_banwordTable.size()));
	peer->Encode(&m_vec_banwordTable[0], sizeof(TBanwordTable) * m_vec_banwordTable.size());

	peer->EncodeWORD(sizeof(building::TLand));
	peer->EncodeWORD(static_cast<WORD>(m_vec_kLandTable.size()));
	peer->Encode(&m_vec_kLandTable[0], sizeof(building::TLand) * m_vec_kLandTable.size());

	peer->EncodeWORD(sizeof(building::TObjectProto));
	peer->EncodeWORD(static_cast<WORD>(m_vec_kObjectProto.size()));
	peer->Encode(&m_vec_kObjectProto[0], sizeof(building::TObjectProto) * m_vec_kObjectProto.size());

	peer->EncodeWORD(sizeof(building::TObject));
	peer->EncodeWORD(static_cast<WORD>(m_map_pkObjectTable.size()));

	auto it = m_map_pkObjectTable.begin();
	while (it != m_map_pkObjectTable.end())
		peer->Encode((it++)->second, sizeof(building::TObject));

	time_t now = time(0);
	peer->Encode(&now, sizeof(time_t));

	TItemIDRangeTable itemRange = CItemIDRangeManager::instance().GetRange();
	TItemIDRangeTable itemRangeSpare = CItemIDRangeManager::instance().GetRange();

	peer->EncodeWORD(sizeof(TItemIDRangeTable));
	peer->EncodeWORD(1);
	peer->Encode(&itemRange, sizeof(TItemIDRangeTable));
	peer->Encode(&itemRangeSpare, sizeof(TItemIDRangeTable));

	peer->SetItemIDRange(itemRange);
	peer->SetSpareItemIDRange(itemRangeSpare);

	// ADMIN_MANAGER
	peer->EncodeWORD(16);
	peer->EncodeWORD(static_cast<WORD>(vHost.size()));

	for (size_t n = 0; n < vHost.size(); ++n)
	{
		peer->Encode(vHost[n].c_str(), 16);
		sys_log(0, "GMHosts %s", vHost[n].c_str());
	}

	peer->EncodeWORD(sizeof(tAdminInfo));
	peer->EncodeWORD(static_cast<WORD>(vAdmin.size()));

	for (size_t n = 0; n < vAdmin.size(); ++n)
	{
		peer->Encode(&vAdmin[n], sizeof(tAdminInfo));
		sys_log(0, "Admin name %s ConntactIP %s", vAdmin[n].m_szName, vAdmin[n].m_szContactIP);
	}
	// END_ADMIN_MANAGER

	// MONARCH
	peer->EncodeWORD(sizeof(TMonarchInfo));
	peer->EncodeWORD(1);
	peer->Encode(CMonarch::instance().GetMonarch(), sizeof(TMonarchInfo));

	CMonarch::VEC_MONARCHCANDIDACY& rVecMonarchCandidacy = CMonarch::instance().GetVecMonarchCandidacy();

	size_t num_monarch_candidacy = CMonarch::instance().MonarchCandidacySize();
	peer->EncodeWORD(sizeof(MonarchCandidacy));
	peer->EncodeWORD(static_cast<WORD>(num_monarch_candidacy));
	if (num_monarch_candidacy != 0)
	{
		peer->Encode(&rVecMonarchCandidacy[0], sizeof(MonarchCandidacy) * num_monarch_candidacy);
	}
	// END_MONARCE

	if (g_test_server)
		sys_log(0, "MONARCHCandidacy Size %d", CMonarch::instance().MonarchCandidacySize());

	peer->EncodeWORD(0xffff);
}

void CClientManager::SendPartyOnSetup(CPeer* pkPeer)
{
	TPartyMap& pm = m_map_pkChannelParty[pkPeer->GetChannel()];
	for (auto it_party = pm.begin(); it_party != pm.end(); ++it_party)
	{
		sys_log(0, "PARTY SendPartyOnSetup Party [%u]", it_party->first);
		pkPeer->EncodeHeader(HEADER_DG_PARTY_CREATE, 0, sizeof(TPacketPartyCreate));
		pkPeer->Encode(&it_party->first, sizeof(DWORD));

		for (auto it_member = it_party->second.begin(); it_member != it_party->second.end(); ++it_member)
		{
			sys_log(0, "PARTY SendPartyOnSetup Party [%u] Member [%u]", it_party->first, it_member->first);
			pkPeer->EncodeHeader(HEADER_DG_PARTY_ADD, 0, sizeof(TPacketPartyAdd));
			pkPeer->Encode(&it_party->first, sizeof(DWORD));
			pkPeer->Encode(&it_member->first, sizeof(DWORD));
			pkPeer->Encode(&it_member->second.bRole, sizeof(BYTE));

			pkPeer->EncodeHeader(HEADER_DG_PARTY_SET_MEMBER_LEVEL, 0, sizeof(TPacketPartySetMemberLevel));
			pkPeer->Encode(&it_party->first, sizeof(DWORD));
			pkPeer->Encode(&it_member->first, sizeof(DWORD));
			pkPeer->Encode(&it_member->second.bLevel, sizeof(BYTE));
		}
	}
}

void CClientManager::QUERY_PLAYER_COUNT(CPeer* pkPeer, TPlayerCountPacket* pPacket)
{
	pkPeer->SetUserCount(pPacket->dwCount);
}

void CClientManager::QUERY_QUEST_SAVE(CPeer* pkPeer, TQuestTable* pTable, DWORD dwLen)
{
	if (0 != (dwLen % sizeof(TQuestTable)))
	{
		sys_err("invalid packet size %d, sizeof(TQuestTable) == %d", dwLen, sizeof(TQuestTable));
		return;
	}

	int iSize = dwLen / sizeof(TQuestTable);

	char szQuery[1024];

	for (int i = 0; i < iSize; ++i, ++pTable)
	{
		if (pTable->lValue == 0)
		{
			snprintf(szQuery, sizeof(szQuery), "DELETE FROM quest%s WHERE `dwPID` = %d AND `szName` = '%s' AND `szState` ='%s'",
				GetTablePostfix(), pTable->dwPID, pTable->szName, pTable->szState);
		}
		else
		{
			snprintf(szQuery, sizeof(szQuery), "REPLACE INTO quest%s (`dwPID`, `szName`, `szState`, `lValue`) VALUES(%d, '%s', '%s', %ld)",
				GetTablePostfix(), pTable->dwPID, pTable->szName, pTable->szState, pTable->lValue);
		}

		CDBManager::instance().ReturnQuery(szQuery, QID_QUEST_SAVE, pkPeer->GetHandle(), NULL);
	}
}

void CClientManager::QUERY_SAFEBOX_LOAD(CPeer* pkPeer, DWORD dwHandle, TSafeboxLoadPacket* packet, bool bMall)
{
	ClientHandleInfo* pi = new ClientHandleInfo(dwHandle);
	strlcpy(pi->safebox_password, packet->szPassword, sizeof(pi->safebox_password));
	pi->account_id = packet->dwID;
	pi->account_index = 0;
	pi->ip[0] = bMall ? 1 : 0;
	strlcpy(pi->login, packet->szLogin, sizeof(pi->login));

	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery),
		"SELECT `account_id`, `size`, `password` FROM safebox%s WHERE `account_id` = %u",
		GetTablePostfix(), packet->dwID);

	if (g_log)
		sys_log(0, "HEADER_GD_SAFEBOX_LOAD (handle: %d account.id %u is_mall %d)", dwHandle, packet->dwID, bMall ? 1 : 0);

	CDBManager::instance().ReturnQuery(szQuery, QID_SAFEBOX_LOAD, pkPeer->GetHandle(), pi);
}

void CClientManager::RESULT_SAFEBOX_LOAD(CPeer* pkPeer, SQLMsg* msg)
{
	sys_log(0, "RESULT_SAFEBOX_LOAD");

	CQueryInfo* qi = (CQueryInfo*)msg->pvUserData;
	ClientHandleInfo* pi = (ClientHandleInfo*)qi->pvData;
	DWORD dwHandle = pi->dwHandle;

	// 여기에서 사용하는 account_index는 쿼리 순서를 말한다.
	// 첫번째 패스워드 알아내기 위해 하는 쿼리가 0
	// 두번째 실제 데이터를 얻어놓는 쿼리가 1

	if (pi->account_index == 0)
	{
		sys_log(0, "RESULT_SAFEBOX_LOAD account_index == 0");

		char szSafeboxPassword[SAFEBOX_PASSWORD_MAX_LEN + 1];
		strlcpy(szSafeboxPassword, pi->safebox_password, sizeof(szSafeboxPassword));

		TSafeboxTable* pSafebox = new TSafeboxTable;
		memset(pSafebox, 0, sizeof(TSafeboxTable));

		SQLResult* res = msg->Get();

		if (res->uiNumRows == 0)
		{
			if (strcmp("000000", szSafeboxPassword))
			{
				pkPeer->EncodeHeader(HEADER_DG_SAFEBOX_WRONG_PASSWORD, dwHandle, 0);
				delete pSafebox;
				delete pi;
				return;
			}
		}
		else
		{
			sys_log(0, "RESULT_SAFEBOX_LOAD get rows");

			MYSQL_ROW row = mysql_fetch_row(res->pSQLResult);

			// 비밀번호가 틀리면..
			if (((!row[2] || !*row[2]) && strcmp("000000", szSafeboxPassword)) ||
				((row[2] && *row[2]) && strcmp(row[2], szSafeboxPassword)))
			{
				pkPeer->EncodeHeader(HEADER_DG_SAFEBOX_WRONG_PASSWORD, dwHandle, 0);
				delete pSafebox;
				delete pi;
				return;
			}

			if (!row[0])
				pSafebox->dwID = 0;
			else
				str_to_number(pSafebox->dwID, row[0]);

			if (!row[1])
				pSafebox->bSize = 0;
			else
				str_to_number(pSafebox->bSize, row[1]);
			/*
			if (!row[3])
				pSafebox->dwGold = 0;
			else
				pSafebox->dwGold = atoi(row[3]);
			*/
			if (pi->ip[0] == 1)
			{
#if defined(__EXTEND_MALLBOX__)
				pSafebox->bSize = 5;
#else
				pSafebox->bSize = 1;
#endif
				sys_log(0, "MALL id[%d] size[%d]", pSafebox->dwID, pSafebox->bSize);
			}
			else
				sys_log(0, "SAFEBOX id[%d] size[%d]", pSafebox->dwID, pSafebox->bSize);
		}

		if (0 == pSafebox->dwID)
			pSafebox->dwID = pi->account_id;

		pi->pSafebox = pSafebox;

		char szQuery[QUERY_MAX_LEN];
		snprintf(szQuery, sizeof(szQuery),
			"SELECT `id`, `window`+0, `pos`, `vnum`, `count`"
#if defined(__SOUL_BIND_SYSTEM__)
			", `soulbind`"
#endif
			", `socket0`, `socket1`, `socket2`"
#if defined(__ITEM_SOCKET6__)
			", `socket3`, `socket4`, `socket5`"
#endif
			", `attrtype0`, `attrvalue0`"
			", `attrtype1`, `attrvalue1`"
			", `attrtype2`, `attrvalue2`"
			", `attrtype3`, `attrvalue3`"
			", `attrtype4`, `attrvalue4`"
			", `attrtype5`, `attrvalue5`"
			", `attrtype6`, `attrvalue6`"
#if defined(__CHANGE_LOOK_SYSTEM__)
			", `transmutation`"
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
			", `refine_element_apply_type`"
			", `refine_element_grade`"
			", `refine_element_value0`, `refine_element_value1`, `refine_element_value2`"
			", `refine_element_bonus_value0`, `refine_element_bonus_value1`, `refine_element_bonus_value2`"
#endif
#if defined(__ITEM_APPLY_RANDOM__)
			", `apply_type0`, `apply_value0`, `apply_path0`"
			", `apply_type1`, `apply_value1`, `apply_path1`"
			", `apply_type2`, `apply_value2`, `apply_path2`"
			", `apply_type3`, `apply_value3`, `apply_path3`"
#endif
#if defined(__SET_ITEM__)
			", `set_value`"
#endif
			" FROM `item%s` WHERE `owner_id` = %u AND `window` = '%s'",
			GetTablePostfix(), pi->account_id, pi->ip[0] == 0 ? "SAFEBOX" : "MALL");

		pi->account_index = 1;

		CDBManager::instance().ReturnQuery(szQuery, QID_SAFEBOX_LOAD, pkPeer->GetHandle(), pi);
	}
	else
	{
		sys_log(0, "RESULT_SAFEBOX_LOAD YES!!!!!!!!!!!");

		if (!pi->pSafebox)
		{
			sys_err("null safebox pointer!");
			delete pi;
			return;
		}

		// 쿼리에 에러가 있었으므로 응답할 경우 창고가 비어있는 것 처럼
		// 보이기 때문에 창고가 아얘 안열리는게 나음
		if (!msg->Get()->pSQLResult)
		{
			sys_err("null safebox result");
			delete pi;
			return;
		}

		static std::vector<TPlayerItem> s_items;
		CreateItemTableFromRes(msg->Get()->pSQLResult, &s_items, pi->account_id);

		ItemAwardSet* pSet = ItemAwardManager::instance().GetByLogin(pi->login);

		if (pSet && !m_vec_itemTable.empty())
		{
#if defined(__EXTEND_MALLBOX__)
			CGrid grid(5, MAX(1, 5) * 9);
#else
			CGrid grid(5, MAX(1, pi->pSafebox->bSize) * 9);
#endif
			bool bEscape = false;

			for (DWORD i = 0; i < s_items.size(); ++i)
			{
				TPlayerItem& r = s_items[i];

				auto it = m_map_itemTableByVnum.find(r.dwVnum);
				if (it == m_map_itemTableByVnum.end())
				{
					bEscape = true;
					sys_err("invalid item vnum %u in safebox: login %s", r.dwVnum, pi->login);
					break;
				}

				grid.Put(r.wPos, 1, it->second->bSize);
			}

			if (!bEscape)
			{
				std::vector<std::pair<DWORD, DWORD> > vec_dwFinishedAwardID;
				ItemAwardSet::const_iterator it = pSet->begin();
				char szQuery[512];

				while (it != pSet->end())
				{
					TItemAward* pItemAward = *(it++);
					const DWORD& dwItemVnum = pItemAward->dwVnum;

					if (pItemAward->bTaken)
						continue;

					if (pi->ip[0] == 0 && pItemAward->bMall)
						continue;

					if (pi->ip[0] == 1 && !pItemAward->bMall)
						continue;

					auto it = m_map_itemTableByVnum.find(pItemAward->dwVnum);
					if (it == m_map_itemTableByVnum.end())
					{
						sys_err("invalid item vnum %u in item_award: login %s", pItemAward->dwVnum, pi->login);
						continue;
					}

					TItemTable* pItemTable = it->second;

					int iPos;

					if ((iPos = grid.FindBlank(1, it->second->bSize)) == -1)
						break;

					TPlayerItem item;
					memset(&item, 0, sizeof(TPlayerItem));

#if defined(__EXTENDED_ITEM_AWARD__)
					DWORD dwSocket0 = pItemAward->dwSocket0;
					DWORD dwSocket2 = pItemAward->dwSocket2;
					DWORD dwSocket4 = pItemAward->dwSocket4;
					DWORD dwSocket5 = pItemAward->dwSocket5;
#else
					DWORD dwSocket2 = 0;
#endif

					if (pItemTable->bType == ITEM_UNIQUE)
					{
#if defined(__EXTENDED_ITEM_AWARD__)
						// 12.04.2019 - Correction for unique items based on the real time.
						const long lValue0 = pItemTable->alValues[ITEM_SOCKET_REMAIN_SEC];
						const long lValue2 = pItemTable->alValues[ITEM_SOCKET_UNIQUE_REMAIN_TIME];
						const time_t tNow = CClientManager::instance().GetCurrentTime();
						dwSocket2 = (lValue2 == 0) ? static_cast<DWORD>(lValue0) : static_cast<DWORD>(tNow + lValue0);
#else
						if (pItemAward->dwSocket2 != 0)
							dwSocket2 = pItemAward->dwSocket2;
						else
							dwSocket2 = pItemTable->alValues[0];
#endif
					}
					else if ((dwItemVnum == ITEM_SKILLBOOK_VNUM || dwItemVnum == ITEM_SKILLFORGET_VNUM) && pItemAward->dwSocket0 == 0)
					{
						DWORD dwSkillIdx;
						DWORD dwSkillVnum;

						do
						{
							dwSkillIdx = number(0, m_vec_skillTable.size() - 1);

							dwSkillVnum = m_vec_skillTable[dwSkillIdx].dwVnum;

							if (dwSkillVnum > 120)
								continue;

							break;
						} while (1);

#if !defined(__EXTENDED_ITEM_AWARD__)
						pItemAward->dwSocket0 = dwSkillVnum;
#endif
					}
					else
					{
						switch (dwItemVnum)
						{
							case 72723: case 72724: case 72725: case 72726:
							case 72727: case 72728: case 72729: case 72730:
								// 무시무시하지만 이전에 하던 걸 고치기는 무섭고...
								// 그래서 그냥 하드 코딩. 선물 상자용 자동물약 아이템들.
							case 76004: case 76005: case 76021: case 76022:
							case 79012: case 79013:
								if (pItemAward->dwSocket2 == 0)
								{
									dwSocket2 = pItemTable->alValues[0];
								}
								else
								{
									dwSocket2 = pItemAward->dwSocket2;
								}
								break;
						}
					}

					if (GetItemID() > m_itemRange.dwMax)
					{
						sys_err("UNIQUE ID OVERFLOW!!");
						break;
					}

					{
						auto it = m_map_itemTableByVnum.find(dwItemVnum);
						if (it == m_map_itemTableByVnum.end())
						{
							sys_err("Invalid item(vnum : %d). It is not in m_map_itemTableByVnum.", dwItemVnum);
							continue;
						}
						TItemTable* item_table = it->second;
						if (item_table == NULL)
						{
							sys_err("Invalid item_table (vnum : %d). It's value is NULL in m_map_itemTableByVnum.", dwItemVnum);
							continue;
						}

#if defined(__MOUNT_COSTUME_SYSTEM__)
						if ((item_table->bType == ITEM_UNIQUE && item_table->bType == COSTUME_MOUNT))
						{
							if (pItemAward->dwSocket4 == 0)
								dwSocket4 = pItemTable->alValues[0];
							else
								dwSocket4 = pItemAward->dwSocket4;
						}
#endif

						if (0 == pItemAward->dwSocket0)
						{
							// Load default values
							for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++)
							{
								if (LIMIT_REAL_TIME == item_table->aLimits[i].bType)
								{
									if (0 == item_table->aLimits[i].lValue)
										pItemAward->dwSocket0 = time(0) + 60 * 60 * 24 * 7;
									else
										pItemAward->dwSocket0 = time(0) + item_table->aLimits[i].lValue;
									break;
								}
								else if (LIMIT_REAL_TIME_START_FIRST_USE == item_table->aLimits[i].bType || LIMIT_TIMER_BASED_ON_WEAR == item_table->aLimits[i].bType)
								{
									if (0 == item_table->aLimits[i].lValue)
										pItemAward->dwSocket0 = 60 * 60 * 24 * 7;
									else
										pItemAward->dwSocket0 = item_table->aLimits[i].lValue;
									break;
								}
							}
						}
						else
						{
#if defined(__EXTENDED_ITEM_AWARD__)
							// Load set values
							for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++)
							{
								if (LIMIT_REAL_TIME == item_table->aLimits[i].bType)
								{
									dwSocket0 = time(0) + pItemAward->dwSocket0;
									break;
								}
								else if (LIMIT_REAL_TIME_START_FIRST_USE == item_table->aLimits[i].bType || LIMIT_TIMER_BASED_ON_WEAR == item_table->aLimits[i].bType)
								{
									dwSocket0 = pItemAward->dwSocket0;
									break;
								}
							}
#else
							for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++)
							{
								if (LIMIT_REAL_TIME == item_table->aLimits[i].bType)
								{
									if (0 == item_table->aLimits[i].lValue)
										pItemAward->dwSocket0 = time(0) + 60 * 60 * 24 * 7;
									else
										pItemAward->dwSocket0 = time(0) + item_table->aLimits[i].lValue;

									break;
								}
								else if (LIMIT_REAL_TIME_START_FIRST_USE == item_table->aLimits[i].bType || LIMIT_TIMER_BASED_ON_WEAR == item_table->aLimits[i].bType)
								{
									if (0 == item_table->aLimits[i].lValue)
										pItemAward->dwSocket0 = 60 * 60 * 24 * 7;
									else
										pItemAward->dwSocket0 = item_table->aLimits[i].lValue;

									break;
								}
							}
#endif
						}

#if defined(__EXTENDED_ITEM_AWARD__)
						ItemAwardManager::instance().CheckItemCount(*pItemAward, *pItemTable);
						ItemAwardManager::instance().CheckItemBlend(*pItemAward, *pItemTable);
						ItemAwardManager::instance().CheckItemAddonType(*pItemAward, *pItemTable);
						ItemAwardManager::instance().CheckItemSkillBook(*pItemAward, m_vec_skillTable);
						ItemAwardManager::instance().CheckItemAttributes(*pItemAward, *pItemTable, m_vec_itemAttrTable);

						// START_OF_AUTO_QUERY
						char szColumns[QUERY_MAX_LEN], szValues[QUERY_MAX_LEN];

						int iLen = snprintf(szColumns, sizeof(szColumns), "`id`, `owner_id`, `window`, `pos`, `vnum`, `count`");
						int iValueLen = snprintf(szValues, sizeof(szValues), "%u, %u, '%s', %d, %u, %u", GainItemID(), pi->account_id, (pi->ip[0] == 0) ? "SAFEBOX" : "MALL", iPos, pItemAward->dwVnum, pItemAward->dwCount);

#if defined(__ITEM_SOCKET6__)
						iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen, ", `socket0`, `socket1`, `socket2`, `socket3`, `socket4`, `socket5`");
						iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen, ", %u, %u, %u, %u, %u, %u", dwSocket0, pItemAward->dwSocket1, dwSocket2, pItemAward->dwSocket3, pItemAward->dwSocket4, pItemAward->dwSocket5);
#else
						iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen, ", `socket0`, `socket1`, `socket2`");
						iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen, ", %u, %u, %u", pItemAward->dwSocket0, pItemAward->dwSocket1, dwSocket2);
#endif

						for (size_t i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
						{
							iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen, ", `attrtype%d`, `attrvalue%d`", i, i);
							iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen, ", %u, %ld", pItemAward->aAttr[i].wType, pItemAward->aAttr[i].lValue);
						}
						// END_OF_AUTO_QUERY

						snprintf(szQuery, sizeof(szQuery), "INSERT INTO item%s (%s) VALUES(%s)", GetTablePostfix(), szColumns, szValues);
#else
						snprintf(szQuery, sizeof(szQuery),
							"INSERT INTO item%s (`id`, `owner_id`, `window`, `pos`, `vnum`, `count`"
							", `socket0`"
							", `socket1`"
							", `socket2`"
#if defined(__ITEM_SOCKET6__)
							", `socket3`"
							", `socket4`"
							", `socket5`"
#endif
							") VALUES (%u, %u, '%s', %d, %u, %u"
							", %u"
							", %u"
							", %u"
#if defined(__ITEM_SOCKET6__)
							", %u"
							", %u"
							", %u"
#endif
							")",
							GetTablePostfix(), GainItemID(), pi->account_id, pi->ip[0] == 0 ? "SAFEBOX" : "MALL", iPos, pItemAward->dwVnum, pItemAward->dwCount
							, pItemAward->dwSocket0
							, pItemAward->dwSocket1
							, dwSocket2
#if defined(__ITEM_SOCKET6__)
							, pItemAward->dwSocket3
							, pItemAward->dwSocket4
							, pItemAward->dwSocket5
#endif
						);
#endif
					}

					std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));
					SQLResult* pRes = pmsg->Get();
					sys_log(0, "SAFEBOX Query : [%s]", szQuery);

					if (pRes->uiAffectedRows == 0 || pRes->uiInsertID == 0 || pRes->uiAffectedRows == (uint32_t)-1)
						break;

					item.dwID = pmsg->Get()->uiInsertID;
					if (pi->ip[0] == 0)
						item.bWindow = SAFEBOX, item.wPos = iPos;
					else
						item.bWindow = MALL, item.wPos = iPos;
					item.dwVnum = pItemAward->dwVnum;
					item.dwCount = pItemAward->dwCount;
#if defined(__EXTENDED_ITEM_AWARD__)
					item.alSockets[0] = dwSocket0;
#else
					item.alSockets[0] = pItemAward->dwSocket0;
#endif
					item.alSockets[1] = pItemAward->dwSocket1;
					item.alSockets[2] = dwSocket2;
#if defined(__ITEM_SOCKET6__)
					item.alSockets[3] = pItemAward->dwSocket3;
					item.alSockets[4] = dwSocket4;
					item.alSockets[5] = dwSocket5;
#endif
#if defined(__EXTENDED_ITEM_AWARD__)
					thecore_memcpy(&item.aAttr, pItemAward->aAttr, sizeof(item.aAttr));
#endif
					s_items.push_back(item);

					vec_dwFinishedAwardID.push_back(std::make_pair(pItemAward->dwID, item.dwID));
					grid.Put(iPos, 1, it->second->bSize);
				}

				for (DWORD i = 0; i < vec_dwFinishedAwardID.size(); ++i)
					ItemAwardManager::instance().Taken(vec_dwFinishedAwardID[i].first, vec_dwFinishedAwardID[i].second);
			}
		}

		pi->pSafebox->wItemCount = static_cast<WORD>(s_items.size());

		if (pi->ip[0] == 0)
			pkPeer->EncodeHeader(HEADER_DG_SAFEBOX_LOAD, dwHandle, sizeof(TSafeboxTable) + sizeof(TPlayerItem) * s_items.size());
		else
			pkPeer->EncodeHeader(HEADER_DG_MALL_LOAD, dwHandle, sizeof(TSafeboxTable) + sizeof(TPlayerItem) * s_items.size());

		pkPeer->Encode(pi->pSafebox, sizeof(TSafeboxTable));

		if (!s_items.empty())
			pkPeer->Encode(&s_items[0], sizeof(TPlayerItem) * s_items.size());

		delete pi;
	}
}

void CClientManager::QUERY_SAFEBOX_CHANGE_SIZE(CPeer* pkPeer, DWORD dwHandle, TSafeboxChangeSizePacket* p)
{
	ClientHandleInfo* pi = new ClientHandleInfo(dwHandle);
	pi->account_index = p->bSize; // account_index를 사이즈로 임시로 사용

	char szQuery[QUERY_MAX_LEN];

	if (p->bSize == 1)
		snprintf(szQuery, sizeof(szQuery), "INSERT INTO safebox%s (`account_id`, `size`) VALUES(%u, %u)", GetTablePostfix(), p->dwID, p->bSize);
	else
		snprintf(szQuery, sizeof(szQuery), "UPDATE safebox%s SET `size` = %u WHERE `account_id` = %u", GetTablePostfix(), p->bSize, p->dwID);

	CDBManager::instance().ReturnQuery(szQuery, QID_SAFEBOX_CHANGE_SIZE, pkPeer->GetHandle(), pi);
}

void CClientManager::RESULT_SAFEBOX_CHANGE_SIZE(CPeer* pkPeer, SQLMsg* msg)
{
	CQueryInfo* qi = (CQueryInfo*)msg->pvUserData;
	ClientHandleInfo* p = (ClientHandleInfo*)qi->pvData;
	DWORD dwHandle = p->dwHandle;
	BYTE bSize = p->account_index;

	delete p;

	if (msg->Get()->uiNumRows > 0)
	{
		pkPeer->EncodeHeader(HEADER_DG_SAFEBOX_CHANGE_SIZE, dwHandle, sizeof(BYTE));
		pkPeer->EncodeBYTE(bSize);
	}
}

void CClientManager::QUERY_SAFEBOX_CHANGE_PASSWORD(CPeer* pkPeer, DWORD dwHandle, TSafeboxChangePasswordPacket* p)
{
	ClientHandleInfo* pi = new ClientHandleInfo(dwHandle);
	strlcpy(pi->safebox_password, p->szNewPassword, sizeof(pi->safebox_password));
	strlcpy(pi->login, p->szOldPassword, sizeof(pi->login));
	pi->account_id = p->dwID;

	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery), "SELECT `password` FROM safebox%s WHERE `account_id` = %u", GetTablePostfix(), p->dwID);

	CDBManager::instance().ReturnQuery(szQuery, QID_SAFEBOX_CHANGE_PASSWORD, pkPeer->GetHandle(), pi);
}

void CClientManager::RESULT_SAFEBOX_CHANGE_PASSWORD(CPeer* pkPeer, SQLMsg* msg)
{
	CQueryInfo* qi = (CQueryInfo*)msg->pvUserData;
	ClientHandleInfo* p = (ClientHandleInfo*)qi->pvData;
	DWORD dwHandle = p->dwHandle;

	if (msg->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);

		if ((row[0] && *row[0] && !strcasecmp(row[0], p->login)) || ((!row[0] || !*row[0]) && !strcmp("000000", p->login)))
		{
			char szQuery[QUERY_MAX_LEN];
			char escape_pwd[64];
			CDBManager::instance().EscapeString(escape_pwd, p->safebox_password, strlen(p->safebox_password));

			snprintf(szQuery, sizeof(szQuery), "UPDATE safebox%s SET `password` = '%s' WHERE `account_id` = %u", GetTablePostfix(), escape_pwd, p->account_id);

			CDBManager::instance().ReturnQuery(szQuery, QID_SAFEBOX_CHANGE_PASSWORD_SECOND, pkPeer->GetHandle(), p);
			return;
		}
	}

	delete p;

	// Wrong old password
	pkPeer->EncodeHeader(HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER, dwHandle, sizeof(BYTE));
	pkPeer->EncodeBYTE(0);
}

void CClientManager::RESULT_SAFEBOX_CHANGE_PASSWORD_SECOND(CPeer* pkPeer, SQLMsg* msg)
{
	CQueryInfo* qi = (CQueryInfo*)msg->pvUserData;
	ClientHandleInfo* p = (ClientHandleInfo*)qi->pvData;
	DWORD dwHandle = p->dwHandle;
	delete p;

	pkPeer->EncodeHeader(HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER, dwHandle, sizeof(BYTE));
	pkPeer->EncodeBYTE(1);
}

// MYSHOP_PRICE_LIST
void CClientManager::RESULT_PRICELIST_LOAD(CPeer* peer, SQLMsg* pMsg)
{
	TItemPricelistReqInfo* pReqInfo = (TItemPricelistReqInfo*)static_cast<CQueryInfo*>(pMsg->pvUserData)->pvData;

	//
	// DB 에서 로드한 정보를 Cache 에 저장
	//

	TItemPriceListTable table;
	table.dwOwnerID = pReqInfo->second;
	table.byCount = 0;

	MYSQL_ROW row;

	while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
	{
		str_to_number(table.aPriceInfo[table.byCount].dwVnum, row[0]);
		str_to_number(table.aPriceInfo[table.byCount].dwPrice, row[1]);
#if defined(__CHEQUE_SYSTEM__)
		str_to_number(table.aPriceInfo[table.byCount].dwCheque, row[2]);
#endif
		table.byCount++;
	}

	PutItemPriceListCache(&table);

	//
	// 로드한 데이터를 Game server 에 전송
	//

	TPacketMyshopPricelistHeader header;

	header.dwOwnerID = pReqInfo->second;
	header.byCount = table.byCount;

	size_t sizePriceListSize = sizeof(TItemPriceInfo) * header.byCount;

	peer->EncodeHeader(HEADER_DG_MYSHOP_PRICELIST_RES, pReqInfo->first, sizeof(header) + sizePriceListSize);
	peer->Encode(&header, sizeof(header));
	peer->Encode(table.aPriceInfo, sizePriceListSize);

	sys_log(0, "Load MyShopPricelist handle[%d] pid[%d] count[%d]", pReqInfo->first, pReqInfo->second, header.byCount);

	delete pReqInfo;
}

void CClientManager::RESULT_PRICELIST_LOAD_FOR_UPDATE(SQLMsg* pMsg)
{
	TItemPriceListTable* pUpdateTable = (TItemPriceListTable*)static_cast<CQueryInfo*>(pMsg->pvUserData)->pvData;

	//
	// DB 에서 로드한 정보를 Cache 에 저장
	//

	TItemPriceListTable table{};
	table.dwOwnerID = pUpdateTable->dwOwnerID;
	table.byCount = 0;

	MYSQL_ROW row;

	while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
	{
		str_to_number(table.aPriceInfo[table.byCount].dwVnum, row[0]);
		str_to_number(table.aPriceInfo[table.byCount].dwPrice, row[1]);
#if defined(__CHEQUE_SYSTEM__)
		str_to_number(table.aPriceInfo[table.byCount].dwCheque, row[2]);
#endif
		table.byCount++;
	}

	PutItemPriceListCache(&table);

	// Update cache
	GetItemPriceListCache(pUpdateTable->dwOwnerID)->UpdateList(pUpdateTable);

	delete pUpdateTable;
}
// END_OF_MYSHOP_PRICE_LIST

void CClientManager::QUERY_SAFEBOX_SAVE(CPeer* pkPeer, TSafeboxTable* pTable)
{
	char szQuery[QUERY_MAX_LEN];

	snprintf(szQuery, sizeof(szQuery),
		"UPDATE safebox%s SET `gold` = '%u' WHERE `account_id` = %u",
		GetTablePostfix(), pTable->dwGold, pTable->dwID);

	CDBManager::instance().ReturnQuery(szQuery, QID_SAFEBOX_SAVE, pkPeer->GetHandle(), NULL);
}

void CClientManager::QUERY_EMPIRE_SELECT(CPeer* pkPeer, DWORD dwHandle, TEmpireSelectPacket* p)
{
	char szQuery[QUERY_MAX_LEN];

	snprintf(szQuery, sizeof(szQuery), "UPDATE player_index%s SET `empire` = %u WHERE `id` = %u", GetTablePostfix(), p->bEmpire, p->dwAccountID);
	delete CDBManager::instance().DirectQuery(szQuery);

	sys_log(0, "EmpireSelect: %s", szQuery);
	{
		snprintf(szQuery, sizeof(szQuery),
			"SELECT `pid1`"
			", `pid2`"
			", `pid3`"
			", `pid4`"
#if defined(__PLAYER_PER_ACCOUNT5__)
			", `pid5`"
#endif
			" FROM player_index%s WHERE `id` = %u", GetTablePostfix(), p->dwAccountID);

		std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

		SQLResult* pRes = pmsg->Get();

		if (pRes->uiNumRows)
		{
			sys_log(0, "EMPIRE %lu", pRes->uiNumRows);

			MYSQL_ROW row = mysql_fetch_row(pRes->pSQLResult);
			DWORD pids[3];

			UINT g_start_map[4] =
			{
				0, // reserved
				1, // 신수국
				21, // 천조국
				41 // 진노국
			};

			// FIXME share with game
			DWORD g_start_position[4][2] =
			{
				{ 0, 0 },
				{ 469300, 964200 }, // 신수국
				{ 55700, 157900 }, // 천조국
				{ 969600, 278400 } // 진노국
			};

			for (int i = 0; i < 3; ++i)
			{
				str_to_number(pids[i], row[i]);
				sys_log(0, "EMPIRE PIDS[%d]", pids[i]);

				if (pids[i])
				{
					sys_log(0, "EMPIRE move to pid[%d] to villiage of %u, map_index %d",
						pids[i], p->bEmpire, g_start_map[p->bEmpire]);

					snprintf(szQuery, sizeof(szQuery), "UPDATE player%s SET `map_index` = %u, `x` = %u, `y` = %u WHERE `id` = %u",
						GetTablePostfix(),
						g_start_map[p->bEmpire],
						g_start_position[p->bEmpire][0],
						g_start_position[p->bEmpire][1],
						pids[i]);

					std::unique_ptr<SQLMsg> pmsg2(CDBManager::instance().DirectQuery(szQuery));
				}
			}
		}
	}

	pkPeer->EncodeHeader(HEADER_DG_EMPIRE_SELECT, dwHandle, sizeof(BYTE));
	pkPeer->EncodeBYTE(p->bEmpire);
}

void CClientManager::QUERY_SETUP(CPeer* peer, DWORD dwHandle, const char* c_pData)
{
	TPacketGDSetup* p = (TPacketGDSetup*)c_pData;
	c_pData += sizeof(TPacketGDSetup);

	if (p->bAuthServer)
	{
		sys_log(0, "AUTH_PEER ptr %p", peer);

		m_pkAuthPeer = peer;
		return;
	}

	peer->SetPublicIP(p->szPublicIP);
	peer->SetChannel(p->bChannel);
	peer->SetListenPort(p->wListenPort);
	peer->SetP2PPort(p->wP2PPort);
	peer->SetMaps(p->alMaps);

	//
	// 어떤 맵이 어떤 서버에 있는지 보내기
	//
	TMapLocation kMapLocations;

	strlcpy(kMapLocations.szHost, peer->GetPublicIP(), sizeof(kMapLocations.szHost));
	kMapLocations.wPort = peer->GetListenPort();
	thecore_memcpy(kMapLocations.alMaps, peer->GetMaps(), sizeof(kMapLocations.alMaps));

	BYTE bMapCount;

	std::vector<TMapLocation> vec_kMapLocations;

	if (peer->GetChannel() == 1)
	{
		// for (TPeerList::const_iterator i = m_peerList.begin(); i != m_peerList.end(); ++i)
		for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
		{
			CPeer* tmp = *i;

			if (tmp == peer)
				continue;

			if (!tmp->GetChannel())
				continue;

			if (tmp->GetChannel() == GUILD_WARP_WAR_CHANNEL || tmp->GetChannel() == peer->GetChannel())
			{
				TMapLocation kMapLocation2;
				strlcpy(kMapLocation2.szHost, tmp->GetPublicIP(), sizeof(kMapLocation2.szHost));
				kMapLocation2.wPort = tmp->GetListenPort();
				thecore_memcpy(kMapLocation2.alMaps, tmp->GetMaps(), sizeof(kMapLocation2.alMaps));
				vec_kMapLocations.push_back(kMapLocation2);

				tmp->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(BYTE) + sizeof(TMapLocation));
				bMapCount = 1;
				tmp->EncodeBYTE(bMapCount);
				tmp->Encode(&kMapLocations, sizeof(TMapLocation));
			}
		}
	}
	else if (peer->GetChannel() == GUILD_WARP_WAR_CHANNEL)
	{
		// for (TPeerList::const_iterator i = m_peerList.begin(); i != m_peerList.end(); ++i)
		for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
		{
			CPeer* tmp = *i;

			if (tmp == peer)
				continue;

			if (!tmp->GetChannel())
				continue;

			if (tmp->GetChannel() == 1 || tmp->GetChannel() == peer->GetChannel())
			{
				TMapLocation kMapLocation2;
				strlcpy(kMapLocation2.szHost, tmp->GetPublicIP(), sizeof(kMapLocation2.szHost));
				kMapLocation2.wPort = tmp->GetListenPort();
				thecore_memcpy(kMapLocation2.alMaps, tmp->GetMaps(), sizeof(kMapLocation2.alMaps));
				vec_kMapLocations.push_back(kMapLocation2);
			}

			tmp->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(BYTE) + sizeof(TMapLocation));
			bMapCount = 1;
			tmp->EncodeBYTE(bMapCount);
			tmp->Encode(&kMapLocations, sizeof(TMapLocation));
		}
	}
	else
	{
		// for (TPeerList::const_iterator i = m_peerList.begin(); i != m_peerList.end(); ++i)
		for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
		{
			CPeer* tmp = *i;

			if (tmp == peer)
				continue;

			if (!tmp->GetChannel())
				continue;

			if (tmp->GetChannel() == GUILD_WARP_WAR_CHANNEL || tmp->GetChannel() == peer->GetChannel())
			{
				TMapLocation kMapLocation2;

				strlcpy(kMapLocation2.szHost, tmp->GetPublicIP(), sizeof(kMapLocation2.szHost));
				kMapLocation2.wPort = tmp->GetListenPort();
				thecore_memcpy(kMapLocation2.alMaps, tmp->GetMaps(), sizeof(kMapLocation2.alMaps));

				vec_kMapLocations.push_back(kMapLocation2);
			}

			if (tmp->GetChannel() == peer->GetChannel())
			{
				tmp->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(BYTE) + sizeof(TMapLocation));
				bMapCount = 1;
				tmp->EncodeBYTE(bMapCount);
				tmp->Encode(&kMapLocations, sizeof(TMapLocation));
			}
		}
	}

	vec_kMapLocations.push_back(kMapLocations);

	peer->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(BYTE) + sizeof(TMapLocation) * vec_kMapLocations.size());
	bMapCount = static_cast<BYTE>(vec_kMapLocations.size());
	peer->EncodeBYTE(bMapCount);
	peer->Encode(&vec_kMapLocations[0], sizeof(TMapLocation) * vec_kMapLocations.size());

	//
	// 셋업 : 접속한 피어에 다른 피어들이 접속하게 만든다. (P2P 컨넥션 생성)
	//
	sys_log(0, "SETUP: channel %u listen %u p2p %u count %u", peer->GetChannel(), p->wListenPort, p->wP2PPort, bMapCount);

	TPacketDGP2P p2pSetupPacket;
	p2pSetupPacket.wPort = peer->GetP2PPort();
	p2pSetupPacket.bChannel = peer->GetChannel();
	strlcpy(p2pSetupPacket.szHost, peer->GetPublicIP(), sizeof(p2pSetupPacket.szHost));

	//for (TPeerList::const_iterator i = m_peerList.begin(); i != m_peerList.end(); ++i)
	for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
	{
		CPeer* tmp = *i;

		if (tmp == peer)
			continue;

		// 채널이 0이라면 아직 SETUP 패킷이 오지 않은 피어 또는 auth라고 간주할 수 있음
		if (0 == tmp->GetChannel())
			continue;

		tmp->EncodeHeader(HEADER_DG_P2P, 0, sizeof(TPacketDGP2P));
		tmp->Encode(&p2pSetupPacket, sizeof(TPacketDGP2P));
	}

	//
	// 로그인 및 빌링정보 보내기
	//
	TPacketLoginOnSetup* pck = (TPacketLoginOnSetup*)c_pData;

	for (DWORD c = 0; c < p->dwLoginCount; ++c, ++pck)
	{
		CLoginData* pkLD = new CLoginData;

		pkLD->SetKey(pck->dwLoginKey);
		pkLD->SetClientKey(pck->adwClientKey);
		pkLD->SetIP(pck->szHost);

		TAccountTable& r = pkLD->GetAccountRef();

		r.id = pck->dwID;
		trim_and_lower(pck->szLogin, r.login, sizeof(r.login));
		strlcpy(r.social_id, pck->szSocialID, sizeof(r.social_id));
		strlcpy(r.passwd, "TEMP", sizeof(r.passwd));
#if defined(__MULTI_LANGUAGE_SYSTEM__)
		strlcpy(r.country, pck->szCountry, sizeof(r.country));
#endif

		InsertLoginData(pkLD);

		if (InsertLogonAccount(pck->szLogin, peer->GetHandle(), pck->szHost))
		{
			sys_log(0, "SETUP: login %u %s login_key %u host %s", pck->dwID, pck->szLogin, pck->dwLoginKey, pck->szHost);
			pkLD->SetPlay(true);
		}
		else
			sys_log(0, "SETUP: login_fail %u %s login_key %u", pck->dwID, pck->szLogin, pck->dwLoginKey);
	}

	SendPartyOnSetup(peer);

	CGuildManager::instance().OnSetup(peer);
	CPrivManager::instance().SendPrivOnSetup(peer);

	SendEventFlagsOnSetup(peer);
#if defined(__GUILD_EVENT_FLAG__) //&& defined(__GUILD_RENEWAL__)
	SendGuildEventFlagsOnSetup(peer);
#endif

	marriage::CManager::instance().OnSetup(peer);
}

void CClientManager::QUERY_ITEM_FLUSH(CPeer* pkPeer, const char* c_pData)
{
	DWORD dwID = *(DWORD*)c_pData;

	if (g_log)
		sys_log(0, "HEADER_GD_ITEM_FLUSH: %u", dwID);

	CItemCache* c = GetItemCache(dwID);

	if (c)
		c->Flush();
}

void CClientManager::QUERY_ITEM_SAVE(CPeer* pkPeer, const char* c_pData)
{
	TPlayerItem* p = (TPlayerItem*)c_pData;

	// 창고면 캐쉬하지 않고, 캐쉬에 있던 것도 빼버려야 한다.

	if (p->bWindow == SAFEBOX || p->bWindow == MALL)
	{
		CItemCache* c = GetItemCache(p->dwID);

		if (c)
		{
			TItemCacheSetPtrMap::iterator it = m_map_pkItemCacheSetPtr.find(c->Get()->dwOwner);

			if (it != m_map_pkItemCacheSetPtr.end())
			{
				if (g_test_server)
					sys_log(0, "ITEM_CACHE: safebox owner %u id %u", c->Get()->dwOwner, c->Get()->dwID);

				it->second->erase(c);
			}

			m_map_itemCache.erase(p->dwID);

			delete c;
		}

		char szQuery[QUERY_MAX_LEN];
		snprintf(szQuery, sizeof(szQuery),
			"REPLACE INTO `item%s` (`id`, `owner_id`, `window`, `pos`, `vnum`, `count`"
#if defined(__SOUL_BIND_SYSTEM__)
			", `soulbind`"
#endif
			", `socket0`, `socket1`, `socket2`"
#if defined(__ITEM_SOCKET6__)
			", `socket3`, `socket4`, `socket5`"
#endif
			", `attrtype0`, `attrvalue0`"
			", `attrtype1`, `attrvalue1`"
			", `attrtype2`, `attrvalue2`"
			", `attrtype3`, `attrvalue3`"
			", `attrtype4`, `attrvalue4`"
			", `attrtype5`, `attrvalue5`"
			", `attrtype6`, `attrvalue6`"
#if defined(__CHANGE_LOOK_SYSTEM__)
			", `transmutation`"
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
			", `refine_element_apply_type`"
			", `refine_element_grade`"
			", `refine_element_value0`, `refine_element_value1`, `refine_element_value2`"
			", `refine_element_bonus_value0`, `refine_element_bonus_value1`, `refine_element_bonus_value2`"
#endif
#if defined(__ITEM_APPLY_RANDOM__)
			", `apply_type0`, `apply_value0`, `apply_path0`"
			", `apply_type1`, `apply_value1`, `apply_path1`"
			", `apply_type2`, `apply_value2`, `apply_path2`"
			", `apply_type3`, `apply_value3`, `apply_path3`"
#endif
#if defined(__SET_ITEM__)
			", `set_value`"
#endif
			") VALUES(%u, %u, %u, %u, %u, %u"
#if defined(__SOUL_BIND_SYSTEM__)
			", %ld"
#endif
			", %ld, %ld, %ld"
#if defined(__ITEM_SOCKET6__)
			", %ld, %ld, %ld"
#endif
			", %u, %ld"
			", %u, %ld"
			", %u, %ld"
			", %u, %ld"
			", %u, %ld"
			", %u, %ld"
			", %u, %ld"
#if defined(__CHANGE_LOOK_SYSTEM__)
			", %u"
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
			", %u"
			", %u"
			", %u, %u, %u"
			", %u, %u, %u"
#endif
#if defined(__ITEM_APPLY_RANDOM__)
			", %u, %ld, %u"
			", %u, %ld, %u"
			", %u, %ld, %u"
			", %u, %ld, %u"
#endif
#if defined(__SET_ITEM__)
			", %u"
#endif
			")", GetTablePostfix()
			, p->dwID
			, p->dwOwner
			, p->bWindow
			, p->wPos
			, p->dwVnum
			, p->dwCount
#if defined(__SOUL_BIND_SYSTEM__)
			, p->lSealDate
#endif
			, p->alSockets[0], p->alSockets[1], p->alSockets[2]
#if defined(__ITEM_SOCKET6__)
			, p->alSockets[3], p->alSockets[4], p->alSockets[5]
#endif
			, p->aAttr[0].wType, p->aAttr[0].lValue
			, p->aAttr[1].wType, p->aAttr[1].lValue
			, p->aAttr[2].wType, p->aAttr[2].lValue
			, p->aAttr[3].wType, p->aAttr[3].lValue
			, p->aAttr[4].wType, p->aAttr[4].lValue
			, p->aAttr[5].wType, p->aAttr[5].lValue
			, p->aAttr[6].wType, p->aAttr[6].lValue
#if defined(__CHANGE_LOOK_SYSTEM__)
			, p->dwTransmutationVnum
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
			, p->RefineElement.wApplyType
			, p->RefineElement.bGrade
			, p->RefineElement.abValue[0], p->RefineElement.abValue[1], p->RefineElement.abValue[2]
			, p->RefineElement.abBonusValue[0], p->RefineElement.abBonusValue[1], p->RefineElement.abBonusValue[2]
#endif
#if defined(__ITEM_APPLY_RANDOM__)
			, p->aApplyRandom[0].wType, p->aApplyRandom[0].lValue, p->aApplyRandom[0].bPath
			, p->aApplyRandom[1].wType, p->aApplyRandom[1].lValue, p->aApplyRandom[1].bPath
			, p->aApplyRandom[2].wType, p->aApplyRandom[2].lValue, p->aApplyRandom[2].bPath
			, p->aApplyRandom[3].wType, p->aApplyRandom[3].lValue, p->aApplyRandom[3].bPath
#endif
#if defined(__SET_ITEM__)
			, p->bSetValue
#endif
		);

		CDBManager::instance().ReturnQuery(szQuery, QID_ITEM_SAVE, pkPeer->GetHandle(), NULL);
	}
	else
	{
		if (g_test_server)
			sys_log(0, "QUERY_ITEM_SAVE => PutItemCache() owner %d id %d vnum %d ", p->dwOwner, p->dwID, p->dwVnum);

		PutItemCache(p);
	}
}

CClientManager::TItemCacheSet* CClientManager::GetItemCacheSet(DWORD pid)
{
	TItemCacheSetPtrMap::iterator it = m_map_pkItemCacheSetPtr.find(pid);

	if (it == m_map_pkItemCacheSetPtr.end())
		return NULL;

	return it->second;
}

void CClientManager::CreateItemCacheSet(DWORD pid)
{
	if (m_map_pkItemCacheSetPtr.find(pid) != m_map_pkItemCacheSetPtr.end())
		return;

	TItemCacheSet* pSet = new TItemCacheSet;
	m_map_pkItemCacheSetPtr.insert(TItemCacheSetPtrMap::value_type(pid, pSet));

	if (g_log)
		sys_log(0, "ITEM_CACHE: new cache %u", pid);
}

void CClientManager::FlushItemCacheSet(DWORD pid)
{
	TItemCacheSetPtrMap::iterator it = m_map_pkItemCacheSetPtr.find(pid);

	if (it == m_map_pkItemCacheSetPtr.end())
	{
		sys_log(0, "FLUSH_ITEMCACHESET : No ItemCacheSet pid(%d)", pid);
		return;
	}

	TItemCacheSet* pSet = it->second;
	TItemCacheSet::iterator it_set = pSet->begin();

	while (it_set != pSet->end())
	{
		CItemCache* c = *it_set++;
		c->Flush();

		m_map_itemCache.erase(c->Get()->dwID);
		delete c;
	}

	pSet->clear();
	delete pSet;

	m_map_pkItemCacheSetPtr.erase(it);

	if (g_log)
		sys_log(0, "FLUSH_ITEMCACHESET : Deleted pid(%d)", pid);
}

CItemCache* CClientManager::GetItemCache(DWORD id)
{
	TItemCacheMap::iterator it = m_map_itemCache.find(id);

	if (it == m_map_itemCache.end())
		return NULL;

	return it->second;
}

void CClientManager::PutItemCache(TPlayerItem* pNew, bool bSkipQuery)
{
	CItemCache* c;

	c = GetItemCache(pNew->dwID);

	// 아이템 새로 생성
	if (!c)
	{
		if (g_log)
			sys_log(0, "ITEM_CACHE: PutItemCache ==> New CItemCache id%d vnum%d new owner%d", pNew->dwID, pNew->dwVnum, pNew->dwOwner);

		c = new CItemCache;
		m_map_itemCache.insert(TItemCacheMap::value_type(pNew->dwID, c));
	}
	// 있을시
	else
	{
		if (g_log)
			sys_log(0, "ITEM_CACHE: PutItemCache ==> Have Cache");

		// 소유자가 틀리면
		if (pNew->dwOwner != c->Get()->dwOwner)
		{
			// 이미 이 아이템을 가지고 있었던 유저로 부터 아이템을 삭제한다.
			TItemCacheSetPtrMap::iterator it = m_map_pkItemCacheSetPtr.find(c->Get()->dwOwner);

			if (it != m_map_pkItemCacheSetPtr.end())
			{
				if (g_log)
					sys_log(0, "ITEM_CACHE: delete owner %u id %u new owner %u", c->Get()->dwOwner, c->Get()->dwID, pNew->dwOwner);
				it->second->erase(c);
			}
		}
	}

	// 새로운 정보 업데이트
	c->Put(pNew, bSkipQuery);

	TItemCacheSetPtrMap::iterator it = m_map_pkItemCacheSetPtr.find(c->Get()->dwOwner);

	if (it != m_map_pkItemCacheSetPtr.end())
	{
		if (g_log)
			sys_log(0, "ITEM_CACHE: save %u id %u", c->Get()->dwOwner, c->Get()->dwID);
		else
			sys_log(1, "ITEM_CACHE: save %u id %u", c->Get()->dwOwner, c->Get()->dwID);
		it->second->insert(c);
	}
	else
	{
		// 현재 소유자가 없으므로 바로 저장해야 다음 접속이 올 때 SQL에 쿼리하여
		// 받을 수 있으므로 바로 저장한다.
		if (g_log)
			sys_log(0, "ITEM_CACHE: direct save %u id %u", c->Get()->dwOwner, c->Get()->dwID);
		else
			sys_log(1, "ITEM_CACHE: direct save %u id %u", c->Get()->dwOwner, c->Get()->dwID);

		c->OnFlush();
	}
}

bool CClientManager::DeleteItemCache(DWORD dwID)
{
	CItemCache* c = GetItemCache(dwID);

	if (!c)
		return false;

	c->Delete();
	return true;
}

// MYSHOP_PRICE_LIST
CItemPriceListTableCache* CClientManager::GetItemPriceListCache(DWORD dwID)
{
	TItemPriceListCacheMap::iterator it = m_mapItemPriceListCache.find(dwID);

	if (it == m_mapItemPriceListCache.end())
		return NULL;

	return it->second;
}

void CClientManager::PutItemPriceListCache(const TItemPriceListTable* pItemPriceList)
{
	CItemPriceListTableCache* pCache = GetItemPriceListCache(pItemPriceList->dwOwnerID);

	if (!pCache)
	{
		pCache = new CItemPriceListTableCache;
		m_mapItemPriceListCache.insert(TItemPriceListCacheMap::value_type(pItemPriceList->dwOwnerID, pCache));
	}

	pCache->Put(const_cast<TItemPriceListTable*>(pItemPriceList), true);
}

void CClientManager::UpdatePlayerCache()
{
	TPlayerTableCacheMap::iterator it = m_map_playerCache.begin();

	while (it != m_map_playerCache.end())
	{
		CPlayerTableCache* c = (it++)->second;

		if (c->CheckTimeout())
		{
			if (g_log)
				sys_log(0, "UPDATE : UpdatePlayerCache() ==> FlushPlayerCache %d %s ", c->Get(false)->id, c->Get(false)->name);

			c->Flush();

			// Item Cache도 업데이트
			UpdateItemCacheSet(c->Get()->id);
		}
		else if (c->CheckFlushTimeout())
			c->Flush();
	}
}
// END_OF_MYSHOP_PRICE_LIST

void CClientManager::SetCacheFlushCountLimit(int iLimit)
{
	m_iCacheFlushCountLimit = MAX(10, iLimit);
	sys_log(0, "CACHE_FLUSH_LIMIT_PER_SECOND: %d", m_iCacheFlushCountLimit);
}

void CClientManager::UpdateItemCache()
{
	if (m_iCacheFlushCount >= m_iCacheFlushCountLimit)
		return;

	TItemCacheMap::iterator it = m_map_itemCache.begin();

	while (it != m_map_itemCache.end())
	{
		CItemCache* c = (it++)->second;

		// 아이템은 Flush만 한다.
		if (c->CheckFlushTimeout())
		{
			if (g_test_server)
				sys_log(0, "UpdateItemCache ==> Flush() vnum %d id owner %d", c->Get()->dwVnum, c->Get()->dwID, c->Get()->dwOwner);

			c->Flush();

			if (++m_iCacheFlushCount >= m_iCacheFlushCountLimit)
				break;
		}
	}
}

void CClientManager::UpdateItemPriceListCache()
{
	TItemPriceListCacheMap::iterator it = m_mapItemPriceListCache.begin();

	while (it != m_mapItemPriceListCache.end())
	{
		CItemPriceListTableCache* pCache = it->second;

		if (pCache->CheckFlushTimeout())
		{
			pCache->Flush();
			m_mapItemPriceListCache.erase(it++);
		}
		else
			++it;
	}
}

void CClientManager::QUERY_ITEM_DESTROY(CPeer* pkPeer, const char* c_pData)
{
	DWORD dwID = *(DWORD*)c_pData;
	c_pData += sizeof(DWORD);

	DWORD dwPID = *(DWORD*)c_pData;

	if (!DeleteItemCache(dwID))
	{
		char szQuery[64];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM item%s WHERE `id` = %u", GetTablePostfix(), dwID);

		if (g_log)
			sys_log(0, "HEADER_GD_ITEM_DESTROY: PID %u ID %u", dwPID, dwID);

		if (dwPID == 0) // 아무도 가진 사람이 없었다면, 비동기 쿼리
			CDBManager::instance().AsyncQuery(szQuery);
		else
			CDBManager::instance().ReturnQuery(szQuery, QID_ITEM_DESTROY, pkPeer->GetHandle(), NULL);
	}
}

void CClientManager::QUERY_FLUSH_CACHE(CPeer* pkPeer, const char* c_pData)
{
	DWORD dwPID = *(DWORD*)c_pData;

	CPlayerTableCache* pkCache = GetPlayerCache(dwPID);

	if (!pkCache)
		return;

	sys_log(0, "FLUSH_CACHE: %u", dwPID);

	pkCache->Flush();
	FlushItemCacheSet(dwPID);

	m_map_playerCache.erase(dwPID);
	delete pkCache;
}

void CClientManager::QUERY_RELOAD_PROTO()
{
	if (!InitializeTables())
	{
		sys_err("QUERY_RELOAD_PROTO: cannot load tables");
		return;
	}

	for (TPeerList::iterator i = m_peerList.begin(); i != m_peerList.end(); ++i)
	{
		CPeer* tmp = *i;

		if (!tmp->GetChannel())
			continue;

		tmp->EncodeHeader(HEADER_DG_RELOAD_PROTO, 0,
			sizeof(WORD) + sizeof(TSkillTable) * m_vec_skillTable.size() +
			sizeof(WORD) + sizeof(TBanwordTable) * m_vec_banwordTable.size() +
			sizeof(WORD) + sizeof(TItemTable) * m_vec_itemTable.size() +
			sizeof(WORD) + sizeof(TMobTable) * m_vec_mobTable.size() +
			sizeof(WORD) + sizeof(TRefineTable) * m_iRefineTableSize);

		tmp->EncodeWORD(static_cast<WORD>(m_vec_skillTable.size()));
		tmp->Encode(&m_vec_skillTable[0], sizeof(TSkillTable) * m_vec_skillTable.size());

		tmp->EncodeWORD(static_cast<WORD>(m_vec_banwordTable.size()));
		tmp->Encode(&m_vec_banwordTable[0], sizeof(TBanwordTable) * m_vec_banwordTable.size());

		tmp->EncodeWORD(static_cast<WORD>(m_vec_itemTable.size()));
		tmp->Encode(&m_vec_itemTable[0], sizeof(TItemTable) * m_vec_itemTable.size());

		tmp->EncodeWORD(static_cast<WORD>(m_vec_mobTable.size()));
		tmp->Encode(&m_vec_mobTable[0], sizeof(TMobTable) * m_vec_mobTable.size());

		tmp->EncodeWORD(static_cast<WORD>(m_iRefineTableSize));
		tmp->Encode(m_pRefineTable, sizeof(TRefineTable) * m_iRefineTableSize);
	}
}

// ADD_GUILD_PRIV_TIME
/**
* @version 05/06/08 Bang2ni - 지속시간 추가
**/
void CClientManager::AddGuildPriv(TPacketGiveGuildPriv* p)
{
	CPrivManager::instance().AddGuildPriv(p->guild_id, p->type, p->value, p->duration_sec);
}

void CClientManager::AddEmpirePriv(TPacketGiveEmpirePriv* p)
{
	CPrivManager::instance().AddEmpirePriv(p->empire, p->type, p->value, p->duration_sec);
}
// END_OF_ADD_GUILD_PRIV_TIME

void CClientManager::AddCharacterPriv(TPacketGiveCharacterPriv* p)
{
	CPrivManager::instance().AddCharPriv(p->pid, p->type, p->value);
}

void CClientManager::MoneyLog(TPacketMoneyLog* p)
{
	CMoneyLog::instance().AddLog(p->type, p->vnum, p->gold
#if defined(__CHEQUE_SYSTEM__)
		, p->cheque
#endif
	);
}

CLoginData* CClientManager::GetLoginData(DWORD dwKey)
{
	TLoginDataByLoginKey::iterator it = m_map_pkLoginData.find(dwKey);

	if (it == m_map_pkLoginData.end())
		return NULL;

	return it->second;
}

CLoginData* CClientManager::GetLoginDataByLogin(const char* c_pszLogin)
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(c_pszLogin, szLogin, sizeof(szLogin));

	TLoginDataByLogin::iterator it = m_map_pkLoginDataByLogin.find(szLogin);

	if (it == m_map_pkLoginDataByLogin.end())
		return NULL;

	return it->second;
}

CLoginData* CClientManager::GetLoginDataByAID(DWORD dwAID)
{
	TLoginDataByAID::iterator it = m_map_pkLoginDataByAID.find(dwAID);

	if (it == m_map_pkLoginDataByAID.end())
		return NULL;

	return it->second;
}

void CClientManager::InsertLoginData(CLoginData* pkLD)
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(pkLD->GetAccountRef().login, szLogin, sizeof(szLogin));

	m_map_pkLoginData.insert(std::make_pair(pkLD->GetKey(), pkLD));
	m_map_pkLoginDataByLogin.insert(std::make_pair(szLogin, pkLD));
	m_map_pkLoginDataByAID.insert(std::make_pair(pkLD->GetAccountRef().id, pkLD));
}

void CClientManager::DeleteLoginData(CLoginData* pkLD)
{
	m_map_pkLoginData.erase(pkLD->GetKey());
	m_map_pkLoginDataByLogin.erase(pkLD->GetAccountRef().login);
	m_map_pkLoginDataByAID.erase(pkLD->GetAccountRef().id);

	if (m_map_kLogonAccount.find(pkLD->GetAccountRef().login) == m_map_kLogonAccount.end())
		delete pkLD;
	else
		pkLD->SetDeleted(true);
}

void CClientManager::QUERY_AUTH_LOGIN(CPeer* pkPeer, DWORD dwHandle, TPacketGDAuthLogin* p)
{
	if (g_test_server)
		sys_log(0, "QUERY_AUTH_LOGIN %d %d %s", p->dwID, p->dwLoginKey, p->szLogin);

	CLoginData* pkLD = GetLoginDataByLogin(p->szLogin);

	if (pkLD)
	{
		DeleteLoginData(pkLD);
	}

	BYTE bResult;

	if (GetLoginData(p->dwLoginKey))
	{
		sys_err("LoginData already exist key %u login %s", p->dwLoginKey, p->szLogin);
		bResult = 0;

		pkPeer->EncodeHeader(HEADER_DG_AUTH_LOGIN, dwHandle, sizeof(BYTE));
		pkPeer->EncodeBYTE(bResult);
	}
	else
	{
		CLoginData* pkLD = new CLoginData;

		pkLD->SetKey(p->dwLoginKey);
		pkLD->SetClientKey(p->adwClientKey);
		pkLD->SetPremium(p->iPremiumTimes);

		TAccountTable& r = pkLD->GetAccountRef();

		r.id = p->dwID;
		trim_and_lower(p->szLogin, r.login, sizeof(r.login));
		strlcpy(r.social_id, p->szSocialID, sizeof(r.social_id));
		strlcpy(r.passwd, "TEMP", sizeof(r.passwd));
#if defined(__MULTI_LANGUAGE_SYSTEM__)
		strlcpy(r.country, p->szCountry, sizeof(r.country));
#endif

		sys_log(0, "AUTH_LOGIN id(%u) login(%s)"
#if defined(__MULTI_LANGUAGE_SYSTEM__)
			" country(%s)"
#endif
			" social_id(%s) login_key(%u), client_key(%u %u %u %u)",
			p->dwID,
			p->szLogin,
#if defined(__MULTI_LANGUAGE_SYSTEM__)
			p->szCountry,
#endif
			p->szSocialID,
			p->dwLoginKey,
			p->adwClientKey[0], p->adwClientKey[1], p->adwClientKey[2], p->adwClientKey[3]
		);

		bResult = 1;

		InsertLoginData(pkLD);

		pkPeer->EncodeHeader(HEADER_DG_AUTH_LOGIN, dwHandle, sizeof(BYTE));
		pkPeer->EncodeBYTE(bResult);
	}
}

void CClientManager::GuildDepositMoney(TPacketGDGuildMoney* p)
{
	CGuildManager::instance().DepositMoney(p->dwGuild, p->iGold);
}

void CClientManager::GuildWithdrawMoney(CPeer* peer, TPacketGDGuildMoney* p)
{
	CGuildManager::instance().WithdrawMoney(peer, p->dwGuild, p->iGold);
}

void CClientManager::GuildWithdrawMoneyGiveReply(TPacketGDGuildMoneyWithdrawGiveReply* p)
{
	CGuildManager::instance().WithdrawMoneyReply(p->dwGuild, p->bGiveSuccess, p->iChangeGold);
}

void CClientManager::GuildWarBet(TPacketGDGuildWarBet* p)
{
	CGuildManager::instance().Bet(p->dwWarID, p->szLogin, p->dwGold, p->dwGuild);
}

void CClientManager::CreateObject(TPacketGDCreateObject* p)
{
	using namespace building;

	char szQuery[512];

	snprintf(szQuery, sizeof(szQuery),
		"INSERT INTO object%s (`land_id`, `vnum`, `map_index`, `x`, `y`, `x_rot`, `y_rot`, `z_rot`) VALUES(%u, %u, %d, %d, %d, %f, %f, %f)",
		GetTablePostfix(), p->dwLandID, p->dwVnum, p->lMapIndex, p->x, p->y, p->xRot, p->yRot, p->zRot);

	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

	if (pmsg->Get()->uiInsertID == 0)
	{
		sys_err("cannot insert object");
		return;
	}

	TObject* pkObj = new TObject;

	memset(pkObj, 0, sizeof(TObject));

	pkObj->dwID = pmsg->Get()->uiInsertID;
	pkObj->dwVnum = p->dwVnum;
	pkObj->dwLandID = p->dwLandID;
	pkObj->lMapIndex = p->lMapIndex;
	pkObj->x = p->x;
	pkObj->y = p->y;
	pkObj->xRot = p->xRot;
	pkObj->yRot = p->yRot;
	pkObj->zRot = p->zRot;
	pkObj->lLife = 0;

	ForwardPacket(HEADER_DG_CREATE_OBJECT, pkObj, sizeof(TObject));

	m_map_pkObjectTable.insert(std::make_pair(pkObj->dwID, pkObj));
}

void CClientManager::DeleteObject(DWORD dwID)
{
	char szQuery[128];

	snprintf(szQuery, sizeof(szQuery), "DELETE FROM object%s WHERE `id` = %u", GetTablePostfix(), dwID);

	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

	if (pmsg->Get()->uiAffectedRows == 0 || pmsg->Get()->uiAffectedRows == (uint32_t)-1)
	{
		sys_err("no object by id %u", dwID);
		return;
	}

	//pkObjectTableMap::const_iterator it = m_map_pkObjectTable.find(dwID);
	auto it = m_map_pkObjectTable.find(dwID);
	if (it != m_map_pkObjectTable.end())
	{
		delete it->second;
		m_map_pkObjectTable.erase(it);
	}

	ForwardPacket(HEADER_DG_DELETE_OBJECT, &dwID, sizeof(DWORD));
}

void CClientManager::UpdateLand(DWORD* pdw)
{
	DWORD dwID = pdw[0];
	DWORD dwGuild = pdw[1];

	building::TLand* p = &m_vec_kLandTable[0];

	DWORD i;

	for (i = 0; i < m_vec_kLandTable.size(); ++i, ++p)
	{
		if (p->dwID == dwID)
		{
			char buf[256];
			snprintf(buf, sizeof(buf), "UPDATE land%s SET `guild_id` = %u WHERE `id` = %u", GetTablePostfix(), dwGuild, dwID);
			CDBManager::instance().AsyncQuery(buf);

			p->dwGuildID = dwGuild;
			break;
		}
	}

	if (i < m_vec_kLandTable.size())
		ForwardPacket(HEADER_DG_UPDATE_LAND, p, sizeof(building::TLand));
}

// BLOCK_CHAT
void CClientManager::BlockChat(TPacketBlockChat* p)
{
	char szQuery[256];

	if (g_stLocale == "sjis")
		snprintf(szQuery, sizeof(szQuery), "SELECT `id `FROM player%s WHERE `name` = '%s' COLLATE sjis_japanese_ci", GetTablePostfix(), p->szName);
	else
		snprintf(szQuery, sizeof(szQuery), "SELECT `id` FROM player%s WHERE `name` = '%s'", GetTablePostfix(), p->szName);
	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));
	SQLResult* pRes = pmsg->Get();

	if (pRes->uiNumRows)
	{
		MYSQL_ROW row = mysql_fetch_row(pRes->pSQLResult);
		DWORD pid = strtoul(row[0], NULL, 10);

		TPacketGDAddAffect pa;
		pa.dwPID = pid;
		pa.elem.dwType = 223;
		pa.elem.wApplyOn = 0;
		pa.elem.lApplyValue = 0;
		pa.elem.dwFlag = 0;
		pa.elem.lDuration = p->lDuration;
		pa.elem.lSPCost = 0;
		QUERY_ADD_AFFECT(NULL, &pa);
	}
	else
	{
		// cannot find user with that name
	}
}
// END_OF_BLOCK_CHAT

void CClientManager::MarriageAdd(TPacketMarriageAdd* p)
{
	sys_log(0, "MarriageAdd %u %u %s %s", p->dwPID1, p->dwPID2, p->szName1, p->szName2);
	marriage::CManager::instance().Add(p->dwPID1, p->dwPID2, p->szName1, p->szName2);
}

void CClientManager::MarriageUpdate(TPacketMarriageUpdate* p)
{
	sys_log(0, "MarriageUpdate PID:%u %u LP:%d ST:%d", p->dwPID1, p->dwPID2, p->iLovePoint, p->byMarried);
	marriage::CManager::instance().Update(p->dwPID1, p->dwPID2, p->iLovePoint, p->byMarried);
}

void CClientManager::MarriageRemove(TPacketMarriageRemove* p)
{
	sys_log(0, "MarriageRemove %u %u", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().Remove(p->dwPID1, p->dwPID2);
}

void CClientManager::WeddingRequest(TPacketWeddingRequest* p)
{
	sys_log(0, "WeddingRequest %u %u", p->dwPID1, p->dwPID2);
	ForwardPacket(HEADER_DG_WEDDING_REQUEST, p, sizeof(TPacketWeddingRequest));
	//marriage::CManager::instance().RegisterWedding(p->dwPID1, p->szName1, p->dwPID2, p->szName2);
}

void CClientManager::WeddingReady(TPacketWeddingReady* p)
{
	sys_log(0, "WeddingReady %u %u", p->dwPID1, p->dwPID2);
	ForwardPacket(HEADER_DG_WEDDING_READY, p, sizeof(TPacketWeddingReady));
	marriage::CManager::instance().ReadyWedding(p->dwMapIndex, p->dwPID1, p->dwPID2);
}

void CClientManager::WeddingEnd(TPacketWeddingEnd* p)
{
	sys_log(0, "WeddingEnd %u %u", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().EndWedding(p->dwPID1, p->dwPID2);
}

//
// 캐시에 가격정보가 있으면 캐시를 업데이트 하고 캐시에 가격정보가 없다면
// 우선 기존의 데이터를 로드한 뒤에 기존의 정보로 캐시를 만들고 새로 받은 가격정보를 업데이트 한다.
//
void CClientManager::MyshopPricelistUpdate(const TItemPriceListTable* pPacket)
{
	if (pPacket->byCount > SHOP_PRICELIST_MAX_NUM)
	{
		sys_err("count overflow!");
		return;
	}

	CItemPriceListTableCache* pCache = GetItemPriceListCache(pPacket->dwOwnerID);

	if (pCache)
	{
		TItemPriceListTable table;

		table.dwOwnerID = pPacket->dwOwnerID;
		table.byCount = pPacket->byCount;

		thecore_memcpy(table.aPriceInfo, pPacket->aPriceInfo, sizeof(TItemPriceInfo) * pPacket->byCount);

		pCache->UpdateList(&table);
	}
	else
	{
		TItemPriceListTable* pUpdateTable = new TItemPriceListTable;

		pUpdateTable->dwOwnerID = pPacket->dwOwnerID;
		pUpdateTable->byCount = pPacket->byCount;

		thecore_memcpy(pUpdateTable->aPriceInfo, pPacket->aPriceInfo, sizeof(TItemPriceInfo) * pPacket->byCount);

		char szQuery[QUERY_MAX_LEN];
		snprintf(szQuery, sizeof(szQuery),
			"SELECT `item_vnum`, `price`"
#if defined(__CHEQUE_SYSTEM__)
			", `cheque`"
#endif
			" FROM myshop_pricelist%s WHERE `owner_id` = %u", GetTablePostfix(), pPacket->dwOwnerID);
		CDBManager::instance().ReturnQuery(szQuery, QID_ITEMPRICE_LOAD_FOR_UPDATE, 0, pUpdateTable);
	}
}

// MYSHOP_PRICE_LIST
// 캐시된 가격정보가 있으면 캐시를 읽어 바로 전송하고 캐시에 정보가 없으면 DB 에 쿼리를 한다.
//
void CClientManager::MyshopPricelistRequest(CPeer* peer, DWORD dwHandle, DWORD dwPlayerID)
{
	if (CItemPriceListTableCache* pCache = GetItemPriceListCache(dwPlayerID))
	{
		sys_log(0, "Cache MyShopPricelist handle[%d] pid[%d]", dwHandle, dwPlayerID);

		TItemPriceListTable* pTable = pCache->Get(false);

		TPacketMyshopPricelistHeader header =
		{
			pTable->dwOwnerID,
			pTable->byCount
		};

		size_t sizePriceListSize = sizeof(TItemPriceInfo) * pTable->byCount;

		peer->EncodeHeader(HEADER_DG_MYSHOP_PRICELIST_RES, dwHandle, sizeof(header) + sizePriceListSize);
		peer->Encode(&header, sizeof(header));
		peer->Encode(pTable->aPriceInfo, sizePriceListSize);
	}
	else
	{
		sys_log(0, "Query MyShopPricelist handle[%d] pid[%d]", dwHandle, dwPlayerID);

		char szQuery[QUERY_MAX_LEN];
		snprintf(szQuery, sizeof(szQuery),
			"SELECT `item_vnum`, `price`"
#if defined(__CHEQUE_SYSTEM__)
			", `cheque`"
#endif
			" FROM myshop_pricelist%s WHERE `owner_id` = %u", GetTablePostfix(), dwPlayerID);
		CDBManager::instance().ReturnQuery(szQuery, QID_ITEMPRICE_LOAD, peer->GetHandle(), new TItemPricelistReqInfo(dwHandle, dwPlayerID));
	}
}
// END_OF_MYSHOP_PRICE_LIST

void CPacketInfo::Add(int header)
{
	auto it = m_map_info.find(header);
	if (it == m_map_info.end())
		m_map_info.insert(std::map<int, int>::value_type(header, 1));
	else
		++it->second;
}

void CPacketInfo::Reset()
{
	m_map_info.clear();
}

void CClientManager::ProcessPackets(CPeer* peer)
{
	BYTE header;
	DWORD dwHandle;
	DWORD dwLength;
	const char* data = NULL;
	int i = 0;
	int iCount = 0;

	while (peer->PeekPacket(i, header, dwHandle, dwLength, &data))
	{
		// DISABLE_DB_HEADER_LOG
		// sys_log(0, "header %d %p size %d", header, this, dwLength);
		// END_OF_DISABLE_DB_HEADER_LOG
		m_bLastHeader = header;
		++iCount;

#if defined(_DEBUG)
		if (header != 10)
			sys_log(0, " ProcessPacket Header [%d] Handle[%d] Length[%d] iCount[%d]", header, dwHandle, dwLength, iCount);
#endif

		if (g_test_server)
		{
			if (header != 10)
				sys_log(0, " ProcessPacket Header [%d] Handle[%d] Length[%d] iCount[%d]", header, dwHandle, dwLength, iCount);
		}

		// test log by mhh
		{
			if (HEADER_GD_BLOCK_COUNTRY_IP == header)
				sys_log(0, "recved : HEADER_GD_BLOCK_COUNTRY_IP");
		}

		switch (header)
		{
			case HEADER_GD_BOOT:
				QUERY_BOOT(peer, (TPacketGDBoot*)data);
				break;

			case HEADER_GD_HAMMER_OF_TOR:
				break;

			case HEADER_GD_LOGIN_BY_KEY:
				QUERY_LOGIN_BY_KEY(peer, dwHandle, (TPacketGDLoginByKey*)data);
				break;

			case HEADER_GD_LOGOUT:
				//sys_log(0, "HEADER_GD_LOGOUT (handle: %d length: %d)", dwHandle, dwLength);
				QUERY_LOGOUT(peer, dwHandle, data);
				break;

			case HEADER_GD_PLAYER_LOAD:
				sys_log(1, "HEADER_GD_PLAYER_LOAD (handle: %d length: %d)", dwHandle, dwLength);
				QUERY_PLAYER_LOAD(peer, dwHandle, (TPlayerLoadPacket*)data);
				break;

			case HEADER_GD_PLAYER_SAVE:
				sys_log(1, "HEADER_GD_PLAYER_SAVE (handle: %d length: %d)", dwHandle, dwLength);
				QUERY_PLAYER_SAVE(peer, dwHandle, (TPlayerTable*)data);
				break;

			case HEADER_GD_PLAYER_CREATE:
				sys_log(0, "HEADER_GD_PLAYER_CREATE (handle: %d length: %d)", dwHandle, dwLength);
				__QUERY_PLAYER_CREATE(peer, dwHandle, (TPlayerCreatePacket*)data);
				sys_log(0, "END");
				break;

			case HEADER_GD_PLAYER_DELETE:
				sys_log(1, "HEADER_GD_PLAYER_DELETE (handle: %d length: %d)", dwHandle, dwLength);
				__QUERY_PLAYER_DELETE(peer, dwHandle, (TPlayerDeletePacket*)data);
				break;

			case HEADER_GD_PLAYER_COUNT:
				QUERY_PLAYER_COUNT(peer, (TPlayerCountPacket*)data);
				break;

			case HEADER_GD_QUEST_SAVE:
				sys_log(1, "HEADER_GD_QUEST_SAVE (handle: %d length: %d)", dwHandle, dwLength);
				QUERY_QUEST_SAVE(peer, (TQuestTable*)data, dwLength);
				break;

			case HEADER_GD_SAFEBOX_LOAD:
				QUERY_SAFEBOX_LOAD(peer, dwHandle, (TSafeboxLoadPacket*)data, 0);
				break;

			case HEADER_GD_SAFEBOX_SAVE:
				sys_log(1, "HEADER_GD_SAFEBOX_SAVE (handle: %d length: %d)", dwHandle, dwLength);
				QUERY_SAFEBOX_SAVE(peer, (TSafeboxTable*)data);
				break;

			case HEADER_GD_SAFEBOX_CHANGE_SIZE:
				QUERY_SAFEBOX_CHANGE_SIZE(peer, dwHandle, (TSafeboxChangeSizePacket*)data);
				break;

			case HEADER_GD_SAFEBOX_CHANGE_PASSWORD:
				QUERY_SAFEBOX_CHANGE_PASSWORD(peer, dwHandle, (TSafeboxChangePasswordPacket*)data);
				break;

			case HEADER_GD_MALL_LOAD:
				QUERY_SAFEBOX_LOAD(peer, dwHandle, (TSafeboxLoadPacket*)data, 1);
				break;

			case HEADER_GD_EMPIRE_SELECT:
				QUERY_EMPIRE_SELECT(peer, dwHandle, (TEmpireSelectPacket*)data);
				break;

			case HEADER_GD_SETUP:
				QUERY_SETUP(peer, dwHandle, data);
				break;

			case HEADER_GD_GUILD_CREATE:
				GuildCreate(peer, *(DWORD*)data);
				break;

			case HEADER_GD_GUILD_SKILL_UPDATE:
				GuildSkillUpdate(peer, (TPacketGuildSkillUpdate*)data);
				break;

			case HEADER_GD_GUILD_EXP_UPDATE:
				GuildExpUpdate(peer, (TPacketGuildExpUpdate*)data);
				break;

			case HEADER_GD_GUILD_ADD_MEMBER:
				GuildAddMember(peer, (TPacketGDGuildAddMember*)data);
				break;

			case HEADER_GD_GUILD_REMOVE_MEMBER:
				GuildRemoveMember(peer, (TPacketGuild*)data);
				break;

			case HEADER_GD_GUILD_CHANGE_GRADE:
				GuildChangeGrade(peer, (TPacketGuild*)data);
				break;

			case HEADER_GD_GUILD_CHANGE_MEMBER_DATA:
				GuildChangeMemberData(peer, (TPacketGuildChangeMemberData*)data);
				break;

			case HEADER_GD_GUILD_DISBAND:
				GuildDisband(peer, (TPacketGuild*)data);
				break;

			case HEADER_GD_GUILD_WAR:
				GuildWar(peer, (TPacketGuildWar*)data);
				break;

			case HEADER_GD_GUILD_WAR_SCORE:
				GuildWarScore(peer, (TPacketGuildWarScore*)data);
				break;

			case HEADER_GD_GUILD_CHANGE_LADDER_POINT:
				GuildChangeLadderPoint((TPacketGuildLadderPoint*)data);
				break;

			case HEADER_GD_GUILD_USE_SKILL:
				GuildUseSkill((TPacketGuildUseSkill*)data);
				break;

			case HEADER_GD_FLUSH_CACHE:
				QUERY_FLUSH_CACHE(peer, data);
				break;

			case HEADER_GD_ITEM_SAVE:
				QUERY_ITEM_SAVE(peer, data);
				break;

			case HEADER_GD_ITEM_DESTROY:
				QUERY_ITEM_DESTROY(peer, data);
				break;

			case HEADER_GD_ITEM_FLUSH:
				QUERY_ITEM_FLUSH(peer, data);
				break;

			case HEADER_GD_ADD_AFFECT:
				sys_log(1, "HEADER_GD_ADD_AFFECT");
				QUERY_ADD_AFFECT(peer, (TPacketGDAddAffect*)data);
				break;

			case HEADER_GD_REMOVE_AFFECT:
				sys_log(1, "HEADER_GD_REMOVE_AFFECT");
				QUERY_REMOVE_AFFECT(peer, (TPacketGDRemoveAffect*)data);
				break;

			case HEADER_GD_HIGHSCORE_REGISTER:
				QUERY_HIGHSCORE_REGISTER(peer, (TPacketGDHighscore*)data);
				break;

			case HEADER_GD_PARTY_CREATE:
				QUERY_PARTY_CREATE(peer, (TPacketPartyCreate*)data);
				break;

			case HEADER_GD_PARTY_DELETE:
				QUERY_PARTY_DELETE(peer, (TPacketPartyDelete*)data);
				break;

			case HEADER_GD_PARTY_ADD:
				QUERY_PARTY_ADD(peer, (TPacketPartyAdd*)data);
				break;

			case HEADER_GD_PARTY_REMOVE:
				QUERY_PARTY_REMOVE(peer, (TPacketPartyRemove*)data);
				break;

			case HEADER_GD_PARTY_STATE_CHANGE:
				QUERY_PARTY_STATE_CHANGE(peer, (TPacketPartyStateChange*)data);
				break;

			case HEADER_GD_PARTY_SET_MEMBER_LEVEL:
				QUERY_PARTY_SET_MEMBER_LEVEL(peer, (TPacketPartySetMemberLevel*)data);
				break;

			case HEADER_GD_RELOAD_PROTO:
				QUERY_RELOAD_PROTO();
				break;

			case HEADER_GD_CHANGE_NAME:
				QUERY_CHANGE_NAME(peer, dwHandle, (TPacketGDChangeName*)data);
				break;

			case HEADER_GD_AUTH_LOGIN:
				QUERY_AUTH_LOGIN(peer, dwHandle, (TPacketGDAuthLogin*)data);
				break;

			case HEADER_GD_REQUEST_GUILD_PRIV:
				AddGuildPriv((TPacketGiveGuildPriv*)data);
				break;

			case HEADER_GD_REQUEST_EMPIRE_PRIV:
				AddEmpirePriv((TPacketGiveEmpirePriv*)data);
				break;

			case HEADER_GD_REQUEST_CHARACTER_PRIV:
				AddCharacterPriv((TPacketGiveCharacterPriv*)data);
				break;

			case HEADER_GD_MONEY_LOG:
				MoneyLog((TPacketMoneyLog*)data);
				break;

			case HEADER_GD_GUILD_DEPOSIT_MONEY:
				GuildDepositMoney((TPacketGDGuildMoney*)data);
				break;

			case HEADER_GD_GUILD_WITHDRAW_MONEY:
				GuildWithdrawMoney(peer, (TPacketGDGuildMoney*)data);
				break;

			case HEADER_GD_GUILD_WITHDRAW_MONEY_GIVE_REPLY:
				GuildWithdrawMoneyGiveReply((TPacketGDGuildMoneyWithdrawGiveReply*)data);
				break;

			case HEADER_GD_GUILD_WAR_BET:
				GuildWarBet((TPacketGDGuildWarBet*)data);
				break;

#if defined(__GUILD_EVENT_FLAG__) //&& defined(__GUILD_RENEWAL__)
			case HEADER_GD_GUILD_EVENT_FLAG:
				GuildSetEventFlag((TPacketSetGuildEventFlag*)data);
				break;
#endif

			case HEADER_GD_SET_EVENT_FLAG:
				SetEventFlag((TPacketSetEventFlag*)data);
				break;

			case HEADER_GD_CREATE_OBJECT:
				CreateObject((TPacketGDCreateObject*)data);
				break;

			case HEADER_GD_DELETE_OBJECT:
				DeleteObject(*(DWORD*)data);
				break;

			case HEADER_GD_UPDATE_LAND:
				UpdateLand((DWORD*)data);
				break;

			case HEADER_GD_MARRIAGE_ADD:
				MarriageAdd((TPacketMarriageAdd*)data);
				break;

			case HEADER_GD_MARRIAGE_UPDATE:
				MarriageUpdate((TPacketMarriageUpdate*)data);
				break;

			case HEADER_GD_MARRIAGE_REMOVE:
				MarriageRemove((TPacketMarriageRemove*)data);
				break;

			case HEADER_GD_WEDDING_REQUEST:
				WeddingRequest((TPacketWeddingRequest*)data);
				break;

			case HEADER_GD_WEDDING_READY:
				WeddingReady((TPacketWeddingReady*)data);
				break;

			case HEADER_GD_WEDDING_END:
				WeddingEnd((TPacketWeddingEnd*)data);
				break;

				// BLOCK_CHAT
			case HEADER_GD_BLOCK_CHAT:
				BlockChat((TPacketBlockChat*)data);
				break;
				// END_OF_BLOCK_CHAT

				// MYSHOP_PRICE_LIST
			case HEADER_GD_MYSHOP_PRICELIST_UPDATE:
				MyshopPricelistUpdate((TItemPriceListTable*)data);
				break;

			case HEADER_GD_MYSHOP_PRICELIST_REQ:
				MyshopPricelistRequest(peer, dwHandle, *(DWORD*)data);
				break;
				// END_OF_MYSHOP_PRICE_LIST

				// RELOAD_ADMIN
			case HEADER_GD_RELOAD_ADMIN:
				ReloadAdmin(peer, (TPacketReloadAdmin*)data);
				break;
				// END_RELOAD_ADMIN

			case HEADER_GD_BREAK_MARRIAGE:
				BreakMarriage(peer, data);
				break;

				// MOANRCH
			case HEADER_GD_ELECT_MONARCH:
				Election(peer, dwHandle, data);
				break;

			case HEADER_GD_CANDIDACY:
				Candidacy(peer, dwHandle, data);
				break;

			case HEADER_GD_ADD_MONARCH_MONEY:
				AddMonarchMoney(peer, dwHandle, data);
				break;

			case HEADER_GD_DEC_MONARCH_MONEY:
				DecMonarchMoney(peer, dwHandle, data);
				break;

			case HEADER_GD_TAKE_MONARCH_MONEY:
				TakeMonarchMoney(peer, dwHandle, data);
				break;

			case HEADER_GD_COME_TO_VOTE:
				ComeToVote(peer, dwHandle, data);
				break;

			case HEADER_GD_RMCANDIDACY: ///< 후보 제거 (운영자)
				RMCandidacy(peer, dwHandle, data);
				break;

			case HEADER_GD_SETMONARCH: ///< 군주설정 (운영자)
				SetMonarch(peer, dwHandle, data);
				break;

			case HEADER_GD_RMMONARCH: ///< 군주삭제
				RMMonarch(peer, dwHandle, data);
				break;
				// END_MONARCH

			case HEADER_GD_CHANGE_MONARCH_LORD:
				ChangeMonarchLord(peer, dwHandle, (TPacketChangeMonarchLord*)data);
				break;

			case HEADER_GD_BLOCK_COUNTRY_IP:
				sys_log(0, "HEADER_GD_BLOCK_COUNTRY_IP received");
				CBlockCountry::instance().SendBlockedCountryIp(peer);
				CBlockCountry::instance().SendBlockException(peer);
				break;

			case HEADER_GD_BLOCK_EXCEPTION:
				sys_log(0, "HEADER_GD_BLOCK_EXCEPTION received");
				BlockException((TPacketBlockException*)data);
				break;

			case HEADER_GD_REQ_SPARE_ITEM_ID_RANGE:
				SendSpareItemIDRange(peer);
				break;

			case HEADER_GD_REQ_CHANGE_GUILD_MASTER:
				GuildChangeMaster((TPacketChangeGuildMaster*)data);
				break;

			case HEADER_GD_UPDATE_HORSE_NAME:
				UpdateHorseName((TPacketUpdateHorseName*)data, peer);
				break;

			case HEADER_GD_REQ_HORSE_NAME:
				AckHorseName(*(DWORD*)data, peer);
				break;

			case HEADER_GD_DC:
				DeleteLoginKey((TPacketDC*)data);
				break;

			case HEADER_GD_VALID_LOGOUT:
				ResetLastPlayerID((TPacketNeedLoginLogInfo*)data);
				break;

			case HEADER_GD_REQUEST_CHARGE_CASH:
				ChargeCash((TRequestChargeCash*)data);
				break;

				// delete gift notify icon
			case HEADER_GD_DELETE_AWARDID:
				DeleteAwardId((TPacketDeleteAwardID*)data);
				break;

			case HEADER_GD_UPDATE_CHANNELSTATUS:
				UpdateChannelStatus((SChannelStatus*)data);
				break;

			case HEADER_GD_REQUEST_CHANNELSTATUS:
				RequestChannelStatus(peer, dwHandle);
				break;

#if defined(__MOVE_CHANNEL__)
			case HEADER_GD_FIND_CHANNEL:
				FindChannel(peer, dwHandle, (TPacketChangeChannel*)data);
				break;
#endif

#if defined(__GEM_SHOP__)
			case HEADER_GD_GEM_SHOP_LOAD:
				LoadGemShop(peer, dwHandle, (TGemShopLoad*)data);
				break;

			case HEADER_GD_GEM_SHOP_UPDATE:
				UpdateGemShop(peer, dwHandle, (TGemShopTable*)data);
				break;
#endif

#if defined(__EXPRESSING_EMOTIONS__)
			case HEADER_GD_EMOTE_LOAD:
				QUERY_EMOTE_LOAD(peer, dwHandle, (TPacketGDEmote*)data);
				break;

			case HEADER_GD_EMOTE_CLEAR:
				QUERY_EMOTE_CLEAR(peer, dwHandle, (TPacketGDEmote*)data);
				break;

			case HEADER_GD_EMOTE_ADD:
				QUERY_EMOTE_ADD(peer, dwHandle, (TPacketGDEmote*)data);
				break;
#endif

#if defined(__MAILBOX__)
			case HEADER_GD_MAILBOX_LOAD:
				QUERY_MAILBOX_LOAD(peer, dwHandle, (TMailBox*)data);
				break;

			case HEADER_GD_MAILBOX_CHECK_NAME:
				QUERY_MAILBOX_CHECK_NAME(peer, dwHandle, (TMailBox*)data);
				break;

			case HEADER_GD_MAILBOX_WRITE:
				QUERY_MAILBOX_WRITE(peer, dwHandle, (TMailBoxTable*)data);
				break;

			case HEADER_GD_MAILBOX_DELETE:
				QUERY_MAILBOX_DELETE(peer, dwHandle, (TMailBox*)data);
				break;

			case HEADER_GD_MAILBOX_CONFIRM:
				QUERY_MAILBOX_CONFIRM(peer, dwHandle, (TMailBox*)data);
				break;

			case HEADER_GD_MAILBOX_GET:
				QUERY_MAILBOX_GET(peer, dwHandle, (TMailBox*)data);
				break;

			case HEADER_GD_MAILBOX_UNREAD:
				QUERY_MAILBOX_UNREAD(peer, dwHandle, (TMailBox*)data);
				break;
#endif

			default:
				sys_err("Unknown header (header: %d handle: %d length: %d)", header, dwHandle, dwLength);
				break;
		}
	}

	peer->RecvEnd(i);
}

void CClientManager::AddPeer(socket_t fd)
{
	CPeer* pPeer = new CPeer;

	if (pPeer->Accept(fd))
		m_peerList.push_front(pPeer);
	else
		delete pPeer;
}

void CClientManager::RemovePeer(CPeer* pPeer)
{
	if (m_pkAuthPeer == pPeer)
	{
		m_pkAuthPeer = NULL;
	}
	else
	{
		TLogonAccountMap::iterator it = m_map_kLogonAccount.begin();

		while (it != m_map_kLogonAccount.end())
		{
			CLoginData* pkLD = it->second;

			if (pkLD->GetConnectedPeerHandle() == pPeer->GetHandle())
			{
				if (pkLD->IsPlay())
					pkLD->SetPlay(false);

				if (pkLD->IsDeleted())
				{
					sys_log(0, "DELETING LoginData");
					delete pkLD;
				}

				m_map_kLogonAccount.erase(it++);
			}
			else
				++it;
		}
	}

	m_peerList.remove(pPeer);
	delete pPeer;
}

CPeer* CClientManager::GetPeer(IDENT ident)
{
	// for (TPeerList::const_iterator i = m_peerList.begin(); i != m_peerList.end(); ++i)
	for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
	{
		CPeer* tmp = *i;

		if (tmp->GetHandle() == ident)
			return tmp;
	}

	return NULL;
}

CPeer* CClientManager::GetAnyPeer()
{
	if (m_peerList.empty())
		return NULL;

	return m_peerList.front();
}

// DB 매니저로 부터 받은 결과를 처리한다.
//
// @version	05/06/10 Bang2ni - 가격정보 관련 쿼리(QID_ITEMPRICE_XXX) 추가
int CClientManager::AnalyzeQueryResult(SQLMsg* msg)
{
	CQueryInfo* qi = (CQueryInfo*)msg->pvUserData;
	CPeer* peer = GetPeer(qi->dwIdent);

#if defined(_DEBUG)
	if (qi->iType != QID_ITEM_AWARD_LOAD)
		sys_log(0, "AnalyzeQueryResult %d", qi->iType);
#endif

	switch (qi->iType)
	{
		case QID_ITEM_AWARD_LOAD:
			ItemAwardManager::instance().Load(msg);
			delete qi;
			return true;

		case QID_GUILD_RANKING:
			CGuildManager::instance().ResultRanking(msg->Get()->pSQLResult);
			break;

			// MYSHOP_PRICE_LIST
		case QID_ITEMPRICE_LOAD_FOR_UPDATE:
			RESULT_PRICELIST_LOAD_FOR_UPDATE(msg);
			break;
			// END_OF_MYSHOP_PRICE_LIST
	}

	if (!peer)
	{
		//sys_err("CClientManager::AnalyzeQueryResult: peer not exist anymore. (ident: %d)", qi->dwIdent);
		delete qi;
		return true;
	}

	switch (qi->iType)
	{
		case QID_PLAYER:
		case QID_ITEM:
		case QID_QUEST:
		case QID_AFFECT:
			RESULT_COMPOSITE_PLAYER(peer, msg, qi->iType);
			break;

		case QID_LOGIN:
			RESULT_LOGIN(peer, msg);
			break;

		case QID_SAFEBOX_LOAD:
			sys_log(0, "QUERY_RESULT: HEADER_GD_SAFEBOX_LOAD");
			RESULT_SAFEBOX_LOAD(peer, msg);
			break;

		case QID_SAFEBOX_CHANGE_SIZE:
			sys_log(0, "QUERY_RESULT: HEADER_GD_SAFEBOX_CHANGE_SIZE");
			RESULT_SAFEBOX_CHANGE_SIZE(peer, msg);
			break;

		case QID_SAFEBOX_CHANGE_PASSWORD:
			sys_log(0, "QUERY_RESULT: HEADER_GD_SAFEBOX_CHANGE_PASSWORD %p", msg);
			RESULT_SAFEBOX_CHANGE_PASSWORD(peer, msg);
			break;

		case QID_SAFEBOX_CHANGE_PASSWORD_SECOND:
			sys_log(0, "QUERY_RESULT: HEADER_GD_SAFEBOX_CHANGE_PASSWORD %p", msg);
			RESULT_SAFEBOX_CHANGE_PASSWORD_SECOND(peer, msg);
			break;

		case QID_HIGHSCORE_REGISTER:
			sys_log(0, "QUERY_RESULT: HEADER_GD_HIGHSCORE_REGISTER %p", msg);
			RESULT_HIGHSCORE_REGISTER(peer, msg);
			break;

		case QID_SAFEBOX_SAVE:
		case QID_ITEM_SAVE:
		case QID_ITEM_DESTROY:
		case QID_QUEST_SAVE:
		case QID_PLAYER_SAVE:
		case QID_ITEM_AWARD_TAKEN:
			break;

			// PLAYER_INDEX_CREATE_BUG_FIX
		case QID_PLAYER_INDEX_CREATE:
			RESULT_PLAYER_INDEX_CREATE(peer, msg);
			break;
			// END_PLAYER_INDEX_CREATE_BUG_FIX

		case QID_PLAYER_DELETE:
			__RESULT_PLAYER_DELETE(peer, msg);
			break;

		case QID_LOGIN_BY_KEY:
			RESULT_LOGIN_BY_KEY(peer, msg);
			break;

			// MYSHOP_PRICE_LIST
		case QID_ITEMPRICE_LOAD:
			RESULT_PRICELIST_LOAD(peer, msg);
			break;
			// END_OF_MYSHOP_PRICE_LIST

		default:
			sys_log(0, "CClientManager::AnalyzeQueryResult unknown query result type: %d, str: %s", qi->iType, msg->stQuery.c_str());
			break;
	}

	delete qi;
	return true;
}

void UsageLog()
{
	FILE* fp = NULL;

	time_t ct;
	char* time_s;
	struct tm lt;

	int avg = g_dwUsageAvg / 3600; // 60 초 * 60 분

	fp = fopen("usage.txt", "a+");

	if (!fp)
		return;

	ct = time(0);
	lt = *localtime(&ct);
	time_s = asctime(&lt);

	time_s[strlen(time_s) - 1] = '\0';

	fprintf(fp, "| %4d %-15.15s | %5d | %5u |", lt.tm_year + 1900, time_s + 4, avg, g_dwUsageMax);

	fprintf(fp, "\n");
	fclose(fp);

	g_dwUsageMax = g_dwUsageAvg = 0;
}

int CClientManager::Process()
{
	int pulses;

	if (!(pulses = thecore_idle()))
		return 0;

	while (pulses--)
	{
		++thecore_heart->pulse;

		/*
		// 30분마다 변경
		if (((thecore_pulse() % (60 * 30 * 10)) == 0))
		{
			g_iPlayerCacheFlushSeconds = MAX(60, rand() % 180);
			g_iItemCacheFlushSeconds = MAX(60, rand() % 180);
			sys_log(0, "[SAVE_TIME]Change saving time item %d player %d", g_iPlayerCacheFlushSeconds, g_iItemCacheFlushSeconds);
		}
		*/

		if (!(thecore_heart->pulse % thecore_heart->passes_per_sec))
		{
			if (g_test_server)
			{
				if (!(thecore_heart->pulse % thecore_heart->passes_per_sec * 10))

				{
					pt_log("[%9d] return %d/%d/%d/%d async %d/%d/%d/%d",
						thecore_heart->pulse,
						CDBManager::instance().CountReturnQuery(SQL_PLAYER),
						CDBManager::instance().CountReturnResult(SQL_PLAYER),
						CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER),
						CDBManager::instance().CountReturnCopiedQuery(SQL_PLAYER),
						CDBManager::instance().CountAsyncQuery(SQL_PLAYER),
						CDBManager::instance().CountAsyncResult(SQL_PLAYER),
						CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER),
						CDBManager::instance().CountAsyncCopiedQuery(SQL_PLAYER));

					if ((thecore_heart->pulse % 50) == 0)
						sys_log(0, "[%9d] return %d/%d/%d async %d/%d/%d",
							thecore_heart->pulse,
							CDBManager::instance().CountReturnQuery(SQL_PLAYER),
							CDBManager::instance().CountReturnResult(SQL_PLAYER),
							CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER),
							CDBManager::instance().CountAsyncQuery(SQL_PLAYER),
							CDBManager::instance().CountAsyncResult(SQL_PLAYER),
							CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER));
				}
			}
			else
			{
				pt_log("[%9d] return %d/%d/%d/%d async %d/%d/%d%/%d",
					thecore_heart->pulse,
					CDBManager::instance().CountReturnQuery(SQL_PLAYER),
					CDBManager::instance().CountReturnResult(SQL_PLAYER),
					CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER),
					CDBManager::instance().CountReturnCopiedQuery(SQL_PLAYER),
					CDBManager::instance().CountAsyncQuery(SQL_PLAYER),
					CDBManager::instance().CountAsyncResult(SQL_PLAYER),
					CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER),
					CDBManager::instance().CountAsyncCopiedQuery(SQL_PLAYER));

				if ((thecore_heart->pulse % 50) == 0)
					sys_log(0, "[%9d] return %d/%d/%d async %d/%d/%d",
						thecore_heart->pulse,
						CDBManager::instance().CountReturnQuery(SQL_PLAYER),
						CDBManager::instance().CountReturnResult(SQL_PLAYER),
						CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER),
						CDBManager::instance().CountAsyncQuery(SQL_PLAYER),
						CDBManager::instance().CountAsyncResult(SQL_PLAYER),
						CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER));
			}

			CDBManager::instance().ResetCounter();

			DWORD dwCount = CClientManager::instance().GetUserCount();

			g_dwUsageAvg += dwCount;
			g_dwUsageMax = MAX(g_dwUsageMax, dwCount);

			memset(&thecore_profiler[0], 0, sizeof(thecore_profiler));

#if defined(__EXPRESSING_EMOTIONS__)
			if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * m_iEmoteDumpDelay)))
				CClientManager::instance().QUERY_EMOTE_DUMP();
#endif

#if defined(__MAILBOX__)
			if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * m_iMailBoxBackupSec)))
				CClientManager::instance().MAILBOX_BACKUP();
#endif

#if defined(__GEM_SHOP__)
			if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * m_iGemShopFlushDelay)))
				CClientManager::instance().FlushGemShop();
#endif

			if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 3600)))
				UsageLog();

			m_iCacheFlushCount = 0;

			// 플레이어 플러쉬
			UpdatePlayerCache();
			// 아이템 플러쉬
			UpdateItemCache();
			//로그아웃시 처리- 캐쉬셋 플러쉬
			UpdateLogoutPlayer();

			// MYSHOP_PRICE_LIST
			UpdateItemPriceListCache();
			// END_OF_MYSHOP_PRICE_LIST

			CGuildManager::instance().Update();
			CPrivManager::instance().Update();
			marriage::CManager::instance().Update();
		}

		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 5)))
		{
			ItemAwardManager::instance().RequestLoad();
		}

		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 10)))
		{
			/*
			char buf[4096 + 1];
			int len

			/////////////////////////////////////////////////////////////////
			buf[0] = '\0';
			len = 0;

			auto it = g_query_info.m_map_info.begin();

			int count = 0;

			while (it != g_query_info.m_map_info.end())
			{
				len += snprintf(buf + len, sizeof(buf) - len, "%2d %3d\n", it->first, it->second);
				count += it->second;
				it++;
			}

			pt_log("QUERY:\n%s-------------------- MAX : %d\n", buf, count);
			g_query_info.Reset();
			*/
			pt_log("QUERY: MAIN[%d] ASYNC[%d]", g_query_count[0], g_query_count[1]);
			g_query_count[0] = 0;
			g_query_count[1] = 0;
			/////////////////////////////////////////////////////////////////

			/////////////////////////////////////////////////////////////////
			/*
			buf[0] = '\0';
			len = 0;

			it = g_item_info.m_map_info.begin();

			count = 0;
			while (it != g_item_info.m_map_info.end())
			{
				len += snprintf(buf + len, sizeof(buf) - len, "%5d %3d\n", it->first, it->second);
				count += it->second;
				it++;
			}

			pt_log("ITEM:\n%s-------------------- MAX : %d\n", buf, count);
			g_item_info.Reset();
			*/
			pt_log("ITEM:%d\n", g_item_count);
			g_item_count = 0;
			/////////////////////////////////////////////////////////////////
		}

		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 60))) // 60초에 한번
		{
			// 유니크 아이템을 위한 시간을 보낸다.
			CClientManager::instance().SendTime();
		}

		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 3600))) // 한시간에 한번
		{
			CMoneyLog::instance().Save();
		}

#if defined(__ENVIRONMENT_SYSTEM__)
		static bool OnSetup = true;
		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 60 * 20)) || OnSetup)
		{
			UpdateEnvironment();
			OnSetup = false;
		}
#endif
	}

	int num_events = fdwatch(m_fdWatcher, 0);
	int idx;
	CPeer* peer;

	for (idx = 0; idx < num_events; ++idx) // 인풋
	{
		peer = (CPeer*)fdwatch_get_client_data(m_fdWatcher, idx);

		if (!peer)
		{
			if (fdwatch_check_event(m_fdWatcher, m_fdAccept, idx) == FDW_READ)
			{
				AddPeer(m_fdAccept);
				fdwatch_clear_event(m_fdWatcher, m_fdAccept, idx);
			}
			else
			{
				sys_log(0, "FDWATCH: peer null in event: ident %d", fdwatch_get_ident(m_fdWatcher, idx));
			}

			continue;
		}

		switch (fdwatch_check_event(m_fdWatcher, peer->GetFd(), idx))
		{
			case FDW_READ:
				if (peer->Recv() < 0)
				{
					sys_err("Recv failed");
					RemovePeer(peer);
				}
				else
				{
					if (peer == m_pkAuthPeer)
						if (g_log)
							sys_log(0, "AUTH_PEER_READ: size %d", peer->GetRecvLength());

					ProcessPackets(peer);
				}
				break;

			case FDW_WRITE:
				if (peer == m_pkAuthPeer)
					if (g_log)
						sys_log(0, "AUTH_PEER_WRITE: size %d", peer->GetSendLength());

				if (peer->Send() < 0)
				{
					sys_err("Send failed");
					RemovePeer(peer);
				}

				break;

			case FDW_EOF:
				RemovePeer(peer);
				break;

			default:
				sys_err("fdwatch_check_fd returned unknown result");
				RemovePeer(peer);
				break;
		}
	}

#ifdef __WIN32__
	if (_kbhit())
	{
		int c = _getch();
		switch (c)
		{
			case 0x1b: // Esc
				return 0; // shutdown
				break;
			default:
				break;
		}
	}
#endif

	return 1;
}

DWORD CClientManager::GetUserCount()
{
	// 단순히 로그인 카운트를 센다.. --;
	return m_map_kLogonAccount.size();
}

void CClientManager::SendAllGuildSkillRechargePacket()
{
	ForwardPacket(HEADER_DG_GUILD_SKILL_RECHARGE, NULL, 0);
}

void CClientManager::SendTime()
{
	time_t now = GetCurrentTime();
	ForwardPacket(HEADER_DG_TIME, &now, sizeof(time_t));
}

void CClientManager::ForwardPacket(BYTE header, const void* data, int size, BYTE bChannel, CPeer* except)
{
	//for (TPeerList::const_iterator it = m_peerList.begin(); it != m_peerList.end(); ++it)
	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;

		if (peer == except)
			continue;

		if (!peer->GetChannel())
			continue;

		if (bChannel && peer->GetChannel() != bChannel)
			continue;

		peer->EncodeHeader(header, 0, size);

		if (size > 0 && data)
			peer->Encode(data, size);
	}
}

void CClientManager::SendNotice(const char* c_pszFormat, ...)
{
	char szBuf[255 + 1];
	va_list args;

	va_start(args, c_pszFormat);
	int len = vsnprintf(szBuf, sizeof(szBuf), c_pszFormat, args);
	va_end(args);
	szBuf[len] = '\0';

	ForwardPacket(HEADER_DG_NOTICE, szBuf, len + 1);
}

time_t CClientManager::GetCurrentTime()
{
	return time(0);
}

// ITEM_UNIQUE_ID
bool CClientManager::InitializeNowItemID()
{
	DWORD dwMin, dwMax;

	// 아이템 ID를 초기화 한다.
	if (!CConfig::instance().GetTwoValue("ITEM_ID_RANGE", &dwMin, &dwMax))
	{
		sys_err("config.txt: Cannot find ITEM_ID_RANGE [start_item_id] [end_item_id]");
		return false;
	}

	sys_log(0, "ItemRange From File %u ~ %u ", dwMin, dwMax);

	if (CItemIDRangeManager::instance().BuildRange(dwMin, dwMax, m_itemRange) == false)
	{
		sys_err("Can not build ITEM_ID_RANGE");
		return false;
	}

	sys_log(0, " Init Success Start %u End %u Now %u\n", m_itemRange.dwMin, m_itemRange.dwMax, m_itemRange.dwUsableItemIDMin);

	return true;
}

DWORD CClientManager::GainItemID()
{
	return m_itemRange.dwUsableItemIDMin++;
}

DWORD CClientManager::GetItemID()
{
	return m_itemRange.dwUsableItemIDMin;
}
// ITEM_UNIQUE_ID_END

// BOOT_LOCALIZATION
bool CClientManager::InitializeLocalization()
{
	char szQuery[512];
	snprintf(szQuery, sizeof(szQuery), "SELECT `mValue`, `mKey` FROM `locale`");
	SQLMsg* pMsg = CDBManager::instance().DirectQuery(szQuery, SQL_COMMON);

	if (pMsg->Get()->uiNumRows == 0)
	{
		sys_err("InitializeLocalization() ==> DirectQuery failed(%s)", szQuery);
		delete pMsg;
		return false;
	}

	sys_log(0, "InitializeLocalization() - LoadLocaleTable(count:%d)", pMsg->Get()->uiNumRows);

	m_vec_Locale.clear();

	MYSQL_ROW row = NULL;

	for (; (row = mysql_fetch_row(pMsg->Get()->pSQLResult)) != NULL;)
	{
		int col = 0;
		tLocale locale;

		strlcpy(locale.szValue, row[col++], sizeof(locale.szValue));
		strlcpy(locale.szKey, row[col++], sizeof(locale.szKey));

		// DB_NAME_COLUMN Setting
		if (strcmp(locale.szKey, "LOCALE") == 0)
		{
			if (strcmp(locale.szValue, "cibn") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "gb2312");

				g_stLocale = "gb2312";
				g_stLocaleNameColumn = "gb2312name";
			}
			else if (strcmp(locale.szValue, "ymir") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "euckr";
				g_stLocaleNameColumn = "name";
			}
			else if (strcmp(locale.szValue, "japan") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "sjis");

				g_stLocale = "sjis";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "english") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "germany") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "france") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "italy") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "spain") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "uk") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "turkey") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin5";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "poland") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin2";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "portugal") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "hongkong") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "big5");

				g_stLocale = "big5";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "newcibn") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "gb2312");

				g_stLocale = "gb2312";
				g_stLocaleNameColumn = "gb2312name";
			}
			else if (strcmp(locale.szValue, "korea") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "euckr";
				g_stLocaleNameColumn = "name";
			}
			else if (strcmp(locale.szValue, "canada") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "gb2312name";
			}
			else if (strcmp(locale.szValue, "brazil") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "greek") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "greek";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "russia") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "cp1251";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "denmark") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "bulgaria") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "cp1251";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "croatia") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "cp1251";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "mexico") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "arabia") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "cp1256";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "czech") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin2";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "hungary") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin2";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "romania") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin2";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "netherlands") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "singapore") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "vietnam") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "thailand") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "usa") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "we_korea") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "euckr");

				g_stLocale = "euckr";
				g_stLocaleNameColumn = "name";
			}
			else if (strcmp(locale.szValue, "taiwan") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "big5");
				g_stLocale = "big5";
				g_stLocaleNameColumn = "locale_name";
			}
			else if (strcmp(locale.szValue, "europe") == 0)
			{
				sys_log(0, "locale[LOCALE] = %s", locale.szValue);

				if (g_stLocale != locale.szValue)
					sys_log(0, "Changed g_stLocale %s to %s", g_stLocale.c_str(), "latin1");

				g_stLocale = "latin1";
				g_stLocaleNameColumn = "locale_name";
			}
			else
			{
				sys_err("locale[LOCALE] = UNKNOWN(%s)", locale.szValue);
				exit(0);
			}

			CDBManager::instance().SetLocale(g_stLocale.c_str());
		}
		else if (strcmp(locale.szKey, "DB_NAME_COLUMN") == 0)
		{
			sys_log(0, "locale[DB_NAME_COLUMN] = %s", locale.szValue);
			g_stLocaleNameColumn = locale.szValue;
		}
		else
		{
			sys_log(0, "locale[UNKNOWN_KEY(%s)] = %s", locale.szKey, locale.szValue);
		}
		m_vec_Locale.push_back(locale);
	}

	delete pMsg;

	return true;
}
// END_BOOT_LOCALIZATION

// ADMIN_MANAGER
bool CClientManager::__GetAdminInfo(const char* szIP, std::vector<tAdminInfo>& rAdminVec)
{
	// szIP == NULL 일경우 모든서버에 운영자 권한을 갖는다.
	char szQuery[512];
	snprintf(szQuery, sizeof(szQuery),
		"SELECT `mID`, `mAccount`, `mName`, `mContactIP`, `mServerIP`, `mAuthority` FROM `gmlist` WHERE `mServerIP` = 'ALL' OR `mServerIP` = '%s'",
		szIP ? szIP : "ALL");

	SQLMsg* pMsg = CDBManager::instance().DirectQuery(szQuery, SQL_COMMON);

	if (pMsg->Get()->uiNumRows == 0)
	{
		sys_err("__GetAdminInfo() ==> DirectQuery failed(%s)", szQuery);
		delete pMsg;
		return false;
	}

	MYSQL_ROW row;
	rAdminVec.reserve(pMsg->Get()->uiNumRows);

	while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
	{
		int idx = 0;
		tAdminInfo Info;

		str_to_number(Info.m_ID, row[idx++]);
		trim_and_lower(row[idx++], Info.m_szAccount, sizeof(Info.m_szAccount));
		strlcpy(Info.m_szName, row[idx++], sizeof(Info.m_szName));
		strlcpy(Info.m_szContactIP, row[idx++], sizeof(Info.m_szContactIP));
		strlcpy(Info.m_szServerIP, row[idx++], sizeof(Info.m_szServerIP));
		std::string stAuth = row[idx++];

		if (!stAuth.compare("IMPLEMENTOR"))
			Info.m_Authority = GM_IMPLEMENTOR;
		else if (!stAuth.compare("GOD"))
			Info.m_Authority = GM_GOD;
		else if (!stAuth.compare("HIGH_WIZARD"))
			Info.m_Authority = GM_HIGH_WIZARD;
		else if (!stAuth.compare("LOW_WIZARD"))
			Info.m_Authority = GM_LOW_WIZARD;
		else if (!stAuth.compare("WIZARD"))
			Info.m_Authority = GM_WIZARD;
		else
			continue;

		rAdminVec.push_back(Info);

		sys_log(0, "GM: PID %u Login %s Character %s ContactIP %s ServerIP %s Authority %d[%s]",
			Info.m_ID, Info.m_szAccount, Info.m_szName, Info.m_szContactIP, Info.m_szServerIP, Info.m_Authority, stAuth.c_str());
	}

	delete pMsg;

	return true;
}

bool CClientManager::__GetHostInfo(std::vector<std::string>& rIPVec)
{
	char szQuery[512];
	snprintf(szQuery, sizeof(szQuery), "SELECT `mIP` FROM `gmhost`");
	SQLMsg* pMsg = CDBManager::instance().DirectQuery(szQuery, SQL_COMMON);

	if (pMsg->Get()->uiNumRows == 0)
	{
		sys_err("__GetHostInfo() ==> DirectQuery failed(%s)", szQuery);
		delete pMsg;
		return false;
	}

	rIPVec.reserve(pMsg->Get()->uiNumRows);

	MYSQL_ROW row;

	while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
	{
		if (row[0] && *row[0])
		{
			rIPVec.push_back(row[0]);
			sys_log(0, "GMHOST: %s", row[0]);
		}
	}

	delete pMsg;
	return true;
}
// END_ADMIN_MANAGER

void CClientManager::ReloadAdmin(CPeer*, TPacketReloadAdmin* p)
{
	std::vector<tAdminInfo> vAdmin;
	std::vector<std::string> vHost;

	__GetHostInfo(vHost);
	__GetAdminInfo(p->szIP, vAdmin);

	DWORD dwPacketSize = sizeof(WORD) + sizeof(WORD) + sizeof(tAdminInfo) * vAdmin.size() +
		sizeof(WORD) + sizeof(WORD) + 16 * vHost.size();

	//for (TPeerList::const_iterator it = m_peerList.begin(); it != m_peerList.end(); ++it)
	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;

		if (!peer->GetChannel())
			continue;

		peer->EncodeHeader(HEADER_DG_RELOAD_ADMIN, 0, dwPacketSize);

		peer->EncodeWORD(16);
		peer->EncodeWORD(static_cast<WORD>(vHost.size()));

		for (size_t n = 0; n < vHost.size(); ++n)
			peer->Encode(vHost[n].c_str(), 16);

		peer->EncodeWORD(sizeof(tAdminInfo));
		peer->EncodeWORD(static_cast<WORD>(vAdmin.size()));

		for (size_t n = 0; n < vAdmin.size(); ++n)
			peer->Encode(&vAdmin[n], sizeof(tAdminInfo));
	}

	sys_log(0, "ReloadAdmin End %s", p->szIP);
}

// BREAK_MARRIAGE
void CClientManager::BreakMarriage(CPeer* peer, const char* data)
{
	DWORD pid1, pid2;

	pid1 = *(int*)data;
	data += sizeof(int);

	pid2 = *(int*)data;
	data += sizeof(int);

	sys_log(0, "Breaking off a marriage engagement! pid %d and pid %d", pid1, pid2);
	marriage::CManager::instance().Remove(pid1, pid2);
}
// END_BREAK_MARIIAGE

void CClientManager::UpdateItemCacheSet(DWORD pid)
{
	auto it = m_map_pkItemCacheSetPtr.find(pid);
	if (it == m_map_pkItemCacheSetPtr.end())
	{
		if (g_test_server)
			sys_log(0, "UPDATE_ITEMCACHESET : UpdateItemCacheSet ==> No ItemCacheSet pid(%d)", pid);
		return;
	}

	TItemCacheSet* pSet = it->second;
	TItemCacheSet::iterator it_set = pSet->begin();

	while (it_set != pSet->end())
	{
		CItemCache* c = *it_set++;
		c->Flush();
	}

	if (g_log)
		sys_log(0, "UPDATE_ITEMCACHESET : UpdateItemCachsSet pid(%d)", pid);
}

void CClientManager::Election(CPeer* peer, DWORD dwHandle, const char* data)
{
	DWORD idx;
	DWORD selectingpid;

	idx = *(DWORD*)data;
	data += sizeof(DWORD);

	selectingpid = *(DWORD*)data;
	data += sizeof(DWORD);

	int Success = 0;

	if (!(Success = CMonarch::instance().VoteMonarch(selectingpid, idx)))
	{
		if (g_test_server)
			sys_log(0, "[MONARCH_VOTE] Failed %d %d", idx, selectingpid);
		peer->EncodeHeader(HEADER_DG_ELECT_MONARCH, dwHandle, sizeof(int));
		peer->Encode(&Success, sizeof(int));
		return;
	}
	else
	{
		if (g_test_server)
			sys_log(0, "[MONARCH_VOTE] Success %d %d", idx, selectingpid);
		peer->EncodeHeader(HEADER_DG_ELECT_MONARCH, dwHandle, sizeof(int));
		peer->Encode(&Success, sizeof(int));
		return;
	}

}

void CClientManager::Candidacy(CPeer* peer, DWORD dwHandle, const char* data)
{
	DWORD pid;

	pid = *(DWORD*)data;
	data += sizeof(DWORD);

	if (!CMonarch::instance().AddCandidacy(pid, data))
	{
		if (g_test_server)
			sys_log(0, "[MONARCH_CANDIDACY] Failed %d %s", pid, data);

		peer->EncodeHeader(HEADER_DG_CANDIDACY, dwHandle, sizeof(int) + 32);
		peer->Encode(0, sizeof(int));
		peer->Encode(data, 32);
		return;
	}
	else
	{
		if (g_test_server)
			sys_log(0, "[MONARCH_CANDIDACY] Success %d %s", pid, data);

		// for (TPeerList::const_iterator it = m_peerList.begin(); it != m_peerList.end(); ++it)
		for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
		{
			CPeer* p = *it;

			if (!p->GetChannel())
				continue;

			if (0 && p->GetChannel() != 0)
				continue;

			if (p == peer)
			{
				p->EncodeHeader(HEADER_DG_CANDIDACY, dwHandle, sizeof(int) + 32);
				p->Encode(&pid, sizeof(int));
				p->Encode(data, 32);
			}
			else
			{
				p->EncodeHeader(HEADER_DG_CANDIDACY, 0, sizeof(int) + 32);
				p->Encode(&pid, sizeof(int));
				p->Encode(data, 32);
			}
		}
	}
}

void CClientManager::AddMonarchMoney(CPeer* peer, DWORD dwHandle, const char* data)
{
	int Empire = *(int*)data;
	data += sizeof(int);

	int Money = *(int*)data;
	data += sizeof(int);

	if (g_test_server)
		sys_log(0, "[MONARCH] Add money Empire(%d) Money(%d)", Empire, Money);

	CMonarch::instance().AddMoney(Empire, Money);

	// for (TPeerList::const_iterator it = m_peerList.begin(); it != m_peerList.end(); ++it)
	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* p = *it;

		if (!p->GetChannel())
			continue;

		if (p == peer)
		{
			p->EncodeHeader(HEADER_DG_ADD_MONARCH_MONEY, dwHandle, sizeof(int) + sizeof(int));
			p->Encode(&Empire, sizeof(int));
			p->Encode(&Money, sizeof(int));
		}
		else
		{
			p->EncodeHeader(HEADER_DG_ADD_MONARCH_MONEY, 0, sizeof(int) + sizeof(int));
			p->Encode(&Empire, sizeof(int));
			p->Encode(&Money, sizeof(int));
		}

	}
}

void CClientManager::DecMonarchMoney(CPeer* peer, DWORD dwHandle, const char* data)
{
	int Empire = *(int*)data;
	data += sizeof(int);

	int Money = *(int*)data;
	data += sizeof(int);

	if (g_test_server)
		sys_log(0, "[MONARCH] Dec money Empire(%d) Money(%d)", Empire, Money);

	CMonarch::instance().DecMoney(Empire, Money);

	// for (TPeerList::const_iterator it = m_peerList.begin(); it != m_peerList.end(); ++it)
	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* p = *it;

		if (!p->GetChannel())
			continue;

		if (p == peer)
		{
			p->EncodeHeader(HEADER_DG_DEC_MONARCH_MONEY, dwHandle, sizeof(int) + sizeof(int));
			p->Encode(&Empire, sizeof(int));
			p->Encode(&Money, sizeof(int));
		}
		else
		{
			p->EncodeHeader(HEADER_DG_DEC_MONARCH_MONEY, 0, sizeof(int) + sizeof(int));
			p->Encode(&Empire, sizeof(int));
			p->Encode(&Money, sizeof(int));
		}
	}
}

void CClientManager::TakeMonarchMoney(CPeer* peer, DWORD dwHandle, const char* data)
{
	int Empire = *(int*)data;
	data += sizeof(int);

	DWORD pid = *(DWORD*)data;
	data += sizeof(int);

	int Money = *(int*)data;
	data += sizeof(int);

	if (g_test_server)
		sys_log(0, "[MONARCH] Take money Empire(%d) Money(%d)", Empire, Money);

	if (CMonarch::instance().TakeMoney(Empire, pid, Money) == true)
	{
		peer->EncodeHeader(HEADER_DG_TAKE_MONARCH_MONEY, dwHandle, sizeof(int) + sizeof(int));
		peer->Encode(&Empire, sizeof(int));
		peer->Encode(&Money, sizeof(int));
	}
	else
	{
		Money = 0;
		peer->EncodeHeader(HEADER_DG_TAKE_MONARCH_MONEY, dwHandle, sizeof(int) + sizeof(int));
		peer->Encode(&Empire, sizeof(int));
		peer->Encode(&Money, sizeof(int));
	}
}

void CClientManager::ComeToVote(CPeer* peer, DWORD dwHandle, const char* data)
{
	CMonarch::instance().ElectMonarch();
}

void CClientManager::RMCandidacy(CPeer* peer, DWORD dwHandle, const char* data)
{
	char szName[32];

	strlcpy(szName, data, sizeof(szName));
	sys_log(0, "[MONARCH_GM] Remove candidacy name(%s)", szName);

	int iRet = CMonarch::instance().DelCandidacy(szName) ? 1 : 0;

	if (1 == iRet)
	{
		// for (TPeerList::const_iterator it = m_peerList.begin(); it != m_peerList.end(); ++it)
		for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
		{
			CPeer* p = *it;

			if (!p->GetChannel())
				continue;

			if (p == peer)
			{
				p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
			else
			{
				p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
		}
	}
	else
	{
		CPeer* p = peer;
		p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
		p->Encode(&iRet, sizeof(int));
		p->Encode(szName, sizeof(szName));
	}
}

void CClientManager::SetMonarch(CPeer* peer, DWORD dwHandle, const char* data)
{
	char szName[32];

	strlcpy(szName, data, sizeof(szName));

	if (g_test_server)
		sys_log(0, "[MONARCH_GM] Set Monarch name(%s)", szName);

	int iRet = CMonarch::instance().SetMonarch(szName) ? 1 : 0;

	if (1 == iRet)
	{
		// for (TPeerList::const_iterator it = m_peerList.begin(); it != m_peerList.end(); ++it)
		for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
		{
			CPeer* p = *it;

			if (!p->GetChannel())
				continue;

			if (p == peer)
			{
				p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
			else
			{
				p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
		}
	}
	else
	{
		CPeer* p = peer;
		p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
		p->Encode(&iRet, sizeof(int));
		p->Encode(szName, sizeof(szName));
	}
}

void CClientManager::RMMonarch(CPeer* peer, DWORD dwHandle, const char* data)
{
	char szName[32];

	strlcpy(szName, data, sizeof(szName));

	if (g_test_server)
		sys_log(0, "[MONARCH_GM] Remove Monarch name(%s)", szName);

	CMonarch::instance().DelMonarch(szName);

	int iRet = CMonarch::instance().DelMonarch(szName) ? 1 : 0;

	if (1 == iRet)
	{
		// for (TPeerList::const_iterator it = m_peerList.begin(); it != m_peerList.end(); ++it)
		for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
		{
			CPeer* p = *it;

			if (!p->GetChannel())
				continue;

			if (p == peer)
			{
				p->EncodeHeader(HEADER_DG_RMMONARCH, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
			else
			{
				p->EncodeHeader(HEADER_DG_RMMONARCH, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
		}
	}
	else
	{
		CPeer* p = peer;
		p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
		p->Encode(&iRet, sizeof(int));
		p->Encode(szName, sizeof(szName));
	}
}

void CClientManager::ChangeMonarchLord(CPeer* peer, DWORD dwHandle, TPacketChangeMonarchLord* info)
{
	char szQuery[1024];
	snprintf(szQuery, sizeof(szQuery),
		"SELECT a.`name`, NOW() FROM player%s AS a, player_index%s AS b WHERE (a.`account_id` = b.`id` AND a.`id` = %u AND b.`empire` = %u) AND "
		"(b.`pid1` = %u OR "
		"b.`pid2` = %u OR "
		"b.`pid3` = %u OR "
		"b.`pid4` = %u"
#if defined(__PLAYER_PER_ACCOUNT5__)
		" OR b.`pid5` = %u"
#endif
		")",
		GetTablePostfix(),
		GetTablePostfix(),
		info->dwPID,
		info->bEmpire,
		info->dwPID,
		info->dwPID,
		info->dwPID,
		info->dwPID
#if defined(__PLAYER_PER_ACCOUNT5__)
		, info->dwPID
#endif
	);

	SQLMsg* pMsg = CDBManager::instance().DirectQuery(szQuery, SQL_PLAYER);

	if (pMsg->Get()->uiNumRows != 0)
	{
		TPacketChangeMonarchLordACK ack;
		ack.bEmpire = info->bEmpire;
		ack.dwPID = info->dwPID;

		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
		strlcpy(ack.szName, row[0], sizeof(ack.szName));
		strlcpy(ack.szDate, row[1], sizeof(ack.szDate));

		snprintf(szQuery, sizeof(szQuery), "UPDATE `monarch` SET `pid` = %u, `windate` = NOW() WHERE `empire` = %d", ack.dwPID, ack.bEmpire);
		SQLMsg* pMsg2 = CDBManager::instance().DirectQuery(szQuery, SQL_PLAYER);

		if (pMsg2->Get()->uiAffectedRows > 0)
		{
			CMonarch::instance().LoadMonarch();

			TMonarchInfo* newInfo = CMonarch::instance().GetMonarch();

			for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
			{
				CPeer* client = *it;

				client->EncodeHeader(HEADER_DG_CHANGE_MONARCH_LORD_ACK, 0, sizeof(TPacketChangeMonarchLordACK));
				client->Encode(&ack, sizeof(TPacketChangeMonarchLordACK));

				client->EncodeHeader(HEADER_DG_UPDATE_MONARCH_INFO, 0, sizeof(TMonarchInfo));
				client->Encode(newInfo, sizeof(TMonarchInfo));
			}
		}

		delete pMsg2;
	}

	delete pMsg;
}

void CClientManager::BlockException(TPacketBlockException* data)
{
	sys_log(0, "[BLOCK_EXCEPTION] CMD(%d) login(%s)", data->cmd, data->login);

	// save sql
	{
		char buf[1024];

		switch (data->cmd)
		{
			case BLOCK_EXCEPTION_CMD_ADD:
				snprintf(buf, sizeof(buf), "INSERT INTO `block_exception` VALUES('%s')", data->login);
				CDBManager::instance().AsyncQuery(buf, SQL_ACCOUNT);
				CBlockCountry::instance().AddBlockException(data->login);
				break;
			case BLOCK_EXCEPTION_CMD_DEL:
				snprintf(buf, sizeof(buf), "DELETE FROM `block_exception` WHERE `login` = %s", data->login);
				CDBManager::instance().AsyncQuery(buf, SQL_ACCOUNT);
				CBlockCountry::instance().DelBlockException(data->login);
				break;
			default:
				return;
		}

	}

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer* peer = *it;

		if (!peer->GetChannel())
			continue;

		CBlockCountry::instance().SendBlockExceptionOne(peer, data->login, data->cmd);
	}
}

void CClientManager::SendSpareItemIDRange(CPeer* peer)
{
	peer->SendSpareItemIDRange();
}

//
// Login Key만 맵에서 지운다.
//
void CClientManager::DeleteLoginKey(TPacketDC* data)
{
	char login[LOGIN_MAX_LEN + 1] = { 0 };
	trim_and_lower(data->login, login, sizeof(login));

	CLoginData* pkLD = GetLoginDataByLogin(login);

	if (pkLD)
	{
		TLoginDataByLoginKey::iterator it = m_map_pkLoginData.find(pkLD->GetKey());

		if (it != m_map_pkLoginData.end())
			m_map_pkLoginData.erase(it);
	}
}

// delete gift notify icon
void CClientManager::DeleteAwardId(TPacketDeleteAwardID* data)
{
	// sys_log(0, "data from game server arrived %d", data->dwID);
	std::map<DWORD, TItemAward*>::iterator it;
	it = ItemAwardManager::Instance().GetMapAward().find(data->dwID);
	if (it != ItemAwardManager::Instance().GetMapAward().end())
	{
		std::set<TItemAward*>& kSet = ItemAwardManager::Instance().GetMapkSetAwardByLogin()[it->second->szLogin];
		if (kSet.erase(it->second))
			sys_log(0, "erase ItemAward id: %d from cache", data->dwID);
		ItemAwardManager::Instance().GetMapAward().erase(data->dwID);
	}
	else
	{
		sys_log(0, "DELETE_AWARDID : could not find the id: %d", data->dwID);
	}
}

void CClientManager::UpdateChannelStatus(TChannelStatus* pData)
{
	TChannelStatusMap::iterator it = m_mChannelStatus.find(pData->nPort);
	if (it != m_mChannelStatus.end())
	{
		it->second = pData->bStatus;
	}
	else
	{
		m_mChannelStatus.insert(TChannelStatusMap::value_type(pData->nPort, pData->bStatus));
	}
}

void CClientManager::RequestChannelStatus(CPeer* peer, DWORD dwHandle)
{
	const int nSize = m_mChannelStatus.size();
	peer->EncodeHeader(HEADER_DG_RESPOND_CHANNELSTATUS, dwHandle, sizeof(TChannelStatus) * nSize + sizeof(int));
	peer->Encode(&nSize, sizeof(int));
	for (TChannelStatusMap::iterator it = m_mChannelStatus.begin(); it != m_mChannelStatus.end(); ++it)
	{
		peer->Encode(&it->first, sizeof(short));
		peer->Encode(&it->second, sizeof(BYTE));
	}
}

void CClientManager::ResetLastPlayerID(const TPacketNeedLoginLogInfo* data)
{
	CLoginData* pkLD = GetLoginDataByAID(data->dwPlayerID);

	if (NULL != pkLD)
	{
		pkLD->SetLastPlayerID(0);
	}
}

void CClientManager::ChargeCash(const TRequestChargeCash* packet)
{
	char szQuery[512];

	if (ERequestCharge_Cash == packet->eChargeType)
		sprintf(szQuery, "update account set `cash` = `cash` + %d where id = %d limit 1", packet->dwAmount, packet->dwAID);
	else if (ERequestCharge_Mileage == packet->eChargeType)
		sprintf(szQuery, "update account set `mileage` = `mileage` + %d where id = %d limit 1", packet->dwAmount, packet->dwAID);
	else
	{
		sys_err("Invalid request charge type (type : %d, amount : %d, aid : %d)", packet->eChargeType, packet->dwAmount, packet->dwAID);
		return;
	}

	sys_err("Request Charge (type : %d, amount : %d, aid : %d)", packet->eChargeType, packet->dwAmount, packet->dwAID);

	CDBManager::Instance().AsyncQuery(szQuery, SQL_ACCOUNT);
}

#if defined(__MOVE_CHANNEL__)
void CClientManager::FindChannel(CPeer* requestPeer, DWORD dwHandle, TPacketChangeChannel* p)
{
	if (!p->lMapIndex || !p->iChannel)
		return;

	long lAddr = 0;
	WORD wPort = 0;

	// for (TPeerList::const_iterator i = m_peerList.begin(); i != m_peerList.end(); ++i)
	for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
	{
		CPeer* peer = *i;
		if (peer->GetChannel() != p->iChannel) // not the channel we are looking for!
			continue;

		TMapLocation kMapLocation;
		thecore_memcpy(kMapLocation.alMaps, peer->GetMaps(), sizeof(kMapLocation.alMaps));
		int iLen = sizeof(kMapLocation.alMaps) / sizeof(kMapLocation.alMaps[0]);

		for (int i = 0; i < iLen; ++i)
		{
			if (kMapLocation.alMaps[i] == p->lMapIndex)
			{
				// Get host, and convert to int
				char host[16];
				strlcpy(host, peer->GetPublicIP(), sizeof(kMapLocation.szHost));
				lAddr = inet_addr(host);

				// Target port
				wPort = peer->GetListenPort();

				break;
			}
		}

		if (lAddr && wPort) // We already obtained them
			break;
	}

	TPacketReturnChannel r;
	r.lAddr = lAddr;
	r.wPort = wPort;

	requestPeer->EncodeHeader(HEADER_DG_CHANNEL_RESULT, dwHandle, sizeof(r));
	requestPeer->Encode(&r, sizeof(r));
}
#endif

#if defined(__ENVIRONMENT_SYSTEM__)
enum DayMode : BYTE { DAY, NIGHT };

static BYTE GetDayMode(int Hour)
{
	if (Hour >= 6 && Hour <= 20)
		return DayMode::DAY;
	return DayMode::NIGHT;
}

static bool IsWinter(int Month)
{
	return (Month >= 11 || Month <= 0);
}

void CClientManager::UpdateEnvironment()
{
	time_t CurrentTime = time(nullptr);
	tm* tm = localtime(&CurrentTime);

	auto SendFlag = [&](const char* flag, BYTE val)
		{
			TEventFlagMap::const_iterator it = m_map_lEventFlag.find(flag);
			if ((it != m_map_lEventFlag.end() && it->second == val) == false)
			{
				TPacketSetEventFlag p;
				std::strcpy(p.szFlagName, flag);
				p.lValue = val;
				SetEventFlag(&p);
			}
		};

	SendFlag("eclipse", GetDayMode(tm->tm_hour));
	BOOL Winter = IsWinter(tm->tm_mon);
	for (const auto& Flag : { "xmas_snow" })
		SendFlag(Flag, Winter);
}
#endif

#if defined(__EXPRESSING_EMOTIONS__)
void CClientManager::QUERY_EMOTE_LOAD(CPeer* pkPeer, DWORD dwHandle, TPacketGDEmote* pTable)
{
	if (pTable == nullptr)
		return;

	TEmoteTableVector* pVec = nullptr;
	TPlayerEmoteMap::iterator it = m_map_kPlayerEmote.find(pTable->dwPID);
	if (it != m_map_kPlayerEmote.end())
		pVec = &it->second;

	if (pVec && !pVec->empty())
	{
		const DWORD dwCurrentTime = static_cast<DWORD>(std::time(nullptr));
		pVec->erase(std::remove_if(pVec->begin(), pVec->end(),
			[dwCurrentTime](const TPacketGDEmote& rkTable)
			{
				return dwCurrentTime > rkTable.dwDuration;
			}
		), pVec->end());
	}

	const WORD wSize = pVec ? static_cast<WORD>(pVec->size()) : 0;
	const DWORD dwPacketSize = sizeof(WORD) + sizeof(WORD) + sizeof(TPacketGDEmote) * wSize;

	pkPeer->EncodeHeader(HEADER_DG_EMOTE_LOAD, dwHandle, dwPacketSize);
	pkPeer->EncodeWORD(sizeof(TPacketGDEmote));
	pkPeer->EncodeWORD(wSize);
	if (pVec && !pVec->empty())
		pkPeer->Encode(&(*pVec)[0], sizeof(TPacketGDEmote) * wSize);
}

void CClientManager::QUERY_EMOTE_CLEAR(CPeer* pkPeer, DWORD dwHandle, TPacketGDEmote* pTable)
{
	if (pTable == nullptr)
		return;

	TPlayerEmoteMap::iterator it = m_map_kPlayerEmote.find(pTable->dwPID);
	if (it != m_map_kPlayerEmote.end())
		it->second.clear();

	QUERY_EMOTE_LOAD(pkPeer, dwHandle, pTable);
}

void CClientManager::QUERY_EMOTE_ADD(CPeer* pkPeer, DWORD dwHandle, TPacketGDEmote* pTable)
{
	if (!pkPeer || !pTable)
		return;

	const DWORD dwCurrentTime = static_cast<DWORD>(std::time(nullptr));

	TPacketGDEmote GDPacket;
	GDPacket.dwPID = pTable->dwPID;
	GDPacket.dwVnum = pTable->dwVnum;
	GDPacket.dwDuration = dwCurrentTime + pTable->dwDuration;

	TPlayerEmoteMap::iterator it = m_map_kPlayerEmote.find(pTable->dwPID);
	if (it != m_map_kPlayerEmote.end())
	{
		TEmoteTableVector& rkVec = it->second;
		if (rkVec.size() >= SPECIAL_ACTION_SLOT_COUNT)
		{
			TPacketGDEmote& rkTable = rkVec[number(0, rkVec.size() - 1)];
			if (dwCurrentTime >= rkTable.dwDuration)
				rkTable.dwDuration = dwCurrentTime + pTable->dwDuration;
			else
				rkTable.dwDuration += pTable->dwDuration;

			GDPacket.dwVnum = rkTable.dwVnum;
			GDPacket.dwDuration = rkTable.dwDuration;
		}
		else
		{
			TEmoteTableVector::iterator itVec = std::find_if(rkVec.begin(), rkVec.end(),
				[pTable](const TPacketGDEmote& rkTable)
				{
					return rkTable.dwVnum == pTable->dwVnum;
				});

			if (itVec != rkVec.end())
			{
				if (dwCurrentTime >= itVec->dwDuration)
					itVec->dwDuration = dwCurrentTime + pTable->dwDuration;
				else
					itVec->dwDuration += pTable->dwDuration;

				GDPacket.dwDuration = itVec->dwDuration;
			}
			else
			{
				pTable->dwDuration = GDPacket.dwDuration;
				rkVec.emplace_back(*pTable);
			}
		}
	}
	else
	{
		pTable->dwDuration = GDPacket.dwDuration;
		m_map_kPlayerEmote[pTable->dwPID].emplace_back(*pTable);
	}

	pkPeer->EncodeHeader(HEADER_DG_EMOTE_GET, dwHandle, sizeof(GDPacket));
	pkPeer->Encode(&GDPacket, sizeof(GDPacket));
}

void CClientManager::QUERY_EMOTE_DUMP()
{
	CDBManager::instance().DirectQuery("TRUNCATE TABLE `player`.`emotions`");

	if (m_map_kPlayerEmote.empty())
		return;

	char szQuery[1024];
	const DWORD dwCurrentTime = static_cast<DWORD>(std::time(nullptr));

	for (TPlayerEmoteMap::value_type& it : m_map_kPlayerEmote)
	{
		TEmoteTableVector& rkVec = it.second;
		if (rkVec.empty())
			continue;

		rkVec.erase(std::remove_if(rkVec.begin(), rkVec.end(),
			[dwCurrentTime](const TPacketGDEmote& rkTable)
			{
				return dwCurrentTime > rkTable.dwDuration;
			}
		), rkVec.end());

		for (const TPacketGDEmote& rkTable : rkVec)
		{
			snprintf(szQuery, sizeof(szQuery),
				"REPLACE INTO emotions%s (`pid`, `vnum`, `duration`) VALUES(%d, %d, FROM_UNIXTIME(%d))",
				GetTablePostfix(), rkTable.dwPID, rkTable.dwVnum, rkTable.dwDuration
			);

			std::unique_ptr<SQLMsg> pkInsert(CDBManager::instance().DirectQuery(szQuery));
		}
	}
}
#endif

#if defined(__MAILBOX__)
void CClientManager::QUERY_MAILBOX_LOAD(CPeer* pkPeer, DWORD dwHandle, TMailBox* p)
{
	if (g_log)
		sys_log(0, "QUERY_MAILBOX (handle: %d ch: %s)", dwHandle, p->szName);

	std::vector<SMailBoxTable>* vec = nullptr;
	auto it = m_map_mailbox.find(p->szName);
	if (it != m_map_mailbox.end())
		vec = &it->second;

	if (vec)
	{
		const time_t now = std::time(nullptr);

		vec->erase(std::remove_if(vec->begin(), vec->end(),
			[now](const TMailBoxTable& mail)
			{ return mail.bIsDeleted || std::difftime(mail.Message.DeleteTime, now) <= 0; }
		), vec->end());

		std::sort(vec->begin(), vec->end(),
			[](const TMailBoxTable& l, const TMailBoxTable& r)
			{
				return l.Message.SendTime > r.Message.SendTime;
			});
	}

	const WORD size = vec ? static_cast<WORD>(vec->size()) : 0;
	const DWORD dwPacketSize = sizeof(WORD) + sizeof(WORD) + sizeof(SMailBoxTable) * size;

	pkPeer->EncodeHeader(HEADER_DG_RESPOND_MAILBOX_LOAD, dwHandle, dwPacketSize);
	pkPeer->EncodeWORD(sizeof(SMailBoxTable));
	pkPeer->EncodeWORD(size);

	if (vec && vec->empty() == false)
		pkPeer->Encode(&(*vec)[0], sizeof(SMailBoxTable) * size);
}

void CClientManager::QUERY_MAILBOX_CHECK_NAME(CPeer* pkPeer, DWORD dwHandle, TMailBox* p)
{
	TMailBox t;
	std::memcpy(t.szName, "", sizeof(t.szName));
	t.Index = 0; // Index: Mail Count

	static std::unordered_set<std::string> NameSet;
	bool bFound = NameSet.find(p->szName) != NameSet.end();

	if (bFound == false)
	{
		char s_szQuery[128];
		//snprintf(s_szQuery, sizeof(s_szQuery), "SELECT * FROM player%s WHERE `name` = '%s' LIMIT 1", GetTablePostfix(), p->szName);
		snprintf(s_szQuery, sizeof(s_szQuery), "SELECT * FROM player%s WHERE `name` = convert('%s' using utf8mb4) collate utf8mb4_bin LIMIT 1", GetTablePostfix(), p->szName);
		std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(s_szQuery));
		bFound = pMsg->Get()->uiNumRows > 0;
	}

	if (bFound)
	{
		NameSet.emplace(p->szName); // player exists, next time we will use this to avoid using mysql.
		std::memcpy(t.szName, p->szName, sizeof(t.szName));
		auto it = m_map_mailbox.find(p->szName);
		if (it != m_map_mailbox.end())
		{
			const time_t now = time(nullptr);
			for (const SMailBoxTable& mail : it->second)
			{
				if (mail.bIsDeleted)
					continue;

				if (std::difftime(mail.Message.DeleteTime, now) <= 0)
					continue;

				t.Index++;
			}
		}
	}

	pkPeer->EncodeHeader(HEADER_DG_RESPOND_MAILBOX_CHECK_NAME, dwHandle, sizeof(TMailBox));
	pkPeer->Encode(&t, sizeof(TMailBox));
}

void CClientManager::QUERY_MAILBOX_WRITE(CPeer* pkPeer, DWORD dwHandle, TMailBoxTable* p)
{
	m_map_mailbox[p->szName].emplace_back(*p);
}

bool CClientManager::GET_MAIL(const char* name, const BYTE index, SMailBoxTable** mail)
{
	auto it = m_map_mailbox.find(name);
	if (it == m_map_mailbox.end())
		return false;

	MailVec& mailvec = it->second;
	if (index >= mailvec.size())
		return false;

	*mail = &mailvec.at(index);
	return true;
}

void CClientManager::QUERY_MAILBOX_DELETE(CPeer* pkPeer, DWORD dwHandle, TMailBox* p)
{
	SMailBoxTable* mail = nullptr;
	if (GET_MAIL(p->szName, p->Index, &mail) == false)
		return;

	mail->bIsDeleted = true;
}

void CClientManager::QUERY_MAILBOX_CONFIRM(CPeer* pkPeer, DWORD dwHandle, TMailBox* p)
{
	SMailBoxTable* mail = nullptr;
	if (GET_MAIL(p->szName, p->Index, &mail) == false)
		return;

	mail->Message.bIsConfirm = true;
}

void CClientManager::QUERY_MAILBOX_GET(CPeer* pkPeer, DWORD dwHandle, TMailBox* p)
{
	SMailBoxTable* mail = nullptr;
	if (GET_MAIL(p->szName, p->Index, &mail) == false)
		return;

	mail->AddData.iYang = 0;
	mail->AddData.iWon = 0;
	mail->Message.bIsItemExist = false;
	mail->Message.bIsConfirm = true;
	mail->AddData.dwItemVnum = 0;
	mail->AddData.dwItemCount = 0;
	memset(mail->AddData.alSockets, 0, sizeof(mail->AddData.alSockets));
	memset(mail->AddData.aAttr, 0, sizeof(mail->AddData.aAttr));
#if defined(__CHANGE_LOOK_SYSTEM__)
	mail->AddData.dwChangeLookVnum = 0;
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
	memset(&mail->AddData.RefineElement, 0, sizeof(mail->AddData.RefineElement));
#endif
#if defined(__ITEM_APPLY_RANDOM__)
	memset(mail->AddData.aApplyRandom, 0, sizeof(mail->AddData.aApplyRandom));
#endif
#if defined(__SET_ITEM__)
	mail->AddData.bSetValue = 0;
#endif
}

void CClientManager::QUERY_MAILBOX_UNREAD(CPeer* pkPeer, DWORD dwHandle, TMailBox* p)
{
	auto it = m_map_mailbox.find(p->szName);
	if (it == m_map_mailbox.end())
		return;

	const MailVec& mailvec = it->second;
	if (mailvec.empty())
		return;

	const time_t now = time(nullptr);
	TMailBoxRespondUnreadData t;

	for (const SMailBoxTable& mail : it->second)
	{
		if (mail.bIsDeleted)
			continue;

		if (mail.Message.bIsConfirm)
			continue;

		if (std::difftime(mail.Message.DeleteTime, now) <= 0)
			continue;

		if (mail.Message.bIsGMPost)
			t.bGMVisible = true;

		if (mail.Message.bIsItemExist)
			t.bItemMessageCount++;
		else
			t.bCommonMessageCount++;
	}

	if ((t.bItemMessageCount + t.bCommonMessageCount) < 1)
		return;

	pkPeer->EncodeHeader(HEADER_DG_RESPOND_MAILBOX_UNREAD, dwHandle, sizeof(TMailBoxRespondUnreadData));
	pkPeer->Encode(&t, sizeof(TMailBoxRespondUnreadData));
}

void CClientManager::MAILBOX_BACKUP()
{
	CDBManager::instance().DirectQuery("TRUNCATE TABLE player.mailbox");

	if (m_map_mailbox.empty())
		return;

	char szQuery[QUERY_MAX_LEN];
	const time_t now = std::time(nullptr);

	for (auto& p : m_map_mailbox)
	{
		auto& mailvec = p.second;
		if (mailvec.empty())
			continue;

		mailvec.erase(std::remove_if(mailvec.begin(), mailvec.end(),
			[now](const TMailBoxTable& mail)
			{ return mail.bIsDeleted || std::difftime(mail.Message.DeleteTime, now) <= 0; }
		), mailvec.end());

		std::sort(mailvec.begin(), mailvec.end(),
			[](const TMailBoxTable& l, const TMailBoxTable& r)
			{
				return l.Message.SendTime > r.Message.SendTime;
			});

		for (const auto& mail : mailvec)
		{
			snprintf(szQuery, sizeof(szQuery),
				"INSERT INTO `mailbox%s` (`name`, `who`, `title`, `message`, `gm`, `confirm`, `send_time`, `delete_time`"
				", `gold`, `won`, `vnum`, `count`"
				", `socket0`, `socket1`, `socket2`"
#if defined(__ITEM_SOCKET6__)
				", `socket3`, `socket4`, `socket5`"
#endif
				", `attrtype0`, `attrvalue0`"
				", `attrtype1`, `attrvalue1`"
				", `attrtype2`, `attrvalue2`"
				", `attrtype3`, `attrvalue3`"
				", `attrtype4`, `attrvalue4`"
				", `attrtype5`, `attrvalue5`"
				", `attrtype6`, `attrvalue6`"
#if defined(__CHANGE_LOOK_SYSTEM__)
				", `changelookvnum`"
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
				", `refine_element_apply_type`"
				", `refine_element_grade`"
				", `refine_element_value0`, `refine_element_value1`, `refine_element_value2`"
				", `refine_element_bonus_value0`, `refine_element_bonus_value1`, `refine_element_bonus_value2`"
#endif
#if defined(__ITEM_APPLY_RANDOM__)
				", `apply_type0`, `apply_value0`, `apply_path0`"
				", `apply_type1`, `apply_value1`, `apply_path1`"
				", `apply_type2`, `apply_value2`, `apply_path2`"
				", `apply_type3`, `apply_value3`, `apply_path3`"
#endif
#if defined(__SET_ITEM__)
				", `set_value`"
#endif
				") VALUES ('%s', '%s', '%s', '%s', %d, %d, %d, %d" // `name`, `who`, `title`, `message`, `gm`, `confirm`, `send_time`, `delete_time`
				", %u, %u, %u, %u" // `gold`, `won`, `vnum`, `count`
				", %ld, %ld, %ld" // `socket0`, `socket1`, `socket2`
#if defined(__ITEM_SOCKET6__)
				", %ld, %ld, %ld" // `socket3`, `socket4`, `socket5`
#endif
				", %u, %ld" // `attrtype0`, `attrvalue0`
				", %u, %ld" // `attrtype1`, `attrvalue1`
				", %u, %ld" // `attrtype2`, `attrvalue2`
				", %u, %ld" // `attrtype3`, `attrvalue3`
				", %u, %ld" // `attrtype4`, `attrvalue4`
				", %u, %ld" // `attrtype5`, `attrvalue5`
				", %u, %ld" // `attrtype6`, `attrvalue6`
#if defined(__CHANGE_LOOK_SYSTEM__)
				", %u" // `changelookvnum`
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
				", %u" // `refine_element_apply_type`
				", %u" // `refine_element_grade`
				", %u, %u, %u" // `refine_element_value0`, `refine_element_value1`, `refine_element_value2`
				", %u, %u, %u" // `refine_element_bonus_value0`, `refine_element_bonus_value1`, `refine_element_bonus_value2`
#endif
#if defined(__ITEM_APPLY_RANDOM__)
				", %u, %ld, %d" // `apply_type0`, `apply_value0`, `apply_path0`
				", %u, %ld, %d" // `apply_type1`, `apply_value1`, `apply_path1`
				", %u, %ld, %d" // `apply_type2`, `apply_value2`, `apply_path2`
				", %u, %ld, %d" // `apply_type3`, `apply_value3`, `apply_path3`
#endif
#if defined(__SET_ITEM__)
				", %d" // `set_value`
#endif
				")", GetTablePostfix()
				, mail.szName
				, mail.AddData.szFrom
				, mail.Message.szTitle
				, mail.AddData.szMessage
				, mail.Message.bIsGMPost
				, mail.Message.bIsConfirm
				, mail.Message.SendTime
				, mail.Message.DeleteTime
				, mail.AddData.iYang
				, mail.AddData.iWon
				, mail.AddData.dwItemVnum
				, mail.AddData.dwItemCount
				, mail.AddData.alSockets[0], mail.AddData.alSockets[1], mail.AddData.alSockets[2]
#if defined(__ITEM_SOCKET6__)
				, mail.AddData.alSockets[3], mail.AddData.alSockets[4], mail.AddData.alSockets[5]
#endif
				, mail.AddData.aAttr[0].wType, mail.AddData.aAttr[0].lValue
				, mail.AddData.aAttr[1].wType, mail.AddData.aAttr[1].lValue
				, mail.AddData.aAttr[2].wType, mail.AddData.aAttr[2].lValue
				, mail.AddData.aAttr[3].wType, mail.AddData.aAttr[3].lValue
				, mail.AddData.aAttr[4].wType, mail.AddData.aAttr[4].lValue
				, mail.AddData.aAttr[5].wType, mail.AddData.aAttr[5].lValue
				, mail.AddData.aAttr[6].wType, mail.AddData.aAttr[6].lValue
#if defined(__CHANGE_LOOK_SYSTEM__)
				, mail.AddData.dwChangeLookVnum
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
				, mail.AddData.RefineElement.wApplyType
				, mail.AddData.RefineElement.bGrade
				, mail.AddData.RefineElement.abValue[0], mail.AddData.RefineElement.abValue[1], mail.AddData.RefineElement.abValue[2]
				, mail.AddData.RefineElement.abBonusValue[0], mail.AddData.RefineElement.abBonusValue[1], mail.AddData.RefineElement.abBonusValue[2]
#endif
#if defined(__ITEM_APPLY_RANDOM__)
				, mail.AddData.aApplyRandom[0].wType, mail.AddData.aApplyRandom[0].lValue, mail.AddData.aApplyRandom[0].bPath
				, mail.AddData.aApplyRandom[1].wType, mail.AddData.aApplyRandom[1].lValue, mail.AddData.aApplyRandom[1].bPath
				, mail.AddData.aApplyRandom[2].wType, mail.AddData.aApplyRandom[2].lValue, mail.AddData.aApplyRandom[2].bPath
				, mail.AddData.aApplyRandom[3].wType, mail.AddData.aApplyRandom[3].lValue, mail.AddData.aApplyRandom[3].bPath
#endif
#if defined(__SET_ITEM__)
				, mail.AddData.bSetValue
#endif
			);

			std::unique_ptr<SQLMsg> pInsert(CDBManager::instance().DirectQuery(szQuery));
		}
	}
}
#endif

#if defined(__GEM_SHOP__)
void CClientManager::LoadGemShop(CPeer* pPeer, DWORD dwHandle, TGemShopLoad* pTable, bool bUpdate)
{
	if (g_log)
		sys_log(0, "LoadGemShop (handle: %d pid: %d)", dwHandle, pTable->dwPID);

	TGemShopTable GemShopTable = {};
#	if defined(__CONQUEROR_LEVEL__)
	GemShopTable.bSpecial = pTable->bSpecial;
	if (GemShopTable.bSpecial)
	{
		GemShopTableMap::iterator it = m_mapGemShopSpecialTable.find(pTable->dwPID);
		if (it != m_mapGemShopSpecialTable.end())
			GemShopTable = it->second;
	}
	else
#	endif
	{
		GemShopTableMap::iterator it = m_mapGemShopTable.find(pTable->dwPID);
		if (it != m_mapGemShopTable.end())
			GemShopTable = it->second;
	}

	pPeer->EncodeHeader(bUpdate ? HEADER_DG_GEM_SHOP_UPDATE : HEADER_DG_GEM_SHOP_LOAD, dwHandle, sizeof(GemShopTable));
	pPeer->Encode(&GemShopTable, sizeof(GemShopTable));
}

void CClientManager::UpdateGemShop(CPeer* pPeer, DWORD dwHandle, TGemShopTable* pTable)
{
	if (g_log)
		sys_log(0, "UpdateGemShop (handle: %d pid: %d)", dwHandle, pTable->dwPID);

#	if defined(__CONQUEROR_LEVEL__)
	if (pTable->bSpecial)
	{
		GemShopTableMap::iterator it = m_mapGemShopSpecialTable.find(pTable->dwPID);
		if (it != m_mapGemShopSpecialTable.end())
			it->second = *pTable;
		else
			m_mapGemShopSpecialTable.emplace(std::make_pair(pTable->dwPID, *pTable));
	}
	else
#	endif
	{
		GemShopTableMap::iterator it = m_mapGemShopTable.find(pTable->dwPID);
		if (it != m_mapGemShopTable.end())
			it->second = *pTable;
		else
			m_mapGemShopTable.emplace(std::make_pair(pTable->dwPID, *pTable));
	}

	// NOTE : Request Gem Shop Load
	TGemShopLoad Packet;
	Packet.dwPID = pTable->dwPID;
#	if defined(__CONQUEROR_LEVEL__)
	Packet.bSpecial = pTable->bSpecial;
#	endif
	LoadGemShop(pPeer, dwHandle, &Packet, true);
}

void CClientManager::FlushGemShop()
{
	// Flush Default Gem Shop Table
	{
		if (m_mapGemShopTable.empty())
			return;

		char szQuery[1024] = {};
		snprintf(szQuery, sizeof(szQuery), "TRUNCATE TABLE `player`.`gem_shop%s`", GetTablePostfix());
		CDBManager::instance().DirectQuery(szQuery);

		GemShopTableMap::iterator it = m_mapGemShopTable.begin();
		for (; it != m_mapGemShopTable.end(); ++it)
		{
			TGemShopTable GemShopTable = it->second;
			if (it->first != GemShopTable.dwPID)
				continue;

			char szColumns[1024] = {};
			char szValues[1024] = {};

			int iLen = snprintf(szColumns, sizeof(szColumns), "`pid`, `refresh_time`, `enabled_slots`");
			int iValueLen = snprintf(szValues, sizeof(szValues), "%u, FROM_UNIXTIME(%ld), %d", GemShopTable.dwPID, GemShopTable.lRefreshTime, GemShopTable.bEnabledSlots);

			for (BYTE bSlotIndex = 0; bSlotIndex < GEM_SHOP_SLOT_COUNT; ++bSlotIndex)
			{
				iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen, ", `slot%d_enable`, `slot%d_vnum`, `slot%d_count`, `slot%d_price`",
					bSlotIndex, bSlotIndex, bSlotIndex, bSlotIndex);
				iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen, ", %d, %u, %d, %u",
					GemShopTable.GemShopItem[bSlotIndex].bEnable,
					GemShopTable.GemShopItem[bSlotIndex].dwItemVnum,
					GemShopTable.GemShopItem[bSlotIndex].bCount,
					GemShopTable.GemShopItem[bSlotIndex].dwPrice
				);
			}

			snprintf(szQuery, sizeof(szQuery), "INSERT INTO `gem_shop%s` (%s) VALUES(%s)",
				GetTablePostfix(), szColumns, szValues);

			std::unique_ptr<SQLMsg> pInsert(CDBManager::instance().DirectQuery(szQuery));
		}
	}

#	if defined(__CONQUEROR_LEVEL__)
	// Flush Port (Special) Gem Shop Table
	{
		if (m_mapGemShopSpecialTable.empty())
			return;

		char szQuery[1024] = {};
		snprintf(szQuery, sizeof(szQuery), "TRUNCATE TABLE `player`.`gem_shop_port%s`", GetTablePostfix());
		CDBManager::instance().DirectQuery(szQuery);

		GemShopTableMap::iterator it = m_mapGemShopSpecialTable.begin();
		for (; it != m_mapGemShopSpecialTable.end(); ++it)
		{
			TGemShopTable GemShopTable = it->second;
			if (it->first != GemShopTable.dwPID)
				continue;

			char szColumns[1024] = {};
			char szValues[1024] = {};

			int iLen = snprintf(szColumns, sizeof(szColumns), "`pid`, `refresh_time`, `enabled_slots`");
			int iValueLen = snprintf(szValues, sizeof(szValues), "%u, FROM_UNIXTIME(%ld), %d", GemShopTable.dwPID, GemShopTable.lRefreshTime, GemShopTable.bEnabledSlots);

			for (BYTE bSlotIndex = 0; bSlotIndex < GEM_SHOP_SLOT_COUNT; ++bSlotIndex)
			{
				iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen, ", `slot%d_enable`, `slot%d_vnum`, `slot%d_count`, `slot%d_price`",
					bSlotIndex, bSlotIndex, bSlotIndex, bSlotIndex);
				iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen, ", %d, %u, %d, %u",
					GemShopTable.GemShopItem[bSlotIndex].bEnable,
					GemShopTable.GemShopItem[bSlotIndex].dwItemVnum,
					GemShopTable.GemShopItem[bSlotIndex].bCount,
					GemShopTable.GemShopItem[bSlotIndex].dwPrice
				);
			}

			snprintf(szQuery, sizeof(szQuery), "INSERT INTO `gem_shop_port%s` (%s) VALUES(%s)",
				GetTablePostfix(), szColumns, szValues);

			std::unique_ptr<SQLMsg> pInsert(CDBManager::instance().DirectQuery(szQuery));
		}
	}
#	endif
}
#endif
