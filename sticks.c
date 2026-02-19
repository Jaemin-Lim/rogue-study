/*
 * Functions to implement the various sticks one might find
 * while wandering around the dungeon.
 * 던전에서 발견할 수 있는 지팡이(staff)와 마법봉(wand)의 효과를 구현한 파일.
 *
 * @(#)sticks.c	4.39 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <curses.h>
#include <string.h>
#include <ctype.h>
#include "rogue.h"

/*
 * fix_stick:
 *	Set up a new stick
 *	새 지팡이/마법봉을 초기화하는 함수.
 *	지팡이(staff)이면 직접 공격 피해 "2x3", 마법봉(wand)이면 "1x1".
 *	WS_LIGHT(조명)는 충전량 10~19개, 나머지는 3~7개.
 */

void
fix_stick(THING *cur)
{
    /* 지팡이(staff)이면 더 강한 직접 공격 피해 */
    if (strcmp(ws_type[cur->o_which], "staff") == 0)
	strncpy(cur->o_damage,"2x3",sizeof(cur->o_damage));
    else
	strncpy(cur->o_damage,"1x1",sizeof(cur->o_damage));
    strncpy(cur->o_hurldmg,"1x1",sizeof(cur->o_hurldmg));  /* 투척 피해 */

    switch (cur->o_which)
    {
	case WS_LIGHT:  /* 조명 마법봉은 충전량 더 많음 */
	    cur->o_charges = rnd(10) + 10;  /* 10~19회 */
	otherwise:
	    cur->o_charges = rnd(5) + 3;    /* 3~7회 */
    }
}

/*
 * do_zap:
 *	Perform a zap with a wand
 *	마법봉/지팡이를 사용하는 함수.
 *	방향을 선택하고 지정된 효과를 발동한다.
 *	충전량이 0이면 "nothing happens" 메시지 출력.
 *	각 유형별 효과:
 *	- WS_LIGHT: 방 조명
 *	- WS_DRAIN: 플레이어 HP를 반으로 줄여 같은 방 몬스터들에게 분배
 *	- WS_INVIS: 몬스터 투명화
 *	- WS_POLYMORPH: 몬스터 변형
 *	- WS_TELAWAY/WS_TELTO: 몬스터 순간 이동
 *	- WS_CANCEL: 몬스터 특수 능력 취소
 *	- WS_MISSILE: 마법 미사일 발사
 *	- WS_HASTE_M/WS_SLOW_M: 몬스터 속도 변경
 *	- WS_ELECT/WS_FIRE/WS_COLD: 번개/불꽃/냉기 볼트 발사
 */

void
do_zap()
{
    THING *obj, *tp;    /* 사용할 지팡이, 대상 몬스터 */
    int y, x;          /* 탐색 좌표 */
    char *name;        /* 볼트 이름 */
    char monster, oldch;  /* 몬스터 유형, 이전 배경 문자 */
    static THING bolt;    /* 미사일 볼트 아이템 */

    if ((obj = get_item("zap with", STICK)) == NULL)  /* 배낭에서 선택 */
	return;
    if (obj->o_type != STICK)  /* 지팡이/마법봉이 아니면 */
    {
	after = FALSE;
	msg("you can't zap with that!");
	return;
    }
    if (obj->o_charges == 0)  /* 충전량이 없으면 */
    {
	msg("nothing happens");
	return;
    }
    switch (obj->o_which)
    {
	case WS_LIGHT:
	    /*
	     * Reddy Kilowat wand.  Light up the room
	     * 조명 마법봉: 현재 방을 밝힘 (ISDARK 플래그 제거)
	     */
	    ws_info[WS_LIGHT].oi_know = TRUE;
	    if (proom->r_flags & ISGONE)  /* 통로이면 */
		msg("the corridor glows and then fades");
	    else  /* 방이면 방 조명 */
	    {
		proom->r_flags &= ~ISDARK;  /* 어둠 플래그 제거 */
		/*
		 * Light the room and put the player back up
		 * 방을 밝히고 플레이어 다시 표시
		 */
		enter_room(&hero);
		addmsg("the room is lit");
		if (!terse)
		    addmsg(" by a shimmering %s light", pick_color("blue"));
		endmsg();
	    }
	when WS_DRAIN:
	    /*
	     * take away 1/2 of hero's hit points, then take it away
	     * evenly from the monsters in the room (or next to hero
	     * if he is in a passage)
	     * 생명력 흡수 마법봉: 플레이어 HP를 반으로 줄이고
	     * 같은 방의 몬스터들에게 균등하게 피해를 입힘
	     */
	    if (pstats.s_hpt < 2)  /* HP가 너무 적으면 사용 불가 */
	    {
		msg("you are too weak to use it");
		return;
	    }
	    else
		drain();  /* 생명력 흡수 처리 */
	when WS_INVIS:   /* 투명화 마법봉 */
	case WS_POLYMORPH:  /* 변형 마법봉 */
	case WS_TELAWAY:    /* 원거리 순간 이동 마법봉 */
	case WS_TELTO:      /* 근거리 순간 이동 마법봉 */
	case WS_CANCEL:     /* 취소 마법봉 */
	    /* 지정된 방향으로 이동 가능한 위치까지 탐색 후 몬스터 처리 */
	    y = hero.y;
	    x = hero.x;
	    while (step_ok(winat(y, x)))  /* 막힐 때까지 전진 */
	    {
		y += delta.y;
		x += delta.x;
	    }
	    if ((tp = moat(y, x)) != NULL)  /* 해당 위치에 몬스터가 있으면 */
	    {
		monster = tp->t_type;
		if (monster == 'F')  /* 식물(플라이 트랩)이면 잡힌 상태 해제 */
		    player.t_flags &= ~ISHELD;
		switch (obj->o_which) {
		    case WS_INVIS:  /* 투명화: 몬스터를 투명으로 만듦 */
			tp->t_flags |= ISINVIS;
			if (cansee(y, x))
			    mvaddch(y, x, tp->t_oldch);  /* 화면에서 몬스터 숨김 */
			break;
		    case WS_POLYMORPH:  /* 변형: 몬스터를 랜덤 유형으로 변형 */
		    {
			THING *pp;

			pp = tp->t_pack;  /* 몬스터 배낭 저장 */
			detach(mlist, tp);
			if (see_monst(tp))
			    mvaddch(y, x, chat(y, x));
			oldch = tp->t_oldch;
			delta.y = y;
			delta.x = x;
			new_monster(tp, monster = (char)(rnd(26) + 'A'), &delta);  /* 랜덤 변형 */
			if (see_monst(tp))
			    mvaddch(y, x, monster);
			tp->t_oldch = oldch;
			tp->t_pack = pp;  /* 배낭 복원 */
			ws_info[WS_POLYMORPH].oi_know |= see_monst(tp);
			break;
		    }
		    case WS_CANCEL:  /* 취소: 몬스터의 특수 능력 무력화 */
			tp->t_flags |= ISCANC;         /* 취소됨 표시 */
			tp->t_flags &= ~(ISINVIS|CANHUH);  /* 투명/혼란 능력 제거 */
			tp->t_disguise = tp->t_type;   /* 위장 해제 */
			if (see_monst(tp))
			    mvaddch(y, x, tp->t_disguise);
			break;
		    case WS_TELAWAY:  /* 원거리 순간 이동: 레벨 랜덤 위치로 이동 */
		    case WS_TELTO:    /* 근거리 순간 이동: 영웅 바로 앞으로 이동 */
                    {
			coord new_pos;

			if (obj->o_which == WS_TELAWAY)
			{
			    do
			    {
				find_floor(NULL, &new_pos, FALSE, TRUE);
			    } while (ce(new_pos, hero));  /* 영웅 위치 제외 */
			}
			else  /* WS_TELTO: 영웅 바로 앞 */
			{
			    new_pos.y = hero.y + delta.y;
			    new_pos.x = hero.x + delta.x;
			}
			tp->t_dest = &hero;  /* 영웅을 목적지로 설정 */
			tp->t_flags |= ISRUN;
			relocate(tp, &new_pos);  /* 새 위치로 이동 */
		    }
		}
	    }
	when WS_MISSILE:  /* 마법 미사일: 확실히 명중하는 마법 투사체 */
	    ws_info[WS_MISSILE].oi_know = TRUE;
	    bolt.o_type = '*';  /* 투사체 문자 */
	    strncpy(bolt.o_hurldmg,"1x4",sizeof(bolt.o_hurldmg));
	    bolt.o_hplus = 100;  /* 높은 명중 보너스 */
	    bolt.o_dplus = 1;
	    bolt.o_flags = ISMISL;
	    if (cur_weapon != NULL)
		bolt.o_launch = cur_weapon->o_which;
	    do_motion(&bolt, delta.y, delta.x);  /* 투사체 이동 처리 */
	    if ((tp = moat(bolt.o_pos.y, bolt.o_pos.x)) != NULL
		&& !save_throw(VS_MAGIC, tp))  /* 마법 내성 실패 시 */
		    hit_monster(unc(bolt.o_pos), &bolt);  /* 몬스터 명중 */
	    else if (terse)
		msg("missle vanishes");
	    else
		msg("the missle vanishes with a puff of smoke");
	when WS_HASTE_M:  /* 속도 증가 마법봉: 몬스터를 빠르게 만듦 */
	case WS_SLOW_M:   /* 속도 감소 마법봉: 몬스터를 느리게 만듦 */
	    y = hero.y;
	    x = hero.x;
	    while (step_ok(winat(y, x)))  /* 몬스터 위치 탐색 */
	    {
		y += delta.y;
		x += delta.x;
	    }
	    if ((tp = moat(y, x)) != NULL)
	    {
		if (obj->o_which == WS_HASTE_M)  /* 속도 증가 */
		{
		    if (on(*tp, ISSLOW))        /* 이미 느리면 정상화 */
			tp->t_flags &= ~ISSLOW;
		    else
			tp->t_flags |= ISHASTE;  /* 아니면 가속 */
		}
		else  /* 속도 감소 */
		{
		    if (on(*tp, ISHASTE))       /* 이미 빠르면 정상화 */
			tp->t_flags &= ~ISHASTE;
		    else
			tp->t_flags |= ISSLOW;  /* 아니면 감속 */
		    tp->t_turn = TRUE;
		}
		delta.y = y;
		delta.x = x;
		runto(&delta);  /* 몬스터를 영웅 방향으로 달리게 함 */
	    }
	when WS_ELECT:   /* 번개 볼트 */
	case WS_FIRE:    /* 불꽃 볼트 */
	case WS_COLD:    /* 냉기 볼트 */
	    if (obj->o_which == WS_ELECT)
		name = "bolt";   /* 번개 */
	    else if (obj->o_which == WS_FIRE)
		name = "flame";  /* 불꽃 */
	    else
		name = "ice";    /* 냉기 */
	    fire_bolt(&hero, &delta, name);  /* 볼트 발사 */
	    ws_info[obj->o_which].oi_know = TRUE;
	when WS_NOP:  /* 아무 효과 없는 마법봉 (더미) */
	    break;
#ifdef MASTER
	otherwise:
	    msg("what a bizarre schtick!");
#endif
    }
    obj->o_charges--;  /* 충전량 1 감소 */
}

/*
 * drain:
 *	Do drain hit points from player shtick
 *	생명력 흡수 마법봉 효과를 처리하는 함수.
 *	플레이어 HP를 반으로 줄이고, 같은 방의 모든 몬스터에게
 *	균등하게 피해를 입힌다.
 *	같은 방에 몬스터가 없으면 "tingling feeling" 메시지만 출력.
 */

void
drain()
{
    THING *mp;        /* 몬스터 포인터 */
    struct room *corp; /* 문 위에 있을 때의 추가 방 */
    THING **dp;       /* 피해를 입을 몬스터 배열 포인터 */
    int cnt;          /* 피해를 입을 몬스터 수 */
    bool inpass;      /* 통로에 있는지 여부 */
    static THING *drainee[40];  /* 피해를 입을 몬스터 배열 (최대 40마리) */

    /*
     * First cnt how many things we need to spread the hit points among
     * 먼저 같은 방에 있는 몬스터 수를 셈
     */
    cnt = 0;
    /* 문 위에 있으면 해당 통로도 같은 방으로 간주 */
    if (chat(hero.y, hero.x) == DOOR)
	corp = &passages[flat(hero.y, hero.x) & F_PNUM];
    else
	corp = NULL;
    inpass = (bool)(proom->r_flags & ISGONE);  /* 통로에 있으면 */
    dp = drainee;
    /* 같은 방의 몬스터를 배열에 수집 */
    for (mp = mlist; mp != NULL; mp = next(mp))
	if (mp->t_room == proom || mp->t_room == corp ||
	    (inpass && chat(mp->t_pos.y, mp->t_pos.x) == DOOR &&
	    &passages[flat(mp->t_pos.y, mp->t_pos.x) & F_PNUM] == proom))
		*dp++ = mp;
    if ((cnt = (int)(dp - drainee)) == 0)  /* 같은 방에 몬스터가 없으면 */
    {
	msg("you have a tingling feeling");
	return;
    }
    *dp = NULL;  /* 배열 끝 표시 */
    pstats.s_hpt /= 2;  /* 플레이어 HP 반으로 감소 */
    cnt = pstats.s_hpt / cnt;  /* 각 몬스터에게 입힐 피해량 */
    /*
     * Now zot all of the monsters
     * 수집된 모든 몬스터에게 피해 입힘
     */
    for (dp = drainee; *dp; dp++)
    {
	mp = *dp;
	if ((mp->t_stats.s_hpt -= cnt) <= 0)  /* 피해로 사망 */
	    killed(mp, see_monst(mp));  /* 몬스터 처치 (fight.c 참조) */
	else
	    runto(&mp->t_pos);  /* 생존한 몬스터는 플레이어에게 달려옴 */
    }
}

/*
 * fire_bolt:
 *	Fire a bolt in a given direction from a specific starting place
 *	시작 위치에서 지정된 방향으로 볼트(번개/불꽃/냉기)를 발사하는 함수.
 *	볼트는 벽에 반사되며, BOLT_LENGTH 칸을 이동한다.
 *	몬스터나 영웅에게 6d6 피해를 입힌다.
 *	드래곤('D')은 불꽃 볼트를 반사한다.
 */

void
fire_bolt(coord *start, coord *dir, char *name)
{
    coord *c1, *c2;          /* 볼트 위치 포인터 */
    THING *tp;               /* 맞은 몬스터 포인터 */
    char dirch = 0, ch;      /* 볼트 방향 문자, 위치 문자 */
    bool hit_hero, used, changed;  /* 영웅 접근 여부, 사용됨, 방향 변경됨 */
    static coord pos;        /* 현재 볼트 위치 */
    static coord spotpos[BOLT_LENGTH];  /* 볼트 경로 기록 (화면 복원용) */
    THING bolt;              /* 볼트 아이템 구조체 */

    bolt.o_type = WEAPON;
    bolt.o_which = FLAME;     /* FLAME은 볼트 무기 유형 */
    strncpy(bolt.o_hurldmg,"6x6",sizeof(bolt.o_hurldmg));  /* 6d6 피해 */
    bolt.o_hplus = 100;       /* 높은 명중 보너스 (거의 무조건 명중) */
    bolt.o_dplus = 0;
    weap_info[FLAME].oi_name = name;  /* 볼트 유형 이름 설정 */
    /* 방향에 따른 볼트 표시 문자 결정 */
    switch (dir->y + dir->x)
    {
	case 0: dirch = '/';    /* 대각선 (좌상/우하) */
	when 1: case -1: dirch = (dir->y == 0 ? '-' : '|');  /* 수평/수직 */
	when 2: case -2: dirch = '\\';  /* 대각선 (우상/좌하) */
    }
    pos = *start;
    hit_hero = (bool)(start != &hero);  /* 영웅이 발사자이면 FALSE */
    used = FALSE;   /* 볼트가 목표에 명중했는지 */
    changed = FALSE; /* 방향이 변경됐는지 */
    for (c1 = spotpos; c1 <= &spotpos[BOLT_LENGTH-1] && !used; c1++)
    {
	pos.y += dir->y;
	pos.x += dir->x;
	*c1 = pos;  /* 경로 기록 */
	ch = winat(pos.y, pos.x);
	switch (ch)
	{
	    case DOOR:
		/*
		 * this code is necessary if the hero is on a door
		 * and he fires at the wall the door is in, it would
		 * otherwise loop infinitely
		 * 영웅이 문 위에서 벽 쪽으로 발사하면 무한 루프 방지
		 */
		if (ce(hero, pos))
		    goto def;
		/* FALLTHROUGH */
	    case '|':   /* 수직 벽 */
	    case '-':   /* 수평 벽 */
	    case ' ':   /* 빈 공간 */
		/* 벽에 반사: 방향 반전 */
		if (!changed)
		    hit_hero = !hit_hero;  /* 영웅 방향 전환 */
		changed = FALSE;
		dir->y = -dir->y;  /* y 방향 반전 */
		dir->x = -dir->x;  /* x 방향 반전 */
		c1--;  /* 경로 포인터 후퇴 */
		msg("the %s bounces", name);
		break;
	    default:
def:
		if (!hit_hero && (tp = moat(pos.y, pos.x)) != NULL)
		{
		    hit_hero = TRUE;
		    changed = !changed;
		    tp->t_oldch = chat(pos.y, pos.x);
		    if (!save_throw(VS_MAGIC, tp))  /* 몬스터가 마법 내성 실패 */
		    {
			bolt.o_pos = pos;
			used = TRUE;
			/* 드래곤은 불꽃 볼트를 반사 */
			if (tp->t_type == 'D' && strcmp(name, "flame") == 0)
			{
			    addmsg("the flame bounces");
			    if (!terse)
				addmsg(" off the dragon");
			    endmsg();
			}
			else
			    hit_monster(unc(pos), &bolt);  /* 몬스터 명중 */
		    }
		    else if (ch != 'M' || tp->t_disguise == 'M')
		    {
			if (start == &hero)
			    runto(&pos);  /* 빗나갔으면 몬스터가 달려옴 */
			if (terse)
			    msg("%s misses", name);
			else
			    msg("the %s whizzes past %s", name, set_mname(tp));
		    }
		}
		else if (hit_hero && ce(pos, hero))  /* 볼트가 영웅에게 도달 */
		{
		    hit_hero = FALSE;
		    changed = !changed;
		    if (!save(VS_MAGIC))  /* 영웅이 마법 내성 실패 */
		    {
			if ((pstats.s_hpt -= roll(6, 6)) <= 0)
			{
			    /* 영웅 사망 */
			    if (start == &hero)
				death('b');  /* 자기 볼트에 맞아 사망 */
			    else
				death(moat(start->y, start->x)->t_type);  /* 몬스터 볼트로 사망 */
			}
			used = TRUE;
			if (terse)
			    msg("the %s hits", name);
			else
			    msg("you are hit by the %s", name);
		    }
		    else
			msg("the %s whizzes by you", name);
		}
		mvaddch(pos.y, pos.x, dirch);  /* 볼트 문자 표시 */
		refresh();
	}
    }
    /* 볼트 경로 복원 (화면에서 볼트 지우기) */
    for (c2 = spotpos; c2 < c1; c2++)
	mvaddch(c2->y, c2->x, chat(c2->y, c2->x));  /* 원래 배경 복원 */
}

/*
 * charge_str:
 *	Return an appropriate string for a wand charge
 *	마법봉/지팡이의 충전량을 문자열로 반환하는 함수.
 *	인식된(ISKNOW) 경우에만 충전량 표시.
 *	terse 모드: "[N]", 일반: "[N charges]"
 */
char *
charge_str(THING *obj)
{
    static char buf[20];

    if (!(obj->o_flags & ISKNOW))  /* 아직 인식되지 않았으면 빈 문자열 */
	buf[0] = '\0';
    else if (terse)
	sprintf(buf, " [%d]", obj->o_charges);
    else
	sprintf(buf, " [%d charges]", obj->o_charges);
    return buf;
}

#include <curses.h>
#include <string.h>
#include <ctype.h>
#include "rogue.h"

/*
 * fix_stick:
 *	Set up a new stick
 */

void
fix_stick(THING *cur)
{
    if (strcmp(ws_type[cur->o_which], "staff") == 0)
	strncpy(cur->o_damage,"2x3",sizeof(cur->o_damage));
    else
	strncpy(cur->o_damage,"1x1",sizeof(cur->o_damage));
    strncpy(cur->o_hurldmg,"1x1",sizeof(cur->o_hurldmg));

    switch (cur->o_which)
    {
	case WS_LIGHT:
	    cur->o_charges = rnd(10) + 10;
	otherwise:
	    cur->o_charges = rnd(5) + 3;
    }
}

/*
 * do_zap:
 *	Perform a zap with a wand
 */

void
do_zap()
{
    THING *obj, *tp;
    int y, x;
    char *name;
    char monster, oldch;
    static THING bolt;

    if ((obj = get_item("zap with", STICK)) == NULL)
	return;
    if (obj->o_type != STICK)
    {
	after = FALSE;
	msg("you can't zap with that!");
	return;
    }
    if (obj->o_charges == 0)
    {
	msg("nothing happens");
	return;
    }
    switch (obj->o_which)
    {
	case WS_LIGHT:
	    /*
	     * Reddy Kilowat wand.  Light up the room
	     */
	    ws_info[WS_LIGHT].oi_know = TRUE;
	    if (proom->r_flags & ISGONE)
		msg("the corridor glows and then fades");
	    else
	    {
		proom->r_flags &= ~ISDARK;
		/*
		 * Light the room and put the player back up
		 */
		enter_room(&hero);
		addmsg("the room is lit");
		if (!terse)
		    addmsg(" by a shimmering %s light", pick_color("blue"));
		endmsg();
	    }
	when WS_DRAIN:
	    /*
	     * take away 1/2 of hero's hit points, then take it away
	     * evenly from the monsters in the room (or next to hero
	     * if he is in a passage)
	     */
	    if (pstats.s_hpt < 2)
	    {
		msg("you are too weak to use it");
		return;
	    }
	    else
		drain();
	when WS_INVIS:
	case WS_POLYMORPH:
	case WS_TELAWAY:
	case WS_TELTO:
	case WS_CANCEL:
	    y = hero.y;
	    x = hero.x;
	    while (step_ok(winat(y, x)))
	    {
		y += delta.y;
		x += delta.x;
	    }
	    if ((tp = moat(y, x)) != NULL)
	    {
		monster = tp->t_type;
		if (monster == 'F')
		    player.t_flags &= ~ISHELD;
		switch (obj->o_which) {
		    case WS_INVIS:
			tp->t_flags |= ISINVIS;
			if (cansee(y, x))
			    mvaddch(y, x, tp->t_oldch);
			break;
		    case WS_POLYMORPH:
		    {
			THING *pp;

			pp = tp->t_pack;
			detach(mlist, tp);
			if (see_monst(tp))
			    mvaddch(y, x, chat(y, x));
			oldch = tp->t_oldch;
			delta.y = y;
			delta.x = x;
			new_monster(tp, monster = (char)(rnd(26) + 'A'), &delta);
			if (see_monst(tp))
			    mvaddch(y, x, monster);
			tp->t_oldch = oldch;
			tp->t_pack = pp;
			ws_info[WS_POLYMORPH].oi_know |= see_monst(tp);
			break;
		    }
		    case WS_CANCEL:
			tp->t_flags |= ISCANC;
			tp->t_flags &= ~(ISINVIS|CANHUH);
			tp->t_disguise = tp->t_type;
			if (see_monst(tp))
			    mvaddch(y, x, tp->t_disguise);
			break;
		    case WS_TELAWAY:
		    case WS_TELTO:
                    {
			coord new_pos;

			if (obj->o_which == WS_TELAWAY)
			{
			    do
			    {
				find_floor(NULL, &new_pos, FALSE, TRUE);
			    } while (ce(new_pos, hero));
			}
			else
			{
			    new_pos.y = hero.y + delta.y;
			    new_pos.x = hero.x + delta.x;
			}
			tp->t_dest = &hero;
			tp->t_flags |= ISRUN;
			relocate(tp, &new_pos);
		    }
		}
	    }
	when WS_MISSILE:
	    ws_info[WS_MISSILE].oi_know = TRUE;
	    bolt.o_type = '*';
	    strncpy(bolt.o_hurldmg,"1x4",sizeof(bolt.o_hurldmg));
	    bolt.o_hplus = 100;
	    bolt.o_dplus = 1;
	    bolt.o_flags = ISMISL;
	    if (cur_weapon != NULL)
		bolt.o_launch = cur_weapon->o_which;
	    do_motion(&bolt, delta.y, delta.x);
	    if ((tp = moat(bolt.o_pos.y, bolt.o_pos.x)) != NULL
		&& !save_throw(VS_MAGIC, tp))
		    hit_monster(unc(bolt.o_pos), &bolt);
	    else if (terse)
		msg("missle vanishes");
	    else
		msg("the missle vanishes with a puff of smoke");
	when WS_HASTE_M:
	case WS_SLOW_M:
	    y = hero.y;
	    x = hero.x;
	    while (step_ok(winat(y, x)))
	    {
		y += delta.y;
		x += delta.x;
	    }
	    if ((tp = moat(y, x)) != NULL)
	    {
		if (obj->o_which == WS_HASTE_M)
		{
		    if (on(*tp, ISSLOW))
			tp->t_flags &= ~ISSLOW;
		    else
			tp->t_flags |= ISHASTE;
		}
		else
		{
		    if (on(*tp, ISHASTE))
			tp->t_flags &= ~ISHASTE;
		    else
			tp->t_flags |= ISSLOW;
		    tp->t_turn = TRUE;
		}
		delta.y = y;
		delta.x = x;
		runto(&delta);
	    }
	when WS_ELECT:
	case WS_FIRE:
	case WS_COLD:
	    if (obj->o_which == WS_ELECT)
		name = "bolt";
	    else if (obj->o_which == WS_FIRE)
		name = "flame";
	    else
		name = "ice";
	    fire_bolt(&hero, &delta, name);
	    ws_info[obj->o_which].oi_know = TRUE;
	when WS_NOP:
	    break;
#ifdef MASTER
	otherwise:
	    msg("what a bizarre schtick!");
#endif
    }
    obj->o_charges--;
}

/*
 * drain:
 *	Do drain hit points from player shtick
 */

void
drain()
{
    THING *mp;
    struct room *corp;
    THING **dp;
    int cnt;
    bool inpass;
    static THING *drainee[40];

    /*
     * First cnt how many things we need to spread the hit points among
     */
    cnt = 0;
    if (chat(hero.y, hero.x) == DOOR)
	corp = &passages[flat(hero.y, hero.x) & F_PNUM];
    else
	corp = NULL;
    inpass = (bool)(proom->r_flags & ISGONE);
    dp = drainee;
    for (mp = mlist; mp != NULL; mp = next(mp))
	if (mp->t_room == proom || mp->t_room == corp ||
	    (inpass && chat(mp->t_pos.y, mp->t_pos.x) == DOOR &&
	    &passages[flat(mp->t_pos.y, mp->t_pos.x) & F_PNUM] == proom))
		*dp++ = mp;
    if ((cnt = (int)(dp - drainee)) == 0)
    {
	msg("you have a tingling feeling");
	return;
    }
    *dp = NULL;
    pstats.s_hpt /= 2;
    cnt = pstats.s_hpt / cnt;
    /*
     * Now zot all of the monsters
     */
    for (dp = drainee; *dp; dp++)
    {
	mp = *dp;
	if ((mp->t_stats.s_hpt -= cnt) <= 0)
	    killed(mp, see_monst(mp));
	else
	    runto(&mp->t_pos);
    }
}

/*
 * fire_bolt:
 *	Fire a bolt in a given direction from a specific starting place
 */

void
fire_bolt(coord *start, coord *dir, char *name)
{
    coord *c1, *c2;
    THING *tp;
    char dirch = 0, ch;
    bool hit_hero, used, changed;
    static coord pos;
    static coord spotpos[BOLT_LENGTH];
    THING bolt;

    bolt.o_type = WEAPON;
    bolt.o_which = FLAME;
    strncpy(bolt.o_hurldmg,"6x6",sizeof(bolt.o_hurldmg));
    bolt.o_hplus = 100;
    bolt.o_dplus = 0;
    weap_info[FLAME].oi_name = name;
    switch (dir->y + dir->x)
    {
	case 0: dirch = '/';
	when 1: case -1: dirch = (dir->y == 0 ? '-' : '|');
	when 2: case -2: dirch = '\\';
    }
    pos = *start;
    hit_hero = (bool)(start != &hero);
    used = FALSE;
    changed = FALSE;
    for (c1 = spotpos; c1 <= &spotpos[BOLT_LENGTH-1] && !used; c1++)
    {
	pos.y += dir->y;
	pos.x += dir->x;
	*c1 = pos;
	ch = winat(pos.y, pos.x);
	switch (ch)
	{
	    case DOOR:
		/*
		 * this code is necessary if the hero is on a door
		 * and he fires at the wall the door is in, it would
		 * otherwise loop infinitely
		 */
		if (ce(hero, pos))
		    goto def;
		/* FALLTHROUGH */
	    case '|':
	    case '-':
	    case ' ':
		if (!changed)
		    hit_hero = !hit_hero;
		changed = FALSE;
		dir->y = -dir->y;
		dir->x = -dir->x;
		c1--;
		msg("the %s bounces", name);
		break;
	    default:
def:
		if (!hit_hero && (tp = moat(pos.y, pos.x)) != NULL)
		{
		    hit_hero = TRUE;
		    changed = !changed;
		    tp->t_oldch = chat(pos.y, pos.x);
		    if (!save_throw(VS_MAGIC, tp))
		    {
			bolt.o_pos = pos;
			used = TRUE;
			if (tp->t_type == 'D' && strcmp(name, "flame") == 0)
			{
			    addmsg("the flame bounces");
			    if (!terse)
				addmsg(" off the dragon");
			    endmsg();
			}
			else
			    hit_monster(unc(pos), &bolt);
		    }
		    else if (ch != 'M' || tp->t_disguise == 'M')
		    {
			if (start == &hero)
			    runto(&pos);
			if (terse)
			    msg("%s misses", name);
			else
			    msg("the %s whizzes past %s", name, set_mname(tp));
		    }
		}
		else if (hit_hero && ce(pos, hero))
		{
		    hit_hero = FALSE;
		    changed = !changed;
		    if (!save(VS_MAGIC))
		    {
			if ((pstats.s_hpt -= roll(6, 6)) <= 0)
			{
			    if (start == &hero)
				death('b');
			    else
				death(moat(start->y, start->x)->t_type);
			}
			used = TRUE;
			if (terse)
			    msg("the %s hits", name);
			else
			    msg("you are hit by the %s", name);
		    }
		    else
			msg("the %s whizzes by you", name);
		}
		mvaddch(pos.y, pos.x, dirch);
		refresh();
	}
    }
    for (c2 = spotpos; c2 < c1; c2++)
	mvaddch(c2->y, c2->x, chat(c2->y, c2->x));
}

/*
 * charge_str:
 *	Return an appropriate string for a wand charge
 */
char *
charge_str(THING *obj)
{
    static char buf[20];

    if (!(obj->o_flags & ISKNOW))
	buf[0] = '\0';
    else if (terse)
	sprintf(buf, " [%d]", obj->o_charges);
    else
	sprintf(buf, " [%d charges]", obj->o_charges);
    return buf;
}
