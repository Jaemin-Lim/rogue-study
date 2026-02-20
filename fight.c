/*
 * All the fighting gets done here
 *
 * @(#)fight.c	4.67 (Berkeley) 09/06/83
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

/*
 * [파일 개요]
 * fight.c - 로그(Rogue) 던전 게임의 전투 시스템을 담당하는 파일.
 *
 * 주요 기능:
 *   - fight()   : 플레이어가 몬스터를 공격하는 메인 전투 함수
 *   - attack()  : 몬스터가 플레이어를 공격하는 함수
 *   - roll_em() : NxM 형식(N개의 M면 주사위)으로 데미지를 계산하는 함수
 *   - swing()   : 공격이 명중하는지 판정하는 함수
 *   - killed()  : 몬스터 사망 처리 및 경험치 획득 함수
 *   - remove_mon(): 화면에서 몬스터를 제거하는 함수
 *
 * 관련 구조체:
 *   - THING (t_stats): 몬스터/아이템 공용 연결 리스트 노드
 *   - pstats        : 플레이어 스탯 (s_hpt=HP, s_str=힘, s_arm=방어구 등급,
 *                     s_lvl=레벨, s_exp=경험치)
 *   - monsters[]    : 26종(A~Z) 몬스터 배열, 인덱스 = 타입문자 - 'A'
 */

#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <ctype.h>
#include "rogue.h"

#define	EQSTR(a, b)	(strcmp(a, b) == 0)

/* 명중 메시지 배열: 플레이어(인덱스 0~3)와 몬스터(인덱스 4~7)의 타격 메시지 */
char *h_names[] = {		/* strings for hitting */
	" scored an excellent hit on ",
	" hit ",
	" have injured ",
	" swing and hit ",
	" scored an excellent hit on ",
	" hit ",
	" has injured ",
	" swings and hits "
};

/* 빗나감 메시지 배열: 플레이어(0~3)와 몬스터(4~7)가 공격을 빗나갈 때 출력 */
char *m_names[] = {		/* strings for missing */
	" miss",
	" swing and miss",
	" barely miss",
	" don't hit",
	" misses",
	" swings and misses",
	" barely misses",
	" doesn't hit",
};

/*
 * 힘(strength) 수치에 따른 명중률 보정값 테이블.
 * 인덱스는 s_str 값이며, 힘이 낮으면 음수(패널티), 높으면 양수(보너스).
 */
static int str_plus[] = {
    -7, -6, -5, -4, -3, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3,
};

/*
 * 힘(strength) 수치에 따른 데미지 보정값 테이블.
 * str_plus와 같은 인덱스 체계; 힘이 강할수록 추가 데미지 증가.
 */
static int add_dam[] = {
    -7, -6, -5, -4, -3, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 3,
    3, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6
};

/*
 * fight:
 *	The player attacks the monster.
 *
 * [한국어 설명]
 * 플레이어가 특정 좌표(mp)에 있는 몬스터를 공격하는 메인 전투 함수.
 *
 * 매개변수:
 *   mp     - 공격 대상 몬스터의 좌표
 *   weap   - 사용 중인 무기 아이템 (NULL이면 맨손)
 *   thrown - TRUE이면 던진 무기에 의한 원거리 공격
 *
 * 반환값: 명중했으면 TRUE, 빗나갔거나 공격이 취소되면 FALSE
 *
 * 주요 처리 흐름:
 *   1. 공격 대상 몬스터(tp)를 좌표로 탐색
 *   2. 전투 중이므로 자동 회복(count/quiet) 정지
 *   3. 제록(xeroc) 몬스터 위장 해제 처리
 *   4. roll_em()으로 명중 판정 및 데미지 적용
 *   5. CANHUH 플래그 보유 시 몬스터를 혼란(confused) 상태로 만듦
 *   6. HP가 0 이하이면 killed()로 처리
 */
int
fight(coord *mp, THING *weap, bool thrown)
{
    register THING *tp;
    register bool did_hit = TRUE;
    register char *mname, ch;

    /*
     * Find the monster we want to fight
     * 공격할 몬스터를 좌표(mp)로 탐색한다.
     */
#ifdef MASTER
    if ((tp = moat(mp->y, mp->x)) == NULL)
	debug("Fight what @ %d,%d", mp->y, mp->x);
#else
    tp = moat(mp->y, mp->x);
#endif
    /*
     * Since we are fighting, things are not quiet so no healing takes
     * place.
     * 전투 중에는 자동 회복이 되지 않도록 count와 quiet를 0으로 초기화한다.
     */
    count = 0;
    quiet = 0;
    runto(mp);
    /*
     * Let him know it was really a xeroc (if it was one).
     * 제록(Xeroc) 몬스터는 다른 몬스터로 위장할 수 있다.
     * 공격 시 위장을 해제하고 플레이어에게 알린다.
     * 환각(ISHALU) 상태라면 랜덤 문자로 표시.
     */
    ch = '\0';
    if (tp->t_type == 'X' && tp->t_disguise != 'X' && !on(player, ISBLIND))
    {
	tp->t_disguise = 'X';
	if (on(player, ISHALU)) {
	    ch = (char)(rnd(26) + 'A');
	    mvaddch(tp->t_pos.y, tp->t_pos.x, ch);
	}
	msg(choose_str("heavy!  That's a nasty critter!",
		       "wait!  That's a xeroc!"));
	if (!thrown)
	    return FALSE;
    }
    mname = set_mname(tp);
    did_hit = FALSE;
    has_hit = (terse && !to_death);
    if (roll_em(&player, tp, weap, thrown))
    {
	did_hit = FALSE;
	if (thrown)
	    thunk(weap, mname, terse);
	else
	    hit((char *) NULL, mname, terse);
	if (on(player, CANHUH))
	{
	    did_hit = TRUE;
	    tp->t_flags |= ISHUH;
	    player.t_flags &= ~CANHUH;
	    endmsg();
	    has_hit = FALSE;
	    msg("your hands stop glowing %s", pick_color("red"));
	}
	if (tp->t_stats.s_hpt <= 0)
	    killed(tp, TRUE);
	else if (did_hit && !on(player, ISBLIND))
	    msg("%s appears confused", mname);
	did_hit = TRUE;
    }
    else
	if (thrown)
	    bounce(weap, mname, terse);
	else
	    miss((char *) NULL, mname, terse);
    return did_hit;
}

/*
 * attack:
 *	The monster attacks the player
 *
 * [한국어 설명]
 * 몬스터(mp)가 플레이어를 공격하는 함수.
 *
 * 매개변수:
 *   mp - 공격하는 몬스터의 THING 포인터
 *
 * 반환값: 몬스터가 제거(사라졌으면) -1, 그 외 0
 *
 * 몬스터별 특수 효과:
 *   'A' (Aquator)    : 방어구 등급(armor class) 감소
 *   'I' (Ice monster): 플레이어 행동 불능 (얼려버림)
 *   'R' (Rattlesnake): 독 공격 → 힘(strength) 감소
 *   'W' (Wraith)     : 경험치 레벨 흡수
 *   'V' (Vampire)    : 최대 HP 흡수
 *   'F' (Flytrap)    : 플레이어를 ISHELD 상태로 고정
 *   'L' (Leprechaun) : 금화 훔치기
 *   'N' (Nymph)      : 마법 아이템 훔치기
 */
int
attack(THING *mp)
{
    register char *mname;
    register int oldhp;

    /*
     * Since this is an attack, stop running and any healing that was
     * going on at the time.
     * 몬스터 공격을 받으면 달리기(running) 상태를 중단하고
     * 자동 회복(count/quiet)도 멈춘다.
     */
    running = FALSE;
    count = 0;
    quiet = 0;
    if (to_death && !on(*mp, ISTARGET))
    {
	to_death = FALSE;
	kamikaze = FALSE;
    }
    if (mp->t_type == 'X' && mp->t_disguise != 'X' && !on(player, ISBLIND))
    {
	mp->t_disguise = 'X';
	if (on(player, ISHALU))
	    mvaddch(mp->t_pos.y, mp->t_pos.x, rnd(26) + 'A');
    }
    mname = set_mname(mp);
    oldhp = pstats.s_hpt;
    if (roll_em(mp, &player, (THING *) NULL, FALSE))
    {
	if (mp->t_type != 'I')
	{
	    if (has_hit)
		addmsg(".  ");
	    hit(mname, (char *) NULL, FALSE);
	}
	else
	    if (has_hit)
		endmsg();
	has_hit = FALSE;
	if (pstats.s_hpt <= 0)
	    death(mp->t_type);	/* Bye bye life ... */
	else if (!kamikaze)
	{
	    oldhp -= pstats.s_hpt;
	    if (oldhp > max_hit)
		max_hit = oldhp;
	    if (pstats.s_hpt <= max_hit)
		to_death = FALSE;
	}
	if (!on(*mp, ISCANC))
	    switch (mp->t_type)
	    {
		case 'A':
		    /*
		     * If an aquator hits, you can lose armor class.
		     * 아쿠아터(Aquator)에게 맞으면 현재 방어구의 방어등급이 하락한다.
		     */
		    rust_armor(cur_armor);
		when 'I':
		    /*
		     * The ice monster freezes you
		     * 아이스 몬스터는 플레이어를 얼려 일정 턴 동안 행동 불능 상태로 만든다.
		     * no_command 카운터가 BORE_LEVEL을 초과하면 사망.
		     */
		    player.t_flags &= ~ISRUN;
		    if (!no_command)
		    {
			addmsg("you are frozen");
			if (!terse)
			    addmsg(" by the %s", mname);
			endmsg();
		    }
		    no_command += rnd(2) + 2;
		    if (no_command > BORE_LEVEL)
			death('h');
		when 'R':
		    /*
		     * Rattlesnakes have poisonous bites
		     * 방울뱀(Rattlesnake)의 독 공격: 독 내성 세이브(VS_POISON)에
		     * 실패하면 플레이어의 힘(strength)이 1 감소한다.
		     * R_SUSTSTR 반지를 끼고 있으면 힘 감소를 막을 수 있다.
		     */
		    if (!save(VS_POISON))
		    {
			if (!ISWEARING(R_SUSTSTR))
			{
			    chg_str(-1);
			    if (!terse)
				msg("you feel a bite in your leg and now feel weaker");
			    else
				msg("a bite has weakened you");
			}
			else if (!to_death)
			{
			    if (!terse)
				msg("a bite momentarily weakens you");
			    else
				msg("bite has no effect");
			}
		    }
		when 'W':
		case 'V':
		    /*
		     * Wraiths might drain energy levels, and Vampires
		     * can steal max_hp
		     * 레이스(Wraith)는 15% 확률로 경험치 레벨을 1 흡수한다.
		     * 뱀파이어(Vampire)는 30% 확률로 최대 HP를 감소시킨다.
		     * 레이스에게 모든 레벨을 잃으면 즉사한다.
		     */
		    if (rnd(100) < (mp->t_type == 'W' ? 15 : 30))
		    {
			register int fewer;

			if (mp->t_type == 'W')
			{
			    if (pstats.s_exp == 0)
				death('W');		/* All levels gone */
			    if (--pstats.s_lvl == 0)
			    {
				pstats.s_exp = 0;
				pstats.s_lvl = 1;
			    }
			    else
				pstats.s_exp = e_levels[pstats.s_lvl-1]+1;
			    fewer = roll(1, 10);
			}
			else
			    fewer = roll(1, 3);
			pstats.s_hpt -= fewer;
			max_hp -= fewer;
			if (pstats.s_hpt <= 0)
			    pstats.s_hpt = 1;
			if (max_hp <= 0)
			    death(mp->t_type);
			msg("you suddenly feel weaker");
		    }
		when 'F':
		    /*
		     * Venus Flytrap stops the poor guy from moving
		     * 비너스 파리지옥(Flytrap)은 플레이어를 ISHELD 상태로 고정한다.
		     * 고정된 동안 매 턴 vf_hit만큼 데미지가 누적된다.
		     */
		    player.t_flags |= ISHELD;
		    sprintf(monsters['F'-'A'].m_stats.s_dmg,"%dx1", ++vf_hit);
		    if (--pstats.s_hpt <= 0)
			death('F');
		when 'L':
		{
		    /*
		     * Leperachaun steals some gold
		     * 레프러콘(Leprechaun)은 플레이어의 금화(purse)를 훔친다.
		     * 마법 세이브(VS_MAGIC)에 실패하면 추가로 4배를 더 잃는다.
		     * 금화를 훔친 후 몬스터는 사라진다.
		     */
		    register int lastpurse;

		    lastpurse = purse;
		    purse -= GOLDCALC;
		    if (!save(VS_MAGIC))
			purse -= GOLDCALC + GOLDCALC + GOLDCALC + GOLDCALC;
		    if (purse < 0)
			purse = 0;
		    remove_mon(&mp->t_pos, mp, FALSE);
                    mp=NULL;
		    if (purse != lastpurse)
			msg("your purse feels lighter");
		}
		when 'N':
		{
		    register THING *obj, *steal;
		    register int nobj;

		    /*
		     * Nymph's steal a magic item, look through the pack
		     * and pick out one we like.
		     * 님프(Nymph)는 플레이어의 인벤토리에서 마법 아이템 하나를 훔친다.
		     * 현재 착용/장착 중인 아이템(방어구, 무기, 반지)은 제외.
		     * 아이템을 훔친 후 몬스터는 사라진다.
		     */
		    steal = NULL;
		    for (nobj = 0, obj = pack; obj != NULL; obj = next(obj))
			if (obj != cur_armor && obj != cur_weapon
			    && obj != cur_ring[LEFT] && obj != cur_ring[RIGHT]
			    && is_magic(obj) && rnd(++nobj) == 0)
				steal = obj;
		    if (steal != NULL)
		    {
			remove_mon(&mp->t_pos, moat(mp->t_pos.y, mp->t_pos.x), FALSE);
                        mp=NULL;
			leave_pack(steal, FALSE, FALSE);
			msg("she stole %s!", inv_name(steal, TRUE));
			discard(steal);
		    }
		}
		otherwise:
		    break;
	    }
    }
    else if (mp->t_type != 'I')
    {
	if (has_hit)
	{
	    addmsg(".  ");
	    has_hit = FALSE;
	}
	if (mp->t_type == 'F')
	{
	    pstats.s_hpt -= vf_hit;
	    if (pstats.s_hpt <= 0)
		death(mp->t_type);	/* Bye bye life ... */
	}
	miss(mname, (char *) NULL, FALSE);
    }
    if (fight_flush && !to_death)
	flush_type();
    count = 0;
    status();
    if (mp == NULL)
        return(-1);
    else
        return(0);
}

/*
 * set_mname:
 *	return the monster name for the given monster
 *
 * [한국어 설명]
 * 주어진 몬스터(tp)의 표시 이름을 반환한다.
 * - 몬스터를 볼 수 없거나 탐지 불가 상태이면 "something"(또는 terse 모드에서 "it") 반환
 * - 환각(ISHALU) 상태이면 화면상의 문자를 기반으로 랜덤 몬스터 이름 반환
 * - 그 외에는 monsters[] 배열에서 실제 몬스터 이름 반환
 * 결과는 항상 "the " 접두사가 붙은 정적 버퍼(tbuf)를 통해 반환된다.
 */
char *
set_mname(THING *tp)
{
    int ch;
    char *mname;
    static char tbuf[MAXSTR] = { 't', 'h', 'e', ' ' };

    if (!see_monst(tp) && !on(player, SEEMONST))
	return (terse ? "it" : "something");
    else if (on(player, ISHALU))
    {
	move(tp->t_pos.y, tp->t_pos.x);
	ch = toascii(inch());
	if (!isupper(ch))
	    ch = rnd(26);
	else
	    ch -= 'A';
	mname = monsters[ch].m_name;
    }
    else
	mname = monsters[tp->t_type - 'A'].m_name;
    strcpy(&tbuf[4], mname);
    return tbuf;
}

/*
 * swing:
 *	Returns true if the swing hits
 *
 * [한국어 설명]
 * 한 번의 공격이 명중하는지 판정한다.
 *
 * 매개변수:
 *   at_lvl - 공격자의 레벨 (높을수록 명중하기 쉬움)
 *   op_arm - 방어자의 방어구 등급 (낮을수록 맞추기 어려움)
 *   wplus  - 무기의 명중 보정값(o_hplus) + 힘 보정값(str_plus)
 *
 * 판정식: rnd(20) + wplus >= (20 - at_lvl) - op_arm
 * D20 굴림 방식으로, 결과가 필요값(need) 이상이면 명중.
 */
int
swing(int at_lvl, int op_arm, int wplus)
{
    int res = rnd(20);
    int need = (20 - at_lvl) - op_arm;

    return (res + wplus >= need);
}

/*
 * roll_em:
 *	Roll several attacks
 *
 * [한국어 설명]
 * 공격자(thatt)가 방어자(thdef)에게 여러 번의 공격을 수행하고 데미지를 적용한다.
 *
 * 매개변수:
 *   thatt - 공격자 THING 포인터 (플레이어 또는 몬스터)
 *   thdef - 방어자 THING 포인터
 *   weap  - 사용 중인 무기 (NULL이면 맨손/기본 공격)
 *   hurl  - TRUE이면 투척 무기 공격
 *
 * 반환값: 하나라도 명중했으면 TRUE
 *
 * 데미지 문자열 형식: "NxM/NxM/..." (N개의 M면 주사위, '/'로 구분된 복수 공격)
 * 각 공격마다 swing()으로 명중 여부를 판정한 후,
 * roll(N, M) + dplus + add_dam[str] 로 최종 데미지를 계산한다.
 * 방어자의 HP(s_hpt)에서 데미지를 차감한다.
 */
bool
roll_em(THING *thatt, THING *thdef, THING *weap, bool hurl)
{
    register struct stats *att, *def;
    register char *cp;
    register int ndice, nsides, def_arm;
    register bool did_hit = FALSE;
    register int hplus;
    register int dplus;
    register int damage;

    att = &thatt->t_stats;
    def = &thdef->t_stats;
    if (weap == NULL)
    {
	cp = att->s_dmg;
	dplus = 0;
	hplus = 0;
    }
    else
    {
	hplus = (weap == NULL ? 0 : weap->o_hplus);
	dplus = (weap == NULL ? 0 : weap->o_dplus);
	if (weap == cur_weapon)
	{
	    if (ISRING(LEFT, R_ADDDAM))
		dplus += cur_ring[LEFT]->o_arm;
	    else if (ISRING(LEFT, R_ADDHIT))
		hplus += cur_ring[LEFT]->o_arm;
	    if (ISRING(RIGHT, R_ADDDAM))
		dplus += cur_ring[RIGHT]->o_arm;
	    else if (ISRING(RIGHT, R_ADDHIT))
		hplus += cur_ring[RIGHT]->o_arm;
	}
	cp = weap->o_damage;
	if (hurl)
	{
	    if ((weap->o_flags&ISMISL) && cur_weapon != NULL &&
	      cur_weapon->o_which == weap->o_launch)
	    {
		cp = weap->o_hurldmg;
		hplus += cur_weapon->o_hplus;
		dplus += cur_weapon->o_dplus;
	    }
	    else if (weap->o_launch < 0)
		cp = weap->o_hurldmg;
	}
    }
    /*
     * If the creature being attacked is not running (alseep or held)
     * then the attacker gets a plus four bonus to hit.
     * 방어자가 움직이지 않는 상태(잠들었거나 고정됨)이면
     * 공격자의 명중 보정값에 +4 보너스를 부여한다.
     */
    if (!on(*thdef, ISRUN))
	hplus += 4;
    def_arm = def->s_arm;
    if (def == &pstats)
    {
	if (cur_armor != NULL)
	    def_arm = cur_armor->o_arm;
	if (ISRING(LEFT, R_PROTECT))
	    def_arm -= cur_ring[LEFT]->o_arm;
	if (ISRING(RIGHT, R_PROTECT))
	    def_arm -= cur_ring[RIGHT]->o_arm;
    }
    while(cp != NULL && *cp != '\0')
    {
	ndice = atoi(cp);
	if ((cp = strchr(cp, 'x')) == NULL)
	    break;
	nsides = atoi(++cp);
	if (swing(att->s_lvl, def_arm, hplus + str_plus[att->s_str]))
	{
	    int proll;

	    proll = roll(ndice, nsides);
#ifdef MASTER
	    if (ndice + nsides > 0 && proll <= 0)
		debug("Damage for %dx%d came out %d, dplus = %d, add_dam = %d, def_arm = %d", ndice, nsides, proll, dplus, add_dam[att->s_str], def_arm);
#endif
	    damage = dplus + proll + add_dam[att->s_str];
	    def->s_hpt -= max(0, damage);
	    did_hit = TRUE;
	}
	if ((cp = strchr(cp, '/')) == NULL)
	    break;
	cp++;
    }
    return did_hit;
}

/*
 * prname:
 *	The print name of a combatant
 *
 * [한국어 설명]
 * 전투 메시지에 사용할 공격자/방어자의 이름을 반환한다.
 * mname이 NULL이면 "you"(플레이어), 그 외엔 mname을 그대로 사용.
 * upper가 TRUE이면 첫 글자를 대문자로 변환한다.
 */
char *
prname(char *mname, bool upper)
{
    static char tbuf[MAXSTR];

    *tbuf = '\0';
    if (mname == 0)
	strcpy(tbuf, "you"); 
    else
	strcpy(tbuf, mname);
    if (upper)
	*tbuf = (char) toupper(*tbuf);
    return tbuf;
}

/*
 * thunk:
 *	A missile hits a monster
 *
 * [한국어 설명]
 * 투척 무기(미사일)가 몬스터에게 명중했을 때 메시지를 출력한다.
 * to_death(죽을 때까지 싸우기) 모드에서는 메시지를 건너뛴다.
 * weap이 WEAPON 타입이면 무기 이름을, 아니면 "you hit"을 출력.
 */
void
thunk(THING *weap, char *mname, bool noend)
{
    if (to_death)
	return;
    if (weap->o_type == WEAPON)
	addmsg("the %s hits ", weap_info[weap->o_which].oi_name);
    else
	addmsg("you hit ");
    addmsg("%s", mname);
    if (!noend)
	endmsg();
}

/*
 * hit:
 *	Print a message to indicate a succesful hit
 *
 * [한국어 설명]
 * 근접 공격 명중 시 메시지를 출력한다.
 * er(공격자)가 NULL이면 플레이어, ee(방어자)가 NULL이면 플레이어.
 * terse 모드에서는 " hit", 일반 모드에서는 h_names[]의 랜덤 메시지를 사용.
 * noend가 TRUE이면 메시지를 아직 마무리(endmsg)하지 않고 이어쓴다.
 */

void
hit(char *er, char *ee, bool noend)
{
    int i;
    char *s;
    extern char *h_names[];

    if (to_death)
	return;
    addmsg(prname(er, TRUE));
    if (terse)
	s = " hit";
    else
    {
	i = rnd(4);
	if (er != NULL)
	    i += 4;
	s = h_names[i];
    }
    addmsg(s);
    if (!terse)
	addmsg(prname(ee, FALSE));
    if (!noend)
	endmsg();
}

/*
 * miss:
 *	Print a message to indicate a poor swing
 *
 * [한국어 설명]
 * 공격이 빗나갔을 때 메시지를 출력한다.
 * terse 모드에서는 고정 메시지, 일반 모드에서는 m_names[]의 랜덤 메시지.
 * er(공격자)가 NULL이면 플레이어, er가 있으면 인덱스 +4 (몬스터용 메시지).
 */
void
miss(char *er, char *ee, bool noend)
{
    int i;
    extern char *m_names[];

    if (to_death)
	return;
    addmsg(prname(er, TRUE));
    if (terse)
	i = 0;
    else
	i = rnd(4);
    if (er != NULL)
	i += 4;
    addmsg(m_names[i]);
    if (!terse)
	addmsg(" %s", prname(ee, FALSE));
    if (!noend)
	endmsg();
}

/*
 * bounce:
 *	A missile misses a monster
 *
 * [한국어 설명]
 * 투척 무기(미사일)가 몬스터를 빗나갔을 때 메시지를 출력한다.
 * to_death 모드에서는 메시지를 건너뛴다.
 */
void
bounce(THING *weap, char *mname, bool noend)
{
    if (to_death)
	return;
    if (weap->o_type == WEAPON)
	addmsg("the %s misses ", weap_info[weap->o_which].oi_name);
    else
	addmsg("you missed ");
    addmsg(mname);
    if (!noend)
	endmsg();
}

/*
 * remove_mon:
 *	Remove a monster from the screen
 *
 * [한국어 설명]
 * 몬스터를 화면과 게임 상태에서 완전히 제거한다.
 *
 * 매개변수:
 *   mp      - 몬스터의 현재 좌표
 *   tp      - 제거할 몬스터의 THING 포인터
 *   waskill - TRUE이면 전투로 처치된 경우 (소지품을 바닥에 떨어뜨림)
 *             FALSE이면 도주/소환취소 등 (소지품 삭제)
 *
 * 처리 순서:
 *   1. 몬스터 소지품을 처리 (waskill이면 fall(), 아니면 discard())
 *   2. moat() 배열에서 해당 좌표의 몬스터 참조 제거
 *   3. 원래 타일 문자(t_oldch)를 화면에 복원
 *   4. mlist 연결 리스트에서 제거 및 메모리 해제
 *   5. ISTARGET이었다면 to_death/kamikaze 플래그 초기화
 */
void
remove_mon(coord *mp, THING *tp, bool waskill)
{
    register THING *obj, *nexti;

    for (obj = tp->t_pack; obj != NULL; obj = nexti)
    {
	nexti = next(obj);
	obj->o_pos = tp->t_pos;
	detach(tp->t_pack, obj);
	if (waskill)
	    fall(obj, FALSE);
	else
	    discard(obj);
    }
    moat(mp->y, mp->x) = NULL;
    mvaddch(mp->y, mp->x, tp->t_oldch);
    detach(mlist, tp);
    if (on(*tp, ISTARGET))
    {
	kamikaze = FALSE;
	to_death = FALSE;
	if (fight_flush)
	    flush_type();
    }
    discard(tp);
}

/*
 * killed:
 *	Called to put a monster to death
 *
 * [한국어 설명]
 * 몬스터 처치 시 호출된다. 경험치 획득, 특수 처리, 화면 제거를 수행한다.
 *
 * 매개변수:
 *   tp - 처치된 몬스터의 THING 포인터
 *   pr - TRUE이면 "defeated" 메시지를 화면에 출력
 *
 * 처리 순서:
 *   1. 플레이어 경험치(pstats.s_exp)에 몬스터의 s_exp를 더함
 *   2. 몬스터 타입별 특수 처리 ('F': ISHELD 해제, 'L': 금화 생성)
 *   3. remove_mon()으로 몬스터 제거
 *   4. pr이 TRUE이면 처치 메시지 출력
 *   5. check_level()로 레벨 업 여부 확인
 */
void
killed(THING *tp, bool pr)
{
    char *mname;

    pstats.s_exp += tp->t_stats.s_exp;

    /*
     * If the monster was a venus flytrap, un-hold him
     * 비너스 파리지옥(F)을 처치하면 플레이어의 ISHELD 상태를 해제하고
     * vf_hit을 0으로 초기화하여 누적 데미지를 리셋한다.
     * 레프러콘(L)을 처치하면 일정 조건 하에 금화 아이템을 생성한다.
     */
    switch (tp->t_type)
    {
	case 'F':
	    player.t_flags &= ~ISHELD;
	    vf_hit = 0;
	    strcpy(monsters['F'-'A'].m_stats.s_dmg, "000x0");
	when 'L':
	{
	    THING *gold;

	    if (fallpos(&tp->t_pos, &tp->t_room->r_gold) && level >= max_level)
	    {
		gold = new_item();
		gold->o_type = GOLD;
		gold->o_goldval = GOLDCALC;
		if (save(VS_MAGIC))
		    gold->o_goldval += GOLDCALC + GOLDCALC
				     + GOLDCALC + GOLDCALC;
		attach(tp->t_pack, gold);
	    }
	}
    }
    /*
     * Get rid of the monster.
     * 몬스터를 화면과 연결 리스트에서 완전히 제거한다.
     */
    mname = set_mname(tp);
    remove_mon(&tp->t_pos, tp, TRUE);
    if (pr)
    {
	if (has_hit)
	{
	    addmsg(".  Defeated ");
	    has_hit = FALSE;
	}
	else
	{
	    if (!terse)
		addmsg("you have ");
	    addmsg("defeated ");
	}
	msg(mname);
    }
    /*
     * Do adjustments if he went up a level
     * check_level()로 경험치에 따른 레벨 업 여부를 확인하고
     * 필요하면 HP 최댓값과 스탯을 조정한다.
     */
    check_level();
    if (fight_flush)
	flush_type();
}
