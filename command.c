/*
 * Read and execute the user commands
 *
 * @(#)command.c	4.73 (Berkeley) 08/06/83
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

/*
 * [파일 개요]
 * command.c - 로그(Rogue) 게임의 사용자 명령 처리 및 메인 게임 루프를 담당하는 파일.
 *
 * 주요 기능:
 *   - command()     : 플레이어 입력을 읽고 명령을 실행하는 메인 루프
 *   - illcom()      : 잘못된 명령 처리
 *   - search()      : 인접 타일에서 숨겨진 문/함정/통로를 탐색
 *   - help()        : 명령 도움말 출력
 *   - identify()    : 화면 기호의 의미 설명
 *   - d_level()     : 다음 층으로 내려가기 ('>')
 *   - u_level()     : 이전 층으로 올라가기 ('<')
 *   - levit_check() : 부유(levitation) 상태 확인
 *   - call()        : 포션/두루마리/반지에 이름 붙이기
 *   - current()     : 현재 착용/장착 중인 아이템 표시
 *
 * 키 바인딩 개요 (command() 내 switch):
 *   h/j/k/l/y/u/b/n        : 8방향 이동 (hjkl = 좌하상우, yubn = 대각선)
 *   H/J/K/L/Y/U/B/N        : 8방향 달리기 (장애물/몬스터까지 자동 이동)
 *   Ctrl+방향키             : 장애물 전까지 달리기 (문 앞에서 정지)
 *   f/F                    : 방향 지정 후 몬스터 공격 (F는 죽을 때까지)
 *   t                      : 방향 지정 후 미사일 투척
 *   a                      : 마지막 명령 재실행
 *   q/r/e/w/W/T/P/R        : 포션 마시기/두루마리 읽기/음식 먹기/무기·방어구·반지 착용 관련
 *   i/I                    : 인벤토리 전체/선택 표시
 *   d/,                    : 아이템 버리기/줍기
 *   s                      : 비밀 통로·문·함정 탐색
 *   z                      : 지팡이/막대기 사용
 *   o/c/D/v/S              : 옵션/이름붙이기/발견 목록/버전/저장
 *   >/< : 계단 이동
 *   ?// : 도움말/기호 설명
 *   .                      : 휴식 (한 턴 대기)
 *   ^                      : 방향 지정 후 함정 확인
 *   @                      : 플레이어 스탯 표시
 *   ESC                    : 달리기/연속 명령 취소
 */

#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <ctype.h>
#include "rogue.h"

/*
 * command:
 *	Process the user commands
 *
 * [한국어 설명]
 * 플레이어 명령을 읽고 실행하는 메인 게임 루프 함수.
 *
 * 처리 흐름:
 *   1. ISHASTE(빠름) 상태이면 ntimes를 2로 설정하여 플레이어가 두 번 행동
 *   2. BEFORE 시점의 데몬(daemon)과 퓨즈(fuse) 효과를 실행
 *   3. ntimes만큼 반복하여:
 *      a. 화면 갱신 (look, status, refresh)
 *      b. 키 입력 읽기 (달리기/count 모드이면 방향키 자동 반복)
 *      c. no_command > 0이면 명령 불가 상태 (얼거나 고정된 경우)
 *      d. 숫자 접두사로 count(반복 횟수) 처리
 *      e. switch(ch)로 명령 디스패치
 *      f. take != 0이면 아이템 자동 습득
 *   4. AFTER 시점의 데몬/퓨즈 실행
 *   5. R_SEARCH/R_TELEPORT 반지 효과 처리
 *
 * 주요 상태 변수:
 *   ntimes    - 이번 턴에 실행할 명령 횟수 (기본 1, 빠름 상태 2)
 *   count     - 명령 반복 횟수 접두사 (예: 5h = 왼쪽 5칸)
 *   running   - 달리기 모드 플래그
 *   runch     - 달리기 중인 방향 문자
 *   countch   - count 모드에서 반복할 명령 문자
 *   direction - Ctrl+방향키로 설정된 달리기 방향
 *   after     - FALSE이면 이번 명령에 소요된 턴을 차감하지 않음
 */
void
command()
{
    register char ch;
    register int ntimes = 1;			/* Number of player moves */
    /* ntimes: 이번 턴에 실행할 명령 횟수 (ISHASTE 상태이면 2) */
    char *fp;
    THING *mp;
    static char countch, direction, newcount = FALSE;
    /* countch: count 모드에서 반복할 명령 문자 */
    /* direction: Ctrl+방향키로 지정된 달리기 방향 */
    /* newcount: 이번 루프에서 새로운 count 접두사가 입력되었는지 여부 */

    if (on(player, ISHASTE))
	ntimes++;
    /*
     * Let the daemons start up
     * BEFORE 시점: 플레이어 행동 전에 실행되어야 할 데몬/퓨즈 효과를 처리한다.
     * (예: 독 효과, 공복, 환각 등 주기적인 상태 효과)
     */
    do_daemons(BEFORE);
    do_fuses(BEFORE);
    while (ntimes--)
    {
	again = FALSE;
	if (has_hit)
	{
	    endmsg();
	    has_hit = FALSE;
	}
	/*
	 * these are illegal things for the player to be, so if any are
	 * set, someone's been poking in memeory
	 * 플레이어가 가져선 안 되는 몬스터 전용 플래그가 설정되어 있으면
	 * 메모리 오염으로 판단하여 즉시 종료한다.
	 */
	if (on(player, ISSLOW|ISGREED|ISINVIS|ISREGEN|ISTARGET))
	    exit(1);

	look(TRUE);
	if (!running)
	    door_stop = FALSE;
	status();
	lastscore = purse;
	move(hero.y, hero.x);
	if (!((running || count) && jump))
	    refresh();			/* Draw screen */
	take = 0;
	after = TRUE;
	/*
	 * Read command or continue run
	 * 명령 읽기: 달리기(running) 또는 to_death 모드이면 runch 방향 반복,
	 * count 모드이면 countch 명령 반복, 그 외에는 readchar()로 입력 받음.
	 * no_command > 0이면 행동 불능 상태이므로 '.'(휴식)으로 대체한다.
	 */
#ifdef MASTER
	if (wizard)
	    noscore = TRUE;
#endif
	if (!no_command)
	{
	    if (running || to_death)
		ch = runch;
	    else if (count)
		ch = countch;
	    else
	    {
		ch = readchar();
		move_on = FALSE;
		if (mpos != 0)		/* Erase message if its there */
		    msg("");
	    }
	}
	else
	    ch = '.';
	if (no_command)
	{
	    if (--no_command == 0)
	    {
		player.t_flags |= ISRUN;
		msg("you can move again");
	    }
	}
	else
	{
	    /*
	     * check for prefixes
	     * 숫자 접두사 처리: 입력이 숫자이면 count(반복 횟수)를 조립한다.
	     * 예: '5' + 'h' → count=5, countch='h' → 왼쪽으로 5칸 이동.
	     * 반복이 의미 없는 명령(인벤토리, 저장 등)은 count를 0으로 초기화.
	     */
	    newcount = FALSE;
	    if (isdigit(ch))
	    {
		count = 0;
		newcount = TRUE;
		while (isdigit(ch))
		{
		    count = count * 10 + (ch - '0');
		    if (count > 255)
			count = 255;
		    ch = readchar();
		}
		countch = ch;
		/*
		 * turn off count for commands which don't make sense
		 * to repeat
		 */
		switch (ch)
		{
		    case CTRL('B'): case CTRL('H'): case CTRL('J'):
		    case CTRL('K'): case CTRL('L'): case CTRL('N'):
		    case CTRL('U'): case CTRL('Y'):
		    case '.': case 'a': case 'b': case 'h': case 'j':
		    case 'k': case 'l': case 'm': case 'n': case 'q':
		    case 'r': case 's': case 't': case 'u': case 'y':
		    case 'z': case 'B': case 'C': case 'H': case 'I':
		    case 'J': case 'K': case 'L': case 'N': case 'U':
		    case 'Y':
#ifdef MASTER
		    case CTRL('D'): case CTRL('A'):
#endif
			break;
		    default:
			count = 0;
		}
	    }
	    /*
	     * execute a command
	     * 명령 실행: switch(ch)로 키에 대응하는 명령을 디스패치한다.
	     * last_comm/last_dir/last_pick: 'a'(재실행)를 위해 마지막 명령을 기억.
	     */
	    if (count && !running)
		count--;
	    if (ch != 'a' && ch != ESCAPE && !(running || count || to_death))
	    {
		l_last_comm = last_comm;
		l_last_dir = last_dir;
		l_last_pick = last_pick;
		last_comm = ch;
		last_dir = '\0';
		last_pick = NULL;
	    }
over:
	    switch (ch)
	    {
		/* ',': 현재 위치의 아이템을 줍는다. 부유 상태이면 줍기 불가. */
		case ',': {
		    THING *obj = NULL;
		    int found = 0;
		    for (obj = lvl_obj; obj != NULL; obj = next(obj))
    			{
			    if (obj->o_pos.y == hero.y && obj->o_pos.x == hero.x)
			    {
				found=1;
				break;
			    }
    			}

		    if (found) {
			if (levit_check())
			    ;
			else
			    pick_up((char)obj->o_type);
		    }
		    else {
			if (!terse)
			    addmsg("there is ");
			addmsg("nothing here");
                        if (!terse)
                            addmsg(" to pick up");
                        endmsg();
		    }
		}
		when '!': shell();          /* 셸 실행 (Unix 셸로 잠시 나가기) */
		/* 8방향 이동: hjkl(좌하상우), yubn(대각선) */
		when 'h': do_move(0, -1);  /* 왼쪽 이동 */
		when 'j': do_move(1, 0);   /* 아래 이동 */
		when 'k': do_move(-1, 0);  /* 위 이동 */
		when 'l': do_move(0, 1);   /* 오른쪽 이동 */
		when 'y': do_move(-1, -1); /* 왼쪽 위 대각선 이동 */
		when 'u': do_move(-1, 1);  /* 오른쪽 위 대각선 이동 */
		when 'b': do_move(1, -1);  /* 왼쪽 아래 대각선 이동 */
		when 'n': do_move(1, 1);   /* 오른쪽 아래 대각선 이동 */
		/* 8방향 달리기: 대문자는 장애물/몬스터를 만날 때까지 자동 이동 */
		when 'H': do_run('h');
		when 'J': do_run('j');
		when 'K': do_run('k');
		when 'L': do_run('l');
		when 'Y': do_run('y');
		when 'U': do_run('u');
		when 'B': do_run('b');
		when 'N': do_run('n');
		/* Ctrl+방향키: 문이나 갈림길 앞에서 자동으로 멈추는 달리기 */
		when CTRL('H'): case CTRL('J'): case CTRL('K'): case CTRL('L'):
		case CTRL('Y'): case CTRL('U'): case CTRL('B'): case CTRL('N'):
		{
		    if (!on(player, ISBLIND))
		    {
			door_stop = TRUE;   /* 문 앞에서 자동 정지 */
			firstmove = TRUE;   /* 첫 이동 플래그 (즉시 정지 방지) */
		    }
		    if (count && !newcount)
			ch = direction;
		    else
		    {
			ch += ('A' - CTRL('A'));
			direction = ch;
		    }
		    goto over;
		}
		when 'F':
		    kamikaze = TRUE; /* F: 죽을 때까지 싸우기 모드 (kamikaze) 활성화 후 낙하 */
		    /* FALLTHROUGH */
		case 'f':
		    /* f/F: 방향을 지정하여 해당 위치의 몬스터를 직접 공격한다.
		     * 몬스터가 없거나 보이지 않으면 공격을 취소한다.
		     * F는 to_death 플래그를 설정하여 몬스터가 죽을 때까지 계속 공격. */
		    if (!get_dir())
		    {
			after = FALSE;
			break;
		    }
		    delta.y += hero.y;
		    delta.x += hero.x;
		    if ( ((mp = moat(delta.y, delta.x)) == NULL)
			|| ((!see_monst(mp)) && !on(player, SEEMONST)))
		    {
			if (!terse)
			    addmsg("I see ");
			msg("no monster there");
			after = FALSE;
		    }
		    else if (diag_ok(&hero, &delta))
		    {
			to_death = TRUE;
			max_hit = 0;
			mp->t_flags |= ISTARGET;
			runch = ch = dir_ch;
			goto over;
		    }
		when 't':
		    /* t: 방향을 지정하여 현재 인벤토리에서 선택한 미사일을 투척한다. */
		    if (!get_dir())
			after = FALSE;
		    else
			missile(delta.y, delta.x);
		when 'a':
		    /* a: 마지막으로 실행한 명령을 재실행한다. */
		    if (last_comm == '\0')
		    {
			msg("you haven't typed a command yet");
			after = FALSE;
		    }
		    else
		    {
			ch = last_comm;
			again = TRUE;
			goto over;
		    }
		when 'q': quaff();          /* q: 포션을 마신다 */
		when 'Q':                   /* Q: 게임을 종료한다 */
		    after = FALSE;
		    q_comm = TRUE;
		    quit(0);
		    q_comm = FALSE;
		when 'i': after = FALSE; inventory(pack, 0);  /* i: 전체 인벤토리 표시 */
		when 'I': after = FALSE; picky_inven();       /* I: 선택적 인벤토리 표시 */
		when 'd': drop();           /* d: 아이템을 바닥에 버린다 */
		when 'r': read_scroll();    /* r: 두루마리를 읽는다 */
		when 'e': eat();            /* e: 음식을 먹는다 */
		when 'w': wield();          /* w: 무기를 장착한다 */
		when 'W': wear();           /* W: 방어구를 착용한다 */
		when 'T': take_off();       /* T: 방어구를 벗는다 */
		when 'P': ring_on();        /* P: 반지를 낀다 */
		when 'R': ring_off();       /* R: 반지를 뺀다 */
		when 'o': option(); after = FALSE;   /* o: 게임 옵션을 설정한다 */
		when 'c': call(); after = FALSE;     /* c: 아이템에 이름을 붙인다 */
		when '>': after = FALSE; d_level();  /* >: 계단을 통해 다음 층으로 내려간다 */
		when '<': after = FALSE; u_level();  /* <: 계단을 통해 이전 층으로 올라간다 */
		when '?': after = FALSE; help();     /* ?: 명령 도움말을 표시한다 */
		when '/': after = FALSE; identify(); /* /: 화면 기호의 의미를 설명한다 */
		when 's': search();         /* s: 인접 타일에서 숨겨진 문/함정/통로를 탐색한다 */
		when 'z':
		    /* z: 방향을 지정하여 지팡이(wand) 또는 막대기(staff)를 사용한다 */
		    if (get_dir())
			do_zap();
		    else
			after = FALSE;
		when 'D': after = FALSE; discovered();   /* D: 지금까지 발견한 아이템 목록 표시 */
		when CTRL('P'): after = FALSE; msg(huh); /* Ctrl+P: 마지막 메시지를 다시 표시 */
		when CTRL('R'):
		    /* Ctrl+R: 화면을 강제로 다시 그린다 (redraw) */
		    after = FALSE;
		    clearok(curscr,TRUE);
		    wrefresh(curscr);
		when 'v':
		    after = FALSE;
		    msg("version %s. (mctesq was here)", release); /* v: 버전 정보 표시 */
		when 'S':
		    /* S: 현재 게임 상태를 파일에 저장한다 */
		    after = FALSE;
		    save_game();
		when '.': ;			/* Rest command */ /* .: 한 턴 휴식 */
		when ' ': after = FALSE;	/* "Legal" illegal command */ /* 스페이스: 아무것도 하지 않음 */
		when '^':
		    /* ^: 방향을 지정하여 해당 위치의 함정을 확인한다.
		     * 환각 상태이면 랜덤 함정 이름을 보여준다. */
		    after = FALSE;
		    if (get_dir()) {
			delta.y += hero.y;
			delta.x += hero.x;
			fp = &flat(delta.y, delta.x);
                        if (!terse)
                            addmsg("You have found ");
			if (chat(delta.y, delta.x) != TRAP)
			    msg("no trap there");
			else if (on(player, ISHALU))
			    msg(tr_name[rnd(NTRAPS)]);
			else {
			    msg(tr_name[*fp & F_TMASK]);
			    *fp |= F_SEEN;
			}
		    }
#ifdef MASTER
		when '+':
		    after = FALSE;
		    if (wizard)
		    {
			wizard = FALSE;
			turn_see(TRUE);
			msg("not wizard any more");
		    }
		    else
		    {
			wizard = passwd();
			if (wizard) 
			{
			    noscore = TRUE;
			    turn_see(FALSE);
			    msg("you are suddenly as smart as Ken Arnold in dungeon #%d", dnum);
			}
			else
			    msg("sorry");
		    }
#endif
		when ESCAPE:	/* Escape: 달리기 및 count/연속 명령을 모두 취소한다 */
		    door_stop = FALSE;
		    count = 0;
		    after = FALSE;
		    again = FALSE;
		when 'm':
		    /* m: 방향을 지정하여 그 위치로 이동한다. 아이템이 있어도 줍지 않는다. */
		    move_on = TRUE;
		    if (!get_dir())
			after = FALSE;
		    else
		    {
			ch = dir_ch;
			countch = dir_ch;
			goto over;
		    }
		when ')': current(cur_weapon, "wielding", NULL); /* ): 현재 장착 무기 표시 */
		when ']': current(cur_armor, "wearing", NULL);   /* ]: 현재 착용 방어구 표시 */
		when '=':
		    /* =: 현재 착용 중인 왼손/오른손 반지를 표시한다 */
		    current(cur_ring[LEFT], "wearing",
					    terse ? "(L)" : "on left hand");
		    current(cur_ring[RIGHT], "wearing",
					    terse ? "(R)" : "on right hand");
		when '@':
		    /* @: 플레이어의 현재 스탯(HP, 힘, 방어구 등)을 메시지로 출력한다 */
		    stat_msg = TRUE;
		    status();
		    stat_msg = FALSE;
		    after = FALSE;
		otherwise:
		    after = FALSE;
#ifdef MASTER
		    if (wizard) switch (ch)
		    {
			case '|': msg("@ %d,%d", hero.y, hero.x);
			when 'C': create_obj();
			when '$': msg("inpack = %d", inpack);
			when CTRL('G'): inventory(lvl_obj, 0);
			when CTRL('W'): whatis(FALSE, 0);
			when CTRL('D'): level++; new_level();
			when CTRL('A'): level--; new_level();
			when CTRL('F'): show_map();
			when CTRL('T'): teleport();
			when CTRL('E'): msg("food left: %d", food_left);
			when CTRL('C'): add_pass();
			when CTRL('X'): turn_see(on(player, SEEMONST));
			when CTRL('~'):
			{
			    THING *item;

			    if ((item = get_item("charge", STICK)) != NULL)
				item->o_charges = 10000;
			}
			when CTRL('I'):
			{
			    int i;
			    THING *obj;

			    for (i = 0; i < 9; i++)
				raise_level();
			    /*
			     * Give him a sword (+1,+1)
			     * 위자드 테스트용: (+1,+1) 양손검을 지급한다.
			     */
			    obj = new_item();
			    init_weapon(obj, TWOSWORD);
			    obj->o_hplus = 1;
			    obj->o_dplus = 1;
			    add_pack(obj, TRUE);
			    cur_weapon = obj;
			    /*
			     * And his suit of armor
			     * 위자드 테스트용: 방어등급 -5의 판금갑옷을 지급한다.
			     */
			    obj = new_item();
			    obj->o_type = ARMOR;
			    obj->o_which = PLATE_MAIL;
			    obj->o_arm = -5;
			    obj->o_flags |= ISKNOW;
			    obj->o_count = 1;
			    obj->o_group = 0;
			    cur_armor = obj;
			    add_pack(obj, TRUE);
			}
			when '*' :
			    pr_list();
			otherwise:
			    illcom(ch);
		    }
		    else
#endif
			illcom(ch);
	    }
	    /*
	     * turn off flags if no longer needed
	     * 달리기가 끝났으면 door_stop 플래그를 해제한다.
	     */
	    if (!running)
		door_stop = FALSE;
	}
	/*
	 * If he ran into something to take, let him pick it up.
	 * 이동 중에 아이템 위를 지나쳤다면 자동으로 줍기를 시도한다.
	 * (take 변수에 아이템 타입이 설정되어 있는 경우)
	 */
	if (take != 0)
	    pick_up(take);
	if (!running)
	    door_stop = FALSE;
	if (!after)
	    ntimes++;
    }
    do_daemons(AFTER);
    do_fuses(AFTER);
    if (ISRING(LEFT, R_SEARCH))
	search();
    else if (ISRING(LEFT, R_TELEPORT) && rnd(50) == 0)
	teleport();
    if (ISRING(RIGHT, R_SEARCH))
	search();
    else if (ISRING(RIGHT, R_TELEPORT) && rnd(50) == 0)
	teleport();
}

/*
 * illcom:
 *	What to do with an illegal command
 *
 * [한국어 설명]
 * 플레이어가 유효하지 않은 키를 입력했을 때 호출된다.
 * save_msg를 FALSE로 잠시 비활성화하여 오류 메시지가 저장되지 않도록 하고,
 * count를 0으로 초기화한 뒤 "illegal command" 메시지를 출력한다.
 */
void
illcom(int ch)
{
    save_msg = FALSE;
    count = 0;
    msg("illegal command '%s'", unctrl(ch));
    save_msg = TRUE;
}

/*
 * search:
 *	player gropes about him to find hidden things.
 *
 * [한국어 설명]
 * 플레이어 주변 8칸의 타일을 탐색하여 숨겨진 것을 찾는다.
 *
 * 탐색 대상:
 *   - '|'/'-' 타일: F_REAL이 없으면 비밀 문(secret door) 후보 → DOOR로 전환
 *   - FLOOR 타일 : F_REAL이 없으면 함정(TRAP) 후보 → TRAP으로 전환
 *   - ' ' 타일   : F_REAL이 없으면 비밀 통로(PASSAGE) 후보 → PASSAGE로 전환
 *
 * probinc: 환각(ISHALU) +3, 시각장애(ISBLIND) +2 → 탐색 확률 저하 (불리한 조건)
 * 발견 시 count와 running을 중단하고 look()으로 화면을 갱신한다.
 */
void
search()
{
    register int y, x;
    register char *fp;
    register int ey, ex;
    int probinc;
    bool found;

    ey = hero.y + 1;
    ex = hero.x + 1;
    probinc = (on(player, ISHALU) ? 3 : 0);
    probinc += (on(player, ISBLIND) ? 2 : 0);
    found = FALSE;
    for (y = hero.y - 1; y <= ey; y++) 
	for (x = hero.x - 1; x <= ex; x++)
	{
	    if (y == hero.y && x == hero.x)
		continue;
	    fp = &flat(y, x);
	    if (!(*fp & F_REAL))
		switch (chat(y, x))
		{
		    case '|':
		    case '-':
			if (rnd(5 + probinc) != 0)
			    break;
			chat(y, x) = DOOR;
                        msg("a secret door");
foundone:
			found = TRUE;
			*fp |= F_REAL;
			count = FALSE;
			running = FALSE;
			break;
		    case FLOOR:
			if (rnd(2 + probinc) != 0)
			    break;
			chat(y, x) = TRAP;
			if (!terse)
			    addmsg("you found ");
			if (on(player, ISHALU))
			    msg(tr_name[rnd(NTRAPS)]);
			else {
			    msg(tr_name[*fp & F_TMASK]);
			    *fp |= F_SEEN;
			}
			goto foundone;
			break;
		    case ' ':
			if (rnd(3 + probinc) != 0)
			    break;
			chat(y, x) = PASSAGE;
			goto foundone;
		}
	}
    if (found)
	look(FALSE);
}

/*
 * help:
 *	Give single character help, or the whole mess if he wants it
 *
 * [한국어 설명]
 * '?' 명령에 응답하여 도움말을 출력한다.
 * - 특정 문자를 입력하면 해당 명령의 설명만 표시
 * - '*'을 입력하면 helpstr[] 배열에 있는 모든 명령을 2열로 나열하여 표시
 * numprint: 화면에 출력할 항목 수 (홀수이면 올림, 최대 LINES-1)
 */
void
help()
{
    register struct h_list *strp;
    register char helpch;
    register int numprint, cnt;
    msg("character you want help for (* for all): ");
    helpch = readchar();
    mpos = 0;
    /*
     * If its not a *, print the right help string
     * or an error if he typed a funny character.
     */
    if (helpch != '*')
    {
	move(0, 0);
	for (strp = helpstr; strp->h_desc != NULL; strp++)
	    if (strp->h_ch == helpch)
	    {
		lower_msg = TRUE;
		msg("%s%s", unctrl(strp->h_ch), strp->h_desc);
		lower_msg = FALSE;
		return;
	    }
	msg("unknown character '%s'", unctrl(helpch));
	return;
    }
    /*
     * Here we print help for everything.
     * Then wait before we return to command mode
     */
    numprint = 0;
    for (strp = helpstr; strp->h_desc != NULL; strp++)
	if (strp->h_print)
	    numprint++;
    if (numprint & 01)		/* round odd numbers up */
	numprint++;
    numprint /= 2;
    if (numprint > LINES - 1)
	numprint = LINES - 1;

    wclear(hw);
    cnt = 0;
    for (strp = helpstr; strp->h_desc != NULL; strp++)
	if (strp->h_print)
	{
	    wmove(hw, cnt % numprint, cnt >= numprint ? COLS / 2 : 0);
	    if (strp->h_ch)
		waddstr(hw, unctrl(strp->h_ch));
	    waddstr(hw, strp->h_desc);
	    if (++cnt >= numprint * 2)
		break;
	}
    wmove(hw, LINES - 1, 0);
    waddstr(hw, "--Press space to continue--");
    wrefresh(hw);
    wait_for(' ');
    clearok(stdscr, TRUE);
/*
    refresh();
*/
    msg("");
    touchwin(stdscr);
    wrefresh(stdscr);
}

/*
 * identify:
 *	Tell the player what a certain thing is.
 *
 * [한국어 설명]
 * '/' 명령에 응답하여 화면에 표시된 기호의 의미를 설명한다.
 * 대문자(A~Z)이면 monsters[] 배열에서 몬스터 이름을 조회하고,
 * 그 외엔 ident_list[]에서 해당 기호의 설명을 찾아 표시한다.
 */
void
identify()
{
    register int ch;
    register struct h_list *hp;
    register char *str;
    static struct h_list ident_list[] = {
	{'|',		"wall of a room",		FALSE},
	{'-',		"wall of a room",		FALSE},
	{GOLD,		"gold",				FALSE},
	{STAIRS,	"a staircase",			FALSE},
	{DOOR,		"door",				FALSE},
	{FLOOR,		"room floor",			FALSE},
	{PLAYER,	"you",				FALSE},
	{PASSAGE,	"passage",			FALSE},
	{TRAP,		"trap",				FALSE},
	{POTION,	"potion",			FALSE},
	{SCROLL,	"scroll",			FALSE},
	{FOOD,		"food",				FALSE},
	{WEAPON,	"weapon",			FALSE},
	{' ',		"solid rock",			FALSE},
	{ARMOR,		"armor",			FALSE},
	{AMULET,	"the Amulet of Yendor",		FALSE},
	{RING,		"ring",				FALSE},
	{STICK,		"wand or staff",		FALSE},
	{'\0'}
    };

    msg("what do you want identified? ");
    ch = readchar();
    mpos = 0;
    if (ch == ESCAPE)
    {
	msg("");
	return;
    }
    if (isupper(ch))
	str = monsters[ch-'A'].m_name;
    else
    {
	str = "unknown character";
	for (hp = ident_list; hp->h_ch != '\0'; hp++)
	    if (hp->h_ch == ch)
	    {
		str = hp->h_desc;
		break;
	    }
    }
    msg("'%s': %s", unctrl(ch), str);
}

/*
 * d_level:
 *	He wants to go down a level
 *
 * [한국어 설명]
 * '>' 명령으로 다음 층(더 깊은 곳)으로 내려간다.
 * 현재 위치가 STAIRS가 아니거나 부유(levitation) 상태이면 거부한다.
 * 이동 시 level을 1 증가시키고 seenstairs를 초기화한 뒤 new_level()을 호출한다.
 */
void
d_level()
{
    if (levit_check())
	return;
    if (chat(hero.y, hero.x) != STAIRS)
	msg("I see no way down");
    else
    {
	level++;
	seenstairs = FALSE;
	new_level();
    }
}

/*
 * u_level:
 *	He wants to go up a level
 *
 * [한국어 설명]
 * '<' 명령으로 이전 층(더 얕은 곳)으로 올라간다.
 * 올라가려면 Yendor의 부적(amulet)을 소지해야 한다.
 * 부적이 없으면 "마법으로 막혀있다"는 메시지가 출력된다.
 * level이 0이 되면 total_winner()를 호출하여 게임 승리 처리.
 */
void
u_level()
{
    if (levit_check())
	return;
    if (chat(hero.y, hero.x) == STAIRS)
	if (amulet)
	{
	    level--;
	    if (level == 0)
		total_winner();
	    new_level();
	    msg("you feel a wrenching sensation in your gut");
	}
	else
	    msg("your way is magically blocked");
    else
	msg("I see no way up");
}

/*
 * levit_check:
 *	Check to see if she's levitating, and if she is, print an
 *	appropriate message.
 *
 * [한국어 설명]
 * 플레이어가 ISLEVIT(부유) 상태인지 확인한다.
 * 부유 중이면 TRUE를 반환하고 메시지를 출력한다.
 * 계단 이동, 아이템 줍기 등 지면 접촉이 필요한 행동 전에 호출된다.
 */
bool
levit_check()
{
    if (!on(player, ISLEVIT))
	return FALSE;
    msg("You can't.  You're floating off the ground!");
    return TRUE;
}

/*
 * call:
 *	Allow a user to call a potion, scroll, or ring something
 *
 * [한국어 설명]
 * 'c' 명령으로 포션/두루마리/반지/지팡이에 플레이어가 원하는 이름을 붙인다.
 * 이미 식별된(oi_know == TRUE) 아이템에는 이름을 붙일 수 없다.
 * 입력받은 이름은 oi_guess(또는 o_label) 필드에 동적 할당하여 저장한다.
 * FOOD는 이름 붙이기 불가.
 */
void
call()
{
    register THING *obj;
    register struct obj_info *op = NULL;
    register char **guess, *elsewise = NULL;
    register bool *know;

    obj = get_item("call", CALLABLE);
    /*
     * Make certain that it is somethings that we want to wear
     */
    if (obj == NULL)
	return;
    switch (obj->o_type)
    {
	case RING:
	    op = &ring_info[obj->o_which];
	    elsewise = r_stones[obj->o_which];
	    goto norm;
	when POTION:
	    op = &pot_info[obj->o_which];
	    elsewise = p_colors[obj->o_which];
	    goto norm;
	when SCROLL:
	    op = &scr_info[obj->o_which];
	    elsewise = s_names[obj->o_which];
	    goto norm;
	when STICK:
	    op = &ws_info[obj->o_which];
	    elsewise = ws_made[obj->o_which];
norm:
	    know = &op->oi_know;
	    guess = &op->oi_guess;
	    if (*guess != NULL)
		elsewise = *guess;
	when FOOD:
	    msg("you can't call that anything");
	    return;
	otherwise:
	    guess = &obj->o_label;
	    know = NULL;
	    elsewise = obj->o_label;
    }
    if (know != NULL && *know)
    {
	msg("that has already been identified");
	return;
    }
    if (elsewise != NULL && elsewise == *guess)
    {
	if (!terse)
	    addmsg("Was ");
	msg("called \"%s\"", elsewise);
    }
    if (terse)
	msg("call it: ");
    else
	msg("what do you want to call it? ");

    if (elsewise == NULL)
	strcpy(prbuf, "");
    else
	strcpy(prbuf, elsewise);
    if (get_str(prbuf, stdscr) == NORM)
    {
	if (*guess != NULL)
	    free(*guess);
	*guess = malloc((unsigned int) strlen(prbuf) + 1);
	strcpy(*guess, prbuf);
    }
}

/*
 * current:
 *	Print the current weapon/armor
 *
 * [한국어 설명]
 * 현재 장착/착용 중인 아이템(무기, 방어구, 반지)을 메시지로 표시한다.
 * cur가 NULL이면 "nothing"을 출력한다.
 * where가 지정되면 위치 정보(예: "on left hand")를 추가로 출력한다.
 */
void
current(THING *cur, char *how, char *where)
{
    after = FALSE;
    if (cur != NULL)
    {
	if (!terse)
	    addmsg("you are %s (", how);
	inv_describe = FALSE;
	addmsg("%c) %s", cur->o_packch, inv_name(cur, TRUE));
	inv_describe = TRUE;
	if (where)
	    addmsg(" %s", where);
	endmsg();
    }
    else
    {
	if (!terse)
	    addmsg("you are ");
	addmsg("%s nothing", how);
	if (where)
	    addmsg(" %s", where);
	endmsg();
    }
}
