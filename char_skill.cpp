#include "stdafx.h"
#include <sstream>

#include "utils.h"
#include "config.h"
#include "vector.h"
#include "char.h"
#include "char_manager.h"
#include "battle.h"
#include "desc.h"
#include "desc_manager.h"
#include "packet.h"
#include "affect.h"
#include "item.h"
#include "sectree_manager.h"
#include "mob_manager.h"
#include "start_position.h"
#include "party.h"
#include "buffer_manager.h"
#include "guild.h"
#include "log.h"
#include "unique_item.h"
#include "questmanager.h"
#if defined(__DAWNMIST_DUNGEON__)
#	include "dawnmist_dungeon.h"
#endif
#if defined(__DEFENSE_WAVE__)
#	include "defense_wave.h"
#endif

extern int test_server;

static const DWORD s_adwSubSkillVnums[] =
{
	SKILL_LEADERSHIP,
	SKILL_COMBO,
	SKILL_MINING,
	SKILL_LANGUAGE1,
	SKILL_LANGUAGE2,
	SKILL_LANGUAGE3,
	SKILL_POLYMORPH,
	SKILL_HORSE,
	SKILL_HORSE_SUMMON,
	SKILL_HORSE_WILDATTACK,
	SKILL_HORSE_CHARGE,
	SKILL_HORSE_ESCAPE,
	SKILL_HORSE_WILDATTACK_RANGE,
	SKILL_ADD_HP,
	SKILL_RESIST_PENETRATE,
#if defined(__PARTY_PROFICY__)
	SKILL_ROLE_PROFICIENCY,
#endif
#if defined(__PARTY_INSIGHT__)
	SKILL_INSIGHT,
#endif
	SKILL_HIT,
};

struct FPartyPIDCollector
{
	std::vector<DWORD> vecPIDs;
	FPartyPIDCollector() = default;
	void operator () (LPCHARACTER ch)
	{
		vecPIDs.push_back(ch->GetPlayerID());
	}
};

struct FPartyMOBCollector
{
	std::vector<DWORD> vecPIDs;
	FPartyMOBCollector() = default;
	void operator () (LPCHARACTER ch)
	{
		vecPIDs.push_back(ch->GetVID());
	}
};

time_t CHARACTER::GetSkillNextReadTime(DWORD dwVnum) const
{
	if (dwVnum >= SKILL_MAX_NUM)
	{
		sys_err("vnum overflow (vnum: %u)", dwVnum);
		return 0;
	}

	return m_pSkillLevels ? m_pSkillLevels[dwVnum].tNextRead : 0;
}

void CHARACTER::SetSkillNextReadTime(DWORD dwVnum, time_t time)
{
	if (m_pSkillLevels && dwVnum < SKILL_MAX_NUM)
		m_pSkillLevels[dwVnum].tNextRead = time;
}

bool TSkillUseInfo::HitOnce(DWORD dwVnum)
{
	// 쓰지도않았으면 때리지도 못한다.
	if (!bUsed)
		return false;

	sys_log(1, "__HitOnce NextUse %u current %u count %d scount %d", dwNextSkillUsableTime, get_dword_time(), iHitCount, iSplashCount);

	if (dwNextSkillUsableTime && dwNextSkillUsableTime < get_dword_time()
		&& dwVnum != SKILL_MUYEONG
		&& dwVnum != SKILL_HORSE_WILDATTACK 
#if defined(__PVP_BALANCE_IMPROVING__)
		&& dwVnum != SKILL_GYEONGGONG
#endif
		)
	{
		sys_log(1, "__HitOnce can't hit");

		return false;
	}

	if (iHitCount == -1)
	{
		sys_log(1, "__HitOnce OK %d %d %d", dwNextSkillUsableTime, get_dword_time(), iHitCount);
		return true;
	}

	if (iHitCount)
	{
		sys_log(1, "__HitOnce OK %d %d %d", dwNextSkillUsableTime, get_dword_time(), iHitCount);
		iHitCount--;
		return true;
	}
	return false;
}

bool TSkillUseInfo::UseSkill(bool isGrandMaster, DWORD vid, DWORD dwCooltime, int splashcount, int hitcount, int range)
{
	this->isGrandMaster = isGrandMaster;
	DWORD dwCur = get_dword_time();

	// 아직 쿨타임이 끝나지 않았다.
	if (bUsed && dwNextSkillUsableTime > dwCur)
	{
		sys_log(0, "cooltime is not over delta %u", dwNextSkillUsableTime - dwCur);
		iHitCount = 0;
		return false;
	}

	bUsed = true;

	if (dwCooltime)
		dwNextSkillUsableTime = dwCur + dwCooltime;
	else
		dwNextSkillUsableTime = 0;

	iRange = range;
	iMaxHitCount = iHitCount = hitcount;

	if (test_server)
		sys_log(0, "UseSkill NextUse %u current %u cooltime %d hitcount %d/%d", dwNextSkillUsableTime, dwCur, dwCooltime, iHitCount, iMaxHitCount);

	dwVID = vid;
	iSplashCount = splashcount;
	return true;
}

int CHARACTER::GetChainLightningMaxCount() const
{
	return aiChainLightningCountBySkillLevel[MIN(SKILL_MAX_LEVEL, GetSkillLevel(SKILL_CHAIN))];
}

void CHARACTER::SetAffectedEunhyung()
{
	m_dwAffectedEunhyungLevel = GetSkillPower(SKILL_EUNHYUNG);
}

void CHARACTER::SetSkillGroup(BYTE bSkillGroup)
{
	if (bSkillGroup > 2)
		return;

	if (GetLevel() < 5)
		return;

	m_points.bSkillGroup = bSkillGroup;

	TPacketGCChangeSkillGroup p;
	p.header = HEADER_GC_SKILL_GROUP;
	p.skill_group = m_points.bSkillGroup;

	GetDesc()->Packet(&p, sizeof(TPacketGCChangeSkillGroup));
}

int CHARACTER::ComputeCooltime(int time)
{
	return CalculateDuration(GetPoint(POINT_CASTING_SPEED), time);
}

void CHARACTER::SkillLevelPacket()
{
	if (!GetDesc())
		return;

	TPacketGCSkillLevel pack;

	pack.bHeader = HEADER_GC_SKILL_LEVEL;
	thecore_memcpy(&pack.skills, m_pSkillLevels, sizeof(TPlayerSkill) * SKILL_MAX_NUM);
	GetDesc()->Packet(&pack, sizeof(TPacketGCSkillLevel));
}

void CHARACTER::SetSkillLevel(DWORD dwVnum, BYTE bLev)
{
	if (NULL == m_pSkillLevels)
		return;

	if (dwVnum >= SKILL_MAX_NUM)
	{
		sys_err("vnum overflow (vnum %u)", dwVnum);
		return;
	}

	m_pSkillLevels[dwVnum].bLevel = MIN(SKILL_MAX_LEVEL, bLev);

	if (bLev >= 40)
		m_pSkillLevels[dwVnum].bMasterType = SKILL_PERFECT_MASTER;
	else if (bLev >= 30)
		m_pSkillLevels[dwVnum].bMasterType = SKILL_GRAND_MASTER;
	else if (bLev >= 20)
		m_pSkillLevels[dwVnum].bMasterType = SKILL_MASTER;
	else
		m_pSkillLevels[dwVnum].bMasterType = SKILL_NORMAL;
}

bool CHARACTER::IsLearnableSkill(DWORD dwSkillVnum) const
{
	const CSkillProto* pkSkill = CSkillManager::instance().Get(dwSkillVnum);

	if (!pkSkill)
		return false;

	if (GetSkillLevel(dwSkillVnum) >= SKILL_MAX_LEVEL)
		return false;

	if (pkSkill->dwType == SKILL_BOOK_TYPE_SUPPORT)
	{
		if (GetSkillLevel(dwSkillVnum) >= pkSkill->bMaxLevel)
			return false;

		return true;
	}

	if (pkSkill->dwType == SKILL_BOOK_TYPE_HORSE)
	{
		if (dwSkillVnum == SKILL_HORSE_WILDATTACK_RANGE && GetJob() != JOB_ASSASSIN)
			return false;

		return true;
	}

	if (GetSkillGroup() == 0)
		return false;

	if (pkSkill->dwType - 1 == GetJob())
		return true;

	if (SKILL_BOOK_TYPE_WOLFMAN == pkSkill->dwType && JOB_WOLFMAN == GetJob())
		return true;

	if (SKILL_BOOK_TYPE_PASSIVE == pkSkill->dwType)
	{
		if (SKILL_7_A_ANTI_TANHWAN <= dwSkillVnum && dwSkillVnum <= SKILL_7_D_ANTI_YONGBI)
		{
			for (int i = 0; i < 4; i++)
			{
				if (unsigned(SKILL_7_A_ANTI_TANHWAN + i) != dwSkillVnum)
				{
					if (0 != GetSkillLevel(SKILL_7_A_ANTI_TANHWAN + i))
					{
						return false;
					}
				}
			}

			return true;
		}

		if (SKILL_8_A_ANTI_GIGONGCHAM <= dwSkillVnum && dwSkillVnum <= SKILL_8_D_ANTI_BYEURAK)
		{
			for (int i = 0; i < 4; i++)
			{
				if (unsigned(SKILL_8_A_ANTI_GIGONGCHAM + i) != dwSkillVnum)
				{
					if (0 != GetSkillLevel(SKILL_8_A_ANTI_GIGONGCHAM + i))
						return false;
				}
			}

			return true;
		}

#if defined(__7AND8TH_SKILLS__)
		if (dwSkillVnum >= SKILL_ANTI_PALBANG && dwSkillVnum <= SKILL_HELP_SALPOONG)
		{
			if (GetSkillLevel(dwSkillVnum) != 0)
				return true;
		}
#endif
	}

	return false;
}

// ADD_GRANDMASTER_SKILL
bool CHARACTER::LearnGrandMasterSkill(DWORD dwSkillVnum)
{
	const CSkillProto* pkSk = CSkillManager::instance().Get(dwSkillVnum);

	if (!pkSk)
		return false;

	if (!IsLearnableSkill(dwSkillVnum))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_STRING("수련할 수 없는 스킬입니다."));
		return false;
	}

	sys_log(0, "learn grand master skill[%d] cur %d, next %d", dwSkillVnum, get_global_time(), GetSkillNextReadTime(dwSkillVnum));

	/*
	if (get_global_time() < GetSkillNextReadTime(dwSkillVnum))
	{
		if (!(test_server && quest::CQuestManager::instance().GetEventFlag("no_read_delay")))
		{
			if (FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
			{
				// 주안술서 사용중에는 시간 제한 무시
				RemoveAffect(AFFECT_SKILL_NO_BOOK_DELAY);
				ChatPacket(CHAT_TYPE_INFO, LC_STRING("주안술서를 통해 주화입마에서 빠져나왔습니다."));
			}
			else
			{
				SkillLearnWaitMoreTimeMessage(GetSkillNextReadTime(dwSkillVnum) - get_global_time());
				return false;
			}
		}
	}
	*/

	// bType 이 0이면 처음부터 책으로 수련 가능
	if (pkSk->dwType == SKILL_BOOK_TYPE_SUPPORT)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_STRING("그랜드 마스터 수련을 할 수 없는 스킬입니다."));
		return false;
	}

	if (GetSkillMasterType(dwSkillVnum) != SKILL_GRAND_MASTER)
	{
		if (GetSkillMasterType(dwSkillVnum) > SKILL_GRAND_MASTER)
			ChatPacket(CHAT_TYPE_INFO, LC_STRING("퍼펙트 마스터된 스킬입니다. 더 이상 수련 할 수 없습니다."));
		else
			ChatPacket(CHAT_TYPE_INFO, LC_STRING("이 스킬은 아직 그랜드 마스터 수련을 할 경지에 이르지 않았습니다."));
		return false;
	}

	std::string strTrainSkill;
	{
		std::ostringstream os;
		os << "training_grandmaster_skill.skill" << dwSkillVnum;
		strTrainSkill = os.str();
	}

	// 여기서 확률을 계산합니다.
	BYTE bLastLevel = GetSkillLevel(dwSkillVnum);

	int idx = MIN(9, GetSkillLevel(dwSkillVnum) - 30);

	sys_log(0, "LearnGrandMasterSkill %s table idx %d value %d", GetName(), idx, aiGrandMasterSkillBookCountForLevelUp[idx]);

	int iTotalReadCount = GetQuestFlag(strTrainSkill) + 1;
	SetQuestFlag(strTrainSkill, iTotalReadCount);

	int iMinReadCount = aiGrandMasterSkillBookMinCount[idx];
	int iMaxReadCount = aiGrandMasterSkillBookMaxCount[idx];

	int iBookCount = aiGrandMasterSkillBookCountForLevelUp[idx];

	if (LC_IsYMIR() == true || LC_IsKorea() == true)
	{
		const int aiGrandMasterSkillBookCountForLevelUp_euckr[10] =
		{
			3, 3, 4, 5, 6, 7, 8, 9, 10, 15,
		};

		const int aiGrandMasterSkillBookMinCount_euckr[10] =
		{
			1, 1, 1, 2, 2, 2, 3, 3, 4, 5
		};

		const int aiGrandMasterSkillBookMaxCount_euckr[10] =
		{
			5, 7, 9, 11, 13, 15, 18, 23, 25, 30
		};

		iMinReadCount = aiGrandMasterSkillBookMinCount_euckr[idx];
		iMaxReadCount = aiGrandMasterSkillBookMaxCount_euckr[idx];
		iBookCount = aiGrandMasterSkillBookCountForLevelUp_euckr[idx];
	}

	if (FindAffect(AFFECT_SKILL_BOOK_BONUS))
	{
		if (iBookCount & 1)
			iBookCount = iBookCount / 2 + 1;
		else
			iBookCount = iBookCount / 2;

		RemoveAffect(AFFECT_SKILL_BOOK_BONUS);
	}

	int n = number(1, iBookCount);
	sys_log(0, "Number(%d)", n);

	DWORD nextTime = get_global_time() + number(28800, 43200);

	sys_log(0, "GrandMaster SkillBookCount min %d cur %d max %d (next_time=%d)", iMinReadCount, iTotalReadCount, iMaxReadCount, nextTime);

	bool bSuccess = n == 2;

	if (iTotalReadCount < iMinReadCount)
		bSuccess = false;
	if (iTotalReadCount > iMaxReadCount)
		bSuccess = true;

	if (bSuccess)
	{
		SkillLevelUp(dwSkillVnum, SKILL_UP_BY_QUEST);
	}

	SetSkillNextReadTime(dwSkillVnum, nextTime);

	if (bLastLevel == GetSkillLevel(dwSkillVnum))
	{
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("크윽, 기가 역류하고 있어! 이거 설마 주화입마인가!? 젠장!"));
		ChatPacket(CHAT_TYPE_INFO, LC_STRING("수련이 실패로 끝났습니다. 다시 도전해주시기 바랍니다."));
		LogManager::instance().CharLog(this, dwSkillVnum, "GM_READ_FAIL", "");
		return false;
	}

	ChatPacket(CHAT_TYPE_TALKING, LC_STRING("몸에서 뭔가 힘이 터져 나오는 기분이야!"));
	ChatPacket(CHAT_TYPE_TALKING, LC_STRING("뜨거운 무엇이 계속 용솟음치고 있어! 이건, 이것은!"));
	ChatPacket(CHAT_TYPE_INFO, LC_STRING("더 높은 경지의 수련을 성공적으로 끝내셨습니다."));
	LogManager::instance().CharLog(this, dwSkillVnum, "GM_READ_SUCCESS", "");
	return true;
}
// END_OF_ADD_GRANDMASTER_SKILL

static bool FN_should_check_exp(LPCHARACTER ch)
{
	if (LC_IsCanada())
		return ch->GetLevel() < gPlayerMaxLevel;

	if (!LC_IsYMIR())
		return true;

	return false;
}

bool CHARACTER::LearnSkillByBook(DWORD dwSkillVnum, BYTE bProb)
{
	const CSkillProto* pkSk = CSkillManager::instance().Get(dwSkillVnum);
	if (pkSk == nullptr)
		return false;

	if (!IsLearnableSkill(dwSkillVnum))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_STRING("수련할 수 없는 스킬입니다."));
		return false;
	}

#if defined(__CONQUEROR_LEVEL__)
	const bool bConquerorSkill = IsConquerorSkill(dwSkillVnum);
	if (bConquerorSkill && GetConquerorLevel() <= 0)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_STRING("You can learn this after reaching the Champion Level."));
		return false;
	}
#endif

	POINT_VALUE need_exp = 0;

	if (FN_should_check_exp(this))
	{
		need_exp = 20000;

#if defined(__CONQUEROR_LEVEL__)
		if (bConquerorSkill && GetConquerorExp() < need_exp)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_STRING("경험치가 부족하여 책을 읽을 수 없습니다."));
			return false;
		}
		else
		{
			if (GetExp() < need_exp)
			{
				ChatPacket(CHAT_TYPE_INFO, LC_STRING("경험치가 부족하여 책을 읽을 수 없습니다."));
				return false;
			}
		}
#else
		if (GetExp() < need_exp)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_STRING("경험치가 부족하여 책을 읽을 수 없습니다."));
			return false;
		}
#endif
	}

	if (pkSk->dwType != SKILL_BOOK_TYPE_SUPPORT)
	{
		if (GetSkillMasterType(dwSkillVnum) != SKILL_MASTER)
		{
			if (GetSkillMasterType(dwSkillVnum) > SKILL_MASTER)
			{
				ChatPacket(CHAT_TYPE_INFO, LC_STRING("이 스킬은 책으로 더이상 수련할 수 없습니다."));
				return false;
			}
			else
			{
				if (pkSk->dwType != SKILL_BOOK_TYPE_PASSIVE
#if defined(__CONQUEROR_LEVEL__)
					&& !bConquerorSkill
#endif
					)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_STRING("이 스킬은 아직 책으로 수련할 경지에 이르지 않았습니다."));
					return false;
				}
			}
		}
	}

	if (get_global_time() < GetSkillNextReadTime(dwSkillVnum))
	{
		if (!(test_server && quest::CQuestManager::instance().GetEventFlag("no_read_delay")))
		{
			if (FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
			{
				// 주안술서 사용중에는 시간 제한 무시
				RemoveAffect(AFFECT_SKILL_NO_BOOK_DELAY);
				ChatPacket(CHAT_TYPE_INFO, LC_STRING("주안술서를 통해 주화입마에서 빠져나왔습니다."));
			}
			else
			{
				SkillLearnWaitMoreTimeMessage(GetSkillNextReadTime(dwSkillVnum) - get_global_time());
				return false;
			}
		}
	}

	// 여기서 확률을 계산합니다.
	BYTE bLastLevel = GetSkillLevel(dwSkillVnum);

	if (bProb != 0)
	{
		// SKILL_BOOK_BONUS
		if (FindAffect(AFFECT_SKILL_BOOK_BONUS))
		{
			bProb += bProb / 2;
			RemoveAffect(AFFECT_SKILL_BOOK_BONUS);
		}
		// END_OF_SKILL_BOOK_BONUS

		sys_log(0, "LearnSkillByBook Pct %u prob %d", dwSkillVnum, bProb);

		if (number(1, 100) <= bProb)
		{
			if (test_server)
				sys_log(0, "LearnSkillByBook %u SUCC", dwSkillVnum);

			SkillLevelUp(dwSkillVnum, SKILL_UP_BY_BOOK);
		}
		else
		{
			if (test_server)
				sys_log(0, "LearnSkillByBook %u FAIL", dwSkillVnum);
		}
	}
	else
	{
		int idx = MIN(9, GetSkillLevel(dwSkillVnum) - 20);

		sys_log(0, "LearnSkillByBook %s table idx %d value %d", GetName(), idx, aiSkillBookCountForLevelUp[idx]);

		if (!LC_IsYMIR())
		{
			int need_bookcount = GetSkillLevel(dwSkillVnum) - 20;

#if defined(__CONQUEROR_LEVEL__)
			PointChange(bConquerorSkill ? POINT_CONQUEROR_EXP : POINT_EXP, -need_exp);
#else
			PointChange(POINT_EXP, -need_exp);
#endif

			quest::CQuestManager& q = quest::CQuestManager::instance();
			quest::PC* pPC = q.GetPC(GetPlayerID());

			if (pPC)
			{
				char flag[128 + 1];
				memset(flag, 0, sizeof(flag));
				snprintf(flag, sizeof(flag), "traning_master_skill.%u.read_count", dwSkillVnum);

				int read_count = pPC->GetFlag(flag);
				int percent = 65;

				if (FindAffect(AFFECT_SKILL_BOOK_BONUS))
				{
					percent = 0;
					RemoveAffect(AFFECT_SKILL_BOOK_BONUS);
				}

				if (number(1, 100) > percent)
				{
					// 책읽기에 성공
					if (read_count >= need_bookcount)
					{
						SkillLevelUp(dwSkillVnum, SKILL_UP_BY_BOOK);
						pPC->SetFlag(flag, 0);

						ChatPacket(CHAT_TYPE_INFO, LC_STRING("책으로 더 높은 경지의 수련을 성공적으로 끝내셨습니다."));
						LogManager::instance().CharLog(this, dwSkillVnum, "READ_SUCCESS", "");
						return true;
					}
					else
					{
						pPC->SetFlag(flag, read_count + 1);

						switch (number(1, 3))
						{
							case 1:
								ChatPacket(CHAT_TYPE_TALKING, LC_STRING("어느정도 이 기술에 대해 이해가 되었지만 조금 부족한듯 한데.."));
								break;

							case 2:
								ChatPacket(CHAT_TYPE_TALKING, LC_STRING("드디어 끝이 보이는 건가...  이 기술은 이해하기가 너무 힘들어.."));
								break;

							case 3:
							default:
								ChatPacket(CHAT_TYPE_TALKING, LC_STRING("열심히 하는 배움을 가지는 것만이 기술을 배울수 있는 유일한 길이다.."));
								break;
						}

						ChatPacket(CHAT_TYPE_INFO, LC_STRING("%d 권을 더 읽어야 수련을 완료 할 수 있습니다.", need_bookcount - read_count));
						return true;
					}
				}
			}
			else
			{
				// 사용자의 퀘스트 정보 로드 실패
			}
		}
		// INTERNATIONAL_VERSION
		else
		{
			int iBookCount = 99;

			if (LC_IsYMIR() == true)
			{
				const int aiSkillBookCountForLevelUp_euckr[10] =
				{
					2, 2, 3, 3, 3, 3, 3, 3, 4, 5
				};

				iBookCount = aiSkillBookCountForLevelUp_euckr[idx];
			}
			else
				iBookCount = aiSkillBookCountForLevelUp[idx];

			if (FindAffect(AFFECT_SKILL_BOOK_BONUS))
			{
				if (iBookCount & 1) // iBookCount % 2
					iBookCount = iBookCount / 2 + 1;
				else
					iBookCount = iBookCount / 2;

				RemoveAffect(AFFECT_SKILL_BOOK_BONUS);
			}

			if (number(1, iBookCount) == 2)
				SkillLevelUp(dwSkillVnum, SKILL_UP_BY_BOOK);
		}
		// END_OF_INTERNATIONAL_VERSION
	}

	if (bLastLevel != GetSkillLevel(dwSkillVnum))
	{
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("몸에서 뭔가 힘이 터져 나오는 기분이야!"));
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("뜨거운 무엇이 계속 용솟음치고 있어! 이건, 이것은!"));
		ChatPacket(CHAT_TYPE_INFO, LC_STRING("책으로 더 높은 경지의 수련을 성공적으로 끝내셨습니다."));
		LogManager::instance().CharLog(this, dwSkillVnum, "READ_SUCCESS", "");
	}
	else
	{
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("크윽, 기가 역류하고 있어! 이거 설마 주화입마인가!? 젠장!"));
		ChatPacket(CHAT_TYPE_INFO, LC_STRING("수련이 실패로 끝났습니다. 다시 도전해주시기 바랍니다."));
		LogManager::instance().CharLog(this, dwSkillVnum, "READ_FAIL", "");
	}

	return true;
}

bool CHARACTER::SkillLevelDown(DWORD dwVnum)
{
	if (NULL == m_pSkillLevels)
		return false;

	if (g_bSkillDisable)
		return false;

	if (IsPolymorphed())
		return false;

	CSkillProto* pkSk = CSkillManager::instance().Get(dwVnum);

	if (!pkSk)
	{
		sys_err("There is no such skill by number %u", dwVnum);
		return false;
	}

	if (!IsLearnableSkill(dwVnum))
		return false;

	if (GetSkillMasterType(pkSk->dwVnum) != SKILL_NORMAL)
		return false;

	if (!GetSkillGroup())
		return false;

	if (pkSk->dwVnum >= SKILL_MAX_NUM)
		return false;

	if (m_pSkillLevels[pkSk->dwVnum].bLevel == 0)
		return false;

	int idx = POINT_SKILL;
	switch (pkSk->dwType)
	{
		case 0:
			idx = POINT_SUB_SKILL;
			break;
		case 1:
		case 2:
		case 3:
		case 4:
		case 6:
		case 7:
			idx = POINT_SKILL;
			break;
		case 5:
			idx = POINT_HORSE_SKILL;
			break;
		default:
			sys_err("Wrong skill type %d skill vnum %d", pkSk->dwType, pkSk->dwVnum);
			return false;
	}

	PointChange(idx, +1);
	SetSkillLevel(pkSk->dwVnum, m_pSkillLevels[pkSk->dwVnum].bLevel - 1);

	sys_log(0, "SkillDown: %s %u %u %u type %u", GetName(), pkSk->dwVnum, m_pSkillLevels[pkSk->dwVnum].bMasterType, m_pSkillLevels[pkSk->dwVnum].bLevel, pkSk->dwType);
	Save();

	ComputePoints();
	SkillLevelPacket();
	return true;
}

void CHARACTER::SkillLevelUp(DWORD dwVnum, BYTE bMethod)
{
	if (NULL == m_pSkillLevels)
		return;

	if (g_bSkillDisable)
		return;

	if (IsPolymorphed())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_STRING("둔갑 중에는 능력을 올릴 수 없습니다."));
		return;
	}

	if (SKILL_7_A_ANTI_TANHWAN <= dwVnum && dwVnum <= SKILL_8_D_ANTI_BYEURAK)
	{
		if (0 == GetSkillLevel(dwVnum))
			return;
	}

	const CSkillProto* pkSk = CSkillManager::instance().Get(dwVnum);

	if (!pkSk)
	{
		sys_err("There is no such skill by number (vnum %u)", dwVnum);
		return;
	}

	if (pkSk->dwVnum >= SKILL_MAX_NUM)
	{
		sys_err("Skill Vnum overflow (vnum %u)", dwVnum);
		return;
	}

	if (!IsLearnableSkill(dwVnum))
		return;

	// 그랜드 마스터는 퀘스트로만 수행가능
	if (pkSk->dwType != SKILL_BOOK_TYPE_SUPPORT && pkSk->dwType != SKILL_BOOK_TYPE_PASSIVE)
	{
		switch (GetSkillMasterType(pkSk->dwVnum))
		{
			case SKILL_GRAND_MASTER:
				if (bMethod != SKILL_UP_BY_QUEST)
					return;
				break;

			case SKILL_PERFECT_MASTER:
				return;
		}
	}

	if (bMethod == SKILL_UP_BY_POINT)
	{
		// 마스터가 아닌 상태에서만 수련가능
		if (GetSkillMasterType(pkSk->dwVnum) != SKILL_NORMAL)
			return;

		if (IS_SET(pkSk->dwFlag, SKILL_FLAG_DISABLE_BY_POINT_UP))
			return;
	}
	else if (bMethod == SKILL_UP_BY_BOOK)
	{
		// 직업에 속하지 않았거나 포인트로 올릴수 없는 스킬은 처음부터 책으로 배울 수 있다.
		if (pkSk->dwType != SKILL_BOOK_TYPE_SUPPORT && pkSk->dwType != SKILL_BOOK_TYPE_PASSIVE)
			if (GetSkillMasterType(pkSk->dwVnum) != SKILL_MASTER
#if defined(__CONQUEROR_LEVEL__)
				&& !IsConquerorSkill(pkSk->dwVnum)
#endif
				)
				return;
	}

	if (GetLevel() < pkSk->bLevelLimit)
		return;

	if (pkSk->preSkillVnum)
	{
		if (GetSkillMasterType(pkSk->preSkillVnum) == SKILL_NORMAL &&
			GetSkillLevel(pkSk->preSkillVnum) < pkSk->preSkillLevel)
			return;
	}

	if (bMethod == SKILL_UP_BY_POINT)
	{
		int idx;

		switch (pkSk->dwType)
		{
			case 0:
				idx = POINT_SUB_SKILL;
				break;

			case 1:
			case 2:
			case 3:
			case 4:
			case 6:
			case 7:
				idx = POINT_SKILL;
				break;

			case 5:
				idx = POINT_HORSE_SKILL;
				break;

			default:
				sys_err("Wrong skill type %d skill vnum %d", pkSk->dwType, pkSk->dwVnum);
				return;
		}

		if (GetPoint(idx) < 1)
			return;

		PointChange(idx, -1);
	}

	int SkillPointBefore = GetSkillLevel(pkSk->dwVnum);
	SetSkillLevel(pkSk->dwVnum, m_pSkillLevels[pkSk->dwVnum].bLevel + 1);

	if (pkSk->dwType != 0)
	{
		// 갑자기 그레이드 업하는 코딩
		switch (GetSkillMasterType(pkSk->dwVnum))
		{
			case SKILL_NORMAL:
				// 번섭은 스킬 업그레이드 17~20 사이 랜덤 마스터 수련
				if (GetSkillLevel(pkSk->dwVnum) >= 17)
				{
					if (GetQuestFlag("reset_scroll.force_to_master_skill") > 0)
					{
						SetSkillLevel(pkSk->dwVnum, 20);
						SetQuestFlag("reset_scroll.force_to_master_skill", 0);
					}
					else
					{
						if (number(1, 21 - MIN(20, GetSkillLevel(pkSk->dwVnum))) == 1)
							SetSkillLevel(pkSk->dwVnum, 20);
					}
				}
				break;

			case SKILL_MASTER:
				if (GetSkillLevel(pkSk->dwVnum) >= 30)
				{
					if (number(1, 31 - MIN(30, GetSkillLevel(pkSk->dwVnum))) == 1)
						SetSkillLevel(pkSk->dwVnum, 30);
				}
				break;

			case SKILL_GRAND_MASTER:
				if (GetSkillLevel(pkSk->dwVnum) >= 40)
				{
					SetSkillLevel(pkSk->dwVnum, 40);
				}
				break;
		}
	}

	char szSkillUp[1024];

	snprintf(szSkillUp, sizeof(szSkillUp), "SkillUp: %s %u %d %d[Before:%d] type %u",
		GetName(), pkSk->dwVnum, m_pSkillLevels[pkSk->dwVnum].bMasterType, m_pSkillLevels[pkSk->dwVnum].bLevel, SkillPointBefore, pkSk->dwType);

	sys_log(0, "%s", szSkillUp);

	LogManager::instance().CharLog(this, pkSk->dwVnum, "SKILLUP", szSkillUp);
	Save();

	ComputePoints();
	SkillLevelPacket();
}

void CHARACTER::ComputeSkillPoints()
{
	if (g_bSkillDisable)
		return;

	PointChange(POINT_HIT_PCT, aiPrecisionPowerByLevel[MINMAX(0, GetSkillLevel(SKILL_HIT), SKILL_MAX_LEVEL)]);
}

void CHARACTER::ResetSkill()
{
	if (NULL == m_pSkillLevels)
		return;

	// 보조 스킬은 리셋시키지 않는다
	std::vector<std::pair<DWORD, TPlayerSkill>> vec;
	size_t count = sizeof(s_adwSubSkillVnums) / sizeof(s_adwSubSkillVnums[0]);

	for (size_t i = 0; i < count; ++i)
	{
		if (s_adwSubSkillVnums[i] >= SKILL_MAX_NUM)
			continue;

		vec.push_back(std::make_pair(s_adwSubSkillVnums[i], m_pSkillLevels[s_adwSubSkillVnums[i]]));
	}

	memset(m_pSkillLevels, 0, sizeof(TPlayerSkill) * SKILL_MAX_NUM);

	std::vector<std::pair<DWORD, TPlayerSkill>>::const_iterator iter = vec.begin();

	while (iter != vec.end())
	{
		const std::pair<DWORD, TPlayerSkill>& pair = *(iter++);
		m_pSkillLevels[pair.first] = pair.second;
	}

	ComputePoints();
	SkillLevelPacket();
}

void CHARACTER::ComputePassiveSkill(DWORD dwVnum)
{
	if (g_bSkillDisable)
		return;

	if (GetSkillLevel(dwVnum) == 0)
		return;

	CSkillProto* pkSk = CSkillManager::instance().Get(dwVnum);
	pkSk->SetPointVar("k", GetSkillLevel(dwVnum));
	int iAmount = (int)pkSk->kPointPoly.Eval();

	sys_log(2, "%s passive #%d on %d amount %d", GetName(), dwVnum, pkSk->wPointOn, iAmount);
	PointChange(pkSk->wPointOn, iAmount);
}

struct FFindNearVictim
{
	FFindNearVictim(LPCHARACTER center, LPCHARACTER attacker, const CHARACTER_SET& excepts_set = empty_set_)
		: m_pkChrCenter(center),
		m_pkChrNextTarget(NULL),
		m_pkChrAttacker(attacker),
		m_count(0),
		m_excepts_set(excepts_set)
	{
	}

	void operator ()(LPENTITY ent)
	{
		if (!ent->IsType(ENTITY_CHARACTER))
			return;

		LPCHARACTER pkChr = (LPCHARACTER)ent;

		if (!m_excepts_set.empty())
		{
			if (m_excepts_set.find(pkChr) != m_excepts_set.end())
				return;
		}

		if (m_pkChrCenter == pkChr)
			return;

		if (!battle_is_attackable(m_pkChrAttacker, pkChr))
		{
			return;
		}

		if (abs(m_pkChrCenter->GetX() - pkChr->GetX()) > 1000 || abs(m_pkChrCenter->GetY() - pkChr->GetY()) > 1000)
			return;

		float fDist = DISTANCE_APPROX(m_pkChrCenter->GetX() - pkChr->GetX(), m_pkChrCenter->GetY() - pkChr->GetY());

		if (fDist < 1000)
		{
			++m_count;

			if ((m_count == 1) || number(1, m_count) == 1)
				m_pkChrNextTarget = pkChr;
		}
	}

	LPCHARACTER GetVictim()
	{
		return m_pkChrNextTarget;
	}

	LPCHARACTER m_pkChrCenter;
	LPCHARACTER m_pkChrNextTarget;
	LPCHARACTER m_pkChrAttacker;
	int m_count;
	const CHARACTER_SET& m_excepts_set;

private:
	static CHARACTER_SET empty_set_;
};

CHARACTER_SET FFindNearVictim::empty_set_;

EVENTINFO(chain_lightning_event_info)
{
	DWORD dwVictim;
	DWORD dwChr;

	chain_lightning_event_info()
		: dwVictim(0)
		, dwChr(0)
	{
	}
};

EVENTFUNC(ChainLightningEvent)
{
	chain_lightning_event_info* info = dynamic_cast<chain_lightning_event_info*>(event->info);

	LPCHARACTER pkChrVictim = CHARACTER_MANAGER::instance().Find(info->dwVictim);
	LPCHARACTER pkChr = CHARACTER_MANAGER::instance().Find(info->dwChr);
	LPCHARACTER pkTarget = NULL;

	if (!pkChr || !pkChrVictim)
	{
		sys_log(1, "use chainlighting, but no character");
		return 0;
	}

	sys_log(1, "chainlighting event %s", pkChr->GetName());

	if (pkChrVictim->GetParty()) // 파티 먼저
	{
		pkTarget = pkChrVictim->GetParty()->GetNextOwnership(NULL, pkChrVictim->GetX(), pkChrVictim->GetY());
		if (pkTarget == pkChrVictim || !number(0, 2) || pkChr->GetChainLightingExcept().find(pkTarget) != pkChr->GetChainLightingExcept().end())
			pkTarget = NULL;
	}

	if (!pkTarget)
	{
		// 1. Find Next victim
		FFindNearVictim f(pkChrVictim, pkChr, pkChr->GetChainLightingExcept());

		if (pkChrVictim->GetSectree())
		{
			pkChrVictim->GetSectree()->ForEachAround(f);
			// 2. If exist, compute it again
			pkTarget = f.GetVictim();
		}
	}

	if (pkTarget)
	{
		pkChrVictim->CreateFly(FLY_CHAIN_LIGHTNING, pkTarget);
		pkChr->ComputeSkill(SKILL_CHAIN, pkTarget);
		pkChr->AddChainLightningExcept(pkTarget);
	}
	else
	{
		sys_log(1, "%s use chainlighting, but find victim failed near %s", pkChr->GetName(), pkChrVictim->GetName());
	}

	return 0;
}

void SetPolyVarForAttack(LPCHARACTER ch, CSkillProto* pkSk, LPITEM pkWeapon)
{
	if (ch->IsPC())
	{
		if (pkWeapon && pkWeapon->GetType() == ITEM_WEAPON)
		{
			int iWep = number(pkWeapon->GetValue(3), pkWeapon->GetValue(4));
			iWep += pkWeapon->GetValue(5);

			int iMtk = number(pkWeapon->GetValue(1), pkWeapon->GetValue(2));
			iMtk += pkWeapon->GetValue(5);

#if defined(__ACCE_COSTUME_SYSTEM__)
			iWep += ch->GetAcceWeaponAttack();
			iMtk += ch->GetAcceWeaponMagicAttack();
#endif

			pkSk->SetPointVar("wep", iWep);
			pkSk->SetPointVar("mtk", iMtk);
			pkSk->SetPointVar("mwep", iMtk);
		}
		else
		{
			pkSk->SetPointVar("wep", 0);
			pkSk->SetPointVar("mtk", 0);
			pkSk->SetPointVar("mwep", 0);
		}
	}
	else
	{
		int iWep = number(ch->GetMobDamageMin(), ch->GetMobDamageMax());
		pkSk->SetPointVar("wep", iWep);
		pkSk->SetPointVar("mwep", iWep);
		pkSk->SetPointVar("mtk", iWep);
	}
}

struct FuncSplashDamage
{
	FuncSplashDamage(int x, int y, CSkillProto* pkSk, LPCHARACTER pkChr, int iAmount, int iAG, int iMaxHit, LPITEM pkWeapon, bool bDisableCooltime, TSkillUseInfo* pInfo, BYTE bUseSkillPower)
		:
		m_x(x), m_y(y), m_pkSk(pkSk), m_pkChr(pkChr), m_iAmount(iAmount), m_iAG(iAG), m_iCount(0), m_iMaxHit(iMaxHit), m_pkWeapon(pkWeapon), m_bDisableCooltime(bDisableCooltime), m_pInfo(pInfo), m_bUseSkillPower(bUseSkillPower)
	{
	}

	void operator () (LPENTITY ent)
	{
		if (!ent->IsType(ENTITY_CHARACTER))
		{
			//if (m_pkSk->dwVnum == SKILL_CHAIN) sys_log(0, "CHAIN target not character %s", m_pkChr->GetName());
			return;
		}

		LPCHARACTER pkChrVictim = dynamic_cast<LPCHARACTER>(ent);

		if (DISTANCE_APPROX(m_x - pkChrVictim->GetX(), m_y - pkChrVictim->GetY()) > m_pkSk->iSplashRange)
		{
			if (test_server)
				sys_log(0, "XXX target too far %s", m_pkChr->GetName());
			return;
		}

		if (!battle_is_attackable(m_pkChr, pkChrVictim))
		{
			if (test_server)
				sys_log(0, "XXX target not attackable %s", m_pkChr->GetName());
			return;
		}

		if (m_pkChr->IsPC())
			// 길드 스킬은 쿨타임 처리를 하지 않는다.
			if (!(m_pkSk->dwVnum >= GUILD_SKILL_START && m_pkSk->dwVnum <= GUILD_SKILL_END))
				if (!m_bDisableCooltime && m_pInfo && !m_pInfo->HitOnce(m_pkSk->dwVnum) && m_pkSk->dwVnum != SKILL_MUYEONG
#if defined(__PVP_BALANCE_IMPROVING__)
					&& m_pkSk->dwVnum != SKILL_GYEONGGONG
#endif
					)
				{
					if (test_server)
						sys_log(0, "check guild skill %s", m_pkChr->GetName());
					return;
				}

		++m_iCount;

		int iDam;

		////////////////////////////////////////////////////////////////////////////////
		//float k = 1.0f * m_pkChr->GetSkillPower(m_pkSk->dwVnum) * m_pkSk->bMaxLevel / 100;
		//m_pkSk->kPointPoly2.SetVar("k", 1.0 * m_bUseSkillPower * m_pkSk->bMaxLevel / 100);
		m_pkSk->SetPointVar("k", 1.0 * m_bUseSkillPower * m_pkSk->bMaxLevel / 100);
		m_pkSk->SetPointVar("lv", m_pkChr->GetLevel());
		m_pkSk->SetPointVar("iq", m_pkChr->GetPoint(POINT_IQ));
		m_pkSk->SetPointVar("str", m_pkChr->GetPoint(POINT_ST));
		m_pkSk->SetPointVar("dex", m_pkChr->GetPoint(POINT_DX));
		m_pkSk->SetPointVar("con", m_pkChr->GetPoint(POINT_HT));
		m_pkSk->SetPointVar("def", m_pkChr->GetPoint(POINT_DEF_GRADE));
		m_pkSk->SetPointVar("odef", m_pkChr->GetPoint(POINT_DEF_GRADE) - m_pkChr->GetPoint(POINT_DEF_GRADE_BONUS));
		m_pkSk->SetPointVar("horse_level", m_pkChr->GetHorseLevel());

		//int iPenetratePct = (int)(1 + k * 4);
		bool bIgnoreDefense = false;

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_PENETRATE))
		{
			int iPenetratePct = (int)m_pkSk->kPointPoly2.Eval();

			if (number(1, 100) <= iPenetratePct)
				bIgnoreDefense = true;
		}

		bool bIgnoreTargetRating = false;

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_IGNORE_TARGET_RATING))
		{
			int iPct = (int)m_pkSk->kPointPoly2.Eval();

			if (number(1, 100) <= iPct)
				bIgnoreTargetRating = true;
		}

		m_pkSk->SetPointVar("ar", CalcAttackRating(m_pkChr, pkChrVictim, bIgnoreTargetRating));

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_USE_MELEE_DAMAGE))
			m_pkSk->SetPointVar("atk", CalcMeleeDamage(m_pkChr, pkChrVictim, true, bIgnoreTargetRating));
		else if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_USE_ARROW_DAMAGE))
		{
			LPITEM pkBow, pkArrow;

			if (1 == m_pkChr->GetArrowAndBow(&pkBow, &pkArrow, 1))
				m_pkSk->SetPointVar("atk", CalcArrowDamage(m_pkChr, pkChrVictim, pkBow, pkArrow, true));
			else
				m_pkSk->SetPointVar("atk", 0);
		}

		if (m_pkSk->wPointOn == POINT_MOV_SPEED)
			m_pkSk->kPointPoly.SetVar("maxv", pkChrVictim->GetLimitPoint(POINT_MOV_SPEED));

		m_pkSk->SetPointVar("maxhp", pkChrVictim->GetMaxHP());
		m_pkSk->SetPointVar("maxsp", pkChrVictim->GetMaxSP());

		m_pkSk->SetPointVar("chain", m_pkChr->GetChainLightningIndex());
		m_pkChr->IncChainLightningIndex();

		bool bUnderEunhyung = m_pkChr->GetAffectedEunhyung() > 0 ? true : false; // 이건 왜 여기서 하지??

		m_pkSk->SetPointVar("ek", m_pkChr->GetAffectedEunhyung() * 1. / 100);
		//m_pkChr->ClearAffectedEunhyung();
		SetPolyVarForAttack(m_pkChr, m_pkSk, m_pkWeapon);

		int iAmount = 0;

		if (m_pkChr->GetUsedSkillMasterType(m_pkSk->dwVnum) >= SKILL_GRAND_MASTER)
		{
			iAmount = (int)m_pkSk->kMasterBonusPoly.Eval();
		}
		else
		{
			iAmount = (int)m_pkSk->kPointPoly.Eval();
		}

		if (test_server && iAmount == 0 && m_pkSk->wPointOn != POINT_NONE)
		{
			m_pkChr->ChatPacket(CHAT_TYPE_INFO, "효과가 없습니다. 스킬 공식을 확인하세요");
		}
		////////////////////////////////////////////////////////////////////////////////
		iAmount = -iAmount;

		if (m_pkSk->dwVnum == SKILL_AMSEOP)
		{
			float fDelta = GetDegreeDelta(m_pkChr->GetRotation(), pkChrVictim->GetRotation());
			float adjust;

			if (fDelta < 35.0f)
			{
				adjust = 1.5f;

				if (bUnderEunhyung)
					adjust += 0.5f;

				if (m_pkChr->GetWear(WEAR_WEAPON) && m_pkChr->GetWear(WEAR_WEAPON)->GetSubType() == WEAPON_DAGGER)
				{
					//if (!g_iUseLocale)
					if (LC_IsYMIR())
						adjust += 1.0f;
					else
						adjust += 0.5f;
				}
			}
			else
			{
				adjust = 1.0f;

				if (!LC_IsYMIR())
				{
					if (bUnderEunhyung)
						adjust += 0.5f;

					if (m_pkChr->GetWear(WEAR_WEAPON) && m_pkChr->GetWear(WEAR_WEAPON)->GetSubType() == WEAPON_DAGGER)
						adjust += 0.5f;
				}
			}

			iAmount = (int)(iAmount * adjust);
		}
		else if (m_pkSk->dwVnum == SKILL_GUNGSIN)
		{
			float adjust = 1.0;

			if (m_pkChr->GetWear(WEAR_WEAPON) && m_pkChr->GetWear(WEAR_WEAPON)->GetSubType() == WEAPON_DAGGER)
			{
				//if (!g_iUseLocale)
				if (LC_IsYMIR())
					adjust = 1.4f;
				else
					adjust = 1.35f;
			}

			iAmount = (int)(iAmount * adjust);
		}
		else if (m_pkSk->dwVnum == SKILL_CHAYEOL)
		{
			if (number(1, 100) <= 20)
			{
				float adjust = 1.0f;
				if (test_server)
					m_pkChr->ChatPacket(CHAT_TYPE_PARTY, "CRITICAL SKILL VNUM %d ATTACK : NORMAL DMG %d, MULTIPLIED DMG %d, ADJUSTMENT (%.2f%%) FINAL %d",
						m_pkSk->dwVnum,
						iAmount,
						(iAmount * 2),
						adjust,
						(int)((iAmount * 2) * adjust)
					);

				m_pkChr->EffectPacket(SE_CRITICAL);
				iAmount *= 2;
				iAmount = (int)(iAmount * adjust);
			}
		}
		else if (m_pkSk->dwVnum == SKILL_GONGDAB)
		{
			float adjust = 1.0;

			if (m_pkChr->GetWear(WEAR_WEAPON) && m_pkChr->GetWear(WEAR_WEAPON)->GetSubType() == WEAPON_CLAW)
			{
				adjust = 1.35f;
			}

			iAmount = (int)(iAmount * adjust);

			if (number(1, 100) <= 20)
			{
				adjust = 1.0f;
				if (test_server)
					m_pkChr->ChatPacket(CHAT_TYPE_PARTY, "CRITICAL SKILL VNUM %d ATTACK : NORMAL DMG %d, MULTIPLIED DMG %d, ADJUSTMENT (%.2f%%) FINAL %d",
						m_pkSk->dwVnum,
						iAmount,
						(iAmount * 2),
						adjust,
						(int)((iAmount * 2) * adjust)
					);

				m_pkChr->EffectPacket(SE_CRITICAL);
				iAmount *= 2;
				iAmount = (int)(iAmount * adjust);
			}
		}
#if defined(__PVP_BALANCE_IMPROVING__)
		else if (m_pkSk->dwVnum == SKILL_GYEONGGONG)
			iAmount = m_iAmount;
#endif

		////////////////////////////////////////////////////////////////////////////////
		//sys_log(0, "name: %s skill: %s amount %d to %s", m_pkChr->GetName(), m_pkSk->szName, iAmount, pkChrVictim->GetName());

		iDam = CalcBattleDamage(iAmount, m_pkChr->GetLevel(), pkChrVictim->GetLevel());

		if (m_pkChr->IsPC() && m_pkChr->m_SkillUseInfo[m_pkSk->dwVnum].GetMainTargetVID() != (DWORD)pkChrVictim->GetVID())
		{
			// 데미지 감소
			iDam = (int)(iDam * m_pkSk->kSplashAroundDamageAdjustPoly.Eval());
		}

		// TODO 스킬에 따른 데미지 타입 기록해야한다.
		EDamageType dt = DAMAGE_TYPE_NONE;

		switch (m_pkSk->bSkillAttrType)
		{
			case SKILL_ATTR_TYPE_NORMAL:
				break;

			case SKILL_ATTR_TYPE_MELEE:
			{
				dt = DAMAGE_TYPE_MELEE;

				LPITEM pkWeapon = m_pkChr->GetWear(WEAR_WEAPON);

				if (pkWeapon)
					switch (pkWeapon->GetSubType())
					{
						case WEAPON_SWORD:
							iDam = iDam * (100 - pkChrVictim->GetPoint(POINT_RESIST_SWORD)) / 100;
							break;

						case WEAPON_TWO_HANDED:
							iDam = iDam * (100 - pkChrVictim->GetPoint(POINT_RESIST_TWOHAND)) / 100;
							// 양손검 페널티 10%
							//iDam = iDam * 95 / 100;
							break;

						case WEAPON_DAGGER:
							iDam = iDam * (100 - pkChrVictim->GetPoint(POINT_RESIST_DAGGER)) / 100;
							break;

						case WEAPON_BELL:
							iDam = iDam * (100 - pkChrVictim->GetPoint(POINT_RESIST_BELL)) / 100;
							break;

						case WEAPON_FAN:
							iDam = iDam * (100 - pkChrVictim->GetPoint(POINT_RESIST_FAN)) / 100;
							break;

						case WEAPON_CLAW:
							iDam = iDam * (100 - pkChrVictim->GetPoint(POINT_RESIST_CLAW)) / 100;
							break;

					}

				if (!bIgnoreDefense)
					iDam -= pkChrVictim->GetPoint(POINT_DEF_GRADE);
			}
			break;

			case SKILL_ATTR_TYPE_RANGE:
				dt = DAMAGE_TYPE_RANGE;
				// 으아아아악
				// 예전에 적용안했던 버그가 있어서 방어력 계산을 다시하면 유저가 난리남
				//iDam -= pkChrVictim->GetPoint(POINT_DEF_GRADE);
				iDam = iDam * (100 - pkChrVictim->GetPoint(POINT_RESIST_BOW)) / 100;
				break;

			case SKILL_ATTR_TYPE_MAGIC:
				dt = DAMAGE_TYPE_MAGIC;
				iDam = CalcAttBonus(m_pkChr, pkChrVictim, iDam);
				// 으아아아악
				// 예전에 적용안했던 버그가 있어서 방어력 계산을 다시하면 유저가 난리남
				//iDam -= pkChrVictim->GetPoint(POINT_MAGIC_DEF_GRADE);
#if defined(__MAGIC_REDUCTION__)
				{
					const int c_iResMagic = MINMAX(0, pkChrVictim->GetPoint(POINT_RESIST_MAGIC), 100);
					const int c_iResMagicReduction = MINMAX(0, (m_pkChr->GetJob() == JOB_SURA) ? m_pkChr->GetPoint(POINT_RESIST_MAGIC_REDUCTION) / 2 : m_pkChr->GetPoint(POINT_RESIST_MAGIC_REDUCTION), 50);
					const int c_iTotalMagicRes = MINMAX(0, c_iResMagic - c_iResMagicReduction, 100);
					iDam = iDam * (100 - c_iTotalMagicRes) / 100;
				}
#else
				iDam = iDam * (100 - pkChrVictim->GetPoint(POINT_RESIST_MAGIC)) / 100;
#endif
				break;

			default:
				sys_err("Unknown skill attr type %u vnum %u", m_pkSk->bSkillAttrType, m_pkSk->dwVnum);
				break;
		}

		//
		// 20091109 독일 스킬 속성 요청 작업
		// 기존 스킬 테이블에 SKILL_FLAG_WIND, SKILL_FLAG_ELEC, SKILL_FLAG_FIRE를 가진 스킬이
		// 전혀 없었으므로 몬스터의 RESIST_WIND, RESIST_ELEC, RESIST_FIRE도 사용되지 않고 있었다.
		//
		// PvP와 PvE밸런스 분리를 위해 의도적으로 NPC만 적용하도록 했으며 기존 밸런스와 차이점을
		// 느끼지 못하기 위해 mob_proto의 RESIST_MAGIC을 RESIST_WIND, RESIST_ELEC, RESIST_FIRE로
		// 복사하였다.
		//
		if (pkChrVictim->IsNPC())
		{
			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_WIND))
			{
				iDam = iDam * (100 - pkChrVictim->GetPoint(POINT_RESIST_WIND)) / 100;
			}

			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_ELEC))
			{
				iDam = iDam * (100 - pkChrVictim->GetPoint(POINT_RESIST_ELEC)) / 100;
			}

			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_FIRE))
			{
				iDam = iDam * (100 - pkChrVictim->GetPoint(POINT_RESIST_FIRE)) / 100;
			}
		}

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_COMPUTE_MAGIC_DAMAGE))
			dt = DAMAGE_TYPE_MAGIC;

		if (pkChrVictim->CanBeginFight())
			pkChrVictim->BeginFight(m_pkChr);

		if (m_pkSk->dwVnum == SKILL_CHAIN)
			sys_log(0, "%s CHAIN INDEX %d DAM %d DT %d", m_pkChr->GetName(), m_pkChr->GetChainLightningIndex() - 1, iDam, dt);

#if defined(__7AND8TH_SKILLS__)
		{
			BYTE HELP_SKILL_ID = 0;
			switch (m_pkSk->dwVnum)
			{
				case SKILL_PALBANG:
					HELP_SKILL_ID = SKILL_HELP_PALBANG;
					break;
				case SKILL_AMSEOP:
					HELP_SKILL_ID = SKILL_HELP_AMSEOP;
					break;
				case SKILL_SWAERYUNG:
					HELP_SKILL_ID = SKILL_HELP_SWAERYUNG;
					break;
				case SKILL_YONGBI:
					HELP_SKILL_ID = SKILL_HELP_YONGBI;
					break;
				case SKILL_GIGONGCHAM:
					HELP_SKILL_ID = SKILL_HELP_GIGONGCHAM;
					break;
				case SKILL_HWAJO:
					HELP_SKILL_ID = SKILL_HELP_HWAJO;
					break;
				case SKILL_MARYUNG:
					HELP_SKILL_ID = SKILL_HELP_MARYUNG;
					break;
				case SKILL_BYEURAK:
					HELP_SKILL_ID = SKILL_HELP_BYEURAK;
					break;
				case SKILL_SALPOONG:
					HELP_SKILL_ID = SKILL_HELP_SALPOONG;
					break;
				default:
					break;
			}

			if (HELP_SKILL_ID != 0)
			{
				BYTE HELP_SKILL_LV = m_pkChr->GetSkillLevel(HELP_SKILL_ID);
				if (HELP_SKILL_LV != 0)
				{
					CSkillProto* pkSk = CSkillManager::instance().Get(HELP_SKILL_ID);
					if (!pkSk)
						sys_err("Can't find %d skill in skill_proto.", HELP_SKILL_ID);
					else
					{
						pkSk->SetPointVar("k", 1.0f * m_pkChr->GetSkillPower(HELP_SKILL_ID) * pkSk->bMaxLevel / 100);

						double IncreaseAmount = pkSk->kPointPoly.Eval();
						sys_log(0, "HELP_SKILL: increase amount: %lf, normal damage: %d, increased damage: %d.", IncreaseAmount, iDam, int(iDam * (IncreaseAmount / 100.0)));
						iDam += iDam * (IncreaseAmount / 100.0);
					}
				}
			}
		}

		{
			BYTE ANTI_SKILL_ID = 0;
			switch (m_pkSk->dwVnum)
			{
				case SKILL_PALBANG:
					ANTI_SKILL_ID = SKILL_ANTI_PALBANG;
					break;
				case SKILL_AMSEOP:
					ANTI_SKILL_ID = SKILL_ANTI_AMSEOP;
					break;
				case SKILL_SWAERYUNG:
					ANTI_SKILL_ID = SKILL_ANTI_SWAERYUNG;
					break;
				case SKILL_YONGBI:
					ANTI_SKILL_ID = SKILL_ANTI_YONGBI;
					break;
				case SKILL_GIGONGCHAM:
					ANTI_SKILL_ID = SKILL_ANTI_GIGONGCHAM;
					break;
				case SKILL_HWAJO:
					ANTI_SKILL_ID = SKILL_ANTI_HWAJO;
					break;
				case SKILL_MARYUNG:
					ANTI_SKILL_ID = SKILL_ANTI_MARYUNG;
					break;
				case SKILL_BYEURAK:
					ANTI_SKILL_ID = SKILL_ANTI_BYEURAK;
					break;
				case SKILL_SALPOONG:
					ANTI_SKILL_ID = SKILL_ANTI_SALPOONG;
					break;
				default:
					break;
			}

			if (ANTI_SKILL_ID != 0)
			{
				BYTE ANTI_SKILL_LV = pkChrVictim->GetSkillLevel(ANTI_SKILL_ID);
				if (ANTI_SKILL_LV != 0)
				{
					CSkillProto* pkSk = CSkillManager::instance().Get(ANTI_SKILL_ID);
					if (!pkSk)
						sys_err("Can't find %d skill in skill_proto.", ANTI_SKILL_ID);
					else
					{
						pkSk->SetPointVar("k", 1.0f * pkChrVictim->GetSkillPower(ANTI_SKILL_ID) * pkSk->bMaxLevel / 100);

						double ResistAmount = pkSk->kPointPoly.Eval();
						sys_log(0, "ANTI_SKILL: resist amount: %lf, normal damage: %d, reduced damage: %d.", ResistAmount, iDam, int(iDam * (ResistAmount / 100.0)));
						iDam -= iDam * (ResistAmount / 100.0);
					}
				}
			}
		}
#endif

		{
			BYTE AntiSkillID = 0;

			switch (m_pkSk->dwVnum)
			{
				case SKILL_TANHWAN: AntiSkillID = SKILL_7_A_ANTI_TANHWAN; break;
				case SKILL_AMSEOP: AntiSkillID = SKILL_7_B_ANTI_AMSEOP; break;
				case SKILL_SWAERYUNG: AntiSkillID = SKILL_7_C_ANTI_SWAERYUNG; break;
				case SKILL_YONGBI: AntiSkillID = SKILL_7_D_ANTI_YONGBI; break;
				case SKILL_GIGONGCHAM: AntiSkillID = SKILL_8_A_ANTI_GIGONGCHAM; break;
				case SKILL_YEONSA: AntiSkillID = SKILL_8_B_ANTI_YEONSA; break;
				case SKILL_MAHWAN: AntiSkillID = SKILL_8_C_ANTI_MAHWAN; break;
				case SKILL_BYEURAK: AntiSkillID = SKILL_8_D_ANTI_BYEURAK; break;
			}

			if (0 != AntiSkillID)
			{
				BYTE AntiSkillLevel = pkChrVictim->GetSkillLevel(AntiSkillID);

				if (0 != AntiSkillLevel)
				{
					CSkillProto* pkSk = CSkillManager::instance().Get(AntiSkillID);
					if (!pkSk)
					{
						sys_err("There is no anti skill(%d) in skill proto", AntiSkillID);
					}
					else
					{
						pkSk->SetPointVar("k", 1.0f * pkChrVictim->GetSkillPower(AntiSkillID) * pkSk->bMaxLevel / 100);

						double ResistAmount = pkSk->kPointPoly.Eval();

						sys_log(0, "ANTI_SKILL: Resist(%lf) Orig(%d) Reduce(%d)", ResistAmount, iDam, int(iDam * (ResistAmount / 100.0)));

						iDam -= iDam * (ResistAmount / 100.0);
					}
				}
			}
		}

		if (!pkChrVictim->Damage(m_pkChr, iDam, dt) && !pkChrVictim->IsStun())
		{
#if defined(__PVP_BALANCE_IMPROVING__)
			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_KNOCKBACK))
			{
				float fKnockbackLength = 300; // Knockback distance.

				if (pkChrVictim->IsStone())
					fKnockbackLength = 0;

#if defined(__DEFENSE_WAVE__)
				if (CDefenseWaveManager::Instance().IsHydraSpawn(pkChrVictim->GetRaceNum()))
					fKnockbackLength = 0;

				if (CDefenseWaveManager::Instance().IsHydra(pkChrVictim->GetRaceNum()))
					fKnockbackLength = 0;
#endif

				float fx, fy;
				float degree = GetDegreeFromPositionXY(m_pkChr->GetX(), m_pkChr->GetY(), pkChrVictim->GetX(), pkChrVictim->GetY());

				if (m_pkSk->dwVnum == SKILL_HORSE_WILDATTACK)
				{
					degree -= m_pkChr->GetRotation();
					degree = fmod(degree, 360.0f) - 180.0f;

					if (degree > 0)
						degree = m_pkChr->GetRotation() + 90.0f;
					else
						degree = m_pkChr->GetRotation() - 90.0f;
				}

				if (fKnockbackLength > 0)
				{
					GetDeltaByDegree(degree, fKnockbackLength, &fx, &fy);
					sys_log(0, "KNOCKBACK! %s -> %s (%d %d) -> (%d %d)",
						m_pkChr->GetName(),
						pkChrVictim->GetName(),
						pkChrVictim->GetX(),
						pkChrVictim->GetY(),
						(long)(pkChrVictim->GetX() + fx),
						(long)(pkChrVictim->GetY() + fy)
					);

					long tx = (long)(pkChrVictim->GetX() + fx);
					long ty = (long)(pkChrVictim->GetY() + fy);

					pkChrVictim->Sync(tx, ty);
					pkChrVictim->Goto(tx, ty);
					pkChrVictim->CalculateMoveDuration();

					pkChrVictim->SyncPacket();
				}
			}
#endif

			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_REMOVE_GOOD_AFFECT))
			{
				int iAmount2 = (int)m_pkSk->kPointPoly2.Eval();
				int iDur2 = (int)m_pkSk->kDurationPoly2.Eval();
				iDur2 += m_pkChr->GetPoint(POINT_PARTY_BUFFER_BONUS);

				int chance = iAmount2;

				if (m_pkChr && m_pkChr->GetLevel() < pkChrVictim->GetLevel())
				{
					int delta = pkChrVictim->GetLevel() - m_pkChr->GetLevel();
					if (delta >= 15)
						chance = 0;
				}

				if (number(1, 100) <= chance)
				{
					pkChrVictim->RemoveGoodAffect();
					pkChrVictim->AddAffect(m_pkSk->dwVnum, POINT_NONE, 0, AFF_PABEOP, iDur2, 0, true);
				}
			}

			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_SLOW | SKILL_FLAG_STUN | SKILL_FLAG_FIRE_CONT | SKILL_FLAG_POISON | SKILL_FLAG_BLEEDING))
			{
				int iPct = (int)m_pkSk->kPointPoly2.Eval();
				int iDur = (int)m_pkSk->kDurationPoly2.Eval();

#if defined(__PVP_BALANCE_IMPROVING__)
				const int iMaxStunDuration = 20;
				if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_STUN))
					iDur += MINMAX(static_cast<int>(m_pkSk->kDurationPoly2.Eval()), m_pkChr->GetPoint(POINT_PARTY_BUFFER_BONUS), iMaxStunDuration);
				else
					iDur += m_pkChr->GetPoint(POINT_PARTY_BUFFER_BONUS);
#else
				iDur += m_pkChr->GetPoint(POINT_PARTY_BUFFER_BONUS);
#endif

				if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_STUN))
				{
					SkillAttackAffect(pkChrVictim, iPct, IMMUNE_STUN, AFFECT_STUN, POINT_NONE, 0, AFF_STUN, iDur, m_pkSk->szName);
				}
				else if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_SLOW))
				{
					SkillAttackAffect(pkChrVictim, iPct, IMMUNE_SLOW, AFFECT_SLOW, POINT_MOV_SPEED, -30, AFF_SLOW, iDur, m_pkSk->szName);
				}
				else if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_FIRE_CONT))
				{
					m_pkSk->SetDurationVar("k", 1.0 * m_bUseSkillPower * m_pkSk->bMaxLevel / 100);
					m_pkSk->SetDurationVar("iq", m_pkChr->GetPoint(POINT_IQ));

					iDur = (int)m_pkSk->kDurationPoly2.Eval();
					int bonus = m_pkChr->GetPoint(POINT_PARTY_BUFFER_BONUS);

					if (bonus != 0)
					{
						iDur += bonus / 2;
					}

					if (number(1, 100) <= iDur)
					{
						pkChrVictim->AttackedByFire(m_pkChr, iPct, 5);
					}
				}
				else if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_POISON))
				{
#if defined(__CONQUEROR_LEVEL__)
					if (m_pkChr->IsSungMaCursed(POINT_SUNGMA_IMMUNE))
						iPct /= 2;
#endif

					if (number(1, 100) <= iPct)
						pkChrVictim->AttackedByPoison(m_pkChr);
				}
			}

			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_BLEEDING))
			{
				// NOTE : Wolfman `SKILL_CHAYEOL` (170) `SKILL_FLAG_BLEEDING` uses the
				// `kDurationPoly2` column as the percentage value for bleeding chance.
				int iPct = (int)m_pkSk->kDurationPoly2.Eval();
#if defined(__CONQUEROR_LEVEL__)
				if (m_pkChr->IsSungMaCursed(POINT_SUNGMA_IMMUNE))
					iPct /= 2;
#endif

				if (number(1, 100) <= iPct)
					pkChrVictim->AttackedByBleeding(m_pkChr);
			}

			if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_CRUSH | SKILL_FLAG_CRUSH_LONG) && !pkChrVictim->IsNoMove())
			{
				float fCrushSlidingLength = 200;

				if (m_pkChr->IsNPC())
					fCrushSlidingLength = 400;

				if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_CRUSH_LONG))
					fCrushSlidingLength *= 2;

				if (g_bDisableBossKnockback)
					if (m_pkChr->GetMobRank() > MOB_RANK_S_KNIGHT)
						fCrushSlidingLength = 0;

				float fx, fy;
				float degree = GetDegreeFromPositionXY(m_pkChr->GetX(), m_pkChr->GetY(), pkChrVictim->GetX(), pkChrVictim->GetY());

				if (m_pkSk->dwVnum == SKILL_HORSE_WILDATTACK)
				{
					degree -= m_pkChr->GetRotation();
					degree = fmod(degree, 360.0f) - 180.0f;

					if (degree > 0)
						degree = m_pkChr->GetRotation() + 90.0f;
					else
						degree = m_pkChr->GetRotation() - 90.0f;
				}

				GetDeltaByDegree(degree, fCrushSlidingLength, &fx, &fy);
				sys_log(0, "CRUSH! %s -> %s (%d %d) -> (%d %d)", m_pkChr->GetName(), pkChrVictim->GetName(),
					pkChrVictim->GetX(), pkChrVictim->GetY(),
					static_cast<long>(pkChrVictim->GetX() + fx), static_cast<long>(pkChrVictim->GetY() + fy));

				long tx = static_cast<long>((pkChrVictim->GetX() + fx));
				long ty = static_cast<long>((pkChrVictim->GetY() + fy));

				// CRUSH_SKILL_WALL_KNOCKBACK_FIX
				while (pkChrVictim->IsInBlockedArea(tx, ty) && fCrushSlidingLength > 0)
				{
					if (fCrushSlidingLength >= 10)
						fCrushSlidingLength -= 10;
					else
						fCrushSlidingLength = 0;

					GetDeltaByDegree(degree, fCrushSlidingLength, &fx, &fy);
					tx = static_cast<long>((pkChrVictim->GetX() + fx));
					ty = static_cast<long>((pkChrVictim->GetY() + fy));
				}
				// END_OF_CRUSH_SKILL_WALL_KNOCKBACK_FIX

				pkChrVictim->Sync(tx, ty);
				pkChrVictim->Goto(tx, ty);
				pkChrVictim->CalculateMoveDuration();

				if (m_pkChr->IsPC() && m_pkChr->m_SkillUseInfo[m_pkSk->dwVnum].GetMainTargetVID() == static_cast<DWORD>(pkChrVictim->GetVID()))
				{
					//if (!g_iUseLocale)
					if (LC_IsYMIR())
						SkillAttackAffect(pkChrVictim, 1000, IMMUNE_STUN, m_pkSk->dwVnum, POINT_NONE, 0, AFF_STUN, 3, m_pkSk->szName);
					else
						SkillAttackAffect(pkChrVictim, 1000, IMMUNE_STUN, m_pkSk->dwVnum, POINT_NONE, 0, AFF_STUN, 4, m_pkSk->szName);
				}
				else
				{
					pkChrVictim->SyncPacket();
				}
			}
		}

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_HP_ABSORB))
		{
			int iPct = static_cast<int>(m_pkSk->kPointPoly2.Eval());
			m_pkChr->PointChange(POINT_HP, iDam * iPct / 100);
		}

		if (IS_SET(m_pkSk->dwFlag, SKILL_FLAG_SP_ABSORB))
		{
			int iPct = static_cast<int>(m_pkSk->kPointPoly2.Eval());
			m_pkChr->PointChange(POINT_SP, iDam * iPct / 100);
		}

		if (m_pkSk->dwVnum == SKILL_CHAIN && m_pkChr->GetChainLightningIndex() < m_pkChr->GetChainLightningMaxCount())
		{
			chain_lightning_event_info* info = AllocEventInfo<chain_lightning_event_info>();

			info->dwVictim = static_cast<DWORD>(pkChrVictim->GetVID());
			info->dwChr = static_cast<DWORD>(m_pkChr->GetVID());

			event_create(ChainLightningEvent, info, passes_per_sec / 5);
		}
		if (test_server)
			sys_log(0, "FuncSplashDamage End :%s ", m_pkChr->GetName());
	}

	int m_x;
	int m_y;
	CSkillProto* m_pkSk;
	LPCHARACTER m_pkChr;
	int m_iAmount;
	int m_iAG;
	int m_iCount;
	int m_iMaxHit;
	LPITEM m_pkWeapon;
	bool m_bDisableCooltime;
	TSkillUseInfo* m_pInfo;
	BYTE m_bUseSkillPower;
};

struct FuncSplashAffect
{
	FuncSplashAffect(LPCHARACTER ch, int x, int y, int iDist, DWORD dwVnum, POINT_TYPE wPointOn, int iAmount, DWORD dwAffectFlag, int iDuration, int iSPCost, bool bOverride, int iMaxHit)
	{
		m_x = x;
		m_y = y;
		m_iDist = iDist;
		m_dwVnum = dwVnum;
		m_wPointOn = wPointOn;
		m_iAmount = iAmount;
		m_dwAffectFlag = dwAffectFlag;
		m_iDuration = iDuration;
		m_iSPCost = iSPCost;
		m_bOverride = bOverride;
		m_pkChrAttacker = ch;
		m_iMaxHit = iMaxHit;
		m_iCount = 0;
	}

	void operator () (LPENTITY ent)
	{
		if (m_iMaxHit && m_iMaxHit <= m_iCount)
			return;

		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER pkChr = (LPCHARACTER)ent;

			if (test_server)
				sys_log(0, "FuncSplashAffect step 1 : name:%s vnum:%d iDur:%d", pkChr->GetName(), m_dwVnum, m_iDuration);

			if (DISTANCE_APPROX(m_x - pkChr->GetX(), m_y - pkChr->GetY()) < m_iDist)
			{
				if (test_server)
					sys_log(0, "FuncSplashAffect step 2 : name:%s vnum:%d iDur:%d", pkChr->GetName(), m_dwVnum, m_iDuration);

				if (m_dwVnum == SKILL_TUSOK)
					if (pkChr->CanBeginFight())
						pkChr->BeginFight(m_pkChrAttacker);

				if (pkChr->IsPC() && m_dwVnum == SKILL_TUSOK)
					pkChr->AddAffect(m_dwVnum, m_wPointOn, m_iAmount, m_dwAffectFlag, m_iDuration / 3, m_iSPCost, m_bOverride);
				else
					pkChr->AddAffect(m_dwVnum, m_wPointOn, m_iAmount, m_dwAffectFlag, m_iDuration, m_iSPCost, m_bOverride);

				m_iCount++;
			}
		}
	}

	LPCHARACTER m_pkChrAttacker;
	int m_x;
	int m_y;
	int m_iDist;
	DWORD m_dwVnum;
	POINT_TYPE m_wPointOn;
	int m_iAmount;
	DWORD m_dwAffectFlag;
	int m_iDuration;
	int m_iSPCost;
	bool m_bOverride;
	int m_iMaxHit;
	int m_iCount;
};

EVENTINFO(skill_gwihwan_info)
{
	DWORD pid;
	BYTE bsklv;

	skill_gwihwan_info()
		: pid(0)
		, bsklv(0)
	{
	}
};

EVENTFUNC(skill_gwihwan_event)
{
	skill_gwihwan_info* info = dynamic_cast<skill_gwihwan_info*>(event->info);

	if (info == NULL)
	{
		sys_err("skill_gwihwan_event> <Factor> Null pointer");
		return 0;
	}

	DWORD pid = info->pid;
	BYTE sklv = info->bsklv;
	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);

	if (!ch)
		return 0;

	int percent = 20 * sklv - 1;

	if (number(1, 100) <= percent)
	{
		PIXEL_POSITION pos;

		// 성공
		if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(ch->GetMapIndex(), ch->GetEmpire(), pos))
		{
			sys_log(1, "Recall: %s %d %d -> %d %d", ch->GetName(), ch->GetX(), ch->GetY(), pos.x, pos.y);
			ch->WarpSet(pos.x, pos.y);
		}
		else
		{
			sys_err("CHARACTER::UseItem : cannot find spawn position (name %s, %d x %d)", ch->GetName(), ch->GetX(), ch->GetY());
			ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
		}
	}
	else
	{
		// 실패
		ch->ChatPacket(CHAT_TYPE_INFO, LC_STRING("귀환에 실패하였습니다."));
	}
	return 0;
}

int CHARACTER::ComputeSkillAtPosition(DWORD dwVnum, const PIXEL_POSITION& posTarget, BYTE bSkillLevel)
{
	if (GetMountVnum())
		return BATTLE_NONE;

	if (IsPolymorphed())
		return BATTLE_NONE;

	if (g_bSkillDisable)
		return BATTLE_NONE;

	CSkillProto* pkSk = CSkillManager::instance().Get(dwVnum);

	if (!pkSk)
		return BATTLE_NONE;

	if (test_server)
	{
		sys_log(0, "ComputeSkillAtPosition %s vnum %d x %d y %d level %d",
			GetName(), dwVnum, posTarget.x, posTarget.y, bSkillLevel);
	}

	// 나에게 쓰는 스킬은 내 위치를 쓴다.
	//if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SELFONLY))
	//	posTarget = GetXYZ();

	// 스플래쉬가 아닌 스킬은 주위이면 이상하다
	if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
		return BATTLE_NONE;

	if (0 == bSkillLevel)
	{
		if ((bSkillLevel = GetSkillLevel(pkSk->dwVnum)) == 0)
		{
			return BATTLE_NONE;
		}
	}

	const float k = 1.0 * GetSkillPower(pkSk->dwVnum, bSkillLevel) * pkSk->bMaxLevel / 100;

	pkSk->SetPointVar("k", k);
	pkSk->kSplashAroundDamageAdjustPoly.SetVar("k", k);

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_MELEE_DAMAGE))
	{
		pkSk->SetPointVar("atk", CalcMeleeDamage(this, this, true, false));
	}
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_MAGIC_DAMAGE))
	{
		pkSk->SetPointVar("atk", CalcMagicDamage(this, this));
	}
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_ARROW_DAMAGE))
	{
		LPITEM pkBow, pkArrow;
		if (1 == GetArrowAndBow(&pkBow, &pkArrow, 1))
		{
			pkSk->SetPointVar("atk", CalcArrowDamage(this, this, pkBow, pkArrow, true));
		}
		else
		{
			pkSk->SetPointVar("atk", 0);
		}
	}

	if (pkSk->wPointOn == POINT_MOV_SPEED)
	{
		pkSk->SetPointVar("maxv", this->GetLimitPoint(POINT_MOV_SPEED));
	}

	pkSk->SetPointVar("lv", GetLevel());
	pkSk->SetPointVar("iq", GetPoint(POINT_IQ));
	pkSk->SetPointVar("str", GetPoint(POINT_ST));
	pkSk->SetPointVar("dex", GetPoint(POINT_DX));
	pkSk->SetPointVar("con", GetPoint(POINT_HT));
	pkSk->SetPointVar("maxhp", this->GetMaxHP());
	pkSk->SetPointVar("maxsp", this->GetMaxSP());
	pkSk->SetPointVar("chain", 0);
	pkSk->SetPointVar("ar", CalcAttackRating(this, this));
	pkSk->SetPointVar("def", GetPoint(POINT_DEF_GRADE));
	pkSk->SetPointVar("odef", GetPoint(POINT_DEF_GRADE) - GetPoint(POINT_DEF_GRADE_BONUS));
	pkSk->SetPointVar("horse_level", GetHorseLevel());

	if (pkSk->bSkillAttrType != SKILL_ATTR_TYPE_NORMAL)
		OnMove(true);

	LPITEM pkWeapon = GetWear(WEAR_WEAPON);

	SetPolyVarForAttack(this, pkSk, pkWeapon);

	pkSk->SetDurationVar("k", k/* bSkillLevel */);

	int iAmount = (int)pkSk->kPointPoly.Eval();
	int iAmount2 = (int)pkSk->kPointPoly2.Eval();

	// ADD_GRANDMASTER_SKILL
	int iAmount3 = (int)pkSk->kPointPoly3.Eval();

	if (GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER)
	{
		/*
		if (iAmount >= 0)
			iAmount += (int)m_pkSk->kMasterBonusPoly.Eval();
		else
			iAmount -= (int)m_pkSk->kMasterBonusPoly.Eval();
		*/
		iAmount = (int)pkSk->kMasterBonusPoly.Eval();
	}

	if (test_server && iAmount == 0 && pkSk->wPointOn != POINT_NONE)
	{
		ChatPacket(CHAT_TYPE_INFO, "효과가 없습니다. 스킬 공식을 확인하세요");
	}

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_REMOVE_BAD_AFFECT))
	{
		if (number(1, 100) <= iAmount2)
		{
			RemoveBadAffect();
		}
	}
	// END_OF_ADD_GRANDMASTER_SKILL

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_ATTACK | SKILL_FLAG_USE_MELEE_DAMAGE | SKILL_FLAG_USE_MAGIC_DAMAGE))
	{
		//
		// 공격 스킬일 경우
		//
		bool bAdded = false;

		if (pkSk->wPointOn == POINT_HP && iAmount < 0)
		{
			int iAG = 0;

			FuncSplashDamage f(posTarget.x, posTarget.y, pkSk, this, iAmount, iAG, pkSk->lMaxHit, pkWeapon, m_bDisableCooltime, IsPC() ? &m_SkillUseInfo[dwVnum] : NULL, GetSkillPower(dwVnum, bSkillLevel));

			if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
			{
				if (GetSectree())
					GetSectree()->ForEachAround(f);
			}
			else
			{
				//if (dwVnum == SKILL_CHAIN) sys_log(0, "CHAIN skill call FuncSplashDamage %s", GetName());
				f(this);
			}
		}
		else
		{
			//if (dwVnum == SKILL_CHAIN) sys_log(0, "CHAIN skill no damage %d %s", iAmount, GetName());
			int iDur = (int)pkSk->kDurationPoly.Eval();

			if (IsPC())
				if (!(dwVnum >= GUILD_SKILL_START && dwVnum <= GUILD_SKILL_END)) // 길드 스킬은 쿨타임 처리를 하지 않는다.
					if (!m_bDisableCooltime && !m_SkillUseInfo[dwVnum].HitOnce(dwVnum) && dwVnum != SKILL_MUYEONG)
					{
						//if (dwVnum == SKILL_CHAIN) sys_log(0, "CHAIN skill cannot hit %s", GetName());
						return BATTLE_NONE;
					}

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					AddAffect(pkSk->dwVnum, pkSk->wPointOn, iAmount, pkSk->dwAffectFlag, iDur, 0, true);
				else
				{
					if (GetSectree())
					{
						FuncSplashAffect f(this, posTarget.x, posTarget.y, pkSk->iSplashRange, pkSk->dwVnum, pkSk->wPointOn, iAmount, pkSk->dwAffectFlag, iDur, 0, true, pkSk->lMaxHit);
						GetSectree()->ForEachAround(f);
					}
				}
				bAdded = true;
			}
		}

		if (pkSk->wPointOn2 != POINT_NONE)
		{
			int iDur = (int)pkSk->kDurationPoly2.Eval();

			sys_log(1, "try second %u %d %d", pkSk->dwVnum, pkSk->wPointOn2, iDur);

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					AddAffect(pkSk->dwVnum, pkSk->wPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur, 0, !bAdded);
				else
				{
					if (GetSectree())
					{
						FuncSplashAffect f(this, posTarget.x, posTarget.y, pkSk->iSplashRange, pkSk->dwVnum, pkSk->wPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur, 0, !bAdded, pkSk->lMaxHit);
						GetSectree()->ForEachAround(f);
					}
				}
				bAdded = true;
			}
			else
			{
				PointChange(pkSk->wPointOn2, iAmount2);
			}
		}

		// ADD_GRANDMASTER_SKILL
		if (GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER && pkSk->wPointOn3 != POINT_NONE)
		{
			int iDur = (int)pkSk->kDurationPoly3.Eval();

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					AddAffect(pkSk->dwVnum, pkSk->wPointOn3, iAmount3, 0 /* pkSk->dwAffectFlag3 */, iDur, 0, !bAdded);
				else
				{
					if (GetSectree())
					{
						FuncSplashAffect f(this, posTarget.x, posTarget.y, pkSk->iSplashRange, pkSk->dwVnum, pkSk->wPointOn3, iAmount3, 0 /* pkSk->dwAffectFlag3 */, iDur, 0, !bAdded, pkSk->lMaxHit);
						GetSectree()->ForEachAround(f);
					}
				}
			}
			else
			{
				PointChange(pkSk->wPointOn3, iAmount3);
			}
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		return BATTLE_DAMAGE;
	}
	else
	{
		bool bAdded = false;
		int iDur = (int)pkSk->kDurationPoly.Eval();

		if (iDur > 0)
		{
			iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);
			// AffectFlag가 없거나, toggle 하는 것이 아니라면..
			pkSk->kDurationSPCostPoly.SetVar("k", k/* bSkillLevel */);

			AddAffect(pkSk->dwVnum,
				pkSk->wPointOn,
				iAmount,
				pkSk->dwAffectFlag,
				iDur,
				(long)pkSk->kDurationSPCostPoly.Eval(),
				!bAdded);

			bAdded = true;
		}
		else
		{
			PointChange(pkSk->wPointOn, iAmount);
		}

		if (pkSk->wPointOn2 != POINT_NONE)
		{
			int iDur = (int)pkSk->kDurationPoly2.Eval();

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);
				AddAffect(pkSk->dwVnum, pkSk->wPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur, 0, !bAdded);
				bAdded = true;
			}
			else
			{
				PointChange(pkSk->wPointOn2, iAmount2);
			}
		}

		// ADD_GRANDMASTER_SKILL
		if (GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER && pkSk->wPointOn3 != POINT_NONE)
		{
			int iDur = (int)pkSk->kDurationPoly3.Eval();

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);
				AddAffect(pkSk->dwVnum, pkSk->wPointOn3, iAmount3, 0 /* pkSk->dwAffectFlag3 */, iDur, 0, !bAdded);
			}
			else
			{
				PointChange(pkSk->wPointOn3, iAmount3);
			}
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		return BATTLE_NONE;
	}
}

struct FComputeSkillParty
{
	FComputeSkillParty(DWORD dwVnum, LPCHARACTER pkAttacker, BYTE bSkillLevel = 0)
		: m_dwVnum(dwVnum), m_pkAttacker(pkAttacker), m_bSkillLevel(bSkillLevel)
	{
	}

	void operator () (LPCHARACTER ch)
	{
		if (ch->IsDead())
			return;

		m_pkAttacker->ComputeSkill(m_dwVnum, ch, m_bSkillLevel);
	}

	DWORD m_dwVnum;
	LPCHARACTER m_pkAttacker;
	BYTE m_bSkillLevel;
};

int CHARACTER::ComputeSkillParty(DWORD dwVnum, LPCHARACTER pkVictim, BYTE bSkillLevel)
{
	FComputeSkillParty f(dwVnum, pkVictim, bSkillLevel);
	if (GetParty() && GetParty()->GetNearMemberCount())
		GetParty()->ForEachNearMember(f);
	else
		f(this);

	return BATTLE_NONE;
}

#if defined(__PVP_BALANCE_IMPROVING__)
int CHARACTER::ComputeGyeongGongSkill(DWORD dwVnum, LPCHARACTER pkVictim, BYTE bSkillLevel)
{
	if (IsPolymorphed())
		return BATTLE_NONE;

	if (g_bSkillDisable)
		return BATTLE_NONE;

	CSkillProto* pkSk = CSkillManager::Instance().Get(dwVnum);

	if (!pkSk)
		return BATTLE_NONE;

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SELFONLY))
		pkVictim = this;

	if (!pkVictim)
	{
		if (test_server)
			sys_log(0, "ComputeGyeongGongSkill: %s Victim == null, skill %d", GetName(), dwVnum);

		return BATTLE_NONE;
	}

	if (0 == bSkillLevel)
	{
		if ((bSkillLevel = GetSkillLevel(pkSk->dwVnum)) == 0)
		{
			if (test_server)
				sys_log(0, "ComputeGyeongGongSkill: name:%s vnum:%d skillLevelBySkill : %d ", GetName(), pkSk->dwVnum, bSkillLevel);
			return BATTLE_NONE;
		}
	}

	if (pkSk->bSkillAttrType != SKILL_ATTR_TYPE_NORMAL)
		OnMove(true);

	LPITEM pkWeapon = GetWear(WEAR_WEAPON);

	SetPolyVarForAttack(this, pkSk, pkWeapon);
	const int iAmount = static_cast<int>(pkSk->kPointPoly2.Eval());

	EffectPacket(SE_FEATHER_WALK, SE_TYPE_POSITION, GetXYZ());

	FuncSplashDamage f(pkVictim->GetX(), pkVictim->GetY(), pkSk, this, iAmount, 0, pkSk->lMaxHit, pkWeapon, m_bDisableCooltime, IsPC() ? &m_SkillUseInfo[dwVnum] : NULL, GetSkillPower(dwVnum, bSkillLevel));
	if (pkVictim->GetSectree())
		pkVictim->GetSectree()->ForEachAround(f);
	else
		f(pkVictim);

	return BATTLE_DAMAGE;
}
#endif

// bSkillLevel 인자가 0이 아닐 경우에는 m_abSkillLevels를 사용하지 않고 강제로
// bSkillLevel로 계산한다.
int CHARACTER::ComputeSkill(DWORD dwVnum, LPCHARACTER pkVictim, BYTE bSkillLevel)
{
	const bool bCanUseHorseSkill = CanUseHorseSkill();

	// 말을 타고있지만 스킬은 사용할 수 없는 상태라면 return
	if (false == bCanUseHorseSkill && true == IsRiding())
		return BATTLE_NONE;

	if (IsPolymorphed())
		return BATTLE_NONE;

	if (g_bSkillDisable)
		return BATTLE_NONE;

	CSkillProto* pkSk = CSkillManager::instance().Get(dwVnum);

	if (!pkSk)
		return BATTLE_NONE;

	if (bCanUseHorseSkill && pkSk->dwType != SKILL_TYPE_HORSE)
		return BATTLE_NONE;

	if (!bCanUseHorseSkill && pkSk->dwType == SKILL_TYPE_HORSE)
		return BATTLE_NONE;

	// 상대방에게 쓰는 것이 아니면 나에게 써야 한다.
	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SELFONLY))
		pkVictim = this;

	if (!pkVictim)
	{
		if (test_server)
			sys_log(0, "ComputeSkill: %s Victim == null, skill %d", GetName(), dwVnum);

		return BATTLE_NONE;
	}

	const DWORD dwDistance = static_cast<DWORD>(DISTANCE_SQRT(GetX() - pkVictim->GetX(), GetY() - pkVictim->GetY()));
	if ((pkSk->dwTargetRange) && (dwDistance >= ((pkSk->dwTargetRange + 50 / 100) * pkVictim->GetMonsterHitRange())))
	{
		if (test_server)
			sys_log(0, "ComputeSkill: Victim too far, skill %d : %s to %s (distance %u limit %u)",
				dwVnum,
				GetName(),
				pkVictim->GetName(),
				(long)DISTANCE_SQRT(GetX() - pkVictim->GetX(), GetY() - pkVictim->GetY()),
				pkSk->dwTargetRange);

		return BATTLE_NONE;
	}

	if (0 == bSkillLevel)
	{
		if ((bSkillLevel = GetSkillLevel(pkSk->dwVnum)) == 0)
		{
			if (test_server)
				sys_log(0, "ComputeSkill : name:%s vnum:%d skillLevelBySkill : %d ", GetName(), pkSk->dwVnum, bSkillLevel);
			return BATTLE_NONE;
		}
	}

	if (pkVictim->IsAffectFlag(AFF_PABEOP) && pkVictim->IsGoodAffect((BYTE)dwVnum))
	{
		return BATTLE_NONE;
	}

	const float k = 1.0 * GetSkillPower(pkSk->dwVnum, bSkillLevel) * pkSk->bMaxLevel / 100;

	pkSk->SetPointVar("k", k);
	pkSk->kSplashAroundDamageAdjustPoly.SetVar("k", k);

	if (pkSk->dwType == SKILL_TYPE_HORSE)
	{
		LPITEM pkBow, pkArrow;
		if (1 == GetArrowAndBow(&pkBow, &pkArrow, 1))
		{
			pkSk->SetPointVar("atk", CalcArrowDamage(this, pkVictim, pkBow, pkArrow, true));
		}
		else
		{
			pkSk->SetPointVar("atk", CalcMeleeDamage(this, pkVictim, true, false));
		}
	}
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_MELEE_DAMAGE))
	{
		pkSk->SetPointVar("atk", CalcMeleeDamage(this, pkVictim, true, false));
	}
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_MAGIC_DAMAGE))
	{
		pkSk->SetPointVar("atk", CalcMagicDamage(this, pkVictim));
	}
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_ARROW_DAMAGE))
	{
		LPITEM pkBow, pkArrow;
		if (1 == GetArrowAndBow(&pkBow, &pkArrow, 1))
		{
			pkSk->SetPointVar("atk", CalcArrowDamage(this, pkVictim, pkBow, pkArrow, true));
		}
		else
		{
			pkSk->SetPointVar("atk", 0);
		}
	}

	if (pkSk->wPointOn == POINT_MOV_SPEED)
	{
		pkSk->SetPointVar("maxv", pkVictim->GetLimitPoint(POINT_MOV_SPEED));
	}

	pkSk->SetPointVar("lv", GetLevel());
	pkSk->SetPointVar("iq", GetPoint(POINT_IQ));
	pkSk->SetPointVar("str", GetPoint(POINT_ST));
	pkSk->SetPointVar("dex", GetPoint(POINT_DX));
	pkSk->SetPointVar("con", GetPoint(POINT_HT));
	pkSk->SetPointVar("maxhp", pkVictim->GetMaxHP());
	pkSk->SetPointVar("maxsp", pkVictim->GetMaxSP());
	pkSk->SetPointVar("chain", 0);
	pkSk->SetPointVar("ar", CalcAttackRating(this, pkVictim));
	pkSk->SetPointVar("def", GetPoint(POINT_DEF_GRADE));
	pkSk->SetPointVar("odef", GetPoint(POINT_DEF_GRADE) - GetPoint(POINT_DEF_GRADE_BONUS));
	pkSk->SetPointVar("horse_level", GetHorseLevel());

	if (pkSk->bSkillAttrType != SKILL_ATTR_TYPE_NORMAL)
		OnMove(true);

	LPITEM pkWeapon = GetWear(WEAR_WEAPON);

	SetPolyVarForAttack(this, pkSk, pkWeapon);

	pkSk->kDurationPoly.SetVar("k", k/* bSkillLevel */);
	pkSk->kDurationPoly2.SetVar("k", k/* bSkillLevel */);

	int iAmount = (int)pkSk->kPointPoly.Eval();
	int iAmount2 = (int)pkSk->kPointPoly2.Eval();
	int iAmount3 = (int)pkSk->kPointPoly3.Eval();

	if (test_server && IsPC())
		sys_log(0, "iAmount: %d %d %d , atk:%f skLevel:%f k:%f GetSkillPower(%d) MaxLevel:%d Per:%f",
			iAmount, iAmount2, iAmount3,
			pkSk->kPointPoly.GetVar("atk"),
			pkSk->kPointPoly.GetVar("k"),
			k,
			GetSkillPower(pkSk->dwVnum, bSkillLevel),
			pkSk->bMaxLevel,
			pkSk->bMaxLevel / 100
		);

	// ADD_GRANDMASTER_SKILL
	if (GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER)
	{
		iAmount = (int)pkSk->kMasterBonusPoly.Eval();
	}

	if (test_server && iAmount == 0 && pkSk->wPointOn != POINT_NONE)
	{
		ChatPacket(CHAT_TYPE_INFO, "효과가 없습니다. 스킬 공식을 확인하세요");
	}
	// END_OF_ADD_GRANDMASTER_SKILL

	//sys_log(0, "XXX SKILL Calc %d Amount %d", dwVnum, iAmount);

	if (IsPC() && pkSk->dwVnum == SKILL_EUNHYUNG)
		ForgetMyAttacker(false);

	// REMOVE_BAD_AFFECT_BUG_FIX
	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_REMOVE_BAD_AFFECT))
	{
		if (number(1, 100) <= iAmount2)
		{
			pkVictim->RemoveBadAffect();
		}
	}
	// END_OF_REMOVE_BAD_AFFECT_BUG_FIX

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_ATTACK | SKILL_FLAG_USE_MELEE_DAMAGE | SKILL_FLAG_USE_MAGIC_DAMAGE) &&
		!(pkSk->dwVnum == SKILL_MUYEONG && pkVictim == this) && !(pkSk->IsChargeSkill() && pkVictim == this))
	{
		bool bAdded = false;

		if (pkSk->wPointOn == POINT_HP && iAmount < 0)
		{
			int iAG = 0;

			FuncSplashDamage f(pkVictim->GetX(), pkVictim->GetY(), pkSk, this, iAmount, iAG, pkSk->lMaxHit, pkWeapon, m_bDisableCooltime, IsPC() ? &m_SkillUseInfo[dwVnum] : NULL, GetSkillPower(dwVnum, bSkillLevel));
			if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
			{
				if (pkVictim->GetSectree())
					pkVictim->GetSectree()->ForEachAround(f);
			}
			else
			{
				f(pkVictim);
			}
		}
		else
		{
			pkSk->kDurationPoly.SetVar("k", k/* bSkillLevel */);
			int iDur = (int)pkSk->kDurationPoly.Eval();

			if (IsPC())
				if (!(dwVnum >= GUILD_SKILL_START && dwVnum <= GUILD_SKILL_END)) // 길드 스킬은 쿨타임 처리를 하지 않는다.
					if (!m_bDisableCooltime && !m_SkillUseInfo[dwVnum].HitOnce(dwVnum) && dwVnum != SKILL_MUYEONG)
						return BATTLE_NONE;

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					pkVictim->AddAffect(pkSk->dwVnum, pkSk->wPointOn, iAmount, pkSk->dwAffectFlag, iDur, 0, true);
				else
				{
					if (pkVictim->GetSectree())
					{
						FuncSplashAffect f(this, pkVictim->GetX(), pkVictim->GetY(), pkSk->iSplashRange, pkSk->dwVnum, pkSk->wPointOn, iAmount, pkSk->dwAffectFlag, iDur, 0, true, pkSk->lMaxHit);
						pkVictim->GetSectree()->ForEachAround(f);
					}
				}
				bAdded = true;
			}
		}

		if (pkSk->wPointOn2 != POINT_NONE && !pkSk->IsChargeSkill())
		{
			pkSk->kDurationPoly2.SetVar("k", k/* bSkillLevel */);
			int iDur = (int)pkSk->kDurationPoly2.Eval();

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

#if defined(__9TH_SKILL__)
				if (pkSk->wPointOn2 == POINT_HIT_PCT)
					AddAffect(pkSk->dwVnum, pkSk->wPointOn2, iAmount2, AFF_NONE, iDur, 0, !bAdded);
#endif

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					pkVictim->AddAffect(pkSk->dwVnum, pkSk->wPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur, 0, !bAdded);
				else
				{
					if (pkVictim->GetSectree())
					{
						FuncSplashAffect f(this, pkVictim->GetX(), pkVictim->GetY(), pkSk->iSplashRange, pkSk->dwVnum, pkSk->wPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur, 0, !bAdded, pkSk->lMaxHit);
						pkVictim->GetSectree()->ForEachAround(f);
					}
				}

				bAdded = true;
			}
			else
			{
				pkVictim->PointChange(pkSk->wPointOn2, iAmount2);
			}
		}

		// ADD_GRANDMASTER_SKILL
		if (pkSk->wPointOn3 != POINT_NONE && !pkSk->IsChargeSkill() && GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER)
		{
			pkSk->kDurationPoly3.SetVar("k", k /* bSkillLevel */);
			int iDur = (int)pkSk->kDurationPoly3.Eval();

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					pkVictim->AddAffect(pkSk->dwVnum, pkSk->wPointOn3, iAmount3, /* pkSk->dwAffectFlag3 */ 0, iDur, 0, !bAdded);
				else
				{
					if (pkVictim->GetSectree())
					{
						FuncSplashAffect f(this, pkVictim->GetX(), pkVictim->GetY(), pkSk->iSplashRange, pkSk->dwVnum, pkSk->wPointOn3, iAmount3, /* pkSk->dwAffectFlag3 */ 0, iDur, 0, !bAdded, pkSk->lMaxHit);
						pkVictim->GetSectree()->ForEachAround(f);
					}
				}

				bAdded = true;
			}
			else
			{
				pkVictim->PointChange(pkSk->wPointOn3, iAmount3);
			}
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		return BATTLE_DAMAGE;
	}
	else
	{
		if (dwVnum == SKILL_MUYEONG)
		{
			pkSk->kDurationPoly.SetVar("k", k/* bSkillLevel */);
			pkSk->kDurationSPCostPoly.SetVar("k", k/* bSkillLevel */);

			int iDur = (long)pkSk->kDurationPoly.Eval();
			iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

			if (pkVictim == this)
				AddAffect(dwVnum,
					POINT_NONE, 0,
					AFF_MUYEONG,
					iDur,
					(long)pkSk->kDurationSPCostPoly.Eval(),
					true);

			return BATTLE_NONE;
		}

		bool bAdded = false;
		pkSk->kDurationPoly.SetVar("k", k/* bSkillLevel */);
		int iDur = (int)pkSk->kDurationPoly.Eval();

		if (iDur > 0)
		{
			if (pkSk->dwVnum == SKILL_JEUNGRYEOK) // Nature's Enchantment
			{
				if (pkVictim->GetJob() == JOB_SHAMAN)
				{
					iAmount *= 2;
					if (test_server)
						sys_log(0, "Buffed Nature's Enchantment to %d.", iAmount);
				}
			}

			iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);
			// AffectFlag가 없거나, toggle 하는 것이 아니라면..
			pkSk->kDurationSPCostPoly.SetVar("k", k/* bSkillLevel */);

			if (pkSk->wPointOn2 != POINT_NONE)
			{
				pkVictim->RemoveAffect(pkSk->dwVnum);

				int iDur2 = (int)pkSk->kDurationPoly2.Eval();

				if (iDur2 > 0)
				{
					if (test_server)
						sys_log(0, "SKILL_AFFECT: %s %s Dur:%d To:%d Amount:%d",
							GetName(),
							pkSk->szName,
							iDur2,
							pkSk->wPointOn2,
							iAmount2);

					iDur2 += GetPoint(POINT_PARTY_BUFFER_BONUS);
					pkVictim->AddAffect(pkSk->dwVnum, pkSk->wPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur2, 0, false);
				}
				else
				{
					pkVictim->PointChange(pkSk->wPointOn2, iAmount2);
				}

				DWORD affact_flag = pkSk->dwAffectFlag;

				// ADD_GRANDMASTER_SKILL
				if ((pkSk->dwVnum == SKILL_CHUNKEON && GetUsedSkillMasterType(pkSk->dwVnum) < SKILL_GRAND_MASTER))
				{
					affact_flag = AFF_CHEONGEUN_WITH_FALL;
				}
				// END_OF_ADD_GRANDMASTER_SKILL

				pkVictim->AddAffect(pkSk->dwVnum,
					pkSk->wPointOn,
					iAmount,
					affact_flag,
					iDur,
					(long)pkSk->kDurationSPCostPoly.Eval(),
					false);
			}
			else
			{
				if (test_server)
					sys_log(0, "SKILL_AFFECT: %s %s Dur:%d To:%d Amount:%d",
						GetName(),
						pkSk->szName,
						iDur,
						pkSk->wPointOn,
						iAmount);

#if defined(__9TH_SKILL__)
				DWORD dwAffectFlag = pkSk->dwAffectFlag;

				if (dwVnum == SKILL_CHEONUN)
				{
					int iSkillMasterType = GetSkillMasterType(pkSk->dwVnum);
					switch (iSkillMasterType)
					{
						case SKILL_NORMAL:
							dwAffectFlag = AFF_CHEONUN_NORMAL;
							break;
						case SKILL_MASTER:
							dwAffectFlag = AFF_CHEONUN_MASTER;
							break;
						case SKILL_GRAND_MASTER:
							dwAffectFlag = AFF_CHEONUN_GRAND_MASTER;
							break;
						case SKILL_PERFECT_MASTER:
							dwAffectFlag = AFF_CHEONUN_PERFECT_MASTER;
							break;
						default:
							dwAffectFlag = pkSk->dwAffectFlag;
							break;
					}
				}

				pkVictim->AddAffect(pkSk->dwVnum,
					pkSk->wPointOn,
					iAmount,
					dwAffectFlag,
					iDur,
					(long)pkSk->kDurationSPCostPoly.Eval(),
					// ADD_GRANDMASTER_SKILL
					!bAdded, false,
#if defined(__AFFECT_RENEWAL__)
					false, /*bRealTime*/
#endif
					iAmount2
				);
				// END_OF_ADD_GRANDMASTER_SKILL
#else

				pkVictim->AddAffect(pkSk->dwVnum,
					pkSk->wPointOn,
					iAmount,
					pkSk->dwAffectFlag,
					iDur,
					(long)pkSk->kDurationSPCostPoly.Eval(),
					// ADD_GRANDMASTER_SKILL
					!bAdded
				);
				// END_OF_ADD_GRANDMASTER_SKILL
#endif
			}

			bAdded = true;
		}
		else
		{
			if (!pkSk->IsChargeSkill())
				pkVictim->PointChange(pkSk->wPointOn, iAmount);

			if (pkSk->wPointOn2 != POINT_NONE)
			{
				pkVictim->RemoveAffect(pkSk->dwVnum);

				int iDur2 = (int)pkSk->kDurationPoly2.Eval();

				if (iDur2 > 0)
				{
					iDur2 += GetPoint(POINT_PARTY_BUFFER_BONUS);

					if (pkSk->IsChargeSkill())
						pkVictim->AddAffect(pkSk->dwVnum, pkSk->wPointOn2, iAmount2, AFF_TANHWAN_DASH, iDur2, 0, false);
					else
						pkVictim->AddAffect(pkSk->dwVnum, pkSk->wPointOn2, iAmount2, pkSk->dwAffectFlag2, iDur2, 0, false);
				}
				else
				{
					pkVictim->PointChange(pkSk->wPointOn2, iAmount2);
				}

			}
		}

		// ADD_GRANDMASTER_SKILL
		if (pkSk->wPointOn3 != POINT_NONE && !pkSk->IsChargeSkill() && GetUsedSkillMasterType(pkSk->dwVnum) >= SKILL_GRAND_MASTER)
		{
			pkSk->kDurationPoly3.SetVar("k", k/* bSkillLevel */);
			int iDur = (int)pkSk->kDurationPoly3.Eval();

			sys_log(0, "try third %u %d %d %d 1894", pkSk->dwVnum, pkSk->wPointOn3, iDur, iAmount3);

			if (iDur > 0)
			{
				iDur += GetPoint(POINT_PARTY_BUFFER_BONUS);

				if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_SPLASH))
					pkVictim->AddAffect(pkSk->dwVnum, pkSk->wPointOn3, iAmount3, /* pkSk->dwAffectFlag3 */ 0, iDur, 0, !bAdded);
				else
				{
					if (pkVictim->GetSectree())
					{
						FuncSplashAffect f(this, pkVictim->GetX(), pkVictim->GetY(), pkSk->iSplashRange, pkSk->dwVnum, pkSk->wPointOn3, iAmount3, /* pkSk->dwAffectFlag3 */ 0, iDur, 0, !bAdded, pkSk->lMaxHit);
						pkVictim->GetSectree()->ForEachAround(f);
					}
				}

				bAdded = true;
			}
			else
			{
				pkVictim->PointChange(pkSk->wPointOn3, iAmount3);
			}
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		return BATTLE_NONE;
	}
}

bool CHARACTER::UseSkill(DWORD dwVnum, LPCHARACTER pkVictim, bool bUseGrandMaster)
{
	if (false == CanUseSkill(dwVnum))
		return false;

	if ((dwVnum == SKILL_GEOMKYUNG || dwVnum == SKILL_GWIGEOM) && !GetWear(WEAR_WEAPON))
		return false;

	// NO_GRANDMASTER
	if (test_server)
	{
		if (quest::CQuestManager::instance().GetEventFlag("no_grand_master"))
		{
			bUseGrandMaster = false;
		}
	}
	// END_OF_NO_GRANDMASTER

	if (g_bSkillDisable)
		return false;

	if (IsObserverMode())
		return false;

	if (!CanMove())
		return false;

	if (IsPolymorphed())
		return false;

	const bool bCanUseHorseSkill = CanUseHorseSkill();

	if (dwVnum == SKILL_HORSE_SUMMON)
	{
		if (GetSkillLevel(dwVnum) == 0)
			return false;

		if (GetHorseLevel() <= 0)
			ChatPacket(CHAT_TYPE_INFO, LC_STRING("말이 없습니다. 마굿간 경비병을 찾아가세요."));
		else
			ChatPacket(CHAT_TYPE_INFO, LC_STRING("말 소환 아이템을 사용하세요."));

		return true;
	}

	// 말을 타고있지만 스킬은 사용할 수 없는 상태라면 return false
	if (false == bCanUseHorseSkill && true == IsRiding())
		return false;

	CSkillProto* pkSk = CSkillManager::instance().Get(dwVnum);
	sys_log(0, "%s: USE_SKILL: %d pkVictim %p", GetName(), dwVnum, get_pointer(pkVictim));

	if (!pkSk)
		return false;

	if (bCanUseHorseSkill && pkSk->dwType != SKILL_TYPE_HORSE)
		return BATTLE_NONE;

	if (!bCanUseHorseSkill && pkSk->dwType == SKILL_TYPE_HORSE)
		return BATTLE_NONE;

	if (GetSkillLevel(dwVnum) == 0)
		return false;

	// NO_GRANDMASTER
	if (GetSkillMasterType(dwVnum) < SKILL_GRAND_MASTER)
		bUseGrandMaster = false;
	// END_OF_NO_GRANDMASTER

	// MINING
	if (GetWear(WEAR_WEAPON) && (GetWear(WEAR_WEAPON)->GetType() == ITEM_ROD || GetWear(WEAR_WEAPON)->GetType() == ITEM_PICK))
		return false;
	// END_OF_MINING

	m_SkillUseInfo[dwVnum].TargetVIDMap.clear();

	if (pkSk->IsChargeSkill())
	{
		if (IsAffectFlag(AFF_TANHWAN_DASH) || pkVictim && pkVictim != this)
		{
			if (!pkVictim)
				return false;

			if (!IsAffectFlag(AFF_TANHWAN_DASH))
			{
				if (!UseSkill(dwVnum, this))
					return false;
			}

			m_SkillUseInfo[dwVnum].SetMainTargetVID(pkVictim->GetVID());
			// DASH 상태의 탄환격은 공격기술
			ComputeSkill(dwVnum, pkVictim);
			RemoveAffect(dwVnum);
			return true;
		}
	}

	if (dwVnum == SKILL_COMBO)
	{
		if (m_bComboIndex)
			m_bComboIndex = 0;
		else
			m_bComboIndex = GetSkillLevel(SKILL_COMBO);

		ChatPacket(CHAT_TYPE_COMMAND, "combo %d", m_bComboIndex);
		return true;
	}

	// Toggle 할 때는 SP를 쓰지 않음 (SelfOnly로 구분)
	if ((0 != pkSk->dwAffectFlag || pkSk->dwVnum == SKILL_MUYEONG) && (pkSk->dwFlag & SKILL_FLAG_TOGGLE) && RemoveAffect(pkSk->dwVnum))
	{
		return true;
	}

	if (IsAffectFlag(AFF_REVIVE_INVISIBLE))
		RemoveAffect(AFFECT_REVIVE_INVISIBLE);

	const float k = GetSkillPower(pkSk->dwVnum) * pkSk->bMaxLevel / 100.0f;

	pkSk->SetPointVar("k", k);
	pkSk->kSplashAroundDamageAdjustPoly.SetVar("k", k);

	// 쿨타임 체크
	pkSk->kCooldownPoly.SetVar("k", k);
	int iCooltime = (int)pkSk->kCooldownPoly.Eval();
	int lMaxHit = pkSk->lMaxHit ? pkSk->lMaxHit : -1;

	pkSk->SetSPCostVar("k", k);

	DWORD dwCur = get_dword_time();

	if (dwVnum == SKILL_TERROR && m_SkillUseInfo[dwVnum].bUsed && m_SkillUseInfo[dwVnum].dwNextSkillUsableTime > dwCur)
	{
		sys_log(0, " SKILL_TERROR's Cooltime is not delta over %u", m_SkillUseInfo[dwVnum].dwNextSkillUsableTime - dwCur);
		return false;
	}

	int iNeededSP = 0;

	//if (dwVnum == 51)
	//	AddAffect(AFF_EUNHYUNG, POINT_NONE, 0, AFF_EUNHYUNG, 4, 0, true);

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_USE_HP_AS_COST))
	{
		pkSk->SetSPCostVar("maxhp", GetMaxHP());
		pkSk->SetSPCostVar("v", GetHP());
		iNeededSP = (int)pkSk->kSPCostPoly.Eval();

		// ADD_GRANDMASTER_SKILL
		if (GetSkillMasterType(dwVnum) >= SKILL_GRAND_MASTER && bUseGrandMaster)
		{
			iNeededSP = (int)pkSk->kGrandMasterAddSPCostPoly.Eval();
		}
		// END_OF_ADD_GRANDMASTER_SKILL

		if (GetHP() < iNeededSP)
			return false;

		PointChange(POINT_HP, -iNeededSP);
	}
	else
	{
		// SKILL_FOMULA_REFACTORING
		pkSk->SetSPCostVar("maxhp", GetMaxHP());
		pkSk->SetSPCostVar("maxv", GetMaxSP());
		pkSk->SetSPCostVar("v", GetSP());

		iNeededSP = (int)pkSk->kSPCostPoly.Eval();

		if (GetSkillMasterType(dwVnum) >= SKILL_GRAND_MASTER && bUseGrandMaster)
		{
			iNeededSP = (int)pkSk->kGrandMasterAddSPCostPoly.Eval();
		}
		// END_OF_SKILL_FOMULA_REFACTORING

		if (GetSP() < iNeededSP)
			return false;

		if (test_server)
			ChatPacket(CHAT_TYPE_INFO, LC_STRING("%s SP소모: %d", LC_SKILL(pkSk->dwVnum), iNeededSP));

		PointChange(POINT_SP, -iNeededSP);
	}

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SELFONLY))
		pkVictim = this;

	if ((pkSk->dwVnum == SKILL_MUYEONG) || (pkSk->IsChargeSkill() && !IsAffectFlag(AFF_TANHWAN_DASH) && !pkVictim))
	{
		// 처음 사용하는 무영진은 자신에게 Affect를 붙인다.
		pkVictim = this;
	}

	int iSplashCount = 1;

	if (false == m_bDisableCooltime)
	{
		if (false ==
			m_SkillUseInfo[dwVnum].UseSkill(
				bUseGrandMaster,
				(NULL != pkVictim && SKILL_HORSE_WILDATTACK != dwVnum) ? pkVictim->GetVID() : 0,
				ComputeCooltime(iCooltime * 1000),
				iSplashCount,
				lMaxHit))
		{
			if (test_server)
				ChatPacket(CHAT_TYPE_NOTICE, "cooltime not finished %s %d", pkSk->szName, iCooltime);

			return false;
		}
	}

	if (dwVnum == SKILL_CHAIN)
	{
		ResetChainLightningIndex();
		AddChainLightningExcept(pkVictim);
	}

#if defined(__PVP_BALANCE_IMPROVING__)
	if (dwVnum == SKILL_PAERYONG)
		ComputeSkill(dwVnum, pkVictim);
#endif

#if defined(__9TH_SKILL__)
	if (dwVnum == SKILL_METEO)
		ComputeSkill(dwVnum, pkVictim);
#endif

	if (IS_SET(pkSk->dwFlag, SKILL_FLAG_SELFONLY))
		ComputeSkill(dwVnum, this);
	else if (IS_SET(pkSk->dwFlag, SKILL_FLAG_PARTY))
	{
		ComputeSkillParty(dwVnum, this);

		if (pkVictim)
			ComputeSkill(dwVnum, pkVictim);
	}
	else if (!IS_SET(pkSk->dwFlag, SKILL_FLAG_ATTACK))
		ComputeSkill(dwVnum, pkVictim);
	else if (dwVnum == SKILL_BYEURAK)
		ComputeSkill(dwVnum, pkVictim);
	else if (dwVnum == SKILL_MUYEONG || pkSk->IsChargeSkill())
		ComputeSkill(dwVnum, pkVictim);

	m_dwLastSkillTime = get_dword_time();

	return true;
}

int CHARACTER::GetUsedSkillMasterType(DWORD dwVnum)
{
	const TSkillUseInfo& rInfo = m_SkillUseInfo[dwVnum];

	if (GetSkillMasterType(dwVnum) < SKILL_GRAND_MASTER)
		return GetSkillMasterType(dwVnum);

	if (rInfo.isGrandMaster)
		return GetSkillMasterType(dwVnum);

	return MIN(GetSkillMasterType(dwVnum), SKILL_MASTER);
}

int CHARACTER::GetSkillMasterType(DWORD dwVnum) const
{
	if (!IsPC())
		return 0;

	if (dwVnum >= SKILL_MAX_NUM)
	{
		sys_err("%s skill vnum overflow %u", GetName(), dwVnum);
		return 0;
	}

	return m_pSkillLevels ? m_pSkillLevels[dwVnum].bMasterType : SKILL_NORMAL;
}

#if defined(__CONQUEROR_LEVEL__)
bool CHARACTER::IsConquerorSkill(DWORD dwVnum) const
{
	if (!IsPC())
		return false;

	if (dwVnum >= SKILL_MAX_NUM)
	{
		sys_err("%s skill vnum overflow %u", GetName(), dwVnum);
		return false;
	}

	/*
	* Since there is currently no way to check if the skills
	* are conqueror level we have to hard code them.
	*/

	if (dwVnum >= SKILL_FINISH && dwVnum <= SKILL_ILIPUNGU)
		return true;

	return false;
}
#endif

int CHARACTER::GetSkillPower(DWORD dwVnum, BYTE bLevel) const
{
	// 인어반지 아이템
	if (dwVnum >= SKILL_LANGUAGE1 && dwVnum <= SKILL_LANGUAGE3 && IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_LANGUAGE))
	{
		return 100;
	}

	if (dwVnum >= GUILD_SKILL_START && dwVnum <= GUILD_SKILL_END)
	{
		if (GetGuild())
			return 100 * GetGuild()->GetSkillLevel(dwVnum) / 7 / 7;
		else
			return 0;
	}

	if (bLevel)
	{
		// SKILL_POWER_BY_LEVEL
		return GetSkillPowerByLevel(bLevel, true);
		// END_SKILL_POWER_BY_LEVEL;
	}

	if (dwVnum >= SKILL_MAX_NUM)
	{
		sys_err("%s skill vnum overflow %u", GetName(), dwVnum);
		return 0;
	}

	// SKILL_POWER_BY_LEVEL
	return GetSkillPowerByLevel(GetSkillLevel(dwVnum));
	// SKILL_POWER_BY_LEVEL
}

int CHARACTER::GetSkillLevel(DWORD dwVnum) const
{
	if (dwVnum >= SKILL_MAX_NUM)
	{
		sys_err("%s skill vnum overflow %u", GetName(), dwVnum);
		sys_log(0, "%s skill vnum overflow %u", GetName(), dwVnum);
		return 0;
	}

	return MIN(SKILL_MAX_LEVEL, m_pSkillLevels ? m_pSkillLevels[dwVnum].bLevel : 0);
}

EVENTFUNC(skill_muyoung_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);

	if (info == NULL)
	{
		sys_err("skill_muyoung_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER ch = info->ch;

	if (ch == NULL) // <Factor>
		return 0;

	if (!ch->IsAffectFlag(AFF_MUYEONG))
	{
		ch->StopMuyeongEvent();
		return 0;
	}

	// 1. Find Victim
	FFindNearVictim f(ch, ch);
	if (ch->GetSectree())
	{
		ch->GetSectree()->ForEachAround(f);
		// 2. Shoot!
		if (f.GetVictim())
		{
			ch->CreateFly(FLY_SKILL_MUYEONG, f.GetVictim());
			ch->ComputeSkill(SKILL_MUYEONG, f.GetVictim());
		}
	}

	return PASSES_PER_SEC(3);
}

void CHARACTER::StartMuyeongEvent()
{
	if (m_pkMuyeongEvent)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();

	info->ch = this;
	m_pkMuyeongEvent = event_create(skill_muyoung_event, info, PASSES_PER_SEC(1));
}

void CHARACTER::StopMuyeongEvent()
{
	event_cancel(&m_pkMuyeongEvent);
}

#if defined(__PVP_BALANCE_IMPROVING__)
EVENTFUNC(skill_gyeonggong_event)
{
	const char_event_info* info = dynamic_cast<char_event_info*>(event->info);

	if (info == NULL)
	{
		sys_err("skill_gyeonggong_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER ch = info->ch;
	if (ch == NULL)
		return 0;

	if (!ch->IsAffectFlag(AFF_GYEONGGONG))
	{
		ch->StopGyeongGongEvent();
		return 0;
	}

	ch->ComputeGyeongGongSkill(SKILL_GYEONGGONG, ch);

	return PASSES_PER_SEC(1);
}

void CHARACTER::StartGyeongGongEvent()
{
	if (m_pkGyeongGongEvent)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();
	info->ch = this;
	m_pkGyeongGongEvent = event_create(skill_gyeonggong_event, info, PASSES_PER_SEC(1));
}

void CHARACTER::StopGyeongGongEvent()
{
	event_cancel(&m_pkGyeongGongEvent);
}
#endif

#if defined(__9TH_SKILL__)
struct FPartyAffect
{
	void operator() (LPCHARACTER ch)
	{
		ch->ExitToSavedLocation();
	}
};

EVENTINFO(skill_cheonun_event_info)
{
	DynamicCharacterPtr ch;
	BYTE prob, duration;
};

EVENTFUNC(skill_cheonun_event)
{
	skill_cheonun_event_info* info = dynamic_cast<skill_cheonun_event_info*>(event->info);
	if (info == nullptr)
	{
		sys_err("skill_cheonun_event_info NULL");
		return 0;
	}

	const LPCHARACTER ch = info->ch;
	if (ch == nullptr)
		return 0;

	if (!ch->FindAffect(SKILL_CHEONUN /*AFFECT_SKILL_9_CHEONUN*/))
	{
		ch->StopCheonunEvent();
		return 0;
	}

	const BYTE max_prob = 100;
	const BYTE prob = number(1, max_prob);
	if (prob >= max_prob - info->prob)
	{
		const LPPARTY party = ch->GetParty();
		if (party)
		{
			auto FPartyAffect = [&info](const LPCHARACTER& ch)
			{
				ch->AddAffect(AFFECT_CHEONUN_INVINCIBILITY, APPLY_NONE, 1, AFF_CHEONUN_INVINCIBILITY, info->duration, 0, true);
			};
			party->ForEachNearMember(FPartyAffect);
		}
		else
		{
			ch->AddAffect(AFFECT_CHEONUN_INVINCIBILITY, APPLY_NONE, 1, AFF_CHEONUN_INVINCIBILITY, info->duration, 0, true);
		}
	}

	return PASSES_PER_SEC(12);
}

void CHARACTER::StartCheonunEvent(BYTE bChance, BYTE bDuration)
{
	if (m_pkCheonunEvent)
		return;

	skill_cheonun_event_info* info = AllocEventInfo<skill_cheonun_event_info>();
	info->ch = this;
	info->prob = bChance;
	info->duration = bDuration;
	m_pkCheonunEvent = event_create(skill_cheonun_event, info, PASSES_PER_SEC(1));
}

void CHARACTER::StopCheonunEvent()
{
	event_cancel(&m_pkCheonunEvent);
}
#endif

void CHARACTER::SkillLearnWaitMoreTimeMessage(DWORD ms)
{
	// const char* str = "";
	//
	if (ms < 3 * 60)
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("몸 속이 뜨겁군. 하지만 아주 편안해. 이대로 기를 안정시키자."));
	else if (ms < 5 * 60)
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("그래, 천천히. 좀더 천천히, 그러나 막힘 없이 빠르게!"));
	else if (ms < 10 * 60) // 10분
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("그래, 이 느낌이야. 체내에 기가 아주 충만해."));
	else if (ms < 30 * 60) // 30분
	{
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("다 읽었다! 이제 비급에 적혀있는 대로 전신에 기를 돌리기만 하면,"));
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("그것으로 수련은 끝난 거야!"));
	}
	else if (ms < 1 * 3600) // 1시간
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("이제 책의 마지막 장이야! 수련의 끝이 눈에 보이고 있어!"));
	else if (ms < 2 * 3600) // 2시간
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("얼마 안 남았어! 조금만 더!"));
	else if (ms < 3 * 3600)
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("좋았어! 조금만 더 읽으면 끝이다!"));
	else if (ms < 6 * 3600)
	{
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("책장도 이제 얼마 남지 않았군."));
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("뭔가 몸 안에 힘이 생기는 기분인 걸."));
	}
	else if (ms < 12 * 3600)
	{
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("이제 좀 슬슬 가닥이 잡히는 것 같은데."));
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("좋아, 이 기세로 계속 나간다!"));
	}
	else if (ms < 18 * 3600)
	{
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("아니 어떻게 된 게 종일 읽어도 머리에 안 들어오냐."));
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("공부하기 싫어지네."));
	}
	else //if (ms < 2 * 86400)
	{
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("생각만큼 읽기가 쉽지가 않군. 이해도 어렵고 내용도 난해해."));
		ChatPacket(CHAT_TYPE_TALKING, LC_STRING("이래서야 공부가 안된다구."));
	}

	/*
		str = "30%";
	else if (ms < 3 * 86400)
		str = "10%";
	else if (ms < 4 * 86400)
		str = "5%";
	else
		str = "0%";

	ChatPacket(CHAT_TYPE_TALKING, "%s", str);
	*/
}

void CHARACTER::DisableCooltime()
{
	m_bDisableCooltime = true;
}

bool CHARACTER::HasMobSkill() const
{
	return CountMobSkill() > 0;
}

size_t CHARACTER::CountMobSkill() const
{
	if (!m_pkMobData)
		return 0;

	size_t c = 0;

	for (size_t i = 0; i < MOB_SKILL_MAX_NUM; ++i)
		if (m_pkMobData->m_table.Skills[i].dwVnum)
			++c;

	return c;
}

const TMobSkillInfo* CHARACTER::GetMobSkill(unsigned int idx) const
{
	if (idx >= MOB_SKILL_MAX_NUM)
		return NULL;

	if (!m_pkMobData)
		return NULL;

	if (0 == m_pkMobData->m_table.Skills[idx].dwVnum)
		return NULL;

	return &m_pkMobData->m_mobSkillInfo[idx];
}

bool CHARACTER::CanUseMobSkill(unsigned int idx) const
{
	const TMobSkillInfo* pInfo = GetMobSkill(idx);

	if (!pInfo)
		return false;

	if (m_adwMobSkillCooltime[idx] > get_dword_time())
		return false;

	if (number(0, 1))
		return false;

#if defined(__GUILD_DRAGONLAIR_SYSTEM__)
	if (CGuildDragonLairManager::Instance().IsKing(GetRaceNum()))
	{
		CGuildDragonLair* pGuildDragonLair = GetGuildDragonLair();
		if (pGuildDragonLair && pGuildDragonLair->GetSpecialAttack())
			return false;
	}
#endif

	return true;
}

EVENTINFO(mob_skill_event_info)
{
	DynamicCharacterPtr ch;
	PIXEL_POSITION pos;
	DWORD vnum;
	int index;
	BYTE level;

	mob_skill_event_info()
		: ch()
		, pos()
		, vnum(0)
		, index(0)
		, level(0)
	{
	}
};

EVENTFUNC(mob_skill_hit_event)
{
	mob_skill_event_info* info = dynamic_cast<mob_skill_event_info*>(event->info);

	if (info == NULL)
	{
		sys_err("mob_skill_event_info> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER ch = info->ch;
	if (ch == NULL) // <Factor>
		return 0;

	ch->ComputeSkillAtPosition(info->vnum, info->pos, info->level);

	ch->m_mapMobSkillEvent.erase(info->index);

	return 0;
}

#if defined(__DAWNMIST_DUNGEON__)
struct FPartyHealer
{
	int m_iSkillLevel;
	FPartyHealer(int iSkillLevel) : m_iSkillLevel(iSkillLevel) {}
	void operator() (const LPENTITY pkEntity)
	{
		if (pkEntity && !pkEntity->IsType(ENTITY_CHARACTER))
			return;

		const LPCHARACTER pkChar = dynamic_cast<LPCHARACTER>(pkEntity);
		if (pkChar == nullptr)
			return;

		if (!pkChar->IsMonster() || pkChar->IsDead())
			return;

		if (pkChar->GetHP() >= pkChar->GetMaxHP())
			return;

		CSkillProto* pkSkill = CSkillManager::instance().Get(SKILL_HEAL);
		if (NULL != pkSkill)
		{
			pkSkill->SetPointVar("maxhp", pkChar->GetMaxHP());
			pkSkill->SetPointVar("k", 1.0f * m_iSkillLevel / 100.0f);

			int iAmount = static_cast<int>(pkSkill->kPointPoly.Eval());
			
			
			pkChar->PointChange(pkSkill->wPointOn, iAmount);
			pkChar->EffectPacket(SE_HEAL);
		}
		//pkChar->PointChange(POINT_HP, MAX(1, (pkChar->GetMaxHP() * pkChar->GetMobTable().bRegenPercent) / 100)); // Old
	}
};

EVENTINFO(HealEventInfo)
{
	LPCHARACTER pHealer;
	BYTE bSkillLevel;
	int iCooltime;
};

EVENTFUNC(HealEvent)
{
	HealEventInfo* pInfo = dynamic_cast<HealEventInfo*>(event->info);
	if (pInfo == nullptr)
	{
		sys_err("HealerEvent: <Factor> Null pointer");
		return 0;
	}

	const LPCHARACTER pkHealer = pInfo->pHealer;
	if ((pkHealer == nullptr) || (pkHealer->IsDead() || pkHealer->IsStun()))
		return 0;

	if (!pkHealer->IsHealer())
		return 0;

	const LPPARTY pkParty = pkHealer->GetParty();
	if (pkParty == nullptr)
		return 0;

	FPartyHealer f(pInfo->bSkillLevel);
	pkParty->ForEachMemberPtr(f);

	return PASSES_PER_SEC(pInfo->iCooltime);
};
#endif

bool CHARACTER::UseMobSkill(unsigned int idx)
{
	if (IsPC())
		return false;

	const TMobSkillInfo* pInfo = GetMobSkill(idx);

	if (!pInfo)
		return false;

	DWORD dwVnum = pInfo->dwSkillVnum;
	CSkillProto* pkSk = CSkillManager::instance().Get(dwVnum);

	if (!pkSk)
		return false;

	const float k = 1.0 * GetSkillPower(pkSk->dwVnum, pInfo->bSkillLevel) * pkSk->bMaxLevel / 100;

	pkSk->kCooldownPoly.SetVar("k", k);
	int iCooltime = (int)(pkSk->kCooldownPoly.Eval() * 1000);

	m_adwMobSkillCooltime[idx] = get_dword_time() + iCooltime;

	sys_log(0, "USE_MOB_SKILL: %s idx %d vnum %u cooltime %d", GetName(), idx, dwVnum, iCooltime);

#if defined(__DAWNMIST_DUNGEON__)
	if (IsHealer() && IS_SET(pkSk->dwFlag, SKILL_FLAG_PARTY))
	{
		if (m_pkHealEvent == NULL)
		{
			HealEventInfo* pHealEventInfo = AllocEventInfo<HealEventInfo>();
			pHealEventInfo->pHealer = this;
			pHealEventInfo->bSkillLevel = pInfo->bSkillLevel;
			pHealEventInfo->iCooltime = iCooltime / 1000;
			m_pkHealEvent = event_create(HealEvent, pHealEventInfo, PASSES_PER_SEC(1));
		}
		return true;
	}
#endif

	sys_log(0, "USE_MOB_SKILL: %s idx %d vnum %u cooltime %d", GetName(), idx, dwVnum, iCooltime);

	if (m_pkMobData->m_mobSkillInfo[idx].vecSplashAttack.empty())
	{
		sys_err("No skill hit data for mob %s index %d", GetName(), idx);
		return false;
	}

	for (size_t i = 0; i < m_pkMobData->m_mobSkillInfo[idx].vecSplashAttack.size(); i++)
	{
		PIXEL_POSITION pos = GetXYZ();
		const TMobSplashAttackInfo& rInfo = m_pkMobData->m_mobSkillInfo[idx].vecSplashAttack[i];

		if (rInfo.dwHitDistance)
		{
			float fx, fy;
			GetDeltaByDegree(GetRotation(), rInfo.dwHitDistance, &fx, &fy);
			pos.x += (long)fx;
			pos.y += (long)fy;
		}

		if (rInfo.dwTiming)
		{
			if (test_server)
				sys_log(0, "               timing %ums", rInfo.dwTiming);

			mob_skill_event_info* info = AllocEventInfo<mob_skill_event_info>();

			info->ch = this;
			info->pos = pos;
			info->level = pInfo->bSkillLevel;
			info->vnum = dwVnum;
			info->index = i;

			// <Factor> Cancel existing event first
			auto it = m_mapMobSkillEvent.find(i);
			if (it != m_mapMobSkillEvent.end())
			{
				LPEVENT existing = it->second;
				event_cancel(&existing);
				m_mapMobSkillEvent.erase(it);
			}

			m_mapMobSkillEvent.insert(std::make_pair(i, event_create(mob_skill_hit_event, info, PASSES_PER_SEC(rInfo.dwTiming) / 1000)));
		}
		else
		{
			ComputeSkillAtPosition(dwVnum, pos, pInfo->bSkillLevel);
		}
	}

	return true;
}

void CHARACTER::ResetMobSkillCooltime()
{
	memset(m_adwMobSkillCooltime, 0, sizeof(m_adwMobSkillCooltime));
}

bool CHARACTER::IsUsableSkillMotion(DWORD dwMotionIndex) const
{
	DWORD selfJobGroup = (GetJob() + 1) * 10 + GetSkillGroup();

	const DWORD SKILL_NUM = 184;
	static DWORD s_anSkill2JobGroup[SKILL_NUM] = {
		0, // common_skill 0
		11, // job_skill(WARRIOR SKILL) 1
		11, // job_skill(WARRIOR SKILL) 2
		11, // job_skill(WARRIOR SKILL) 3
		11, // job_skill(WARRIOR SKILL) 4
		11, // job_skill(WARRIOR SKILL) 5
		11, // job_skill(WARRIOR SKILL) 6
		0, // common_skill 7
		0, // common_skill 8
		0, // common_skill 9
		0, // common_skill 10
		0, // common_skill 11
		0, // common_skill 12
		0, // common_skill 13
		0, // common_skill 14
		0, // common_skill 15
		12, // job_skill(WARRIOR SKILL) 16
		12, // job_skill(WARRIOR SKILL) 17
		12, // job_skill(WARRIOR SKILL) 18
		12, // job_skill(WARRIOR SKILL) 19
		12, // job_skill(WARRIOR SKILL) 20
		12, // job_skill(WARRIOR SKILL) 21
		0, // common_skill 22
		0, // common_skill 23
		0, // common_skill 24
		0, // common_skill 25
		0, // common_skill 26
		0, // common_skill 27
		0, // common_skill 28
		0, // common_skill 29
		0, // common_skill 30
		21, // job_skill(ASSASSIN SKILL) 31
		21, // job_skill(ASSASSIN SKILL) 32
		21, // job_skill(ASSASSIN SKILL) 33
		21, // job_skill(ASSASSIN SKILL) 34
		21, // job_skill(ASSASSIN SKILL) 35
		21, // job_skill(ASSASSIN SKILL) 36
		0, // common_skill 37
		0, // common_skill 38
		0, // common_skill 39
		0, // common_skill 40
		0, // common_skill 41
		0, // common_skill 42
		0, // common_skill 43
		0, // common_skill 44
		0, // common_skill 45
		22, // job_skill(ASSASSIN SKILL) 46
		22, // job_skill(ASSASSIN SKILL) 47
		22, // job_skill(ASSASSIN SKILL) 48
		22, // job_skill(ASSASSIN SKILL) 49
		22, // job_skill(ASSASSIN SKILL) 50
		22, // job_skill(ASSASSIN SKILL) 51
		0, // common_skill 52
		0, // common_skill 53
		0, // common_skill 54
		0, // common_skill 55
		0, // common_skill 56
		0, // common_skill 57
		0, // common_skill 58
		0, // common_skill 59
		0, // common_skill 60
		31, // job_skill(SURA SKILL) 61
		31, // job_skill(SURA SKILL) 62
		31, // job_skill(SURA SKILL) 63
		31, // job_skill(SURA SKILL) 64
		31, // job_skill(SURA SKILL) 65
		31, // job_skill(SURA SKILL) 66
		0, // common_skill 67
		0, // common_skill 68
		0, // common_skill 69
		0, // common_skill 70
		0, // common_skill 71
		0, // common_skill 72
		0, // common_skill 73
		0, // common_skill 74
		0, // common_skill 75
		32, // job_skill(SURA SKILL) 76
		32, // job_skill(SURA SKILL) 77
		32, // job_skill(SURA SKILL) 78
		32, // job_skill(SURA SKILL) 79
		32, // job_skill(SURA SKILL) 80
		32, // job_skill(SURA SKILL) 81
		0, // common_skill 82
		0, // common_skill 83
		0, // common_skill 84
		0, // common_skill 85
		0, // common_skill 86
		0, // common_skill 87
		0, // common_skill 88
		0, // common_skill 89
		0, // common_skill 90
		41, // job_skill(SHAMAN SKILL) 91
		41, // job_skill(SHAMAN SKILL) 92
		41, // job_skill(SHAMAN SKILL) 93
		41, // job_skill(SHAMAN SKILL) 94
		41, // job_skill(SHAMAN SKILL) 95
		41, // job_skill(SHAMAN SKILL) 96
		0, // common_skill 97
		0, // common_skill 98
		0, // common_skill 99
		0, // common_skill 100
		0, // common_skill 101
		0, // common_skill 102
		0, // common_skill 103
		0, // common_skill 104
		0, // common_skill 105
		42, // job_skill(SHAMAN SKILL) 106
		42, // job_skill(SHAMAN SKILL) 107
		42, // job_skill(SHAMAN SKILL) 108
		42, // job_skill(SHAMAN SKILL) 109
		42, // job_skill(SHAMAN SKILL) 110
		42, // job_skill(SHAMAN SKILL) 111
		0, // common_skill 112
		0, // common_skill 113
		0, // common_skill 114
		0, // common_skill 115
		0, // common_skill 116
		0, // common_skill 117
		0, // common_skill 118
		0, // common_skill 119
		0, // common_skill 120
		0, // common_skill 121
		0, // common_skill 122
		0, // common_skill 123
		0, // common_skill 124
		0, // common_skill 125
		0, // common_skill 126
		0, // common_skill 127
		0, // common_skill 128
		0, // common_skill 129
		0, // common_skill 130
		0, // common_skill 131
		0, // common_skill 132
		0, // common_skill 133
		0, // common_skill 134
		0, // common_skill 135
		0, // common_skill 136
		0, // job_skill 137
		0, // job_skill 138
		0, // job_skill 139
		0, // job_skill 140
		0, // common_skill 141
		0, // common_skill 142
		0, // common_skill 143
		0, // common_skill 144
		0, // common_skill 145
		0, // common_skill 146
		0, // common_skill 147
		0, // common_skill 148
		0, // common_skill 149
		0, // common_skill 150
		0, // common_skill 151
		0, // job_skill 152
		0, // job_skill 153
		0, // job_skill 154
		0, // job_skill 155
		0, // job_skill 156
		0, // job_skill 157
		0, // empty(reserved) 158
		0, // empty(reserved) 159
		0, // empty(reserved) 160
		0, // empty(reserved) 161
		0, // empty(reserved) 162
		0, // empty(reserved) 163
		0, // empty(reserved) 164
		0, // empty(reserved) 165
		0, // empty(reserved) 166
		0, // empty(reserved) 167
		0, // empty(reserved) 168
		0, // empty(reserved) 169
		51, // job_skill(WOLFMAN SKILL) 170
		51, // job_skill(WOLFMAN SKILL) 171
		51, // job_skill(WOLFMAN SKILL) 172
		51, // job_skill(WOLFMAN SKILL) 173
		51, // job_skill(WOLFMAN SKILL) 174
		51, // job_skill(WOLFMAN SKILL) 175
		0, // job_skill(WARRIOR CONQUEROR SKILL) 176
		0, // job_skill(ASSASSIN CONQUEROR SKILL) 177
		0, // job_skill(ASSASSIN CONQUEROR SKILL) 178
		0, // job_skill(SURA CONQUEROR SKILL) 179
		0, // job_skill(SURA CONQUEROR SKILL) 180
		0, // job_skill(SHAMAN CONQUEROR SKILL) 181
		0, // job_skill(SHAMAN CONQUEROR SKILL) 182
		0 // job_skill(WOLFMAN CONQUEROR SKILL) 183
	}; // s_anSkill2JobGroup

	const DWORD MOTION_MAX_NUM = 124;
	const DWORD SKILL_LIST_MAX_COUNT = 6;

	static DWORD s_anMotion2SkillVnumList[MOTION_MAX_NUM][SKILL_LIST_MAX_COUNT] =
	{
		//	스킬수	무사스킬ID	자객스킬ID	수라스킬ID	무당스킬ID	수인족(WOLFMAN) 스킬ID
		{	0,		0,			0,			0,			0,			0		}, // 0 (RESERVED)

		// 1번 직군 기본 스킬 SKILL GROUP 1 - TRAINING
		{	5,		1,			31,			61,			91,			170		}, // 1
		{	5,		2,			32,			62,			92,			171		}, // 2
		{	5,		3,			33,			63,			93,			172		}, // 3
		{	5,		4,			34,			64,			94,			173		}, // 4
		{	5,		5,			35,			65,			95,			174		}, // 5
		{	5,		6,			36,			66,			96,			175		}, // 6
		{	0,		0,			0,			0,			0,			0		}, // 7
		{	0,		0,			0,			0,			0,			0		}, // 8
		{	5,		176,		177,		179,		181,		183		}, // 9
		{	0,		0,			0,			0,			0,			0		}, // 10
		// 1번 직군 기본 스킬 끝 END

		// 여유분 MARGIN
		{	0,		0,			0,			0,			0,			0		}, // 11
		{	0,		0,			0,			0,			0,			0		}, // 12
		{	0,		0,			0,			0,			0,			0		}, // 13
		{	0,		0,			0,			0,			0,			0		}, // 14
		{	0,		0,			0,			0,			0,			0		}, // 15
		// 여유분 끝 END

		// 2번 직군 기본 스킬 SKILL GROUP 2 - TRAINING
		{	4,		16,			46,			76,			106,		0		}, // 16
		{	4,		17,			47,			77,			107,		0		}, // 17
		{	4,		18,			48,			78,			108,		0		}, // 18
		{	4,		19,			49,			79,			109,		0		}, // 19
		{	4,		20,			50,			80,			110,		0		}, // 20
		{	4,		21,			51,			81,			111,		0		}, // 21
		{	0,		0,			0,			0,			0,			0		}, // 22
		{	0,		0,			0,			0,			0,			0		}, // 23
		{	4,		176,		178,		180,		182,		0		}, // 24
		{	0,		0,			0,			0,			0,			0		}, // 25
		// 2번 직군 기본 스킬 끝 END

		// 1번 직군 마스터 스킬 SKILL GROUP 1 - MASTER
		{	5,		1,			31,			61,			91,			170		}, // 26
		{	5,		2,			32,			62,			92,			171		}, // 27
		{	5,		3,			33,			63,			93,			172		}, // 28
		{	5,		4,			34,			64,			94,			173		}, // 29
		{	5,		5,			35,			65,			95,			174		}, // 30
		{	5,		6,			36,			66,			96,			175		}, // 31
		{	0,		0,			0,			0,			0,			0		}, // 32
		{	0,		0,			0,			0,			0,			0		}, // 33
		{	5,		176,		177,		179,		181,		183		}, // 34
		{	0,		0,			0,			0,			0,			0		}, // 35
		// 1번 직군 마스터 스킬 끝 END

		// 여유분 MARGIN
		{	0,		0,			0,			0,			0,			0		}, // 36
		{	0,		0,			0,			0,			0,			0		}, // 37
		{	0,		0,			0,			0,			0,			0		}, // 38
		{	0,		0,			0,			0,			0,			0		}, // 39
		{	0,		0,			0,			0,			0,			0		}, // 40
		// 여유분 끝 END

		// 2번 직군 마스터 스킬 SKILL GROUP 2 - MASTER
		{	4,		16,			46,			76,			106,		0		}, // 41
		{	4,		17,			47,			77,			107,		0		}, // 42
		{	4,		18,			48,			78,			108,		0		}, // 43
		{	4,		19,			49,			79,			109,		0		}, // 44
		{	4,		20,			50,			80,			110,		0		}, // 45
		{	4,		21,			51,			81,			111,		0		}, // 46
		{	0,		0,			0,			0,			0,			0		}, // 47
		{	0,		0,			0,			0,			0,			0		}, // 48
		{	4,		176,		178,		180,		182,		0		}, // 49
		{	0,		0,			0,			0,			0,			0		}, // 50
		// 2번 직군 마스터 스킬 끝 END

		// 1번 직군 그랜드 마스터 스킬 SKILL GROUP 1 - GRAND MASTER
		{	5,		1,			31,			61,			91,			170		}, // 51
		{	5,		2,			32,			62,			92,			171		}, // 52
		{	5,		3,			33,			63,			93,			172		}, // 53
		{	5,		4,			34,			64,			94,			173		}, // 54
		{	5,		5,			35,			65,			95,			174		}, // 55
		{	5,		6,			36,			66,			96,			175		}, // 56
		{	0,		0,			0,			0,			0,			0		}, // 57
		{	0,		0,			0,			0,			0,			0		}, // 58
		{	5,		176,		177,		179,		181,		183		}, // 59
		{	0,		0,			0,			0,			0,			0		}, // 60
		// 1번 직군 그랜드 마스터 스킬 끝 END

		// 여유분 MARGIN
		{	0,		0,			0,			0,			0,			0		}, // 61
		{	0,		0,			0,			0,			0,			0		}, // 62
		{	0,		0,			0,			0,			0,			0		}, // 63
		{	0,		0,			0,			0,			0,			0		}, // 64
		{	0,		0,			0,			0,			0,			0		}, // 65
		// 여유분 끝 END

		// 2번 직군 그랜드 마스터 스킬 SKILL GROUP 2 - GRAND MASTER
		{	4,		16,			46,			76,			106,		0		}, // 66
		{	4,		17,			47,			77,			107,		0		}, // 67
		{	4,		18,			48,			78,			108,		0		}, // 68
		{	4,		19,			49,			79,			109,		0		}, // 69
		{	4,		20,			50,			80,			110,		0		}, // 70
		{	4,		21,			51,			81,			111,		0		}, // 71
		{	0,		0,			0,			0,			0,			0		}, // 72
		{	0,		0,			0,			0,			0,			0		}, // 73
		{	4,		176,		178,		180,		182,		0		}, // 74
		{	0,		0,			0,			0,			0,			0		}, // 75
		// 2번 직군 그랜드 마스터 스킬 끝 END

		// 1번 직군 퍼펙트 마스터 스킬 SKILL GROUP 1 - PERFECT MASTER
		{	5,		1,			31,			61,			91,			170		}, // 76
		{	5,		2,			32,			62,			92,			171		}, // 77
		{	5,		3,			33,			63,			93,			172		}, // 78
		{	5,		4,			34,			64,			94,			173		}, // 79
		{	5,		5,			35,			65,			95,			174		}, // 80
		{	5,		6,			36,			66,			96,			175		}, // 81
		{	0,		0,			0,			0,			0,			0		}, // 82
		{	0,		0,			0,			0,			0,			0		}, // 83
		{	5,		176,		177,		179,		181,		183		}, // 84
		{	0,		0,			0,			0,			0,			0		}, // 85
		// 1번 직군 퍼펙트 마스터 스킬 끝 END

		// 여유분 MARGIN
		{	0,		0,			0,			0,			0,			0		}, // 86
		{	0,		0,			0,			0,			0,			0		}, // 87
		{	0,		0,			0,			0,			0,			0		}, // 88
		{	0,		0,			0,			0,			0,			0		}, // 89
		{	0,		0,			0,			0,			0,			0		}, // 90
		// 여유분 끝 END

		// 2번 직군 퍼펙트 마스터 스킬 SKILL GROUP 2 - PERFECT MASTER
		{	4,		16,			46,			76,			106,		0		}, // 91
		{	4,		17,			47,			77,			107,		0		}, // 92
		{	4,		18,			48,			78,			108,		0		}, // 93
		{	4,		19,			49,			79,			109,		0		}, // 94
		{	4,		20,			50,			80,			110,		0		}, // 95
		{	4,		21,			51,			81,			111,		0		}, // 96
		{	0,		0,			0,			0,			0,			0		}, // 97
		{	0,		0,			0,			0,			0,			0		}, // 98
		{	4,		176,		178,		180,		182,		0		}, // 99
		{	0,		0,			0,			0,			0,			0		}, // 100
		// 2번 직군 퍼펙트 마스터 스킬 끝 END

		// 길드 스킬 GUILD SKILLS
		{	1,		152,		0,			0,			0,			0		}, // 101
		{	1,		153,		0,			0,			0,			0		}, // 102
		{	1,		154,		0,			0,			0,			0		}, // 103
		{	1,		155,		0,			0,			0,			0		}, // 104
		{	1,		156,		0,			0,			0,			0		}, // 105
		{	1,		157,		0,			0,			0,			0		}, // 106
		// 길드 스킬 끝 END

		// 여유분 MARGIN
		{	0,		0,			0,			0,			0,			0		}, // 107
		{	0,		0,			0,			0,			0,			0		}, // 108
		{	0,		0,			0,			0,			0,			0		}, // 109
		{	0,		0,			0,			0,			0,			0		}, // 110
		{	0,		0,			0,			0,			0,			0		}, // 111
		{	0,		0,			0,			0,			0,			0		}, // 112
		{	0,		0,			0,			0,			0,			0		}, // 113
		{	0,		0,			0,			0,			0,			0		}, // 114
		{	0,		0,			0,			0,			0,			0		}, // 115
		{	0,		0,			0,			0,			0,			0		}, // 116
		{	0,		0,			0,			0,			0,			0		}, // 117
		{	0,		0,			0,			0,			0,			0		}, // 118
		{	0,		0,			0,			0,			0,			0		}, // 119
		{	0,		0,			0,			0,			0,			0		}, // 120
		// 여유분 끝 END

		// 승마 스킬 HORSE SKILLS
		{	2,		137,		140,		0,			0,			0		}, // 121
		{	1,		138,		0,			0,			0,			0		}, // 122
		{	1,		139,		0,			0,			0,			0		}, // 123
		// 승마 스킬 끝 END
	};

	if (dwMotionIndex >= MOTION_MAX_NUM)
	{
		sys_err("OUT_OF_MOTION_VNUM: name=%s, motion=%d/%d", GetName(), dwMotionIndex, MOTION_MAX_NUM);
		return false;
	}

	DWORD* skillVNums = s_anMotion2SkillVnumList[dwMotionIndex];

	DWORD skillCount = *skillVNums++;
	if (skillCount >= SKILL_LIST_MAX_COUNT)
	{
		sys_err("OUT_OF_SKILL_LIST: name=%s, count=%d/%d", GetName(), skillCount, SKILL_LIST_MAX_COUNT);
		return false;
	}

	for (DWORD skillIndex = 0; skillIndex != skillCount + 2; ++skillIndex)
	{
		if (skillIndex >= SKILL_MAX_NUM)
		{
			sys_err("OUT_OF_SKILL_VNUM: name=%s, skill=%d/%d", GetName(), skillIndex, SKILL_MAX_NUM);
			return false;
		}

		DWORD eachSkillVNum = skillVNums[skillIndex];

		if (eachSkillVNum != 0)
		{
			DWORD eachJobGroup = s_anSkill2JobGroup[eachSkillVNum];

			if (0 == eachJobGroup || eachJobGroup == selfJobGroup)
			{
				// GUILDSKILL_BUG_FIX
				DWORD eachSkillLevel = 0;

				if (eachSkillVNum >= GUILD_SKILL_START && eachSkillVNum <= GUILD_SKILL_END)
				{
					if (GetGuild())
						eachSkillLevel = GetGuild()->GetSkillLevel(eachSkillVNum);
					else
						eachSkillLevel = 0;
				}
#if defined(__9TH_SKILL__)
				// CONQUEROR_SKILL_BUG_FIX
				else if (eachSkillVNum >= SKILL_FINISH && eachSkillVNum <= SKILL_ILIPUNGU)
				{
					eachSkillLevel = GetSkillLevel(eachSkillVNum);
				}
				// END_OF_CONQUEROR_SKILL_BUG_FIX
#endif
				else
				{
					eachSkillLevel = GetSkillLevel(eachSkillVNum);

					if (test_server)
					{
						if (eachSkillLevel <= 0)
						{
							sys_err("SKILL_LEVEL: name=%s, skill=%d, level=%d", GetName(), skillIndex, eachSkillLevel);
							return false;
						}
					}
				}

				if (eachSkillLevel > 0)
				{
					return true;
				}

				// END_OF_GUILDSKILL_BUG_FIX
			}
		}
	}

	return false;
}

void CHARACTER::ClearSkill()
{
	PointChange(POINT_SKILL, 4 + (GetLevel() - 5) - GetPoint(POINT_SKILL));

	ResetSkill();
}

#if defined(__SKILL_COOLTIME_UPDATE__)
void CHARACTER::ResetSkillCoolTimes()
{
	if (!GetSkillGroup() || m_SkillUseInfo.empty())
		return;

	for (std::map<int, TSkillUseInfo>::iterator it = m_SkillUseInfo.begin(); it != m_SkillUseInfo.end(); ++it)
		it->second.dwNextSkillUsableTime = 0;
}
#endif

void CHARACTER::ClearSubSkill()
{
	PointChange(POINT_SUB_SKILL, GetLevel() < 10 ? 0 : (GetLevel() - 9) - GetPoint(POINT_SUB_SKILL));

	if (m_pSkillLevels == NULL)
	{
		sys_err("m_pSkillLevels nil (name: %s)", GetName());
		return;
	}

	TPlayerSkill CleanSkill;
	memset(&CleanSkill, 0, sizeof(TPlayerSkill));

	size_t count = sizeof(s_adwSubSkillVnums) / sizeof(s_adwSubSkillVnums[0]);

	for (size_t i = 0; i < count; ++i)
	{
		if (s_adwSubSkillVnums[i] >= SKILL_MAX_NUM)
			continue;

		m_pSkillLevels[s_adwSubSkillVnums[i]] = CleanSkill;
	}

	ComputePoints();
	SkillLevelPacket();
}

bool CHARACTER::ResetOneSkill(DWORD dwVnum)
{
	if (NULL == m_pSkillLevels)
	{
		sys_err("m_pSkillLevels nil (name %s, vnum %u)", GetName(), dwVnum);
		return false;
	}

	if (dwVnum >= SKILL_MAX_NUM)
	{
		sys_err("vnum overflow (name %s, vnum %u)", GetName(), dwVnum);
		return false;
	}

	BYTE level = m_pSkillLevels[dwVnum].bLevel;

	m_pSkillLevels[dwVnum].bLevel = 0;
	m_pSkillLevels[dwVnum].bMasterType = 0;
	m_pSkillLevels[dwVnum].tNextRead = 0;

	if (level > 17)
		level = 17;

	PointChange(POINT_SKILL, level);

	LogManager::instance().CharLog(this, dwVnum, "ONE_SKILL_RESET_BY_SCROLL", "");

	ComputePoints();
	SkillLevelPacket();

	return true;
}

bool CHARACTER::CanUseSkill(DWORD dwSkillVnum) const
{
	if (0 == dwSkillVnum) return false;

	if (0 < GetSkillGroup())
	{
		const int SKILL_COUNT = 7;
		static const DWORD SkillList[JOB_MAX_NUM][SKILL_GROUP_MAX_NUM][SKILL_COUNT] =
		{
			{ { 1, 2, 3, 4, 5, 6, 176 }, { 16, 17, 18, 19, 20, 21, 176 } },
			{ { 31, 32, 33, 34, 35, 36, 177 }, { 46, 47, 48, 49, 50, 51, 178 } },
			{ { 61, 62, 63, 64, 65, 66, 179 }, { 76, 77, 78, 79, 80, 81, 180 } },
			{ { 91, 92, 93, 94, 95, 96, 181 }, { 106, 107, 108, 109, 110, 111, 182 } },
			{ { 170, 171, 172, 173, 174, 175, 183 }, { 0, 0, 0, 0, 0, 0, 0 } },
		};

		const DWORD* pSkill = SkillList[GetJob()][GetSkillGroup() - 1];

		for (int i = 0; i < SKILL_COUNT; ++i)
		{
			if (pSkill[i] == dwSkillVnum) return true;
		}
	}

	//if (true == IsHorseRiding())

	if (true == IsRiding())
	{
		//마운트 탈것중 고급말만 스킬 사용가능
		/*
		if (GetMountVnum())
		{
			if (!((GetMountVnum() >= 20209 && GetMountVnum() <= 20212) ||
				GetMountVnum() == 20215 || GetMountVnum() == 20218 || GetMountVnum() == 20225))
				return false;
		}
		*/

		switch (dwSkillVnum)
		{
			case SKILL_HORSE_WILDATTACK:
			case SKILL_HORSE_CHARGE:
			case SKILL_HORSE_ESCAPE:
			case SKILL_HORSE_WILDATTACK_RANGE:
				return true;
		}
	}

	switch (dwSkillVnum)
	{
		case SKILL_LEADERSHIP:
		case SKILL_COMBO:
		case SKILL_CREATE:
		case SKILL_MINING:
		case SKILL_LANGUAGE1:
		case SKILL_LANGUAGE2:
		case SKILL_LANGUAGE3:
		case SKILL_POLYMORPH:
		case SKILL_HORSE:
		case SKILL_HORSE_SUMMON:
#if defined(__PARTY_PROFICY__)
		case SKILL_ROLE_PROFICIENCY:
#endif
#if defined(__PARTY_INSIGHT__)
		case SKILL_INSIGHT:
#endif
		case SKILL_HIT:
		case SKILL_HORSE_WILDATTACK:
		case SKILL_HORSE_CHARGE:
		case SKILL_HORSE_ESCAPE:
		case SKILL_HORSE_WILDATTACK_RANGE:
		case SKILL_ADD_HP:
		case SKILL_RESIST_PENETRATE:
		case GUILD_SKILL_EYE:
		case GUILD_SKILL_BLOOD:
		case GUILD_SKILL_BLESS:
		case GUILD_SKILL_SEONGHWI:
		case GUILD_SKILL_ACCEL:
		case GUILD_SKILL_BUNNO:
		case GUILD_SKILL_JUMUN:
		case GUILD_SKILL_TELEPORT:
		case GUILD_SKILL_DOOR:
			return true;
	}

	return false;
}

bool CHARACTER::CheckSkillHitCount(const BYTE SkillID, const VID TargetVID)
{
	std::map<int, TSkillUseInfo>::iterator iter = m_SkillUseInfo.find(SkillID);

	if (iter == m_SkillUseInfo.end())
	{
		sys_log(0, "SkillHack: Skill(%u) is not in container", SkillID);
		return false;
	}

	TSkillUseInfo& rSkillUseInfo = iter->second;

	if (false == rSkillUseInfo.bUsed)
	{
		sys_log(0, "SkillHack: not used skill(%u)", SkillID);
		return false;
	}

	switch (SkillID)
	{
		case SKILL_YONGKWON:
		case SKILL_HWAYEOMPOK:
		case SKILL_DAEJINGAK:
		case SKILL_PAERYONG:
#if defined(__9TH_SKILL__)
		case SKILL_METEO:
#endif
			sys_log(0, "SkillHack: cannot use attack packet for skill(%u)", SkillID);
			return false;
	}

	auto iterTargetMap = rSkillUseInfo.TargetVIDMap.find(TargetVID);

	if (rSkillUseInfo.TargetVIDMap.end() != iterTargetMap)
	{
		size_t MaxAttackCountPerTarget = 1;

		switch (SkillID)
		{
			case SKILL_SAMYEON:
			case SKILL_CHARYUN:
			case SKILL_CHAYEOL:
				MaxAttackCountPerTarget = 3;
				break;

			case SKILL_HORSE_WILDATTACK_RANGE:
				MaxAttackCountPerTarget = 5;
				break;

			case SKILL_YEONSA:
				MaxAttackCountPerTarget = 7;
				break;

			case SKILL_HORSE_ESCAPE:
				MaxAttackCountPerTarget = 10;
				break;
		}

		if (iterTargetMap->second >= MaxAttackCountPerTarget)
		{
			sys_log(0, "SkillHack: Too Many Hit count from SkillID(%u) count(%u)", SkillID, iterTargetMap->second);
			return false;
		}

		iterTargetMap->second++;
	}
	else
	{
		rSkillUseInfo.TargetVIDMap.insert(std::make_pair(TargetVID, 1));
	}

	return true;
}
