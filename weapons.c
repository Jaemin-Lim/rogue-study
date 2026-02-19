/*
 * Functions for dealing with problems brought about by weapons
 * 무기 관련 처리 함수들을 담은 파일.
 * 투척, 무기 장착, 낙하, 초기화 등을 처리한다.
 *
 * @(#)weapons.c	4.34 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <curses.h>
#include <string.h>
#include <ctype.h>
#include "rogue.h"

#define NO_WEAPON -1  /* 발사체 무기가 필요 없음을 나타내는 상수 */

int group = 2;  /* 무기 그룹 번호 (동일 그룹의 아이템은 배낭에서 묶임) */

/* 무기 초기화 데이터 테이블 */
static struct init_weaps {
    char *iw_dam;	/* Damage when wielded */
                    /* 직접 공격 시 피해 (형식: NxM = N개의 M면 주사위) */
    char *iw_hrl;	/* Damage when thrown */
                    /* 투척 시 피해 */
    char iw_launch;	/* Launching weapon */
                    /* 이 무기를 발사하는 발사체 무기 (예: 화살은 활 필요) */
    int iw_flags;	/* Miscellaneous flags */
                    /* ISMANY: 여러 개로 묶임, ISMISL: 투척 가능 */
} init_dam[MAXWEAPONS] = {
    { "2x4",	"1x3",	NO_WEAPON,	0,		},	/* Mace */
    { "3x4",	"1x2",	NO_WEAPON,	0,		},	/* Long sword */
    { "1x1",	"1x1",	NO_WEAPON,	0,		},	/* Bow */
    { "1x1",	"2x3",	BOW,		ISMANY|ISMISL,	},	/* Arrow */
    { "1x6",	"1x4",	NO_WEAPON,	ISMISL|ISMISL,	},	/* Dagger */
    { "4x4",	"1x2",	NO_WEAPON,	0,		},	/* 2h sword */
    { "1x1",	"1x3",	NO_WEAPON,	ISMANY|ISMISL,	},	/* Dart */
    { "1x2",	"2x4",	NO_WEAPON,	ISMANY|ISMISL,	},	/* Shuriken */
    { "2x3",	"1x6",	NO_WEAPON,	ISMISL,		},	/* Spear */
};

/*
 * missile:
 *	Fire a missile in a given direction
 *	지정한 방향으로 투척 무기를 발사하는 함수.
 *	배낭에서 투척할 무기를 선택하고, do_motion()으로 비행 경로를 처리한다.
 *	벽/문에 맞거나 몬스터를 놓치면 fall()로 아이템을 바닥에 떨어뜨린다.
 */

void
missile(int ydelta, int xdelta)
{
    THING *obj;  /* 투척할 아이템 포인터 */

    /*
     * Get which thing we are hurling
     * 배낭에서 투척할 무기 선택
     */
    if ((obj = get_item("throw", WEAPON)) == NULL)
	return;
    if (!dropcheck(obj) || is_current(obj))  /* 저주받거나 현재 사용 중이면 투척 불가 */
	return;
    obj = leave_pack(obj, TRUE, FALSE);  /* 배낭에서 아이템 꺼내기 */
    do_motion(obj, ydelta, xdelta);  /* 투척 비행 처리 */
    /*
     * AHA! Here it has hit something.  If it is a wall or a door,
     * or if it misses (combat) the monster, put it on the floor
     * 벽/문에 맞거나 몬스터를 공격해서 놓치면 바닥에 떨어뜨림
     */
    if (moat(obj->o_pos.y, obj->o_pos.x) == NULL ||
	!hit_monster(unc(obj->o_pos), obj))
	    fall(obj, TRUE);  /* 아이템 낙하 처리 */
}

/*
 * do_motion:
 *	Do the actual motion on the screen done by an object traveling
 *	across the room
 *	투척된 물체가 화면을 가로질러 이동하는 것을 처리하는 함수.
 *	각 위치에서 이전 위치를 지우고 새 위치에 아이템 문자를 표시한다.
 *	벽, 문, 또는 이동 불가 위치에 도달하면 멈춘다.
 */

void
do_motion(THING *obj, int ydelta, int xdelta)
{
    int ch;  /* 현재 위치의 문자 */

    /*
     * Come fly with us ...
     * 영웅의 현재 위치에서 비행 시작
     */
    obj->o_pos = hero;
    for (;;)
    {
	/*
	 * Erase the old one
	 * 이전 위치의 아이템 문자 지우기
	 */
	if (!ce(obj->o_pos, hero) && cansee(unc(obj->o_pos)) && !terse)
	{
	    ch = chat(obj->o_pos.y, obj->o_pos.x);
	    if (ch == FLOOR && !show_floor())
		ch = ' ';
	    mvaddch(obj->o_pos.y, obj->o_pos.x, ch);  /* 배경 문자로 복원 */
	}
	/*
	 * Get the new position
	 * 새 위치 계산
	 */
	obj->o_pos.y += ydelta;
	obj->o_pos.x += xdelta;
	if (step_ok(ch = winat(obj->o_pos.y, obj->o_pos.x)) && ch != DOOR)
	{
	    /*
	     * It hasn't hit anything yet, so display it
	     * If it alright.
	     * 아직 아무것도 맞지 않았으므로 화면에 표시
	     */
	    if (cansee(unc(obj->o_pos)) && !terse)
	    {
		mvaddch(obj->o_pos.y, obj->o_pos.x, obj->o_type);  /* 아이템 문자 표시 */
		refresh();
	    }
	    continue;
	}
	break;  /* 이동 불가 위치에 도달하면 종료 */
    }
}

/*
 * fall:
 *	Drop an item someplace around here.
 *	아이템을 지정 위치 주변의 임의 위치에 떨어뜨리는 함수.
 *	주변에 빈 공간이 없으면 아이템이 사라지고 메시지를 출력한다.
 *	pr: TRUE이면 "vanishes" 메시지 출력
 */

void
fall(THING *obj, bool pr)
{
    PLACE *pp;        /* 낙하 위치의 맵 데이터 */
    static coord fpos; /* 아이템이 떨어질 위치 */

    if (fallpos(&obj->o_pos, &fpos))  /* 주변에 빈 위치가 있으면 */
    {
	pp = INDEX(fpos.y, fpos.x);
	pp->p_ch = (char) obj->o_type;   /* 맵 데이터에 아이템 문자 설정 */
	obj->o_pos = fpos;
	if (cansee(fpos.y, fpos.x))  /* 볼 수 있는 위치이면 화면에 표시 */
	{
	    if (pp->p_monst != NULL)   /* 몬스터가 있으면 몬스터의 t_oldch 설정 */
		pp->p_monst->t_oldch = (char) obj->o_type;
	    else
		mvaddch(fpos.y, fpos.x, obj->o_type);  /* 아이템 문자 표시 */
	}
	attach(lvl_obj, obj);  /* 레벨 아이템 리스트에 추가 */
	return;
    }
    /* 주변에 빈 공간이 없으면 아이템 사라짐 */
    if (pr)
    {
	if (has_hit)
	{
	    endmsg();
	    has_hit = FALSE;
	}
	msg("the %s vanishes as it hits the ground",
	    weap_info[obj->o_which].oi_name);
    }
    discard(obj);  /* 아이템 메모리 해제 */
}

/*
 * init_weapon:
 *	Set up the initial goodies for a weapon
 *	무기 THING 구조체를 초기화하는 함수.
 *	init_dam 테이블에서 해당 무기 유형의 데이터를 가져와 설정한다.
 *	다거(단검)와 ISMANY 무기(화살, 다트, 수리검)는 여러 개로 생성된다.
 */

void
init_weapon(THING *weap, int which)
{
    struct init_weaps *iwp;  /* 무기 초기화 데이터 포인터 */

    weap->o_type = WEAPON;   /* 아이템 유형: 무기 */
    weap->o_which = which;   /* 무기 종류 (MACE, SWORD 등) */
    iwp = &init_dam[which];
    strncpy(weap->o_damage, iwp->iw_dam, sizeof(weap->o_damage));   /* 직접 공격 피해 */
    strncpy(weap->o_hurldmg,iwp->iw_hrl, sizeof(weap->o_hurldmg));  /* 투척 피해 */
    weap->o_launch = iwp->iw_launch;  /* 필요한 발사 무기 */
    weap->o_flags = iwp->iw_flags;    /* 아이템 플래그 */
    weap->o_hplus = 0;  /* 명중 보너스 초기화 */
    weap->o_dplus = 0;  /* 피해 보너스 초기화 */
    if (which == DAGGER)  /* 단검은 2~5개 생성 */
    {
	weap->o_count = rnd(4) + 2;
	weap->o_group = group++;  /* 같은 그룹 번호 (배낭에서 묶임) */
    }
    else if (weap->o_flags & ISMANY)  /* 화살, 다트, 수리검 등은 8~15개 생성 */
    {
	weap->o_count = rnd(8) + 8;
	weap->o_group = group++;
    }
    else  /* 일반 무기는 1개 */
    {
	weap->o_count = 1;
	weap->o_group = 0;
    }
}

/*
 * hit_monster:
 *	Does the missile hit the monster?
 *	투척된 무기가 특정 위치의 몬스터를 맞추는지 판별하는 함수.
 *	fight() 함수를 호출하여 전투를 처리한다 (투척 공격).
 */
int
hit_monster(int y, int x, THING *obj)
{
    static coord mp;  /* 몬스터 위치 좌표 */

    mp.y = y;
    mp.x = x;
    return fight(&mp, obj, TRUE);  /* 투척 공격 (fight.c 참조) */
}

/*
 * num:
 *	Figure out the plus number for armor/weapons
 *	갑옷/무기의 보정 수치를 문자열로 반환하는 함수.
 *	예: n1=+1, n2=+2 이면 "+1,+2" 반환 (무기)
 *	n1이 음수이면 "-" 기호 사용
 */
char *
num(int n1, int n2, char type)
{
    static char numbuf[10];

    sprintf(numbuf, n1 < 0 ? "%d" : "+%d", n1);  /* 첫 번째 수치 */
    if (type == WEAPON)  /* 무기이면 두 번째 수치(피해 보너스)도 추가 */
	sprintf(&numbuf[strlen(numbuf)], n2 < 0 ? ",%d" : ",+%d", n2);
    return numbuf;
}

/*
 * wield:
 *	Pull out a certain weapon
 *	배낭에서 무기를 꺼내 장착하는 함수.
 *	현재 장착된 무기가 저주받았다면 변경 불가.
 *	갑옷은 장착할 수 없다.
 */

void
wield()
{
    THING *obj, *oweapon;  /* 새 무기, 이전 무기 포인터 */
    char *sp;              /* 무기 이름 문자열 */

    oweapon = cur_weapon;  /* 이전 무기 저장 */
    if (!dropcheck(cur_weapon))  /* 현재 무기가 저주받았으면 */
    {
	cur_weapon = oweapon;  /* 이전 무기로 복원 */
	return;
    }
    cur_weapon = oweapon;
    if ((obj = get_item("wield", WEAPON)) == NULL)  /* 배낭에서 무기 선택 */
    {
bad:
	after = FALSE;
	return;
    }

    if (obj->o_type == ARMOR)  /* 갑옷은 장착 불가 */
    {
	msg("you can't wield armor");
	goto bad;
    }
    if (is_current(obj))  /* 이미 사용 중인 아이템 */
        goto bad;

    sp = inv_name(obj, TRUE);
    cur_weapon = obj;  /* 새 무기 장착 */
    if (!terse)
	addmsg("you are now ");
    msg("wielding %s (%c)", sp, obj->o_packch);
}

/*
 * fallpos:
 *	Pick a random position around the give (y, x) coordinates
 *	지정 좌표 주변(3x3 범위)에서 아이템이 떨어질 수 있는 임의 위치를 선택하는 함수.
 *	영웅 위치는 제외. 바닥('.')이나 통로('#')만 선택.
 *	반환값: 유효한 위치를 찾았으면 TRUE, 없으면 FALSE
 */
bool
fallpos(coord *pos, coord *newpos)
{
    int y, x, cnt, ch;

    cnt = 0;  /* 유효한 위치 후보 수 */
    for (y = pos->y - 1; y <= pos->y + 1; y++)
	for (x = pos->x - 1; x <= pos->x + 1; x++)
	{
	    /*
	     * check to make certain the spot is empty, if it is,
	     * put the object there, set it in the level list
	     * and re-draw the room if he can see it
	     * 위치가 빈 곳인지 확인 (바닥 또는 통로)
	     */
	    if (y == hero.y && x == hero.x)  /* 영웅 위치 제외 */
		continue;
	    if (((ch = chat(y, x)) == FLOOR || ch == PASSAGE)
					&& rnd(++cnt) == 0)  /* 균등 확률로 선택 */
	    {
		newpos->y = y;
		newpos->x = x;
	    }
	}
    return (bool)(cnt != 0);  /* 유효한 위치가 있었으면 TRUE */
}

#include <curses.h>
#include <string.h>
#include <ctype.h>
#include "rogue.h"

#define NO_WEAPON -1

int group = 2;

static struct init_weaps {
    char *iw_dam;	/* Damage when wielded */
    char *iw_hrl;	/* Damage when thrown */
    char iw_launch;	/* Launching weapon */
    int iw_flags;	/* Miscellaneous flags */
} init_dam[MAXWEAPONS] = {
    { "2x4",	"1x3",	NO_WEAPON,	0,		},	/* Mace */
    { "3x4",	"1x2",	NO_WEAPON,	0,		},	/* Long sword */
    { "1x1",	"1x1",	NO_WEAPON,	0,		},	/* Bow */
    { "1x1",	"2x3",	BOW,		ISMANY|ISMISL,	},	/* Arrow */
    { "1x6",	"1x4",	NO_WEAPON,	ISMISL|ISMISL,	},	/* Dagger */
    { "4x4",	"1x2",	NO_WEAPON,	0,		},	/* 2h sword */
    { "1x1",	"1x3",	NO_WEAPON,	ISMANY|ISMISL,	},	/* Dart */
    { "1x2",	"2x4",	NO_WEAPON,	ISMANY|ISMISL,	},	/* Shuriken */
    { "2x3",	"1x6",	NO_WEAPON,	ISMISL,		},	/* Spear */
};

/*
 * missile:
 *	Fire a missile in a given direction
 */

void
missile(int ydelta, int xdelta)
{
    THING *obj;

    /*
     * Get which thing we are hurling
     */
    if ((obj = get_item("throw", WEAPON)) == NULL)
	return;
    if (!dropcheck(obj) || is_current(obj))
	return;
    obj = leave_pack(obj, TRUE, FALSE);
    do_motion(obj, ydelta, xdelta);
    /*
     * AHA! Here it has hit something.  If it is a wall or a door,
     * or if it misses (combat) the monster, put it on the floor
     */
    if (moat(obj->o_pos.y, obj->o_pos.x) == NULL ||
	!hit_monster(unc(obj->o_pos), obj))
	    fall(obj, TRUE);
}

/*
 * do_motion:
 *	Do the actual motion on the screen done by an object traveling
 *	across the room
 */

void
do_motion(THING *obj, int ydelta, int xdelta)
{
    int ch;

    /*
     * Come fly with us ...
     */
    obj->o_pos = hero;
    for (;;)
    {
	/*
	 * Erase the old one
	 */
	if (!ce(obj->o_pos, hero) && cansee(unc(obj->o_pos)) && !terse)
	{
	    ch = chat(obj->o_pos.y, obj->o_pos.x);
	    if (ch == FLOOR && !show_floor())
		ch = ' ';
	    mvaddch(obj->o_pos.y, obj->o_pos.x, ch);
	}
	/*
	 * Get the new position
	 */
	obj->o_pos.y += ydelta;
	obj->o_pos.x += xdelta;
	if (step_ok(ch = winat(obj->o_pos.y, obj->o_pos.x)) && ch != DOOR)
	{
	    /*
	     * It hasn't hit anything yet, so display it
	     * If it alright.
	     */
	    if (cansee(unc(obj->o_pos)) && !terse)
	    {
		mvaddch(obj->o_pos.y, obj->o_pos.x, obj->o_type);
		refresh();
	    }
	    continue;
	}
	break;
    }
}

/*
 * fall:
 *	Drop an item someplace around here.
 */

void
fall(THING *obj, bool pr)
{
    PLACE *pp;
    static coord fpos;

    if (fallpos(&obj->o_pos, &fpos))
    {
	pp = INDEX(fpos.y, fpos.x);
	pp->p_ch = (char) obj->o_type;
	obj->o_pos = fpos;
	if (cansee(fpos.y, fpos.x))
	{
	    if (pp->p_monst != NULL)
		pp->p_monst->t_oldch = (char) obj->o_type;
	    else
		mvaddch(fpos.y, fpos.x, obj->o_type);
	}
	attach(lvl_obj, obj);
	return;
    }
    if (pr)
    {
	if (has_hit)
	{
	    endmsg();
	    has_hit = FALSE;
	}
	msg("the %s vanishes as it hits the ground",
	    weap_info[obj->o_which].oi_name);
    }
    discard(obj);
}

/*
 * init_weapon:
 *	Set up the initial goodies for a weapon
 */

void
init_weapon(THING *weap, int which)
{
    struct init_weaps *iwp;

    weap->o_type = WEAPON;
    weap->o_which = which;
    iwp = &init_dam[which];
    strncpy(weap->o_damage, iwp->iw_dam, sizeof(weap->o_damage));
    strncpy(weap->o_hurldmg,iwp->iw_hrl, sizeof(weap->o_hurldmg));
    weap->o_launch = iwp->iw_launch;
    weap->o_flags = iwp->iw_flags;
    weap->o_hplus = 0;
    weap->o_dplus = 0;
    if (which == DAGGER)
    {
	weap->o_count = rnd(4) + 2;
	weap->o_group = group++;
    }
    else if (weap->o_flags & ISMANY)
    {
	weap->o_count = rnd(8) + 8;
	weap->o_group = group++;
    }
    else
    {
	weap->o_count = 1;
	weap->o_group = 0;
    }
}

/*
 * hit_monster:
 *	Does the missile hit the monster?
 */
int
hit_monster(int y, int x, THING *obj)
{
    static coord mp;

    mp.y = y;
    mp.x = x;
    return fight(&mp, obj, TRUE);
}

/*
 * num:
 *	Figure out the plus number for armor/weapons
 */
char *
num(int n1, int n2, char type)
{
    static char numbuf[10];

    sprintf(numbuf, n1 < 0 ? "%d" : "+%d", n1);
    if (type == WEAPON)
	sprintf(&numbuf[strlen(numbuf)], n2 < 0 ? ",%d" : ",+%d", n2);
    return numbuf;
}

/*
 * wield:
 *	Pull out a certain weapon
 */

void
wield()
{
    THING *obj, *oweapon;
    char *sp;

    oweapon = cur_weapon;
    if (!dropcheck(cur_weapon))
    {
	cur_weapon = oweapon;
	return;
    }
    cur_weapon = oweapon;
    if ((obj = get_item("wield", WEAPON)) == NULL)
    {
bad:
	after = FALSE;
	return;
    }

    if (obj->o_type == ARMOR)
    {
	msg("you can't wield armor");
	goto bad;
    }
    if (is_current(obj))
        goto bad;

    sp = inv_name(obj, TRUE);
    cur_weapon = obj;
    if (!terse)
	addmsg("you are now ");
    msg("wielding %s (%c)", sp, obj->o_packch);
}

/*
 * fallpos:
 *	Pick a random position around the give (y, x) coordinates
 */
bool
fallpos(coord *pos, coord *newpos)
{
    int y, x, cnt, ch;

    cnt = 0;
    for (y = pos->y - 1; y <= pos->y + 1; y++)
	for (x = pos->x - 1; x <= pos->x + 1; x++)
	{
	    /*
	     * check to make certain the spot is empty, if it is,
	     * put the object there, set it in the level list
	     * and re-draw the room if he can see it
	     */
	    if (y == hero.y && x == hero.x)
		continue;
	    if (((ch = chat(y, x)) == FLOOR || ch == PASSAGE)
					&& rnd(++cnt) == 0)
	    {
		newpos->y = y;
		newpos->x = x;
	    }
	}
    return (bool)(cnt != 0);
}
