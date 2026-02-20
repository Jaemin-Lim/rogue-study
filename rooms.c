/*
 * Create the layout for the new level
 * 새 레벨의 방 배치를 생성하는 함수들을 담은 파일.
 * 방 생성, 방 그리기, 벽 그리기, 미로 생성, 진입/퇴장 처리 등을 담당한다.
 *
 * @(#)rooms.c	4.45 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <ctype.h>
#include <curses.h>
#include "rogue.h"

/* SPOT: 미로 생성 알고리즘에서 사용하는 위치 구조체 */
typedef struct spot {		/* position matrix for maze positions */
                            /* 미로 위치 행렬 */
	int	nexits;    /* 이 위치의 출구 수 */
	coord	exits[4];  /* 출구 방향 좌표 목록 (최대 4방향) */
	int	used;      /* 이미 사용됐는지 여부 */
} SPOT;

#define GOLDGRP 1  /* 금화의 그룹 번호 (같은 그룹은 배낭에서 묶임) */

/*
 * do_rooms:
 *	Create rooms and corridors with a connectivity graph
 *	방을 생성하고 연결 그래프를 만드는 함수.
 *	레벨은 3x3 그리드(9개 슬롯)로 나뉘며, 각 슬롯에 방이 하나씩 들어간다.
 *	0~3개의 방이 없어지고(ISGONE), 나머지는 일반/어둠/미로 방으로 생성된다.
 *	각 방에는 50% 확률로 금화, 일정 확률로 몬스터가 배치된다.
 */

void
do_rooms()
{
    int i;
    struct room *rp;  /* 현재 방 포인터 */
    THING *tp;        /* 몬스터 포인터 */
    int left_out;     /* 없앨 방의 수 (0~3) */
    static coord top; /* 각 방 슬롯의 왼쪽 상단 좌표 */
    coord bsze;				/* maximum room size */
                        /* 각 슬롯의 최대 크기 */
    coord mp;          /* 몬스터 배치 위치 */

    bsze.x = NUMCOLS / 3;  /* 슬롯 가로 크기: 화면 너비의 1/3 */
    bsze.y = NUMLINES / 3; /* 슬롯 세로 크기: 화면 높이의 1/3 */
    /*
     * Clear things for a new level
     * 새 레벨을 위해 방 배열 초기화
     */
    for (rp = rooms; rp < &rooms[MAXROOMS]; rp++)
    {
	rp->r_goldval = 0;   /* 금화 가치 초기화 */
	rp->r_nexits = 0;    /* 출구 수 초기화 */
	rp->r_flags = 0;     /* 플래그 초기화 */
    }
    /*
     * Put the gone rooms, if any, on the level
     * 레벨에서 없어질 방 선택 (0~3개)
     */
    left_out = rnd(4);
    for (i = 0; i < left_out; i++)
	rooms[rnd_room()].r_flags |= ISGONE;  /* ISGONE: 이 슬롯에 실제 방 없음 */
    /*
     * dig and populate all the rooms on the level
     * 모든 방을 생성하고 아이템/몬스터 배치
     */
    for (i = 0, rp = rooms; i < MAXROOMS; rp++, i++)
    {
	/*
	 * Find upper left corner of box that this room goes in
	 * 방이 들어갈 슬롯의 왼쪽 상단 좌표 계산
	 * 슬롯은 3x3 그리드: i%3 = 열, i/3 = 행
	 */
	top.x = (i % 3) * bsze.x + 1;
	top.y = (i / 3) * bsze.y;
	if (rp->r_flags & ISGONE)  /* 없어진 방은 통로 연결용 점만 설정 */
	{
	    /*
	     * Place a gone room.  Make certain that there is a blank line
	     * for passage drawing.
	     * 통로 그리기를 위해 빈 줄이 있도록 위치 설정
	     */
	    do
	    {
		rp->r_pos.x = top.x + rnd(bsze.x - 2) + 1;
		rp->r_pos.y = top.y + rnd(bsze.y - 2) + 1;
		rp->r_max.x = -NUMCOLS;  /* 음수: 없어진 방 표시 */
		rp->r_max.y = -NUMLINES;
	    } until (rp->r_pos.y > 0 && rp->r_pos.y < NUMLINES-1);
	    continue;
	}
	/*
	 * set room type
	 * 방 유형 설정: 레벨이 높을수록 어두운 방/미로 방 가능성 증가
	 */
	if (rnd(10) < level - 1)
	{
	    rp->r_flags |= ISDARK;		/* dark room */
                                    /* 어두운 방 (램프 없이는 못 봄) */
	    if (rnd(15) == 0)
		rp->r_flags = ISMAZE;		/* maze room */
                                    /* 미로 방 (약 1/15 확률) */
	}
	/*
	 * Find a place and size for a random room
	 * 방의 위치와 크기 설정
	 */
	if (rp->r_flags & ISMAZE)  /* 미로 방은 슬롯 전체 사용 */
	{
	    rp->r_max.x = bsze.x - 1;
	    rp->r_max.y = bsze.y - 1;
	    if ((rp->r_pos.x = top.x) == 1)
		rp->r_pos.x = 0;
	    if ((rp->r_pos.y = top.y) == 0)
	    {
		rp->r_pos.y++;
		rp->r_max.y--;
	    }
	}
	else  /* 일반 방: 슬롯 내에서 랜덤 크기와 위치 */
	    do
	    {
		rp->r_max.x = rnd(bsze.x - 4) + 4;  /* 최소 4x4 */
		rp->r_max.y = rnd(bsze.y - 4) + 4;
		rp->r_pos.x = top.x + rnd(bsze.x - rp->r_max.x);
		rp->r_pos.y = top.y + rnd(bsze.y - rp->r_max.y);
	    } until (rp->r_pos.y != 0);  /* y=0은 화면 상단(메시지 줄)이므로 제외 */
	draw_room(rp);  /* 방 그리기 */
	/*
	 * Put the gold in
	 * 50% 확률로 방에 금화 배치 (부적을 갖고 있고 이전 레벨이면 제외)
	 */
	if (rnd(2) == 0 && (!amulet || level >= max_level))
	{
	    THING *gold;

	    gold = new_item();
	    gold->o_goldval = rp->r_goldval = GOLDCALC;  /* 금화 가치 계산 */
	    find_floor(rp, &rp->r_gold, FALSE, FALSE);    /* 방 내 임의 위치 */
	    gold->o_pos = rp->r_gold;
	    chat(rp->r_gold.y, rp->r_gold.x) = GOLD;  /* 맵에 금화('*') 표시 */
	    gold->o_flags = ISMANY;
	    gold->o_group = GOLDGRP;   /* 그룹 번호 (배낭에서 묶임) */
	    gold->o_type = GOLD;
	    attach(lvl_obj, gold);
	}
	/*
	 * Put the monster in
	 * 금화가 있으면 80%, 없으면 25% 확률로 몬스터 배치
	 */
	if (rnd(100) < (rp->r_goldval > 0 ? 80 : 25))
	{
	    tp = new_item();
	    find_floor(rp, &mp, FALSE, TRUE);             /* 몬스터 위치 */
	    new_monster(tp, randmonster(FALSE), &mp);     /* 몬스터 생성 */
	    give_pack(tp);  /* 몬스터에게 아이템 부여 */
	}
    }
}

/*
 * draw_room:
 *	Draw a box around a room and lay down the floor for normal
 *	rooms; for maze rooms, draw maze.
 *	방의 외곽 벽을 그리고 내부를 바닥으로 채우는 함수.
 *	미로 방이면 do_maze()로 미로 생성.
 *	일반 방이면 vert()/horiz()로 벽을 그리고 내부를 FLOOR로 채움.
 */

void
draw_room(struct room *rp)
{
    int y, x;

    if (rp->r_flags & ISMAZE)  /* 미로 방이면 미로 생성 */
	do_maze(rp);
    else  /* 일반 방: 벽 그리기 */
    {
	vert(rp, rp->r_pos.x);				/* Draw left side */
                                        /* 왼쪽 벽 그리기 */
	vert(rp, rp->r_pos.x + rp->r_max.x - 1);	/* Draw right side */
                                                /* 오른쪽 벽 그리기 */
	horiz(rp, rp->r_pos.y);				/* Draw top */
                                        /* 위쪽 벽 그리기 */
	horiz(rp, rp->r_pos.y + rp->r_max.y - 1);	/* Draw bottom */
                                                /* 아래쪽 벽 그리기 */

	/*
	 * Put the floor down
	 * 방 내부를 바닥 문자('.')로 채우기
	 */
	for (y = rp->r_pos.y + 1; y < rp->r_pos.y + rp->r_max.y - 1; y++)
	    for (x = rp->r_pos.x + 1; x < rp->r_pos.x + rp->r_max.x - 1; x++)
		chat(y, x) = FLOOR;  /* places 배열의 p_ch 필드에 바닥 문자 저장 */
    }
}

/*
 * vert:
 *	Draw a vertical line
 *	방의 수직 벽을 그리는 함수.
 *	startx 열의 방 높이 범위에 '|' 문자를 설정한다.
 */

void
vert(struct room *rp, int startx)
{
    int y;

    for (y = rp->r_pos.y + 1; y <= rp->r_max.y + rp->r_pos.y - 1; y++)
	chat(y, startx) = '|';
}

/*
 * horiz:
 *	Draw a horizontal line
 *	방의 수평 벽을 그리는 함수.
 *	starty 행의 방 너비 범위에 '-' 문자를 설정한다.
 */

void
horiz(struct room *rp, int starty)
{
    int x;

    for (x = rp->r_pos.x; x <= rp->r_pos.x + rp->r_max.x - 1; x++)
	chat(starty, x) = '-';
}

/*
 * do_maze:
 *	Dig a maze
 *	미로 방을 생성하는 함수.
 *	재귀적 백트래킹 알고리즘으로 미로를 생성한다.
 *	SPOT 배열(maze[][])로 각 위치의 사용 여부와 출구를 추적한다.
 *	짝수 좌표 위치에서만 시작하여 2칸씩 이동하는 방식으로 미로 패턴을 형성한다.
 */

static int	Maxy, Maxx, Starty, Startx;
/* 미로 크기와 시작 위치 (전역 변수로 재귀 함수에서 사용) */

static SPOT	maze[NUMLINES/3+1][NUMCOLS/3+1];
/* 미로 위치 행렬: 각 위치의 사용 여부와 출구 정보 */


void
do_maze(struct room *rp)
{
    SPOT *sp;       /* 미로 위치 포인터 */
    int starty, startx;   /* 시작 위치 (짝수 좌표) */
    static coord pos;

    /* 미로 배열 초기화 */
    for (sp = &maze[0][0]; sp <= &maze[NUMLINES / 3][NUMCOLS / 3]; sp++)
    {
	sp->used = FALSE;   /* 사용 여부 초기화 */
	sp->nexits = 0;     /* 출구 수 초기화 */
    }

    Maxy = rp->r_max.y;    /* 미로 높이 */
    Maxx = rp->r_max.x;    /* 미로 너비 */
    Starty = rp->r_pos.y;  /* 미로 시작 y 좌표 */
    Startx = rp->r_pos.x;  /* 미로 시작 x 좌표 */
    /* 짝수 위치에서 무작위 시작점 선택 */
    starty = (rnd(rp->r_max.y) / 2) * 2;
    startx = (rnd(rp->r_max.x) / 2) * 2;
    pos.y = starty + Starty;
    pos.x = startx + Startx;
    putpass(&pos);   /* 시작 위치를 통로로 설정 */
    dig(starty, startx);  /* 재귀적 미로 생성 */
}

/*
 * dig:
 *	Dig out from around where we are now, if possible
 *	현재 위치에서 주변을 파는 재귀적 미로 생성 함수.
 *	4방향(상하좌우)으로 2칸씩 탐색하여 아직 파지 않은 위치가 있으면
 *	그 방향을 무작위로 선택하여 통로를 만들고 재귀 호출한다.
 *	del[]: 4방향 2칸 이동 벡터 (2, -2, 0, 0 / 0, 0, 2, -2)
 */

void
dig(int y, int x)
{
    coord *cp;
    int cnt, newy, newx, nexty = 0, nextx = 0;
    static coord pos;
    static coord del[4] = {
	{2, 0}, {-2, 0}, {0, 2}, {0, -2}  /* 4방향 2칸 이동 벡터 */
    };

    for (;;)
    {
	cnt = 0;  /* 이동 가능한 방향 수 */
	for (cp = del; cp <= &del[3]; cp++)  /* 4방향 탐색 */
	{
	    newy = y + cp->y;
	    newx = x + cp->x;
	    if (newy < 0 || newy > Maxy || newx < 0 || newx > Maxx)  /* 경계 초과 */
		continue;
	    if (flat(newy + Starty, newx + Startx) & F_PASS)  /* 이미 통로이면 */
		continue;
	    if (rnd(++cnt) == 0)  /* 균등 확률로 방향 선택 */
	    {
		nexty = newy;
		nextx = newx;
	    }
	}
	if (cnt == 0)  /* 더 이상 팔 곳이 없으면 */
	    return;
	accnt_maze(y, x, nexty, nextx);    /* 현재 위치에 새 출구 추가 */
	accnt_maze(nexty, nextx, y, x);   /* 새 위치에 현재 위치를 출구로 추가 */
	/* 두 위치 사이의 중간 위치도 통로로 설정 */
	if (nexty == y)  /* 가로 방향 이동 */
	{
	    pos.y = y + Starty;
	    if (nextx - x < 0)
		pos.x = nextx + Startx + 1;  /* 왼쪽으로 이동 시 */
	    else
		pos.x = nextx + Startx - 1;  /* 오른쪽으로 이동 시 */
	}
	else  /* 세로 방향 이동 */
	{
	    pos.x = x + Startx;
	    if (nexty - y < 0)
		pos.y = nexty + Starty + 1;  /* 위쪽으로 이동 시 */
	    else
		pos.y = nexty + Starty - 1;  /* 아래쪽으로 이동 시 */
	}
	putpass(&pos);  /* 중간 위치를 통로로 */
	pos.y = nexty + Starty;
	pos.x = nextx + Startx;
	putpass(&pos);  /* 새 위치를 통로로 */
	dig(nexty, nextx);  /* 새 위치에서 재귀 호출 */
    }
}

/*
 * accnt_maze:
 *	Account for maze exits
 *	미로에서 위치 (y,x)에 (ny,nx) 방향 출구를 추가하는 함수.
 *	이미 출구가 등록된 경우 중복 추가를 방지한다.
 *	SPOT.nexits: 현재 출구 수, SPOT.exits[]: 출구 좌표 배열
 */

void
accnt_maze(int y, int x, int ny, int nx)
{
    SPOT *sp;  /* 현재 위치의 SPOT 구조체 */
    coord *cp;

    sp = &maze[y][x];
    /* 이미 등록된 출구인지 확인 */
    for (cp = sp->exits; cp < &sp->exits[sp->nexits]; cp++)
	if (cp->y == ny && cp->x == nx)
	    return;  /* 이미 있으면 추가하지 않음 */
    /* 새 출구 추가 */
    cp->y = ny;
    cp->x = nx;
    /* 주의: sp->nexits는 증가시키지 않지만 포인터로 접근 (원본 코드 유지) */
}

/*
 * rnd_pos:
 *	Pick a random spot in a room
 *	방 내부(벽 제외)의 임의 위치를 선택하는 함수.
 *	r_pos.x+1 ~ r_pos.x+r_max.x-2 범위 (양쪽 벽 제외)
 */

void
rnd_pos(struct room *rp, coord *cp)
{
    cp->x = rp->r_pos.x + rnd(rp->r_max.x - 2) + 1;
    cp->y = rp->r_pos.y + rnd(rp->r_max.y - 2) + 1;
}

/*
 * find_floor:
 *	Find a valid floor spot in this room.  If rp is NULL, then
 *	pick a new room each time around the loop.
 *	방에서 유효한 바닥 위치를 찾는 함수.
 *	rp가 NULL이면 매번 랜덤 방에서 찾는다.
 *	monst: TRUE이면 몬스터가 없는 위치를 찾음, FALSE이면 바닥 문자만 확인.
 *	limit > 0: 최대 limit번 시도하고 실패하면 FALSE 반환.
 *	limit == 0: 무한 시도 (반드시 찾을 때까지).
 */
bool
find_floor(struct room *rp, coord *cp, int limit, bool monst)
{
    PLACE *pp;           /* 위치 데이터 포인터 */
    int cnt;             /* 남은 시도 횟수 */
    char compchar = 0;   /* 비교할 바닥 문자 */
    bool pickroom;       /* 매번 새 방을 선택할지 여부 */

    pickroom = (bool)(rp == NULL);

    if (!pickroom)  /* 방이 지정된 경우 */
	compchar = ((rp->r_flags & ISMAZE) ? PASSAGE : FLOOR);  /* 미로: 통로, 일반: 바닥 */
    cnt = limit;
    for (;;)
    {
	if (limit && cnt-- == 0)  /* 시도 횟수 초과 */
	    return FALSE;
	if (pickroom)  /* 매번 새 방 선택 */
	{
	    rp = &rooms[rnd_room()];
	    compchar = ((rp->r_flags & ISMAZE) ? PASSAGE : FLOOR);
	}
	rnd_pos(rp, cp);  /* 방 내 랜덤 위치 선택 */
	pp = INDEX(cp->y, cp->x);
	if (monst)  /* 몬스터 배치 가능 여부 확인 */
	{
	    if (pp->p_monst == NULL && step_ok(pp->p_ch))  /* 몬스터 없고 이동 가능 */
		return TRUE;
	}
	else if (pp->p_ch == compchar)  /* 바닥 문자와 일치 */
	    return TRUE;
    }
}

/*
 * enter_room:
 *	Code that is executed whenver you appear in a room
 *	영웅이 방에 진입할 때 실행되는 함수.
 *	proom을 현재 방으로 갱신하고, 방 안의 몬스터들을 깨운다.
 *	어둡지 않고 눈이 멀지 않은 경우 방의 모든 내용물을 화면에 표시한다.
 *	몬스터가 있으면 t_oldch를 갱신하고 볼 수 있으면 위장 문자를 표시한다.
 */

void
enter_room(coord *cp)
{
    struct room *rp;   /* 현재 방 포인터 */
    THING *tp;         /* 몬스터 포인터 */
    int y, x;
    char ch;

    rp = proom = roomin(cp);  /* 해당 좌표의 방 설정 */
    door_open(rp);             /* 방 안의 몬스터 깨우기 */
    /* 어둡지 않고 눈이 멀지 않은 경우 방 내용 표시 */
    if (!(rp->r_flags & ISDARK) && !on(player, ISBLIND))
	for (y = rp->r_pos.y; y < rp->r_max.y + rp->r_pos.y; y++)
	{
	    move(y, rp->r_pos.x);
	    for (x = rp->r_pos.x; x < rp->r_max.x + rp->r_pos.x; x++)
	    {
		tp = moat(y, x);    /* 해당 위치의 몬스터 */
		ch = chat(y, x);    /* 맵 데이터의 문자 */
		if (tp == NULL)     /* 몬스터가 없으면 */
		    if (CCHAR(inch()) != ch)  /* 화면 문자와 다르면 */
			addch(ch);              /* 맵 문자 표시 */
		    else
			move(y, x + 1);  /* 같으면 커서만 이동 */
		else  /* 몬스터가 있으면 */
		{
		    tp->t_oldch = ch;  /* 몬스터 아래 배경 문자 저장 */
		    if (!see_monst(tp))  /* 몬스터를 볼 수 없으면 */
			if (on(player, SEEMONST))  /* 몬스터 감지 능력이 있으면 */
			{
			    standout();
			    addch(tp->t_disguise);  /* 강조 표시 */
			    standend();
			}
			else
			    addch(ch);  /* 배경만 표시 */
		    else
			addch(tp->t_disguise);  /* 몬스터 위장 문자 표시 */
		}
	    }
	}
}

/*
 * leave_room:
 *	Code for when we exit a room
 *	영웅이 방을 나갈 때 실행되는 함수.
 *	proom을 통로로 갱신하고, 방 안의 내용을 숨기거나 어두운 방 처리를 한다.
 *	미로 방(ISMAZE)은 처리하지 않는다.
 *	어두운 방(ISDARK)에서는 방 내부를 공백(' ')으로 처리한다.
 *	몬스터 감지 능력이 있으면 방을 나간 후에도 강조 표시를 유지한다.
 */

void
leave_room(coord *cp)
{
    PLACE *pp;       /* 위치 데이터 포인터 */
    struct room *rp; /* 현재 방 포인터 */
    int y, x;
    char floor;      /* 바닥에 표시할 문자 */
    char ch;

    rp = proom;

    if (rp->r_flags & ISMAZE)  /* 미로 방은 특별 처리 없음 */
	return;

    /* 나간 후 바닥 표시 문자 결정 */
    if (rp->r_flags & ISGONE)         /* 없어진 방(통로): 통로 문자 */
	floor = PASSAGE;
    else if (!(rp->r_flags & ISDARK) || on(player, ISBLIND))  /* 밝은 방: 바닥 문자 */
	floor = FLOOR;
    else
	floor = ' ';  /* 어두운 방: 방 내부 숨김 */

    proom = &passages[flat(cp->y, cp->x) & F_PNUM];  /* 이동한 통로 번호 */
    for (y = rp->r_pos.y; y < rp->r_max.y + rp->r_pos.y; y++)
	for (x = rp->r_pos.x; x < rp->r_max.x + rp->r_pos.x; x++)
	{
	    move(y, x);
	    switch ( ch = CCHAR(inch()) )  /* 화면에 표시된 문자 */
	    {
		case FLOOR:  /* 바닥 문자 처리 */
		    if (floor == ' ' && ch != ' ')  /* 어두운 방이면 숨기기 */
			addch(' ');
		    break;
		default:
		    /*
		     * to check for monster, we have to strip out
		     * standout bit
		     * 몬스터 확인을 위해 강조 비트 제거 필요
		     */
		    if (isupper(toascii(ch)))  /* 대문자이면 몬스터 */
		    {
			if (on(player, SEEMONST))  /* 몬스터 감지 능력이 있으면 */
			{
			    standout();
			    addch(ch);
			    standend();
			    break;
			}
                        pp = INDEX(y,x);
			addch(pp->p_ch == DOOR ? DOOR : floor);  /* 문이면 문, 아니면 바닥 */
		    }
	    }
	}
    door_open(rp);  /* 방을 나가는 중에도 몬스터 깨우기 */
}

#include <ctype.h>
#include <curses.h>
#include "rogue.h"

typedef struct spot {		/* position matrix for maze positions */
	int	nexits;
	coord	exits[4];
	int	used;
} SPOT;

#define GOLDGRP 1

/*
 * do_rooms:
 *	Create rooms and corridors with a connectivity graph
 */

void
do_rooms()
{
    int i;
    struct room *rp;
    THING *tp;
    int left_out;
    static coord top;
    coord bsze;				/* maximum room size */
    coord mp;

    bsze.x = NUMCOLS / 3;
    bsze.y = NUMLINES / 3;
    /*
     * Clear things for a new level
     */
    for (rp = rooms; rp < &rooms[MAXROOMS]; rp++)
    {
	rp->r_goldval = 0;
	rp->r_nexits = 0;
	rp->r_flags = 0;
    }
    /*
     * Put the gone rooms, if any, on the level
     */
    left_out = rnd(4);
    for (i = 0; i < left_out; i++)
	rooms[rnd_room()].r_flags |= ISGONE;
    /*
     * dig and populate all the rooms on the level
     */
    for (i = 0, rp = rooms; i < MAXROOMS; rp++, i++)
    {
	/*
	 * Find upper left corner of box that this room goes in
	 */
	top.x = (i % 3) * bsze.x + 1;
	top.y = (i / 3) * bsze.y;
	if (rp->r_flags & ISGONE)
	{
	    /*
	     * Place a gone room.  Make certain that there is a blank line
	     * for passage drawing.
	     */
	    do
	    {
		rp->r_pos.x = top.x + rnd(bsze.x - 2) + 1;
		rp->r_pos.y = top.y + rnd(bsze.y - 2) + 1;
		rp->r_max.x = -NUMCOLS;
		rp->r_max.y = -NUMLINES;
	    } until (rp->r_pos.y > 0 && rp->r_pos.y < NUMLINES-1);
	    continue;
	}
	/*
	 * set room type
	 */
	if (rnd(10) < level - 1)
	{
	    rp->r_flags |= ISDARK;		/* dark room */
	    if (rnd(15) == 0)
		rp->r_flags = ISMAZE;		/* maze room */
	}
	/*
	 * Find a place and size for a random room
	 */
	if (rp->r_flags & ISMAZE)
	{
	    rp->r_max.x = bsze.x - 1;
	    rp->r_max.y = bsze.y - 1;
	    if ((rp->r_pos.x = top.x) == 1)
		rp->r_pos.x = 0;
	    if ((rp->r_pos.y = top.y) == 0)
	    {
		rp->r_pos.y++;
		rp->r_max.y--;
	    }
	}
	else
	    do
	    {
		rp->r_max.x = rnd(bsze.x - 4) + 4;
		rp->r_max.y = rnd(bsze.y - 4) + 4;
		rp->r_pos.x = top.x + rnd(bsze.x - rp->r_max.x);
		rp->r_pos.y = top.y + rnd(bsze.y - rp->r_max.y);
	    } until (rp->r_pos.y != 0);
	draw_room(rp);
	/*
	 * Put the gold in
	 */
	if (rnd(2) == 0 && (!amulet || level >= max_level))
	{
	    THING *gold;

	    gold = new_item();
	    gold->o_goldval = rp->r_goldval = GOLDCALC;
	    find_floor(rp, &rp->r_gold, FALSE, FALSE);
	    gold->o_pos = rp->r_gold;
	    chat(rp->r_gold.y, rp->r_gold.x) = GOLD;
	    gold->o_flags = ISMANY;
	    gold->o_group = GOLDGRP;
	    gold->o_type = GOLD;
	    attach(lvl_obj, gold);
	}
	/*
	 * Put the monster in
	 */
	if (rnd(100) < (rp->r_goldval > 0 ? 80 : 25))
	{
	    tp = new_item();
	    find_floor(rp, &mp, FALSE, TRUE);
	    new_monster(tp, randmonster(FALSE), &mp);
	    give_pack(tp);
	}
    }
}

/*
 * draw_room:
 *	Draw a box around a room and lay down the floor for normal
 *	rooms; for maze rooms, draw maze.
 */

void
draw_room(struct room *rp)
{
    int y, x;

    if (rp->r_flags & ISMAZE)
	do_maze(rp);
    else
    {
	vert(rp, rp->r_pos.x);				/* Draw left side */
	vert(rp, rp->r_pos.x + rp->r_max.x - 1);	/* Draw right side */
	horiz(rp, rp->r_pos.y);				/* Draw top */
	horiz(rp, rp->r_pos.y + rp->r_max.y - 1);	/* Draw bottom */

	/*
	 * Put the floor down
	 */
	for (y = rp->r_pos.y + 1; y < rp->r_pos.y + rp->r_max.y - 1; y++)
	    for (x = rp->r_pos.x + 1; x < rp->r_pos.x + rp->r_max.x - 1; x++)
		chat(y, x) = FLOOR;
    }
}

/*
 * vert:
 *	Draw a vertical line
 */

void
vert(struct room *rp, int startx)
{
    int y;

    for (y = rp->r_pos.y + 1; y <= rp->r_max.y + rp->r_pos.y - 1; y++)
	chat(y, startx) = '|';
}

/*
 * horiz:
 *	Draw a horizontal line
 */

void
horiz(struct room *rp, int starty)
{
    int x;

    for (x = rp->r_pos.x; x <= rp->r_pos.x + rp->r_max.x - 1; x++)
	chat(starty, x) = '-';
}

/*
 * do_maze:
 *	Dig a maze
 */

static int	Maxy, Maxx, Starty, Startx;

static SPOT	maze[NUMLINES/3+1][NUMCOLS/3+1];


void
do_maze(struct room *rp)
{
    SPOT *sp;
    int starty, startx;
    static coord pos;

    for (sp = &maze[0][0]; sp <= &maze[NUMLINES / 3][NUMCOLS / 3]; sp++)
    {
	sp->used = FALSE;
	sp->nexits = 0;
    }

    Maxy = rp->r_max.y;
    Maxx = rp->r_max.x;
    Starty = rp->r_pos.y;
    Startx = rp->r_pos.x;
    starty = (rnd(rp->r_max.y) / 2) * 2;
    startx = (rnd(rp->r_max.x) / 2) * 2;
    pos.y = starty + Starty;
    pos.x = startx + Startx;
    putpass(&pos);
    dig(starty, startx);
}

/*
 * dig:
 *	Dig out from around where we are now, if possible
 */

void
dig(int y, int x)
{
    coord *cp;
    int cnt, newy, newx, nexty = 0, nextx = 0;
    static coord pos;
    static coord del[4] = {
	{2, 0}, {-2, 0}, {0, 2}, {0, -2}
    };

    for (;;)
    {
	cnt = 0;
	for (cp = del; cp <= &del[3]; cp++)
	{
	    newy = y + cp->y;
	    newx = x + cp->x;
	    if (newy < 0 || newy > Maxy || newx < 0 || newx > Maxx)
		continue;
	    if (flat(newy + Starty, newx + Startx) & F_PASS)
		continue;
	    if (rnd(++cnt) == 0)
	    {
		nexty = newy;
		nextx = newx;
	    }
	}
	if (cnt == 0)
	    return;
	accnt_maze(y, x, nexty, nextx);
	accnt_maze(nexty, nextx, y, x);
	if (nexty == y)
	{
	    pos.y = y + Starty;
	    if (nextx - x < 0)
		pos.x = nextx + Startx + 1;
	    else
		pos.x = nextx + Startx - 1;
	}
	else
	{
	    pos.x = x + Startx;
	    if (nexty - y < 0)
		pos.y = nexty + Starty + 1;
	    else
		pos.y = nexty + Starty - 1;
	}
	putpass(&pos);
	pos.y = nexty + Starty;
	pos.x = nextx + Startx;
	putpass(&pos);
	dig(nexty, nextx);
    }
}

/*
 * accnt_maze:
 *	Account for maze exits
 */

void
accnt_maze(int y, int x, int ny, int nx)
{
    SPOT *sp;
    coord *cp;

    sp = &maze[y][x];
    for (cp = sp->exits; cp < &sp->exits[sp->nexits]; cp++)
	if (cp->y == ny && cp->x == nx)
	    return;
    cp->y = ny;
    cp->x = nx;
}

/*
 * rnd_pos:
 *	Pick a random spot in a room
 */

void
rnd_pos(struct room *rp, coord *cp)
{
    cp->x = rp->r_pos.x + rnd(rp->r_max.x - 2) + 1;
    cp->y = rp->r_pos.y + rnd(rp->r_max.y - 2) + 1;
}

/*
 * find_floor:
 *	Find a valid floor spot in this room.  If rp is NULL, then
 *	pick a new room each time around the loop.
 */
bool
find_floor(struct room *rp, coord *cp, int limit, bool monst)
{
    PLACE *pp;
    int cnt;
    char compchar = 0;
    bool pickroom;

    pickroom = (bool)(rp == NULL);

    if (!pickroom)
	compchar = ((rp->r_flags & ISMAZE) ? PASSAGE : FLOOR);
    cnt = limit;
    for (;;)
    {
	if (limit && cnt-- == 0)
	    return FALSE;
	if (pickroom)
	{
	    rp = &rooms[rnd_room()];
	    compchar = ((rp->r_flags & ISMAZE) ? PASSAGE : FLOOR);
	}
	rnd_pos(rp, cp);
	pp = INDEX(cp->y, cp->x);
	if (monst)
	{
	    if (pp->p_monst == NULL && step_ok(pp->p_ch))
		return TRUE;
	}
	else if (pp->p_ch == compchar)
	    return TRUE;
    }
}

/*
 * enter_room:
 *	Code that is executed whenver you appear in a room
 */

void
enter_room(coord *cp)
{
    struct room *rp;
    THING *tp;
    int y, x;
    char ch;

    rp = proom = roomin(cp);
    door_open(rp);
    if (!(rp->r_flags & ISDARK) && !on(player, ISBLIND))
	for (y = rp->r_pos.y; y < rp->r_max.y + rp->r_pos.y; y++)
	{
	    move(y, rp->r_pos.x);
	    for (x = rp->r_pos.x; x < rp->r_max.x + rp->r_pos.x; x++)
	    {
		tp = moat(y, x);
		ch = chat(y, x);
		if (tp == NULL)
		    if (CCHAR(inch()) != ch)
			addch(ch);
		    else
			move(y, x + 1);
		else
		{
		    tp->t_oldch = ch;
		    if (!see_monst(tp))
			if (on(player, SEEMONST))
			{
			    standout();
			    addch(tp->t_disguise);
			    standend();
			}
			else
			    addch(ch);
		    else
			addch(tp->t_disguise);
		}
	    }
	}
}

/*
 * leave_room:
 *	Code for when we exit a room
 */

void
leave_room(coord *cp)
{
    PLACE *pp;
    struct room *rp;
    int y, x;
    char floor;
    char ch;

    rp = proom;

    if (rp->r_flags & ISMAZE)
	return;

    if (rp->r_flags & ISGONE)
	floor = PASSAGE;
    else if (!(rp->r_flags & ISDARK) || on(player, ISBLIND))
	floor = FLOOR;
    else
	floor = ' ';

    proom = &passages[flat(cp->y, cp->x) & F_PNUM];
    for (y = rp->r_pos.y; y < rp->r_max.y + rp->r_pos.y; y++)
	for (x = rp->r_pos.x; x < rp->r_max.x + rp->r_pos.x; x++)
	{
	    move(y, x);
	    switch ( ch = CCHAR(inch()) )
	    {
		case FLOOR:
		    if (floor == ' ' && ch != ' ')
			addch(' ');
		    break;
		default:
		    /*
		     * to check for monster, we have to strip out
		     * standout bit
		     */
		    if (isupper(toascii(ch)))
		    {
			if (on(player, SEEMONST))
			{
			    standout();
			    addch(ch);
			    standend();
			    break;
			}
                        pp = INDEX(y,x);
			addch(pp->p_ch == DOOR ? DOOR : floor);
		    }
	    }
	}
    door_open(rp);
}
