/*
 * Read a scroll and let it happen
 * 스크롤을 읽고 효과를 적용하는 함수들을 담은 파일.
 * 각 스크롤의 효과는 S_CONFUSE, S_ARMOR, S_MAP 등의 상수로 정의된다 (rogue.h 참조).
 *
 * @(#)scrolls.c	4.44 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <curses.h>
#include <ctype.h>
#include "rogue.h"

/*
 * read_scroll:
 *	Read a scroll from the pack and do the appropriate thing
 *	배낭에서 스크롤을 꺼내 읽고 효과를 적용하는 함수.
 *	각 스크롤 유형에 따라 서로 다른 효과가 발동된다:
 *	- S_CONFUSE: 몬스터 혼란 (다음 공격 시 몬스터를 혼란시킴)
 *	- S_ARMOR: 현재 갑옷 강화 (방어력+1, 저주 해제)
 *	- S_HOLD: 주변 몬스터 고정
 *	- S_SLEEP: 플레이어 수면
 *	- S_CREATE: 몬스터 생성
 *	- S_ID_*: 아이템 종류 감정
 *	- S_MAP: 현재 레벨 지도 공개
 *	- S_FDET: 음식 감지
 *	- S_TELEP: 순간 이동
 *	- S_ENCH: 현재 무기 강화
 *	- S_SCARE: (읽으면 아무 효과 없음, 밟으면 몬스터 두려움)
 *	- S_REMOVE: 저주 해제
 *	- S_AGGR: 몬스터 자극
 *	- S_PROTECT: 갑옷 보호
 */

void
read_scroll()
{
    THING *obj;           /* 읽을 스크롤 포인터 */
    PLACE *pp;            /* 맵 위치 포인터 (S_MAP 처리용) */
    int y, x;            /* 좌표 */
    char ch;             /* 문자 변수 */
    int i;               /* 반복 카운터 */
    bool discardit = FALSE; /* 스크롤 사용 후 버릴지 여부 */
    struct room *cur_room;  /* 현재 방 (S_TELEP 처리용) */
    THING *orig_obj;     /* 원래 스크롤 포인터 (call_it용) */
    static coord mp;     /* 몬스터 생성 위치 */

    obj = get_item("read", SCROLL);  /* 배낭에서 스크롤 선택 */
    if (obj == NULL)
	return;
    if (obj->o_type != SCROLL)  /* 스크롤이 아니면 읽을 수 없음 */
    {
	if (!terse)
	    msg("there is nothing on it to read");
	else
	    msg("nothing to read");
	return;
    }
    /*
     * Calculate the effect it has on the poor guy.
     * 스크롤 효과 계산
     */
    if (obj == cur_weapon)  /* 무기로 장착되어 있었다면 해제 */
	cur_weapon = NULL;
    /*
     * Get rid of the thing
     * 스크롤이 1개이면 사용 후 버림 표시
     */
    discardit = (bool)(obj->o_count == 1);
    leave_pack(obj, FALSE, FALSE);  /* 배낭에서 꺼내기 (개수 감소) */
    orig_obj = obj;  /* 원래 포인터 저장 (call_it 호출용) */

    switch (obj->o_which)
    {
	case S_CONFUSE:
	    /*
	     * Scroll of monster confusion.  Give him that power.
	     * 몬스터 혼란 스크롤: 다음 공격 시 몬스터를 혼란시키는 능력 부여
	     */
	    player.t_flags |= CANHUH;  /* 다음 근접 공격 시 몬스터 혼란 */
	    msg("your hands begin to glow %s", pick_color("red"));
	when S_ARMOR:
	    /* 갑옷 강화 스크롤: 방어력 +1, 저주 해제 */
	    if (cur_armor != NULL)
	    {
		cur_armor->o_arm--;    /* o_arm이 낮을수록 좋은 갑옷 */
		cur_armor->o_flags &= ~ISCURSED;  /* 저주 해제 */
		msg("your armor glows %s for a moment", pick_color("silver"));
	    }
	when S_HOLD:
	    /*
	     * Hold monster scroll.  Stop all monsters within two spaces
	     * from chasing after the hero.
	     * 몬스터 고정 스크롤: 영웅 주변 2칸 이내의 몬스터를 모두 고정
	     */

	    ch = 0;  /* 고정된 몬스터 수 */
	    for (x = hero.x - 2; x <= hero.x + 2; x++)
		if (x >= 0 && x < NUMCOLS)
		    for (y = hero.y - 2; y <= hero.y + 2; y++)
			if (y >= 0 && y <= NUMLINES - 1)
			    if ((obj = moat(y, x)) != NULL && on(*obj, ISRUN))
			    {
				obj->t_flags &= ~ISRUN;   /* 달리기 멈춤 */
				obj->t_flags |= ISHELD;   /* 고정 상태 */
				ch++;
			    }
	    if (ch)
	    {
		addmsg("the monster");
		if (ch > 1)
		    addmsg("s around you");
		addmsg(" freeze");
		if (ch == 1)
		    addmsg("s");
		endmsg();
		scr_info[S_HOLD].oi_know = TRUE;
	    }
	    else
		msg("you feel a strange sense of loss");
	when S_SLEEP:
	    /*
	     * Scroll which makes you fall asleep
	     * 수면 스크롤: 플레이어를 일정 시간 행동 불능 상태로 만들기
	     */
	    scr_info[S_SLEEP].oi_know = TRUE;
	    no_command += rnd(SLEEPTIME) + 4;  /* SLEEPTIME+4 턴간 행동 불능 */
	    player.t_flags &= ~ISRUN;  /* 달리기 중단 */
	    msg("you fall asleep");
	when S_CREATE:
	    /*
	     * Create a monster:
	     * First look in a circle around him, next try his room
	     * otherwise give up
	     * 몬스터 생성 스크롤: 영웅 주변 1칸 내에 몬스터를 생성
	     */
	    i = 0;
	    for (y = hero.y - 1; y <= hero.y + 1; y++)
		for (x = hero.x - 1; x <= hero.x + 1; x++)
		    /*
		     * Don't put a monster in top of the player.
		     * 플레이어 위치는 제외
		     */
		    if (y == hero.y && x == hero.x)
			continue;
		    /*
		     * Or anything else nasty
		     * 이동 불가 위치나 공포 스크롤 위치는 제외
		     */
		    else if (step_ok(ch = winat(y, x)))
		    {
			if (ch == SCROLL
			    && find_obj(y, x)->o_which == S_SCARE)
				continue;
			else if (rnd(++i) == 0)  /* 균등 확률로 위치 선택 */
			{
			    mp.y = y;
			    mp.x = x;
			}
		    }
	    if (i == 0)
		msg("you hear a faint cry of anguish in the distance");
	    else
	    {
		obj = new_item();
		new_monster(obj, randmonster(FALSE), &mp);  /* 새 몬스터 생성 */
	    }
	when S_ID_POTION:
	case S_ID_SCROLL:
	case S_ID_WEAPON:
	case S_ID_ARMOR:
	case S_ID_R_OR_S:
	{
	    /* 감정 스크롤: 지정된 유형의 아이템을 하나 식별한다 */
	    static char id_type[S_ID_R_OR_S + 1] =
		{ 0, 0, 0, 0, 0, POTION, SCROLL, WEAPON, ARMOR, R_OR_S };
	    /*
	     * Identify, let him figure something out
	     * whatis()로 플레이어에게 식별할 아이템 선택 요구
	     */
	    scr_info[obj->o_which].oi_know = TRUE;
	    msg("this scroll is an %s scroll", scr_info[obj->o_which].oi_name);
	    whatis(TRUE, id_type[obj->o_which]);  /* 아이템 식별 (command.c 참조) */
	}
	when S_MAP:
	    /*
	     * Scroll of magic mapping.
	     * 지도 공개 스크롤: 현재 레벨의 방, 통로, 함정, 계단을 모두 보여줌
	     */
	    scr_info[S_MAP].oi_know = TRUE;
	    msg("oh, now this scroll has a map on it");
	    /*
	     * take all the things we want to keep hidden out of the window
	     * 숨겨진 요소들(함정, 통로)을 화면에 표시
	     */
	    for (y = 1; y < NUMLINES - 1; y++)
		for (x = 0; x < NUMCOLS; x++)
		{
		    pp = INDEX(y, x);
		    switch (ch = pp->p_ch)
		    {
			case DOOR:    /* 문은 그대로 표시 */
			case STAIRS:  /* 계단은 그대로 표시 */
			    break;

			case '-':     /* 가로 벽 */
			case '|':     /* 세로 벽 */
			    /* 숨겨진 문이면 DOOR로 공개 */
			    if (!(pp->p_flags & F_REAL))
			    {
				ch = pp->p_ch = DOOR;
				pp->p_flags |= F_REAL;
			    }
			    break;

			case ' ':   /* 빈 공간 */
			    if (pp->p_flags & F_REAL)  /* 실제 빈 공간이면 건너뜀 */
				goto def;
			    pp->p_flags |= F_REAL;
			    ch = pp->p_ch = PASSAGE;  /* 숨겨진 통로를 공개 */
			    /* FALLTHROUGH */

			case PASSAGE:   /* 통로 */
pass:
			    if (!(pp->p_flags & F_REAL))
				pp->p_ch = PASSAGE;
			    pp->p_flags |= (F_SEEN|F_REAL);  /* 발견됨으로 표시 */
			    ch = PASSAGE;
			    break;

			case FLOOR:  /* 바닥 */
			    if (pp->p_flags & F_REAL)
				ch = ' ';   /* 보이지 않는 어두운 방 */
			    else
			    {
				ch = TRAP;        /* 숨겨진 함정 공개 */
				pp->p_ch = TRAP;
				pp->p_flags |= (F_SEEN|F_REAL);
			    }
			    break;

			default:
def:
			    if (pp->p_flags & F_PASS)  /* 통로에 있으면 */
				goto pass;
			    ch = ' ';  /* 나머지는 빈 공간으로 */
			    break;
		    }
		    if (ch != ' ')
		    {
			if ((obj = pp->p_monst) != NULL)
			    obj->t_oldch = ch;  /* 몬스터 아래 문자 업데이트 */
			if (obj == NULL || !on(player, SEEMONST))
			    mvaddch(y, x, ch);  /* 화면에 표시 */
		    }
		}
	when S_FDET:
	    /*
	     * Potion of gold detection
	     * 음식 감지 스크롤: 레벨의 모든 음식 위치를 표시
	     */
	    ch = FALSE;
	    wclear(hw);
	    for (obj = lvl_obj; obj != NULL; obj = next(obj))
		if (obj->o_type == FOOD)
		{
		    ch = TRUE;
		    wmove(hw, obj->o_pos.y, obj->o_pos.x);
		    waddch(hw, FOOD);  /* 음식 문자('%') 표시 */
		}
	    if (ch)
	    {
		scr_info[S_FDET].oi_know = TRUE;
		show_win("Your nose tingles and you smell food.--More--");
	    }
	    else
		msg("your nose tingles");
	when S_TELEP:
	    /*
	     * Scroll of teleportation:
	     * Make him dissapear and reappear
	     * 순간 이동 스크롤: 플레이어를 랜덤 위치로 이동
	     */
	    {
		cur_room = proom;
		teleport();  /* 이동 (passages.c 참조) */
		if (cur_room != proom)  /* 다른 방으로 이동했으면 스크롤 식별 */
		    scr_info[S_TELEP].oi_know = TRUE;
	    }
	when S_ENCH:
	    /* 무기 강화 스크롤: 현재 무기의 저주 해제 및 명중/피해 보너스 +1 */
	    if (cur_weapon == NULL || cur_weapon->o_type != WEAPON)
		msg("you feel a strange sense of loss");
	    else
	    {
		cur_weapon->o_flags &= ~ISCURSED;  /* 저주 해제 */
		if (rnd(2) == 0)
		    cur_weapon->o_hplus++;  /* 50% 확률로 명중 보너스 +1 */
		else
		    cur_weapon->o_dplus++;  /* 50% 확률로 피해 보너스 +1 */
		msg("your %s glows %s for a moment",
		    weap_info[cur_weapon->o_which].oi_name, pick_color("blue"));
	    }
	when S_SCARE:
	    /*
	     * Reading it is a mistake and produces laughter at her
	     * poor boo boo.
	     * 공포 스크롤 읽기는 실수: 원래는 바닥에 놓으면 몬스터를 접근 못하게 함
	     * 직접 읽으면 먼 곳에서 비웃음 소리만 들린다
	     */
	    msg("you hear maniacal laughter in the distance");
	when S_REMOVE:
	    /* 저주 제거 스크롤: 현재 장착 중인 모든 아이템의 저주 해제 */
	    uncurse(cur_armor);
	    uncurse(cur_weapon);
	    uncurse(cur_ring[LEFT]);
	    uncurse(cur_ring[RIGHT]);
	    msg(choose_str("you feel in touch with the Universal Onenes",
			   "you feel as if somebody is watching over you"));
	when S_AGGR:
	    /*
	     * This scroll aggravates all the monsters on the current
	     * level and sets them running towards the hero
	     * 몬스터 자극 스크롤: 현재 레벨의 모든 몬스터를 영웅 방향으로 달리게 함
	     */
	    aggravate();  /* 모든 몬스터 자극 (misc.c 참조) */
	    msg("you hear a high pitched humming noise");
	when S_PROTECT:
	    /* 갑옷 보호 스크롤: 현재 갑옷에 ISPROT 플래그 설정 (녹슬지 않음) */
	    if (cur_armor != NULL)
	    {
		cur_armor->o_flags |= ISPROT;
		msg("your armor is covered by a shimmering %s shield",
		    pick_color("gold"));
	    }
	    else
		msg("you feel a strange sense of loss");
#ifdef MASTER
	otherwise:
	    msg("what a puzzling scroll!");
	    return;
#endif
    }
    obj = orig_obj;
    look(TRUE);	/* put the result of the scroll on the screen */
                /* 스크롤 효과를 화면에 반영 */
    status();   /* 상태 표시줄 갱신 */

    call_it(&scr_info[obj->o_which]);  /* 스크롤 이름을 붙일 기회 부여 */

    if (discardit)  /* 스크롤이 1개였다면 메모리 해제 */
	discard(obj);
}

/*
 * uncurse:
 *	Uncurse an item
 *	아이템의 저주(ISCURSED) 플래그를 해제하는 함수.
 *	NULL이면 아무것도 하지 않는다.
 */

void
uncurse(THING *obj)
{
    if (obj != NULL)
	obj->o_flags &= ~ISCURSED;  /* 저주 플래그 해제 */
}

#include <curses.h>
#include <ctype.h>
#include "rogue.h"

/*
 * read_scroll:
 *	Read a scroll from the pack and do the appropriate thing
 */

void
read_scroll()
{
    THING *obj;
    PLACE *pp;
    int y, x;
    char ch;
    int i;
    bool discardit = FALSE;
    struct room *cur_room;
    THING *orig_obj;
    static coord mp;

    obj = get_item("read", SCROLL);
    if (obj == NULL)
	return;
    if (obj->o_type != SCROLL)
    {
	if (!terse)
	    msg("there is nothing on it to read");
	else
	    msg("nothing to read");
	return;
    }
    /*
     * Calculate the effect it has on the poor guy.
     */
    if (obj == cur_weapon)
	cur_weapon = NULL;
    /*
     * Get rid of the thing
     */
    discardit = (bool)(obj->o_count == 1);
    leave_pack(obj, FALSE, FALSE);
    orig_obj = obj;

    switch (obj->o_which)
    {
	case S_CONFUSE:
	    /*
	     * Scroll of monster confusion.  Give him that power.
	     */
	    player.t_flags |= CANHUH;
	    msg("your hands begin to glow %s", pick_color("red"));
	when S_ARMOR:
	    if (cur_armor != NULL)
	    {
		cur_armor->o_arm--;
		cur_armor->o_flags &= ~ISCURSED;
		msg("your armor glows %s for a moment", pick_color("silver"));
	    }
	when S_HOLD:
	    /*
	     * Hold monster scroll.  Stop all monsters within two spaces
	     * from chasing after the hero.
	     */

	    ch = 0;
	    for (x = hero.x - 2; x <= hero.x + 2; x++)
		if (x >= 0 && x < NUMCOLS)
		    for (y = hero.y - 2; y <= hero.y + 2; y++)
			if (y >= 0 && y <= NUMLINES - 1)
			    if ((obj = moat(y, x)) != NULL && on(*obj, ISRUN))
			    {
				obj->t_flags &= ~ISRUN;
				obj->t_flags |= ISHELD;
				ch++;
			    }
	    if (ch)
	    {
		addmsg("the monster");
		if (ch > 1)
		    addmsg("s around you");
		addmsg(" freeze");
		if (ch == 1)
		    addmsg("s");
		endmsg();
		scr_info[S_HOLD].oi_know = TRUE;
	    }
	    else
		msg("you feel a strange sense of loss");
	when S_SLEEP:
	    /*
	     * Scroll which makes you fall asleep
	     */
	    scr_info[S_SLEEP].oi_know = TRUE;
	    no_command += rnd(SLEEPTIME) + 4;
	    player.t_flags &= ~ISRUN;
	    msg("you fall asleep");
	when S_CREATE:
	    /*
	     * Create a monster:
	     * First look in a circle around him, next try his room
	     * otherwise give up
	     */
	    i = 0;
	    for (y = hero.y - 1; y <= hero.y + 1; y++)
		for (x = hero.x - 1; x <= hero.x + 1; x++)
		    /*
		     * Don't put a monster in top of the player.
		     */
		    if (y == hero.y && x == hero.x)
			continue;
		    /*
		     * Or anything else nasty
		     */
		    else if (step_ok(ch = winat(y, x)))
		    {
			if (ch == SCROLL
			    && find_obj(y, x)->o_which == S_SCARE)
				continue;
			else if (rnd(++i) == 0)
			{
			    mp.y = y;
			    mp.x = x;
			}
		    }
	    if (i == 0)
		msg("you hear a faint cry of anguish in the distance");
	    else
	    {
		obj = new_item();
		new_monster(obj, randmonster(FALSE), &mp);
	    }
	when S_ID_POTION:
	case S_ID_SCROLL:
	case S_ID_WEAPON:
	case S_ID_ARMOR:
	case S_ID_R_OR_S:
	{
	    static char id_type[S_ID_R_OR_S + 1] =
		{ 0, 0, 0, 0, 0, POTION, SCROLL, WEAPON, ARMOR, R_OR_S };
	    /*
	     * Identify, let him figure something out
	     */
	    scr_info[obj->o_which].oi_know = TRUE;
	    msg("this scroll is an %s scroll", scr_info[obj->o_which].oi_name);
	    whatis(TRUE, id_type[obj->o_which]);
	}
	when S_MAP:
	    /*
	     * Scroll of magic mapping.
	     */
	    scr_info[S_MAP].oi_know = TRUE;
	    msg("oh, now this scroll has a map on it");
	    /*
	     * take all the things we want to keep hidden out of the window
	     */
	    for (y = 1; y < NUMLINES - 1; y++)
		for (x = 0; x < NUMCOLS; x++)
		{
		    pp = INDEX(y, x);
		    switch (ch = pp->p_ch)
		    {
			case DOOR:
			case STAIRS:
			    break;

			case '-':
			case '|':
			    if (!(pp->p_flags & F_REAL))
			    {
				ch = pp->p_ch = DOOR;
				pp->p_flags |= F_REAL;
			    }
			    break;

			case ' ':
			    if (pp->p_flags & F_REAL)
				goto def;
			    pp->p_flags |= F_REAL;
			    ch = pp->p_ch = PASSAGE;
			    /* FALLTHROUGH */

			case PASSAGE:
pass:
			    if (!(pp->p_flags & F_REAL))
				pp->p_ch = PASSAGE;
			    pp->p_flags |= (F_SEEN|F_REAL);
			    ch = PASSAGE;
			    break;

			case FLOOR:
			    if (pp->p_flags & F_REAL)
				ch = ' ';
			    else
			    {
				ch = TRAP;
				pp->p_ch = TRAP;
				pp->p_flags |= (F_SEEN|F_REAL);
			    }
			    break;

			default:
def:
			    if (pp->p_flags & F_PASS)
				goto pass;
			    ch = ' ';
			    break;
		    }
		    if (ch != ' ')
		    {
			if ((obj = pp->p_monst) != NULL)
			    obj->t_oldch = ch;
			if (obj == NULL || !on(player, SEEMONST))
			    mvaddch(y, x, ch);
		    }
		}
	when S_FDET:
	    /*
	     * Potion of gold detection
	     */
	    ch = FALSE;
	    wclear(hw);
	    for (obj = lvl_obj; obj != NULL; obj = next(obj))
		if (obj->o_type == FOOD)
		{
		    ch = TRUE;
		    wmove(hw, obj->o_pos.y, obj->o_pos.x);
		    waddch(hw, FOOD);
		}
	    if (ch)
	    {
		scr_info[S_FDET].oi_know = TRUE;
		show_win("Your nose tingles and you smell food.--More--");
	    }
	    else
		msg("your nose tingles");
	when S_TELEP:
	    /*
	     * Scroll of teleportation:
	     * Make him dissapear and reappear
	     */
	    {
		cur_room = proom;
		teleport();
		if (cur_room != proom)
		    scr_info[S_TELEP].oi_know = TRUE;
	    }
	when S_ENCH:
	    if (cur_weapon == NULL || cur_weapon->o_type != WEAPON)
		msg("you feel a strange sense of loss");
	    else
	    {
		cur_weapon->o_flags &= ~ISCURSED;
		if (rnd(2) == 0)
		    cur_weapon->o_hplus++;
		else
		    cur_weapon->o_dplus++;
		msg("your %s glows %s for a moment",
		    weap_info[cur_weapon->o_which].oi_name, pick_color("blue"));
	    }
	when S_SCARE:
	    /*
	     * Reading it is a mistake and produces laughter at her
	     * poor boo boo.
	     */
	    msg("you hear maniacal laughter in the distance");
	when S_REMOVE:
	    uncurse(cur_armor);
	    uncurse(cur_weapon);
	    uncurse(cur_ring[LEFT]);
	    uncurse(cur_ring[RIGHT]);
	    msg(choose_str("you feel in touch with the Universal Onenes",
			   "you feel as if somebody is watching over you"));
	when S_AGGR:
	    /*
	     * This scroll aggravates all the monsters on the current
	     * level and sets them running towards the hero
	     */
	    aggravate();
	    msg("you hear a high pitched humming noise");
	when S_PROTECT:
	    if (cur_armor != NULL)
	    {
		cur_armor->o_flags |= ISPROT;
		msg("your armor is covered by a shimmering %s shield",
		    pick_color("gold"));
	    }
	    else
		msg("you feel a strange sense of loss");
#ifdef MASTER
	otherwise:
	    msg("what a puzzling scroll!");
	    return;
#endif
    }
    obj = orig_obj;
    look(TRUE);	/* put the result of the scroll on the screen */
    status();

    call_it(&scr_info[obj->o_which]);

    if (discardit)
	discard(obj);
}

/*
 * uncurse:
 *	Uncurse an item
 */

void
uncurse(THING *obj)
{
    if (obj != NULL)
	obj->o_flags &= ~ISCURSED;
}
