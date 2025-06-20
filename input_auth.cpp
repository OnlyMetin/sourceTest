#include "stdafx.h" 
#include "constants.h"
#include "config.h"
#include "input.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "protocol.h"
#include "locale_service.h"
#include "auth_brazil.h"
#include "db.h"

#ifndef __WIN32__
#include "limit_time.h"
#endif

extern time_t get_global_time();

bool FN_IS_VALID_LOGIN_STRING(const char* str)
{
	const char* tmp;

	if (!str || !*str)
		return false;

	if (strlen(str) < 2)
		return false;

	for (tmp = str; *tmp; ++tmp)
	{
		// 알파벳과 수자만 허용
		if (isdigit(*tmp) || isalpha(*tmp))
			continue;

		// 캐나다는 몇몇 특수문자 허용
		if (LC_IsCanada())
		{
			switch (*tmp)
			{
				case ' ':
				case '_':
				case '-':
				case '.':
				case '!':
				case '@':
				case '#':
				case '$':
				case '%':
				case '^':
				case '&':
				case '*':
				case '(':
				case ')':
					continue;
			}
		}

		if (LC_IsYMIR() == true || LC_IsKorea() == true)
		{
			switch (*tmp)
			{
				case '-':
				case '_':
					continue;
			}
		}

		if (LC_IsBrazil() == true)
		{
			switch (*tmp)
			{
				case '_':
				case '-':
				case '=':
					continue;
			}
		}

		if (LC_IsJapan() == true)
		{
			switch (*tmp)
			{
				case '-':
				case '_':
				case '@':
				case '#':
					continue;
			}
		}

		return false;
	}

	return true;
}

bool Login_IsInChannelService(const char* c_login)
{
	if (c_login[0] == '[')
		return true;
	return false;
}

CInputAuth::CInputAuth()
{
}

void CInputAuth::Login(LPDESC d, const char* c_pData)
{
#if defined(ENABLE_LIMIT_TIME)
	extern bool Metin2Server_IsInvalid();

	if (Metin2Server_IsInvalid())
	{
		extern void ClearAdminPages();
		ClearAdminPages();
		exit(1);
		return;
	}
#endif

	TPacketCGLogin3* pinfo = (TPacketCGLogin3*)c_pData;

	if (!g_bAuthServer)
	{
		sys_err("CInputAuth class is not for game server. IP %s might be a hacker.",
			inet_ntoa(d->GetAddr().sin_addr));
		d->DelayedDisconnect(5);
		return;
	}

	// string 무결성을 위해 복사
	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(pinfo->login, login, sizeof(login));

	char passwd[PASSWD_MAX_LEN + 1];
	strlcpy(passwd, pinfo->passwd, sizeof(passwd));

	sys_log(0, "InputAuth::Login : %s(%d) desc %p",
		login, strlen(login), get_pointer(d));

	// check login string
	if (false == FN_IS_VALID_LOGIN_STRING(login))
	{
		sys_log(0, "InputAuth::Login : IS_NOT_VALID_LOGIN_STRING(%s) desc %p",
			login, get_pointer(d));
		LoginFailure(d, "NOID");
		return;
	}

	if (g_bNoMoreClient)
	{
		TPacketGCLoginFailure failurePacket;

		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "SHUTDOWN", sizeof(failurePacket.szStatus));

		d->Packet(&failurePacket, sizeof(failurePacket));
		return;
	}

	if (DESC_MANAGER::instance().FindByLoginName(login))
	{
		LoginFailure(d, "ALREADY");
		return;
	}

	DWORD dwKey = DESC_MANAGER::instance().CreateLoginKey(d);
	DWORD dwPanamaKey = dwKey ^ pinfo->adwClientKey[0] ^ pinfo->adwClientKey[1] ^ pinfo->adwClientKey[2] ^ pinfo->adwClientKey[3];
	d->SetPanamaKey(dwPanamaKey);

	sys_log(0, "InputAuth::Login : key %u:0x%x login %s", dwKey, dwPanamaKey, login);

	// BRAZIL_AUTH
	if (LC_IsBrazil() && !test_server)
	{
		int result = auth_brazil(login, passwd);

		switch (result)
		{
			case AUTH_BRAZIL_SERVER_ERR:
			case AUTH_BRAZIL_NOID:
				LoginFailure(d, "NOID");
				return;
			case AUTH_BRAZIL_WRONGPWD:
				LoginFailure(d, "WRONGPWD");
				return;
			case AUTH_BRAZIL_FLASHUSER:
				LoginFailure(d, "FLASH");
				return;
		}
	}

	TPacketCGLogin3* p = M2_NEW TPacketCGLogin3;
	thecore_memcpy(p, pinfo, sizeof(TPacketCGLogin3));

	char szPasswd[PASSWD_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szPasswd, sizeof(szPasswd), passwd, strlen(passwd));

	char szLogin[LOGIN_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szLogin, sizeof(szLogin), login, strlen(login));

	// CHANNEL_SERVICE_LOGIN
	if (Login_IsInChannelService(szLogin))
	{
		sys_log(0, "ChannelServiceLogin [%s]", szLogin);

		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN, dwKey, p,
			"SELECT '%s', `password`, `social_id`, `id`, `status`"
			", `availDt` - NOW() > 0"
			", UNIX_TIMESTAMP(`silver_expire`)"
			", UNIX_TIMESTAMP(`gold_expire`)"
			", UNIX_TIMESTAMP(`safebox_expire`)"
			", UNIX_TIMESTAMP(`autoloot_expire`)"
			", UNIX_TIMESTAMP(`fish_mind_expire`)"
			", UNIX_TIMESTAMP(`marriage_fast_expire`)"
			", UNIX_TIMESTAMP(`money_drop_rate_expire`)"
#if defined(__CONQUEROR_LEVEL__)
			", UNIX_TIMESTAMP(`sungma_expire`)"
#endif
			", UNIX_TIMESTAMP(`create_time`)"
			" FROM `account` WHERE `login` = '%s'",
			szPasswd, szLogin);
	}
	// END_OF_CHANNEL_SERVICE_LOGIN
	else
	{
		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN, dwKey, p,
			"SELECT CONCAT('*', UPPER(SHA1(UNHEX(SHA1('%s'))))), `password`, `social_id`, `id`, `status`"
			", `availDt` - NOW() > 0"
			", UNIX_TIMESTAMP(`silver_expire`)"
			", UNIX_TIMESTAMP(`gold_expire`)"
			", UNIX_TIMESTAMP(`safebox_expire`)"
			", UNIX_TIMESTAMP(`autoloot_expire`)"
			", UNIX_TIMESTAMP(`fish_mind_expire`)"
			", UNIX_TIMESTAMP(`marriage_fast_expire`)"
			", UNIX_TIMESTAMP(`money_drop_rate_expire`)"
#if defined(__CONQUEROR_LEVEL__)
			", UNIX_TIMESTAMP(`sungma_expire`)"
#endif
			", UNIX_TIMESTAMP(`create_time`)"
			" FROM `account` WHERE `login` = '%s'",
			szPasswd, szLogin);
	}
}

extern void socket_timeout(socket_t s, long sec, long usec);

int CInputAuth::Analyze(LPDESC d, BYTE bHeader, const char* c_pData)
{
	if (!g_bAuthServer)
	{
		sys_err("CInputAuth class is not for game server. IP %s might be a hacker.",
			inet_ntoa(d->GetAddr().sin_addr));
		d->DelayedDisconnect(5);
		return 0;
	}

	int iExtraLen = 0;

	if (test_server)
		sys_log(0, " InputAuth Analyze Header[%d] ", bHeader);

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_LOGIN3:
			Login(d, c_pData);
			break;

		case HEADER_CG_HANDSHAKE:
			break;

		default:
			sys_err("This phase does not handle this header %d (0x%x)(phase: AUTH)", bHeader, bHeader);
			break;
	}

	return iExtraLen;
}
