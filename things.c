/*
 * Contains functions for dealing with things like potions, scrolls,
 * and other items.
 *
 * @(#)things.c	4.53 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

/*
 * [파일 개요]
 * 이 파일은 게임 내 아이템(물건) 관리를 담당하는 함수들을 포함합니다.
 * 주요 기능:
 *   - inv_name()  : 인벤토리에 표시할 아이템 이름 문자열 생성
 *   - drop()      : 아이템을 바닥에 내려놓기
 *   - dropcheck() : 장착 중인 저주 아이템 등의 내려놓기 제한 확인
 *   - new_thing() : 확률 테이블에 따라 새 랜덤 아이템 생성
 *   - pick_one()  : obj_info 배열에서 확률적으로 아이템 종류 선택
 *   - discovered(): 이번 게임에서 발견한 아이템 목록 표시
 *   - nameit()    : 포션·지팡이·반지 등의 이름 조합 생성
 *   - nullstr()   : 빈 문자열 반환 (nameit()의 콜백으로 사용)
 *   - pr_list(), pr_spec(): 위자드 전용 아이템 목록 출력 (MASTER 빌드)
 */

#include <curses.h>
#include <string.h>
#include <ctype.h>
#include "rogue.h"

/*
 * inv_name:
 *	Return the name of something as it would appear in an
 *	inventory.
 *
 * [한국어] 아이템의 인벤토리 표시 이름을 반환한다.
 *   - obj: 이름을 생성할 아이템 THING 구조체 포인터
 *   - drop: TRUE이면 첫 글자를 소문자로(내려놓을 때 메시지용),
 *            FALSE이면 대문자로 시작
 *   - 아이템 종류에 따라 포션은 색깔, 두루마리는 제목, 반지는 보석 이름,
 *     지팡이는 재질로 표시하되, 식별(oi_know)되었거나 별명(oi_guess)이
 *     있으면 실제 이름/별명을 사용한다.
 *   - 결과 문자열은 전역 버퍼 prbuf에 저장되고, 그 포인터를 반환한다.
 */
char *
inv_name(THING *obj, bool drop)
{
    char *pb;           /* prbuf 내 현재 쓰기 위치 */
    struct obj_info *op;/* 아이템 정보 구조체 포인터 */
    char *sp;           /* 아이템 이름 문자열 포인터 */
    int which;          /* 아이템의 세부 종류 인덱스 */

    pb = prbuf;
    which = obj->o_which;
    switch (obj->o_type)
    {
        case POTION:
	    /* 포션: p_colors[]에서 색깔 이름을 가져와 표시 */
	    nameit(obj, "potion", p_colors[which], &pot_info[which], nullstr);
	when RING:
	    /* 반지: r_stones[]에서 보석 이름을 가져와 표시 */
	    nameit(obj, "ring", r_stones[which], &ring_info[which], ring_num);
	when STICK:
	    /* 지팡이/막대기: ws_type[]에서 타입, ws_made[]에서 재질을 가져와 표시 */
	    nameit(obj, ws_type[which], ws_made[which], &ws_info[which], charge_str);
	when SCROLL:
	    /* 두루마리: 식별되면 실제 이름, 별명이 있으면 별명, 없으면 s_names[] 제목 */
	    if (obj->o_count == 1)
	    {
		strcpy(pb, "A scroll ");
		pb = &prbuf[9];
	    }
	    else
	    {
		sprintf(pb, "%d scrolls ", obj->o_count);
		pb = &prbuf[strlen(prbuf)];
	    }
	    op = &scr_info[which];
	    if (op->oi_know)
		sprintf(pb, "of %s", op->oi_name);
	    else if (op->oi_guess)
		sprintf(pb, "called %s", op->oi_guess);
	    else
		sprintf(pb, "titled '%s'", s_names[which]);
	when FOOD:
	    if (which == 1)
		if (obj->o_count == 1)
		    sprintf(pb, "A%s %s", vowelstr(fruit), fruit);
		else
		    sprintf(pb, "%d %ss", obj->o_count, fruit);
	    else
		if (obj->o_count == 1)
		    strcpy(pb, "Some food");
		else
		    sprintf(pb, "%d rations of food", obj->o_count);
	when WEAPON:
	    /* 무기: 식별(ISKNOW)되면 +hit/+dmg 보너스 표시, o_label이 있으면 별명 추가 */
	    sp = weap_info[which].oi_name;
	    if (obj->o_count > 1)
		sprintf(pb, "%d ", obj->o_count);
	    else
		sprintf(pb, "A%s ", vowelstr(sp));
	    pb = &prbuf[strlen(prbuf)];
	    if (obj->o_flags & ISKNOW)
		sprintf(pb, "%s %s", num(obj->o_hplus,obj->o_dplus,WEAPON), sp);
	    else
		sprintf(pb, "%s", sp);
	    if (obj->o_count > 1)
		strcat(pb, "s");
	    if (obj->o_label != NULL)
	    {
		pb = &prbuf[strlen(prbuf)];
		sprintf(pb, " called %s", obj->o_label);
	    }
	when ARMOR:
	    /* 방어구: 식별되면 보너스와 실제 방어력([protection N]) 표시 */
	    sp = arm_info[which].oi_name;
	    if (obj->o_flags & ISKNOW)
	    {
		sprintf(pb, "%s %s [",
		    num(a_class[which] - obj->o_arm, 0, ARMOR), sp);
		if (!terse)
		    strcat(pb, "protection ");
		pb = &prbuf[strlen(prbuf)];
		sprintf(pb, "%d]", 10 - obj->o_arm);
	    }
	    else
		sprintf(pb, "%s", sp);
	    if (obj->o_label != NULL)
	    {
		pb = &prbuf[strlen(prbuf)];
		sprintf(pb, " called %s", obj->o_label);
	    }
	when AMULET:
	    strcpy(pb, "The Amulet of Yendor");
	when GOLD:
	    sprintf(prbuf, "%d Gold pieces", obj->o_goldval);
#ifdef MASTER
	otherwise:
	    debug("Picked up something funny %s", unctrl(obj->o_type));
	    sprintf(pb, "Something bizarre %s", unctrl(obj->o_type));
#endif
    }
    if (inv_describe)
    {
	/* 현재 장착 중인 아이템에 대한 상태 정보 추가 */
	if (obj == cur_armor)
	    strcat(pb, " (being worn)");
	if (obj == cur_weapon)
	    strcat(pb, " (weapon in hand)");
	if (obj == cur_ring[LEFT])
	    strcat(pb, " (on left hand)");
	else if (obj == cur_ring[RIGHT])
	    strcat(pb, " (on right hand)");
    }
    if (drop && isupper(prbuf[0]))  /* drop 모드이면 첫 글자를 소문자로 */
	prbuf[0] = (char) tolower(prbuf[0]);
    else if (!drop && islower(*prbuf))  /* 인벤토리 표시이면 첫 글자를 대문자로 */
	*prbuf = (char) toupper(*prbuf);
    prbuf[MAXSTR-1] = '\0';
    return prbuf;
}

/*
 * drop:
 *	Put something down
 *
 * [한국어] 플레이어가 아이템을 현재 위치에 내려놓는다.
 *   - 현재 위치가 바닥(FLOOR) 또는 통로(PASSAGE)가 아니면 내려놓을 수 없다.
 *   - dropcheck()로 저주 아이템 등의 제약을 확인한다.
 *   - 내려놓은 아이템은 lvl_obj 목록에 추가되고 지도에 표시된다.
 *   - 부적(AMULET)을 내려놓으면 amulet 플래그를 FALSE로 설정한다.
 */

void
drop()
{
    char ch;
    THING *obj;

    ch = chat(hero.y, hero.x);
    if (ch != FLOOR && ch != PASSAGE)
    {
	after = FALSE;
	msg("there is something there already");
	return;
    }
    if ((obj = get_item("drop", 0)) == NULL)
	return;
    if (!dropcheck(obj))
	return;
    obj = leave_pack(obj, TRUE, (bool)!ISMULT(obj->o_type));
    /*
     * Link it into the level object list
     */
    attach(lvl_obj, obj);
    chat(hero.y, hero.x) = (char) obj->o_type;
    flat(hero.y, hero.x) |= F_DROPPED;
    obj->o_pos = hero;
    if (obj->o_type == AMULET)
	amulet = FALSE;
    msg("dropped %s", inv_name(obj, TRUE));
}

/*
 * dropcheck:
 *	Do special checks for dropping or unweilding|unwearing|unringing
 *
 * [한국어] 아이템을 내려놓거나 장착 해제할 때 특별 조건을 확인한다.
 *   - obj가 NULL이거나 장착 중이 아니면 항상 TRUE(허용)
 *   - 저주(ISCURSED) 아이템은 내려놓을 수 없음
 *   - 무기: cur_weapon을 NULL로
 *   - 방어구: waste_time() 후 cur_armor를 NULL로
 *   - 반지: 해당 손의 cur_ring[]을 NULL로, 근력/투명 감지 반지면 효과도 해제
 */
bool
dropcheck(THING *obj)
{
    if (obj == NULL)
	return TRUE;
    if (obj != cur_armor && obj != cur_weapon
	&& obj != cur_ring[LEFT] && obj != cur_ring[RIGHT])
	    return TRUE;
    if (obj->o_flags & ISCURSED)
    {
	msg("you can't.  It appears to be cursed");
	return FALSE;
    }
    if (obj == cur_weapon)
	cur_weapon = NULL;
    else if (obj == cur_armor)
    {
	waste_time();
	cur_armor = NULL;
    }
    else
    {
	cur_ring[obj == cur_ring[LEFT] ? LEFT : RIGHT] = NULL;
	switch (obj->o_which)
	{
	    case R_ADDSTR:
		chg_str(-obj->o_arm);
		break;
	    case R_SEEINVIS:
		unsee();
		extinguish(unsee);
		break;
	}
    }
    return TRUE;
}

/*
 * new_thing:
 *	Return a new thing
 *
 * [한국어] 확률 테이블에 따라 새로운 랜덤 아이템을 생성하고 반환한다.
 *   - no_food > 3 이면 강제로 식량을 생성 (플레이어가 굶지 않도록)
 *   - 그 외에는 things[] 배열의 확률에 따라 포션/두루마리/식량/무기/방어구/반지/지팡이 중 선택
 *   - 무기와 방어구는 일정 확률로 저주받거나 강화될 수 있음
 *   - 반지 종류에 따라 보너스(o_arm) 설정 및 저주 여부 결정
 */
THING *
new_thing()
{
    THING *cur;  /* 새로 생성할 아이템 THING 포인터 */
    int r;       /* 저주/강화 확률 판정용 난수 */

    cur = new_item();
    cur->o_hplus = 0;
    cur->o_dplus = 0;
    strncpy(cur->o_damage, "0x0", sizeof(cur->o_damage));
    strncpy(cur->o_hurldmg, "0x0", sizeof(cur->o_hurldmg));
    cur->o_arm = 11;
    cur->o_count = 1;
    cur->o_group = 0;
    cur->o_flags = 0;
    /*
     * Decide what kind of object it will be
     * If we haven't had food for a while, let it be food.
     * [한국어] 아이템 종류 결정:
     *   no_food > 3이면 무조건 식량(case 2), 그 외엔 things[] 확률표에서 pick_one
     */
    switch (no_food > 3 ? 2 : pick_one(things, NUMTHINGS))
    {
	case 0:  /* 포션 */
	    cur->o_type = POTION;
	    cur->o_which = pick_one(pot_info, MAXPOTIONS);
	when 1:  /* 두루마리 */
	    cur->o_type = SCROLL;
	    cur->o_which = pick_one(scr_info, MAXSCROLLS);
	when 2:  /* 식량: no_food 카운터 초기화, 10% 확률로 과일, 나머지는 일반 식량 */
	    cur->o_type = FOOD;
	    no_food = 0;
	    if (rnd(10) != 0)
		cur->o_which = 0;
	    else
		cur->o_which = 1;
	when 3:  /* 무기: 10% 저주(명중 감소), 5% 강화(명중 증가) */
	    init_weapon(cur, pick_one(weap_info, MAXWEAPONS));
	    if ((r = rnd(100)) < 10)
	    {
		cur->o_flags |= ISCURSED;
		cur->o_hplus -= rnd(3) + 1;
	    }
	    else if (r < 15)
		cur->o_hplus += rnd(3) + 1;
	when 4:  /* 방어구: 20% 저주(방어력 감소), 8% 강화(방어력 증가) */
	    cur->o_type = ARMOR;
	    cur->o_which = pick_one(arm_info, MAXARMORS);
	    cur->o_arm = a_class[cur->o_which];
	    if ((r = rnd(100)) < 20)
	    {
		cur->o_flags |= ISCURSED;
		cur->o_arm += rnd(3) + 1;
	    }
	    else if (r < 28)
		cur->o_arm -= rnd(3) + 1;
	when 5:  /* 반지: 종류에 따라 보너스값(o_arm) 설정, 일부는 저주 */
	    cur->o_type = RING;
	    cur->o_which = pick_one(ring_info, MAXRINGS);
	    switch (cur->o_which)
	    {
		case R_ADDSTR:
		case R_PROTECT:
		case R_ADDHIT:
		case R_ADDDAM:
		    /* 보너스 반지: 0이면 -1로 설정하고 저주 */
		    if ((cur->o_arm = rnd(3)) == 0)
		    {
			cur->o_arm = -1;
			cur->o_flags |= ISCURSED;
		    }
		when R_AGGR:
		case R_TELEPORT:
		    /* 공격성/순간이동 반지는 항상 저주 */
		    cur->o_flags |= ISCURSED;
	    }
	when 6:  /* 지팡이/막대기: fix_stick()으로 충전 횟수 설정 */
	    cur->o_type = STICK;
	    cur->o_which = pick_one(ws_info, MAXSTICKS);
	    fix_stick(cur);
#ifdef MASTER
	otherwise:
	    debug("Picked a bad kind of object");
	    wait_for(' ');
#endif
    }
    return cur;
}

/*
 * pick_one:
 *	Pick an item out of a list of nitems possible objects
 *
 * [한국어] obj_info 배열에서 확률적으로 아이템 종류 하나를 선택한다.
 *   - info: obj_info 배열 (oi_prob 필드가 누적 확률값)
 *   - nitems: 배열의 원소 수
 *   - 0~99 사이의 난수를 구해 누적 확률을 초과하는 첫 번째 항목을 반환
 *   - 배열 끝까지 못 찾으면 첫 번째 항목으로 폴백 (위자드 모드에서 경고 출력)
 *   - 반환값: 선택된 항목의 인덱스 (0-based)
 */
int
pick_one(struct obj_info *info, int nitems)
{
    struct obj_info *end;    /* 배열의 끝 포인터 */
    struct obj_info *start;  /* 배열의 시작 포인터 (폴백용) */
    int i;                   /* 0~99 범위의 난수 */

    start = info;
    for (end = &info[nitems], i = rnd(100); info < end; info++)
	if (i < info->oi_prob)
	    break;
    if (info == end)
    {
#ifdef MASTER
	if (wizard)
	{
	    msg("bad pick_one: %d from %d items", i, nitems);
	    for (info = start; info < end; info++)
		msg("%s: %d%%", info->oi_name, info->oi_prob);
	}
#endif
	info = start;
    }
    return (int)(info - start);
}

/*
 * discovered:
 *	list what the player has discovered in this game of a certain type
 *
 * [한국어] 이번 게임에서 플레이어가 발견(식별)한 아이템 종류 목록을 출력한다.
 *   - 포션(!), 두루마리(?), 반지(=), 지팡이(/|) 또는 * (전체)를 선택할 수 있다.
 *   - 선택된 종류에 대해 print_disc()를 호출하여 목록을 출력한다.
 */
static int line_cnt = 0;

static bool newpage = FALSE;

static char *lastfmt, *lastarg;


void
discovered()
{
    char ch;
    bool disc_list;

    do {
	disc_list = FALSE;
	if (!terse)
	    addmsg("for ");
	addmsg("what type");
	if (!terse)
	    addmsg(" of object do you want a list");
	msg("? (* for all)");
	ch = readchar();
	switch (ch)
	{
	    case ESCAPE:
		msg("");
		return;
	    case POTION:
	    case SCROLL:
	    case RING:
	    case STICK:
	    case '*':
		disc_list = TRUE;
		break;
	    default:
		if (terse)
		    msg("Not a type");
		else
		    msg("Please type one of %c%c%c%c (ESCAPE to quit)", POTION, SCROLL, RING, STICK);
	}
    } while (!disc_list);
    if (ch == '*')
    {
	print_disc(POTION);
	add_line("", NULL);
	print_disc(SCROLL);
	add_line("", NULL);
	print_disc(RING);
	add_line("", NULL);
	print_disc(STICK);
	end_line();
    }
    else
    {
	print_disc(ch);
	end_line();
    }
}

/*
 * print_disc:
 *	Print what we've discovered of type 'type'
 *
 * [한국어] 특정 아이템 종류(type)에 대해 발견한 항목들을 출력한다.
 *   - oi_know(식별 완료) 또는 oi_guess(별명 부여) 중 하나라도 있으면 목록에 추가
 *   - set_order()로 무작위 순서로 섞어서 표시
 *   - 발견한 항목이 없으면 nothing() 메시지 출력
 */

/* MAX4: 네 값 중 최대값을 구하는 매크로 (order[] 배열 크기 결정에 사용) */
#define MAX4(a,b,c,d)	(a > b ? (a > c ? (a > d ? a : d) : (c > d ? c : d)) : (b > c ? (b > d ? b : d) : (c > d ? c : d)))


void
print_disc(char type)
{
    struct obj_info *info = NULL;
    int i, maxnum = 0, num_found;
    static THING obj;
    static int order[MAX4(MAXSCROLLS, MAXPOTIONS, MAXRINGS, MAXSTICKS)];

    switch (type)
    {
	case SCROLL:
	    maxnum = MAXSCROLLS;
	    info = scr_info;
	    break;
	case POTION:
	    maxnum = MAXPOTIONS;
	    info = pot_info;
	    break;
	case RING:
	    maxnum = MAXRINGS;
	    info = ring_info;
	    break;
	case STICK:
	    maxnum = MAXSTICKS;
	    info = ws_info;
	    break;
    }
    set_order(order, maxnum);
    obj.o_count = 1;
    obj.o_flags = 0;
    num_found = 0;
    for (i = 0; i < maxnum; i++)
	if (info[order[i]].oi_know || info[order[i]].oi_guess)
	{
	    obj.o_type = type;
	    obj.o_which = order[i];
	    add_line("%s", inv_name(&obj, FALSE));
	    num_found++;
	}
    if (num_found == 0)
	add_line(nothing(type), NULL);
}

/*
 * set_order:
 *	Set up order for list
 *
 * [한국어] 0~numthings-1 인덱스 배열을 Fisher-Yates 알고리즘으로 무작위로 섞는다.
 *   discovered/print_disc에서 목록 출력 순서를 무작위화할 때 사용한다.
 */

void
set_order(int *order, int numthings)  /* order: 인덱스 배열, numthings: 배열 크기 */
{
    int i, r, t;

    for (i = 0; i< numthings; i++)
	order[i] = i;

    for (i = numthings; i > 0; i--)
    {
	r = rnd(i);
	t = order[i - 1];
	order[i - 1] = order[r];
	order[r] = t;
    }
}

/*
 * add_line:
 *	Add a line to the list of discoveries
 *
 * [한국어] 발견 목록에 한 줄을 추가한다.
 *   - INV_SLOW 모드: msg()로 한 줄씩 출력하고 다음 줄로 넘어감
 *   - INV_OVER 모드: hw(숨겨진 창)에 누적 후 화면이 가득 차면
 *     별도 팝업 창(tw/sw)을 만들어 표시하고 스페이스를 기다림
 *   - fmt가 NULL이면 페이지 종료를 의미
 *   - 반환값: ESCAPE이면 사용자가 중단을 선택한 것
 */
/* VARARGS1 */
char
add_line(char *fmt, char *arg)  /* fmt: 형식 문자열, arg: 인자 문자열 */
{
    WINDOW *tw, *sw;
    int x, y;
    char *prompt = "--Press space to continue--";
    static int maxlen = -1;

    if (line_cnt == 0)
    {
	    wclear(hw);
	    if (inv_type == INV_SLOW)
		mpos = 0;
    }
    if (inv_type == INV_SLOW)
    {
	if (*fmt != '\0')
	    if (msg(fmt, arg) == ESCAPE)
		return ESCAPE;
	line_cnt++;
    }
    else
    {
	if (maxlen < 0)
	    maxlen = (int) strlen(prompt);
	if (line_cnt >= LINES - 1 || fmt == NULL)
	{
	    if (inv_type == INV_OVER && fmt == NULL && !newpage)
	    {
		msg("");
		refresh();
		tw = newwin(line_cnt + 1, maxlen + 2, 0, COLS - maxlen - 3);
		sw = subwin(tw, line_cnt + 1, maxlen + 1, 0, COLS - maxlen - 2);
                for (y = 0; y <= line_cnt; y++) 
                { 
                    wmove(sw, y, 0); 
                    for (x = 0; x <= maxlen; x++) 
                        waddch(sw, mvwinch(hw, y, x)); 
                } 
		wmove(tw, line_cnt, 1);
		waddstr(tw, prompt);
		/*
		 * if there are lines below, use 'em
		 */
		if (LINES > NUMLINES)
		{
		    if (NUMLINES + line_cnt > LINES)
			mvwin(tw, LINES - (line_cnt + 1), COLS - maxlen - 3);
		    else
			mvwin(tw, NUMLINES, 0);
		}
		touchwin(tw);
		wrefresh(tw);
		wait_for(' ');
                if (md_hasclreol())
		{
		    werase(tw);
		    leaveok(tw, TRUE);
		    wrefresh(tw);
		}
		delwin(tw);
		touchwin(stdscr);
	    }
	    else
	    {
		wmove(hw, LINES - 1, 0);
		waddstr(hw, prompt);
		wrefresh(hw);
		wait_for(' ');
		clearok(curscr, TRUE);
		wclear(hw);
		touchwin(stdscr);
	    }
	    newpage = TRUE;
	    line_cnt = 0;
	    maxlen = (int) strlen(prompt);
	}
	if (fmt != NULL && !(line_cnt == 0 && *fmt == '\0'))
	{
	    mvwprintw(hw, line_cnt++, 0, fmt, arg);
	    getyx(hw, y, x);
	    if (maxlen < x)
		maxlen = x;
	    lastfmt = fmt;
	    lastarg = arg;
	}
    }
    return ~ESCAPE;
}

/*
 * end_line:
 *	End the list of lines
 *
 * [한국어] 발견 목록 출력을 마무리한다.
 *   - INV_SLOW 모드가 아닐 때:
 *     줄이 1개이고 새 페이지가 아니면 msg()로 바로 출력
 *     그 외에는 add_line(NULL)을 호출해 페이지를 닫음
 *   - line_cnt와 newpage를 초기화한다.
 */

void
end_line()
{
    if (inv_type != INV_SLOW)
    {
	if (line_cnt == 1 && !newpage)
	{
	    mpos = 0;
	    msg(lastfmt, lastarg);
	}
	else
	    add_line((char *) NULL, NULL);
    }
    line_cnt = 0;
    newpage = FALSE;
}

/*
 * nothing:
 *	Set up prbuf so that message for "nothing found" is there
 *
 * [한국어] "발견한 것 없음" 메시지를 prbuf에 조합하여 반환한다.
 *   - terse 모드이면 짧게 "Nothing", 아니면 긴 문장 사용
 *   - type이 '*'가 아니면 아이템 종류명을 뒤에 붙임
 */
char *
nothing(char type)  /* type: 아이템 종류 문자 (POTION, SCROLL, RING, STICK, 또는 '*') */
{
    char *sp, *tystr = NULL;

    if (terse)
	sprintf(prbuf, "Nothing");
    else
	sprintf(prbuf, "Haven't discovered anything");
    if (type != '*')
    {
	sp = &prbuf[strlen(prbuf)];
	switch (type)
	{
	    case POTION: tystr = "potion";
	    when SCROLL: tystr = "scroll";
	    when RING: tystr = "ring";
	    when STICK: tystr = "stick";
	}
	sprintf(sp, " about any %ss", tystr);
    }
    return prbuf;
}

/*
 * nameit:
 *	Give the proper name to a potion, stick, or ring
 *
 * [한국어] 포션·지팡이·반지의 표시 이름을 prbuf에 조합한다.
 *   - obj: 대상 아이템
 *   - type: 아이템 종류 문자열 ("potion", "ring", "wand" 등)
 *   - which: 외관 문자열 (색깔, 재질, 보석 이름 등)
 *   - op: obj_info 구조체 (oi_know, oi_guess, oi_name 포함)
 *   - prfunc: 추가 정보 문자열을 반환하는 함수 포인터 (충전 횟수, 보너스 등)
 *   - 식별(oi_know)되었으면 "of <실제이름>", 별명(oi_guess)이 있으면 "called <별명>",
 *     모두 없으면 외관("a red potion" 등)만 표시
 */

void
nameit(THING *obj, char *type, char *which, struct obj_info *op,
    char *(*prfunc)(THING *))
{
    char *pb;

    if (op->oi_know || op->oi_guess)
    {
	if (obj->o_count == 1)
	    sprintf(prbuf, "A %s ", type);
	else
	    sprintf(prbuf, "%d %ss ", obj->o_count, type);
	pb = &prbuf[strlen(prbuf)];
	if (op->oi_know)
	    sprintf(pb, "of %s%s(%s)", op->oi_name, (*prfunc)(obj), which);
	else if (op->oi_guess)
	    sprintf(pb, "called %s%s(%s)", op->oi_guess, (*prfunc)(obj), which);
    }
    else if (obj->o_count == 1)
	sprintf(prbuf, "A%s %s %s", vowelstr(which), which, type);
    else
	sprintf(prbuf, "%d %s %ss", obj->o_count, which, type);
}

/*
 * nullstr:
 *	Return a pointer to a null-length string
 *
 * [한국어] 빈 문자열("")의 포인터를 반환한다.
 *   nameit()의 prfunc 콜백으로 포션처럼 추가 정보가 없는 경우에 사용된다.
 */
char *
nullstr(THING *ignored)
{
    NOOP(ignored);
    return "";
}

# ifdef	MASTER
/*
 * pr_list:
 *	List possible potions, scrolls, etc. for wizard.
 *
 * [한국어] 위자드 전용: 선택한 아이템 종류의 전체 목록을 출력한다.
 *   플레이어에게 아이템 종류를 입력받아 pr_spec()으로 목록을 출력한다.
 */

void
pr_list()
{
    int ch;

    if (!terse)
	addmsg("for ");
    addmsg("what type");
    if (!terse)
	addmsg(" of object do you want a list");
    msg("? ");
    ch = readchar();
    switch (ch)
    {
	case POTION:
	    pr_spec(pot_info, MAXPOTIONS);
	when SCROLL:
	    pr_spec(scr_info, MAXSCROLLS);
	when RING:
	    pr_spec(ring_info, MAXRINGS);
	when STICK:
	    pr_spec(ws_info, MAXSTICKS);
	when ARMOR:
	    pr_spec(arm_info, MAXARMORS);
	when WEAPON:
	    pr_spec(weap_info, MAXWEAPONS);
	otherwise:
	    return;
    }
}

/*
 * pr_spec:
 *	Print specific list of possible items to choose from
 *
 * [한국어] 위자드 전용: 특정 아이템 종류의 세부 목록을 출력한다.
 *   - info: obj_info 배열, nitems: 배열 크기
 *   - 각 항목을 '0'~'9', 'a'~'f' 키에 대응하여 이름과 확률(%)를 표시한다.
 *   - lastprob: 누적 확률을 추적하여 각 항목의 개별 확률을 계산
 */

void
pr_spec(struct obj_info *info, int nitems)
{
    struct obj_info *endp;  /* 배열의 끝 포인터 */
    int i, lastprob;        /* i: 키 문자, lastprob: 이전 누적 확률 */

    endp = &info[nitems];
    lastprob = 0;
    for (i = '0'; info < endp; i++)
    {
	if (i == '9' + 1)
	    i = 'a';
	sprintf(prbuf, "%c: %%s (%d%%%%)", i, info->oi_prob - lastprob);
	lastprob = info->oi_prob;
	add_line(prbuf, info->oi_name);
	info++;
    }
    end_line();
}
# endif	/* MASTER */
