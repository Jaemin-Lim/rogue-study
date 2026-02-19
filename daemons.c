/*
 * All the daemon and fuse functions are in here
 * 게임에서 사용되는 구체적인 데몬(daemon)과 퓨즈(fuse) 함수들이 모여 있는 파일.
 * 데몬/퓨즈 시스템 자체는 daemon.c에 구현되어 있다.
 *
 * @(#)daemons.c	4.24 (Berkeley) 02/05/99
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
 * doctor:
 *	A healing daemon that restors hit points after rest
 *	쉬는 동안 체력(HP)을 회복시키는 데몬 함수.
 *	매 턴 실행되며, quiet 카운터가 증가할수록 더 많이 회복된다.
 *	레벨 8 미만: quiet + 2*레벨 > 20이면 1HP 회복
 *	레벨 8 이상: quiet >= 3이면 1~(레벨-7)HP 회복
 *	재생(R_REGEN) 반지를 끼고 있으면 추가로 HP 회복.
 *	회복이 발생하면 quiet를 0으로 초기화.
 */
void
doctor()
{
    register int lv, ohp;  /* 현재 레벨, 이전 HP */

    lv = pstats.s_lvl;   /* 플레이어 레벨 */
    ohp = pstats.s_hpt;  /* 이전 HP 값 */
    quiet++;  /* 조용한 턴 수 증가 (싸우지 않을수록 회복 빠름) */
    if (lv < 8)  /* 저레벨 회복 공식 */
    {
	if (quiet + (lv << 1) > 20)  /* quiet + 2*레벨 > 20 */
	    pstats.s_hpt++;
    }
    else  /* 고레벨 회복 공식 */
	if (quiet >= 3)
	    pstats.s_hpt += rnd(lv - 7) + 1;  /* 1~(레벨-7) HP 회복 */
    /* 재생 반지 효과: 각 반지마다 1HP 추가 회복 */
    if (ISRING(LEFT, R_REGEN))
	pstats.s_hpt++;
    if (ISRING(RIGHT, R_REGEN))
	pstats.s_hpt++;
    if (ohp != pstats.s_hpt)  /* HP가 변화했으면 */
    {
	if (pstats.s_hpt > max_hp)  /* 최대 HP 초과 방지 */
	    pstats.s_hpt = max_hp;
	quiet = 0;  /* 회복 발생 시 quiet 초기화 (연속 회복 방지) */
    }
}

/*
 * Swander:
 *	Called when it is time to start rolling for wandering monsters
 *	방랑 몬스터 생성 주기가 도래했을 때 호출되는 함수.
 *	rollwand 데몬을 시작하여 방랑 몬스터 생성 주사위를 굴리기 시작한다.
 */
void
swander()
{
    start_daemon(rollwand, 0, BEFORE);  /* 방랑 몬스터 생성 시도 데몬 시작 */
}

/*
 * rollwand:
 *	Called to roll to see if a wandering monster starts up
 *	매 4턴마다 1/6 확률로 방랑 몬스터를 생성하는 데몬 함수.
 *	방랑 몬스터가 생성되면 rollwand 데몬을 종료하고
 *	WANDERTIME 후 다시 swander를 발동시키는 퓨즈를 설정한다.
 */
int between = 0;  /* 마지막 주사위 굴림 이후 경과 턴 수 */
void
rollwand()
{

    if (++between >= 4)  /* 4턴마다 한 번 시도 */
    {
	if (roll(1, 6) == 4)  /* 1/6 확률로 방랑 몬스터 생성 */
	{
	    wanderer();  /* 방랑 몬스터 생성 (monsters.c 참조) */
	    kill_daemon(rollwand);  /* rollwand 데몬 종료 */
	    fuse(swander, 0, WANDERTIME, BEFORE);  /* 다음 방랑 몬스터 타이머 설정 */
	}
	between = 0;  /* 카운터 초기화 */
    }
}

/*
 * unconfuse:
 *	Release the poor player from his confusion
 *	플레이어의 혼란(confusion) 상태를 해제하는 퓨즈 함수.
 *	ISHUH 플래그를 해제하고 메시지를 표시한다.
 *	환각(ISHALU) 상태이면 다른 메시지를 표시한다.
 */
void
unconfuse()
{
    player.t_flags &= ~ISHUH;  /* 혼란 플래그 해제 */
    /* 환각 상태이면 "trippy" 메시지, 아니면 "confused" 메시지 */
    msg("you feel less %s now", choose_str("trippy", "confused"));
}

/*
 * unsee:
 *	Turn off the ability to see invisible
 *	투명 감지(see invisible) 능력을 해제하는 퓨즈 함수.
 *	CANSEE 플래그를 해제하고, 보이지 않는 몬스터들을 숨긴다.
 */
void
unsee()
{
    register THING *th;  /* 몬스터 포인터 */

    /* 투명 몬스터 중 현재 보이는 것들을 화면에서 숨긴다 */
    for (th = mlist; th != NULL; th = next(th))
	if (on(*th, ISINVIS) && see_monst(th))
	    mvaddch(th->t_pos.y, th->t_pos.x, th->t_oldch);  /* 배경 문자로 복원 */
    player.t_flags &= ~CANSEE;  /* 투명 감지 플래그 해제 */
}

/*
 * sight:
 *	He gets his sight back
 *	실명(ISBLIND) 상태를 해제하고 시야를 회복하는 퓨즈 함수.
 *	실명이 해제되면 현재 방을 다시 표시한다.
 */
void
sight()
{
    if (on(player, ISBLIND))  /* 아직 실명 상태이면 */
    {
	extinguish(sight);  /* 중복 발동 방지를 위해 퓨즈 제거 */
	player.t_flags &= ~ISBLIND;  /* 실명 플래그 해제 */
	if (!(proom->r_flags & ISGONE))  /* 복도가 아닌 방이면 */
	    enter_room(&hero);  /* 방 재표시 (rooms.c 참조) */
	/* 환각 상태이면 다른 메시지 표시 */
	msg(choose_str("far out!  Everything is all cosmic again",
		       "the veil of darkness lifts"));
    }
}

/*
 * nohaste:
 *	End the hasting
 *	속도 증가(haste) 효과를 종료하는 퓨즈 함수.
 *	ISHASTE 플래그를 해제하고 메시지를 표시한다.
 */
void
nohaste()
{
    player.t_flags &= ~ISHASTE;  /* 속도 증가 플래그 해제 */
    msg("you feel yourself slowing down");
}

/*
 * stomach:
 *	Digest the hero's food
 *	매 턴 음식을 소화시키는 데몬 함수.
 *	food_left가 감소하며, 부족하면 배고픔 단계를 높인다.
 *	food_left <= 0: 기절(faint) 상태 가능, 충분히 지나면 사망
 *	반지 효과(R_DIGEST): 음식 소화 속도 영향
 *	부적(amulet)을 갖고 있으면 소화가 느려짐 (-1 보정)
 */
void
stomach()
{
    register int oldfood;        /* 이전 food_left 값 */
    int orig_hungry = hungry_state; /* 이전 배고픔 상태 */

    if (food_left <= 0)  /* 음식이 없는 경우 */
    {
	if (food_left-- < -STARVETIME)  /* 기아 한계치를 넘으면 사망 */
	    death('s');  /* 굶어 죽음 */
	/*
	 * the hero is fainting
	 * 기절 상태: 일정 확률로 no_command 증가 (행동 불능)
	 */
	if (no_command || rnd(5) != 0)
	    return;
	no_command += rnd(8) + 4;  /* 4~11턴간 행동 불능 */
	hungry_state = 3;  /* 최고 배고픔 단계: Faint */
	if (!terse)
	    addmsg(choose_str("the munchies overpower your motor capabilities.  ",
			      "you feel too weak from lack of food.  "));
	msg(choose_str("You freak out", "You faint"));
    }
    else  /* 음식이 남아 있는 경우 */
    {
	oldfood = food_left;
	/* 반지 효과와 부적 여부를 고려한 소화 */
	food_left -= ring_eat(LEFT) + ring_eat(RIGHT) + 1 - amulet;

	/* 배고픔 단계 2: Weak (MORETIME ~ 2*MORETIME) */
	if (food_left < MORETIME && oldfood >= MORETIME)
	{
	    hungry_state = 2;
	    msg(choose_str("the munchies are interfering with your motor capabilites",
			   "you are starting to feel weak"));
	}
	/* 배고픔 단계 1: Hungry (2*MORETIME ~ 3*MORETIME) */
	else if (food_left < 2 * MORETIME && oldfood >= 2 * MORETIME)
	{
	    hungry_state = 1;
	    if (terse)
		msg(choose_str("getting the munchies", "getting hungry"));
	    else
		msg(choose_str("you are getting the munchies",
			       "you are starting to get hungry"));
	}
    }
    /* 배고픔 상태가 변화하면 이동/달리기 중단 */
    if (hungry_state != orig_hungry) { 
        player.t_flags &= ~ISRUN; 
        running = FALSE; 
        to_death = FALSE; 
        count = 0; 
    } 
}

/*
 * come_down:
 *	Take the hero down off her acid trip.
 *	환각(hallucination, ISHALU) 상태를 해제하는 퓨즈 함수.
 *	환각 중 변형된 화면(아이템, 몬스터 외관)을 실제 외관으로 복원한다.
 */
void
come_down()
{
    register THING *tp;       /* 아이템/몬스터 포인터 */
    register bool seemonst;   /* 몬스터 감지 상태 */

    if (!on(player, ISHALU))  /* 환각 상태가 아니면 처리 불필요 */
	return;

    kill_daemon(visuals);  /* 환각 시각 효과 데몬 종료 */
    player.t_flags &= ~ISHALU;  /* 환각 플래그 해제 */

    if (on(player, ISBLIND))  /* 실명 상태이면 화면 복원 불필요 */
	return;

    /*
     * undo the things
     * 레벨의 아이템들을 실제 외관으로 복원
     */
    for (tp = lvl_obj; tp != NULL; tp = next(tp))
	if (cansee(tp->o_pos.y, tp->o_pos.x))
	    mvaddch(tp->o_pos.y, tp->o_pos.x, tp->o_type);  /* 실제 아이템 문자로 */

    /*
     * undo the monsters
     * 몬스터들을 실제 외관으로 복원
     */
    seemonst = on(player, SEEMONST);
    for (tp = mlist; tp != NULL; tp = next(tp))
    {
	move(tp->t_pos.y, tp->t_pos.x);
	if (cansee(tp->t_pos.y, tp->t_pos.x))
	    if (!on(*tp, ISINVIS) || on(player, CANSEE))
		addch(tp->t_disguise);  /* 실제 몬스터 위장 문자로 */
	    else
		addch(chat(tp->t_pos.y, tp->t_pos.x));  /* 투명 몬스터는 배경으로 */
	else if (seemonst)  /* 몬스터 감지 중이면 강조 표시 */
	{
	    standout();
	    addch(tp->t_type);
	    standend();
	}
    }
    msg("Everything looks SO boring now.");
}

/*
 * visuals:
 *	change the characters for the player
 *	환각 상태에서 화면의 문자들을 무작위로 변경하는 데몬 함수.
 *	아이템, 계단, 몬스터의 외관을 임의로 바꾸어 혼란을 준다.
 */
void
visuals()
{
    register THING *tp;
    register bool seemonst;

    if (!after || (running && jump))  /* 행동 후가 아니거나 달리기 중에 점프 모드이면 건너뜀 */
	return;
    /*
     * change the things
     * 레벨의 아이템들을 무작위 문자로 변경
     */
    for (tp = lvl_obj; tp != NULL; tp = next(tp))
	if (cansee(tp->o_pos.y, tp->o_pos.x))
	    mvaddch(tp->o_pos.y, tp->o_pos.x, rnd_thing());  /* 랜덤 아이템 문자 */

    /*
     * change the stairs
     * 계단을 무작위 문자로 변경
     */
    if (!seenstairs && cansee(stairs.y, stairs.x))
	mvaddch(stairs.y, stairs.x, rnd_thing());

    /*
     * change the monsters
     * 몬스터들을 무작위 대문자로 변경
     */
    seemonst = on(player, SEEMONST);
    for (tp = mlist; tp != NULL; tp = next(tp))
    {
	move(tp->t_pos.y, tp->t_pos.x);
	if (see_monst(tp))
	{
	    if (tp->t_type == 'X' && tp->t_disguise != 'X')
		addch(rnd_thing());  /* 제록(Xeroc)은 랜덤 아이템 문자로 */
	    else
		addch(rnd(26) + 'A');  /* 다른 몬스터는 랜덤 대문자로 */
	}
	else if (seemonst)
	{
	    standout();
	    addch(rnd(26) + 'A');
	    standend();
	}
    }
}

/*
 * land:
 *	Land from a levitation potion
 *	공중 부양(levitation) 효과가 끝나 착지하는 퓨즈 함수.
 *	ISLEVIT 플래그를 해제하고 착지 메시지를 표시한다.
 */
void
land()
{
    player.t_flags &= ~ISLEVIT;  /* 공중 부양 플래그 해제 */
    /* 환각 상태이면 "bummer" 메시지, 아니면 정상 착지 메시지 */
    msg(choose_str("bummer!  You've hit the ground",
		   "you float gently to the ground"));
}

#include <curses.h>
#include "rogue.h"

/*
 * doctor:
 *	A healing daemon that restors hit points after rest
 */
void
doctor()
{
    register int lv, ohp;

    lv = pstats.s_lvl;
    ohp = pstats.s_hpt;
    quiet++;
    if (lv < 8)
    {
	if (quiet + (lv << 1) > 20)
	    pstats.s_hpt++;
    }
    else
	if (quiet >= 3)
	    pstats.s_hpt += rnd(lv - 7) + 1;
    if (ISRING(LEFT, R_REGEN))
	pstats.s_hpt++;
    if (ISRING(RIGHT, R_REGEN))
	pstats.s_hpt++;
    if (ohp != pstats.s_hpt)
    {
	if (pstats.s_hpt > max_hp)
	    pstats.s_hpt = max_hp;
	quiet = 0;
    }
}

/*
 * Swander:
 *	Called when it is time to start rolling for wandering monsters
 */
void
swander()
{
    start_daemon(rollwand, 0, BEFORE);
}

/*
 * rollwand:
 *	Called to roll to see if a wandering monster starts up
 */
int between = 0;
void
rollwand()
{

    if (++between >= 4)
    {
	if (roll(1, 6) == 4)
	{
	    wanderer();
	    kill_daemon(rollwand);
	    fuse(swander, 0, WANDERTIME, BEFORE);
	}
	between = 0;
    }
}

/*
 * unconfuse:
 *	Release the poor player from his confusion
 */
void
unconfuse()
{
    player.t_flags &= ~ISHUH;
    msg("you feel less %s now", choose_str("trippy", "confused"));
}

/*
 * unsee:
 *	Turn off the ability to see invisible
 */
void
unsee()
{
    register THING *th;

    for (th = mlist; th != NULL; th = next(th))
	if (on(*th, ISINVIS) && see_monst(th))
	    mvaddch(th->t_pos.y, th->t_pos.x, th->t_oldch);
    player.t_flags &= ~CANSEE;
}

/*
 * sight:
 *	He gets his sight back
 */
void
sight()
{
    if (on(player, ISBLIND))
    {
	extinguish(sight);
	player.t_flags &= ~ISBLIND;
	if (!(proom->r_flags & ISGONE))
	    enter_room(&hero);
	msg(choose_str("far out!  Everything is all cosmic again",
		       "the veil of darkness lifts"));
    }
}

/*
 * nohaste:
 *	End the hasting
 */
void
nohaste()
{
    player.t_flags &= ~ISHASTE;
    msg("you feel yourself slowing down");
}

/*
 * stomach:
 *	Digest the hero's food
 */
void
stomach()
{
    register int oldfood;
    int orig_hungry = hungry_state;

    if (food_left <= 0)
    {
	if (food_left-- < -STARVETIME)
	    death('s');
	/*
	 * the hero is fainting
	 */
	if (no_command || rnd(5) != 0)
	    return;
	no_command += rnd(8) + 4;
	hungry_state = 3;
	if (!terse)
	    addmsg(choose_str("the munchies overpower your motor capabilities.  ",
			      "you feel too weak from lack of food.  "));
	msg(choose_str("You freak out", "You faint"));
    }
    else
    {
	oldfood = food_left;
	food_left -= ring_eat(LEFT) + ring_eat(RIGHT) + 1 - amulet;

	if (food_left < MORETIME && oldfood >= MORETIME)
	{
	    hungry_state = 2;
	    msg(choose_str("the munchies are interfering with your motor capabilites",
			   "you are starting to feel weak"));
	}
	else if (food_left < 2 * MORETIME && oldfood >= 2 * MORETIME)
	{
	    hungry_state = 1;
	    if (terse)
		msg(choose_str("getting the munchies", "getting hungry"));
	    else
		msg(choose_str("you are getting the munchies",
			       "you are starting to get hungry"));
	}
    }
    if (hungry_state != orig_hungry) { 
        player.t_flags &= ~ISRUN; 
        running = FALSE; 
        to_death = FALSE; 
        count = 0; 
    } 
}

/*
 * come_down:
 *	Take the hero down off her acid trip.
 */
void
come_down()
{
    register THING *tp;
    register bool seemonst;

    if (!on(player, ISHALU))
	return;

    kill_daemon(visuals);
    player.t_flags &= ~ISHALU;

    if (on(player, ISBLIND))
	return;

    /*
     * undo the things
     */
    for (tp = lvl_obj; tp != NULL; tp = next(tp))
	if (cansee(tp->o_pos.y, tp->o_pos.x))
	    mvaddch(tp->o_pos.y, tp->o_pos.x, tp->o_type);

    /*
     * undo the monsters
     */
    seemonst = on(player, SEEMONST);
    for (tp = mlist; tp != NULL; tp = next(tp))
    {
	move(tp->t_pos.y, tp->t_pos.x);
	if (cansee(tp->t_pos.y, tp->t_pos.x))
	    if (!on(*tp, ISINVIS) || on(player, CANSEE))
		addch(tp->t_disguise);
	    else
		addch(chat(tp->t_pos.y, tp->t_pos.x));
	else if (seemonst)
	{
	    standout();
	    addch(tp->t_type);
	    standend();
	}
    }
    msg("Everything looks SO boring now.");
}

/*
 * visuals:
 *	change the characters for the player
 */
void
visuals()
{
    register THING *tp;
    register bool seemonst;

    if (!after || (running && jump))
	return;
    /*
     * change the things
     */
    for (tp = lvl_obj; tp != NULL; tp = next(tp))
	if (cansee(tp->o_pos.y, tp->o_pos.x))
	    mvaddch(tp->o_pos.y, tp->o_pos.x, rnd_thing());

    /*
     * change the stairs
     */
    if (!seenstairs && cansee(stairs.y, stairs.x))
	mvaddch(stairs.y, stairs.x, rnd_thing());

    /*
     * change the monsters
     */
    seemonst = on(player, SEEMONST);
    for (tp = mlist; tp != NULL; tp = next(tp))
    {
	move(tp->t_pos.y, tp->t_pos.x);
	if (see_monst(tp))
	{
	    if (tp->t_type == 'X' && tp->t_disguise != 'X')
		addch(rnd_thing());
	    else
		addch(rnd(26) + 'A');
	}
	else if (seemonst)
	{
	    standout();
	    addch(rnd(26) + 'A');
	    standend();
	}
    }
}

/*
 * land:
 *	Land from a levitation potion
 */
void
land()
{
    player.t_flags &= ~ISLEVIT;
    msg(choose_str("bummer!  You've hit the ground",
		   "you float gently to the ground"));
}
