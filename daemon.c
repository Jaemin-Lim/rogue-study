/*
 * Contains functions for dealing with things that happen in the
 * future.
 * 미래에 발생할 이벤트를 처리하는 데몬(daemon)과 퓨즈(fuse) 시스템을 구현한 파일.
 *
 * 데몬(DAEMON): 매 턴마다 반복적으로 실행되는 함수 (예: 체력 회복, 몬스터 이동)
 * 퓨즈(fuse): 일정 턴 후 한 번만 실행되는 타이머 함수 (예: 혼란 종료, 속도 상승 종료)
 *
 * @(#)daemon.c	4.7 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <curses.h>
#include "rogue.h"

#define EMPTY 0   /* 슬롯이 비어 있음을 나타내는 상수 */
#define DAEMON -1 /* 데몬(반복 실행)을 나타내는 특수 시간 값 */
#define MAXDAEMONS 20  /* 동시에 활성화될 수 있는 최대 데몬/퓨즈 수 */

#define _X_ { EMPTY }  /* 빈 슬롯 초기화용 매크로 */

/* d_list: 지연 액션(데몬/퓨즈)의 목록 배열 */
/* delayed_action 구조체 (rogue.h 참조):
 *   d_type: EMPTY(비어있음), BEFORE(행동 전), AFTER(행동 후)
 *   d_func: 실행할 함수 포인터
 *   d_arg:  함수에 전달할 인수
 *   d_time: DAEMON(-1)이면 데몬(매 턴 실행), 양수이면 퓨즈(해당 턴 후 실행) */
struct delayed_action d_list[MAXDAEMONS] = {
    _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_,
    _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, 
};

/*
 * d_slot:
 *	Find an empty slot in the daemon/fuse list
 *	데몬/퓨즈 목록에서 빈 슬롯을 찾아 반환하는 함수.
 *	사용 가능한 슬롯이 없으면 NULL 반환 (MASTER 모드에서는 디버그 메시지 출력).
 */
struct delayed_action *
d_slot()
{
    register struct delayed_action *dev;  /* 슬롯 포인터 */

    for (dev = d_list; dev <= &d_list[MAXDAEMONS-1]; dev++)
	if (dev->d_type == EMPTY)  /* 비어 있는 슬롯 발견 */
	    return dev;
#ifdef MASTER
    debug("Ran out of fuse slots");
#endif
    return NULL;
}

/*
 * find_slot:
 *	Find a particular slot in the table
 *	특정 함수에 해당하는 슬롯을 찾아 반환하는 함수.
 *	lengthen(), extinguish(), kill_daemon() 등에서 사용된다.
 */
struct delayed_action *
find_slot(void (*func)())
{
    register struct delayed_action *dev;

    for (dev = d_list; dev <= &d_list[MAXDAEMONS-1]; dev++)
	if (dev->d_type != EMPTY && func == dev->d_func)  /* 함수 포인터 일치 */
	    return dev;
    return NULL;
}

/*
 * start_daemon:
 *	Start a daemon, takes a function.
 *	데몬을 시작하는 함수. 매 턴마다 반복적으로 실행된다.
 *	func: 실행할 함수, arg: 인수, type: BEFORE 또는 AFTER
 */
void
start_daemon(void (*func)(), int arg, int type)
{
    register struct delayed_action *dev;

    dev = d_slot();   /* 빈 슬롯 획득 */
    dev->d_type = type;  /* 실행 시점 (BEFORE: 행동 전, AFTER: 행동 후) */
    dev->d_func = func;  /* 실행할 함수 */
    dev->d_arg = arg;    /* 함수 인수 */
    dev->d_time = DAEMON; /* -1: 데몬임을 표시 (매 턴 실행) */
}

/*
 * kill_daemon:
 *	Remove a daemon from the list
 *	활성화된 데몬을 목록에서 제거하는 함수.
 *	슬롯을 EMPTY로 표시하여 비활성화한다.
 */
void
kill_daemon(void (*func)())
{
    register struct delayed_action *dev;

    if ((dev = find_slot(func)) == NULL)  /* 해당 데몬 슬롯 찾기 */
	return;
    /*
     * Take it out of the list
     * 목록에서 제거 (EMPTY로 표시)
     */
    dev->d_type = EMPTY;
}

/*
 * do_daemons:
 *	Run all the daemons that are active with the current flag,
 *	passing the argument to the function.
 *	지정된 플래그(flag)에 해당하는 활성 데몬들을 모두 실행하는 함수.
 *	flag가 BEFORE이면 플레이어 행동 전에, AFTER이면 행동 후에 실행된다.
 *	d_time이 DAEMON(-1)인 슬롯만 실행한다 (퓨즈 제외).
 */
void
do_daemons(int flag)
{
    register struct delayed_action *dev;

    /*
     * Loop through the devil list
     * 데몬 목록을 순회
     */
    for (dev = d_list; dev <= &d_list[MAXDAEMONS-1]; dev++)
	/*
	 * Executing each one, giving it the proper arguments
	 * 해당 플래그를 가진 데몬을 실행
	 */
	if (dev->d_type == flag && dev->d_time == DAEMON)
	    (*dev->d_func)(dev->d_arg);  /* 함수 포인터를 통해 함수 실행 */
}

/*
 * fuse:
 *	Start a fuse to go off in a certain number of turns
 *	일정 턴 후에 한 번만 실행되는 퓨즈를 시작하는 함수.
 *	func: 실행할 함수, arg: 인수, time: 실행까지 남은 턴 수, type: BEFORE/AFTER
 */
void
fuse(void (*func)(), int arg, int time, int type)
{
    register struct delayed_action *wire;  /* 퓨즈 슬롯 포인터 */

    wire = d_slot();       /* 빈 슬롯 획득 */
    wire->d_type = type;   /* 실행 시점 */
    wire->d_func = func;   /* 실행할 함수 */
    wire->d_arg = arg;     /* 함수 인수 */
    wire->d_time = time;   /* 실행까지 남은 턴 수 */
}

/*
 * lengthen:
 *	Increase the time until a fuse goes off
 *	퓨즈가 발동되기까지의 시간을 늘리는 함수.
 *	예: 포션 효과가 중복될 때 지속 시간 연장.
 */
void
lengthen(void (*func)(), int xtime)
{
    register struct delayed_action *wire;

    if ((wire = find_slot(func)) == NULL)  /* 해당 퓨즈 슬롯 찾기 */
	return;
    wire->d_time += xtime;  /* 남은 시간에 추가 시간 더하기 */
}

/*
 * extinguish:
 *	Put out a fuse
 *	활성화된 퓨즈를 끄는 함수 (취소).
 *	슬롯을 EMPTY로 표시하여 비활성화한다.
 */
void
extinguish(void (*func)())
{
    register struct delayed_action *wire;

    if ((wire = find_slot(func)) == NULL)
	return;
    wire->d_type = EMPTY;  /* 퓨즈 비활성화 */
}

/*
 * do_fuses:
 *	Decrement counters and start needed fuses
 *	지정된 플래그(flag)에 해당하는 퓨즈들의 카운터를 감소시키고,
 *	카운터가 0이 된 퓨즈를 실행하는 함수.
 *	실행된 퓨즈는 목록에서 제거된다.
 */
void
do_fuses(int flag)
{
    register struct delayed_action *wire;

    /*
     * Step though the list
     * 퓨즈 목록을 순회
     */
    for (wire = d_list; wire <= &d_list[MAXDAEMONS-1]; wire++)
	/*
	 * Decrementing counters and starting things we want.  We also need
	 * to remove the fuse from the list once it has gone off.
	 * 카운터를 줄이고, 0이 되면 함수를 실행 후 목록에서 제거
	 */
	if (flag == wire->d_type && wire->d_time > 0 && --wire->d_time == 0)
	{
	    wire->d_type = EMPTY;  /* 퓨즈 제거 (먼저 제거하여 재진입 방지) */
	    (*wire->d_func)(wire->d_arg);  /* 퓨즈 함수 실행 */
	}
}

#include <curses.h>
#include "rogue.h"

#define EMPTY 0
#define DAEMON -1
#define MAXDAEMONS 20

#define _X_ { EMPTY }

struct delayed_action d_list[MAXDAEMONS] = {
    _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_,
    _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, 
};

/*
 * d_slot:
 *	Find an empty slot in the daemon/fuse list
 */
struct delayed_action *
d_slot()
{
    register struct delayed_action *dev;

    for (dev = d_list; dev <= &d_list[MAXDAEMONS-1]; dev++)
	if (dev->d_type == EMPTY)
	    return dev;
#ifdef MASTER
    debug("Ran out of fuse slots");
#endif
    return NULL;
}

/*
 * find_slot:
 *	Find a particular slot in the table
 */
struct delayed_action *
find_slot(void (*func)())
{
    register struct delayed_action *dev;

    for (dev = d_list; dev <= &d_list[MAXDAEMONS-1]; dev++)
	if (dev->d_type != EMPTY && func == dev->d_func)
	    return dev;
    return NULL;
}

/*
 * start_daemon:
 *	Start a daemon, takes a function.
 */
void
start_daemon(void (*func)(), int arg, int type)
{
    register struct delayed_action *dev;

    dev = d_slot();
    dev->d_type = type;
    dev->d_func = func;
    dev->d_arg = arg;
    dev->d_time = DAEMON;
}

/*
 * kill_daemon:
 *	Remove a daemon from the list
 */
void
kill_daemon(void (*func)())
{
    register struct delayed_action *dev;

    if ((dev = find_slot(func)) == NULL)
	return;
    /*
     * Take it out of the list
     */
    dev->d_type = EMPTY;
}

/*
 * do_daemons:
 *	Run all the daemons that are active with the current flag,
 *	passing the argument to the function.
 */
void
do_daemons(int flag)
{
    register struct delayed_action *dev;

    /*
     * Loop through the devil list
     */
    for (dev = d_list; dev <= &d_list[MAXDAEMONS-1]; dev++)
	/*
	 * Executing each one, giving it the proper arguments
	 */
	if (dev->d_type == flag && dev->d_time == DAEMON)
	    (*dev->d_func)(dev->d_arg);
}

/*
 * fuse:
 *	Start a fuse to go off in a certain number of turns
 */
void
fuse(void (*func)(), int arg, int time, int type)
{
    register struct delayed_action *wire;

    wire = d_slot();
    wire->d_type = type;
    wire->d_func = func;
    wire->d_arg = arg;
    wire->d_time = time;
}

/*
 * lengthen:
 *	Increase the time until a fuse goes off
 */
void
lengthen(void (*func)(), int xtime)
{
    register struct delayed_action *wire;

    if ((wire = find_slot(func)) == NULL)
	return;
    wire->d_time += xtime;
}

/*
 * extinguish:
 *	Put out a fuse
 */
void
extinguish(void (*func)())
{
    register struct delayed_action *wire;

    if ((wire = find_slot(func)) == NULL)
	return;
    wire->d_type = EMPTY;
}

/*
 * do_fuses:
 *	Decrement counters and start needed fuses
 */
void
do_fuses(int flag)
{
    register struct delayed_action *wire;

    /*
     * Step though the list
     */
    for (wire = d_list; wire <= &d_list[MAXDAEMONS-1]; wire++)
	/*
	 * Decrementing counters and starting things we want.  We also need
	 * to remove the fuse from the list once it has gone off.
	 */
	if (flag == wire->d_type && wire->d_time > 0 && --wire->d_time == 0)
	{
	    wire->d_type = EMPTY;
	    (*wire->d_func)(wire->d_arg);
	}
}
