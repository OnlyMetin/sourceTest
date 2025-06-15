#ifndef __INC_COMMON_VNUM_HELPER_H__
#define __INC_COMMON_VNUM_HELPER_H__

#include "service.h"
#include "length.h"

/**
	�̹� �����ϰų� ������ �߰��� ������, �� ���� �ҽ����� �ĺ��� �� ����� ���
	�ĺ���(����=VNum)�� �ϵ��ڵ��ϴ� ������� �Ǿ��־ �������� �ſ� �������µ�

	�����δ� �ҽ��� ���� � ������(Ȥ�� ��)���� �� �� �ְ� ���ڴ� ��ö���� �������� �߰�.

		* �� ������ ������ ���������� ����Ǵµ� PCH�� ������ �ٲ� ������ ��ü ������ �ؾ��ϴ�
		�ϴ��� �ʿ��� cpp���Ͽ��� include �ؼ� ������ ����.

		* cpp���� �����ϸ� ������ ~ ��ũ�ؾ��ϴ� �׳� common�� ����� �־���. (game, db������Ʈ �� �� ��� ����)

	@date 2011.8.29
**/

class CItemVnumHelper
{
public:
#if defined(__PET_SYSTEM__)
	/// ���� DVD�� �һ��� ��ȯ��
	static const bool IsPhoenix(DWORD vnum) { return 53001 == vnum; } // NOTE: �һ��� ��ȯ �������� 53001 ������ mob-vnum�� 34001 �Դϴ�.
#endif

	/// �󸶴� �̺�Ʈ �ʽ´��� ���� (������ �󸶴� �̺�Ʈ�� Ư�� �������̾����� ������ ���� �������� ��Ȱ���ؼ� ��� ���ٰ� ��)
	static const bool IsRamadanMoonRing(DWORD vnum) { return 71135 == vnum; }

	/// �ҷ��� ���� (������ �ʽ´��� ������ ����)
	static const bool IsHalloweenCandy(DWORD vnum) { return 71136 == vnum; }

	/// ũ�������� �ູ�� ����
	static const bool IsHappinessRing(DWORD vnum) { return 71143 == vnum; }

	/// �߷�Ÿ�� ����� �Ҵ�Ʈ 
	static const bool IsLovePendant(DWORD vnum) { return 71145 == vnum; }

	/// Magic Ring
	static const bool IsMagicRing(DWORD vnum) { return 71148 == vnum || 71149 == vnum; }

	/// Easter Candy
	static const bool IsEasterCandy(DWORD vnum) { return 71188 == vnum; }

	/// Chocolate Pendant
	static const bool IsChocolatePendant(DWORD vnum) { return 71199 == vnum; }

	/// Nazar Pendant
	static const bool IsNazarPendant(DWORD vnum) { return 71202 == vnum; }

	/// Guardian Pendant
	static const bool IsGuardianPendant(DWORD vnum) { return 72054 == vnum; }

	/// Gem Pendant
	static const bool IsGemCandy(DWORD vnum) { return 76030 == vnum || 76047 == vnum; }

	// Unique Items
	static const bool IsUniqueItem(DWORD vnum)
	{
		switch (vnum)
		{
			case 71135: // IsRamadanMoonRing
			case 71136: // IsHalloweenCandy
			case 71143: // IsHappinessRing
			case 71145: // IsLovePendant

			case 71148: // IsMagicRing
			case 71149: // IsMagicRing

			case 71188: // IsEasterCandy
			case 71199: // IsChocolatePendant
			case 71202: // IsNazarPendant
			case 72054: // IsGuardianPendant

			case 76030: // IsGemCandy
			case 76047: // IsGemCandy
				return true;
		}
		return false;
	}

	static const bool IsDragonSoul(DWORD vnum)
	{
		return (vnum >= 110000 && vnum <= 165400);
	}
};

class CMobVnumHelper
{
public:
#if defined(__PET_SYSTEM__)
	/// ���� DVD�� �һ��� �� ��ȣ
	static const bool IsPhoenix(DWORD vnum) { return 34001 == vnum; }
	static const bool IsIcePhoenix(DWORD vnum) { return 34003 == vnum; }

	/// 2011�� ũ�������� �̺�Ʈ�� �� (�Ʊ� ����)
	static const bool IsReindeerYoung(DWORD vnum) { return 34002 == vnum; }

	/// PetSystem �� �����ϴ� ���ΰ�?
	static const bool IsPetUsingPetSystem(DWORD vnum) { return (IsPhoenix(vnum) || IsReindeerYoung(vnum)) || IsIcePhoenix(vnum); }
#endif

	/// �󸶴� �̺�Ʈ ����� �渶(20119) .. �ҷ��� �̺�Ʈ�� �󸶴� �渶 Ŭ��(������ ����, 20219)
	static const bool IsRamadanBlackHorse(DWORD vnum) { return 20119 == vnum || 20219 == vnum || 22022 == vnum; }
};

class CVnumHelper
{
};

#endif // __INC_COMMON_VNUM_HELPER_H__
