/**
* date : 2007.05.31
* file : BlockCountry.h
* author : mhh
* description :
**/

#ifndef __INC_BLOCK_COUNTRY_H__
#define __INC_BLOCK_COUNTRY_H__

#include "Peer.h"

#define MAX_COUNTRY_NAME_LENGTH 50

class CBlockCountry : public singleton<CBlockCountry>
{
private:
	struct BLOCK_IP
	{
		DWORD ip_from;
		DWORD ip_to;
		char country[MAX_COUNTRY_NAME_LENGTH + 1];
	};

	typedef std::vector<BLOCK_IP*> BLOCK_IP_VECTOR;
	BLOCK_IP_VECTOR m_block_ip;

	typedef std::vector<const char*> BLOCK_EXCEPTION_VECTOR;
	BLOCK_EXCEPTION_VECTOR m_block_exception;

public:
	CBlockCountry();
	~CBlockCountry();

public:
	bool Load();
	bool IsBlockedCountryIp(const char* user_ip);
	void SendBlockedCountryIp(CPeer* peer);
	void SendBlockException(CPeer* peer);
	void SendBlockExceptionOne(CPeer* peer, const char* login, BYTE cmd);
	void AddBlockException(const char* login);
	void DelBlockException(const char* login);
};
#endif // __INC_BLOCK_COUNTRY_H__
