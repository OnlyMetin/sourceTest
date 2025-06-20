#ifndef __INC_COMMON_TABLES_H__
#define __INC_COMMON_TABLES_H__

#include "service.h"
#include "length.h"

typedef DWORD IDENT;

/**
* @version 05/06/10 Bang2ni - Myshop Pricelist 관련 패킷 HEADER_XX_MYSHOP_PRICELIST_XXX 추가
**/
enum GD_HEADERS
{
	HEADER_GD_LOGIN = 1,
	HEADER_GD_LOGOUT = 2,

	HEADER_GD_PLAYER_LOAD = 3,
	HEADER_GD_PLAYER_SAVE = 4,
	HEADER_GD_PLAYER_CREATE = 5,
	HEADER_GD_PLAYER_DELETE = 6,

	HEADER_GD_LOGIN_KEY = 7,
	//HEADER_GD_EMPTY = 8,
	HEADER_GD_BOOT = 9,
	HEADER_GD_PLAYER_COUNT = 10,
	HEADER_GD_QUEST_SAVE = 11,
	HEADER_GD_SAFEBOX_LOAD = 12,
	HEADER_GD_SAFEBOX_SAVE = 13,
	HEADER_GD_SAFEBOX_CHANGE_SIZE = 14,
	HEADER_GD_EMPIRE_SELECT = 15,

	HEADER_GD_SAFEBOX_CHANGE_PASSWORD = 16,
	HEADER_GD_SAFEBOX_CHANGE_PASSWORD_SECOND = 17, // Not really a packet, used internal
	HEADER_GD_DIRECT_ENTER = 18,

	HEADER_GD_GUILD_SKILL_UPDATE = 19,
	HEADER_GD_GUILD_EXP_UPDATE = 20,
	HEADER_GD_GUILD_ADD_MEMBER = 21,
	HEADER_GD_GUILD_REMOVE_MEMBER = 22,
	HEADER_GD_GUILD_CHANGE_GRADE = 23,
	HEADER_GD_GUILD_CHANGE_MEMBER_DATA = 24,
	HEADER_GD_GUILD_DISBAND = 25,
	HEADER_GD_GUILD_WAR = 26,
	HEADER_GD_GUILD_WAR_SCORE = 27,
	HEADER_GD_GUILD_CREATE = 28,

	//HEADER_GD_EMPTY = 29,

	HEADER_GD_ITEM_SAVE = 30,
	HEADER_GD_ITEM_DESTROY = 31,

	HEADER_GD_ADD_AFFECT = 32,
	HEADER_GD_REMOVE_AFFECT = 33,

	HEADER_GD_HIGHSCORE_REGISTER = 34,
	HEADER_GD_ITEM_FLUSH = 35,

	HEADER_GD_PARTY_CREATE = 36,
	HEADER_GD_PARTY_DELETE = 37,
	HEADER_GD_PARTY_ADD = 38,
	HEADER_GD_PARTY_REMOVE = 39,
	HEADER_GD_PARTY_STATE_CHANGE = 40,
	HEADER_GD_PARTY_HEAL_USE = 41,

	HEADER_GD_FLUSH_CACHE = 42,
	HEADER_GD_RELOAD_PROTO = 43,

	HEADER_GD_CHANGE_NAME = 44,
	//HEADER_GD_EMPTY = 45,

	HEADER_GD_GUILD_CHANGE_LADDER_POINT = 46,
	HEADER_GD_GUILD_USE_SKILL = 47,

	HEADER_GD_REQUEST_EMPIRE_PRIV = 48,
	HEADER_GD_REQUEST_GUILD_PRIV = 49,

	HEADER_GD_MONEY_LOG = 50,

	HEADER_GD_GUILD_DEPOSIT_MONEY = 51,
	HEADER_GD_GUILD_WITHDRAW_MONEY = 52,
	HEADER_GD_GUILD_WITHDRAW_MONEY_GIVE_REPLY = 53,

	HEADER_GD_REQUEST_CHARACTER_PRIV = 54,

	HEADER_GD_SET_EVENT_FLAG = 55,

	HEADER_GD_PARTY_SET_MEMBER_LEVEL = 56,

	HEADER_GD_GUILD_WAR_BET = 57,
#if defined(__GUILD_EVENT_FLAG__) //&& defined(__GUILD_RENEWAL__)
	HEADER_GD_GUILD_EVENT_FLAG = 58,
#endif
	//HEADER_GD_EMPTY = 59,

	HEADER_GD_CREATE_OBJECT = 60,
	HEADER_GD_DELETE_OBJECT = 61,
	HEADER_GD_UPDATE_LAND = 62,

	//HEADER_GD_EMPTY = 63,
	//HEADER_GD_EMPTY = 64,
	//HEADER_GD_EMPTY = 65,
	//HEADER_GD_EMPTY = 66,
	//HEADER_GD_EMPTY = 67,
	//HEADER_GD_EMPTY = 68,
	//HEADER_GD_EMPTY = 69,

	HEADER_GD_MARRIAGE_ADD = 70,
	HEADER_GD_MARRIAGE_UPDATE = 71,
	HEADER_GD_MARRIAGE_REMOVE = 72,

	HEADER_GD_WEDDING_REQUEST = 73,
	HEADER_GD_WEDDING_READY = 74,
	HEADER_GD_WEDDING_END = 75,

	//HEADER_GD_EMPTY = 76,
	//HEADER_GD_EMPTY = 77,
	//HEADER_GD_EMPTY = 78,
	//HEADER_GD_EMPTY = 79,
	//HEADER_GD_EMPTY = 80,
	//HEADER_GD_EMPTY = 81,
	//HEADER_GD_EMPTY = 82,
	//HEADER_GD_EMPTY = 83,
	//HEADER_GD_EMPTY = 84,
	//HEADER_GD_EMPTY = 85,
	//HEADER_GD_EMPTY = 86,
	//HEADER_GD_EMPTY = 87,
	//HEADER_GD_EMPTY = 88,
	//HEADER_GD_EMPTY = 89,
	//HEADER_GD_EMPTY = 90,
	//HEADER_GD_EMPTY = 91,
	//HEADER_GD_EMPTY = 92,
	//HEADER_GD_EMPTY = 93,
	//HEADER_GD_EMPTY = 94,
	//HEADER_GD_EMPTY = 95,
	//HEADER_GD_EMPTY = 96,
	//HEADER_GD_EMPTY = 97,
	//HEADER_GD_EMPTY = 98,
	//HEADER_GD_EMPTY = 99,

	HEADER_GD_AUTH_LOGIN = 100,
	HEADER_GD_LOGIN_BY_KEY = 101,
	//HEADER_GD_EMPTY = 102,
	//HEADER_GD_EMPTY = 103,
	//HEADER_GD_EMPTY = 104,
	//HEADER_GD_EMPTY = 105,
	//HEADER_GD_EMPTY = 106,
	HEADER_GD_MALL_LOAD = 107,

	HEADER_GD_MYSHOP_PRICELIST_UPDATE = 108, /// < 가격정보 갱신 요청
	HEADER_GD_MYSHOP_PRICELIST_REQ = 109, /// < 가격정보 리스트 요청

	HEADER_GD_BLOCK_CHAT = 110,

	// PCBANG_IP_LIST_BY_AUTH
	HEADER_GD_PCBANG_REQUEST_IP_LIST = 111,
	HEADER_GD_PCBANG_CLEAR_IP_LIST = 112,
	HEADER_GD_PCBANG_INSERT_IP = 113,
	// END_OF_PCBANG_IP_LIST_BY_AUTH

	HEADER_GD_HAMMER_OF_TOR = 114,
	HEADER_GD_RELOAD_ADMIN = 115, /// < 운영자 정보 요청
	HEADER_GD_BREAK_MARRIAGE = 116, /// < 결혼 파기
	HEADER_GD_ELECT_MONARCH = 117, /// < 군주 투표
	HEADER_GD_CANDIDACY = 118, /// < 군주 등록
	HEADER_GD_ADD_MONARCH_MONEY = 119, /// < 군주 돈 증가
	HEADER_GD_TAKE_MONARCH_MONEY = 120, /// < 군주 돈 감소
	HEADER_GD_COME_TO_VOTE = 121, /// < 표결
	HEADER_GD_RMCANDIDACY = 122, /// < 후보 제거 (운영자)
	HEADER_GD_SETMONARCH = 123, /// <군주설정 (운영자)
	HEADER_GD_RMMONARCH = 124, /// <군주삭제
	HEADER_GD_DEC_MONARCH_MONEY = 125,

	HEADER_GD_CHANGE_MONARCH_LORD = 126,
	HEADER_GD_BLOCK_COUNTRY_IP = 127, // 광대역 IP-Block
	HEADER_GD_BLOCK_EXCEPTION = 128, // 광대역 IP-Block 예외

	HEADER_GD_REQ_CHANGE_GUILD_MASTER = 129,

	HEADER_GD_REQ_SPARE_ITEM_ID_RANGE = 130,

	HEADER_GD_UPDATE_HORSE_NAME = 131,
	HEADER_GD_REQ_HORSE_NAME = 132,

	HEADER_GD_DC = 133, // Login Key를 지움

	HEADER_GD_VALID_LOGOUT = 134,

	//HEADER_GD_EMPTY = 135,
	//HEADER_GD_EMPTY = 136,

	HEADER_GD_REQUEST_CHARGE_CASH = 137,

	HEADER_GD_DELETE_AWARDID = 138, // delete gift notify icon

	HEADER_GD_UPDATE_CHANNELSTATUS = 139,
	HEADER_GD_REQUEST_CHANNELSTATUS = 140,

	//HEADER_GD_EMPTY = 141,
	//HEADER_GD_EMPTY = 142,
	//HEADER_GD_EMPTY = 143,

#if defined(__MOVE_CHANNEL__)
	HEADER_GD_FIND_CHANNEL = 144,
#endif

	//HEADER_GD_EMPTY = 145,
	//HEADER_GD_EMPTY = 146,
	//HEADER_GD_EMPTY = 147,
	//HEADER_GD_EMPTY = 148,
	//HEADER_GD_EMPTY = 149,
#if defined(__MAILBOX__)
	HEADER_GD_MAILBOX_LOAD = 150,
	HEADER_GD_MAILBOX_CHECK_NAME = 151,
	HEADER_GD_MAILBOX_WRITE = 152,
	HEADER_GD_MAILBOX_DELETE = 153,
	HEADER_GD_MAILBOX_CONFIRM = 154,
	HEADER_GD_MAILBOX_GET = 155,
	HEADER_GD_MAILBOX_UNREAD = 156,
#endif
	//HEADER_GD_EMPTY = 157,
	//HEADER_GD_EMPTY = 158,
	//HEADER_GD_EMPTY = 159,
#if defined(__GEM_SHOP__)
	HEADER_GD_GEM_SHOP_LOAD = 160,
	HEADER_GD_GEM_SHOP_UPDATE = 161,
#endif
	//HEADER_GD_EMPTY = 162,
	//HEADER_GD_EMPTY = 163,
	//HEADER_GD_EMPTY = 164,
#if defined(__EXPRESSING_EMOTIONS__)
	HEADER_GD_EMOTE_LOAD = 165,
	HEADER_GD_EMOTE_CLEAR = 166,
	HEADER_GD_EMOTE_ADD = 167,
#endif
	//HEADER_GD_EMPTY = 168,
	//HEADER_GD_EMPTY = 169,
	//HEADER_GD_EMPTY = 170,
	//HEADER_GD_EMPTY = 171,
	//HEADER_GD_EMPTY = 172,
	//HEADER_GD_EMPTY = 173,
	//HEADER_GD_EMPTY = 174,
	//HEADER_GD_EMPTY = 175,
	//HEADER_GD_EMPTY = 176,
	//HEADER_GD_EMPTY = 177,
	//HEADER_GD_EMPTY = 178,
	//HEADER_GD_EMPTY = 179,
	//HEADER_GD_EMPTY = 180,
	//HEADER_GD_EMPTY = 181,
	//HEADER_GD_EMPTY = 182,
	//HEADER_GD_EMPTY = 183,
	//HEADER_GD_EMPTY = 184,
	//HEADER_GD_EMPTY = 185,
	//HEADER_GD_EMPTY = 186,
	//HEADER_GD_EMPTY = 187,
	//HEADER_GD_EMPTY = 188,
	//HEADER_GD_EMPTY = 189,
	//HEADER_GD_EMPTY = 190,
	//HEADER_GD_EMPTY = 191,
	//HEADER_GD_EMPTY = 192,
	//HEADER_GD_EMPTY = 193,
	//HEADER_GD_EMPTY = 194,
	//HEADER_GD_EMPTY = 195,
	//HEADER_GD_EMPTY = 196,
	//HEADER_GD_EMPTY = 197,
	//HEADER_GD_EMPTY = 198,
	//HEADER_GD_EMPTY = 199,
	//HEADER_GD_EMPTY = 200,
	//HEADER_GD_EMPTY = 201,
	//HEADER_GD_EMPTY = 202,
	//HEADER_GD_EMPTY = 203,
	//HEADER_GD_EMPTY = 204,
	//HEADER_GD_EMPTY = 205,
	//HEADER_GD_EMPTY = 206,
	//HEADER_GD_EMPTY = 207,
	//HEADER_GD_EMPTY = 208,
	//HEADER_GD_EMPTY = 209,
	//HEADER_GD_EMPTY = 210,
	//HEADER_GD_EMPTY = 211,
	//HEADER_GD_EMPTY = 212,
	//HEADER_GD_EMPTY = 213,
	//HEADER_GD_EMPTY = 214,
	//HEADER_GD_EMPTY = 215,
	//HEADER_GD_EMPTY = 216,
	//HEADER_GD_EMPTY = 217,
	//HEADER_GD_EMPTY = 218,
	//HEADER_GD_EMPTY = 219,
	//HEADER_GD_EMPTY = 220,
	//HEADER_GD_EMPTY = 221,
	//HEADER_GD_EMPTY = 222,
	//HEADER_GD_EMPTY = 223,
	//HEADER_GD_EMPTY = 224,
	//HEADER_GD_EMPTY = 225,
	//HEADER_GD_EMPTY = 226,
	//HEADER_GD_EMPTY = 227,
	//HEADER_GD_EMPTY = 228,
	//HEADER_GD_EMPTY = 229,
	//HEADER_GD_EMPTY = 230,
	//HEADER_GD_EMPTY = 231,
	//HEADER_GD_EMPTY = 232,
	//HEADER_GD_EMPTY = 233,
	//HEADER_GD_EMPTY = 234,
	//HEADER_GD_EMPTY = 235,
	//HEADER_GD_EMPTY = 236,
	//HEADER_GD_EMPTY = 237,
	//HEADER_GD_EMPTY = 238,
	//HEADER_GD_EMPTY = 239,
	//HEADER_GD_EMPTY = 240,
	//HEADER_GD_EMPTY = 241,
	//HEADER_GD_EMPTY = 242,
	//HEADER_GD_EMPTY = 243,
	//HEADER_GD_EMPTY = 244,
	//HEADER_GD_EMPTY = 245,
	//HEADER_GD_EMPTY = 246,
	//HEADER_GD_EMPTY = 247,
	//HEADER_GD_EMPTY = 248,
	//HEADER_GD_EMPTY = 249,
	//HEADER_GD_EMPTY = 250,
	//HEADER_GD_EMPTY = 251,
	//HEADER_GD_EMPTY = 252,
	//HEADER_GD_EMPTY = 253,
	//HEADER_GD_EMPTY = 254,
	HEADER_GD_SETUP = 0xff, // 255
};

enum DG_HEADERS
{
	HEADER_DG_NOTICE = 1,
	//HEADER_DG_EMPTY = 2,
	//HEADER_DG_EMPTY = 3,
	//HEADER_DG_EMPTY = 4,
	//HEADER_DG_EMPTY = 5,
	//HEADER_DG_EMPTY = 6,
	//HEADER_DG_EMPTY = 7,
	//HEADER_DG_EMPTY = 8,
	//HEADER_DG_EMPTY = 9,
	//HEADER_DG_EMPTY = 10,
	//HEADER_DG_EMPTY = 11,
	//HEADER_DG_EMPTY = 12,
	//HEADER_DG_EMPTY = 13,
	//HEADER_DG_EMPTY = 14,
	//HEADER_DG_EMPTY = 15,
	//HEADER_DG_EMPTY = 16,
	//HEADER_DG_EMPTY = 17,
	//HEADER_DG_EMPTY = 18,
	//HEADER_DG_EMPTY = 19,
	//HEADER_DG_EMPTY = 20,
	//HEADER_DG_EMPTY = 21,
	//HEADER_DG_EMPTY = 22,
	//HEADER_DG_EMPTY = 23,
	//HEADER_DG_EMPTY = 24,
	//HEADER_DG_EMPTY = 25,
	//HEADER_DG_EMPTY = 26,
	//HEADER_DG_EMPTY = 27,
	//HEADER_DG_EMPTY = 28,
	//HEADER_DG_EMPTY = 29,
	HEADER_DG_LOGIN_SUCCESS = 30,
	HEADER_DG_LOGIN_NOT_EXIST = 31,
	//HEADER_DG_EMPTY = 32,
	HEADER_DG_LOGIN_WRONG_PASSWD = 33,
	HEADER_DG_LOGIN_ALREADY = 34,

	HEADER_DG_PLAYER_LOAD_SUCCESS = 35,
	HEADER_DG_PLAYER_LOAD_FAILED = 36,
	HEADER_DG_PLAYER_CREATE_SUCCESS = 37,
	HEADER_DG_PLAYER_CREATE_ALREADY = 38,
	HEADER_DG_PLAYER_CREATE_FAILED = 39,
	HEADER_DG_PLAYER_DELETE_SUCCESS = 40,
	HEADER_DG_PLAYER_DELETE_FAILED = 41,

	HEADER_DG_ITEM_LOAD = 42,

	HEADER_DG_BOOT = 43,
	HEADER_DG_QUEST_LOAD = 44,

	HEADER_DG_SAFEBOX_LOAD = 45,
	HEADER_DG_SAFEBOX_CHANGE_SIZE = 46,
	HEADER_DG_SAFEBOX_WRONG_PASSWORD = 47,
	HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER = 48,

	HEADER_DG_EMPIRE_SELECT = 49,

	HEADER_DG_AFFECT_LOAD = 50,
	HEADER_DG_MALL_LOAD = 51,

	//HEADER_DG_EMPTY = 52,
	//HEADER_DG_EMPTY = 53,
	//HEADER_DG_EMPTY = 54,

	HEADER_DG_DIRECT_ENTER = 55,

	HEADER_DG_GUILD_SKILL_UPDATE = 56,
	HEADER_DG_GUILD_SKILL_RECHARGE = 57,
	HEADER_DG_GUILD_EXP_UPDATE = 58,

	HEADER_DG_PARTY_CREATE = 59,
	HEADER_DG_PARTY_DELETE = 60,
	HEADER_DG_PARTY_ADD = 61,
	HEADER_DG_PARTY_REMOVE = 62,
	HEADER_DG_PARTY_STATE_CHANGE = 63,
	HEADER_DG_PARTY_HEAL_USE = 64,
	HEADER_DG_PARTY_SET_MEMBER_LEVEL = 65,

	//HEADER_DG_EMPTY = 66,
	//HEADER_DG_EMPTY = 67,
	//HEADER_DG_EMPTY = 68,
	//HEADER_DG_EMPTY = 69,
	//HEADER_DG_EMPTY = 70,
	//HEADER_DG_EMPTY = 71,
	//HEADER_DG_EMPTY = 72,
	//HEADER_DG_EMPTY = 73,
	//HEADER_DG_EMPTY = 74,
	//HEADER_DG_EMPTY = 75,
	//HEADER_DG_EMPTY = 76,
	//HEADER_DG_EMPTY = 77,
	//HEADER_DG_EMPTY = 78,
	//HEADER_DG_EMPTY = 79,
	//HEADER_DG_EMPTY = 80,
	//HEADER_DG_EMPTY = 81,
	//HEADER_DG_EMPTY = 82,
	//HEADER_DG_EMPTY = 83,
	//HEADER_DG_EMPTY = 84,
	//HEADER_DG_EMPTY = 85,
	//HEADER_DG_EMPTY = 86,
	//HEADER_DG_EMPTY = 87,
	//HEADER_DG_EMPTY = 88,
	//HEADER_DG_EMPTY = 89,

	HEADER_DG_TIME = 90,
	HEADER_DG_ITEM_ID_RANGE = 91,

	HEADER_DG_GUILD_ADD_MEMBER = 92,
	HEADER_DG_GUILD_REMOVE_MEMBER = 93,
	HEADER_DG_GUILD_CHANGE_GRADE = 94,
	HEADER_DG_GUILD_CHANGE_MEMBER_DATA = 95,
	HEADER_DG_GUILD_DISBAND = 96,
	HEADER_DG_GUILD_WAR = 97,
	HEADER_DG_GUILD_WAR_SCORE = 98,
	HEADER_DG_GUILD_TIME_UPDATE = 99,
	HEADER_DG_GUILD_LOAD = 100,
	HEADER_DG_GUILD_LADDER = 101,
	HEADER_DG_GUILD_SKILL_USABLE_CHANGE = 102,
	HEADER_DG_GUILD_MONEY_CHANGE = 103,
	HEADER_DG_GUILD_WITHDRAW_MONEY_GIVE = 104,

	HEADER_DG_SET_EVENT_FLAG = 105,

	HEADER_DG_GUILD_WAR_RESERVE_ADD = 106,
	HEADER_DG_GUILD_WAR_RESERVE_DEL = 107,
	HEADER_DG_GUILD_WAR_BET = 108,
#if defined(__GUILD_EVENT_FLAG__) //&& defined(__GUILD_RENEWAL__)
	HEADER_DG_GUILD_EVENT_FLAG = 109,
#endif
	//HEADER_DG_EMPTY = 110,
	//HEADER_DG_EMPTY = 111,
	//HEADER_DG_EMPTY = 112,
	//HEADER_DG_EMPTY = 113,
	//HEADER_DG_EMPTY = 114,
	//HEADER_DG_EMPTY = 115,
	//HEADER_DG_EMPTY = 116,
	//HEADER_DG_EMPTY = 117,
	//HEADER_DG_EMPTY = 118,
	//HEADER_DG_EMPTY = 119,
	HEADER_DG_RELOAD_PROTO = 120,
	HEADER_DG_CHANGE_NAME = 121,

	HEADER_DG_AUTH_LOGIN = 122,
	//HEADER_DG_EMPTY = 123,
	HEADER_DG_CHANGE_EMPIRE_PRIV = 124,
	HEADER_DG_CHANGE_GUILD_PRIV = 125,

	HEADER_DG_MONEY_LOG = 126,

	HEADER_DG_CHANGE_CHARACTER_PRIV = 127,

	//HEADER_DG_EMPTY = 128,
	//HEADER_DG_EMPTY = 129,
	//HEADER_DG_EMPTY = 130,
	//HEADER_DG_EMPTY = 131,
	//HEADER_DG_EMPTY = 132,
	//HEADER_DG_EMPTY = 133,
	//HEADER_DG_EMPTY = 134,
	//HEADER_DG_EMPTY = 135,
	//HEADER_DG_EMPTY = 136,
	//HEADER_DG_EMPTY = 137,
	//HEADER_DG_EMPTY = 138,
	//HEADER_DG_EMPTY = 139,
	HEADER_DG_CREATE_OBJECT = 140,
	HEADER_DG_DELETE_OBJECT = 141,
	HEADER_DG_UPDATE_LAND = 142,
	//HEADER_DG_EMPTY = 143,
	//HEADER_DG_EMPTY = 144,
	//HEADER_DG_EMPTY = 145,
	//HEADER_DG_EMPTY = 146,
	//HEADER_DG_EMPTY = 147,
	//HEADER_DG_EMPTY = 148,
	//HEADER_DG_EMPTY = 149,
	HEADER_DG_MARRIAGE_ADD = 150,
	HEADER_DG_MARRIAGE_UPDATE = 151,
	HEADER_DG_MARRIAGE_REMOVE = 152,

	HEADER_DG_WEDDING_REQUEST = 153,
	HEADER_DG_WEDDING_READY = 154,
	HEADER_DG_WEDDING_START = 155,
	HEADER_DG_WEDDING_END = 156,

	HEADER_DG_MYSHOP_PRICELIST_RES = 157, /// < 가격정보 리스트 응답
	HEADER_DG_RELOAD_ADMIN = 158, /// < 운영자 정보 리로드
	HEADER_DG_BREAK_MARRIAGE = 159, /// < 결혼 파기
	HEADER_DG_ELECT_MONARCH = 160, /// < 군주 투표
	HEADER_DG_CANDIDACY = 161, /// < 군주 등록
	HEADER_DG_ADD_MONARCH_MONEY = 162, /// < 군주 돈 증가
	HEADER_DG_TAKE_MONARCH_MONEY = 163, /// < 군주 돈 감소
	HEADER_DG_COME_TO_VOTE = 164, /// < 표결
	HEADER_DG_RMCANDIDACY = 165, /// < 후보 제거 (운영자)
	HEADER_DG_SETMONARCH = 166, /// < 군주설정 (운영자)
	HEADER_DG_RMMONARCH = 167, /// < 군주삭제
	HEADER_DG_DEC_MONARCH_MONEY = 168,

	HEADER_DG_CHANGE_MONARCH_LORD_ACK = 169,
	HEADER_DG_UPDATE_MONARCH_INFO = 170,
	HEADER_DG_BLOCK_COUNTRY_IP = 171, // 광대역 IP-Block
	HEADER_DG_BLOCK_EXCEPTION = 172, // 광대역 IP-Block 예외 account

	HEADER_DG_ACK_CHANGE_GUILD_MASTER = 173,

	HEADER_DG_ACK_SPARE_ITEM_ID_RANGE = 174,

	HEADER_DG_UPDATE_HORSE_NAME = 175,
	HEADER_DG_ACK_HORSE_NAME = 176,

	HEADER_DG_NEED_LOGIN_LOG = 177,

	//HEADER_DG_EMPTY = 178,

	HEADER_DG_RESULT_CHARGE_CASH = 179,
	HEADER_DG_ITEMAWARD_INFORMER = 180, // gift notify
	HEADER_DG_RESPOND_CHANNELSTATUS = 181,

#if defined(__EXPRESSING_EMOTIONS__)
	HEADER_DG_EMOTE_LOAD = 182,
	HEADER_DG_EMOTE_GET = 183,
#endif
	//HEADER_DG_EMPTY = 184,
#if defined(__MOVE_CHANNEL__)
	HEADER_DG_CHANNEL_RESULT = 185,
#endif
	//HEADER_DG_EMPTY = 186,
	//HEADER_DG_EMPTY = 187,
	//HEADER_DG_EMPTY = 188,
	//HEADER_DG_EMPTY = 189,
#if defined(__MAILBOX__)
	HEADER_DG_RESPOND_MAILBOX_LOAD = 190,
	HEADER_DG_RESPOND_MAILBOX_CHECK_NAME = 191,
	HEADER_DG_RESPOND_MAILBOX_UNREAD = 192,
#endif
	//HEADER_DG_EMPTY = 193,
	//HEADER_DG_EMPTY = 194,
	//HEADER_DG_EMPTY = 195,
#if defined(__GEM_SHOP__)
	HEADER_DG_GEM_SHOP_LOAD = 196,
	HEADER_DG_GEM_SHOP_UPDATE = 197,
#endif
	//HEADER_DG_EMPTY = 198,
	//HEADER_DG_EMPTY = 199,
	//HEADER_DG_EMPTY = 200,
	//HEADER_DG_EMPTY = 201,
	//HEADER_DG_EMPTY = 202,
	//HEADER_DG_EMPTY = 203,
	//HEADER_DG_EMPTY = 204,
	//HEADER_DG_EMPTY = 205,
	//HEADER_DG_EMPTY = 206,
	//HEADER_DG_EMPTY = 207,
	//HEADER_DG_EMPTY = 208,
	//HEADER_DG_EMPTY = 209,
	//HEADER_DG_EMPTY = 210,
	//HEADER_DG_EMPTY = 211,
	//HEADER_DG_EMPTY = 212,
	//HEADER_DG_EMPTY = 213,
	//HEADER_DG_EMPTY = 214,
	//HEADER_DG_EMPTY = 215,
	//HEADER_DG_EMPTY = 216,
	//HEADER_DG_EMPTY = 217,
	//HEADER_DG_EMPTY = 218,
	//HEADER_DG_EMPTY = 219,
	//HEADER_DG_EMPTY = 220,
	//HEADER_DG_EMPTY = 221,
	//HEADER_DG_EMPTY = 222,
	//HEADER_DG_EMPTY = 223,
	//HEADER_DG_EMPTY = 224,
	//HEADER_DG_EMPTY = 225,
	//HEADER_DG_EMPTY = 226,
	//HEADER_DG_EMPTY = 227,
	//HEADER_DG_EMPTY = 228,
	//HEADER_DG_EMPTY = 229,
	//HEADER_DG_EMPTY = 230,
	//HEADER_DG_EMPTY = 231,
	//HEADER_DG_EMPTY = 232,
	//HEADER_DG_EMPTY = 233,
	//HEADER_DG_EMPTY = 234,
	//HEADER_DG_EMPTY = 235,
	//HEADER_DG_EMPTY = 236,
	//HEADER_DG_EMPTY = 237,
	//HEADER_DG_EMPTY = 238,
	//HEADER_DG_EMPTY = 239,
	//HEADER_DG_EMPTY = 240,
	//HEADER_DG_EMPTY = 241,
	//HEADER_DG_EMPTY = 242,
	//HEADER_DG_EMPTY = 243,
	//HEADER_DG_EMPTY = 244,
	//HEADER_DG_EMPTY = 245,
	//HEADER_DG_EMPTY = 246,
	//HEADER_DG_EMPTY = 247,
	//HEADER_DG_EMPTY = 248,
	//HEADER_DG_EMPTY = 249,
	//HEADER_DG_EMPTY = 250,
	//HEADER_DG_EMPTY = 251,
	//HEADER_DG_EMPTY = 252,
	//HEADER_DG_EMPTY = 253,
	HEADER_DG_MAP_LOCATIONS = 0xfe, // 254
	HEADER_DG_P2P = 0xff, // 255
};

/* game Server -> DB Server */
#pragma pack(1)
enum ERequestChargeType
{
	ERequestCharge_Cash = 0,
	ERequestCharge_Mileage,
};

typedef struct SRequestChargeCash
{
	DWORD dwAID; // id(primary key) - Account Table
	DWORD dwAmount;
	ERequestChargeType eChargeType;

} TRequestChargeCash;

typedef struct SSimplePlayer
{
	DWORD dwID;
	char szName[CHARACTER_NAME_MAX_LEN + 1];
	BYTE byJob;
	BYTE byLevel;
	DWORD dwPlayMinutes;
	BYTE byST, byHT, byDX, byIQ;
	DWORD dwMainPart;
	BYTE bChangeName;
	DWORD dwHairPart;
#if defined(__ACCE_COSTUME_SYSTEM__)
	DWORD dwAccePart;
#endif
	BYTE bDummy[4];
	long x, y;
	long lAddr;
	WORD wPort;
	BYTE skill_group;
	DWORD last_play;
#if defined(__CONQUEROR_LEVEL__)
	BYTE byConquerorLevel;
	BYTE bySungmaStr, bySungmaHp, bySungmaMove, bySungmaImmune;
#endif
} TSimplePlayer;

typedef struct SAccountTable
{
	DWORD id;
	char login[LOGIN_MAX_LEN + 1];
	char passwd[PASSWD_MAX_LEN + 1];
	char social_id[SOCIAL_ID_MAX_LEN + 1];
	char status[ACCOUNT_STATUS_MAX_LEN + 1];
	BYTE bEmpire;
#if defined(__MULTI_LANGUAGE_SYSTEM__)
	char country[COUNTRY_NAME_MAX_LEN + 1];
#endif
	TSimplePlayer players[PLAYER_PER_ACCOUNT];
} TAccountTable;

typedef struct SPacketDGCreateSuccess
{
	BYTE bAccountCharacterIndex;
	TSimplePlayer player;
} TPacketDGCreateSuccess;

typedef struct SPlayerItemAttribute
{
	POINT_TYPE wType;
	POINT_VALUE lValue;
#if defined(__ITEM_APPLY_RANDOM__)
	BYTE bPath;
#endif
} TPlayerItemAttribute;

#if defined(__REFINE_ELEMENT_SYSTEM__)
typedef struct SPlayerItemRefineElement
{
	WORD wApplyType;
	BYTE bGrade;
	BYTE abValue[REFINE_ELEMENT_MAX];
	BYTE abBonusValue[REFINE_ELEMENT_MAX];
	SPlayerItemRefineElement()
	{
		wApplyType = 0;
		bGrade = 0;
		memset(&abValue, 0, sizeof(abValue));
		memset(&abBonusValue, 0, sizeof(abBonusValue));
	}
} TPlayerItemRefineElement;
#endif

typedef struct SPlayerItem
{
	DWORD dwID;
	DWORD dwOwner;

	BYTE bWindow;
	WORD wPos;

	DWORD dwVnum;
	DWORD dwCount;

#if defined(__SOUL_BIND_SYSTEM__)
	long lSealDate;
#endif

	long alSockets[ITEM_SOCKET_MAX_NUM]; // 소켓번호
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
#if defined(__CHANGE_LOOK_SYSTEM__)
	DWORD dwTransmutationVnum;
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
	TPlayerItemRefineElement RefineElement;
#endif
#if defined(__ITEM_APPLY_RANDOM__)
	TPlayerItemAttribute aApplyRandom[ITEM_APPLY_MAX_NUM];
#endif
#if defined(__SET_ITEM__)
	BYTE bSetValue;
#endif
} TPlayerItem;

typedef struct packet_item
{
	DWORD dwVnum;
	DWORD dwCount;
	DWORD dwFlags;
	DWORD dwAntiFlags;
#if defined(__SOUL_BIND_SYSTEM__)
	long lSealDate;
#endif
	long alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
#if defined(__CHANGE_LOOK_SYSTEM__)
	DWORD dwTransmutationVnum;
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
	TPlayerItemRefineElement RefineElement;
#endif
#if defined(__ITEM_APPLY_RANDOM__)
	TPlayerItemAttribute aApplyRandom[ITEM_APPLY_MAX_NUM];
#endif
#if defined(__SET_ITEM__)
	BYTE bSetValue;
#endif
	packet_item()
	{
		memset(&alSockets, 0, sizeof(alSockets));
		memset(&aAttr, 0, sizeof(aAttr));
#if defined(__CHANGE_LOOK_SYSTEM__)
		dwTransmutationVnum = 0;
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
		memset(&RefineElement, 0, sizeof(RefineElement));
#endif
#if defined(__ITEM_APPLY_RANDOM__)
		memset(&aApplyRandom, 0, sizeof(aApplyRandom));
#endif
#if defined(__SET_ITEM__)
		bSetValue = 0;
#endif
	}
} TItemData;

typedef struct SQuickslot
{
	BYTE type;
	WORD pos;
} TQuickslot;

typedef struct SPlayerSkill
{
	BYTE bMasterType;
	BYTE bLevel;
	time_t tNextRead;
} TPlayerSkill;

typedef struct SHorseInfo
{
	BYTE bLevel;
	BYTE bRiding;
	short sStamina;
	short sHealth;
	DWORD dwHorseHealthDropTime;
} THorseInfo;

typedef struct SPlayerTable
{
	DWORD id;

	char name[CHARACTER_NAME_MAX_LEN + 1];
	char ip[IP_ADDRESS_LENGTH + 1];

	WORD job;
	BYTE voice;

	BYTE level;
	BYTE level_step;
	short st, ht, dx, iq;

	DWORD exp;
	int gold;
#if defined(__CHEQUE_SYSTEM__)
	int cheque;
#endif
#if defined(__GEM_SYSTEM__)
	int gem;
#endif

	BYTE dir;
	INT x, y, z;
	INT lMapIndex;

	long lExitX, lExitY;
	long lExitMapIndex;

	int hp;
	int sp;

	short sRandomHP;
	short sRandomSP;

	int playtime;

	short stat_point;
	short skill_point;
	short sub_skill_point;
	short horse_skill_point;

	TPlayerSkill skills[SKILL_MAX_NUM];

	TQuickslot quickslot[QUICKSLOT_MAX_NUM];

	BYTE part_base;
	DWORD adwParts[PART_MAX_NUM];

	short stamina;

	BYTE skill_group;
	long lAlignment;

	short stat_reset_count;

	THorseInfo horse;

	DWORD logoff_interval;
	DWORD last_play;

	int aiPremiumTimes[PREMIUM_MAX_NUM];

#if defined(__CONQUEROR_LEVEL__)
	BYTE conqueror_level;
	BYTE conqueror_level_step;
	short sungma_str, sungma_hp, sungma_move, sungma_immune;
	DWORD conqueror_exp;
	short conqueror_point;
#endif

#if defined(__EXTEND_INVEN_SYSTEM__)
	BYTE inven_stage;
#endif
} TPlayerTable;

typedef struct SMobSkillLevel
{
	DWORD dwVnum;
	BYTE bLevel;
} TMobSkillLevel;

typedef struct SEntityTable
{
	DWORD dwVnum;
} TEntityTable;

typedef struct SMobTable : public SEntityTable
{
	char szName[CHARACTER_NAME_MAX_LEN + 1];
	char szLocaleName[CHARACTER_NAME_MAX_LEN + 1];

	BYTE bType; // Monster, NPC
	BYTE bRank; // PAWN, KNIGHT, KING
	BYTE bBattleType; // MELEE, etc..
	BYTE bLevel;
	BYTE bScale;
	BYTE bSize;

	DWORD dwGoldMin;
	DWORD dwGoldMax;
	DWORD dwExp;
	DWORD dwSungMaExp;
	DWORD dwMaxHP;
	BYTE bRegenCycle;
	BYTE bRegenPercent;
	WORD wDef;

	DWORD dwAIFlag;
	DWORD dwRaceFlag;
	DWORD dwImmuneFlag;

	BYTE bStr, bDex, bCon, bInt;
	BYTE bSungMaStr, bSungMaDex, bSungMaCon, bSungMaInt;
	DWORD dwDamageRange[2];

	short sAttackSpeed;
	short sMovingSpeed;

	BYTE bAggressiveHPPct;
	WORD wAggressiveSight;
	WORD wAttackRange;

	char cEnchants[MOB_ENCHANTS_MAX_NUM];
	char cResists[MOB_RESISTS_MAX_NUM];
	char cElements[MOB_ELEMENT_MAX_NUM];
	char cResistDark, cResistIce, cResistEarth;

	DWORD dwResurrectionVnum;
	DWORD dwDropItemVnum;

	BYTE bMountCapacity;
	BYTE bOnClickType;

	BYTE bEmpire;
	char szFolder[CHARACTER_FOLDER_MAX_LEN + 1];

	float fDamMultiply;

	DWORD dwSummonVnum;
	DWORD dwDrainSP;
	DWORD dwMobColor;
	DWORD dwPolymorphItemVnum;

	TMobSkillLevel Skills[MOB_SKILL_MAX_NUM];

	BYTE bBerserkPoint;
	BYTE bStoneSkinPoint;
	BYTE bGodSpeedPoint;
	BYTE bDeathBlowPoint;
	BYTE bRevivePoint;

	BYTE bHealPoint;

	BYTE bRAttSpeedPoint;
	BYTE bRCastSpeedPoint;

	BYTE bRHPRegenPoint;

	float fHitRange;
} TMobTable;

typedef struct SSkillTable
{
	DWORD dwVnum;
	char szName[32 + 1];
	BYTE bType;
	BYTE bMaxLevel;
	DWORD dwSplashRange;

	char szPointOn[64];
	char szPointPoly[100 + 1];
	char szSPCostPoly[100 + 1];
	char szDurationPoly[100 + 1];
	char szDurationSPCostPoly[100 + 1];
	char szCooldownPoly[100 + 1];
	char szMasterBonusPoly[100 + 1];
	//char szAttackGradePoly[100 + 1];
	char szGrandMasterAddSPCostPoly[100 + 1];
	DWORD dwFlag;
	DWORD dwAffectFlag;

	// Data for secondary skill
	char szPointOn2[64];
	char szPointPoly2[100 + 1];
	char szDurationPoly2[100 + 1];
	DWORD dwAffectFlag2;

	// Data for grand master point
	char szPointOn3[64];
	char szPointPoly3[100 + 1];
	char szDurationPoly3[100 + 1];

	BYTE bLevelStep;
	BYTE bLevelLimit;
	DWORD preSkillVnum;
	BYTE preSkillLevel;

	long lMaxHit;
	char szSplashAroundDamageAdjustPoly[100 + 1];

	BYTE bSkillAttrType;

	DWORD dwTargetRange;
} TSkillTable;

typedef struct SShopItemTable
{
	DWORD vnum;
	DWORD count;

	TItemPos pos; // PC 상점에만 이용
	DWORD price; // PC, shop_table_ex.txt 상점에만 이용
#if defined(__CHEQUE_SYSTEM__)
	DWORD cheque;
#endif
	BYTE display_pos; // PC, shop_table_ex.txt 상점에만 이용, 보일 위치.
#if defined(__SHOPEX_RENEWAL__)
	BYTE bPriceType;
	DWORD dwPriceVnum;
	long alSockets[ITEM_SOCKET_MAX_NUM];
	SShopItemTable() : bPriceType(SHOP_COIN_TYPE_GOLD), dwPriceVnum(0)
	{
		memset(&alSockets, 0, sizeof(alSockets));
	}
#endif
} TShopItemTable;

typedef struct SShopTable
{
	DWORD dwVnum;
	DWORD dwNPCVnum;
	BYTE bItemCount;
#if defined(__MYSHOP_EXPANSION__)
	TShopItemTable items[SHOP_HOST_ITEM_MAX];
#else
	TShopItemTable items[SHOP_HOST_ITEM_MAX_NUM];
#endif
} TShopTable;

#define QUEST_NAME_MAX_LEN 50 // 32
#define QUEST_STATE_MAX_LEN 64

typedef struct SQuestTable
{
	DWORD dwPID;
	char szName[QUEST_NAME_MAX_LEN + 1];
	char szState[QUEST_STATE_MAX_LEN + 1];
	long lValue;
} TQuestTable;

typedef struct SItemLimit
{
	BYTE bType;
	long lValue;
} TItemLimit;

typedef struct SItemApply
{
	POINT_TYPE wType;
	POINT_VALUE lValue;
} TItemApply;

typedef struct SItemTable : public SEntityTable
{
	DWORD dwVnumRange;
	char szName[ITEM_NAME_MAX_LEN + 1];
	char szLocaleName[ITEM_NAME_MAX_LEN + 1];
	BYTE bType;
	BYTE bSubType;

	BYTE bWeight;
	BYTE bSize;

	uint64_t ullAntiFlags;
	DWORD dwFlags;
	DWORD dwWearFlags;
	DWORD dwImmuneFlag;

	DWORD dwShopBuyPrice;
	DWORD dwShopSellPrice;

	TItemLimit aLimits[ITEM_LIMIT_MAX_NUM];
	TItemApply aApplies[ITEM_APPLY_MAX_NUM];
	long alValues[ITEM_VALUES_MAX_NUM];
	long alSockets[ITEM_SOCKET_MAX_NUM];
	DWORD dwRefinedVnum;
	WORD wRefineSet;
	DWORD dw67AttrMaterial;
	BYTE bAlterToMagicItemPct;
	BYTE bSpecular;
	BYTE bGainSocketPct;

	short int sAddonType; // 기본 속성

	// 아래 limit flag들은 realtime에 체크 할 일이 많고, 아이템 VNUM당 고정된 값인데,
	// 현재 구조대로 매번 아이템마다 필요한 경우에 LIMIT_MAX_NUM까지 루프돌면서 체크하는 부하가 커서 미리 저장 해 둠.
	char cLimitRealTimeFirstUseIndex; // 아이템 limit 필드값 중에서 LIMIT_REAL_TIME_FIRST_USE 플래그의 위치 (없으면 -1)
	char cLimitTimerBasedOnWearIndex; // 아이템 limit 필드값 중에서 LIMIT_TIMER_BASED_ON_WEAR 플래그의 위치 (없으면 -1)

	char* GetOriginalName() { return szName; }
	DWORD GetVNum() { return dwVnum; }
	char* GetName() { return szLocaleName; }

	BYTE GetType() { return bType; }
	BYTE GetSubType() { return bSubType; }
	BYTE GetWeight() { return bWeight; }
	BYTE GetSize() { return bSize; }

	uint64_t GetAntiFlags() { return ullAntiFlags; }
	DWORD GetWearFlags() { return dwWearFlags; }
	DWORD GetImmuneFlags() { return dwImmuneFlag; }

	DWORD GetBuyPrice() { return dwShopBuyPrice; }
	DWORD GetSellPrice() { return dwShopSellPrice; }

	long GetValue(unsigned int index)
	{
		return alValues[index];
	}

	BYTE GetLimitType(DWORD idx) const { return aLimits[idx].bType; }
	long GetLimitValue(DWORD idx) const { return aLimits[idx].lValue; }

	// Weapon
	bool IsWeapon() { return GetType() == ITEM_WEAPON; }
	bool IsSword() { return GetType() == ITEM_WEAPON && GetSubType() == WEAPON_SWORD; }
	bool IsDagger() { return GetType() == ITEM_WEAPON && GetSubType() == WEAPON_DAGGER; }
	bool IsBow() { return GetType() == ITEM_WEAPON && GetSubType() == WEAPON_BOW; }
	bool IsTwoHandSword() { return GetType() == ITEM_WEAPON && GetSubType() == WEAPON_TWO_HANDED; }
	bool IsBell() { return GetType() == ITEM_WEAPON && GetSubType() == WEAPON_BELL; }
	bool IsFan() { return GetType() == ITEM_WEAPON && GetSubType() == WEAPON_FAN; }
	bool IsArrow() { return GetType() == ITEM_WEAPON && GetSubType() == WEAPON_ARROW; }
	bool IsMountSpear() { return GetType() == ITEM_WEAPON && GetSubType() == WEAPON_MOUNT_SPEAR; }
	bool IsClaw() { return GetType() == ITEM_WEAPON && GetSubType() == WEAPON_CLAW; }
#if defined(__QUIVER_SYSTEM__)
	bool IsQuiver() { return GetType() == ITEM_WEAPON && GetSubType() == WEAPON_QUIVER; }
#endif

	// Armor
	bool IsArmor() { return GetType() == ITEM_ARMOR; }
	bool IsArmorBody() { return GetType() == ITEM_ARMOR && GetSubType() == ARMOR_BODY; }
	bool IsHelmet() { return GetType() == ITEM_ARMOR && GetSubType() == ARMOR_HEAD; }
	bool IsShield() { return GetType() == ITEM_ARMOR && GetSubType() == ARMOR_SHIELD; }
	bool IsWrist() { return GetType() == ITEM_ARMOR && GetSubType() == ARMOR_WRIST; }
	bool IsShoe() { return GetType() == ITEM_ARMOR && GetSubType() == ARMOR_FOOTS; }
	bool IsNecklace() { return GetType() == ITEM_ARMOR && GetSubType() == ARMOR_NECK; }
	bool IsEarRing() { return GetType() == ITEM_ARMOR && GetSubType() == ARMOR_EAR; }

	bool IsBelt() { return GetType() == ITEM_BELT; }
	bool IsRing() { return GetType() == ITEM_RING; }

	// Costume
	bool IsCostume() { return GetType() == ITEM_COSTUME; }
	bool IsCostumeBody() { return GetType() == ITEM_COSTUME && GetSubType() == COSTUME_BODY; }
	bool IsCostumeHair() { return GetType() == ITEM_COSTUME && GetSubType() == COSTUME_HAIR; }
#if defined(__MOUNT_COSTUME_SYSTEM__)
	bool IsCostumeMount() { return GetType() == ITEM_COSTUME && GetSubType() == COSTUME_MOUNT; }
#endif
#if defined(__ACCE_COSTUME_SYSTEM__)
	bool IsCostumeAcce() { return GetType() == ITEM_COSTUME && GetSubType() == COSTUME_ACCE; }
#endif
#if defined(__WEAPON_COSTUME_SYSTEM__)
	bool IsCostumeWeapon() { return GetType() == ITEM_COSTUME && GetSubType() == COSTUME_WEAPON; }
#endif
#if defined(__MOVE_COSTUME_ATTR__)
	bool IsCostumeModifyItem() { return GetType() == ITEM_USE && (GetSubType() == USE_CHANGE_COSTUME_ATTR || GetSubType() == USE_RESET_COSTUME_ATTR); }
#endif

	bool CanUseByJob(BYTE bJob)
	{
		switch (bJob)
		{
		case JOB_WARRIOR:
			if (GetAntiFlags() & ITEM_ANTIFLAG_WARRIOR)
				return false;
			break;
		case JOB_ASSASSIN:
			if (GetAntiFlags() & ITEM_ANTIFLAG_ASSASSIN)
				return false;
			break;
		case JOB_SHAMAN:
			if (GetAntiFlags() & ITEM_ANTIFLAG_SHAMAN)
				return false;
			break;
		case JOB_SURA:
			if (GetAntiFlags() & ITEM_ANTIFLAG_SURA)
				return false;
			break;
		case JOB_WOLFMAN:
			if (GetAntiFlags() & ITEM_ANTIFLAG_WOLFMAN)
				return false;
			break;
		}
		return true;
	}

	POINT_VALUE FindApplyValue(POINT_TYPE dwType)
	{
		for (BYTE bIndex = 0; bIndex < ITEM_APPLY_MAX_NUM; ++bIndex)
		{
			if (aApplies[bIndex].wType == dwType)
				return aApplies[bIndex].lValue;
		}
		return 0;
	}

} TItemTable;

struct TItemAttrTable
{
	TItemAttrTable() :
		wApplyIndex(0),
		dwProb(0)
	{
		szApply[0] = 0;
		memset(&lValues, 0, sizeof(lValues));
		memset(&bMaxLevelBySet, 0, sizeof(bMaxLevelBySet));
	}

	char szApply[APPLY_NAME_MAX_LEN + 1];
	POINT_TYPE wApplyIndex;
	DWORD dwProb;
	POINT_VALUE lValues[ITEM_ATTRIBUTE_MAX_LEVEL];
	BYTE bMaxLevelBySet[ATTRIBUTE_SET_MAX_NUM];
};

typedef struct SConnectTable
{
	char login[LOGIN_MAX_LEN + 1];
	IDENT ident;
} TConnectTable;

typedef struct SLoginPacket
{
	char login[LOGIN_MAX_LEN + 1];
	char passwd[PASSWD_MAX_LEN + 1];
} TLoginPacket;

typedef struct SPlayerLoadPacket
{
	DWORD account_id;
	DWORD player_id;
	BYTE account_index; /* account 에서의 위치 */
} TPlayerLoadPacket;

typedef struct SPlayerCreatePacket
{
	char login[LOGIN_MAX_LEN + 1];
	char passwd[PASSWD_MAX_LEN + 1];
	DWORD account_id;
	BYTE account_index;
	TPlayerTable player_table;
} TPlayerCreatePacket;

typedef struct SPlayerDeletePacket
{
	char login[LOGIN_MAX_LEN + 1];
	DWORD player_id;
	BYTE account_index;
	//char name[CHARACTER_NAME_MAX_LEN + 1];
	char private_code[8];
} TPlayerDeletePacket;

#if defined(__DELETE_FAILURE_TYPE__)
typedef struct SPlayerDeleteFailurePacket
{
	BYTE bType;
	INT iTime;
} TPlayerDeleteFailurePacket;
#endif

typedef struct SLogoutPacket
{
	char login[LOGIN_MAX_LEN + 1];
	char passwd[PASSWD_MAX_LEN + 1];
} TLogoutPacket;

typedef struct SPlayerCountPacket
{
	DWORD dwCount;
} TPlayerCountPacket;

#if defined(__EXTEND_MALLBOX__)
#	define SAFEBOX_MAX_NUM 225 // 45 (Slots) * 5 (Pages)
#else
#	define SAFEBOX_MAX_NUM 135 // 45 (Slots) * 3 (Pages)
#endif
#define SAFEBOX_PASSWORD_MAX_LEN 6

typedef struct SSafeboxTable
{
	DWORD dwID;
	BYTE bSize;
	DWORD dwGold;
	WORD wItemCount;
} TSafeboxTable;

typedef struct SSafeboxChangeSizePacket
{
	DWORD dwID;
	BYTE bSize;
} TSafeboxChangeSizePacket;

typedef struct SSafeboxLoadPacket
{
	DWORD dwID;
	char szLogin[LOGIN_MAX_LEN + 1];
	char szPassword[SAFEBOX_PASSWORD_MAX_LEN + 1];
} TSafeboxLoadPacket;

typedef struct SSafeboxChangePasswordPacket
{
	DWORD dwID;
	char szOldPassword[SAFEBOX_PASSWORD_MAX_LEN + 1];
	char szNewPassword[SAFEBOX_PASSWORD_MAX_LEN + 1];
} TSafeboxChangePasswordPacket;

typedef struct SSafeboxChangePasswordPacketAnswer
{
	BYTE flag;
} TSafeboxChangePasswordPacketAnswer;

typedef struct SEmpireSelectPacket
{
	DWORD dwAccountID;
	BYTE bEmpire;
} TEmpireSelectPacket;

typedef struct SPacketGDSetup
{
	char szPublicIP[16]; // Public IP which listen to users
	BYTE bChannel; // 채널
	WORD wListenPort; // 클라이언트가 접속하는 포트 번호
	WORD wP2PPort; // 서버끼리 연결 시키는 P2P 포트 번호
	long alMaps[MAX_MAP_ALLOW];
	DWORD dwLoginCount;
	BYTE bAuthServer;
} TPacketGDSetup;

typedef struct SPacketDGMapLocations
{
	BYTE bCount;
} TPacketDGMapLocations;

typedef struct SMapLocation
{
	long alMaps[MAX_MAP_ALLOW];
	char szHost[MAX_HOST_LENGTH + 1];
	WORD wPort;
} TMapLocation;

typedef struct SPacketDGP2P
{
	char szHost[MAX_HOST_LENGTH + 1];
	WORD wPort;
	BYTE bChannel;
} TPacketDGP2P;

typedef struct SPacketGDDirectEnter
{
	char login[LOGIN_MAX_LEN + 1];
	char passwd[PASSWD_MAX_LEN + 1];
	BYTE index;
} TPacketGDDirectEnter;

typedef struct SPacketDGDirectEnter
{
	TAccountTable accountTable;
	TPlayerTable playerTable;
} TPacketDGDirectEnter;

typedef struct SPacketGuildSkillUpdate
{
	DWORD guild_id;
	int amount;
	BYTE skill_levels[12];
	BYTE skill_point;
	BYTE save;
} TPacketGuildSkillUpdate;

typedef struct SPacketGuildExpUpdate
{
	DWORD guild_id;
	int amount;
} TPacketGuildExpUpdate;

typedef struct SPacketGuildChangeMemberData
{
	DWORD guild_id;
	DWORD pid;
	DWORD offer;
	BYTE level;
	BYTE grade;
} TPacketGuildChangeMemberData;

typedef struct SPacketDGLoginAlready
{
	char szLogin[LOGIN_MAX_LEN + 1];
} TPacketDGLoginAlready;

typedef struct TPacketAffectElement
{
	DWORD dwType;
	POINT_TYPE wApplyOn;
	POINT_VALUE lApplyValue;
	DWORD dwFlag;
	long lDuration;
	long lSPCost;
#if defined(__AFFECT_RENEWAL__)
	bool bRealTime;
	bool bUpdate;
#endif
} TPacketAffectElement;

typedef struct SPacketGDAddAffect
{
	DWORD dwPID;
	TPacketAffectElement elem;
} TPacketGDAddAffect;

typedef struct SPacketGDRemoveAffect
{
	DWORD dwPID;
	DWORD dwType;
	POINT_TYPE wApplyOn;
} TPacketGDRemoveAffect;

typedef struct SPacketGDHighscore
{
	DWORD dwPID;
	long lValue;
	char cDir;
	char szBoard[21];
} TPacketGDHighscore;

typedef struct SPacketPartyCreate
{
	DWORD dwLeaderPID;
} TPacketPartyCreate;

typedef struct SPacketPartyDelete
{
	DWORD dwLeaderPID;
} TPacketPartyDelete;

typedef struct SPacketPartyAdd
{
	DWORD dwLeaderPID;
	DWORD dwPID;
	BYTE bState;
} TPacketPartyAdd;

typedef struct SPacketPartyRemove
{
	DWORD dwLeaderPID;
	DWORD dwPID;
} TPacketPartyRemove;

typedef struct SPacketPartyStateChange
{
	DWORD dwLeaderPID;
	DWORD dwPID;
	BYTE bRole;
	BYTE bFlag;
} TPacketPartyStateChange;

typedef struct SPacketPartySetMemberLevel
{
	DWORD dwLeaderPID;
	DWORD dwPID;
	BYTE bLevel;
} TPacketPartySetMemberLevel;

typedef struct SPacketGDBoot
{
	DWORD dwItemIDRange[2];
	char szIP[16];
} TPacketGDBoot;

typedef struct SPacketGuild
{
	DWORD dwGuild;
	DWORD dwInfo;
} TPacketGuild;

typedef struct SPacketGDGuildAddMember
{
	DWORD dwPID;
	DWORD dwGuild;
	BYTE bGrade;
} TPacketGDGuildAddMember;

typedef struct SPacketDGGuildMember
{
	DWORD dwPID;
	DWORD dwGuild;
	BYTE bGrade;
	BYTE isGeneral;
	BYTE bJob;
	BYTE bLevel;
	DWORD dwOffer;
	char szName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketDGGuildMember;

typedef struct SPacketGuildWar
{
	BYTE bType;
	BYTE bWar;
	DWORD dwGuildFrom;
	DWORD dwGuildTo;
	long lWarPrice;
	long lInitialScore;
} TPacketGuildWar;

// Game -> DB : 상대적 변화값
// DB -> Game : 토탈된 최종값
typedef struct SPacketGuildWarScore
{
	DWORD dwGuildGainPoint;
	DWORD dwGuildOpponent;
	long lScore;
	long lBetScore;
} TPacketGuildWarScore;

typedef struct SRefineMaterial
{
	DWORD vnum;
	int count;
} TRefineMaterial;

typedef struct SRefineTable
{
	//DWORD src_vnum;
	//DWORD result_vnum;
	DWORD id;
	WORD material_count;
	int cost; // 소요 비용
	int prob; // 확률
	TRefineMaterial materials[REFINE_MATERIAL_MAX_NUM];
} TRefineTable;

typedef struct SBanwordTable
{
	char szWord[BANWORD_MAX_LEN + 1];
} TBanwordTable;

typedef struct SPacketGDChangeName
{
	DWORD pid;
	char name[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGDChangeName;

typedef struct SPacketDGChangeName
{
	DWORD pid;
	char name[CHARACTER_NAME_MAX_LEN + 1];
} TPacketDGChangeName;

typedef struct SPacketGuildLadder
{
	DWORD dwGuild;
	long lLadderPoint;
	long lWin;
	long lDraw;
	long lLoss;
} TPacketGuildLadder;

typedef struct SPacketGuildLadderPoint
{
	DWORD dwGuild;
	long lChange;
} TPacketGuildLadderPoint;

typedef struct SPacketGuildUseSkill
{
	DWORD dwGuild;
	DWORD dwSkillVnum;
	DWORD dwCooltime;
} TPacketGuildUseSkill;

typedef struct SPacketGuildSkillUsableChange
{
	DWORD dwGuild;
	DWORD dwSkillVnum;
	BYTE bUsable;
} TPacketGuildSkillUsableChange;

typedef struct SPacketGDLoginKey
{
	DWORD dwAccountID;
	DWORD dwLoginKey;
} TPacketGDLoginKey;

typedef struct SPacketGDAuthLogin
{
	DWORD dwID;
	DWORD dwLoginKey;
	char szLogin[LOGIN_MAX_LEN + 1];
	char szSocialID[SOCIAL_ID_MAX_LEN + 1];
	DWORD adwClientKey[4];
	int iPremiumTimes[PREMIUM_MAX_NUM];
#if defined(__MULTI_LANGUAGE_SYSTEM__)
	char szCountry[COUNTRY_NAME_MAX_LEN + 1];
#endif
} TPacketGDAuthLogin;

typedef struct SPacketGDLoginByKey
{
	char szLogin[LOGIN_MAX_LEN + 1];
	DWORD dwLoginKey;
	DWORD adwClientKey[4];
	char szIP[MAX_HOST_LENGTH + 1];
} TPacketGDLoginByKey;

/**
* @version 05/06/08 Bang2ni - 지속시간 추가
**/
typedef struct SPacketGiveGuildPriv
{
	BYTE type;
	int value;
	DWORD guild_id;
	time_t duration_sec; ///< 지속시간
} TPacketGiveGuildPriv;

typedef struct SPacketGiveEmpirePriv
{
	BYTE type;
	int value;
	BYTE empire;
	time_t duration_sec;
} TPacketGiveEmpirePriv;

typedef struct SPacketGiveCharacterPriv
{
	BYTE type;
	int value;
	DWORD pid;
} TPacketGiveCharacterPriv;

typedef struct SPacketRemoveGuildPriv
{
	BYTE type;
	DWORD guild_id;
} TPacketRemoveGuildPriv;

typedef struct SPacketRemoveEmpirePriv
{
	BYTE type;
	BYTE empire;
} TPacketRemoveEmpirePriv;

typedef struct SPacketDGChangeCharacterPriv
{
	BYTE type;
	int value;
	DWORD pid;
	BYTE bLog;
} TPacketDGChangeCharacterPriv;

/**
* @version 05/06/08 Bang2ni - 지속시간 추가
**/
typedef struct SPacketDGChangeGuildPriv
{
	BYTE type;
	int value;
	DWORD guild_id;
	BYTE bLog;
	time_t end_time_sec; ///< 지속시간
} TPacketDGChangeGuildPriv;

typedef struct SPacketDGChangeEmpirePriv
{
	BYTE type;
	int value;
	BYTE empire;
	BYTE bLog;
	time_t end_time_sec;
} TPacketDGChangeEmpirePriv;

typedef struct SPacketMoneyLog
{
	BYTE type;
	DWORD vnum;
	int gold;
#if defined(__CHEQUE_SYSTEM__)
	int cheque;
#endif
} TPacketMoneyLog;

typedef struct SPacketGDGuildMoney
{
	DWORD dwGuild;
	INT iGold;
} TPacketGDGuildMoney;

typedef struct SPacketDGGuildMoneyChange
{
	DWORD dwGuild;
	INT iTotalGold;
} TPacketDGGuildMoneyChange;

typedef struct SPacketDGGuildMoneyWithdraw
{
	DWORD dwGuild;
	INT iChangeGold;
} TPacketDGGuildMoneyWithdraw;

typedef struct SPacketGDGuildMoneyWithdrawGiveReply
{
	DWORD dwGuild;
	INT iChangeGold;
	BYTE bGiveSuccess;
} TPacketGDGuildMoneyWithdrawGiveReply;

typedef struct SPacketSetEventFlag
{
	char szFlagName[EVENT_FLAG_NAME_MAX_LEN + 1];
	long lValue;
} TPacketSetEventFlag;

typedef struct SPacketLoginOnSetup
{
	DWORD dwID;
	char szLogin[LOGIN_MAX_LEN + 1];
	char szSocialID[SOCIAL_ID_MAX_LEN + 1];
	char szHost[MAX_HOST_LENGTH + 1];
	DWORD dwLoginKey;
	DWORD adwClientKey[4];
#if defined(__MULTI_LANGUAGE_SYSTEM__)
	char szCountry[COUNTRY_NAME_MAX_LEN + 1];
#endif
} TPacketLoginOnSetup;

typedef struct SPacketGDCreateObject
{
	DWORD dwVnum;
	DWORD dwLandID;
	INT lMapIndex;
	INT x, y;
	float xRot;
	float yRot;
	float zRot;
} TPacketGDCreateObject;

typedef struct SPacketGDHammerOfTor
{
	DWORD key;
	DWORD delay;
} TPacketGDHammerOfTor;

typedef struct SGuildReserve
{
	DWORD dwID;
	DWORD dwGuildFrom;
	DWORD dwGuildTo;
	DWORD dwTime;
	BYTE bType;
	long lWarPrice;
	long lInitialScore;
	bool bStarted;
	DWORD dwBetFrom;
	DWORD dwBetTo;
	long lPowerFrom;
	long lPowerTo;
	long lHandicap;
} TGuildWarReserve;

typedef struct
{
	DWORD dwWarID;
	char szLogin[LOGIN_MAX_LEN + 1];
	DWORD dwGold;
	DWORD dwGuild;
} TPacketGDGuildWarBet;

// Marriage
typedef struct
{
	DWORD dwPID1;
	DWORD dwPID2;
	time_t tMarryTime;
	char szName1[CHARACTER_NAME_MAX_LEN + 1];
	char szName2[CHARACTER_NAME_MAX_LEN + 1];
} TPacketMarriageAdd;

typedef struct
{
	DWORD dwPID1;
	DWORD dwPID2;
	INT iLovePoint;
	BYTE byMarried;
} TPacketMarriageUpdate;

typedef struct
{
	DWORD dwPID1;
	DWORD dwPID2;
} TPacketMarriageRemove;

typedef struct
{
	DWORD dwPID1;
	DWORD dwPID2;
} TPacketWeddingRequest;

typedef struct
{
	DWORD dwPID1;
	DWORD dwPID2;
	DWORD dwMapIndex;
} TPacketWeddingReady;

typedef struct
{
	DWORD dwPID1;
	DWORD dwPID2;
} TPacketWeddingStart;

typedef struct
{
	DWORD dwPID1;
	DWORD dwPID2;
} TPacketWeddingEnd;

/// 개인상점 가격정보의 헤더. 가변 패킷으로 이 뒤에 byCount 만큼의 TItemPriceInfo 가 온다.
typedef struct SPacketMyshopPricelistHeader
{
	DWORD dwOwnerID; ///< 가격정보를 가진 플레이어 ID
	BYTE byCount; ///< 가격정보 갯수
} TPacketMyshopPricelistHeader;

/// 개인상점의 단일 아이템에 대한 가격정보
typedef struct SItemPriceInfo
{
	DWORD dwVnum; ///< 아이템 vnum
	DWORD dwPrice; ///< 가격
#if defined(__CHEQUE_SYSTEM__)
	DWORD dwCheque;
#endif
} TItemPriceInfo;

/// 개인상점 아이템 가격정보 리스트 테이블
typedef struct SItemPriceListTable
{
	DWORD dwOwnerID; ///< 가격정보를 가진 플레이어 ID
	BYTE byCount; ///< 가격정보 리스트의 갯수

	TItemPriceInfo aPriceInfo[SHOP_PRICELIST_MAX_NUM]; ///< 가격정보 리스트
} TItemPriceListTable;

typedef struct
{
	char szName[CHARACTER_NAME_MAX_LEN + 1];
	long lDuration;
} TPacketBlockChat;

// PCBANG_IP_LIST
typedef struct SPacketPCBangIP
{
	DWORD id;
	DWORD ip;
} TPacketPCBangIP;
// END_OF_PCBANG_IP_LIST

// ADMIN_MANAGER
typedef struct TAdminInfo
{
	int m_ID; // 고유ID
	char m_szAccount[32]; // 계정
	char m_szName[32]; // 캐릭터이름
	char m_szContactIP[16]; // 접근아이피
	char m_szServerIP[16]; // 서버아이피
	int m_Authority; // 권한
} tAdminInfo;
// END_ADMIN_MANAGER

// BOOT_LOCALIZATION
struct tLocale
{
	char szValue[32];
	char szKey[32];
};
// BOOT_LOCALIZATION

// RELOAD_ADMIN
typedef struct SPacketReloadAdmin
{
	char szIP[16];
} TPacketReloadAdmin;
// END_RELOAD_ADMIN

typedef struct TMonarchInfo
{
	DWORD pid[4]; // 군주의 PID
	int64_t money[4]; // 군주의 별개 돈
	char name[4][32]; // 군주의 이름
	char date[4][32]; // 군주 등록 날짜
} MonarchInfo;

typedef struct TMonarchElectionInfo
{
	DWORD pid; // 투표 한사람 PID
	DWORD selectedpid; // 투표 당한 PID ( 군주 참가자 )
	char date[32]; // 투표 날짜
} MonarchElectionInfo;

// 군주 출마자
typedef struct tMonarchCandidacy
{
	DWORD pid;
	char name[32];
	char date[32];
} MonarchCandidacy;

typedef struct tChangeMonarchLord
{
	BYTE bEmpire;
	DWORD dwPID;
} TPacketChangeMonarchLord;

typedef struct tChangeMonarchLordACK
{
	BYTE bEmpire;
	DWORD dwPID;
	char szName[32];
	char szDate[32];
} TPacketChangeMonarchLordACK;

// Block Country Ip
typedef struct tBlockCountryIp
{
	DWORD ip_from;
	DWORD ip_to;
} TPacketBlockCountryIp;

enum EBlockExceptionCommand
{
	BLOCK_EXCEPTION_CMD_ADD = 1,
	BLOCK_EXCEPTION_CMD_DEL = 2,
};

// Block Exception Account
typedef struct tBlockException
{
	BYTE cmd; // 1 == add, 2 == delete
	char login[LOGIN_MAX_LEN + 1];
}TPacketBlockException;

typedef struct tChangeGuildMaster
{
	DWORD dwGuildID;
	DWORD idFrom;
	DWORD idTo;
} TPacketChangeGuildMaster;

typedef struct tItemIDRange
{
	DWORD dwMin;
	DWORD dwMax;
	DWORD dwUsableItemIDMin;
} TItemIDRangeTable;

typedef struct tUpdateHorseName
{
	DWORD dwPlayerID;
	char szHorseName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketUpdateHorseName;

typedef struct tDC
{
	char login[LOGIN_MAX_LEN + 1];
} TPacketDC;

typedef struct tNeedLoginLogInfo
{
	DWORD dwPlayerID;
} TPacketNeedLoginLogInfo;

// 독일 선물 알림 기능 테스트용 패킷 정보
typedef struct tItemAwardInformer
{
	char login[LOGIN_MAX_LEN + 1];
	char command[20]; // 명령어
	unsigned int vnum; // 아이템
} TPacketItemAwardInfromer;

// 선물 알림 기능 삭제용 패킷 정보
typedef struct tDeleteAwardID
{
	DWORD dwID;
} TPacketDeleteAwardID;

typedef struct SChannelStatus
{
	short nPort;
	BYTE bStatus;
} TChannelStatus;

#if defined(__MESSENGER_BLOCK_SYSTEM__)
enum MessengerBlock
{
	MESSENGER_BLOCK,
	MESSENGER_FRIEND
};
#endif

#if defined(__MOVE_CHANNEL__)
typedef struct
{
	long lMapIndex;
	int iChannel;
} TPacketChangeChannel;

typedef struct
{
	long lAddr;
	WORD wPort;
} TPacketReturnChannel;
#endif

#if defined(__MAILBOX__)
enum EMAILBOX
{
	MAILBOX_TAX = 5,
	MAILBOX_REMAIN_DAY = 30,
	MAILBOX_REMAIN_DAY_GM = 7,
	MAILBOX_LEVEL_LIMIT = 20,
	MAILBOX_PRICE_YANG = 1000,
	MAILBOX_PAGE_SIZE = 9,
	MAILBOX_PAGE_COUNT = 10,
	MAILBOX_MAX_MAIL = MAILBOX_PAGE_SIZE * MAILBOX_PAGE_COUNT,
};

typedef struct SMailBoxRespondUnreadData
{
	SMailBoxRespondUnreadData() :
		bHeader(0),
		bItemMessageCount(0),
		bCommonMessageCount(0),
		bGMVisible(false)
	{}
	BYTE bHeader;
	BYTE bItemMessageCount;
	BYTE bCommonMessageCount;
	bool bGMVisible;
} TMailBoxRespondUnreadData;

typedef struct SMailBox
{
	char szName[CHARACTER_NAME_MAX_LEN + 1];
	BYTE Index;
} TMailBox;

typedef struct packet_mailbox_add_data
{
	BYTE bHeader;
	BYTE Index;
	char szFrom[CHARACTER_NAME_MAX_LEN + 1];
	char szMessage[100 + 1];
	int iYang;
	int iWon;
	DWORD dwItemVnum;
	DWORD dwItemCount;
	long alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
#if defined(__CHANGE_LOOK_SYSTEM__)
	DWORD dwChangeLookVnum;
#endif
#if defined(__REFINE_ELEMENT_SYSTEM__)
	TPlayerItemRefineElement RefineElement;
#endif
#if defined(__ITEM_APPLY_RANDOM__)
	TPlayerItemAttribute aApplyRandom[ITEM_APPLY_MAX_NUM];
#endif
#if defined(__SET_ITEM__)
	BYTE bSetValue;
#endif
} TPacketGCMailBoxAddData;

typedef struct packet_mailbox_message
{
	time_t SendTime;
	time_t DeleteTime;
	char szTitle[25 + 1];
	bool bIsGMPost;
	bool bIsItemExist;
	bool bIsConfirm;
} TPacketGCMailBoxMessage;

typedef struct SMailBoxTable
{
	char szName[CHARACTER_NAME_MAX_LEN + 1];
	bool bIsDeleted;
	packet_mailbox_message Message;
	packet_mailbox_add_data AddData;
} TMailBoxTable;
#endif

#if defined(__RANKING_SYSTEM__)
typedef struct SPartyMemberName
{
	char szName[CHARACTER_NAME_MAX_LEN + 1];
} TPartyMember;

typedef struct SRankingData
{
	char szGuildName[GUILD_NAME_MAX_LEN + 1];
	SPartyMemberName Member[PARTY_MAX_MEMBER];
	DWORD dwRecord0, dwRecord1;
	DWORD dwStartTime;
	BYTE bEmpire;
} TRankingData;
#endif

#if defined(__ATTR_6TH_7TH__)
typedef struct SAttr67AddData
{
	SAttr67AddData() : wRegistItemPos(0), byMaterialCount(0), wSupportItemPos(0), bySupportItemCount(0) {}
	WORD wRegistItemPos;
	BYTE byMaterialCount;
	WORD wSupportItemPos;
	BYTE bySupportItemCount;
} TAttr67AddData;
#endif

#if defined(__GEM_SHOP__)
typedef struct SGemShopLoad
{
	DWORD dwPID;
#	if defined(__CONQUEROR_LEVEL__)
	bool bSpecial;
#	endif
} TGemShopLoad;

typedef struct SGemShopItem
{
	bool bEnable;
	DWORD dwItemVnum;
	BYTE bCount;
	DWORD dwPrice;
} TGemShopItem;

typedef struct SGemShopTable
{
	DWORD dwPID;
	long lRefreshTime;
	BYTE bEnabledSlots;
	TGemShopItem GemShopItem[GEM_SHOP_SLOT_COUNT];
#	if defined(__CONQUEROR_LEVEL__)
	bool bSpecial;
#	endif
} TGemShopTable;
#endif

#if defined(__EXPRESSING_EMOTIONS__)
typedef struct SPacketGDEmote
{
	DWORD dwPID;
	DWORD dwVnum;
	DWORD dwDuration;
} TPacketGDEmote;
#endif

#if defined(__GUILD_EVENT_FLAG__) //&& defined(__GUILD_RENEWAL__)
typedef struct SPacketSetGuildEventFlag
{
	DWORD dwGuildID;
	char szFlagName[EVENT_FLAG_NAME_MAX_LEN + 1];
	long lValue;
} TPacketSetGuildEventFlag;
#endif

#pragma pack()
#endif // __INC_COMMON_TABLES_H__
