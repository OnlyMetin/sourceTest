#include "stdafx.h"
#include "constants.h"
#include "config.h"
#include "log.h"

#include "char.h"
#include "desc.h"
#include "item.h"
#include "locale_service.h"

static char __escape_hint[1024];

LogManager::LogManager() : m_bIsConnect(false)
{
}

LogManager::~LogManager()
{
}

bool LogManager::Connect(const char* host, const int port, const char* user, const char* pwd, const char* db)
{
	if (m_sql.Setup(host, user, pwd, db, g_stLocale.c_str(), false, port))
		m_bIsConnect = true;

	return m_bIsConnect;
}

void LogManager::Query(const char* c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	if (test_server)
		sys_log(0, "LOG: %s", szQuery);

	m_sql.AsyncQuery(szQuery);
}

bool LogManager::IsConnected()
{
	return m_bIsConnect;
}

void LogManager::ItemLog(DWORD dwPID, DWORD x, DWORD y, DWORD dwItemID, const char* c_pszText, const char* c_pszHint, const char* c_pszIP, DWORD dwVnum)
{
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), c_pszHint, strlen(c_pszHint));

	Query("INSERT DELAYED INTO log%s (type, time, who, x, y, what, how, hint, ip, vnum) VALUES('ITEM', NOW(), %u, %u, %u, %u, '%s', '%s', '%s', %u)",
		get_table_postfix(), dwPID, x, y, dwItemID, c_pszText, __escape_hint, c_pszIP, dwVnum);
}

void LogManager::ItemLog(LPCHARACTER ch, LPITEM item, const char* c_pszText, const char* c_pszHint)
{
	if (NULL == ch || NULL == item)
	{
		sys_err("character or item nil (ch %p item %p text %s)", get_pointer(ch), get_pointer(item), c_pszText);
		return;
	}

	ItemLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(), item->GetID(),
		NULL == c_pszText ? "" : c_pszText,
		c_pszHint, ch->GetDesc() ? ch->GetDesc()->GetHostName() : "",
		item->GetOriginalVnum());
}

void LogManager::ItemLog(LPCHARACTER ch, int itemID, int itemVnum, const char* c_pszText, const char* c_pszHint)
{
	ItemLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(), itemID, c_pszText, c_pszHint, ch->GetDesc() ? ch->GetDesc()->GetHostName() : "", itemVnum);
}

void LogManager::CharLog(DWORD dwPID, DWORD x, DWORD y, DWORD dwValue, const char* c_pszText, const char* c_pszHint, const char* c_pszIP)
{
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), c_pszHint, strlen(c_pszHint));

	Query("INSERT DELAYED INTO log%s (type, time, who, x, y, what, how, hint, ip) VALUES('CHARACTER', NOW(), %u, %u, %u, %u, '%s', '%s', '%s')",
		get_table_postfix(), dwPID, x, y, dwValue, c_pszText, __escape_hint, c_pszIP);
}

void LogManager::CharLog(LPCHARACTER ch, DWORD dw, const char* c_pszText, const char* c_pszHint)
{
	if (ch)
		CharLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(), dw, c_pszText, c_pszHint, ch->GetDesc() ? ch->GetDesc()->GetHostName() : "");
	else
		CharLog(0, 0, 0, dw, c_pszText, c_pszHint, "");
}

void LogManager::LoginLog(bool isLogin, DWORD dwAccountID, DWORD dwPID, BYTE bLevel, BYTE bJob, DWORD dwPlayTime)
{
	Query("INSERT DELAYED INTO loginlog%s (type, time, channel, account_id, pid, level, job, playtime) VALUES (%s, NOW(), %d, %u, %u, %d, %d, %u)",
		get_table_postfix(), isLogin ? "'LOGIN'" : "'LOGOUT'", g_bChannel, dwAccountID, dwPID, bLevel, bJob, dwPlayTime);
}

void LogManager::MoneyLog(BYTE type, DWORD vnum, int gold
#if defined(__CHEQUE_SYSTEM__)
	, int cheque
#endif
)
{
	if (type == MONEY_LOG_RESERVED || type >= MONEY_LOG_TYPE_MAX_NUM)
	{
#if defined(__CHEQUE_SYSTEM__)
		sys_err("TYPE ERROR: type %d vnum %u gold %d cheque %d", type, vnum, gold, cheque);
#else
		sys_err("TYPE ERROR: type %d vnum %u gold %d", type, vnum, gold);
#endif
		return;
	}

	Query("INSERT DELAYED INTO money_log%s VALUES (NOW(), %d, %d, %d"
#if defined(__CHEQUE_SYSTEM__)
		", %d"
#endif
		")", get_table_postfix(), type, vnum, gold
#if defined(__CHEQUE_SYSTEM__)
		, cheque
#endif
	);
}

void LogManager::HackLog(const char* c_pszHackName, const char* c_pszLogin, const char* c_pszName, const char* c_pszIP)
{
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), c_pszHackName, strlen(c_pszHackName));

	Query("INSERT INTO hack_log (time, login, name, ip, server, why) VALUES(NOW(), '%s', '%s', '%s', '%s', '%s')", c_pszLogin, c_pszName, c_pszIP, g_stHostname.c_str(), __escape_hint);
}

void LogManager::HackLog(const char* c_pszHackName, LPCHARACTER ch)
{
	if (ch->GetDesc())
	{
		HackLog(c_pszHackName,
			ch->GetDesc()->GetAccountTable().login,
			ch->GetName(),
			ch->GetDesc()->GetHostName());
	}
}

void LogManager::HackCRCLog(const char* c_pszHackName, const char* c_pszLogin, const char* c_pszName, const char* c_pszIP, DWORD dwCRC)
{
	Query("INSERT INTO hack_crc_log (time, login, name, ip, server, why, crc) VALUES(NOW(), '%s', '%s', '%s', '%s', '%s', %u)", c_pszLogin, c_pszName, c_pszIP, g_stHostname.c_str(), c_pszHackName, dwCRC);
}

void LogManager::PCBangLoginLog(DWORD dwPCBangID, const char* c_szPCBangIP, DWORD dwPlayerID, DWORD dwPlayTime)
{
	Query("INSERT INTO pcbang_loginlog (time, pcbang_id, ip, pid, play_time) VALUES (NOW(), %u, '%s', %u, %u)",
		dwPCBangID, c_szPCBangIP, dwPlayerID, dwPlayTime);
}

void LogManager::GoldBarLog(DWORD dwPID, DWORD dwItemID, GOLDBAR_HOW eHow, const char* c_pszHint)
{
	char szHow[32 + 1];

	switch (eHow)
	{
	case PERSONAL_SHOP_BUY:
		snprintf(szHow, sizeof(szHow), "'BUY'");
		break;

	case PERSONAL_SHOP_SELL:
		snprintf(szHow, sizeof(szHow), "'SELL'");
		break;

	case SHOP_BUY:
		snprintf(szHow, sizeof(szHow), "'SHOP_BUY'");
		break;

	case SHOP_SELL:
		snprintf(szHow, sizeof(szHow), "'SHOP_SELL'");
		break;

	case EXCHANGE_TAKE:
		snprintf(szHow, sizeof(szHow), "'EXCHANGE_TAKE'");
		break;

	case EXCHANGE_GIVE:
		snprintf(szHow, sizeof(szHow), "'EXCHANGE_GIVE'");
		break;

	case QUEST:
		snprintf(szHow, sizeof(szHow), "'QUEST'");
		break;

	default:
		snprintf(szHow, sizeof(szHow), "''");
		break;
	}

	Query("INSERT DELAYED INTO goldlog%s (date, time, pid, what, how, hint) VALUES(CURDATE(), CURTIME(), %u, %u, %s, '%s')",
		get_table_postfix(), dwPID, dwItemID, szHow, c_pszHint);
}

void LogManager::CubeLog(DWORD dwPID, DWORD x, DWORD y, DWORD item_vnum, DWORD item_uid, int item_count, bool success)
{
	Query("INSERT DELAYED INTO cube%s (pid, time, x, y, item_vnum, item_uid, item_count, success) "
		"VALUES(%u, NOW(), %u, %u, %u, %u, %d, %d)",
		get_table_postfix(), dwPID, x, y, item_vnum, item_uid, item_count, success ? 1 : 0);
}

void LogManager::WhisperLog(DWORD fromPID, DWORD toPID, const char* message)
{
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), message, strlen(message)); // Message mysql_real_escape_string Output = __escape_hint;
	Query("INSERT INTO whisper_log%s (`time`, `from`, `to`, `message`) VALUES(NOW(), %u, %u, '%s');", get_table_postfix(), fromPID, toPID, __escape_hint);
}

void LogManager::SpeedHackLog(DWORD pid, DWORD x, DWORD y, int hack_count)
{
	Query("INSERT INTO speed_hack%s (pid, time, x, y, hack_count) "
		"VALUES(%u, NOW(), %u, %u, %d)",
		get_table_postfix(), pid, x, y, hack_count);
}

void LogManager::ChangeNameLog(DWORD pid, const char* old_name, const char* new_name, const char* ip)
{
	Query("INSERT DELAYED INTO change_name%s (pid, old_name, new_name, time, ip) "
		"VALUES(%u, '%s', '%s', NOW(), '%s') ",
		get_table_postfix(), pid, old_name, new_name, ip);
}

void LogManager::GMCommandLog(DWORD dwPID, const char* szName, const char* szIP, BYTE byChannel, const char* szCommand)
{
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), szCommand, strlen(szCommand));

	Query("INSERT DELAYED INTO command_log%s (userid, server, ip, port, username, command, date ) "
		"VALUES(%u, 999, '%s', %u, '%s', '%s', NOW()) ",
		get_table_postfix(), dwPID, szIP, byChannel, szName, __escape_hint);
}

void LogManager::RefineLog(DWORD pid, const char* item_name, DWORD item_id, int item_refine_level, int is_success, const char* how)
{
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), item_name, strlen(item_name));

	Query("INSERT INTO refinelog%s (pid, item_name, item_id, step, time, is_success, setType) VALUES(%u, '%s', %u, %d, NOW(), %d, '%s')",
		get_table_postfix(), pid, __escape_hint, item_id, item_refine_level, is_success, how);
}

void LogManager::ShoutLog(BYTE bChannel, BYTE bEmpire, const char* pszText)
{
	m_sql.EscapeString(__escape_hint, sizeof(__escape_hint), pszText, strlen(pszText));

	Query("INSERT INTO shout_log%s VALUES(NOW(), %d, %d,'%s')", get_table_postfix(), bChannel, bEmpire, __escape_hint);
}

void LogManager::LevelLog(LPCHARACTER pChar, unsigned int level, unsigned int playhour)
{
	if (true == LC_IsEurope())
	{
		DWORD aid = 0;

		if (NULL != pChar->GetDesc())
		{
			aid = pChar->GetDesc()->GetAccountTable().id;
		}

		Query("REPLACE INTO levellog%s (name, level, time, account_id, pid, playtime) VALUES('%s', %u, NOW(), %u, %u, %d)",
			get_table_postfix(), pChar->GetName(), level, aid, pChar->GetPlayerID(), playhour);
	}
	else
	{
		Query("REPLACE INTO levellog%s (name, level, time, playtime) VALUES('%s', %u, NOW(), %d)",
			get_table_postfix(), pChar->GetName(), level, playhour);
	}
}

void LogManager::BootLog(const char* c_pszHostName, BYTE bChannel)
{
	Query("INSERT INTO bootlog (time, hostname, channel) VALUES(NOW(), '%s', %d)",
		c_pszHostName, bChannel);
}

void LogManager::FishLog(DWORD dwPID, int prob_idx, int fish_id, int fish_level, DWORD dwMiliseconds, DWORD dwVnum, DWORD dwValue)
{
	Query("INSERT INTO fish_log%s VALUES(NOW(), %u, %d, %u, %d, %u, %u, %u)",
		get_table_postfix(),
		dwPID,
		prob_idx,
		fish_id,
		fish_level,
		dwMiliseconds,
		dwVnum,
		dwValue);
}

void LogManager::QuestRewardLog(const char* c_pszQuestName, DWORD dwPID, DWORD dwLevel, int iValue1, int iValue2)
{
	Query("INSERT INTO quest_reward_log%s VALUES('%s',%u,%u,2,%u,%u,NOW())",
		get_table_postfix(),
		c_pszQuestName,
		dwPID,
		dwLevel,
		iValue1,
		iValue2);
}

void LogManager::DetailLoginLog(bool isLogin, LPCHARACTER ch)
{
	if (NULL == ch->GetDesc())
		return;

	if (true == isLogin)
	{
		Query("INSERT INTO loginlog2(type, is_gm, login_time, channel, account_id, pid, ip, client_version) "
			"VALUES('INVALID', %s, NOW(), %d, %u, %u, inet_aton('%s'), '%s')",
			ch->IsGM() == TRUE ? "'Y'" : "'N'",
			g_bChannel,
			ch->GetDesc()->GetAccountTable().id,
			ch->GetPlayerID(),
			ch->GetDesc()->GetHostName(),
			ch->GetDesc()->GetClientVersion());
	}
	else
	{
		Query("SET @i = (SELECT MAX(id) FROM loginlog2 WHERE account_id=%u AND pid=%u)",
			ch->GetDesc()->GetAccountTable().id,
			ch->GetPlayerID());

		Query("UPDATE loginlog2 SET type='VALID', logout_time=NOW(), playtime=TIMEDIFF(logout_time,login_time) WHERE id=@i");
	}
}

void LogManager::DragonSlayLog(DWORD dwGuildID, DWORD dwDragonVnum, DWORD dwStartTime, DWORD dwEndTime)
{
	Query("INSERT INTO dragon_slay_log%s VALUES( %d, %d, FROM_UNIXTIME(%d), FROM_UNIXTIME(%d) )",
		get_table_postfix(),
		dwGuildID, dwDragonVnum, dwStartTime, dwEndTime);
}

#if defined(__MAILBOX__)
void LogManager::MailLog(const char* const szName, const char* const szWho, const char* const szTitle, const char* const szMessage, const bool bIsGM, const DWORD dwItemVnum, const DWORD dwItemCount, const int iYang, const int iWon)
{
	char szEscapeTitle[1024];
	m_sql.EscapeString(szEscapeTitle, sizeof(szEscapeTitle), szTitle, strlen(szTitle));

	char szEscapeMessage[1024];
	m_sql.EscapeString(szEscapeMessage, sizeof(szEscapeMessage), szMessage, strlen(szMessage));

	Query("INSERT DELAYED INTO mailbox_log%s (name, who, title, message, gm, gold, won, ivnum, icount, date) "
		"VALUES('%s', '%s', '%s', '%s', %d, %d, %d, %lu, %lu, NOW()) ",
		get_table_postfix(), szName, szWho, szEscapeTitle, szEscapeMessage, bIsGM, iYang, iWon, dwItemVnum, dwItemCount);
}
#endif
