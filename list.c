/*
 * Functions for dealing with linked lists of goodies
 * 아이템과 몬스터를 관리하는 연결 리스트(linked list) 함수들을 담은 파일.
 * THING 구조체 (rogue.h 참조)는 양방향 연결 리스트로 구현되어 있다.
 * l_next, l_prev 필드로 앞뒤 연결을 관리한다.
 *
 * @(#)list.c	4.12 (Berkeley) 02/05/99
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

#ifdef MASTER
int total = 0;			/* total dynamic memory bytes */
                        /* 총 동적 메모리 사용량 추적 (MASTER 모드에서만) */
#endif

/*
 * detach:
 *	takes an item out of whatever linked list it might be in
 *	연결 리스트에서 특정 아이템을 제거하는 함수.
 *	l_prev/l_next 포인터를 조정하여 리스트의 연결을 유지한다.
 *	rogue.h의 detach(a,b) 매크로가 이 함수를 호출한다.
 */

void
_detach(THING **list, THING *item)
{
    /* 제거할 아이템이 리스트의 첫 번째이면 리스트 헤드를 다음 아이템으로 갱신 */
    if (*list == item)
	*list = next(item);
    /* 이전 아이템의 next를 다음 아이템으로 연결 */
    if (prev(item) != NULL)
	item->l_prev->l_next = next(item);
    /* 다음 아이템의 prev를 이전 아이템으로 연결 */
    if (next(item) != NULL)
	item->l_next->l_prev = prev(item);
    item->l_next = NULL;  /* 제거된 아이템의 포인터 초기화 */
    item->l_prev = NULL;
}

/*
 * _attach:
 *	add an item to the head of a list
 *	연결 리스트의 맨 앞에 새 아이템을 추가하는 함수.
 *	rogue.h의 attach(a,b) 매크로가 이 함수를 호출한다.
 */

void
_attach(THING **list, THING *item)
{
    if (*list != NULL)  /* 리스트가 비어있지 않으면 */
    {
	item->l_next = *list;    /* 새 아이템의 next를 현재 첫 번째로 */
	(*list)->l_prev = item;  /* 현재 첫 번째의 prev를 새 아이템으로 */
	item->l_prev = NULL;     /* 새 아이템은 리스트의 첫 번째이므로 prev는 NULL */
    }
    else  /* 리스트가 비어있으면 */
    {
	item->l_next = NULL;
	item->l_prev = NULL;
    }
    *list = item;  /* 리스트 헤드를 새 아이템으로 갱신 */
}

/*
 * _free_list:
 *	Throw the whole blamed thing away
 *	연결 리스트 전체를 메모리에서 해제하는 함수.
 *	rogue.h의 free_list(a) 매크로가 이 함수를 호출한다.
 */

void
_free_list(THING **ptr)
{
    THING *item;

    while (*ptr != NULL)
    {
	item = *ptr;
	*ptr = next(item);  /* 헤드를 다음 아이템으로 이동 */
	discard(item);      /* 현재 아이템 메모리 해제 */
    }
}

/*
 * discard:
 *	Free up an item
 *	단일 아이템의 메모리를 해제하는 함수.
 *	MASTER 모드에서는 total 카운터를 감소시켜 메모리 사용량을 추적한다.
 */

void
discard(THING *item)
{
#ifdef MASTER
    total--;  /* 메모리 사용 카운터 감소 */
#endif
    free((char *) item);  /* 동적 메모리 해제 */
}

/*
 * new_item
 *	Get a new item with a specified size
 *	새로운 THING 구조체를 동적으로 할당하는 함수.
 *	calloc으로 0으로 초기화된 메모리를 할당한다.
 *	MASTER 모드에서 메모리 부족 시 오류 메시지 출력.
 */
THING *
new_item()
{
    THING *item;

#ifdef MASTER
    if ((item = calloc(1, sizeof *item)) == NULL)
	msg("ran out of memory after %d items", total);
    else
	total++;
#else
    item = calloc(1, sizeof *item);  /* 0으로 초기화된 THING 구조체 할당 */
#endif
    item->l_next = NULL;  /* 연결 리스트 포인터 초기화 */
    item->l_prev = NULL;
    return item;
}

#include <stdlib.h>
#include <curses.h>
#include "rogue.h"

#ifdef MASTER
int total = 0;			/* total dynamic memory bytes */
#endif

/*
 * detach:
 *	takes an item out of whatever linked list it might be in
 */

void
_detach(THING **list, THING *item)
{
    if (*list == item)
	*list = next(item);
    if (prev(item) != NULL)
	item->l_prev->l_next = next(item);
    if (next(item) != NULL)
	item->l_next->l_prev = prev(item);
    item->l_next = NULL;
    item->l_prev = NULL;
}

/*
 * _attach:
 *	add an item to the head of a list
 */

void
_attach(THING **list, THING *item)
{
    if (*list != NULL)
    {
	item->l_next = *list;
	(*list)->l_prev = item;
	item->l_prev = NULL;
    }
    else
    {
	item->l_next = NULL;
	item->l_prev = NULL;
    }
    *list = item;
}

/*
 * _free_list:
 *	Throw the whole blamed thing away
 */

void
_free_list(THING **ptr)
{
    THING *item;

    while (*ptr != NULL)
    {
	item = *ptr;
	*ptr = next(item);
	discard(item);
    }
}

/*
 * discard:
 *	Free up an item
 */

void
discard(THING *item)
{
#ifdef MASTER
    total--;
#endif
    free((char *) item);
}

/*
 * new_item
 *	Get a new item with a specified size
 */
THING *
new_item()
{
    THING *item;

#ifdef MASTER
    if ((item = calloc(1, sizeof *item)) == NULL)
	msg("ran out of memory after %d items", total);
    else
	total++;
#else
    item = calloc(1, sizeof *item);
#endif
    item->l_next = NULL;
    item->l_prev = NULL;
    return item;
}
