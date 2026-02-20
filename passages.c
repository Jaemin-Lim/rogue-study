/*
 * Draw the connecting passages
 *
 * @(#)passages.c	4.22 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

/*
 * [파일 개요]
 * passages.c - 로그(Rogue) 던전의 통로(복도) 생성을 담당하는 파일.
 *
 * 주요 기능:
 *   - do_passages() : 레벨 전체의 통로를 생성하는 진입점 함수
 *   - conn()        : 두 방 사이를 L자형 통로로 연결하는 함수
 *   - putpass()     : 통로 타일을 맵에 배치 (비밀 통로 확률 포함)
 *   - door()        : 방 출입구에 문(또는 비밀 문)을 배치
 *   - passnum()     : 통로에 번호를 부여하는 함수 (패시지 그래프 완성 후 호출)
 *   - numpass()     : 재귀적으로 연결된 통로 타일에 번호를 할당
 *   - add_pass()    : 위자드 모드 전용 - 통로를 화면에 표시
 *
 * 통로 생성 알고리즘:
 *   - 9개의 방(3x3 그리드)이 rdes[] 그래프로 연결 가능 여부를 정의한다.
 *   - 스패닝 트리 방식으로 먼저 모든 방을 최소 1개의 통로로 연결한다.
 *   - 이후 rnd(5)번의 추가 통로를 무작위로 더 생성하여 경로를 다양하게 한다.
 *   - 각 통로는 conn()을 통해 실제 맵에 '#' 타일로 그려진다.
 *
 * 관련 상수/구조체:
 *   - MAXROOMS  : 방의 최대 수 (9)
 *   - rooms[]   : 방 구조체 배열
 *   - passages[]: 통로 구조체 배열 (passnum 결과)
 *   - F_PASS    : 통로 플래그 (p_flags 비트)
 *   - F_REAL    : 실제 타일 플래그 (비밀 통로이면 해제됨)
 *   - PASSAGE   : 통로 표시 문자 ('#')
 *   - DOOR      : 문 표시 문자 ('+')
 */

#include <stdlib.h>
#include <curses.h>
#include "rogue.h"

/*
 * do_passages:
 *	Draw all the passages on a level.
 *
 * [한국어 설명]
 * 레벨에 존재하는 모든 방(최대 MAXROOMS=9개)을 통로로 연결하는 최상위 함수.
 *
 * 처리 흐름:
 *   1. rdes[] 그래프를 초기화 (isconn, ingraph 리셋)
 *   2. 무작위 방에서 시작하여 스패닝 트리 방식으로 모든 방을 연결
 *      - conn[i]: 방 i와 연결 가능 여부 (그리드 인접 관계)
 *      - isconn[i]: 이미 통로가 생성된 여부
 *      - ingraph: 이미 연결된 방 여부
 *   3. rnd(5)번 추가 통로 생성 (중복 경로 방지를 위해 !isconn인 경우만)
 *   4. passnum()으로 각 통로에 번호 부여
 *
 * rdes[] 배열의 conn[][] 초기화:
 *   방 번호는 3x3 그리드에서 0~8 (행 우선):
 *     0-1-2
 *     3-4-5
 *     6-7-8
 *   인접 관계만 연결 가능 (대각선 불가).
 */

void
do_passages()
{
    struct rdes *r1, *r2 = NULL;
    int i, j;
    int roomcount;
    static struct rdes
    {
	bool	conn[MAXROOMS];		/* possible to connect to room i? */
	bool	isconn[MAXROOMS];	/* connection been made to room i? */
	bool	ingraph;		/* this room in graph already? */
    } rdes[MAXROOMS] = {
	{ { 0, 1, 0, 1, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 1, 0, 1, 0, 1, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 0, 1, 0, 0, 0, 1, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 1, 0, 0, 0, 1, 0, 1, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 0, 1, 0, 1, 0, 1, 0, 1, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 0, 0, 1, 0, 1, 0, 0, 0, 1 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 0, 0, 0, 1, 0, 0, 0, 1, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 0, 0, 0, 0, 1, 0, 1, 0, 1 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 0, 0, 0, 0, 0, 1, 0, 1, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
    };

    /*
     * reinitialize room graph description
     * 방 그래프 기술자를 재초기화한다.
     * 이전 레벨에서 남은 isconn(연결 여부)와 ingraph(그래프 포함 여부)를 모두 리셋.
     */
    for (r1 = rdes; r1 <= &rdes[MAXROOMS-1]; r1++)
    {
	for (j = 0; j < MAXROOMS; j++)
	    r1->isconn[j] = FALSE;
	r1->ingraph = FALSE;
    }

    /*
     * starting with one room, connect it to a random adjacent room and
     * then pick a new room to start with.
     * 무작위 방 하나를 그래프의 시작점으로 삼고,
     * 인접한 방을 하나씩 추가해 스패닝 트리를 완성한다.
     * 모든 방이 그래프에 포함될 때까지 반복한다.
     */
    roomcount = 1;
    r1 = &rdes[rnd(MAXROOMS)];
    r1->ingraph = TRUE;
    do
    {
	/*
	 * find a room to connect with
	 * 현재 방(r1)에서 연결 가능하면서 아직 그래프에 없는 방 중
	 * 무작위로 하나(r2)를 선택한다.
	 */
	j = 0;
	for (i = 0; i < MAXROOMS; i++)
	    if (r1->conn[i] && !rdes[i].ingraph && rnd(++j) == 0)
		r2 = &rdes[i];
	/*
	 * if no adjacent rooms are outside the graph, pick a new room
	 * to look from
	 * 연결할 수 있는 외부 방이 없으면 이미 그래프에 있는 방 중
	 * 무작위로 새 출발점을 선택한다.
	 */
	if (j == 0)
	{
	    do
		r1 = &rdes[rnd(MAXROOMS)];
	    until (r1->ingraph);
	}
	/*
	 * otherwise, connect new room to the graph, and draw a tunnel
	 * to it
	 * 연결 가능한 외부 방(r2)이 있으면 그래프에 추가하고 conn()으로 통로를 그린다.
	 * 양방향 isconn을 TRUE로 설정하여 중복 연결을 방지한다.
	 */
	else
	{
	    r2->ingraph = TRUE;
	    i = (int)(r1 - rdes);
	    j = (int)(r2 - rdes);
	    conn(i, j);
	    r1->isconn[j] = TRUE;
	    r2->isconn[i] = TRUE;
	    roomcount++;
	}
    } while (roomcount < MAXROOMS);

    /*
     * attempt to add passages to the graph a random number of times so
     * that there isn't always just one unique passage through it.
     * 스패닝 트리 완성 후 추가 통로를 최대 rnd(5)번 생성하여
     * 던전 내 경로를 더 다양하게 만든다.
     * 이미 연결된 방(!isconn)이 아닌 경우에만 추가 통로를 그린다.
     */
    for (roomcount = rnd(5); roomcount > 0; roomcount--)
    {
	r1 = &rdes[rnd(MAXROOMS)];	/* a random room to look from */
	/*
	 * find an adjacent room not already connected
	 * 현재 방(r1)에서 연결 가능하지만 아직 연결되지 않은 방을 무작위로 선택.
	 */
	j = 0;
	for (i = 0; i < MAXROOMS; i++)
	    if (r1->conn[i] && !r1->isconn[i] && rnd(++j) == 0)
		r2 = &rdes[i];
	/*
	 * if there is one, connect it and look for the next added
	 * passage
	 * 연결할 방이 있으면 conn()으로 통로를 그리고 isconn을 양방향으로 설정한다.
	 */
	if (j != 0)
	{
	    i = (int)(r1 - rdes);
	    j = (int)(r2 - rdes);
	    conn(i, j);
	    r1->isconn[j] = TRUE;
	    r2->isconn[i] = TRUE;
	}
    }
    passnum();
}

/*
 * conn:
 *	Draw a corridor from a room in a certain direction.
 *
 * [한국어 설명]
 * 방 r1과 방 r2 사이에 L자형 통로를 그린다.
 *
 * 매개변수:
 *   r1, r2 - rooms[] 배열의 인덱스 (0~8)
 *
 * 방향 결정:
 *   - r1 < r2이면 r1이 출발점
 *   - r2 == r1 + 1이면 오른쪽('r') 방향, r2 == r1 + 3이면 아래('d') 방향
 *
 * 통로 구조:
 *   - spos: 출발 방의 문 위치 (벽 위에 무작위 선택)
 *   - epos: 도착 방의 문 위치
 *   - del: 주 이동 방향 (d: y+1, r: x+1)
 *   - turn_delta: 수평/수직 꺾임 방향
 *   - turn_spot: 꺾이는 지점 (distance 내 무작위)
 *   - turn_distance: 꺾임 후 이동할 거리 (두 방의 오프셋 차이)
 *
 * ISMAZE 방이면 F_PASS 비트가 있는 위치에서만 문을 배치한다.
 * 통로 끝에서 curr != epos이면 연결 오류 경고 메시지를 출력한다.
 */

void
conn(int r1, int r2)
{
    struct room *rpf, *rpt = NULL;
    int rmt;
    int distance = 0, turn_spot, turn_distance = 0;
    int rm;
    char direc;
    static coord del, curr, turn_delta, spos, epos;

    if (r1 < r2)
    {
	rm = r1;
	if (r1 + 1 == r2)
	    direc = 'r';
	else
	    direc = 'd';
    }
    else
    {
	rm = r2;
	if (r2 + 1 == r1)
	    direc = 'r';
	else
	    direc = 'd';
    }
    rpf = &rooms[rm];
    /*
     * Set up the movement variables, in two cases:
     * first drawing one down.
     * 이동 변수 설정 - 아래('d') 방향의 경우:
     *   del: y 방향으로 이동 (del.y=1, del.x=0)
     *   spos: 출발 방의 하단 벽에서 무작위 x 위치
     *   epos: 도착 방의 상단 벽에서 무작위 x 위치
     *   distance: 두 방 사이의 y 거리 - 1
     *   turn_delta: x 방향으로 꺾임 (spos.x < epos.x 이면 +1, 아니면 -1)
     *   turn_distance: |spos.x - epos.x| (꺾어야 할 수평 거리)
     */
    if (direc == 'd')
    {
	rmt = rm + 3;				/* room # of dest */
	rpt = &rooms[rmt];			/* room pointer of dest */
	del.x = 0;				/* direction of move */
	del.y = 1;
	spos.x = rpf->r_pos.x;			/* start of move */
	spos.y = rpf->r_pos.y;
	epos.x = rpt->r_pos.x;			/* end of move */
	epos.y = rpt->r_pos.y;
	if (!(rpf->r_flags & ISGONE))		/* if not gone pick door pos */
	    do
	    {
		spos.x = rpf->r_pos.x + rnd(rpf->r_max.x - 2) + 1;
		spos.y = rpf->r_pos.y + rpf->r_max.y - 1;
	    } while ((rpf->r_flags&ISMAZE) && !(flat(spos.y, spos.x)&F_PASS));
	if (!(rpt->r_flags & ISGONE))
	    do
	    {
		epos.x = rpt->r_pos.x + rnd(rpt->r_max.x - 2) + 1;
	    } while ((rpt->r_flags&ISMAZE) && !(flat(epos.y, epos.x)&F_PASS));
	distance = abs(spos.y - epos.y) - 1;	/* distance to move */
	turn_delta.y = 0;			/* direction to turn */
	turn_delta.x = (spos.x < epos.x ? 1 : -1);
	turn_distance = abs(spos.x - epos.x);	/* how far to turn */
    }
    else if (direc == 'r')			/* setup for moving right */
    /* 오른쪽('r') 방향의 경우:
     *   del: x 방향으로 이동 (del.x=1, del.y=0)
     *   spos: 출발 방의 오른쪽 벽에서 무작위 y 위치
     *   epos: 도착 방의 왼쪽 벽에서 무작위 y 위치
     *   distance: 두 방 사이의 x 거리 - 1
     *   turn_delta: y 방향으로 꺾임
     */
    {
	rmt = rm + 1;
	rpt = &rooms[rmt];
	del.x = 1;
	del.y = 0;
	spos.x = rpf->r_pos.x;
	spos.y = rpf->r_pos.y;
	epos.x = rpt->r_pos.x;
	epos.y = rpt->r_pos.y;
	if (!(rpf->r_flags & ISGONE))
	    do
	    {
		spos.x = rpf->r_pos.x + rpf->r_max.x - 1;
		spos.y = rpf->r_pos.y + rnd(rpf->r_max.y - 2) + 1;
	    } while ((rpf->r_flags&ISMAZE) && !(flat(spos.y, spos.x)&F_PASS));
	if (!(rpt->r_flags & ISGONE))
	    do
	    {
		epos.y = rpt->r_pos.y + rnd(rpt->r_max.y - 2) + 1;
	    } while ((rpt->r_flags&ISMAZE) && !(flat(epos.y, epos.x)&F_PASS));
	distance = abs(spos.x - epos.x) - 1;
	turn_delta.y = (spos.y < epos.y ? 1 : -1);
	turn_delta.x = 0;
	turn_distance = abs(spos.y - epos.y);
    }
#ifdef MASTER
    else
	debug("error in connection tables");
#endif

    turn_spot = rnd(distance - 1) + 1;		/* where turn starts */
    /* turn_spot: 통로가 꺾이기 시작하는 지점 (distance 내 무작위 선택) */

    /*
     * Draw in the doors on either side of the passage or just put #'s
     * if the rooms are gone.
     * 통로 양 끝에 문을 배치한다.
     * 방이 사라진(ISGONE) 경우에는 문 대신 통로 타일('#')을 배치한다.
     */
    if (!(rpf->r_flags & ISGONE))
	door(rpf, &spos);
    else
	putpass(&spos);
    if (!(rpt->r_flags & ISGONE))
	door(rpt, &epos);
    else
	putpass(&epos);
    /*
     * Get ready to move...
     * curr를 시작 위치(spos)로 초기화하고 통로를 한 칸씩 그려 나간다.
     */
    curr.x = spos.x;
    curr.y = spos.y;
    while (distance > 0)
    {
	/*
	 * Move to new position
	 * del 방향으로 한 칸 이동한다.
	 */
	curr.x += del.x;
	curr.y += del.y;
	/*
	 * Check if we are at the turn place, if so do the turn
	 * 꺾이는 지점(turn_spot)에 도달했으면 turn_delta 방향으로
	 * turn_distance만큼 통로를 그린 후 계속 진행한다.
	 */
	if (distance == turn_spot)
	    while (turn_distance--)
	    {
		putpass(&curr);
		curr.x += turn_delta.x;
		curr.y += turn_delta.y;
	    }
	/*
	 * Continue digging along
	 * 꺾임이 없는 일반 구간: 현재 위치에 통로 타일을 놓고 distance를 감소시킨다.
	 */
	putpass(&curr);
	distance--;
    }
    curr.x += del.x;
    curr.y += del.y;
    if (!ce(curr, epos))
	msg("warning, connectivity problem on this level");
}

/*
 * putpass:
 *	add a passage character or secret passage here
 *
 * [한국어 설명]
 * 주어진 좌표(cp)에 통로 타일을 배치한다.
 *
 * F_PASS 플래그를 항상 설정한다.
 * 레벨이 높을수록 비밀 통로(secret passage)가 생성될 확률이 높아진다:
 *   - rnd(10)+1 < level && rnd(40)==0 이면 F_REAL 해제 (비밀 통로)
 *   - 그 외에는 p_ch = PASSAGE('#') 로 일반 통로 배치
 */

void
putpass(coord *cp)
{
    PLACE *pp;

    pp = INDEX(cp->y, cp->x);
    pp->p_flags |= F_PASS;
    if (rnd(10) + 1 < level && rnd(40) == 0)
	pp->p_flags &= ~F_REAL;
    else
	pp->p_ch = PASSAGE;
}

/*
 * door:
 *	Add a door or possibly a secret door.  Also enters the door in
 *	the exits array of the room.
 *
 * [한국어 설명]
 * 방(rm)의 출구 배열(r_exit[])에 문의 좌표를 추가하고
 * 해당 위치에 문 타일을 배치한다.
 *
 * ISMAZE 방이면 문을 배치하지 않고 바로 반환한다.
 * 비밀 문(secret door) 생성 조건:
 *   rnd(10)+1 < level && rnd(5)==0 이면 F_REAL 해제 + 벽 문자('|' 또는 '-')로 위장.
 * 그 외에는 p_ch = DOOR('+')로 일반 문 배치.
 */

void
door(struct room *rm, coord *cp)
{
    PLACE *pp;

    rm->r_exit[rm->r_nexits++] = *cp;

    if (rm->r_flags & ISMAZE)
	return;

    pp = INDEX(cp->y, cp->x);
    if (rnd(10) + 1 < level && rnd(5) == 0)
    {
	if (cp->y == rm->r_pos.y || cp->y == rm->r_pos.y + rm->r_max.y - 1)
		pp->p_ch = '-';
	else
		pp->p_ch = '|';
	pp->p_flags &= ~F_REAL;
    }
    else
	pp->p_ch = DOOR;
}

#ifdef MASTER
/*
 * add_pass:
 *	Add the passages to the current window (wizard command)
 *
 * [한국어 설명]
 * 위자드 모드 전용 명령(Ctrl+C). 현재 레벨의 모든 통로와 문을
 * 화면에 강제로 표시한다.
 * F_PASS 또는 DOOR 플래그를 가진 타일을 찾아 F_SEEN 표시 후 화면에 출력한다.
 * 비밀 문은 standout(역상) 으로 강조 표시된다.
 */

void
add_pass()
{
    PLACE *pp;
    int y, x;
    char ch;

    for (y = 1; y < NUMLINES - 1; y++)
	for (x = 0; x < NUMCOLS; x++)
	{
	    pp = INDEX(y, x);
	    if ((pp->p_flags & F_PASS) || pp->p_ch == DOOR ||
		(!(pp->p_flags&F_REAL) && (pp->p_ch == '|' || pp->p_ch == '-')))
	    {
		ch = pp->p_ch;
		if (pp->p_flags & F_PASS)
		    ch = PASSAGE;
		pp->p_flags |= F_SEEN;
		move(y, x);
		if (pp->p_monst != NULL)
		    pp->p_monst->t_oldch = pp->p_ch;
		else if (pp->p_flags & F_REAL)
		    addch(ch);
		else
		{
		    standout();
		    addch((pp->p_flags & F_PASS) ? PASSAGE : DOOR);
		    standend();
		}
	    }
	}
}
#endif

/*
 * passnum:
 *	Assign a number to each passageway
 *
 * [한국어 설명]
 * 레벨의 모든 통로에 고유 번호를 부여하는 함수.
 * 각 방의 출구(r_exit[]) 좌표에서 numpass()를 재귀 호출하여
 * 연결된 통로 타일 전체에 같은 번호(pnum)를 할당한다.
 *
 * pnum     : 현재 할당 중인 통로 번호 (0부터 시작)
 * newpnum  : TRUE이면 다음 numpass() 호출에서 새 번호로 증가
 * passages[]: 번호별 통로 출구 좌표를 저장하는 배열
 */
static int pnum;
static bool newpnum;


void
passnum()
{
    struct room *rp;
    int i;

    pnum = 0;
    newpnum = FALSE;
    for (rp = passages; rp < &passages[MAXPASS]; rp++)
	rp->r_nexits = 0;
    for (rp = rooms; rp < &rooms[MAXROOMS]; rp++)
	for (i = 0; i < rp->r_nexits; i++)
	{
	    newpnum++;
	    numpass(rp->r_exit[i].y, rp->r_exit[i].x);
	}
}

/*
 * numpass:
 *	Number a passageway square and its brethren
 *
 * [한국어 설명]
 * 좌표 (y, x)를 시작점으로 재귀적으로 인접한 통로 타일 전체에
 * 동일한 번호(pnum)를 할당한다.
 *
 * 종료 조건:
 *   - 범위 밖 좌표
 *   - 이미 번호가 부여된 타일(F_PNUM 플래그)
 *   - F_PASS 플래그가 없고 DOOR/비밀문도 아닌 타일
 *
 * DOOR 또는 비밀문이면 passages[pnum].r_exit[]에 좌표를 등록한다.
 * 이후 상하좌우 4방향으로 재귀 호출하여 연결된 통로를 모두 번호 부여한다.
 */

void
numpass(int y, int x)
{
    char *fp;
    struct room *rp;
    char ch;

    if (x >= NUMCOLS || x < 0 || y >= NUMLINES || y <= 0)
	return;
    fp = &flat(y, x);
    if (*fp & F_PNUM)
	return;
    if (newpnum)
    {
	pnum++;
	newpnum = FALSE;
    }
    /*
     * check to see if it is a door or secret door, i.e., a new exit,
     * or a numerable type of place
     * 문(DOOR) 또는 비밀문이면 해당 통로의 출구로 등록한다.
     * F_PASS 타일이 아니면 통로가 아니므로 즉시 반환한다.
     */
    if ((ch = chat(y, x)) == DOOR ||
	(!(*fp & F_REAL) && (ch == '|' || ch == '-')))
    {
	rp = &passages[pnum];
	rp->r_exit[rp->r_nexits].y = y;
	rp->r_exit[rp->r_nexits++].x = x;
    }
    else if (!(*fp & F_PASS))
	return;
    *fp |= pnum;
    /*
     * recurse on the surrounding places
     * 상하좌우 4방향으로 재귀 호출하여 연결된 통로 전체에 번호를 부여한다.
     */
    numpass(y + 1, x);
    numpass(y - 1, x);
    numpass(y, x + 1);
    numpass(y, x - 1);
}
