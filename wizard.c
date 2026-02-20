/*
 * Special wizard commands (some of which are also non-wizard commands
 * under strange circumstances)
 *
 * @(#)wizard.c	4.30 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

/*
 * [파일 개요]
 * 이 파일은 위자드(마법사/디버그) 모드 관련 명령 함수들을 포함합니다.
 * 일부 함수(whatis, set_know, type_name, teleport)는 일반 플레이에서도 사용되며,
 * MASTER 빌드에서만 활성화되는 함수(create_obj, passwd, show_map)도 있습니다.
 *
 * 주요 기능:
 *   - whatis()     : 특정 아이템을 식별한다 (두루마리 '식별' 효과와 동일)
 *   - set_know()   : 아이템 식별 상태(oi_know)를 TRUE로 설정하고 별명을 제거
 *   - type_name()  : 아이템 타입 코드를 사람이 읽을 수 있는 문자열로 변환
 *   - teleport()   : 플레이어를 현재 층의 무작위 위치로 순간이동
 *   - create_obj() : 위자드 전용 - 원하는 아이템을 직접 생성 (MASTER)
 *   - passwd()     : 위자드 모드 진입을 위한 비밀번호 확인 (MASTER)
 *   - show_map()   : 위자드 전용 - 현재 층의 전체 지도를 표시 (MASTER)
 */

#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <ctype.h>
#include "rogue.h"

/*
 * whatis:
 *	What a certin object is
 *
 * [한국어] 특정 아이템의 정체를 식별한다.
 *   - insist: TRUE이면 반드시 유효한 아이템을 선택할 때까지 반복 요청
 *   - type: 식별할 아이템 타입 필터 (0이면 모든 타입 허용)
 *   - 아이템 종류에 따라 set_know() 또는 ISKNOW 플래그를 설정한다.
 *   - 식별 후 inv_name()으로 아이템의 전체 이름을 메시지로 출력한다.
 *   - R_OR_S: 반지 또는 지팡이를 묶어서 지칭하는 특수 타입
 */

void
whatis(bool insist, int type)  /* insist: 강제 선택 여부, type: 아이템 타입 필터 */
{
    THING *obj;  /* 식별할 아이템 포인터 */

    if (pack == NULL)
    {
	msg("you don't have anything in your pack to identify");
	return;
    }

    for (;;)
    {
	obj = get_item("identify", type);
	if (insist)
	{
	    if (n_objs == 0)
		return;
	    else if (obj == NULL)
		msg("you must identify something");
	    else if (type && obj->o_type != type &&
	       !(type == R_OR_S && (obj->o_type == RING || obj->o_type == STICK)) )
		    msg("you must identify a %s", type_name(type));
	    else
		break;
	}
	else
	    break;
    }

    if (obj == NULL)
	return;

    switch (obj->o_type)
    {
        case SCROLL:
	    set_know(obj, scr_info);   /* 두루마리: scr_info에서 식별 표시 */
        when POTION:
	    set_know(obj, pot_info);   /* 포션: pot_info에서 식별 표시 */
	when STICK:
	    set_know(obj, ws_info);    /* 지팡이/막대기: ws_info에서 식별 표시 */
        when WEAPON:
        case ARMOR:
	    obj->o_flags |= ISKNOW;   /* 무기/방어구: ISKNOW 플래그 직접 설정 */
        when RING:
	    set_know(obj, ring_info);  /* 반지: ring_info에서 식별 표시 */
    }
    msg(inv_name(obj, FALSE));
}

/*
 * set_know:
 *	Set things up when we really know what a thing is
 *
 * [한국어] 아이템의 식별 상태를 완전히 설정한다.
 *   - info 배열에서 해당 아이템 종류의 oi_know를 TRUE로 설정
 *   - 아이템 자체에도 ISKNOW 플래그 설정
 *   - 기존에 플레이어가 붙인 별명(oi_guess)이 있으면 메모리 해제 후 NULL로 초기화
 */

void
set_know(THING *obj, struct obj_info *info)  /* obj: 식별할 아이템, info: 해당 타입의 정보 배열 */
{
    char **guess;  /* oi_guess 필드를 가리키는 포인터 */

    info[obj->o_which].oi_know = TRUE;
    obj->o_flags |= ISKNOW;
    guess = &info[obj->o_which].oi_guess;
    if (*guess)
    {
	free(*guess);
	*guess = NULL;
    }
}

/*
 * type_name:
 *	Return a pointer to the name of the type
 *
 * [한국어] 아이템 타입 코드를 사람이 읽을 수 있는 영어 문자열로 반환한다.
 *   tlist[]에 타입 코드와 이름의 대응 테이블이 정의되어 있다.
 *   whatis()에서 잘못된 타입 선택 시 오류 메시지에 사용된다.
 */
char *
type_name(int type)  /* type: 아이템 타입 코드 (POTION, SCROLL, RING 등) */
{
    struct h_list *hp;
    /* 타입 코드와 이름의 대응 테이블 */
    static struct h_list tlist[] = {
	{POTION, "potion",		FALSE},
	{SCROLL, "scroll",		FALSE},
	{FOOD,	 "food",		FALSE},
	{R_OR_S, "ring, wand or staff",	FALSE},
	{RING,	 "ring",		FALSE},
	{STICK,	 "wand or staff",	FALSE},
	{WEAPON, "weapon",		FALSE},
	{ARMOR,	 "suit of armor",	FALSE},
    };

    for (hp = tlist; hp->h_ch; hp++)
	if (type == hp->h_ch)
	    return hp->h_desc;
    /* NOTREACHED */
    return(0);
}

#ifdef MASTER
/*
 * create_obj:
 *	wizard command for getting anything he wants
 *
 * [한국어] 위자드 전용 명령: 원하는 아이템을 직접 생성하여 가방에 넣는다.
 *   - 아이템 타입(o_type)과 세부 종류(o_which, 0~f 16진수)를 입력받는다.
 *   - 무기/방어구는 축복(+), 저주(-), 보통(n)을 선택할 수 있다.
 *   - 보너스 반지는 축복/저주 여부와 o_arm 보너스값을 결정한다.
 *   - 공격성/순간이동 반지는 항상 저주 상태로 생성된다.
 *   - 지팡이는 fix_stick()으로 초기 충전 횟수를 설정한다.
 *   - 금화는 수량을 직접 입력한다.
 *   - 생성된 아이템은 add_pack()으로 즉시 가방에 추가된다.
 */

void
create_obj()
{
    THING *obj;      /* 생성할 아이템 포인터 */
    char ch, bless;  /* ch: 세부 종류 입력 문자, bless: 축복/저주 선택 */

    obj = new_item();
    msg("type of item: ");
    obj->o_type = readchar();
    mpos = 0;
    msg("which %c do you want? (0-f)", obj->o_type);
    obj->o_which = (isdigit((ch = readchar())) ? ch - '0' : ch - 'a' + 10);
    obj->o_group = 0;
    obj->o_count = 1;
    mpos = 0;
    if (obj->o_type == WEAPON || obj->o_type == ARMOR)
    {
	msg("blessing? (+,-,n)");
	bless = readchar();
	mpos = 0;
	if (bless == '-')
	    obj->o_flags |= ISCURSED;
	if (obj->o_type == WEAPON)
	{
	    init_weapon(obj, obj->o_which);
	    if (bless == '-')
		obj->o_hplus -= rnd(3)+1;
	    if (bless == '+')
		obj->o_hplus += rnd(3)+1;
	}
	else
	{
	    obj->o_arm = a_class[obj->o_which];
	    if (bless == '-')
		obj->o_arm += rnd(3)+1;
	    if (bless == '+')
		obj->o_arm -= rnd(3)+1;
	}
    }
    else if (obj->o_type == RING)
	switch (obj->o_which)
	{
	    case R_PROTECT:
	    case R_ADDSTR:
	    case R_ADDHIT:
	    case R_ADDDAM:
		msg("blessing? (+,-,n)");
		bless = readchar();
		mpos = 0;
		if (bless == '-')
		    obj->o_flags |= ISCURSED;
		obj->o_arm = (bless == '-' ? -1 : rnd(2) + 1);
	    when R_AGGR:
	    case R_TELEPORT:
		obj->o_flags |= ISCURSED;
	}
    else if (obj->o_type == STICK)
	fix_stick(obj);
    else if (obj->o_type == GOLD)
    {
	msg("how much?");
	get_num(&obj->o_goldval, stdscr);
    }
    add_pack(obj, FALSE);
}
#endif

/*
 * telport:
 *	Bamf the hero someplace else
 *
 * [한국어] 플레이어를 현재 층의 임의 위치로 순간이동시킨다.
 *   - 이전 위치에 바닥 타일을 복원하고 새 위치를 찾는다(find_floor).
 *   - 방이 바뀌면 leave_room/enter_room으로 시야를 갱신한다.
 *   - 파리지옥(Flytrap)에게 붙잡혀 있는 상태(ISHELD)를 해제한다.
 *   - 이동 중이거나 연속 행동 중이던 상태도 초기화한다.
 */

void
teleport()
{
    static coord c;

    mvaddch(hero.y, hero.x, floor_at());
    find_floor((struct room *) NULL, &c, FALSE, TRUE);
    if (roomin(&c) != proom)
    {
	leave_room(&hero);
	hero = c;
	enter_room(&hero);
    }
    else
    {
	hero = c;
	look(TRUE);
    }
    mvaddch(hero.y, hero.x, PLAYER);
    /*
     * turn off ISHELD in case teleportation was done while fighting
     * a Flytrap
     * [한국어] 파리지옥과 싸우던 중 순간이동했을 경우 ISHELD 플래그를 해제한다.
     *   파리지옥의 데미지도 초기화하여 다음 조우 시 정상 작동하도록 한다.
     */
    if (on(player, ISHELD)) {
	player.t_flags &= ~ISHELD;
	vf_hit = 0;
	strcpy(monsters['F'-'A'].m_stats.s_dmg, "000x0");
    }
    no_move = 0;    /* 이동 불능 카운터 초기화 */
    count = 0;      /* 연속 이동 카운터 초기화 */
    running = FALSE;/* 달리기 모드 해제 */
    flush_type();   /* 버퍼에 쌓인 입력 키 제거 */
}

#ifdef MASTER
/*
 * passwd:
 *	See if user knows password
 *
 * [한국어] 위자드 모드 진입을 위한 비밀번호를 확인한다.
 *   - 비밀번호를 에코 없이 읽어서 md_crypt()로 해시한 뒤 PASSWD 상수와 비교한다.
 *   - md_killchar(): 줄 전체 삭제 문자, md_erasechar(): 한 글자 삭제 문자
 *   - 빈 문자열을 입력하면 FALSE 반환
 *   - 반환값: 비밀번호가 맞으면 TRUE(1), 틀리면 FALSE(0)
 */
int
passwd()
{
    char *sp, c;          /* sp: 버퍼 내 현재 위치, c: 입력 문자 */
    static char buf[MAXSTR];  /* 비밀번호 입력 버퍼 */

    msg("wizard's Password:");
    mpos = 0;
    sp = buf;
    while ((c = readchar()) != '\n' && c != '\r' && c != ESCAPE)
	if (c == md_killchar())
	    sp = buf;
	else if (c == md_erasechar() && sp > buf)
	    sp--;
	else
	    *sp++ = c;
    if (sp == buf)
	return FALSE;
    *sp = '\0';
    return (strcmp(PASSWD, md_crypt(buf, "mT")) == 0);
}

/*
 * show_map:
 *	Print out the map for the wizard
 *
 * [한국어] 위자드 전용: 현재 층의 전체 지도를 hw 창에 출력한다.
 *   - flat(y, x)로 각 타일의 실제(F_REAL) 상태를 확인한다.
 *   - F_REAL 플래그가 없는 타일(허상/미탐색)은 wstandout()으로 강조 표시한다.
 *   - chat(y, x)는 해당 좌표의 화면 문자를 반환한다.
 *   - show_win()으로 "---More (level map)---" 메시지와 함께 표시한다.
 */

void
show_map()
{
    int y, x, real;  /* y, x: 화면 좌표, real: flat() 반환값(타일 플래그) */

    wclear(hw);
    for (y = 1; y < NUMLINES - 1; y++)
	for (x = 0; x < NUMCOLS; x++)
	{
	    real = flat(y, x);
	    if (!(real & F_REAL))
		wstandout(hw);
	    wmove(hw, y, x);
	    waddch(hw, chat(y, x));
	    if (!real)
		wstandend(hw);
	}
    show_win("---More (level map)---");
}
#endif
