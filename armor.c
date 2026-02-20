/*
 * This file contains misc functions for dealing with armor
 * 이 파일은 갑옷(armor) 처리를 위한 다양한 함수들을 포함합니다.
 * @(#)armor.c	4.14 (Berkeley) 02/05/99
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
 * wear:
 *	The player wants to wear something, so let him/her put it on.
 *	플레이어가 갑옷을 입고 싶을 때 호출되는 함수.
 *	배낭에서 갑옷을 선택하여 장착한다.
 *	이미 갑옷을 입고 있으면 먼저 벗어야 한다는 메시지를 출력한다.
 */
void
wear()
{
    register THING *obj;  /* 선택된 아이템 포인터 */
    register char *sp;    /* 아이템 이름 문자열 포인터 */

    /* 배낭에서 갑옷 유형의 아이템을 선택 (없으면 종료) */
    if ((obj = get_item("wear", ARMOR)) == NULL)
	return;
    /* 이미 갑옷을 입고 있다면 장착 불가 */
    if (cur_armor != NULL)
    {
	addmsg("you are already wearing some");
	if (!terse)
	    addmsg(".  You'll have to take it off first");
	endmsg();
	after = FALSE;
	return;
    }
    /* 선택한 아이템이 갑옷 유형이 아니면 장착 불가 */
    if (obj->o_type != ARMOR)
    {
	msg("you can't wear that");
	return;
    }
    waste_time();  /* 갑옷을 입는 데 한 턴 소요 */
    obj->o_flags |= ISKNOW;  /* 갑옷 정보를 알게 됨으로 표시 */
    sp = inv_name(obj, TRUE);  /* 갑옷 이름 획득 */
    cur_armor = obj;  /* 현재 장착 갑옷으로 설정 */
    if (!terse)
	addmsg("you are now ");
    msg("wearing %s", sp);
}

/*
 * take_off:
 *	Get the armor off of the players back
 *	플레이어가 갑옷을 벗을 때 호출되는 함수.
 *	저주받은(ISCURSED) 갑옷은 벗을 수 없다 (dropcheck에서 검사).
 */
void
take_off()
{
    register THING *obj;  /* 현재 착용 중인 갑옷 포인터 */

    /* 갑옷을 입고 있지 않으면 메시지 출력 후 종료 */
    if ((obj = cur_armor) == NULL)
    {
	after = FALSE;
	if (terse)
		msg("not wearing armor");
	else
		msg("you aren't wearing any armor");
	return;
    }
    /* 저주받은 갑옷인지 확인 (dropcheck 함수 참조) */
    if (!dropcheck(cur_armor))
	return;
    cur_armor = NULL;  /* 현재 착용 갑옷 해제 */
    if (terse)
	addmsg("was");
    else
	addmsg("you used to be");
    msg(" wearing %c) %s", obj->o_packch, inv_name(obj, TRUE));
}

/*
 * waste_time:
 *	Do nothing but let other things happen
 *	아무것도 하지 않고 한 턴을 소비하는 함수.
 *	갑옷을 입는 등 시간이 소요되는 행동에 사용된다.
 *	데몬(주기적 이벤트)과 퓨즈(일정 시간 후 발동 이벤트)를 실행한다.
 *	BEFORE: 플레이어 행동 전 실행되어야 할 이벤트
 *	AFTER: 플레이어 행동 후 실행되어야 할 이벤트
 */
void
waste_time()
{
    do_daemons(BEFORE);  /* 행동 전 데몬 실행 (daemon.c 참조) */
    do_fuses(BEFORE);    /* 행동 전 퓨즈 실행 (daemon.c 참조) */
    do_daemons(AFTER);   /* 행동 후 데몬 실행 */
    do_fuses(AFTER);     /* 행동 후 퓨즈 실행 */
}
