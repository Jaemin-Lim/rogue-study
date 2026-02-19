/*
 * Code for one creature to chase another
 * 한 생명체가 다른 생명체를 추적하는 AI 코드를 담은 파일.
 *
 * @(#)chase.c	4.57 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <stdlib.h>
#include <curses.h>
#include "rogue.h"

#define DRAGONSHOT  5	/* one chance in DRAGONSHOT that a dragon will flame */
                        /* 드래곤이 불을 뿜을 확률: 1/DRAGONSHOT */

static coord ch_ret;				/* Where chasing takes you */
                                    /* 추적 후 이동할 좌표를 저장하는 정적 변수 */

/*
 * runners:
 *	Make all the running monsters move.
 *	현재 달리고 있는(ISRUN 플래그가 설정된) 모든 몬스터를 이동시키는 함수.
 *	mlist(몬스터 연결 리스트)를 순회하며 각 몬스터를 한 턴씩 이동시킨다.
 *	날아다니는(ISFLY) 몬스터는 영웅과의 거리가 3 이상이면 추가로 한 번 더 이동한다.
 */
void
runners()
{
    register THING *tp;   /* 현재 처리 중인 몬스터 포인터 */
    THING *next;          /* 다음 몬스터 포인터 (이동 중 리스트 변경 대비) */
    bool wastarget;       /* 이 몬스터가 'f' 명령의 대상이었는지 여부 */
    static coord orig_pos; /* 이동 전 원래 위치 */

    for (tp = mlist; tp != NULL; tp = next)
    {
        /* remember this in case the monster's "next" is changed */
        /* 몬스터 처리 중 연결 리스트가 변경될 수 있으므로 미리 저장 */
        next = next(tp);
	if (!on(*tp, ISHELD) && on(*tp, ISRUN))  /* 잡히지 않고 달리는 중이면 */
	{
	    orig_pos = tp->t_pos;
	    wastarget = on(*tp, ISTARGET);  /* 추적 대상 여부 저장 */
	    if (move_monst(tp) == -1)  /* 몬스터 한 턴 이동 (실패 시 -1 반환) */
                continue;
	    /* 날아다니는 몬스터가 영웅과 멀리 있으면 추가 이동 */
	    if (on(*tp, ISFLY) && dist_cp(&hero, &tp->t_pos) >= 3)
		move_monst(tp);
	    /* 추적 대상이 이동했다면 추적 상태 해제 */
	    if (wastarget && !ce(orig_pos, tp->t_pos))
	    {
		tp->t_flags &= ~ISTARGET;
		to_death = FALSE;
	    }
	}
    }
    /* 누적된 "hit" 메시지가 있으면 출력 */
    if (has_hit)
    {
	endmsg();
	has_hit = FALSE;
    }
}

/*
 * move_monst:
 *	Execute a single turn of running for a monster
 *	몬스터의 단일 턴 이동을 실행하는 함수.
 *	느려진(ISSLOW) 몬스터는 매 2턴마다 이동하고,
 *	빨라진(ISHASTE) 몬스터는 한 턴에 2번 이동한다.
 *	t_turn 필드로 느린 몬스터의 이동 턴을 추적한다.
 */
int
move_monst(THING *tp)
{
    /* 느리지 않거나 이번 턴이 이동 턴이면 추격 실행 */
    if (!on(*tp, ISSLOW) || tp->t_turn)
	if (do_chase(tp) == -1)
            return(-1);
    /* 빠른 몬스터는 한 번 더 이동 */
    if (on(*tp, ISHASTE))
	if (do_chase(tp) == -1)
            return(-1);
    tp->t_turn ^= TRUE;  /* 느린 몬스터의 이동 가능 턴 토글 (XOR) */
    return(0);
}

/*
 * relocate:
 *	Make the monster's new location be the specified one, updating
 *	all the relevant state.
 *	몬스터를 새로운 위치로 이동시키는 함수.
 *	화면 갱신, 방 정보 업데이트, 몬스터 맵(moat) 업데이트 등을 처리한다.
 *	th: 이동할 몬스터, new_loc: 새로운 위치 좌표
 */
void
relocate(THING *th, coord *new_loc)
{
    struct room *oroom;  /* 이동 전 방 포인터 */

    if (!ce(*new_loc, th->t_pos))  /* 현재 위치와 다른 위치로 이동할 때만 처리 */
    {
	mvaddch(th->t_pos.y, th->t_pos.x, th->t_oldch);  /* 이전 위치 복원 */
	th->t_room = roomin(new_loc);  /* 새 위치의 방 정보 업데이트 */
	set_oldch(th, new_loc);  /* 새 위치의 배경 문자 저장 */
	oroom = th->t_room;
	moat(th->t_pos.y, th->t_pos.x) = NULL;  /* 이전 위치의 몬스터 맵 삭제 */

	if (oroom != th->t_room)  /* 다른 방으로 이동했다면 목적지 재설정 */
	    th->t_dest = find_dest(th);
	th->t_pos = *new_loc;  /* 새 위치로 업데이트 */
	moat(new_loc->y, new_loc->x) = th;  /* 새 위치의 몬스터 맵에 등록 */
    }
    move(new_loc->y, new_loc->x);
    if (see_monst(th))  /* 영웅이 이 몬스터를 볼 수 있으면 */
	addch(th->t_disguise);  /* 위장 문자로 표시 */
    else if (on(player, SEEMONST))  /* 몬스터 감지 능력이 있으면 */
    {
	standout();
	addch(th->t_type);  /* 몬스터 실제 유형 표시 (강조) */
	standend();
    }
}

/*
 * do_chase:
 *	Make one thing chase another.
 *	몬스터가 목표(보통 영웅)를 향해 한 턴 이동하는 핵심 추적 함수.
 *	- 욕심 많은 몬스터(ISGREED)는 금이 없어진 경우 영웅을 추적한다.
 *	- 다른 방에 있는 목표를 향해 문(door) 쪽으로 이동한다.
 *	- 드래곤('D')은 일직선상에 영웅이 있으면 불을 뿜을 수 있다.
 *	- 목표에 도달하면 공격하거나 금/아이템을 집는다.
 */
int
do_chase(THING *th)
{
    register coord *cp;
    register struct room *rer, *ree;	/* room of chaser, room of chasee */
                                        /* 추적자의 방, 피추적자의 방 */
    register int mindist = 32767, curdist;  /* 최소 거리, 현재 거리 */
    register bool stoprun = FALSE;	/* TRUE means we are there */
                                    /* TRUE이면 목표에 도달했음을 의미 */
    register bool door;             /* 현재 문 위에 있는지 여부 */
    register THING *obj;
    static coord this;			/* Temporary destination for chaser */
                                /* 이번 턴의 임시 목적지 좌표 */

    rer = th->t_room;		/* Find room of chaser */
                            /* 추적자(몬스터)의 현재 방 */
    /* 욕심 많은 몬스터가 방의 금이 없어진 경우 영웅을 추적 */
    if (on(*th, ISGREED) && rer->r_goldval == 0)
	th->t_dest = &hero;	/* If gold has been taken, run after hero */
    if (th->t_dest == &hero)	/* Find room of chasee */
                                /* 목표가 영웅이면 영웅의 현재 방 */
	ree = proom;
    else
	ree = roomin(th->t_dest);  /* 목표 위치의 방 */
    /*
     * We don't count doors as inside rooms for this routine
     * 이 루틴에서는 문(door)을 방의 내부로 보지 않는다
     */
    door = (chat(th->t_pos.y, th->t_pos.x) == DOOR);
    /*
     * If the object of our desire is in a different room,
     * and we are not in a corridor, run to the door nearest to
     * our goal.
     * 목표가 다른 방에 있고 복도에 있지 않으면,
     * 목표에 가장 가까운 문(출구)으로 이동한다.
     */
over:
    if (rer != ree)  /* 추적자와 피추적자가 다른 방에 있으면 */
    {
	/* 방의 모든 출구 중 목표에 가장 가까운 것을 찾는다 */
	for (cp = rer->r_exit; cp < &rer->r_exit[rer->r_nexits]; cp++)
	{
	    curdist = dist_cp(th->t_dest, cp);
	    if (curdist < mindist)
	    {
		this = *cp;    /* 가장 가까운 출구를 임시 목적지로 설정 */
		mindist = curdist;
	    }
	}
	if (door)  /* 문 위에 있으면 통로(passage)의 방으로 이동 */
	{
	    rer = &passages[flat(th->t_pos.y, th->t_pos.x) & F_PNUM];
	    door = FALSE;
	    goto over;
	}
    }
    else  /* 같은 방에 있으면 직접 목표를 향해 이동 */
    {
	this = *th->t_dest;
	/*
	 * For dragons check and see if (a) the hero is on a straight
	 * line from it, and (b) that it is within shooting distance,
	 * but outside of striking range.
	 * 드래곤의 경우:
	 * (a) 영웅이 직선상에 있고
	 * (b) 사거리 내에 있으나 근접 공격 범위 밖인 경우
	 * 불꽃(flame)을 발사할 수 있다.
	 */
	if (th->t_type == 'D' && (th->t_pos.y == hero.y || th->t_pos.x == hero.x
	    || abs(th->t_pos.y - hero.y) == abs(th->t_pos.x - hero.x))
	    && dist_cp(&th->t_pos, &hero) <= BOLT_LENGTH * BOLT_LENGTH
	    && !on(*th, ISCANC) && rnd(DRAGONSHOT) == 0)
	{
	    delta.y = sign(hero.y - th->t_pos.y);  /* 불꽃 방향 y */
	    delta.x = sign(hero.x - th->t_pos.x);  /* 불꽃 방향 x */
	    if (has_hit)
		endmsg();
	    fire_bolt(&th->t_pos, &delta, "flame");  /* 불꽃 발사 (sticks.c 참조) */
	    running = FALSE;
	    count = 0;
	    quiet = 0;
	    if (to_death && !on(*th, ISTARGET))
	    {
		to_death = FALSE;
		kamikaze = FALSE;
	    }
	    return(0);
	}
    }
    /*
     * This now contains what we want to run to this time
     * so we run to it.  If we hit it we either want to fight it
     * or stop running
     * this에 이번 턴의 목적지가 설정되었으므로 이동한다.
     * 목적지에 도달하면 전투하거나 달리기를 멈춘다.
     */
    if (!chase(th, &this))  /* 목적지로 이동 (이미 도달했으면 FALSE 반환) */
    {
	if (ce(this, hero))  /* 목적지가 영웅 위치이면 공격 */
	{
	    return( attack(th) );  /* 영웅 공격 (fight.c 참조) */
	}
	else if (ce(this, *th->t_dest))  /* 목적지에 도달한 경우 */
	{
	    /* 목적지에 아이템이 있으면 집는다 */
	    for (obj = lvl_obj; obj != NULL; obj = next(obj))
		if (th->t_dest == &obj->o_pos)
		{
		    detach(lvl_obj, obj);  /* 레벨 아이템 리스트에서 분리 */
		    attach(th->t_pack, obj);  /* 몬스터 배낭에 추가 */
		    chat(obj->o_pos.y, obj->o_pos.x) =
			(th->t_room->r_flags & ISGONE) ? PASSAGE : FLOOR;
		    th->t_dest = find_dest(th);  /* 다음 목적지 찾기 */
		    break;
		}
	    if (th->t_type != 'F')  /* 식물(Venus Flytrap)이 아니면 달리기 중단 */
		stoprun = TRUE;
	}
    }
    else
    {
	if (th->t_type == 'F')  /* 식물은 이동하지 않음 */
	    return(0);
    }
    relocate(th, &ch_ret);  /* 몬스터를 새 위치로 이동 */
    /*
     * And stop running if need be
     * 필요한 경우 달리기 중단
     */
    if (stoprun && ce(th->t_pos, *(th->t_dest)))
	th->t_flags &= ~ISRUN;  /* ISRUN 플래그 해제 */
    return(0);
}

/*
 * set_oldch:
 *	Set the oldch character for the monster
 *	몬스터가 이동할 새 위치의 배경 문자를 t_oldch에 저장하는 함수.
 *	몬스터가 이동한 후 원래 있던 위치를 복원할 때 사용된다.
 *	어두운 방(ISDARK)이거나 램프 거리 안에 있는지에 따라 보이는 문자가 달라진다.
 */
void
set_oldch(THING *tp, coord *cp)
{
    char sch;  /* 이전 t_oldch 값 */

    if (ce(tp->t_pos, *cp))  /* 현재 위치와 동일하면 처리 불필요 */
        return;

    sch = tp->t_oldch;
    tp->t_oldch = CCHAR( mvinch(cp->y,cp->x) );  /* 새 위치의 화면 문자 저장 */
    if (!on(player, ISBLIND))  /* 플레이어가 눈이 멀지 않은 경우 */
    {
	    /* 어두운 방의 바닥은 보이지 않으므로 공백으로 처리 */
	    if ((sch == FLOOR || tp->t_oldch == FLOOR) &&
		(tp->t_room->r_flags & ISDARK))
		    tp->t_oldch = ' ';
	    /* 램프 범위 내에 있고 바닥이 보이면 실제 맵 문자 사용 */
	    else if (dist_cp(cp, &hero) <= LAMPDIST && see_floor)
		tp->t_oldch = chat(cp->y, cp->x);
    }
}

/*
 * see_monst:
 *	Return TRUE if the hero can see the monster
 *	영웅이 특정 몬스터를 볼 수 있는지 판별하는 함수.
 *	다음 조건에서 볼 수 없다:
 *	- 플레이어가 눈이 멀었을 때 (ISBLIND)
 *	- 몬스터가 투명(ISINVIS)하고 플레이어가 투명 감지 능력(CANSEE)이 없을 때
 *	- 몬스터가 LAMPDIST보다 멀리 있을 때 (방이 어둡거나 다른 방일 때)
 */
bool
see_monst(THING *mp)
{
    int y, x;

    if (on(player, ISBLIND))  /* 플레이어가 시각 장애 상태 */
	return FALSE;
    /* 투명 몬스터이고 투명 감지 능력이 없으면 */
    if (on(*mp, ISINVIS) && !on(player, CANSEE))
	return FALSE;
    y = mp->t_pos.y;
    x = mp->t_pos.x;
    if (dist(y, x, hero.y, hero.x) < LAMPDIST)  /* 램프 범위 내에 있으면 */
    {
	/* 대각선 방향에 벽이 있으면 볼 수 없음 */
	if (y != hero.y && x != hero.x &&
	    !step_ok(chat(y, hero.x)) && !step_ok(chat(hero.y, x)))
		return FALSE;
	return TRUE;
    }
    if (mp->t_room != proom)  /* 다른 방에 있으면 볼 수 없음 */
	return FALSE;
    /* 같은 방이지만 어두운 방이면 볼 수 없음 */
    return ((bool)!(mp->t_room->r_flags & ISDARK));
}

/*
 * runto:
 *	Set a monster running after the hero.
 *	특정 위치의 몬스터를 영웅을 향해 달리게 하는 함수.
 *	ISRUN 플래그를 설정하고 ISHELD 플래그를 해제한다.
 *	find_dest()로 최적의 추적 목적지를 설정한다.
 */
void
runto(coord *runner)
{
    register THING *tp;  /* 달릴 몬스터 포인터 */

    /*
     * If we couldn't find him, something is funny
     * 몬스터를 찾지 못하면 오류 (MASTER 모드에서 디버그 메시지 출력)
     */
#ifdef MASTER
    if ((tp = moat(runner->y, runner->x)) == NULL)
	msg("couldn't find monster in runto at (%d,%d)", runner->y, runner->x);
#else
    tp = moat(runner->y, runner->x);
#endif
    /*
     * Start the beastie running
     * 몬스터 달리기 시작
     */
    tp->t_flags |= ISRUN;    /* 달리기 플래그 설정 */
    tp->t_flags &= ~ISHELD;  /* 잡힌 상태 해제 */
    tp->t_dest = find_dest(tp);  /* 목적지 설정 */
}

/*
 * chase:
 *	Find the spot for the chaser(er) to move closer to the
 *	chasee(ee).  Returns TRUE if we want to keep on chasing later
 *	FALSE if we reach the goal.
 *	추적자(er)가 피추적자(ee)에 가장 가까이 이동할 위치를 찾는 함수.
 *	TRUE를 반환하면 계속 추적이 필요함을 의미하고,
 *	FALSE를 반환하면 목표에 도달했음을 의미한다.
 *	결과는 전역 변수 ch_ret에 저장된다.
 *
 *	혼란(ISHUH) 상태의 몬스터나 특수 몬스터(투명 추적자 'P', 박쥐 'B')는
 *	무작위로 이동한다.
 */
bool
chase(THING *tp, coord *ee)
{
    register THING *obj;
    register int x, y;
    register int curdist, thisdist;
    register coord *er = &tp->t_pos;  /* 추적자의 현재 위치 */
    register char ch;
    register int plcnt = 1;   /* 동등 거리 후보 개수 (랜덤 선택용) */
    static coord tryp;         /* 시도할 위치 좌표 */

    /*
     * If the thing is confused, let it move randomly. Invisible
     * Stalkers are slightly confused all of the time, and bats are
     * quite confused all the time
     * 혼란 상태이면 무작위 이동.
     * 투명 추적자('P')는 항상 약간 혼란스럽고,
     * 박쥐('B')는 항상 많이 혼란스럽다.
     */
    if ((on(*tp, ISHUH) && rnd(5) != 0) || (tp->t_type == 'P' && rnd(5) == 0)
	|| (tp->t_type == 'B' && rnd(2) == 0))
    {
	/*
	 * get a valid random move
	 * 유효한 랜덤 이동 위치 획득
	 */
	ch_ret = *rndmove(tp);
	curdist = dist_cp(&ch_ret, ee);
	/*
	 * Small chance that it will become un-confused 
	 * 낮은 확률로 혼란 상태 해제
	 */
	if (rnd(20) == 0)
	    tp->t_flags &= ~ISHUH;
    }
    /*
     * Otherwise, find the empty spot next to the chaser that is
     * closest to the chasee.
     * 그렇지 않으면, 피추적자에 가장 가까운 이동 가능한 위치를 찾는다.
     */
    else
    {
	register int ey, ex;  /* 탐색 범위의 끝 좌표 */
	/*
	 * This will eventually hold where we move to get closer
	 * If we can't find an empty spot, we stay where we are.
	 * 가장 가까운 이동 위치를 저장. 없으면 제자리.
	 */
	curdist = dist_cp(er, ee);  /* 현재 거리 */
	ch_ret = *er;  /* 기본값은 현재 위치 (이동 불가 시) */

	/* 탐색 범위: 현재 위치 주변 3x3 */
	ey = er->y + 1;
	if (ey >= NUMLINES - 1)
	    ey = NUMLINES - 2;
	ex = er->x + 1;
	if (ex >= NUMCOLS)
	    ex = NUMCOLS - 1;

	/* 주변 모든 위치를 탐색 */
	for (x = er->x - 1; x <= ex; x++)
	{
	    if (x < 0)
		continue;
	    tryp.x = x;
	    for (y = er->y - 1; y <= ey; y++)
	    {
		tryp.y = y;
		if (!diag_ok(er, &tryp))  /* 대각선 이동 가능 여부 확인 */
		    continue;
		ch = winat(y, x);  /* 해당 위치의 문자 (몬스터 또는 맵 문자) */
		if (step_ok(ch))  /* 이동 가능한 위치인지 확인 */
		{
		    /*
		     * If it is a scroll, it might be a scare monster scroll
		     * so we need to look it up to see what type it is.
		     * 스크롤이면 몬스터 공포 스크롤(S_SCARE)인지 확인.
		     * 공포 스크롤 위치는 이동하지 않는다.
		     */
		    if (ch == SCROLL)
		    {
			for (obj = lvl_obj; obj != NULL; obj = next(obj))
			{
			    if (y == obj->o_pos.y && x == obj->o_pos.x)
				break;
			}
			if (obj != NULL && obj->o_which == S_SCARE)
			    continue;
		    }
		    /*
		     * It can also be a Xeroc, which we shouldn't step on
		     * 제록(Xeroc, 'X')은 밟을 수 없음 (다른 몬스터처럼 보이는 몬스터)
		     */
		    if ((obj = moat(y, x)) != NULL && obj->t_type == 'X')
			continue;
		    /*
		     * If we didn't find any scrolls at this place or it
		     * wasn't a scare scroll, then this place counts
		     * 공포 스크롤이 없는 유효한 위치이면 거리 비교
		     */
		    thisdist = dist(y, x, ee->y, ee->x);  /* 피추적자와의 거리 */
		    if (thisdist < curdist)  /* 더 가까운 위치 발견 */
		    {
			plcnt = 1;
			ch_ret = tryp;
			curdist = thisdist;
		    }
		    else if (thisdist == curdist && rnd(++plcnt) == 0)
		    {
			/* 같은 거리면 랜덤으로 선택 (균등 분포를 위해) */
			ch_ret = tryp;
			curdist = thisdist;
		    }
		}
	    }
	}
    }
    /* 목표에 아직 도달하지 않았고 영웅 위치가 아니면 TRUE 반환 */
    return (bool)(curdist != 0 && !ce(ch_ret, hero));
}

/*
 * roomin:
 *	Find what room some coordinates are in. NULL means they aren't
 *	in any room.
 *	특정 좌표가 어느 방(room)에 속하는지 반환하는 함수.
 *	통로(F_PASS 플래그)에 있으면 passages[] 배열의 해당 통로를 반환한다.
 *	방에도 통로에도 속하지 않으면 NULL 반환.
 */
struct room *
roomin(coord *cp)
{
    register struct room *rp;  /* 방 포인터 */
    register char *fp;         /* 플래그 포인터 */


    fp = &flat(cp->y, cp->x);  /* 해당 위치의 플래그 */
    if (*fp & F_PASS)  /* 통로에 있으면 통로 구조체 반환 */
	return &passages[*fp & F_PNUM];

    /* 모든 방을 순회하여 좌표가 포함된 방을 찾는다 */
    for (rp = rooms; rp < &rooms[MAXROOMS]; rp++)
	if (cp->x <= rp->r_pos.x + rp->r_max.x && rp->r_pos.x <= cp->x
	 && cp->y <= rp->r_pos.y + rp->r_max.y && rp->r_pos.y <= cp->y)
	    return rp;

    msg("in some bizarre place (%d, %d)", unc(*cp));
#ifdef MASTER
    abort();
    return NULL;
#else
    return NULL;
#endif
}

/*
 * diag_ok:
 *	Check to see if the move is legal if it is diagonal
 *	대각선 이동이 합법적인지 확인하는 함수.
 *	대각선 이동 시 양쪽 인접한 칸 중 하나는 이동 가능해야 한다.
 *	(벽을 통과하는 대각선 이동 방지)
 */
bool
diag_ok(coord *sp, coord *ep)
{
    /* 화면 경계를 벗어나면 불가 */
    if (ep->x < 0 || ep->x >= NUMCOLS || ep->y <= 0 || ep->y >= NUMLINES - 1)
	return FALSE;
    /* 직선 이동이면 항상 가능 */
    if (ep->x == sp->x || ep->y == sp->y)
	return TRUE;
    /* 대각선이면 양쪽 인접 칸 중 하나가 이동 가능해야 함 */
    return (bool)(step_ok(chat(ep->y, sp->x)) && step_ok(chat(sp->y, ep->x)));
}

/*
 * cansee:
 *	Returns true if the hero can see a certain coordinate.
 *	영웅이 특정 좌표를 볼 수 있는지 판별하는 함수.
 *	- 눈이 멀었으면 볼 수 없다.
 *	- LAMPDIST보다 가까우면 볼 수 있다 (단, 통로에서 대각선 시야 제한).
 *	- 같은 방에 있고 어둡지 않으면 볼 수 있다.
 */
bool
cansee(int y, int x)
{
    register struct room *rer;  /* 해당 좌표의 방 포인터 */
    static coord tp;

    if (on(player, ISBLIND))  /* 눈이 멀었으면 */
	return FALSE;
    if (dist(y, x, hero.y, hero.x) < LAMPDIST)  /* 램프 범위 내이면 */
    {
	/* 통로에서 대각선 방향은 시야 가림 확인 */
	if (flat(y, x) & F_PASS)
	    if (y != hero.y && x != hero.x &&
		!step_ok(chat(y, hero.x)) && !step_ok(chat(hero.y, x)))
		    return FALSE;
	return TRUE;
    }
    /*
     * We can only see if the hero in the same room as
     * the coordinate and the room is lit or if it is close.
     * 같은 방이고 방이 밝아야만 볼 수 있다.
     */
    tp.y = y;
    tp.x = x;
    return (bool)((rer = roomin(&tp)) == proom && !(rer->r_flags & ISDARK));
}

/*
 * find_dest:
 *	find the proper destination for the monster
 *	몬스터의 적절한 목적지를 찾는 함수.
 *	- 아이템을 집을 수 없는 몬스터나, 같은 방에 있거나, 영웅이 보이면 영웅을 목적지로 설정.
 *	- 같은 방의 아이템(공포 스크롤 제외)을 목적지로 설정할 수 있다.
 *	- 다른 몬스터가 이미 같은 목적지를 향하고 있으면 건너뛴다.
 */
coord *
find_dest(THING *tp)
{
    register THING *obj;
    register int prob;  /* 아이템을 집을 확률 */

    /* 아이템을 집지 않는 몬스터이거나, 같은 방이거나, 영웅이 보이면 영웅 추적 */
    if ((prob = monsters[tp->t_type - 'A'].m_carry) <= 0 || tp->t_room == proom
	|| see_monst(tp))
	    return &hero;
    /* 레벨의 아이템들을 탐색 */
    for (obj = lvl_obj; obj != NULL; obj = next(obj))
    {
	if (obj->o_type == SCROLL && obj->o_which == S_SCARE)  /* 공포 스크롤 건너뜀 */
	    continue;
	if (roomin(&obj->o_pos) == tp->t_room && rnd(100) < prob)  /* 같은 방의 아이템 */
	{
	    /* 다른 몬스터가 이미 이 아이템을 목적지로 하고 있는지 확인 */
	    for (tp = mlist; tp != NULL; tp = next(tp))
		if (tp->t_dest == &obj->o_pos)
		    break;
	    if (tp == NULL)  /* 아무도 향하지 않는 아이템이면 */
		return &obj->o_pos;
	}
    }
    return &hero;  /* 적절한 아이템이 없으면 영웅 추적 */
}

/*
 * dist:
 *	Calculate the "distance" between to points.  Actually,
 *	this calculates d^2, not d, but that's good enough for
 *	our purposes, since it's only used comparitively.
 *	두 점 사이의 "거리"를 계산하는 함수.
 *	실제 거리(d)가 아닌 거리의 제곱(d^2)을 반환하지만,
 *	비교 목적으로만 사용되므로 충분하다 (제곱근 계산 불필요).
 */
int
dist(int y1, int x1, int y2, int x2)
{
    return ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

/*
 * dist_cp:
 *	Call dist() with appropriate arguments for coord pointers
 *	좌표 포인터를 인수로 받아 dist()를 호출하는 래퍼 함수.
 */
int
dist_cp(coord *c1, coord *c2)
{
    return dist(c1->y, c1->x, c2->y, c2->x);
}

/*
 * runners:
 *	Make all the running monsters move.
 */
void
runners()
{
    register THING *tp;
    THING *next;
    bool wastarget;
    static coord orig_pos;

    for (tp = mlist; tp != NULL; tp = next)
    {
        /* remember this in case the monster's "next" is changed */
        next = next(tp);
	if (!on(*tp, ISHELD) && on(*tp, ISRUN))
	{
	    orig_pos = tp->t_pos;
	    wastarget = on(*tp, ISTARGET);
	    if (move_monst(tp) == -1)
                continue;
	    if (on(*tp, ISFLY) && dist_cp(&hero, &tp->t_pos) >= 3)
		move_monst(tp);
	    if (wastarget && !ce(orig_pos, tp->t_pos))
	    {
		tp->t_flags &= ~ISTARGET;
		to_death = FALSE;
	    }
	}
    }
    if (has_hit)
    {
	endmsg();
	has_hit = FALSE;
    }
}

/*
 * move_monst:
 *	Execute a single turn of running for a monster
 */
int
move_monst(THING *tp)
{
    if (!on(*tp, ISSLOW) || tp->t_turn)
	if (do_chase(tp) == -1)
            return(-1);
    if (on(*tp, ISHASTE))
	if (do_chase(tp) == -1)
            return(-1);
    tp->t_turn ^= TRUE;
    return(0);
}

/*
 * relocate:
 *	Make the monster's new location be the specified one, updating
 *	all the relevant state.
 */
void
relocate(THING *th, coord *new_loc)
{
    struct room *oroom;

    if (!ce(*new_loc, th->t_pos))
    {
	mvaddch(th->t_pos.y, th->t_pos.x, th->t_oldch);
	th->t_room = roomin(new_loc);
	set_oldch(th, new_loc);
	oroom = th->t_room;
	moat(th->t_pos.y, th->t_pos.x) = NULL;

	if (oroom != th->t_room)
	    th->t_dest = find_dest(th);
	th->t_pos = *new_loc;
	moat(new_loc->y, new_loc->x) = th;
    }
    move(new_loc->y, new_loc->x);
    if (see_monst(th))
	addch(th->t_disguise);
    else if (on(player, SEEMONST))
    {
	standout();
	addch(th->t_type);
	standend();
    }
}

/*
 * do_chase:
 *	Make one thing chase another.
 */
int
do_chase(THING *th)
{
    register coord *cp;
    register struct room *rer, *ree;	/* room of chaser, room of chasee */
    register int mindist = 32767, curdist;
    register bool stoprun = FALSE;	/* TRUE means we are there */
    register bool door;
    register THING *obj;
    static coord this;			/* Temporary destination for chaser */

    rer = th->t_room;		/* Find room of chaser */
    if (on(*th, ISGREED) && rer->r_goldval == 0)
	th->t_dest = &hero;	/* If gold has been taken, run after hero */
    if (th->t_dest == &hero)	/* Find room of chasee */
	ree = proom;
    else
	ree = roomin(th->t_dest);
    /*
     * We don't count doors as inside rooms for this routine
     */
    door = (chat(th->t_pos.y, th->t_pos.x) == DOOR);
    /*
     * If the object of our desire is in a different room,
     * and we are not in a corridor, run to the door nearest to
     * our goal.
     */
over:
    if (rer != ree)
    {
	for (cp = rer->r_exit; cp < &rer->r_exit[rer->r_nexits]; cp++)
	{
	    curdist = dist_cp(th->t_dest, cp);
	    if (curdist < mindist)
	    {
		this = *cp;
		mindist = curdist;
	    }
	}
	if (door)
	{
	    rer = &passages[flat(th->t_pos.y, th->t_pos.x) & F_PNUM];
	    door = FALSE;
	    goto over;
	}
    }
    else
    {
	this = *th->t_dest;
	/*
	 * For dragons check and see if (a) the hero is on a straight
	 * line from it, and (b) that it is within shooting distance,
	 * but outside of striking range.
	 */
	if (th->t_type == 'D' && (th->t_pos.y == hero.y || th->t_pos.x == hero.x
	    || abs(th->t_pos.y - hero.y) == abs(th->t_pos.x - hero.x))
	    && dist_cp(&th->t_pos, &hero) <= BOLT_LENGTH * BOLT_LENGTH
	    && !on(*th, ISCANC) && rnd(DRAGONSHOT) == 0)
	{
	    delta.y = sign(hero.y - th->t_pos.y);
	    delta.x = sign(hero.x - th->t_pos.x);
	    if (has_hit)
		endmsg();
	    fire_bolt(&th->t_pos, &delta, "flame");
	    running = FALSE;
	    count = 0;
	    quiet = 0;
	    if (to_death && !on(*th, ISTARGET))
	    {
		to_death = FALSE;
		kamikaze = FALSE;
	    }
	    return(0);
	}
    }
    /*
     * This now contains what we want to run to this time
     * so we run to it.  If we hit it we either want to fight it
     * or stop running
     */
    if (!chase(th, &this))
    {
	if (ce(this, hero))
	{
	    return( attack(th) );
	}
	else if (ce(this, *th->t_dest))
	{
	    for (obj = lvl_obj; obj != NULL; obj = next(obj))
		if (th->t_dest == &obj->o_pos)
		{
		    detach(lvl_obj, obj);
		    attach(th->t_pack, obj);
		    chat(obj->o_pos.y, obj->o_pos.x) =
			(th->t_room->r_flags & ISGONE) ? PASSAGE : FLOOR;
		    th->t_dest = find_dest(th);
		    break;
		}
	    if (th->t_type != 'F')
		stoprun = TRUE;
	}
    }
    else
    {
	if (th->t_type == 'F')
	    return(0);
    }
    relocate(th, &ch_ret);
    /*
     * And stop running if need be
     */
    if (stoprun && ce(th->t_pos, *(th->t_dest)))
	th->t_flags &= ~ISRUN;
    return(0);
}

/*
 * set_oldch:
 *	Set the oldch character for the monster
 */
void
set_oldch(THING *tp, coord *cp)
{
    char sch;

    if (ce(tp->t_pos, *cp))
        return;

    sch = tp->t_oldch;
    tp->t_oldch = CCHAR( mvinch(cp->y,cp->x) );
    if (!on(player, ISBLIND))
    {
	    if ((sch == FLOOR || tp->t_oldch == FLOOR) &&
		(tp->t_room->r_flags & ISDARK))
		    tp->t_oldch = ' ';
	    else if (dist_cp(cp, &hero) <= LAMPDIST && see_floor)
		tp->t_oldch = chat(cp->y, cp->x);
    }
}

/*
 * see_monst:
 *	Return TRUE if the hero can see the monster
 */
bool
see_monst(THING *mp)
{
    int y, x;

    if (on(player, ISBLIND))
	return FALSE;
    if (on(*mp, ISINVIS) && !on(player, CANSEE))
	return FALSE;
    y = mp->t_pos.y;
    x = mp->t_pos.x;
    if (dist(y, x, hero.y, hero.x) < LAMPDIST)
    {
	if (y != hero.y && x != hero.x &&
	    !step_ok(chat(y, hero.x)) && !step_ok(chat(hero.y, x)))
		return FALSE;
	return TRUE;
    }
    if (mp->t_room != proom)
	return FALSE;
    return ((bool)!(mp->t_room->r_flags & ISDARK));
}

/*
 * runto:
 *	Set a monster running after the hero.
 */
void
runto(coord *runner)
{
    register THING *tp;

    /*
     * If we couldn't find him, something is funny
     */
#ifdef MASTER
    if ((tp = moat(runner->y, runner->x)) == NULL)
	msg("couldn't find monster in runto at (%d,%d)", runner->y, runner->x);
#else
    tp = moat(runner->y, runner->x);
#endif
    /*
     * Start the beastie running
     */
    tp->t_flags |= ISRUN;
    tp->t_flags &= ~ISHELD;
    tp->t_dest = find_dest(tp);
}

/*
 * chase:
 *	Find the spot for the chaser(er) to move closer to the
 *	chasee(ee).  Returns TRUE if we want to keep on chasing later
 *	FALSE if we reach the goal.
 */
bool
chase(THING *tp, coord *ee)
{
    register THING *obj;
    register int x, y;
    register int curdist, thisdist;
    register coord *er = &tp->t_pos;
    register char ch;
    register int plcnt = 1;
    static coord tryp;

    /*
     * If the thing is confused, let it move randomly. Invisible
     * Stalkers are slightly confused all of the time, and bats are
     * quite confused all the time
     */
    if ((on(*tp, ISHUH) && rnd(5) != 0) || (tp->t_type == 'P' && rnd(5) == 0)
	|| (tp->t_type == 'B' && rnd(2) == 0))
    {
	/*
	 * get a valid random move
	 */
	ch_ret = *rndmove(tp);
	curdist = dist_cp(&ch_ret, ee);
	/*
	 * Small chance that it will become un-confused 
	 */
	if (rnd(20) == 0)
	    tp->t_flags &= ~ISHUH;
    }
    /*
     * Otherwise, find the empty spot next to the chaser that is
     * closest to the chasee.
     */
    else
    {
	register int ey, ex;
	/*
	 * This will eventually hold where we move to get closer
	 * If we can't find an empty spot, we stay where we are.
	 */
	curdist = dist_cp(er, ee);
	ch_ret = *er;

	ey = er->y + 1;
	if (ey >= NUMLINES - 1)
	    ey = NUMLINES - 2;
	ex = er->x + 1;
	if (ex >= NUMCOLS)
	    ex = NUMCOLS - 1;

	for (x = er->x - 1; x <= ex; x++)
	{
	    if (x < 0)
		continue;
	    tryp.x = x;
	    for (y = er->y - 1; y <= ey; y++)
	    {
		tryp.y = y;
		if (!diag_ok(er, &tryp))
		    continue;
		ch = winat(y, x);
		if (step_ok(ch))
		{
		    /*
		     * If it is a scroll, it might be a scare monster scroll
		     * so we need to look it up to see what type it is.
		     */
		    if (ch == SCROLL)
		    {
			for (obj = lvl_obj; obj != NULL; obj = next(obj))
			{
			    if (y == obj->o_pos.y && x == obj->o_pos.x)
				break;
			}
			if (obj != NULL && obj->o_which == S_SCARE)
			    continue;
		    }
		    /*
		     * It can also be a Xeroc, which we shouldn't step on
		     */
		    if ((obj = moat(y, x)) != NULL && obj->t_type == 'X')
			continue;
		    /*
		     * If we didn't find any scrolls at this place or it
		     * wasn't a scare scroll, then this place counts
		     */
		    thisdist = dist(y, x, ee->y, ee->x);
		    if (thisdist < curdist)
		    {
			plcnt = 1;
			ch_ret = tryp;
			curdist = thisdist;
		    }
		    else if (thisdist == curdist && rnd(++plcnt) == 0)
		    {
			ch_ret = tryp;
			curdist = thisdist;
		    }
		}
	    }
	}
    }
    return (bool)(curdist != 0 && !ce(ch_ret, hero));
}

/*
 * roomin:
 *	Find what room some coordinates are in. NULL means they aren't
 *	in any room.
 */
struct room *
roomin(coord *cp)
{
    register struct room *rp;
    register char *fp;


    fp = &flat(cp->y, cp->x);
    if (*fp & F_PASS)
	return &passages[*fp & F_PNUM];

    for (rp = rooms; rp < &rooms[MAXROOMS]; rp++)
	if (cp->x <= rp->r_pos.x + rp->r_max.x && rp->r_pos.x <= cp->x
	 && cp->y <= rp->r_pos.y + rp->r_max.y && rp->r_pos.y <= cp->y)
	    return rp;

    msg("in some bizarre place (%d, %d)", unc(*cp));
#ifdef MASTER
    abort();
    return NULL;
#else
    return NULL;
#endif
}

/*
 * diag_ok:
 *	Check to see if the move is legal if it is diagonal
 */
bool
diag_ok(coord *sp, coord *ep)
{
    if (ep->x < 0 || ep->x >= NUMCOLS || ep->y <= 0 || ep->y >= NUMLINES - 1)
	return FALSE;
    if (ep->x == sp->x || ep->y == sp->y)
	return TRUE;
    return (bool)(step_ok(chat(ep->y, sp->x)) && step_ok(chat(sp->y, ep->x)));
}

/*
 * cansee:
 *	Returns true if the hero can see a certain coordinate.
 */
bool
cansee(int y, int x)
{
    register struct room *rer;
    static coord tp;

    if (on(player, ISBLIND))
	return FALSE;
    if (dist(y, x, hero.y, hero.x) < LAMPDIST)
    {
	if (flat(y, x) & F_PASS)
	    if (y != hero.y && x != hero.x &&
		!step_ok(chat(y, hero.x)) && !step_ok(chat(hero.y, x)))
		    return FALSE;
	return TRUE;
    }
    /*
     * We can only see if the hero in the same room as
     * the coordinate and the room is lit or if it is close.
     */
    tp.y = y;
    tp.x = x;
    return (bool)((rer = roomin(&tp)) == proom && !(rer->r_flags & ISDARK));
}

/*
 * find_dest:
 *	find the proper destination for the monster
 */
coord *
find_dest(THING *tp)
{
    register THING *obj;
    register int prob;

    if ((prob = monsters[tp->t_type - 'A'].m_carry) <= 0 || tp->t_room == proom
	|| see_monst(tp))
	    return &hero;
    for (obj = lvl_obj; obj != NULL; obj = next(obj))
    {
	if (obj->o_type == SCROLL && obj->o_which == S_SCARE)
	    continue;
	if (roomin(&obj->o_pos) == tp->t_room && rnd(100) < prob)
	{
	    for (tp = mlist; tp != NULL; tp = next(tp))
		if (tp->t_dest == &obj->o_pos)
		    break;
	    if (tp == NULL)
		return &obj->o_pos;
	}
    }
    return &hero;
}

/*
 * dist:
 *	Calculate the "distance" between to points.  Actually,
 *	this calculates d^2, not d, but that's good enough for
 *	our purposes, since it's only used comparitively.
 */
int
dist(int y1, int x1, int y2, int x2)
{
    return ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

/*
 * dist_cp:
 *	Call dist() with appropriate arguments for coord pointers
 */
int
dist_cp(coord *c1, coord *c2)
{
    return dist(c1->y, c1->x, c2->y, c2->x);
}
