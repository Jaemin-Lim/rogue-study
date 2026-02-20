/*
 * Function(s) for dealing with potions
 *
 * @(#)potions.c	4.46 (Berkeley) 06/07/83
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

/*
 * [파일 개요]
 * 이 파일은 포션(물약) 효과를 처리하는 함수들을 포함합니다.
 * 플레이어가 포션을 마시면 quaff()가 호출되어 포션 종류에 따른 효과를 적용합니다.
 * 각 포션은 p_actions[] 테이블에 정의된 플래그·데몬·지속시간·메시지를 사용합니다.
 * 주요 포션 효과: 혼란(P_CONFUSE), 독(P_POISON), 체력회복(P_HEALING),
 *   근력강화(P_STRENGTH), 몬스터 탐지(P_MFIND), 마법 탐지(P_TFIND),
 *   환각(P_LSD), 투명 감지(P_SEEINVIS), 레벨상승(P_RAISE),
 *   대폭 회복(P_XHEAL), 가속(P_HASTE), 근력 복원(P_RESTORE),
 *   실명(P_BLIND), 공중부양(P_LEVIT).
 */

#include <curses.h>
#include <ctype.h>
#include "rogue.h"

/* 포션 행동(action) 구조체:
 * pa_flags   - 효과가 활성화될 때 설정되는 플레이어 상태 플래그
 * pa_daemon  - 효과가 끝날 때 호출될 데몬(타이머) 함수 포인터
 * pa_time    - 효과 지속 시간(턴 수)
 * pa_high    - 환각 상태(ISHALU)일 때 표시할 메시지
 * pa_straight- 정상 상태일 때 표시할 메시지
 */
typedef struct
{
    int pa_flags;
    void (*pa_daemon)();
    int pa_time;
    char *pa_high, *pa_straight;
} PACT;

/* 포션 종류별 행동 테이블: 인덱스는 P_CONFUSE, P_LSD, P_POISON, ... 순서와 일치 */
static PACT p_actions[] =
{
	{ ISHUH,	unconfuse,	HUHDURATION,	/* P_CONFUSE */
		"what a tripy feeling!",
		"wait, what's going on here. Huh? What? Who?" },
	{ ISHALU,	come_down,	SEEDURATION,	/* P_LSD */
		"Oh, wow!  Everything seems so cosmic!",
		"Oh, wow!  Everything seems so cosmic!" },
	{ 0,		NULL,	0 },			/* P_POISON */
	{ 0,		NULL,	0 },			/* P_STRENGTH */
	{ CANSEE,	unsee,	SEEDURATION,		/* P_SEEINVIS */
		prbuf,
		prbuf },
	{ 0,		NULL,	0 },			/* P_HEALING */
	{ 0,		NULL,	0 },			/* P_MFIND */
	{ 0,		NULL,	0 },			/* P_TFIND  */
	{ 0,		NULL,	0 },			/* P_RAISE */
	{ 0,		NULL,	0 },			/* P_XHEAL */
	{ 0,		NULL,	0 },			/* P_HASTE */
	{ 0,		NULL,	0 },			/* P_RESTORE */
	{ ISBLIND,	sight,	SEEDURATION,		/* P_BLIND */
		"oh, bummer!  Everything is dark!  Help!",
		"a cloak of darkness falls around you" },
	{ ISLEVIT,	land,	HEALTIME,		/* P_LEVIT */
		"oh, wow!  You're floating in the air!",
		"you start to float in the air" }
};

/*
 * quaff:
 *	Quaff a potion from the pack
 *
 * [한국어] 가방에서 포션을 꺼내 마신다.
 *   - 포션 타입인지 확인 후 종류별로 효과를 적용한다.
 *   - 효과 적용 후 call_it()으로 플레이어가 포션 이름을 붙일 수 있게 한다.
 *   - 포션이 마지막 1개이면 소모(discard)한다.
 */

void
quaff()
{
    THING *obj, *tp, *mp;
    bool discardit = FALSE;  /* 포션을 버려야 하는지 여부 */
    bool show, trip;         /* show: 마법 감지 시각화 여부, trip: 현재 환각 상태 여부 */

    obj = get_item("quaff", POTION);
    /*
     * Make certain that it is somethings that we want to drink
     */
    if (obj == NULL)
	return;
    if (obj->o_type != POTION)  /* 포션이 아닌 아이템을 마시려 할 경우 경고 */
    {
	if (!terse)
	    msg("yuk! Why would you want to drink that?");
	else
	    msg("that's undrinkable");
	return;
    }
    if (obj == cur_weapon)  /* 현재 무기로 들고 있던 포션이면 무기 해제 */
	cur_weapon = NULL;

    /*
     * Calculate the effect it has on the poor guy.
     */
    trip = on(player, ISHALU);  /* 현재 플레이어가 환각 상태인지 확인 */
    discardit = (bool)(obj->o_count == 1);  /* 마지막 1개면 마신 후 버림 */
    leave_pack(obj, FALSE, FALSE);  /* 가방에서 포션 제거 */
    switch (obj->o_which)
    {
	case P_CONFUSE:   /* 혼란 포션 */
	    do_pot(P_CONFUSE, !trip);
	when P_POISON:    /* 독 포션: 근력 감소 */
	    pot_info[P_POISON].oi_know = TRUE;
	    if (ISWEARING(R_SUSTSTR))  /* 근력 유지 반지를 끼면 큰 영향 없음 */
		msg("you feel momentarily sick");
	    else
	    {
		chg_str(-(rnd(3) + 1));  /* 근력을 1~3 감소 */
		msg("you feel very sick now");
		come_down();
	    }
	when P_HEALING:   /* 회복 포션: HP를 레벨×4 만큼 회복, 최대 HP 증가 가능 */
	    pot_info[P_HEALING].oi_know = TRUE;
	    if ((pstats.s_hpt += roll(pstats.s_lvl, 4)) > max_hp)
		pstats.s_hpt = ++max_hp;
	    sight();  /* 실명 상태 해제 */
	    msg("you begin to feel better");
	when P_STRENGTH:  /* 근력 포션: 근력 +1 */
	    pot_info[P_STRENGTH].oi_know = TRUE;
	    chg_str(1);
	    msg("you feel stronger, now.  What bulging muscles!");
	when P_MFIND:     /* 몬스터 탐지 포션: 일정 시간 모든 몬스터 보임 */
	    player.t_flags |= SEEMONST;
	    fuse((void(*)())turn_see, TRUE, HUHDURATION, AFTER);
	    if (!turn_see(FALSE))
		msg("you have a %s feeling for a moment, then it passes",
		    choose_str("normal", "strange"));
	when P_TFIND:
	    /*
	     * Potion of magic detection.  Show the potions and scrolls
	     * [한국어] 마법 탐지 포션: 현재 층의 마법 아이템 위치를 보여준다.
	     *   lvl_obj(바닥 아이템) 및 몬스터 소지품을 순회하며 마법 아이템 표시.
	     */
	    show = FALSE;
	    if (lvl_obj != NULL)
	    {
		wclear(hw);
		for (tp = lvl_obj; tp != NULL; tp = next(tp))  /* 바닥의 모든 아이템 순회 */
		{
		    if (is_magic(tp))
		    {
			show = TRUE;
			wmove(hw, tp->o_pos.y, tp->o_pos.x);
			waddch(hw, MAGIC);  /* 마법 아이템 위치에 '*' 표시 */
			pot_info[P_TFIND].oi_know = TRUE;
		    }
		}
		for (mp = mlist; mp != NULL; mp = next(mp))  /* 모든 몬스터 소지품 순회 */
		{
		    for (tp = mp->t_pack; tp != NULL; tp = next(tp))
		    {
			if (is_magic(tp))
			{
			    show = TRUE;
			    wmove(hw, mp->t_pos.y, mp->t_pos.x);
			    waddch(hw, MAGIC);
			}
		    }
		}
	    }
	    if (show)
	    {
		pot_info[P_TFIND].oi_know = TRUE;
		show_win("You sense the presence of magic on this level.--More--");
	    }
	    else
		msg("you have a %s feeling for a moment, then it passes",
		    choose_str("normal", "strange"));
	when P_LSD:       /* 환각 포션: visuals 데몬 시작, 화면이 왜곡됨 */
	    if (!trip)
	    {
		if (on(player, SEEMONST))
		    turn_see(FALSE);
		start_daemon(visuals, 0, BEFORE);
		seenstairs = seen_stairs();
	    }
	    do_pot(P_LSD, TRUE);
	when P_SEEINVIS:  /* 투명 감지 포션: 투명 몬스터를 볼 수 있게 됨 */
	    sprintf(prbuf, "this potion tastes like %s juice", fruit);
	    show = on(player, CANSEE);
	    do_pot(P_SEEINVIS, FALSE);
	    if (!show)
		invis_on();
	    sight();
	when P_RAISE:     /* 레벨 상승 포션: 플레이어 레벨 즉시 상승 */
	    pot_info[P_RAISE].oi_know = TRUE;
	    msg("you suddenly feel much more skillful");
	    raise_level();
	when P_XHEAL:     /* 대폭 회복 포션: HP를 레벨×8 회복, 최대 HP 증가 가능 */
	    pot_info[P_XHEAL].oi_know = TRUE;
	    if ((pstats.s_hpt += roll(pstats.s_lvl, 8)) > max_hp)
	    {
		if (pstats.s_hpt > max_hp + pstats.s_lvl + 1)
		    ++max_hp;
		pstats.s_hpt = ++max_hp;
	    }
	    sight();
	    come_down();
	    msg("you begin to feel much better");
	when P_HASTE:     /* 가속 포션: 플레이어 이동속도 2배 */
	    pot_info[P_HASTE].oi_know = TRUE;
	    after = FALSE;
	    if (add_haste(TRUE))
		msg("you feel yourself moving much faster");
	when P_RESTORE:   /* 근력 복원 포션: 감소된 근력을 최대치로 복원 */
	    /* 반지 효과를 일시적으로 제거하고 근력을 최대치로 복원한 뒤 반지 효과 재적용 */
	    if (ISRING(LEFT, R_ADDSTR))
		add_str(&pstats.s_str, -cur_ring[LEFT]->o_arm);
	    if (ISRING(RIGHT, R_ADDSTR))
		add_str(&pstats.s_str, -cur_ring[RIGHT]->o_arm);
	    if (pstats.s_str < max_stats.s_str)
		pstats.s_str = max_stats.s_str;
	    if (ISRING(LEFT, R_ADDSTR))
		add_str(&pstats.s_str, cur_ring[LEFT]->o_arm);
	    if (ISRING(RIGHT, R_ADDSTR))
		add_str(&pstats.s_str, cur_ring[RIGHT]->o_arm);
	    msg("hey, this tastes great.  It make you feel warm all over");
	when P_BLIND:     /* 실명 포션: 일정 시간 시야 차단 */
	    do_pot(P_BLIND, TRUE);
	when P_LEVIT:     /* 공중부양 포션: 일정 시간 공중에 떠오름 */
	    do_pot(P_LEVIT, TRUE);
#ifdef MASTER
	otherwise:
	    msg("what an odd tasting potion!");
	    return;
#endif
    }
    status();
    /*
     * Throw the item away
     */
    /* 포션을 다 마셨으면, 이 포션 종류에 이름을 붙일 기회를 플레이어에게 줌 */
    call_it(&pot_info[obj->o_which]);

    if (discardit)
	discard(obj);
    return;
}

/*
 * is_magic:
 *	Returns true if an object radiates magic
 *
 * [한국어] 아이템이 마법 기운을 발산하는지 여부를 반환한다.
 *   - 방어구: 저주받았거나 기본 방어력과 다르면 마법 아이템
 *   - 무기: 명중 보너스(o_hplus) 또는 데미지 보너스(o_dplus)가 있으면 마법
 *   - 포션, 두루마리, 지팡이, 반지, 부적: 항상 마법으로 간주
 */
bool
is_magic(THING *obj)
{
    switch (obj->o_type)
    {
	case ARMOR:
	    return (bool)((obj->o_flags&ISPROT) || obj->o_arm != a_class[obj->o_which]);
	case WEAPON:
	    return (bool)(obj->o_hplus != 0 || obj->o_dplus != 0);
	case POTION:
	case SCROLL:
	case STICK:
	case RING:
	case AMULET:
	    return TRUE;
    }
    return FALSE;
}

/*
 * invis_on:
 *	Turn on the ability to see invisible
 *
 * [한국어] 투명 감지 능력을 활성화한다.
 *   - 플레이어에게 CANSEE 플래그를 설정한다.
 *   - 현재 투명 상태(ISINVIS)이면서 보이는 위치에 있는 몬스터를
 *     화면에 표시한다(환각 상태가 아닌 경우에만).
 */

void
invis_on()
{
    THING *mp;  /* 몬스터 목록 순회용 포인터 */

    player.t_flags |= CANSEE;  /* 투명 감지 플래그 설정 */
    for (mp = mlist; mp != NULL; mp = next(mp))
	if (on(*mp, ISINVIS) && see_monst(mp) && !on(player, ISHALU))
	    mvaddch(mp->t_pos.y, mp->t_pos.x, mp->t_disguise);  /* 투명 몬스터 표시 */
}

/*
 * turn_see:
 *	Put on or off seeing monsters on this level
 *
 * [한국어] 현재 층의 모든 몬스터를 보이게 하거나 숨긴다.
 *   - turn_off가 TRUE이면 몬스터를 가리고 SEEMONST 플래그 해제
 *   - turn_off가 FALSE이면 몬스터를 강조 표시하고 SEEMONST 플래그 설정
 *   - 환각 상태(ISHALU)이면 몬스터 대신 랜덤 알파벳을 표시
 *   - 반환값: 새로 보이게 된 몬스터 수
 */
bool
turn_see(bool turn_off)  /* turn_off: TRUE면 몬스터 숨김, FALSE면 표시 */
{
    THING *mp;
    bool can_see, add_new;  /* can_see: 이미 보이는 몬스터인지, add_new: 새로 보이는 몬스터 수 */

    add_new = FALSE;
    for (mp = mlist; mp != NULL; mp = next(mp))
    {
	move(mp->t_pos.y, mp->t_pos.x);
	can_see = see_monst(mp);
	if (turn_off)
	{
	    if (!can_see)
		addch(mp->t_oldch);
	}
	else
	{
	    if (!can_see)
		standout();
	    if (!on(player, ISHALU))
		addch(mp->t_type);
	    else
		addch(rnd(26) + 'A');
	    if (!can_see)
	    {
		standend();
		add_new++;
	    }
	}
    }
    if (turn_off)
	player.t_flags &= ~SEEMONST;
    else
	player.t_flags |= SEEMONST;
    return add_new;
}

/*
 * seen_stairs:
 *	Return TRUE if the player has seen the stairs
 *
 * [한국어] 플레이어가 이미 계단을 발견했는지 여부를 반환한다.
 *   세 가지 경우를 확인한다:
 *   1) 계단이 현재 화면 지도에 표시되어 있는 경우
 *   2) 플레이어가 계단 위에 서 있는 경우
 *   3) 몬스터가 계단 위에 있는 경우: 그 몬스터가 보이면서 깨어있거나,
 *      몬스터 탐지 능력으로 발견했고 그 자리에 계단이 있었던 경우
 */
bool
seen_stairs()
{
    THING	*tp;

    move(stairs.y, stairs.x);
    if (inch() == STAIRS)			/* it's on the map */
	return TRUE;
    if (ce(hero, stairs))			/* It's under him */
	return TRUE;

    /*
     * if a monster is on the stairs, this gets hairy
     */
    if ((tp = moat(stairs.y, stairs.x)) != NULL)
    {
	if (see_monst(tp) && on(*tp, ISRUN))	/* if it's visible and awake */
	    return TRUE;			/* it must have moved there */

	if (on(player, SEEMONST)		/* if she can detect monster */
	    && tp->t_oldch == STAIRS)		/* and there once were stairs */
		return TRUE;			/* it must have moved there */
    }
    return FALSE;
}

/*
 * raise_level:
 *	The guy just magically went up a level.
 *
 * [한국어] 포션 등으로 레벨을 즉시 올린다.
 *   현재 레벨에 해당하는 경험치 최솟값 +1을 설정한 뒤 check_level()을 호출해
 *   레벨업 처리(HP 증가, 메시지 출력 등)를 수행한다.
 */

void
raise_level()
{
    pstats.s_exp = e_levels[pstats.s_lvl-1] + 1L;  /* 다음 레벨의 최소 경험치 설정 */
    check_level();  /* 레벨업 처리 */
}

/*
 * do_pot:
 *	Do a potion with standard setup.  This means it uses a fuse and
 *	turns on a flag
 *
 * [한국어] 표준 절차로 포션 효과를 처리한다.
 *   - type: 포션 종류 인덱스 (p_actions[] 배열 인덱스와 동일)
 *   - knowit: TRUE이면 이 포션을 이미 식별한 것으로 기록
 *   - 효과가 아직 없으면 상태 플래그를 설정하고 퓨즈(타이머 데몬)를 점화한다.
 *   - 이미 같은 효과가 있으면 퓨즈 시간을 연장(lengthen)한다.
 *   - 환각 상태 여부에 따라 다른 메시지를 출력한다.
 */

void
do_pot(int type, bool knowit)  /* type: 포션 종류, knowit: 식별 여부 */
{
    PACT *pp;  /* 해당 포션의 행동 정보 포인터 */
    int t;     /* 확산된(spread) 효과 지속 시간 */

    pp = &p_actions[type];
    if (!pot_info[type].oi_know)  /* 아직 식별되지 않은 경우에만 knowit 플래그 적용 */
	pot_info[type].oi_know = knowit;
    t = spread(pp->pa_time);  /* 지속 시간에 약간의 무작위 변동 추가 */
    if (!on(player, pp->pa_flags))  /* 해당 효과가 아직 없으면 새로 시작 */
    {
	player.t_flags |= pp->pa_flags;  /* 상태 플래그 설정 */
	fuse(pp->pa_daemon, 0, t, AFTER);  /* 효과 종료 데몬 예약 */
	look(FALSE);
    }
    else
	lengthen(pp->pa_daemon, t);  /* 이미 효과 중이면 지속 시간 연장 */
    msg(choose_str(pp->pa_high, pp->pa_straight));  /* 상태에 따른 메시지 출력 */
}
