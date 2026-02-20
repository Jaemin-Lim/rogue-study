/*
 * Routines to deal with the pack
 *
 * @(#)pack.c	4.40 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

/*
 * 【파일 개요】
 * 이 파일은 Rogue 게임에서 플레이어의 소지품(pack/inventory) 관리를 담당한다.
 * 주요 기능:
 *   - add_pack()      : 바닥의 아이템을 주워 팩에 추가 (같은 종류 스택 처리 포함)
 *   - pack_room()     : 팩에 여유 공간이 있는지 확인하고 바닥에서 아이템을 분리
 *   - leave_pack()    : 팩에서 아이템을 꺼냄 (개수가 여러 개일 때 하나씩 꺼내기 지원)
 *   - pack_char()     : 팩 슬롯 알파벳('a'~'z') 중 사용하지 않는 것을 반환
 *   - inventory()     : 팩 목록을 화면에 나열 (타입 필터링 가능)
 *   - pick_up()       : 플레이어가 아이템 위에 올라섰을 때 자동 줍기 처리
 *   - move_msg()      : 아이템 위로 이동했을 때 메시지 출력
 *   - picky_inven()   : 팩에서 단일 아이템 정보를 확인
 *   - get_item()      : 특정 목적(먹기/마시기 등)에 맞는 아이템을 팩에서 선택
 *   - money()         : 금화를 purse에 추가하고 바닥 문자를 갱신
 *   - floor_ch()      : 현재 위치에 표시할 바닥 문자를 반환
 *   - floor_at()      : hero 위치의 맵 문자를 반환 (see_floor 고려)
 *   - reset_last()    : 중단된 명령의 마지막 상태를 복원
 *
 * 핵심 전역 변수:
 *   - pack        : 플레이어 소지품 연결 리스트 (THING* 헤드)
 *   - inpack      : 현재 팩에 있는 아이템 수
 *   - pack_used[] : 알파벳 슬롯 사용 여부 배열 ('a'=0 ~ 'z'=25)
 *   - purse       : 보유 금화 수량
 *   - lvl_obj     : 현재 층 바닥에 놓인 아이템 연결 리스트
 *   - proom       : 플레이어가 현재 있는 방 포인터
 *   - hero        : 플레이어 좌표
 *   - last_pick   : 마지막으로 선택한 아이템 (재실행용)
 */

#include <string.h>
#include <curses.h>
#include <ctype.h>
#include "rogue.h"

/*
 * add_pack:
 *	Pick up an object and add it to the pack.  If the argument is
 *	non-null use it as the linked_list pointer instead of gettting
 *	it off the ground.
 *
 * 【역할】 아이템을 주워 플레이어의 팩(pack 연결 리스트)에 추가한다.
 *         같은 종류·같은 하위종·같은 그룹의 아이템이 있으면 스택(수량 누적)한다.
 * 【매개변수】
 *   obj    - 추가할 아이템 포인터. NULL이면 현재 위치(hero)의 바닥 아이템을 가져옴
 *   silent - TRUE이면 "you now have ..." 메시지를 출력하지 않음
 * 【동작 원리】
 *   1. 공포 두루마리(S_SCARE)를 ISFOUND 상태에서 줍는 경우 소각(dust) 처리
 *   2. 팩이 비어 있으면 obj를 헤드로 설정
 *   3. 같은 타입의 아이템 뒤에 정렬하여 삽입 (ISMULT 타입은 수량 증가)
 *   4. 아뮬렛(AMULET) 획득 시 amulet 플래그 설정
 *   5. 해당 아이템을 목표로 삼던 몬스터의 목표를 hero로 변경
 */

void
add_pack(THING *obj, bool silent)
{
    THING *op, *lp;   /* op: 팩 순회용, lp: 삽입 위치 추적용 */
    bool from_floor;  /* 바닥에서 가져왔는지 여부 */

    from_floor = FALSE;
    if (obj == NULL)
    {
	if ((obj = find_obj(hero.y, hero.x)) == NULL)
	    return;
	from_floor = TRUE; /* 바닥에서 아이템을 가져옴 */
    }

    /*
     * Check for and deal with scare monster scrolls
     * 공포 몬스터 두루마리(S_SCARE)를 한 번 바닥에 놓았다가 다시 줍는 경우
     * ISFOUND 플래그가 설정되어 있으면 먼지가 되어 소멸
     */
    if (obj->o_type == SCROLL && obj->o_which == S_SCARE)
	if (obj->o_flags & ISFOUND)
	{
	    detach(lvl_obj, obj);
	    mvaddch(hero.y, hero.x, floor_ch());
	    chat(hero.y, hero.x) = (proom->r_flags & ISGONE) ? PASSAGE : FLOOR;
	    discard(obj);
	    msg("the scroll turns to dust as you pick it up");
	    return;
	}

    if (pack == NULL)
    {
	/* 팩이 비어 있으면 obj를 첫 번째 아이템으로 등록 */
	pack = obj;
	obj->o_packch = pack_char(); /* 팩 슬롯 알파벳 할당 */
	inpack++;
    }
    else
    {
	lp = NULL;
	for (op = pack; op != NULL; op = next(op))
	{
	    if (op->o_type != obj->o_type)
		lp = op; /* 타입이 다른 아이템은 삽입 경계 후보로 기록 */
	    else
	    {
		/* 같은 타입의 아이템 중에서 같은 하위종(o_which)을 탐색 */
		while (op->o_type == obj->o_type && op->o_which != obj->o_which)
		{
		    lp = op;
		    if (next(op) == NULL)
			break;
		    else
			op = next(op);
		}
		if (op->o_type == obj->o_type && op->o_which == obj->o_which)
		{
		    if (ISMULT(op->o_type))
		    {
			/* 복수 가능한 타입(ISMULT: 식량, 두루마리 등): 수량 증가 */
			if (!pack_room(from_floor, obj))
			    return;
			op->o_count++;
dump_it:
			discard(obj); /* 중복 노드 메모리 해제 */
			obj = op;
			lp = NULL;
			goto out;
		    }
		    else if (obj->o_group)
		    {
			/* 그룹(o_group) 아이템: 같은 그룹이면 수량 합산 */
			lp = op;
			while (op->o_type == obj->o_type
			    && op->o_which == obj->o_which
			    && op->o_group != obj->o_group)
			{
			    lp = op;
			    if (next(op) == NULL)
				break;
			    else
				op = next(op);
			}
			if (op->o_type == obj->o_type
			    && op->o_which == obj->o_which
			    && op->o_group == obj->o_group)
			{
				op->o_count += obj->o_count; /* 수량 합산 */
				inpack--;                    /* 개별 슬롯 감소 */
				if (!pack_room(from_floor, obj))
				    return;
				goto dump_it;
			}
		    }
		    else
			lp = op;
		}
out:
		break;
	    }
	}

	if (lp != NULL)
	{
	    /* lp 다음 위치에 obj 삽입 (타입별 정렬 유지) */
	    if (!pack_room(from_floor, obj))
		return;
	    else
	    {
		obj->o_packch = pack_char();
		next(obj) = next(lp);
		prev(obj) = lp;
		if (next(lp) != NULL)
		    prev(next(lp)) = obj;
		next(lp) = obj;
	    }
	}
    }

    obj->o_flags |= ISFOUND; /* 한 번이라도 주워진 아이템으로 표시 */

    /*
     * If this was the object of something's desire, that monster will
     * get mad and run at the hero.
     * 이 아이템을 목표로 삼고 있던 몬스터(탐욕 몬스터 등)가 있으면
     * 이제 플레이어를 직접 추격하도록 목표를 변경
     */
    for (op = mlist; op != NULL; op = next(op))
	if (op->t_dest == &obj->o_pos)
	    op->t_dest = &hero;

    if (obj->o_type == AMULET)
	amulet = TRUE; /* 아뮬렛 획득 → 탈출 목표 달성 플래그 설정 */
    /*
     * Notify the user
     * 조용한 모드(silent)가 아니면 획득 메시지 출력
     */
    if (!silent)
    {
	if (!terse)
	    addmsg("you now have ");
	msg("%s (%c)", inv_name(obj, !terse), obj->o_packch);
    }
}

/*
 * pack_room:
 *	See if there's room in the pack.  If not, print out an
 *	appropriate message
 *
 * 【역할】 팩에 아이템을 추가할 공간이 있는지 확인한다.
 *         공간이 있으면 바닥에서 아이템을 분리하고 TRUE를 반환한다.
 *         공간이 없으면 메시지를 출력하고 FALSE를 반환한다.
 * 【매개변수】
 *   from_floor - TRUE이면 바닥에서 가져온 아이템 (lvl_obj에서 분리 필요)
 *   obj        - 팩에 추가하려는 아이템 포인터
 * 【반환값】 공간 있음(성공)이면 TRUE, 없으면 FALSE
 */
bool
pack_room(bool from_floor, THING *obj)
{
    if (++inpack > MAXPACK)
    {
	/* 팩이 가득 찼을 때 메시지 출력 */
	if (!terse)
	    addmsg("there's ");
	addmsg("no room");
	if (!terse)
	    addmsg(" in your pack");
	endmsg();
	if (from_floor)
	    move_msg(obj); /* 바닥 아이템이면 "moved onto" 메시지 출력 */
	inpack = MAXPACK;  /* 카운터를 최대값으로 복원 */
	return FALSE;
    }

    if (from_floor)
    {
	/* 바닥 아이템을 lvl_obj에서 분리하고 화면의 아이템 문자를 바닥 문자로 교체 */
	detach(lvl_obj, obj);
	mvaddch(hero.y, hero.x, floor_ch());
	chat(hero.y, hero.x) = (proom->r_flags & ISGONE) ? PASSAGE : FLOOR;
    }

    return TRUE;
}

/*
 * leave_pack:
 *	take an item out of the pack
 *
 * 【역할】 팩에서 아이템을 꺼낸다. 수량이 여러 개인 경우 하나만 꺼낼 수 있다.
 * 【매개변수】
 *   obj    - 꺼낼 아이템 포인터
 *   newobj - TRUE이면 새 THING 노드를 할당하여 복사본을 반환 (사용/던지기 등)
 *   all    - TRUE이면 수량에 상관없이 전체를 한 번에 꺼냄
 * 【반환값】 꺼낸 아이템의 THING 포인터
 *           (newobj=TRUE이면 새로 할당된 단일 노드, 아니면 원래 obj)
 * 【동작 원리】
 *   - 수량이 1개이거나 all=TRUE이면 팩에서 완전히 분리(detach)
 *   - 수량이 2개 이상이고 all=FALSE이면 o_count를 1 감소시키고,
 *     newobj=TRUE일 때는 복사 노드(count=1)를 새로 만들어 반환
 */
THING *
leave_pack(THING *obj, bool newobj, bool all)
{
    THING *nobj; /* 반환할 아이템 포인터 (복사본 또는 원본) */

    inpack--;
    nobj = obj;
    if (obj->o_count > 1 && !all)
    {
	/* 수량이 2개 이상이고 전체 꺼내기가 아닌 경우: 하나만 분리 */
	last_pick = obj;
	obj->o_count--;           /* 원본 수량 1 감소 */
	if (obj->o_group)
	    inpack++;             /* 그룹 아이템은 팩 슬롯이 유지되므로 카운터 복원 */
	if (newobj)
	{
	    /* 수량 1인 복사 노드를 새로 할당하여 반환 */
	    nobj = new_item();
	    *nobj = *obj;          /* 전체 필드 복사 */
	    next(nobj) = NULL;
	    prev(nobj) = NULL;
	    nobj->o_count = 1;    /* 복사본은 수량 1 */
	}
    }
    else
    {
	/* 수량 1이거나 all=TRUE: 팩에서 완전히 제거 */
	last_pick = NULL;
	pack_used[obj->o_packch - 'a'] = FALSE; /* 팩 슬롯 반납 */
	detach(pack, obj);                      /* 팩 연결 리스트에서 분리 */
    }
    return nobj;
}

/*
 * pack_char:
 *	Return the next unused pack character.
 *
 * 【역할】 pack_used[] 배열에서 사용되지 않는 첫 번째 슬롯 알파벳을 반환한다.
 *         반환과 동시에 해당 슬롯을 사용 중(TRUE)으로 표시한다.
 * 【반환값】 'a'~'z' 중 사용되지 않은 가장 앞의 문자
 */
char
pack_char()
{
    bool *bp; /* pack_used 배열 순회 포인터 */

    for (bp = pack_used; *bp; bp++) /* 이미 사용 중인 슬롯은 건너뜀 */
	continue;
    *bp = TRUE; /* 해당 슬롯을 사용 중으로 표시 */
    return (char)((int)(bp - pack_used) + 'a'); /* 배열 인덱스 → 알파벳 변환 */
}

/*
 * inventory:
 *	List what is in the pack.  Return TRUE if there is something of
 *	the given type.
 *
 * 【역할】 팩(또는 지정 연결 리스트)에 있는 아이템 목록을 화면에 출력한다.
 *         type 필터에 맞는 아이템만 표시한다.
 * 【매개변수】
 *   list - 목록을 출력할 THING 연결 리스트 (보통 pack)
 *   type - 필터링할 아이템 타입 (0이면 전체, CALLABLE이면 이름 붙이기 가능한 것만 등)
 * 【반환값】 표시할 아이템이 하나 이상 있으면 TRUE, 없으면 FALSE
 * 【동작 원리】
 *   add_line()으로 항목을 누적 출력하고, end_line()으로 마무리한다.
 *   ESC 입력 시 즉시 반환 (목록 표시 중단).
 */
bool
inventory(THING *list, int type)
{
    static char inv_temp[MAXSTR]; /* 출력 형식 문자열 버퍼 */

    n_objs = 0; /* 출력된 아이템 수 초기화 */
    for (; list != NULL; list = next(list))
    {
	/* 타입 필터: 조건에 맞지 않는 아이템은 건너뜀 */
	if (type && type != list->o_type && !(type == CALLABLE &&
	    list->o_type != FOOD && list->o_type != AMULET) &&
	    !(type == R_OR_S && (list->o_type == RING || list->o_type == STICK)))
		continue;
	n_objs++;
#ifdef MASTER
	if (!list->o_packch)
	    strcpy(inv_temp, "%s");
	else
#endif
	    sprintf(inv_temp, "%c) %%s", list->o_packch); /* 예: "a) %s" */
	msg_esc = TRUE;
	if (add_line(inv_temp, inv_name(list, FALSE)) == ESCAPE)
	{
	    /* ESC 입력 시 목록 표시 중단 */
	    msg_esc = FALSE;
	    msg("");
	    return TRUE;
	}
	msg_esc = FALSE;
    }
    if (n_objs == 0)
    {
	/* 표시할 아이템이 없을 때 메시지 출력 */
	if (terse)
	    msg(type == 0 ? "empty handed" :
			    "nothing appropriate");
	else
	    msg(type == 0 ? "you are empty handed" :
			    "you don't have anything appropriate");
	return FALSE;
    }
    end_line(); /* 목록 출력 완료 */
    return TRUE;
}

/*
 * pick_up:
 *	Add something to characters pack.
 *
 * 【역할】 플레이어가 아이템이 있는 칸으로 이동했을 때 호출된다.
 *         아이템 종류에 따라 자동으로 주워 팩에 넣거나 금화를 지갑에 추가한다.
 * 【매개변수】
 *   ch - 해당 위치의 맵 문자 (아이템 종류 식별)
 * 【동작 원리】
 *   - ISLEVIT(부유) 상태에서는 아이템을 줍지 않음
 *   - move_on 모드(이동 중)이면 "moved onto" 메시지만 출력
 *   - GOLD이면 돈을 지갑에 추가하고 아이템을 삭제
 *   - 그 외 아이템은 add_pack()을 호출하여 팩에 추가
 */

void
pick_up(char ch)
{
    THING *obj; /* 현재 위치의 아이템 포인터 */

    if (on(player, ISLEVIT))
	return; /* 부유 중에는 아이템을 줍지 않음 */

    obj = find_obj(hero.y, hero.x);
    if (move_on)
	move_msg(obj); /* 단순 이동 중: 아이템 위에 올라섰다는 메시지만 출력 */
    else
	switch (ch)
	{
	    case GOLD:
		if (obj == NULL)
		    return;
		money(obj->o_goldval); /* 금화를 purse에 추가 */
		detach(lvl_obj, obj);  /* lvl_obj 목록에서 제거 */
		discard(obj);          /* 메모리 해제 */
		proom->r_goldval = 0;  /* 방의 금화 값 초기화 */
		break;
	    default:
#ifdef MASTER
		debug("Where did you pick a '%s' up???", unctrl(ch));
#endif
	    case ARMOR:
	    case POTION:
	    case FOOD:
	    case WEAPON:
	    case SCROLL:	
	    case AMULET:
	    case RING:
	    case STICK:
		add_pack((THING *) NULL, FALSE); /* 팩에 아이템 추가 */
		break;
	}
}

/*
 * move_msg:
 *	Print out the message if you are just moving onto an object
 *
 * 【역할】 달리기(running) 중 아이템 위로 이동했을 때 메시지를 출력한다.
 *         "you moved onto <아이템 이름>" 형식으로 알린다.
 * 【매개변수】
 *   obj - 이동한 위치의 아이템 포인터
 */

void
move_msg(THING *obj)
{
    if (!terse)
	addmsg("you ");
    msg("moved onto %s", inv_name(obj, TRUE));
}

/*
 * picky_inven:
 *	Allow player to inventory a single item
 *
 * 【역할】 플레이어가 팩의 특정 아이템 하나를 조회할 수 있게 한다.
 *         팩이 비어 있거나 아이템이 하나뿐이면 바로 표시하고,
 *         여러 개가 있으면 알파벳 슬롯 입력을 받아 해당 아이템 정보를 출력한다.
 */

void
picky_inven()
{
    THING *obj; /* 팩 순회용 포인터 */
    char mch;   /* 플레이어가 입력한 팩 슬롯 문자 */

    if (pack == NULL)
	msg("you aren't carrying anything");
    else if (next(pack) == NULL)
	msg("a) %s", inv_name(pack, FALSE)); /* 아이템이 하나뿐이면 바로 표시 */
    else
    {
	msg(terse ? "item: " : "which item do you wish to inventory: ");
	mpos = 0;
	if ((mch = readchar()) == ESCAPE)
	{
	    msg("");
	    return;
	}
	/* 입력한 문자와 일치하는 팩 슬롯 아이템 검색 */
	for (obj = pack; obj != NULL; obj = next(obj))
	    if (mch == obj->o_packch)
	    {
		msg("%c) %s", mch, inv_name(obj, FALSE));
		return;
	    }
	msg("'%s' not in pack", unctrl(mch)); /* 해당 슬롯에 아이템 없음 */
    }
}

/*
 * get_item:
 *	Pick something out of a pack for a purpose
 *
 * 【역할】 특정 목적(먹기/마시기/읽기/던지기 등)에 사용할 아이템을 팩에서 선택한다.
 *         '*' 입력 시 팩 전체 목록을 표시하며, ESC로 취소할 수 있다.
 * 【매개변수】
 *   purpose - 사용 목적 문자열 (예: "eat", "drink", "read")
 *   type    - 선택 가능한 아이템 타입 필터 (0이면 전체)
 * 【반환값】 선택된 아이템의 THING 포인터, 취소 또는 팩이 비었으면 NULL
 * 【동작 원리】
 *   - again(재실행) 모드이면 last_pick을 바로 반환
 *   - '*' 입력으로 전체 목록 표시 후 계속 선택 가능
 *   - 유효하지 않은 슬롯 입력이면 오류 메시지 후 재입력 요청
 */
THING *
get_item(char *purpose, int type)
{
    THING *obj; /* 팩 순회 및 반환용 포인터 */
    char ch;    /* 플레이어 입력 문자 */

    if (pack == NULL)
	msg("you aren't carrying anything");
    else if (again)
	/* 재실행 모드: 마지막에 선택한 아이템 재사용 */
	if (last_pick)
	    return last_pick;
	else
	    msg("you ran out");
    else
    {
	for (;;)
	{
	    if (!terse)
		addmsg("which object do you want to ");
	    addmsg(purpose);
	    if (terse)
		addmsg(" what");
	    msg("? (* for list): ");
	    ch = readchar();
	    mpos = 0;
	    /*
	     * Give the poor player a chance to abort the command
	     * ESC 입력으로 명령 취소
	     */
	    if (ch == ESCAPE)
	    {
		reset_last();
		after = FALSE;
		msg("");
		return NULL;
	    }
	    n_objs = 1;		/* normal case: person types one char */
	    if (ch == '*')
	    {
		/* '*' 입력: 팩 전체 목록 표시 후 다시 선택 */
		mpos = 0;
		if (inventory(pack, type) == 0)
		{
		    after = FALSE;
		    return NULL;
		}
		continue;
	    }
	    /* 입력한 알파벳 슬롯과 일치하는 아이템 검색 */
	    for (obj = pack; obj != NULL; obj = next(obj))
		if (obj->o_packch == ch)
		    break;
	    if (obj == NULL)
	    {
		msg("'%s' is not a valid item",unctrl(ch)); /* 잘못된 슬롯 */
		continue;
	    }
	    else 
		return obj;
	}
    }
    return NULL;
}

/*
 * money:
 *	Add or subtract gold from the pack
 *
 * 【역할】 금화(gold)를 purse(지갑)에 추가하고, 해당 위치의 맵 문자를 바닥으로 교체한다.
 *         value가 양수이면 "you found N gold pieces" 메시지를 출력한다.
 * 【매개변수】
 *   value - 추가할 금화 양 (양수: 획득, 음수: 소비)
 */

void
money(int value)
{
    purse += value; /* purse(지갑)에 금화 추가/차감 */
    /* 금화가 있던 자리를 바닥 문자로 교체 (통로이면 PASSAGE, 방이면 FLOOR) */
    mvaddch(hero.y, hero.x, floor_ch());
    chat(hero.y, hero.x) = (proom->r_flags & ISGONE) ? PASSAGE : FLOOR;
    if (value > 0)
    {
	if (!terse)
	    addmsg("you found ");
	msg("%d gold pieces", value);
    }
}

/*
 * floor_ch:
 *	Return the appropriate floor character for her room
 *
 * 【역할】 현재 플레이어가 있는 방에 표시할 바닥 문자를 반환한다.
 *         - 사라진 방(ISGONE, 즉 통로 연결점)이면 PASSAGE 문자
 *         - 어두운 방이고 바닥을 표시하지 않을 때는 ' '(공백)
 *         - 그 외이면 FLOOR 문자
 */
char
floor_ch()
{
    if (proom->r_flags & ISGONE)
	return PASSAGE;             /* 통로: PASSAGE 문자 반환 */
    return (show_floor() ? FLOOR : ' '); /* 밝으면 FLOOR, 어두우면 공백 */
}

/*
 * floor_at:
 *	Return the character at hero's position, taking see_floor
 *	into account
 *
 * 【역할】 플레이어(hero) 현재 위치의 맵 문자를 반환한다.
 *         바닥(FLOOR) 문자이면 see_floor 상태를 반영하여 적절한 문자로 교환한다.
 */
char
floor_at()
{
    char ch;

    ch = chat(hero.y, hero.x); /* 맵에 저장된 실제 문자 */
    if (ch == FLOOR)
	ch = floor_ch(); /* 바닥이면 어둠/밝음에 따라 적절한 문자로 교환 */
    return ch;
}

/*
 * reset_last:
 *	Reset the last command when the current one is aborted
 *
 * 【역할】 현재 명령이 취소(ESC 등)되었을 때 마지막 명령 정보를 복원한다.
 *         last_comm, last_dir, last_pick을 l_last_* 백업값으로 되돌린다.
 */

void
reset_last()
{
    last_comm = l_last_comm;
    last_dir = l_last_dir;
    last_pick = l_last_pick;
}
