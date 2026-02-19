/*
 * hero movement commands
 * 영웅 이동 명령 처리 함수들을 담은 파일.
 * 방향 이동, 달리기, 함정 처리, 방 진입/퇴장, 혼란 이동, 갑옷 녹슬기 등을 처리한다.
 *
 * @(#)move.c	4.49 (Berkeley) 02/05/99
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
 * used to hold the new hero position
 * 이동 후 영웅의 새 위치를 저장하는 전역 변수
 */

coord nh;

/*
 * do_run:
 *	Start the hero running
 *	영웅이 달리기를 시작하는 함수.
 *	running = TRUE로 설정하고 달리기 방향 문자(runch)를 저장한다.
 *	after = FALSE: 달리기 시작 시 이번 턴의 후처리 이벤트는 실행하지 않음
 */

void
do_run(char ch)
{
    running = TRUE;   /* 달리기 상태 활성화 */
    after = FALSE;    /* 이번 턴 후처리 건너뜀 */
    runch = ch;       /* 달리기 방향 저장 (h,j,k,l,y,u,b,n) */
}

/*
 * do_move:
 *	Check to see that a move is legal.  If it is handle the
 * consequences (fighting, picking up, etc.)
 *	이동이 합법적인지 확인하고 그 결과를 처리하는 함수.
 *	혼란 상태, 함정, 문, 계단, 몬스터 등 이동의 결과를 처리한다.
 *	dy, dx: 이동 방향 (-1, 0, 1)
 */

void
do_move(int dy, int dx)
{
    char ch, fl;  /* 이동할 위치의 문자, 플래그 */

    firstmove = FALSE;  /* 첫 번째 이동 아님 */
    if (no_move)  /* 곰 함정에 걸린 경우 이동 불가 */
    {
	no_move--;
	msg("you are still stuck in the bear trap");
	return;
    }
    /*
     * Do a confused move (maybe)
     * 혼란 상태에서 무작위 이동 (80% 확률)
     */
    if (on(player, ISHUH) && rnd(5) != 0)
    {
	nh = *rndmove(&player);  /* 무작위 이동 위치 계산 */
	if (ce(nh, hero))  /* 이동이 불가능해서 제자리이면 */
	{
	    after = FALSE;
	    running = FALSE;
	    to_death = FALSE;
	    return;
	}
    }
    else  /* 정상적인 이동 */
    {
over:
	nh.y = hero.y + dy;  /* 이동할 위치 계산 */
	nh.x = hero.x + dx;
    }

    /*
     * Check if he tried to move off the screen or make an illegal
     * diagonal move, and stop him if he did.
     * 화면 경계 밖이나 불법 대각선 이동 확인
     */
    if (nh.x < 0 || nh.x >= NUMCOLS || nh.y <= 0 || nh.y >= NUMLINES - 1)
	goto hit_bound;  /* 화면 경계를 벗어나면 벽에 부딪힘 처리 */
    if (!diag_ok(&hero, &nh))  /* 대각선 이동이 불가능하면 */
    {
	after = FALSE;
	running = FALSE;
	return;
    }
    if (running && ce(hero, nh))  /* 달리기 중 제자리이면 중단 */
	after = running = FALSE;
    fl = flat(nh.y, nh.x);   /* 이동할 위치의 플래그 */
    ch = winat(nh.y, nh.x);  /* 이동할 위치의 문자 (몬스터 포함) */
    /* 숨겨진 함정(F_REAL이 없는 바닥)을 밟는 경우 */
    if (!(fl & F_REAL) && ch == FLOOR)
    {
	if (!on(player, ISLEVIT))  /* 공중 부양 중이 아니면 함정 발동 */
	{
	    chat(nh.y, nh.x) = ch = TRAP;  /* 함정 공개 */
	    flat(nh.y, nh.x) |= F_REAL;    /* 실제 표시로 변경 */
	}
    }
    else if (on(player, ISHELD) && ch != 'F')  /* 잡힌 상태에서 식물 제외하고 이동 시도 */
    {
	msg("you are being held");
	return;
    }
    switch (ch)  /* 이동할 위치에 따른 처리 */
    {
	case ' ':  /* 빈 공간: 벽 */
	case '|':  /* 수직 벽 */
	case '-':  /* 수평 벽 */
hit_bound:
	    /* passgo 옵션 활성화 시 달리기 중 통로 꺾기 */
	    if (passgo && running && (proom->r_flags & ISGONE)
		&& !on(player, ISBLIND))
	    {
		bool	b1, b2;

		switch (runch)
		{
		    case 'h':  /* 서쪽 달리기 */
		    case 'l':  /* 동쪽 달리기 */
			/* 위아래로 꺾을 수 있는지 확인 */
			b1 = (bool)(hero.y != 1 && turn_ok(hero.y - 1, hero.x));
			b2 = (bool)(hero.y != NUMLINES - 2 && turn_ok(hero.y + 1, hero.x));
			if (!(b1 ^ b2))  /* 둘 다 가능하거나 둘 다 불가능하면 중단 */
			    break;
			if (b1)  /* 위쪽으로 꺾기 */
			{
			    runch = 'k';
			    dy = -1;
			}
			else  /* 아래쪽으로 꺾기 */
			{
			    runch = 'j';
			    dy = 1;
			}
			dx = 0;
			turnref();  /* 꺾는 지점에서 화면 갱신 */
			goto over;
		    case 'j':  /* 남쪽 달리기 */
		    case 'k':  /* 북쪽 달리기 */
			/* 좌우로 꺾을 수 있는지 확인 */
			b1 = (bool)(hero.x != 0 && turn_ok(hero.y, hero.x - 1));
			b2 = (bool)(hero.x != NUMCOLS - 1 && turn_ok(hero.y, hero.x + 1));
			if (!(b1 ^ b2))
			    break;
			if (b1)  /* 왼쪽으로 꺾기 */
			{
			    runch = 'h';
			    dx = -1;
			}
			else  /* 오른쪽으로 꺾기 */
			{
			    runch = 'l';
			    dx = 1;
			}
			dy = 0;
			turnref();
			goto over;
		}
	    }
	    running = FALSE;
	    after = FALSE;
	    break;
	case DOOR:  /* 문 */
	    running = FALSE;
	    if (flat(hero.y, hero.x) & F_PASS)  /* 통로에서 문으로 들어가면 */
		enter_room(&nh);  /* 방 진입 처리 */
	    goto move_stuff;
	case TRAP:  /* 함정 */
	    ch = be_trapped(&nh);  /* 함정 발동 */
	    if (ch == T_DOOR || ch == T_TELEP)  /* 낙하 함정이나 순간 이동 함정 */
		return;  /* 이미 이동됨 */
	    goto move_stuff;
	case PASSAGE:  /* 통로 */
	    /*
	     * when you're in a corridor, you don't know if you're in
	     * a maze room or not, and there ain't no way to find out
	     * if you're leaving a maze room, so it is necessary to
	     * always recalculate proom.
	     * 미로 방을 나가는지 알 수 없으므로 항상 proom을 재계산
	     */
	    proom = roomin(&hero);  /* 현재 방 재계산 */
	    goto move_stuff;
	case FLOOR:  /* 바닥 */
	    if (!(fl & F_REAL))  /* 실제 표시가 아닌 바닥 (숨겨진 함정) */
		be_trapped(&hero);  /* 함정 발동 (현재 위치) */
	    goto move_stuff;
	case STAIRS:  /* 계단 */
	    seenstairs = TRUE;  /* 계단 발견 표시 */
	    /* FALLTHROUGH */
	default:
	    running = FALSE;
	    if (isupper(ch) || moat(nh.y, nh.x))  /* 몬스터이면 전투 */
		fight(&nh, cur_weapon, FALSE);
	    else
	    {
		if (ch != STAIRS)  /* 계단이 아닌 아이템이면 밟기 예약 */
		    take = ch;
move_stuff:
		mvaddch(hero.y, hero.x, floor_at());  /* 이전 위치 배경 복원 */
		if ((fl & F_PASS) && chat(oldpos.y, oldpos.x) == DOOR)
		    leave_room(&nh);  /* 방을 나갈 때 처리 */
		hero = nh;  /* 영웅 위치 갱신 */
	    }
    }
}

/*
 * turn_ok:
 *	Decide whether it is legal to turn onto the given space
 *	지정 위치로 꺾어 이동하는 것이 가능한지 판별하는 함수.
 *	문(DOOR)이거나 실제 통로(F_REAL|F_PASS)이면 이동 가능.
 */
bool
turn_ok(int y, int x)
{
    PLACE *pp;

    pp = INDEX(y, x);
    return (pp->p_ch == DOOR
	|| (pp->p_flags & (F_REAL|F_PASS)) == (F_REAL|F_PASS));
}

/*
 * turnref:
 *	Decide whether to refresh at a passage turning or not
 *	통로에서 방향을 꺾을 때 화면을 갱신할지 결정하는 함수.
 *	처음 발견한 위치이면 화면을 갱신하고 F_SEEN 플래그를 설정한다.
 *	jump 옵션: 빠른 터미널에서만 움직임을 표시
 */

void
turnref()
{
    PLACE *pp;

    pp = INDEX(hero.y, hero.x);
    if (!(pp->p_flags & F_SEEN))  /* 처음 방문하는 위치이면 */
    {
	if (jump)  /* jump 모드이면 leaveok 사용 (커서 이동 최소화) */
	{
	    leaveok(stdscr, TRUE);
	    refresh();
	    leaveok(stdscr, FALSE);
	}
	pp->p_flags |= F_SEEN;  /* 발견됨으로 표시 */
    }
}

/*
 * door_open:
 *	Called to illuminate a room.  If it is dark, remove anything
 *	that might move.
 *	방 진입 시 방 안의 몬스터들을 깨우는 함수.
 *	방의 모든 위치에 몬스터가 있으면 wake_monster()를 호출한다.
 *	ISGONE 방(없어진 방)에서는 호출하지 않는다.
 */

void
door_open(struct room *rp)
{
    int y, x;

    if (!(rp->r_flags & ISGONE))  /* 존재하는 방이면 */
	for (y = rp->r_pos.y; y < rp->r_pos.y + rp->r_max.y; y++)
	    for (x = rp->r_pos.x; x < rp->r_pos.x + rp->r_max.x; x++)
		if (isupper(winat(y, x)))  /* 몬스터가 있으면 */
		    wake_monster(y, x);    /* 몬스터 깨우기 (monsters.c 참조) */
}

/*
 * be_trapped:
 *	The guy stepped on a trap.... Make him pay.
 *	플레이어가 함정을 밟았을 때 처리하는 함수.
 *	공중 부양 중이면 함정이 발동되지 않는다.
 *	함정 유형 (F_TMASK 플래그 비트에 저장):
 *	- T_DOOR: 한 레벨 아래로 낙하
 *	- T_BEAR: 곰 함정 (BEARTIME 턴간 이동 불가)
 *	- T_MYST: 신비한 함정 (무작위 메시지)
 *	- T_SLEEP: 수면 함정 (SLEEPTIME 턴간 행동 불가)
 *	- T_ARROW: 화살 함정 (명중 여부에 따라 피해 또는 바닥에 화살)
 *	- T_TELEP: 순간 이동 함정 (teleport() 호출)
 *	- T_DART: 독 다트 함정 (독 피해 + 힘 감소)
 *	- T_RUST: 녹물 함정 (갑옷 녹슬기)
 */
char
be_trapped(coord *tc)
{
    PLACE *pp;     /* 함정 위치 데이터 */
    THING *arrow;  /* 화살 아이템 포인터 */
    char tr;       /* 함정 유형 */

    if (on(player, ISLEVIT))  /* 공중 부양 중이면 함정 무시 */
	return T_RUST;	/* anything that's not a door or teleport */
                    /* 문/순간 이동 함정이 아닌 값 반환 */
    running = FALSE;  /* 달리기 중단 */
    count = FALSE;
    pp = INDEX(tc->y, tc->x);
    pp->p_ch = TRAP;  /* 함정 문자('^') 설정 */
    tr = pp->p_flags & F_TMASK;  /* 함정 유형 추출 */
    pp->p_flags |= F_SEEN;  /* 함정 발견됨으로 표시 */
    switch (tr)
    {
	case T_DOOR:  /* 낙하 함정: 다음 레벨로 */
	    level++;
	    new_level();
	    msg("you fell into a trap!");
	when T_BEAR:  /* 곰 함정: BEARTIME 턴간 이동 불가 */
	    no_move += BEARTIME;
	    msg("you are caught in a bear trap");
        when T_MYST:  /* 신비한 함정: 다양한 메시지 중 하나 */
            switch(rnd(11))
            {
                case 0: msg("you are suddenly in a parallel dimension");
                when 1: msg("the light in here suddenly seems %s", rainbow[rnd(cNCOLORS)]);
                when 2: msg("you feel a sting in the side of your neck");
                when 3: msg("multi-colored lines swirl around you, then fade");
                when 4: msg("a %s light flashes in your eyes", rainbow[rnd(cNCOLORS)]);
                when 5: msg("a spike shoots past your ear!");
                when 6: msg("%s sparks dance across your armor", rainbow[rnd(cNCOLORS)]);
                when 7: msg("you suddenly feel very thirsty");
                when 8: msg("you feel time speed up suddenly");
                when 9: msg("time now seems to be going slower");
                when 10: msg("you pack turns %s!", rainbow[rnd(cNCOLORS)]);
            }
	when T_SLEEP:  /* 수면 함정: SLEEPTIME 턴간 행동 불가 */
	    no_command += SLEEPTIME;
	    player.t_flags &= ~ISRUN;
	    msg("a strange white mist envelops you and you fall asleep");
	when T_ARROW:  /* 화살 함정: 방어력에 따라 명중 여부 결정 */
	    if (swing(pstats.s_lvl - 1, pstats.s_arm, 1))  /* 명중 */
	    {
		pstats.s_hpt -= roll(1, 6);  /* 1d6 피해 */
		if (pstats.s_hpt <= 0)
		{
		    msg("an arrow killed you");
		    death('a');
		}
		else
		    msg("oh no! An arrow shot you");
	    }
	    else  /* 빗나감: 화살이 바닥에 떨어짐 */
	    {
		arrow = new_item();
		init_weapon(arrow, ARROW);
		arrow->o_count = 1;
		arrow->o_pos = hero;
		fall(arrow, FALSE);  /* 화살 바닥에 떨어뜨리기 */
		msg("an arrow shoots past you");
	    }
	when T_TELEP:
	    /*
	     * since the hero's leaving, look() won't put a TRAP
	     * down for us, so we have to do it ourself
	     * 영웅이 이동하므로 look()이 함정 표시를 못 함. 직접 처리.
	     */
	    teleport();  /* 순간 이동 */
	    mvaddch(tc->y, tc->x, TRAP);  /* 원래 위치에 함정 문자 표시 */
	when T_DART:  /* 독 다트 함정 */
	    if (!swing(pstats.s_lvl+1, pstats.s_arm, 1))  /* 빗나감 */
		msg("a small dart whizzes by your ear and vanishes");
	    else  /* 명중 */
	    {
		pstats.s_hpt -= roll(1, 4);  /* 1d4 피해 */
		if (pstats.s_hpt <= 0)
		{
		    msg("a poisoned dart killed you");
		    death('d');
		}
		if (!ISWEARING(R_SUSTSTR) && !save(VS_POISON))  /* 독 내성 실패 */
		    chg_str(-1);  /* 힘 -1 */
		msg("a small dart just hit you in the shoulder");
	    }
	when T_RUST:  /* 녹물 함정: 현재 갑옷 녹슬기 */
	    msg("a gush of water hits you on the head");
	    rust_armor(cur_armor);
    }
    flush_type();  /* 키 버퍼 비우기 */
    return tr;
}

/*
 * rndmove:
 *	Move in a random direction if the monster/person is confused
 *	혼란 상태의 몬스터 또는 플레이어가 무작위로 이동할 위치를 계산하는 함수.
 *	현재 위치 주변 3x3 범위에서 유효한 위치를 무작위로 선택한다.
 *	공포 스크롤 위치, 대각선 제약, 이동 불가 위치는 제외.
 *	이동이 불가능하면 현재 위치를 반환한다.
 */
coord *
rndmove(THING *who)
{
    THING *obj;
    int x, y;
    char ch;
    static coord ret;  /* what we will be returning */
                       /* 반환할 좌표 (정적 변수이므로 포인터 반환 가능) */

    y = ret.y = who->t_pos.y + rnd(3) - 1;  /* -1, 0, 1 중 하나 */
    x = ret.x = who->t_pos.x + rnd(3) - 1;
    /*
     * Now check to see if that's a legal move.  If not, don't move.
     * (I.e., bump into the wall or whatever)
     * 법적 이동인지 확인
     */
    if (y == who->t_pos.y && x == who->t_pos.x)  /* 제자리이면 그대로 */
	return &ret;
    if (!diag_ok(&who->t_pos, &ret))  /* 대각선 이동 불가이면 */
	goto bad;
    else
    {
	ch = winat(y, x);
	if (!step_ok(ch))  /* 이동 불가 위치이면 */
	    goto bad;
	if (ch == SCROLL)  /* 스크롤이 있으면 공포 스크롤인지 확인 */
	{
	    for (obj = lvl_obj; obj != NULL; obj = next(obj))
		if (y == obj->o_pos.y && x == obj->o_pos.x)
		    break;
	    if (obj != NULL && obj->o_which == S_SCARE)  /* 공포 스크롤이면 이동 불가 */
		goto bad;
	}
    }
    return &ret;

bad:
    ret = who->t_pos;  /* 이동 불가이면 현재 위치 반환 */
    return &ret;
}

/*
 * rust_armor:
 *	Rust the given armor, if it is a legal kind to rust, and we
 *	aren't wearing a magic ring.
 *	갑옷을 녹슬게 하는 함수.
 *	가죽 갑옷(LEATHER)이나 이미 최악의 방어력이면 녹슬지 않는다.
 *	ISPROT(보호 스크롤) 플래그나 R_SUSTARM 반지를 끼고 있으면 녹슬지 않는다.
 *	녹슬면 o_arm이 1 증가한다 (o_arm이 낮을수록 좋은 갑옷이므로 악화).
 */

void
rust_armor(THING *arm)
{
    /* 갑옷이 없거나 적합하지 않은 유형이면 */
    if (arm == NULL || arm->o_type != ARMOR || arm->o_which == LEATHER ||
	arm->o_arm >= 9)
	    return;

    /* 보호받는 갑옷이면 녹 제거 */
    if ((arm->o_flags & ISPROT) || ISWEARING(R_SUSTARM))
    {
	if (!to_death)
	    msg("the rust vanishes instantly");
    }
    else  /* 갑옷이 녹슬음 */
    {
	arm->o_arm++;  /* 방어력 악화 (o_arm 증가) */
	if (!terse)
	    msg("your armor appears to be weaker now. Oh my!");
	else
	    msg("your armor weakens");
    }
}

#include <curses.h>
#include <ctype.h>
#include "rogue.h"

/*
 * used to hold the new hero position
 */

coord nh;

/*
 * do_run:
 *	Start the hero running
 */

void
do_run(char ch)
{
    running = TRUE;
    after = FALSE;
    runch = ch;
}

/*
 * do_move:
 *	Check to see that a move is legal.  If it is handle the
 * consequences (fighting, picking up, etc.)
 */

void
do_move(int dy, int dx)
{
    char ch, fl;

    firstmove = FALSE;
    if (no_move)
    {
	no_move--;
	msg("you are still stuck in the bear trap");
	return;
    }
    /*
     * Do a confused move (maybe)
     */
    if (on(player, ISHUH) && rnd(5) != 0)
    {
	nh = *rndmove(&player);
	if (ce(nh, hero))
	{
	    after = FALSE;
	    running = FALSE;
	    to_death = FALSE;
	    return;
	}
    }
    else
    {
over:
	nh.y = hero.y + dy;
	nh.x = hero.x + dx;
    }

    /*
     * Check if he tried to move off the screen or make an illegal
     * diagonal move, and stop him if he did.
     */
    if (nh.x < 0 || nh.x >= NUMCOLS || nh.y <= 0 || nh.y >= NUMLINES - 1)
	goto hit_bound;
    if (!diag_ok(&hero, &nh))
    {
	after = FALSE;
	running = FALSE;
	return;
    }
    if (running && ce(hero, nh))
	after = running = FALSE;
    fl = flat(nh.y, nh.x);
    ch = winat(nh.y, nh.x);
    if (!(fl & F_REAL) && ch == FLOOR)
    {
	if (!on(player, ISLEVIT))
	{
	    chat(nh.y, nh.x) = ch = TRAP;
	    flat(nh.y, nh.x) |= F_REAL;
	}
    }
    else if (on(player, ISHELD) && ch != 'F')
    {
	msg("you are being held");
	return;
    }
    switch (ch)
    {
	case ' ':
	case '|':
	case '-':
hit_bound:
	    if (passgo && running && (proom->r_flags & ISGONE)
		&& !on(player, ISBLIND))
	    {
		bool	b1, b2;

		switch (runch)
		{
		    case 'h':
		    case 'l':
			b1 = (bool)(hero.y != 1 && turn_ok(hero.y - 1, hero.x));
			b2 = (bool)(hero.y != NUMLINES - 2 && turn_ok(hero.y + 1, hero.x));
			if (!(b1 ^ b2))
			    break;
			if (b1)
			{
			    runch = 'k';
			    dy = -1;
			}
			else
			{
			    runch = 'j';
			    dy = 1;
			}
			dx = 0;
			turnref();
			goto over;
		    case 'j':
		    case 'k':
			b1 = (bool)(hero.x != 0 && turn_ok(hero.y, hero.x - 1));
			b2 = (bool)(hero.x != NUMCOLS - 1 && turn_ok(hero.y, hero.x + 1));
			if (!(b1 ^ b2))
			    break;
			if (b1)
			{
			    runch = 'h';
			    dx = -1;
			}
			else
			{
			    runch = 'l';
			    dx = 1;
			}
			dy = 0;
			turnref();
			goto over;
		}
	    }
	    running = FALSE;
	    after = FALSE;
	    break;
	case DOOR:
	    running = FALSE;
	    if (flat(hero.y, hero.x) & F_PASS)
		enter_room(&nh);
	    goto move_stuff;
	case TRAP:
	    ch = be_trapped(&nh);
	    if (ch == T_DOOR || ch == T_TELEP)
		return;
	    goto move_stuff;
	case PASSAGE:
	    /*
	     * when you're in a corridor, you don't know if you're in
	     * a maze room or not, and there ain't no way to find out
	     * if you're leaving a maze room, so it is necessary to
	     * always recalculate proom.
	     */
	    proom = roomin(&hero);
	    goto move_stuff;
	case FLOOR:
	    if (!(fl & F_REAL))
		be_trapped(&hero);
	    goto move_stuff;
	case STAIRS:
	    seenstairs = TRUE;
	    /* FALLTHROUGH */
	default:
	    running = FALSE;
	    if (isupper(ch) || moat(nh.y, nh.x))
		fight(&nh, cur_weapon, FALSE);
	    else
	    {
		if (ch != STAIRS)
		    take = ch;
move_stuff:
		mvaddch(hero.y, hero.x, floor_at());
		if ((fl & F_PASS) && chat(oldpos.y, oldpos.x) == DOOR)
		    leave_room(&nh);
		hero = nh;
	    }
    }
}

/*
 * turn_ok:
 *	Decide whether it is legal to turn onto the given space
 */
bool
turn_ok(int y, int x)
{
    PLACE *pp;

    pp = INDEX(y, x);
    return (pp->p_ch == DOOR
	|| (pp->p_flags & (F_REAL|F_PASS)) == (F_REAL|F_PASS));
}

/*
 * turnref:
 *	Decide whether to refresh at a passage turning or not
 */

void
turnref()
{
    PLACE *pp;

    pp = INDEX(hero.y, hero.x);
    if (!(pp->p_flags & F_SEEN))
    {
	if (jump)
	{
	    leaveok(stdscr, TRUE);
	    refresh();
	    leaveok(stdscr, FALSE);
	}
	pp->p_flags |= F_SEEN;
    }
}

/*
 * door_open:
 *	Called to illuminate a room.  If it is dark, remove anything
 *	that might move.
 */

void
door_open(struct room *rp)
{
    int y, x;

    if (!(rp->r_flags & ISGONE))
	for (y = rp->r_pos.y; y < rp->r_pos.y + rp->r_max.y; y++)
	    for (x = rp->r_pos.x; x < rp->r_pos.x + rp->r_max.x; x++)
		if (isupper(winat(y, x)))
		    wake_monster(y, x);
}

/*
 * be_trapped:
 *	The guy stepped on a trap.... Make him pay.
 */
char
be_trapped(coord *tc)
{
    PLACE *pp;
    THING *arrow;
    char tr;

    if (on(player, ISLEVIT))
	return T_RUST;	/* anything that's not a door or teleport */
    running = FALSE;
    count = FALSE;
    pp = INDEX(tc->y, tc->x);
    pp->p_ch = TRAP;
    tr = pp->p_flags & F_TMASK;
    pp->p_flags |= F_SEEN;
    switch (tr)
    {
	case T_DOOR:
	    level++;
	    new_level();
	    msg("you fell into a trap!");
	when T_BEAR:
	    no_move += BEARTIME;
	    msg("you are caught in a bear trap");
        when T_MYST:
            switch(rnd(11))
            {
                case 0: msg("you are suddenly in a parallel dimension");
                when 1: msg("the light in here suddenly seems %s", rainbow[rnd(cNCOLORS)]);
                when 2: msg("you feel a sting in the side of your neck");
                when 3: msg("multi-colored lines swirl around you, then fade");
                when 4: msg("a %s light flashes in your eyes", rainbow[rnd(cNCOLORS)]);
                when 5: msg("a spike shoots past your ear!");
                when 6: msg("%s sparks dance across your armor", rainbow[rnd(cNCOLORS)]);
                when 7: msg("you suddenly feel very thirsty");
                when 8: msg("you feel time speed up suddenly");
                when 9: msg("time now seems to be going slower");
                when 10: msg("you pack turns %s!", rainbow[rnd(cNCOLORS)]);
            }
	when T_SLEEP:
	    no_command += SLEEPTIME;
	    player.t_flags &= ~ISRUN;
	    msg("a strange white mist envelops you and you fall asleep");
	when T_ARROW:
	    if (swing(pstats.s_lvl - 1, pstats.s_arm, 1))
	    {
		pstats.s_hpt -= roll(1, 6);
		if (pstats.s_hpt <= 0)
		{
		    msg("an arrow killed you");
		    death('a');
		}
		else
		    msg("oh no! An arrow shot you");
	    }
	    else
	    {
		arrow = new_item();
		init_weapon(arrow, ARROW);
		arrow->o_count = 1;
		arrow->o_pos = hero;
		fall(arrow, FALSE);
		msg("an arrow shoots past you");
	    }
	when T_TELEP:
	    /*
	     * since the hero's leaving, look() won't put a TRAP
	     * down for us, so we have to do it ourself
	     */
	    teleport();
	    mvaddch(tc->y, tc->x, TRAP);
	when T_DART:
	    if (!swing(pstats.s_lvl+1, pstats.s_arm, 1))
		msg("a small dart whizzes by your ear and vanishes");
	    else
	    {
		pstats.s_hpt -= roll(1, 4);
		if (pstats.s_hpt <= 0)
		{
		    msg("a poisoned dart killed you");
		    death('d');
		}
		if (!ISWEARING(R_SUSTSTR) && !save(VS_POISON))
		    chg_str(-1);
		msg("a small dart just hit you in the shoulder");
	    }
	when T_RUST:
	    msg("a gush of water hits you on the head");
	    rust_armor(cur_armor);
    }
    flush_type();
    return tr;
}

/*
 * rndmove:
 *	Move in a random direction if the monster/person is confused
 */
coord *
rndmove(THING *who)
{
    THING *obj;
    int x, y;
    char ch;
    static coord ret;  /* what we will be returning */

    y = ret.y = who->t_pos.y + rnd(3) - 1;
    x = ret.x = who->t_pos.x + rnd(3) - 1;
    /*
     * Now check to see if that's a legal move.  If not, don't move.
     * (I.e., bump into the wall or whatever)
     */
    if (y == who->t_pos.y && x == who->t_pos.x)
	return &ret;
    if (!diag_ok(&who->t_pos, &ret))
	goto bad;
    else
    {
	ch = winat(y, x);
	if (!step_ok(ch))
	    goto bad;
	if (ch == SCROLL)
	{
	    for (obj = lvl_obj; obj != NULL; obj = next(obj))
		if (y == obj->o_pos.y && x == obj->o_pos.x)
		    break;
	    if (obj != NULL && obj->o_which == S_SCARE)
		goto bad;
	}
    }
    return &ret;

bad:
    ret = who->t_pos;
    return &ret;
}

/*
 * rust_armor:
 *	Rust the given armor, if it is a legal kind to rust, and we
 *	aren't wearing a magic ring.
 */

void
rust_armor(THING *arm)
{
    if (arm == NULL || arm->o_type != ARMOR || arm->o_which == LEATHER ||
	arm->o_arm >= 9)
	    return;

    if ((arm->o_flags & ISPROT) || ISWEARING(R_SUSTARM))
    {
	if (!to_death)
	    msg("the rust vanishes instantly");
    }
    else
    {
	arm->o_arm++;
	if (!terse)
	    msg("your armor appears to be weaker now. Oh my!");
	else
	    msg("your armor weakens");
    }
}
