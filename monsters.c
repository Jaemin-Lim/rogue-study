/*
 * File with various monster functions in it
 *
 * @(#)monsters.c	4.46 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

/*
 * 【파일 개요】
 * 이 파일은 Rogue 던전 게임의 몬스터 관련 함수들을 모두 담고 있습니다.
 * 주요 기능:
 *   - randmonster()  : 현재 던전 레벨에 맞는 몬스터 종류를 무작위로 선택
 *   - new_monster()  : 새 몬스터를 생성하여 mlist(몬스터 연결 리스트)에 추가
 *   - exp_add()      : 몬스터를 처치했을 때 추가 경험치를 계산
 *   - wanderer()     : 배회 몬스터를 생성해 플레이어 쪽으로 향하게 함
 *   - wake_monster() : 플레이어가 몬스터 옆을 지나갈 때 몬스터를 깨움
 *   - give_pack()    : 몬스터에게 아이템 꾸러미(pack)를 줄지 결정
 *   - save_throw()   : 특정 생물이 마법/독 등에 대한 저항 굴림을 수행
 *   - save()         : 플레이어가 각종 위험에 대한 저항 굴림을 수행
 *
 * 핵심 자료구조:
 *   - mlist          : 현재 층에 존재하는 모든 몬스터의 연결 리스트
 *   - monsters[]     : 알파벳 A~Z 총 26종 몬스터의 기본 스탯 배열
 *   - moat(y,x)      : 맵 좌표 (y,x)에 있는 몬스터 포인터를 반환하는 매크로
 */

#include <curses.h>
#include <string.h>
#include "rogue.h"
#include <ctype.h>

/*
 * List of monsters in rough order of vorpalness
 * 몬스터 목록 - 대략 위험도(강함) 순으로 나열되어 있음
 * 던전 레벨이 낮을수록 앞쪽(약한) 몬스터가, 높을수록 뒤쪽(강한) 몬스터가 등장
 */
static char lvl_mons[] =  {
    'K', 'E', 'B', 'S', 'H', 'I', 'R', 'O', 'Z', 'L', 'C', 'Q', 'A',
    'N', 'Y', 'F', 'T', 'W', 'P', 'X', 'U', 'M', 'V', 'G', 'J', 'D'
};

/* 배회 몬스터(wanderer) 전용 목록 - 일부 몬스터(0 항목)는 배회하지 않음 */
static char wand_mons[] = {
    'K', 'E', 'B', 'S', 'H',   0, 'R', 'O', 'Z',   0, 'C', 'Q', 'A',
      0, 'Y',   0, 'T', 'W', 'P',   0, 'U', 'M', 'V', 'G', 'J',   0
};

/*
 * randmonster:
 *	Pick a monster to show up.  The lower the level,
 *	the meaner the monster.
 *
 * 【역할】 현재 던전 레벨에 맞는 몬스터 문자(A~Z)를 무작위로 선택한다.
 * 【매개변수】
 *   wander - TRUE이면 배회 몬스터 목록(wand_mons)에서 선택,
 *            FALSE이면 일반 레벨 목록(lvl_mons)에서 선택
 * 【반환값】 선택된 몬스터를 나타내는 문자 (예: 'K', 'D' 등)
 * 【동작 원리】
 *   현재 레벨(level)에 ±6 범위의 난수를 더해 인덱스를 계산한다.
 *   wand_mons에서 0인 항목(배회 불가 몬스터)은 건너뛴다.
 */
char
randmonster(bool wander)
{
    int d;      /* 몬스터 배열의 인덱스 */
    char *mons; /* 사용할 몬스터 목록 포인터 */

    mons = (wander ? wand_mons : lvl_mons);
    do
    {
	d = level + (rnd(10) - 6); /* 현재 레벨을 기준으로 ±6 범위 내에서 인덱스 선택 */
	if (d < 0)
	    d = rnd(5);      /* 음수가 되면 0~4 범위로 보정 (약한 몬스터) */
	if (d > 25)
	    d = rnd(5) + 21; /* 25 초과이면 21~25 범위로 보정 (강한 몬스터) */
    } while (mons[d] == 0); /* 배회 불가 항목(0)은 다시 뽑음 */
    return mons[d];
}

/*
 * new_monster:
 *	Pick a new monster and add it to the list
 *
 * 【역할】 새 몬스터를 생성하고 mlist(몬스터 연결 리스트)에 추가한다.
 *         몬스터의 각종 스탯을 초기화하고 맵에 배치한다.
 * 【매개변수】
 *   tp   - 새로 생성할 몬스터의 THING 구조체 포인터
 *   type - 몬스터 종류를 나타내는 문자 (예: 'D'=드래곤, 'K'=코볼트)
 *   cp   - 몬스터를 배치할 좌표 (coord 포인터)
 */

void
new_monster(THING *tp, char type, coord *cp)
{
    struct monster *mp; /* monsters[] 배열에서 해당 몬스터 기본 정보 포인터 */
    int lev_add;        /* 아뮬렛 레벨 이후 추가되는 레벨 보정값 */

    /* 아뮬렛 레벨(AMULETLEVEL) 이후 층이면 몬스터가 더 강해짐 */
    if ((lev_add = level - AMULETLEVEL) < 0)
	lev_add = 0;
    attach(mlist, tp);          /* mlist에 새 몬스터를 연결 */
    tp->t_type = type;          /* 몬스터 종류 문자 설정 */
    tp->t_disguise = type;      /* 변장 문자 (Xeroc는 나중에 변경됨) */
    tp->t_pos = *cp;            /* 맵 상의 위치 */
    move(cp->y, cp->x);
    tp->t_oldch = CCHAR( inch() ); /* 몬스터가 서 있던 자리의 원래 화면 문자 저장 */
    tp->t_room = roomin(cp);    /* 몬스터가 속한 방 포인터 */
    moat(cp->y, cp->x) = tp;   /* 맵 위치에 몬스터 포인터 등록 */
    mp = &monsters[tp->t_type-'A']; /* 몬스터 종류에 해당하는 기본 스탯 참조 */
    /* 던전 레벨 보정을 포함한 스탯 초기화 */
    tp->t_stats.s_lvl = mp->m_stats.s_lvl + lev_add;
    tp->t_stats.s_maxhp = tp->t_stats.s_hpt = roll(tp->t_stats.s_lvl, 8); /* HP: 레벨 × d8 */
    tp->t_stats.s_arm = mp->m_stats.s_arm - lev_add; /* 깊은 층일수록 방어력이 낮아져 더 강해짐 */
    strcpy(tp->t_stats.s_dmg,mp->m_stats.s_dmg); /* 공격 피해 문자열 복사 */
    tp->t_stats.s_str = mp->m_stats.s_str;
    tp->t_stats.s_exp = mp->m_stats.s_exp + lev_add * 10 + exp_add(tp); /* 처치 시 경험치 */
    tp->t_flags = mp->m_flags;  /* 몬스터 속성 플래그 복사 */
    if (level > 29)
	tp->t_flags |= ISHASTE; /* 29층 이후에는 모든 몬스터가 빠르게 이동 */
    tp->t_turn = TRUE;          /* 이번 턴에 행동 가능 */
    tp->t_pack = NULL;          /* 소지품 목록 초기화 */
    if (ISWEARING(R_AGGR))
	runto(cp);              /* 플레이어가 R_AGGR(적대) 반지를 끼고 있으면 즉시 추격 */
    if (type == 'X')
	tp->t_disguise = rnd_thing(); /* Xeroc는 임의 아이템으로 변장 */
}

/*
 * expadd:
 *	Experience to add for this monster's level/hit points
 *
 * 【역할】 몬스터의 레벨과 최대 HP를 기반으로 처치 시 추가 경험치를 계산한다.
 * 【매개변수】
 *   tp - 경험치를 계산할 몬스터의 THING 포인터
 * 【반환값】 추가 경험치 값 (정수)
 * 【동작 원리】
 *   레벨 1이면 최대HP/8, 그 외엔 최대HP/6 을 기본값으로 삼고,
 *   레벨 7 이상이면 ×4, 레벨 10 이상이면 ×20 의 배율을 적용한다.
 */
int
exp_add(THING *tp)
{
    int mod; /* 경험치 배율 기준값 */

    if (tp->t_stats.s_lvl == 1)
	mod = tp->t_stats.s_maxhp / 8;  /* 레벨 1 몬스터: 최대HP / 8 */
    else
	mod = tp->t_stats.s_maxhp / 6;  /* 레벨 2 이상: 최대HP / 6 */
    if (tp->t_stats.s_lvl > 9)
	mod *= 20;  /* 레벨 10 이상: 20배 (강력한 고레벨 몬스터) */
    else if (tp->t_stats.s_lvl > 6)
	mod *= 4;   /* 레벨 7~9: 4배 */
    return mod;
}

/*
 * wanderer:
 *	Create a new wandering monster and aim it at the player
 *
 * 【역할】 배회 몬스터를 새로 생성하고 플레이어를 향해 이동하도록 설정한다.
 *         생성 위치는 플레이어가 현재 있는 방(proom)이 아닌 곳으로 선택한다.
 * 【동작 원리】
 *   1. 빈 바닥 위치를 찾되, 플레이어의 현재 방과 다른 방이어야 함
 *   2. wand_mons 목록에서 배회 가능한 몬스터를 무작위로 선택해 생성
 *   3. SEEMONST 상태라면 화면에 몬스터를 표시 (환각 상태면 무작위 문자)
 *   4. runto()로 플레이어 방향으로 추격 개시
 */

void
wanderer()
{
    THING *tp;       /* 새로 생성할 배회 몬스터 */
    static coord cp; /* 몬스터를 배치할 좌표 */

    tp = new_item();
    do
    {
	find_floor((struct room *) NULL, &cp, FALSE, TRUE);
    } while (roomin(&cp) == proom); /* 플레이어 현재 방이 아닌 곳을 선택 */
    new_monster(tp, randmonster(TRUE), &cp); /* 배회 몬스터 목록에서 선택해 생성 */
    if (on(player, SEEMONST))
    {
	standout();
	if (!on(player, ISHALU))
	    addch(tp->t_type);       /* 실제 몬스터 문자 표시 */
	else
	    addch(rnd(26) + 'A');    /* 환각 상태: 무작위 알파벳 표시 */
	standend();
    }
    runto(&tp->t_pos); /* 플레이어를 향해 추격 시작 */
#ifdef MASTER
    if (wizard)
	msg("started a wandering %s", monsters[tp->t_type-'A'].m_name);
#endif
}

/*
 * wake_monster:
 *	What to do when the hero steps next to a monster
 *
 * 【역할】 플레이어가 몬스터 옆 칸으로 이동할 때 호출된다.
 *         몬스터를 깨워 플레이어를 추격하게 하거나, 특수 효과를 발동한다.
 * 【매개변수】
 *   y, x - 몬스터가 있는 맵 좌표
 * 【반환값】 해당 위치의 몬스터 THING 포인터
 * 【동작 원리】
 *   1. ISMEAN(사나운) 몬스터는 높은 확률로 추격(ISRUN) 상태가 됨
 *      단, 이미 추격 중이거나, 플레이어가 STEALTH/LEVITATION 상태이면 제외
 *   2. 'M'(메두사류) 몬스터는 시선으로 플레이어를 혼란(ISHUH) 상태로 만들 수 있음
 *      - 밝은 방이거나 램프 범위 안에 있을 때만 발동
 *      - save(VS_MAGIC)에 실패하면 혼란 지속 퓨즈(fuse)가 설치됨
 *   3. ISGREED(탐욕스러운) 몬스터는 방의 금화를 지키려 함
 */
THING *
wake_monster(int y, int x)
{
    THING *tp;           /* 깨울 몬스터의 THING 포인터 */
    struct room *rp;     /* 현재 플레이어의 방 포인터 */
    char ch, *mname;     /* 몬스터 종류 문자, 몬스터 이름 문자열 */

#ifdef MASTER
    if ((tp = moat(y, x)) == NULL)
	msg("can't find monster in wake_monster");
#else
    tp = moat(y, x);
    if (tp == NULL) 	 	 
	endwin(), abort(); 
#endif
    ch = tp->t_type;
    /*
     * Every time he sees mean monster, it might start chasing him
     * 사나운(ISMEAN) 몬스터를 발견하면 높은 확률로 추격을 시작함
     * 단, 이미 추격 중(ISRUN)이거나, 붙잡혀 있거나(ISHELD),
     * 플레이어가 은신(STEALTH) 또는 부유(LEVITATION) 상태이면 제외
     */
    if (!on(*tp, ISRUN) && rnd(3) != 0 && on(*tp, ISMEAN) && !on(*tp, ISHELD)
	&& !ISWEARING(R_STEALTH) && !on(player, ISLEVIT))
    {
	tp->t_dest = &hero;     /* 추격 목표를 플레이어로 설정 */
	tp->t_flags |= ISRUN;   /* 추격(달리기) 상태 플래그 설정 */
    }
    /* 'M' 몬스터(메두사 등)의 시선 혼란 효과 처리 */
    if (ch == 'M' && !on(player, ISBLIND) && !on(player, ISHALU)
	&& !on(*tp, ISFOUND) && !on(*tp, ISCANC) && on(*tp, ISRUN))
    {
        rp = proom;
	/* 밝은 방이거나 램프 범위(LAMPDIST) 안에 있을 때만 효과 발동 */
	if ((rp != NULL && !(rp->r_flags & ISDARK))
	    || dist(y, x, hero.y, hero.x) < LAMPDIST)
	{
	    tp->t_flags |= ISFOUND; /* 이미 발견됐다고 표시 (중복 발동 방지) */
	    if (!save(VS_MAGIC))    /* 마법 저항 실패 시 혼란 상태 부여 */
	    {
		if (on(player, ISHUH))
		    lengthen(unconfuse, spread(HUHDURATION)); /* 이미 혼란 중이면 지속시간 연장 */
		else
		    fuse(unconfuse, 0, spread(HUHDURATION), AFTER); /* 혼란 해제 퓨즈 설치 */
		player.t_flags |= ISHUH; /* 플레이어 혼란 상태 설정 */
		mname = set_mname(tp);
		addmsg("%s", mname);
		if (strcmp(mname, "it") != 0)
		    addmsg("'");
		msg("s gaze has confused you");
	    }
	}
    }
    /*
     * Let greedy ones guard gold
     * 탐욕스러운(ISGREED) 몬스터는 방의 금화를 목표로 삼음
     * 방에 금화가 없으면 플레이어를 직접 추격
     */
    if (on(*tp, ISGREED) && !on(*tp, ISRUN))
    {
	tp->t_flags |= ISRUN;
	if (proom->r_goldval)
	    tp->t_dest = &proom->r_gold; /* 방의 금화 위치를 목표로 */
	else
	    tp->t_dest = &hero;          /* 금화가 없으면 플레이어를 목표로 */
    }
    return tp;
}

/*
 * give_pack:
 *	Give a pack to a monster if it deserves one
 *
 * 【역할】 조건을 만족하는 몬스터에게 소지품(아이템 꾸러미)을 부여한다.
 *         현재 층이 최대 방문 층(max_level) 이상이고,
 *         해당 몬스터 종류의 m_carry 확률에 따라 새 아이템을 생성하여 추가한다.
 * 【매개변수】
 *   tp - 소지품을 줄 몬스터의 THING 포인터
 */

void
give_pack(THING *tp)
{
    /* 현재 층 >= 최대 방문 층 이고, m_carry 확률 통과 시 아이템 부여 */
    if (level >= max_level && rnd(100) < monsters[tp->t_type-'A'].m_carry)
	attach(tp->t_pack, new_thing()); /* 새 아이템을 생성해 몬스터 소지품에 연결 */
}

/*
 * save_throw:
 *	See if a creature save against something
 *
 * 【역할】 특정 생물(tp)이 마법/독/罠 등에 저항하는지 굴림을 수행한다.
 * 【매개변수】
 *   which - 저항 종류 (VS_MAGIC, VS_POISON 등의 상수)
 *   tp    - 저항 굴림을 하는 생물의 THING 포인터
 * 【반환값】 저항 성공이면 1(TRUE), 실패이면 0(FALSE)
 * 【동작 원리】
 *   필요 수치(need) = 14 + which - (생물 레벨 / 2)
 *   1d20 굴림 결과가 need 이상이면 성공
 */
int
save_throw(int which, THING *tp)
{
    int need; /* 저항 성공에 필요한 최소 주사위 값 */

    need = 14 + which - tp->t_stats.s_lvl / 2;
    return (roll(1, 20) >= need);
}

/*
 * save:
 *	See if he saves against various nasty things
 *
 * 【역할】 플레이어가 각종 위험(마법, 독 등)에 대해 저항 굴림을 수행한다.
 * 【매개변수】
 *   which - 저항 종류 (VS_MAGIC 등)
 * 【반환값】 저항 성공이면 1(TRUE), 실패이면 0(FALSE)
 * 【동작 원리】
 *   마법(VS_MAGIC)에 대한 저항일 경우, 보호 반지(R_PROTECT)의 bonus를 차감하여
 *   저항 수치를 낮춤(더 쉽게 통과). 이후 save_throw()로 실제 판정을 위임.
 */
int
save(int which)
{
    if (which == VS_MAGIC)
    {
	/* 왼손/오른손의 보호 반지 착용 시 마법 저항 수치를 낮춤 */
	if (ISRING(LEFT, R_PROTECT))
	    which -= cur_ring[LEFT]->o_arm;
	if (ISRING(RIGHT, R_PROTECT))
	    which -= cur_ring[RIGHT]->o_arm;
    }
    return save_throw(which, &player);
}
