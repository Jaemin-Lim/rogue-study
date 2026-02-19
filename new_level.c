/*
 * new_level:
 *	Dig and draw a new level
 *	새로운 던전 레벨을 생성하고 화면에 그리는 함수들을 담은 파일.
 *
 * @(#)new_level.c	4.38 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <curses.h>
#include <string.h>
#include "rogue.h"

#define TREAS_ROOM 20	/* one chance in TREAS_ROOM for a treasure room */
                        /* 보물 방이 생성될 확률: 1/TREAS_ROOM */
#define MAXTREAS 10	/* maximum number of treasures in a treasure room */
                    /* 보물 방의 최대 아이템 수 */
#define MINTREAS 2	/* minimum number of treasures in a treasure room */
                    /* 보물 방의 최소 아이템 수 */

/*
 * new_level:
 *	새 레벨 생성 및 초기화 함수.
 *	이전 레벨의 몬스터, 아이템, 맵 정보를 모두 지우고
 *	새 레벨을 생성한다.
 *	- places 배열 초기화 (맵 데이터)
 *	- 이전 레벨 몬스터/아이템 메모리 해제
 *	- 방, 통로 생성
 *	- 함정, 계단 배치
 *	- 플레이어 시작 위치 설정
 */
void
new_level()
{
    THING *tp;   /* 몬스터/아이템 포인터 */
    PLACE *pp;   /* 맵 위치 포인터 */
    char *sp;    /* 맵 플래그 포인터 */
    int i;

    player.t_flags &= ~ISHELD;	/* unhold when you go down just in case */
                                /* 레벨 이동 시 잡힌(ISHELD) 상태 해제 */
    if (level > max_level)  /* 최고 레벨 갱신 */
	max_level = level;
    /*
     * Clean things off from last level
     * 이전 레벨 맵 데이터 초기화
     */
    for (pp = places; pp < &places[MAXCOLS*MAXLINES]; pp++)
    {
	pp->p_ch = ' ';         /* 문자를 빈 공간으로 초기화 */
	pp->p_flags = F_REAL;   /* F_REAL: 실제 표시와 동일함을 표시 */
	pp->p_monst = NULL;     /* 몬스터 포인터 초기화 */
    }
    clear();  /* 화면 지우기 */
    /*
     * Free up the monsters on the last level
     * 이전 레벨 몬스터들의 배낭과 몬스터 리스트 메모리 해제
     */
    for (tp = mlist; tp != NULL; tp = next(tp))
	free_list(tp->t_pack);  /* 몬스터 배낭 메모리 해제 */
    free_list(mlist);           /* 몬스터 리스트 메모리 해제 */
    /*
     * Throw away stuff left on the previous level (if anything)
     * 이전 레벨에 남은 아이템들 메모리 해제
     */
    free_list(lvl_obj);
    do_rooms();				/* Draw rooms */
                            /* 방 생성 및 그리기 (rooms.c 참조) */
    do_passages();			/* Draw passages */
                            /* 통로 생성 및 그리기 (passages.c 참조) */
    no_food++;  /* 음식 없는 레벨 카운터 증가 (아이템 생성 시 음식 확률 증가) */
    put_things();			/* Place objects (if any) */
                            /* 아이템 배치 */
    /*
     * Place the traps
     * 함정 배치: 레벨이 높을수록 더 많은 함정이 생성된다
     */
    if (rnd(10) < level)  /* 레벨이 높을수록 함정 생성 확률 증가 */
    {
	ntraps = rnd(level / 4) + 1;  /* 함정 개수: 1 ~ 레벨/4 */
	if (ntraps > MAXTRAPS)
	    ntraps = MAXTRAPS;
	i = ntraps;
	while (i--)
	{
	    /*
	     * not only wouldn't it be NICE to have traps in mazes
	     * (not that we care about being nice), since the trap
	     * number is stored where the passage number is, we
	     * can't actually do it.
	     * 미로(maze) 방에는 함정을 놓을 수 없다
	     * (함정 번호와 통로 번호가 같은 플래그 비트를 사용하기 때문)
	     */
	    do
	    {
		find_floor((struct room *) NULL, &stairs, FALSE, FALSE);
	    } while (chat(stairs.y, stairs.x) != FLOOR);  /* 바닥 위치만 선택 */
	    sp = &flat(stairs.y, stairs.x);
	    *sp &= ~F_REAL;       /* F_REAL 플래그 해제 (숨겨진 함정) */
	    *sp |= rnd(NTRAPS);   /* 랜덤 함정 종류 설정 (F_TMASK 비트에 저장) */
	}
    }
    /*
     * Place the staircase down.
     * 내려가는 계단 배치
     */
    find_floor((struct room *) NULL, &stairs, FALSE, FALSE);
    chat(stairs.y, stairs.x) = STAIRS;  /* 계단 문자('%') 설정 */
    seenstairs = FALSE;  /* 계단 발견 여부 초기화 */

    /* 모든 몬스터의 방 정보 갱신 */
    for (tp = mlist; tp != NULL; tp = next(tp))
	tp->t_room = roomin(&tp->t_pos);

    /* 플레이어 시작 위치 찾기 및 배치 */
    find_floor((struct room *) NULL, &hero, FALSE, TRUE);
    enter_room(&hero);    /* 방 진입 처리 (rooms.c 참조) */
    mvaddch(hero.y, hero.x, PLAYER);  /* 플레이어('@') 표시 */
    if (on(player, SEEMONST))  /* 몬스터 감지 능력이 있으면 */
	turn_see(FALSE);  /* 모든 몬스터 강조 표시 */
    if (on(player, ISHALU))  /* 환각 상태이면 */
	visuals();  /* 환각 시각 효과 적용 */
}

/*
 * rnd_room:
 *	Pick a room that is really there
 *	실제로 존재하는 방(ISGONE 플래그가 없는 방)을 무작위로 선택하는 함수.
 *	ISGONE 방은 실제 방이 아니라 통로 연결점이다.
 */
int
rnd_room()
{
    int rm;

    do
    {
	rm = rnd(MAXROOMS);  /* 0 ~ MAXROOMS-1 사이의 랜덤 방 번호 */
    } while (rooms[rm].r_flags & ISGONE);  /* 없어진 방이면 다시 선택 */
    return rm;
}

/*
 * put_things:
 *	Put potions and scrolls on this level
 *	현재 레벨에 아이템을 배치하는 함수.
 *	부적(amulet)을 이미 가지고 있고 이전에 갔던 레벨보다 높으면 아이템 없음.
 *	- TREAS_ROOM(1/20) 확률로 보물 방 생성
 *	- MAXOBJ(9)번 시도하여 각 36% 확률로 아이템 배치
 *	- AMULETLEVEL 이상이고 아직 부적이 없으면 부적 배치
 */

void
put_things()
{
    int i;
    THING *obj;

    /*
     * Once you have found the amulet, the only way to get new stuff is
     * go down into the dungeon.
     * 부적을 찾은 후에는 더 깊이 내려가야만 새 아이템을 얻을 수 있다.
     */
    if (amulet && level < max_level)
	return;
    /*
     * check for treasure rooms, and if so, put it in.
     * 보물 방 생성 여부 확인 (1/TREAS_ROOM 확률)
     */
    if (rnd(TREAS_ROOM) == 0)
	treas_room();
    /*
     * Do MAXOBJ attempts to put things on a level
     * MAXOBJ번 시도하여 아이템 배치 (각 36% 확률)
     */
    for (i = 0; i < MAXOBJ; i++)
	if (rnd(100) < 36)  /* 36% 확률로 아이템 생성 */
	{
	    /*
	     * Pick a new object and link it in the list
	     * 새 아이템 생성 및 레벨 아이템 리스트에 추가
	     */
	    obj = new_thing();  /* 랜덤 아이템 생성 (things.c 참조) */
	    attach(lvl_obj, obj);
	    /*
	     * Put it somewhere
	     * 아이템을 레벨의 임의 바닥 위치에 배치
	     */
	    find_floor((struct room *) NULL, &obj->o_pos, FALSE, FALSE);
	    chat(obj->o_pos.y, obj->o_pos.x) = (char) obj->o_type;  /* 맵에 아이템 문자 표시 */
	}
    /*
     * If he is really deep in the dungeon and he hasn't found the
     * amulet yet, put it somewhere on the ground
     * AMULETLEVEL(26) 이상이고 아직 부적이 없으면 부적을 바닥에 배치
     */
    if (level >= AMULETLEVEL && !amulet)
    {
	obj = new_item();
	attach(lvl_obj, obj);
	obj->o_hplus = 0;
	obj->o_dplus = 0;
	strncpy(obj->o_damage,"0x0",sizeof(obj->o_damage));
        strncpy(obj->o_hurldmg,"0x0",sizeof(obj->o_hurldmg));
	obj->o_arm = 11;
	obj->o_type = AMULET;  /* 부적 유형 설정 */
	/*
	 * Put it somewhere
	 * 부적을 레벨의 임의 위치에 배치
	 */
	find_floor((struct room *) NULL, &obj->o_pos, FALSE, FALSE);
	chat(obj->o_pos.y, obj->o_pos.x) = AMULET;  /* 맵에 부적 문자(',') 표시 */
    }
}

/*
 * treas_room:
 *	Add a treasure room
 *	보물 방을 생성하는 함수.
 *	임의의 방에 다수의 아이템을 배치하고, 그 방을 지키는 몬스터들로 채운다.
 *	몬스터들은 다음 레벨의 몬스터 중에서 선택되고 ISMEAN 플래그가 설정된다.
 */
#define MAXTRIES 10	/* max number of tries to put down a monster */
                    /* 몬스터를 배치하기 위한 최대 시도 횟수 */


void
treas_room()
{
    int nm;             /* 배치할 아이템/몬스터 수 */
    THING *tp;          /* 아이템/몬스터 포인터 */
    struct room *rp;    /* 보물 방 포인터 */
    int spots, num_monst;  /* 가능한 위치 수, 몬스터 수 */
    static coord mp;    /* 몬스터/아이템 위치 */

    rp = &rooms[rnd_room()];  /* 랜덤 방 선택 */
    /* 방 내부의 가능한 위치 수 계산 (벽 제외) */
    spots = (rp->r_max.y - 2) * (rp->r_max.x - 2) - MINTREAS;
    if (spots > (MAXTREAS - MINTREAS))
	spots = (MAXTREAS - MINTREAS);
    num_monst = nm = rnd(spots) + MINTREAS;  /* MINTREAS ~ spots+MINTREAS 개 아이템 */
    while (nm--)  /* 아이템 배치 */
    {
	find_floor(rp, &mp, 2 * MAXTRIES, FALSE);
	tp = new_thing();  /* 랜덤 아이템 생성 */
	tp->o_pos = mp;
	attach(lvl_obj, tp);
	chat(mp.y, mp.x) = (char) tp->o_type;
    }

    /*
     * fill up room with monsters from the next level down
     * 방을 다음 레벨 몬스터로 채운다 (보물을 지키는 몬스터)
     */

    if ((nm = rnd(spots) + MINTREAS) < num_monst + 2)
	nm = num_monst + 2;  /* 아이템보다 최소 2마리 더 많은 몬스터 */
    spots = (rp->r_max.y - 2) * (rp->r_max.x - 2);
    if (nm > spots)
	nm = spots;
    level++;  /* 다음 레벨의 몬스터 선택을 위해 레벨 임시 증가 */
    while (nm--)
    {
	spots = 0;
	if (find_floor(rp, &mp, MAXTRIES, TRUE))  /* 몬스터 위치 찾기 */
	{
	    tp = new_item();
	    new_monster(tp, randmonster(FALSE), &mp);  /* 몬스터 생성 (monsters.c 참조) */
	    tp->t_flags |= ISMEAN;	/* no sloughers in THIS room */
                                    /* 이 방의 몬스터는 항상 공격적 */
	    give_pack(tp);  /* 몬스터에게 아이템 부여 */
	}
    }
    level--;  /* 레벨 원복 */
}

#include <curses.h>
#include <string.h>
#include "rogue.h"

#define TREAS_ROOM 20	/* one chance in TREAS_ROOM for a treasure room */
#define MAXTREAS 10	/* maximum number of treasures in a treasure room */
#define MINTREAS 2	/* minimum number of treasures in a treasure room */

void
new_level()
{
    THING *tp;
    PLACE *pp;
    char *sp;
    int i;

    player.t_flags &= ~ISHELD;	/* unhold when you go down just in case */
    if (level > max_level)
	max_level = level;
    /*
     * Clean things off from last level
     */
    for (pp = places; pp < &places[MAXCOLS*MAXLINES]; pp++)
    {
	pp->p_ch = ' ';
	pp->p_flags = F_REAL;
	pp->p_monst = NULL;
    }
    clear();
    /*
     * Free up the monsters on the last level
     */
    for (tp = mlist; tp != NULL; tp = next(tp))
	free_list(tp->t_pack);
    free_list(mlist);
    /*
     * Throw away stuff left on the previous level (if anything)
     */
    free_list(lvl_obj);
    do_rooms();				/* Draw rooms */
    do_passages();			/* Draw passages */
    no_food++;
    put_things();			/* Place objects (if any) */
    /*
     * Place the traps
     */
    if (rnd(10) < level)
    {
	ntraps = rnd(level / 4) + 1;
	if (ntraps > MAXTRAPS)
	    ntraps = MAXTRAPS;
	i = ntraps;
	while (i--)
	{
	    /*
	     * not only wouldn't it be NICE to have traps in mazes
	     * (not that we care about being nice), since the trap
	     * number is stored where the passage number is, we
	     * can't actually do it.
	     */
	    do
	    {
		find_floor((struct room *) NULL, &stairs, FALSE, FALSE);
	    } while (chat(stairs.y, stairs.x) != FLOOR);
	    sp = &flat(stairs.y, stairs.x);
	    *sp &= ~F_REAL;
	    *sp |= rnd(NTRAPS);
	}
    }
    /*
     * Place the staircase down.
     */
    find_floor((struct room *) NULL, &stairs, FALSE, FALSE);
    chat(stairs.y, stairs.x) = STAIRS;
    seenstairs = FALSE;

    for (tp = mlist; tp != NULL; tp = next(tp))
	tp->t_room = roomin(&tp->t_pos);

    find_floor((struct room *) NULL, &hero, FALSE, TRUE);
    enter_room(&hero);
    mvaddch(hero.y, hero.x, PLAYER);
    if (on(player, SEEMONST))
	turn_see(FALSE);
    if (on(player, ISHALU))
	visuals();
}

/*
 * rnd_room:
 *	Pick a room that is really there
 */
int
rnd_room()
{
    int rm;

    do
    {
	rm = rnd(MAXROOMS);
    } while (rooms[rm].r_flags & ISGONE);
    return rm;
}

/*
 * put_things:
 *	Put potions and scrolls on this level
 */

void
put_things()
{
    int i;
    THING *obj;

    /*
     * Once you have found the amulet, the only way to get new stuff is
     * go down into the dungeon.
     */
    if (amulet && level < max_level)
	return;
    /*
     * check for treasure rooms, and if so, put it in.
     */
    if (rnd(TREAS_ROOM) == 0)
	treas_room();
    /*
     * Do MAXOBJ attempts to put things on a level
     */
    for (i = 0; i < MAXOBJ; i++)
	if (rnd(100) < 36)
	{
	    /*
	     * Pick a new object and link it in the list
	     */
	    obj = new_thing();
	    attach(lvl_obj, obj);
	    /*
	     * Put it somewhere
	     */
	    find_floor((struct room *) NULL, &obj->o_pos, FALSE, FALSE);
	    chat(obj->o_pos.y, obj->o_pos.x) = (char) obj->o_type;
	}
    /*
     * If he is really deep in the dungeon and he hasn't found the
     * amulet yet, put it somewhere on the ground
     */
    if (level >= AMULETLEVEL && !amulet)
    {
	obj = new_item();
	attach(lvl_obj, obj);
	obj->o_hplus = 0;
	obj->o_dplus = 0;
	strncpy(obj->o_damage,"0x0",sizeof(obj->o_damage));
        strncpy(obj->o_hurldmg,"0x0",sizeof(obj->o_hurldmg));
	obj->o_arm = 11;
	obj->o_type = AMULET;
	/*
	 * Put it somewhere
	 */
	find_floor((struct room *) NULL, &obj->o_pos, FALSE, FALSE);
	chat(obj->o_pos.y, obj->o_pos.x) = AMULET;
    }
}

/*
 * treas_room:
 *	Add a treasure room
 */
#define MAXTRIES 10	/* max number of tries to put down a monster */


void
treas_room()
{
    int nm;
    THING *tp;
    struct room *rp;
    int spots, num_monst;
    static coord mp;

    rp = &rooms[rnd_room()];
    spots = (rp->r_max.y - 2) * (rp->r_max.x - 2) - MINTREAS;
    if (spots > (MAXTREAS - MINTREAS))
	spots = (MAXTREAS - MINTREAS);
    num_monst = nm = rnd(spots) + MINTREAS;
    while (nm--)
    {
	find_floor(rp, &mp, 2 * MAXTRIES, FALSE);
	tp = new_thing();
	tp->o_pos = mp;
	attach(lvl_obj, tp);
	chat(mp.y, mp.x) = (char) tp->o_type;
    }

    /*
     * fill up room with monsters from the next level down
     */

    if ((nm = rnd(spots) + MINTREAS) < num_monst + 2)
	nm = num_monst + 2;
    spots = (rp->r_max.y - 2) * (rp->r_max.x - 2);
    if (nm > spots)
	nm = spots;
    level++;
    while (nm--)
    {
	spots = 0;
	if (find_floor(rp, &mp, MAXTRIES, TRUE))
	{
	    tp = new_item();
	    new_monster(tp, randmonster(FALSE), &mp);
	    tp->t_flags |= ISMEAN;	/* no sloughers in THIS room */
	    give_pack(tp);
	}
    }
    level--;
}
