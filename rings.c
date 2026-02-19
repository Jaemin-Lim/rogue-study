/*
 * Routines dealing specifically with rings
 * 반지(ring) 관련 처리 함수들을 담은 파일.
 * 반지 착용, 탈착, 손 선택, 음식 소화에 대한 반지 효과 등을 처리한다.
 *
 * @(#)rings.c	4.19 (Berkeley) 05/29/83
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <curses.h>
#include "rogue.h"

/*
 * ring_on:
 *	Put a ring on a hand
 *	배낭에서 반지를 선택하여 손에 끼는 함수.
 *	양손에 반지가 없으면 어느 손에 낄지 선택.
 *	한쪽에만 있으면 나머지 손에 자동 착용.
 *	양손 모두 차 있으면 착용 불가.
 *	반지 효과(힘 강화, 투명 감지, 몬스터 자극)도 즉시 적용한다.
 */

void
ring_on()
{
    THING *obj;  /* 착용할 반지 포인터 */
    int ring;    /* 착용할 손: LEFT(0) 또는 RIGHT(1) */

    obj = get_item("put on", RING);  /* 배낭에서 반지 선택 */
    /*
     * Make certain that it is somethings that we want to wear
     * 선택한 아이템이 반지인지 확인
     */
    if (obj == NULL)
	return;
    if (obj->o_type != RING)
    {
	if (!terse)
	    msg("it would be difficult to wrap that around a finger");
	else
	    msg("not a ring");
	return;
    }

    /*
     * find out which hand to put it on
     * 어느 손에 끼울지 결정
     */
    if (is_current(obj))  /* 이미 착용 중이면 중단 */
	return;

    if (cur_ring[LEFT] == NULL && cur_ring[RIGHT] == NULL)
    {
	if ((ring = gethand()) < 0)  /* 양손 모두 비어 있으면 선택 요구 */
	    return;
    }
    else if (cur_ring[LEFT] == NULL)  /* 왼손이 비어 있으면 왼손에 착용 */
	ring = LEFT;
    else if (cur_ring[RIGHT] == NULL)  /* 오른손이 비어 있으면 오른손에 착용 */
	ring = RIGHT;
    else  /* 양손 모두 차 있으면 착용 불가 */
    {
	if (!terse)
	    msg("you already have a ring on each hand");
	else
	    msg("wearing two");
	return;
    }
    cur_ring[ring] = obj;  /* 선택한 손에 반지 착용 */

    /*
     * Calculate the effect it has on the poor guy.
     * 반지 착용 시 즉시 적용되는 효과
     */
    switch (obj->o_which)
    {
	case R_ADDSTR:    /* 힘 강화 반지: 착용 시 힘 증가 */
	    chg_str(obj->o_arm);
	    break;
	case R_SEEINVIS:  /* 투명 감지 반지: 착용 시 투명 감지 활성화 */
	    invis_on();
	    break;
	case R_AGGR:      /* 몬스터 자극 반지: 착용 시 모든 몬스터 공격적으로 변함 */
	    aggravate();
	    break;
    }

    if (!terse)
	addmsg("you are now wearing ");
    msg("%s (%c)", inv_name(obj, TRUE), obj->o_packch);
}

/*
 * ring_off:
 *	take off a ring
 *	착용 중인 반지를 벗는 함수.
 *	한쪽 손에만 반지가 있으면 자동으로 그 손의 반지를 벗는다.
 *	양손 모두 착용 중이면 어느 손의 반지를 벗을지 선택.
 *	저주받은 반지는 벗을 수 없다 (dropcheck 함수 참조).
 */

void
ring_off()
{
    int ring;    /* 벗을 반지가 있는 손 */
    THING *obj;  /* 벗을 반지 포인터 */

    if (cur_ring[LEFT] == NULL && cur_ring[RIGHT] == NULL)  /* 반지를 안 끼고 있으면 */
    {
	if (terse)
	    msg("no rings");
	else
	    msg("you aren't wearing any rings");
	return;
    }
    else if (cur_ring[LEFT] == NULL)   /* 왼손에만 없으면 오른손 선택 */
	ring = RIGHT;
    else if (cur_ring[RIGHT] == NULL)  /* 오른손에만 없으면 왼손 선택 */
	ring = LEFT;
    else
	if ((ring = gethand()) < 0)  /* 양손 모두 있으면 선택 요구 */
	    return;
    mpos = 0;
    obj = cur_ring[ring];
    if (obj == NULL)
    {
	msg("not wearing such a ring");
	return;
    }
    if (dropcheck(obj))  /* 저주 확인 후 반지 벗기 */
	msg("was wearing %s(%c)", inv_name(obj, TRUE), obj->o_packch);
    /* dropcheck() 내부에서 cur_ring[ring] = NULL 처리됨 */
}

/*
 * gethand:
 *	Which hand is the hero interested in?
 *	플레이어에게 왼손 또는 오른손을 선택하도록 묻는 함수.
 *	'l'/'L': 왼손(LEFT=0), 'r'/'R': 오른손(RIGHT=1), ESCAPE: 취소(-1 반환)
 */
int
gethand()
{
    int c;  /* 입력 문자 */

    for (;;)
    {
	if (terse)
	    msg("left or right ring? ");
	else
	    msg("left hand or right hand? ");
	if ((c = readchar()) == ESCAPE)
	    return -1;
	mpos = 0;
	if (c == 'l' || c == 'L')
	    return LEFT;
	else if (c == 'r' || c == 'R')
	    return RIGHT;
	if (terse)
	    msg("L or R");
	else
	    msg("please type L or R");
    }
}

/*
 * ring_eat:
 *	How much food does this ring use up?
 *	반지가 매 턴 음식 소화에 미치는 영향을 반환하는 함수.
 *	양수: 음식이 추가로 소모됨 (R_PROTECT, R_ADDSTR 등)
 *	음수(확률적): 가끔 음식 소모를 줄임 (R_SEARCH: 3턴 중 1번 절약)
 *	R_DIGEST: 소화를 돕는 반지 (음식 소모 감소)
 *	반지가 없으면 0 반환.
 */
int
ring_eat(int hand)
{
    THING *ring;    /* 해당 손의 반지 포인터 */
    int eat;        /* 음식 소모량 */
    /* 반지 유형별 음식 소모 테이블:
     * 양수: 추가 소모, 음수: -N이면 1/N 확률로 1 절약 */
    static int uses[] = {
	 1,	/* R_PROTECT */		 1,	/* R_ADDSTR */
	 1,	/* R_SUSTSTR */		-3,	/* R_SEARCH (1/3 확률로 절약) */
	-5,	/* R_SEEINVIS (1/5 확률로 절약) */	 0,	/* R_NOP */
	 0,	/* R_AGGR */		-3,	/* R_ADDHIT */
	-3,	/* R_ADDDAM */		 2,	/* R_REGEN (추가 소모) */
	-2,	/* R_DIGEST (소화 돕기: 음수이지만 반전됨) */	 0,	/* R_TELEPORT */
	 1,	/* R_STEALTH */		 1	/* R_SUSTARM */
    };

    if ((ring = cur_ring[hand]) == NULL)  /* 반지를 끼지 않았으면 */
	return 0;
    if ((eat = uses[ring->o_which]) < 0)  /* 음수이면 확률적으로 절약 */
	eat = (rnd(-eat) == 0);  /* 1/(-eat) 확률로 1 반환, 아니면 0 */
    if (ring->o_which == R_DIGEST)  /* 소화 반지는 음수로 변환 (음식 소모 감소) */
	eat = -eat;
    return eat;
}

/*
 * ring_num:
 *	Print ring bonuses
 *	반지의 보정 수치를 문자열로 반환하는 함수.
 *	인식된(ISKNOW) 반지 중 수치 보너스가 있는 반지만 "[+N]" 형태로 반환한다.
 *	R_PROTECT, R_ADDSTR, R_ADDDAM, R_ADDHIT 반지에만 적용.
 */
char *
ring_num(THING *obj)
{
    static char buf[10];

    if (!(obj->o_flags & ISKNOW))  /* 반지 정보를 모르면 빈 문자열 반환 */
	return "";
    switch (obj->o_which)
    {
	case R_PROTECT:   /* 방어력 강화 반지 */
	case R_ADDSTR:    /* 힘 강화 반지 */
	case R_ADDDAM:    /* 피해 강화 반지 */
	case R_ADDHIT:    /* 명중 강화 반지 */
	    sprintf(buf, " [%s]", num(obj->o_arm, 0, RING));
	otherwise:
	    return "";
    }
    return buf;
}

#include <curses.h>
#include "rogue.h"

/*
 * ring_on:
 *	Put a ring on a hand
 */

void
ring_on()
{
    THING *obj;
    int ring;

    obj = get_item("put on", RING);
    /*
     * Make certain that it is somethings that we want to wear
     */
    if (obj == NULL)
	return;
    if (obj->o_type != RING)
    {
	if (!terse)
	    msg("it would be difficult to wrap that around a finger");
	else
	    msg("not a ring");
	return;
    }

    /*
     * find out which hand to put it on
     */
    if (is_current(obj))
	return;

    if (cur_ring[LEFT] == NULL && cur_ring[RIGHT] == NULL)
    {
	if ((ring = gethand()) < 0)
	    return;
    }
    else if (cur_ring[LEFT] == NULL)
	ring = LEFT;
    else if (cur_ring[RIGHT] == NULL)
	ring = RIGHT;
    else
    {
	if (!terse)
	    msg("you already have a ring on each hand");
	else
	    msg("wearing two");
	return;
    }
    cur_ring[ring] = obj;

    /*
     * Calculate the effect it has on the poor guy.
     */
    switch (obj->o_which)
    {
	case R_ADDSTR:
	    chg_str(obj->o_arm);
	    break;
	case R_SEEINVIS:
	    invis_on();
	    break;
	case R_AGGR:
	    aggravate();
	    break;
    }

    if (!terse)
	addmsg("you are now wearing ");
    msg("%s (%c)", inv_name(obj, TRUE), obj->o_packch);
}

/*
 * ring_off:
 *	take off a ring
 */

void
ring_off()
{
    int ring;
    THING *obj;

    if (cur_ring[LEFT] == NULL && cur_ring[RIGHT] == NULL)
    {
	if (terse)
	    msg("no rings");
	else
	    msg("you aren't wearing any rings");
	return;
    }
    else if (cur_ring[LEFT] == NULL)
	ring = RIGHT;
    else if (cur_ring[RIGHT] == NULL)
	ring = LEFT;
    else
	if ((ring = gethand()) < 0)
	    return;
    mpos = 0;
    obj = cur_ring[ring];
    if (obj == NULL)
    {
	msg("not wearing such a ring");
	return;
    }
    if (dropcheck(obj))
	msg("was wearing %s(%c)", inv_name(obj, TRUE), obj->o_packch);
}

/*
 * gethand:
 *	Which hand is the hero interested in?
 */
int
gethand()
{
    int c;

    for (;;)
    {
	if (terse)
	    msg("left or right ring? ");
	else
	    msg("left hand or right hand? ");
	if ((c = readchar()) == ESCAPE)
	    return -1;
	mpos = 0;
	if (c == 'l' || c == 'L')
	    return LEFT;
	else if (c == 'r' || c == 'R')
	    return RIGHT;
	if (terse)
	    msg("L or R");
	else
	    msg("please type L or R");
    }
}

/*
 * ring_eat:
 *	How much food does this ring use up?
 */
int
ring_eat(int hand)
{
    THING *ring;
    int eat;
    static int uses[] = {
	 1,	/* R_PROTECT */		 1,	/* R_ADDSTR */
	 1,	/* R_SUSTSTR */		-3,	/* R_SEARCH */
	-5,	/* R_SEEINVIS */	 0,	/* R_NOP */
	 0,	/* R_AGGR */		-3,	/* R_ADDHIT */
	-3,	/* R_ADDDAM */		 2,	/* R_REGEN */
	-2,	/* R_DIGEST */		 0,	/* R_TELEPORT */
	 1,	/* R_STEALTH */		 1	/* R_SUSTARM */
    };

    if ((ring = cur_ring[hand]) == NULL)
	return 0;
    if ((eat = uses[ring->o_which]) < 0)
	eat = (rnd(-eat) == 0);
    if (ring->o_which == R_DIGEST)
	eat = -eat;
    return eat;
}

/*
 * ring_num:
 *	Print ring bonuses
 */
char *
ring_num(THING *obj)
{
    static char buf[10];

    if (!(obj->o_flags & ISKNOW))
	return "";
    switch (obj->o_which)
    {
	case R_PROTECT:
	case R_ADDSTR:
	case R_ADDDAM:
	case R_ADDHIT:
	    sprintf(buf, " [%s]", num(obj->o_arm, 0, RING));
	otherwise:
	    return "";
    }
    return buf;
}
