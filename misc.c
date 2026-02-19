/*
 * All sorts of miscellaneous routines
 *
 * @(#)misc.c	4.66 (Berkeley) 08/06/83
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

/*
 * 【파일 개요】
 * 이 파일은 Rogue 게임의 각종 잡다한(miscellaneous) 루틴들을 담고 있습니다.
 * 주요 기능:
 *   - look()         : 플레이어 주변 한 칸을 훑어보고 화면을 갱신, 몬스터를 깨움
 *   - trip_ch()      : 환각(ISHALU) 상태에 따라 보여줄 화면 문자를 결정
 *   - erase_lamp()   : 어두운 방에서 이전 위치의 램프 조명 영역을 지움
 *   - show_floor()   : 현재 방 바닥을 표시해야 하는지 판단
 *   - find_obj()     : 특정 좌표의 미소유 아이템을 lvl_obj 목록에서 찾음
 *   - eat()          : 플레이어가 음식을 먹는 동작 처리
 *   - check_level()  : 경험치 기반으로 플레이어 레벨 업 여부 확인
 *   - chg_str()      : 플레이어 힘(strength) 수치를 변경하고 최대값 추적
 *   - add_str()      : 힘 수치 증감 (상하한 3~31 범위 내로 제한)
 *   - add_haste()    : 플레이어에게 속도 증가(ISHASTE) 효과를 적용
 *   - aggravate()    : 현재 층의 모든 몬스터를 플레이어에게 적대적으로 만듦
 *   - vowelstr()     : 영어 관사 "a/an" 선택을 위한 모음 문자열 반환
 *   - is_current()   : 아이템이 현재 장착 중인지 확인
 *   - get_dir()      : 방향 입력을 받아 delta 좌표를 설정
 *   - sign()         : 정수의 부호를 반환 (-1, 0, 1)
 *   - spread()       : 주어진 값의 ±10% 범위 내 난수를 반환
 *   - call_it()      : 아이템에 별명을 붙이는 처리
 *   - rnd_thing()    : 현재 레벨에 맞는 랜덤 아이템 문자를 반환
 *   - choose_str()   : 환각 여부에 따라 두 문자열 중 하나를 선택
 *
 * 핵심 전역 변수:
 *   - proom      : 플레이어가 현재 있는 방 포인터
 *   - hero       : 플레이어의 맵 좌표
 *   - pstats     : 플레이어 스탯 구조체 (s_hpt=HP, s_str=힘, s_lvl=레벨, s_exp=경험치)
 *   - mlist      : 현재 층 몬스터 연결 리스트
 *   - lvl_obj    : 현재 층에 놓인 아이템 연결 리스트
 */

#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <ctype.h>
#include "rogue.h"

/*
 * look:
 *	A quick glance all around the player
 *
 * 【역할】 플레이어 주변 3×3 범위(한 칸 반경)를 훑어보고 화면을 갱신한다.
 *         몬스터가 있으면 깨우고(wake_monster), 달리기 중에 장애물을 감지하여
 *         자동 이동(running)을 멈추는 판단도 수행한다.
 * 【매개변수】
 *   wakeup - TRUE이면 인접 몬스터를 깨움, FALSE이면 보기만 함
 * 【주요 지역 변수】
 *   passcount - 현재 이동 방향의 통로(PASSAGE) 수 (분기점 감지용)
 *   sumhero / diffhero - 대각선 이동 방향 판정용 좌표 합산값
 *   pch / pfl - 플레이어 현재 위치의 맵 문자 / 플래그
 */
#undef DEBUG


void
look(bool wakeup)
{
    int x, y;
    int ch;
    THING *tp;
    PLACE *pp;
    struct room *rp;
    int ey, ex;
    int passcount;
    char pfl, *fp, pch;
    int sy, sx, sumhero = 0, diffhero = 0;
# ifdef DEBUG
    static bool done = FALSE;

    if (done)
	return;
    done = TRUE;
# endif /* DEBUG */
    passcount = 0;
    rp = proom;       /* 플레이어 현재 방 */
    if (!ce(oldpos, hero))
    {
	erase_lamp(&oldpos, oldrp); /* 이전 위치의 램프 영역을 지움 */
	oldpos = hero;
	oldrp = rp;
    }
    /* 주변 탐색 범위: 플레이어 위치 ±1 칸 */
    ey = hero.y + 1;
    ex = hero.x + 1;
    sx = hero.x - 1;
    sy = hero.y - 1;
    if (door_stop && !firstmove && running)
    {
	/* 달리기 중 방향 판단용 좌표 합산값 계산 (대각선 방향 감지) */
	sumhero = hero.y + hero.x;
	diffhero = hero.y - hero.x;
    }
    pp = INDEX(hero.y, hero.x);
    pch = pp->p_ch;   /* 플레이어 현재 위치 맵 문자 */
    pfl = pp->p_flags; /* 플레이어 현재 위치 맵 플래그 */

    for (y = sy; y <= ey; y++)
	if (y > 0 && y < NUMLINES - 1) for (x = sx; x <= ex; x++)
	{
	    if (x < 0 || x >= NUMCOLS)
		continue;
	    if (!on(player, ISBLIND))
	    {
		if (y == hero.y && x == hero.x)
		    continue; /* 플레이어 자신의 위치는 건너뜀 */
	    }

	    pp = INDEX(y, x);
	    ch = pp->p_ch;
	    if (ch == ' ')		/* nothing need be done with a ' ' */
		    continue;       /* 빈 공간은 처리 불필요 */
	    fp = &pp->p_flags;
	    /* 현재 위치와 인접 칸이 서로 다른 구역(방/통로)이면 보이지 않음 */
	    if (pch != DOOR && ch != DOOR)
		if ((pfl & F_PASS) != (*fp & F_PASS))
		    continue;
	    /* 통로와 통로, 또는 통로와 문 사이에서 직선이 아닌 코너는 건너뜀 */
	    if (((*fp & F_PASS) || ch == DOOR) && 
		 ((pfl & F_PASS) || pch == DOOR))
	    {
		if (hero.x != x && hero.y != y &&
		    !step_ok(chat(y, hero.x)) && !step_ok(chat(hero.y, x)))
			continue;
	    }

	    if ((tp = pp->p_monst) == NULL)
		ch = trip_ch(y, x, ch); /* 몬스터 없으면 환각 처리된 문자 반환 */
	    else
		if (on(player, SEEMONST) && on(*tp, ISINVIS))
		{
		    /* 투명 몬스터를 볼 수 있는 상태이면 달리기 중지 후 건너뜀 */
		    if (door_stop && !firstmove)
			running = FALSE;
		    continue;
		}
		else
		{
		    if (wakeup)
			wake_monster(y, x); /* 몬스터를 깨움 */
		    if (see_monst(tp))
		    {
			if (on(player, ISHALU))
			    ch = rnd(26) + 'A'; /* 환각 상태: 무작위 알파벳 표시 */
			else
			    ch = tp->t_disguise; /* 정상: 몬스터(또는 변장) 문자 */
		    }
		}
	    if (on(player, ISBLIND) && (y != hero.y || x != hero.x))
		continue; /* 눈 먼 상태에서는 자기 위치만 갱신 */

	    move(y, x);

	    /* 어두운 방에서 바닥 타일은 공백으로 표시 */
	    if ((proom->r_flags & ISDARK) && !see_floor && ch == FLOOR)
		ch = ' ';

	    if (tp != NULL || ch != CCHAR( inch() ))
		addch(ch); /* 화면 문자가 바뀌었을 때만 다시 그림 */

	    if (door_stop && !firstmove && running)
	    {
		/* 달리기 중 이동 방향 바깥쪽 칸은 장애물 검사에서 제외 */
		switch (runch)
		{
		    case 'h':
			if (x == ex)
			    continue;
		    when 'j':
			if (y == sy)
			    continue;
		    when 'k':
			if (y == ey)
			    continue;
		    when 'l':
			if (x == sx)
			    continue;
		    when 'y':
			if ((y + x) - sumhero >= 1)
			    continue;
		    when 'u':
			if ((y - x) - diffhero >= 1)
			    continue;
		    when 'n':
			if ((y + x) - sumhero <= -1)
			    continue;
		    when 'b':
			if ((y - x) - diffhero <= -1)
			    continue;
		}
		/* 달리기 중지 조건 판단 */
		switch (ch)
		{
		    case DOOR:
			if (x == hero.x || y == hero.y)
			    running = FALSE; /* 문을 정면에서 만나면 멈춤 */
			break;
		    case PASSAGE:
			if (x == hero.x || y == hero.y)
			    passcount++; /* 정면 통로 수 누적 (분기점 감지) */
			break;
		    case FLOOR:
		    case '|':
		    case '-':
		    case ' ':
			break;
		    default:
			running = FALSE; /* 기타 장애물(아이템, 몬스터 등)에서 멈춤 */
			break;
		}
	    }
	}
    if (door_stop && !firstmove && passcount > 1)
	running = FALSE; /* 통로 분기점(둘 이상)에서 달리기 멈춤 */
    if (!running || !jump)
	mvaddch(hero.y, hero.x, PLAYER); /* 플레이어('@') 화면에 표시 */
# ifdef DEBUG
    done = FALSE;
# endif /* DEBUG */
}

/*
 * trip_ch:
 *	Return the character appropriate for this space, taking into
 *	account whether or not the player is tripping.
 *
 * 【역할】 환각(ISHALU) 상태를 고려하여 해당 위치에 표시할 화면 문자를 반환한다.
 *         환각 상태에서는 아이템, 몬스터 등이 다른 무언가로 보일 수 있다.
 * 【매개변수】
 *   y, x - 검사할 맵 좌표
 *   ch   - 원래의 맵 문자
 * 【반환값】 실제로 화면에 표시할 문자
 * 【동작 원리】
 *   환각 중이고 after(데몬/퓨즈 실행 후) 상태이면,
 *   구조물(바닥·통로·문·벽·트랩)을 제외한 오브젝트류를 랜덤 문자로 대체한다.
 *   단, 이미 본 계단(seenstairs)은 환각으로 바뀌지 않는다.
 */
int
trip_ch(int y, int x, int ch)
{
    if (on(player, ISHALU) && after)
	switch (ch)
	{
	    case FLOOR:
	    case ' ':
	    case PASSAGE:
	    case '-':
	    case '|':
	    case DOOR:
	    case TRAP:
		break; /* 구조물은 환각에 영향받지 않음 */
	    default:
		if (y != stairs.y || x != stairs.x || !seenstairs)
		    ch = rnd_thing(); /* 아이템/기타 문자를 랜덤 아이템 문자로 교체 */
		break;
	}
    return ch;
}

/*
 * erase_lamp:
 *	Erase the area shown by a lamp in a dark room.
 *
 * 【역할】 어두운 방(ISDARK)에서 플레이어가 이동한 후,
 *         이전 위치(pos)를 중심으로 램프가 비추던 3×3 영역의 바닥 타일을
 *         공백(' ')으로 되돌린다.
 * 【매개변수】
 *   pos - 지울 이전 위치 좌표
 *   rp  - 이전 위치가 속한 방 포인터
 * 【동작 원리】
 *   - 현재 방이 어둡고(ISDARK), 사라진 방(ISGONE)이 아니며,
 *     플레이어가 눈을 뜨고 있을 때만 지움
 *   - 플레이어 현재 위치(hero)는 지우지 않음
 */

void
erase_lamp(coord *pos, struct room *rp)
{
    int y, x, ey, sy, ex; /* 지울 영역의 행/열 범위 */

    /* 바닥 표시 조건 불충족 시 (밝은 방, 사라진 방, 눈 멀었을 때) 건너뜀 */
    if (!(see_floor && (rp->r_flags & (ISGONE|ISDARK)) == ISDARK
	&& !on(player,ISBLIND)))
	    return;

    ey = pos->y + 1;
    ex = pos->x + 1;
    sy = pos->y - 1;
    for (x = pos->x - 1; x <= ex; x++)
	for (y = sy; y <= ey; y++)
	{
	    if (y == hero.y && x == hero.x)
		continue; /* 플레이어 현재 위치는 건드리지 않음 */
	    move(y, x);
	    if (inch() == FLOOR)
		addch(' '); /* 바닥 타일이면 공백으로 덮어씀 */
	}
}

/*
 * show_floor:
 *	Should we show the floor in her room at this time?
 *
 * 【역할】 현재 방의 바닥(FLOOR)을 화면에 표시해야 하는지 결정한다.
 * 【반환값】 TRUE이면 바닥 표시, FALSE이면 표시하지 않음
 * 【동작 원리】
 *   방이 어두운(ISDARK) 경우에는 see_floor(램프 가시성) 값을 반환하고,
 *   밝은 방이거나 ISGONE(사라진 방/통로)이면 항상 TRUE를 반환한다.
 */
bool
show_floor()
{
    if ((proom->r_flags & (ISGONE|ISDARK)) == ISDARK && !on(player, ISBLIND))
	return see_floor; /* 어두운 방: 램프/시야 여부에 따라 결정 */
    else
	return TRUE; /* 밝은 방 또는 통로: 항상 표시 */
}

/*
 * find_obj:
 *	Find the unclaimed object at y, x
 *
 * 【역할】 맵 좌표 (y, x)에 놓인 미소유 아이템을 lvl_obj 연결 리스트에서 찾는다.
 * 【매개변수】
 *   y, x - 탐색할 맵 좌표
 * 【반환값】 해당 위치의 THING 포인터, 없으면 NULL
 */
THING *
find_obj(int y, int x)
{
    THING *obj; /* lvl_obj 순회용 포인터 */

    /* lvl_obj(레벨 아이템 목록)를 선형 탐색 */
    for (obj = lvl_obj; obj != NULL; obj = next(obj))
    {
	if (obj->o_pos.y == y && obj->o_pos.x == x)
		return obj;
    }
#ifdef MASTER
    sprintf(prbuf, "Non-object %d,%d", y, x);
    msg(prbuf);
    return NULL;
#else
    /* NOTREACHED */
    return NULL;
#endif
}

/*
 * eat:
 *	She wants to eat something, so let her try
 *
 * 【역할】 플레이어가 음식을 먹는 동작을 처리한다.
 *         배고픔 상태(food_left)를 회복하고 특수 메시지를 출력한다.
 * 【동작 원리】
 *   1. FOOD 타입 아이템을 선택 (다른 타입은 먹을 수 없음)
 *   2. food_left(포만도)를 HUNGERTIME ± 200 범위만큼 증가 (최대 STOMACHSIZE)
 *   3. o_which==1 이면 과일(fruit), 아니면 일반 식량
 *   4. 30% 확률로 "맛없지만 경험치 +1" 효과 발동
 */

void
eat()
{
    THING *obj; /* 먹을 아이템 포인터 */

    if ((obj = get_item("eat", FOOD)) == NULL)
	return;
    if (obj->o_type != FOOD)
    {
	if (!terse)
	    msg("ugh, you would get ill if you ate that");
	else
	    msg("that's Inedible!");
	return;
    }
    if (food_left < 0)
	food_left = 0;
    /* 포만도 증가: HUNGERTIME - 200 ~ HUNGERTIME + 200 범위, 최대 STOMACHSIZE */
    if ((food_left += HUNGERTIME - 200 + rnd(400)) > STOMACHSIZE)
	food_left = STOMACHSIZE;
    hungry_state = 0; /* 배고픔 상태 초기화 (배부름) */
    if (obj == cur_weapon)
	cur_weapon = NULL; /* 무기로 쥐고 있던 음식이면 해제 */
    if (obj->o_which == 1)
	msg("my, that was a yummy %s", fruit); /* 과일(fruit) 먹기 */
    else
	if (rnd(100) > 70)
	{
	    pstats.s_exp++;     /* 30% 확률: 맛없는 식량 먹으면 경험치 +1 */
	    msg("%s, this food tastes awful", choose_str("bummer", "yuk"));
	    check_level();      /* 레벨 업 여부 확인 */
	}
	else
	    msg("%s, that tasted good", choose_str("oh, wow", "yum"));
    leave_pack(obj, FALSE, FALSE); /* 팩에서 음식 제거 */
}

/*
 * check_level:
 *	Check to see if the guy has gone up a level.
 *
 * 【역할】 현재 경험치(pstats.s_exp)를 확인하여 플레이어 레벨 업 여부를 판정한다.
 *         레벨이 올라가면 최대 HP와 현재 HP를 증가시키고 메시지를 출력한다.
 * 【동작 원리】
 *   e_levels[] 배열에서 현재 경험치가 몇 번째 임계값을 넘었는지 찾아 레벨을 결정한다.
 *   레벨 상승 시 (새 레벨 - 이전 레벨) × d10 만큼 HP를 추가한다.
 */

void
check_level()
{
    int i, add, olevel; /* i=새 레벨 인덱스, add=추가 HP, olevel=이전 레벨 */

    /* e_levels 배열에서 현재 경험치를 초과하는 첫 번째 임계값 위치를 찾음 */
    for (i = 0; e_levels[i] != 0; i++)
	if (e_levels[i] > pstats.s_exp)
	    break;
    i++;                   /* 1-based 레벨로 보정 */
    olevel = pstats.s_lvl;
    pstats.s_lvl = i;
    if (i > olevel)
    {
	add = roll(i - olevel, 10); /* 레벨 증가 폭 × d10 추가 HP */
	max_hp += add;
	pstats.s_hpt += add;
	msg("welcome to level %d", i);
    }
}

/*
 * chg_str:
 *	used to modify the playes strength.  It keeps track of the
 *	highest it has been, just in case
 *
 * 【역할】 플레이어의 힘(strength) 수치를 amt만큼 변경한다.
 *         장착 중인 힘 추가 반지의 효과를 제외한 '실제 기본 힘'이
 *         역대 최대값보다 크면 max_stats.s_str을 갱신한다.
 * 【매개변수】
 *   amt - 힘 변화량 (양수: 증가, 음수: 감소)
 */

void
chg_str(int amt)
{
    auto str_t comp; /* 반지 효과를 제외한 실제 기본 힘 계산용 임시 변수 */

    if (amt == 0)
	return;
    add_str(&pstats.s_str, amt); /* 실제 힘 수치 변경 */
    comp = pstats.s_str;
    /* 장착 중인 힘 추가 반지(R_ADDSTR)의 보너스를 역산하여 기본 힘을 구함 */
    if (ISRING(LEFT, R_ADDSTR))
	add_str(&comp, -cur_ring[LEFT]->o_arm);
    if (ISRING(RIGHT, R_ADDSTR))
	add_str(&comp, -cur_ring[RIGHT]->o_arm);
    if (comp > max_stats.s_str)
	max_stats.s_str = comp; /* 역대 최대 기본 힘 갱신 */
}

/*
 * add_str:
 *	Perform the actual add, checking upper and lower bound limits
 *
 * 【역할】 힘(strength) 수치에 amt를 더하되, 3~31 범위를 벗어나지 않도록 클램핑한다.
 * 【매개변수】
 *   sp  - 변경할 힘 수치 포인터
 *   amt - 증감량
 */
void
add_str(str_t *sp, int amt)
{
    if ((*sp += amt) < 3)
	*sp = 3;       /* 최솟값: 3 (힘이 3 미만으로 내려가지 않음) */
    else if (*sp > 31)
	*sp = 31;      /* 최댓값: 31 */
}

/*
 * add_haste:
 *	Add a haste to the player
 *
 * 【역할】 플레이어에게 속도 증가(ISHASTE) 효과를 부여한다.
 *         이미 빠른 상태에서 다시 빨라지려 하면 과부하로 기절한다.
 * 【매개변수】
 *   potion - TRUE이면 물약에 의한 효과 (퓨즈로 지속시간 설정)
 * 【반환값】 성공이면 TRUE, 기절(실패)이면 FALSE
 * 【동작 원리】
 *   - 이미 ISHASTE 상태이면 no_command(행동 불가 턴)를 추가하고 기절 처리
 *   - 정상 적용 시 ISHASTE 플래그 설정, potion이면 nohaste 퓨즈 설치
 */
bool
add_haste(bool potion)
{
    if (on(player, ISHASTE))
    {
	/* 이미 속도 증가 중: 과부하로 기절 */
	no_command += rnd(8);            /* 무작위 턴만큼 행동 불가 */
	player.t_flags &= ~(ISRUN|ISHASTE); /* 달리기 및 속도 증가 플래그 해제 */
	extinguish(nohaste);             /* 기존 nohaste 퓨즈 제거 */
	msg("you faint from exhaustion");
	return FALSE;
    }
    else
    {
	player.t_flags |= ISHASTE;       /* 속도 증가 상태 설정 */
	if (potion)
	    fuse(nohaste, 0, rnd(4)+4, AFTER); /* 4~7턴 후 속도 증가 해제 퓨즈 설치 */
	return TRUE;
    }
}

/*
 * aggravate:
 *	Aggravate all the monsters on this level
 *
 * 【역할】 현재 층의 모든 몬스터를 플레이어에게 적대적(추격)으로 만든다.
 *         mlist를 순회하며 각 몬스터의 목표를 플레이어로 설정한다.
 */

void
aggravate()
{
    THING *mp; /* mlist 순회용 몬스터 포인터 */

    for (mp = mlist; mp != NULL; mp = next(mp))
	runto(&mp->t_pos); /* 각 몬스터를 플레이어 방향으로 달리게 설정 */
}

/*
 * vowelstr:
 *      For printfs: if string starts with a vowel, return "n" for an
 *	"an".
 *
 * 【역할】 영어 관사 "a/an" 선택을 위해 문자열이 모음으로 시작하면 "n"을 반환한다.
 *         예: "a" + vowelstr("Elf") + " Elf" → "an Elf"
 * 【매개변수】
 *   str - 확인할 문자열
 * 【반환값】 모음으로 시작하면 "n", 자음이면 ""(빈 문자열)
 */
char *
vowelstr(char *str)
{
    switch (*str)
    {
	case 'a': case 'A':
	case 'e': case 'E':
	case 'i': case 'I':
	case 'o': case 'O':
	case 'u': case 'U':
	    return "n";  /* 모음 시작: "an" 완성을 위해 "n" 반환 */
	default:
	    return "";   /* 자음 시작: "a" 그대로 사용 */
    }
}

/* 
 * is_current:
 *	See if the object is one of the currently used items
 *
 * 【역할】 주어진 아이템이 현재 장착 중(무기/방어구/반지)인지 확인한다.
 *         장착 중이면 메시지를 출력하고 TRUE를 반환한다.
 * 【매개변수】
 *   obj - 확인할 아이템 THING 포인터
 * 【반환값】 장착 중이면 TRUE, 아니면 FALSE
 */
bool
is_current(THING *obj)
{
    if (obj == NULL)
	return FALSE;
    /* cur_armor(방어구), cur_weapon(무기), cur_ring[](반지) 와 비교 */
    if (obj == cur_armor || obj == cur_weapon || obj == cur_ring[LEFT]
	|| obj == cur_ring[RIGHT])
    {
	if (!terse)
	    addmsg("That's already ");
	msg("in use");
	return TRUE;
    }
    return FALSE;
}

/*
 * get_dir:
 *      Set up the direction co_ordinate for use in varios "prefix"
 *	commands
 *
 * 【역할】 방향 키 입력을 받아 이동 방향 벡터(delta)와 방향 문자(dir_ch)를 설정한다.
 *         'h/j/k/l/y/u/b/n' 8방향과 대문자를 지원하며, ESC로 취소 가능하다.
 * 【반환값】 방향 설정 성공이면 TRUE, ESC 취소이면 FALSE
 * 【동작 원리】
 *   - again(재실행) 모드이면 마지막 방향(last_dir/last_delt)을 재사용
 *   - 혼란(ISHUH) 상태에서는 20% 확률로 방향이 무작위로 변경됨
 */
bool
get_dir()
{
    char *prompt;
    bool gotit;
    static coord last_delt= {0,0}; /* 마지막 이동 벡터 저장 (재실행용) */

    if (again && last_dir != '\0')
    {
	/* 재실행 모드: 이전 방향 그대로 사용 */
	delta.y = last_delt.y;
	delta.x = last_delt.x;
	dir_ch = last_dir;
    }
    else
    {
	if (!terse)
	    msg(prompt = "which direction? ");
	else
	    prompt = "direction: ";
	do
	{
	    gotit = TRUE;
	    /* 방향 키를 delta 벡터로 변환 */
	    switch (dir_ch = readchar())
	    {
		case 'h': case'H': delta.y =  0; delta.x = -1; /* 서(西) */
		when 'j': case'J': delta.y =  1; delta.x =  0; /* 남(南) */
		when 'k': case'K': delta.y = -1; delta.x =  0; /* 북(北) */
		when 'l': case'L': delta.y =  0; delta.x =  1; /* 동(東) */
		when 'y': case'Y': delta.y = -1; delta.x = -1; /* 북서(北西) */
		when 'u': case'U': delta.y = -1; delta.x =  1; /* 북동(北東) */
		when 'b': case'B': delta.y =  1; delta.x = -1; /* 남서(南西) */
		when 'n': case'N': delta.y =  1; delta.x =  1; /* 남동(南東) */
		when ESCAPE: last_dir = '\0'; reset_last(); return FALSE; /* 취소 */
		otherwise:
		    mpos = 0;
		    msg(prompt); /* 잘못된 입력: 다시 프롬프트 */
		    gotit = FALSE;
	    }
	} until (gotit);
	if (isupper(dir_ch))
	    dir_ch = (char) tolower(dir_ch); /* 대문자 방향키를 소문자로 통일 */
	last_dir = dir_ch;        /* 마지막 방향 저장 (재실행 대비) */
	last_delt.y = delta.y;
	last_delt.x = delta.x;
    }
    /* 혼란(ISHUH) 상태에서 20% 확률로 방향이 무작위로 바뀜 */
    if (on(player, ISHUH) && rnd(5) == 0)
	do
	{
	    delta.y = rnd(3) - 1;
	    delta.x = rnd(3) - 1;
	} while (delta.y == 0 && delta.x == 0); /* 제자리(0,0)는 재선택 */
    mpos = 0;
    return TRUE;
}

/*
 * sign:
 *	Return the sign of the number
 *
 * 【역할】 정수의 부호를 반환한다.
 * 【반환값】 음수이면 -1, 0이면 0, 양수이면 1
 */
int
sign(int nm)
{
    if (nm < 0)
	return -1;
    else
	return (nm > 0);
}

/*
 * spread:
 *	Give a spread around a given number (+/- 20%)
 *
 * 【역할】 주어진 값(nm)의 ±10% 범위 내 난수를 반환한다.
 *         지속시간 등에 약간의 무작위성을 부여할 때 사용한다.
 * 【매개변수】
 *   nm - 기준 값
 * 【반환값】 nm의 약 ±10% 범위 내 정수
 */
int
spread(int nm)
{
    return nm - nm / 20 + rnd(nm / 10);
}

/*
 * call_it:
 *	Call an object something after use.
 *
 * 【역할】 아이템을 사용한 후 플레이어가 그 아이템에 별명(oi_guess)을 붙이도록 한다.
 *         이미 정체를 알고 있으면(oi_know) 기존 별명만 삭제하고, 
 *         모르는 경우 별명 입력을 받아 저장한다.
 * 【매개변수】
 *   info - 아이템 종류 정보 구조체 포인터 (oi_know=정체 파악 여부, oi_guess=별명)
 */

void
call_it(struct obj_info *info)
{
    if (info->oi_know)
    {
	/* 이미 정체를 알고 있으면 불필요한 별명 삭제 */
	if (info->oi_guess)
	{
	    free(info->oi_guess);
	    info->oi_guess = NULL;
	}
    }
    else if (!info->oi_guess)
    {
	/* 별명이 없으면 플레이어에게 입력받아 저장 */
	msg(terse ? "call it: " : "what do you want to call it? ");
	if (get_str(prbuf, stdscr) == NORM)
	{
	    if (info->oi_guess != NULL)
		free(info->oi_guess);
	    info->oi_guess = malloc((unsigned int) strlen(prbuf) + 1);
	    strcpy(info->oi_guess, prbuf);
	}
    }
}

/*
 * rnd_thing:
 *	Pick a random thing appropriate for this level
 *
 * 【역할】 현재 던전 레벨에 맞는 임의의 아이템 문자를 반환한다.
 *         아뮬렛 레벨 이상이면 아뮬렛(AMULET)을 포함한 전체 목록에서 선택하고,
 *         그 이전 레벨에서는 아뮬렛을 제외하여 선택한다.
 *         환각 상태에서 아이템처럼 보이는 가짜 문자를 만들 때도 사용한다.
 */
char
rnd_thing()
{
    int i;
    static char thing_list[] = {
	POTION, SCROLL, RING, STICK, FOOD, WEAPON, ARMOR, STAIRS, GOLD, AMULET
    };

    if (level >= AMULETLEVEL)
        i = rnd(sizeof thing_list / sizeof (char));       /* 아뮬렛 포함 전체 선택 */
    else
        i = rnd(sizeof thing_list / sizeof (char) - 1);  /* 아뮬렛 제외 선택 */
    return thing_list[i];
}

/*
 str str:
 *	Choose the first or second string depending on whether it the
 *	player is tripping
 *
 * 【역할】 플레이어가 환각(ISHALU) 상태인지에 따라 두 문자열 중 하나를 선택한다.
 * 【매개변수】
 *   ts - 환각 상태일 때 반환할 문자열 ("tripping string")
 *   ns - 정상 상태일 때 반환할 문자열 ("normal string")
 * 【반환값】 환각 상태이면 ts, 정상이면 ns
 */
char *
choose_str(char *ts, char *ns)
{
	return (on(player, ISHALU) ? ts : ns);
}
