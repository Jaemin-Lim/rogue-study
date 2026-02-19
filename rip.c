/*
 * File for the fun ends
 * Death or a total win
 * 게임 종료와 관련된 함수들을 담은 파일.
 * 사망 화면, 점수 저장/출력, 승리 화면, 사망 원인 이름 등을 처리한다.
 *
 * @(#)rip.c	4.57 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <curses.h>
#include "rogue.h"
#include "score.h"

/* rip: 묘비(tombstone) ASCII 아트 배열 */
/* 각 줄은 묘비의 외형을 그린다 */
static char *rip[] = {
"                       __________\n",
"                      /          \\\n",
"                     /    REST    \\\n",
"                    /      IN      \\\n",
"                   /     PEACE      \\\n",
"                  /                  \\\n",
"                  |                  |\n",
"                  |                  |\n",
"                  |   killed by a    |\n",   /* 16번째 줄: "killed by a" + 모음 */
"                  |                  |\n",   /* 17번째 줄: 킬러 이름 */
"                  |       1980       |\n",   /* 18번째 줄: 연도 */
"                 *|     *  *  *      | *\n",
"         ________)/\\\\_//(\\/(/\\)/\\//\\/|_)_______\n",
    0
};

/*
 * score:
 *	Figure score and post it.
 *	점수를 계산하고 점수 파일에 저장 및 출력하는 함수.
 *	amount: 최종 금액, flags: 종료 유형(0=사망, 1=종료, 2=완승, 3=부적사망),
 *	monst: 사망 원인 몬스터 코드
 *
 *	점수 파일(score.h의 SCORE 구조체 배열)을 읽어 순위를 비교하고,
 *	상위 점수이면 삽입한다. 각 사용자는 하나의 점수만 기록된다(allscore가 아닌 경우).
 */
/* VARARGS2 */

void
score(int amount, int flags, char monst)
{
    SCORE *scp;       /* 현재 처리 중인 점수 항목 */
    int i;
    SCORE *sc2;       /* 삽입할 위치 포인터 */
    SCORE *top_ten, *endp;  /* 상위 점수 배열의 시작/끝 */
# ifdef MASTER
    int prflags = 0;  /* MASTER 모드: 출력 플래그 (1=이름, 2=편집) */
# endif
    void (*fp)(int);  /* SIGINT 핸들러 저장용 */
    unsigned int uid; /* 사용자 ID */
    /* 종료 사유 문자열 */
    static char *reason[] = {
	"killed",           /* 0: 몬스터에 의한 사망 */
	"quit",             /* 1: 플레이어 종료 */
	"A total winner",   /* 2: 완전한 승리 */
	"killed with Amulet" /* 3: 부적 소지 상태에서 사망 */
    };

    start_score();  /* 점수 화면 시작 */

    /* 정상 종료(flags >= 0)이거나 마법사 모드이면 화면 표시 */
 if (flags >= 0
#ifdef MASTER
            || wizard
#endif
        )
    {
	mvaddstr(LINES - 1, 0 , "[Press return to continue]");
        refresh();
        wgetnstr(stdscr,prbuf,80);
 	endwin();
        printf("\n");
        resetltchars();
	/*
	 * free up space to "guarantee" there is space for the top_ten
	 * 상위 10개 점수 배열을 위한 메모리 공간 확보
	 */
	delwin(stdscr);
	delwin(curscr);
	if (hw != NULL)
	    delwin(hw);
    }

    top_ten = (SCORE *) malloc(numscores * sizeof (SCORE));  /* 점수 배열 할당 */
    endp = &top_ten[numscores];  /* 배열 끝 포인터 */
    /* 새 점수 배열을 랜덤 데이터로 초기화 (보안: 의도치 않은 데이터 노출 방지) */
    for (scp = top_ten; scp < endp; scp++)
    {
	scp->sc_score = 0;
	for (i = 0; i < MAXSTR; i++)
	    scp->sc_name[i] = (unsigned char) rnd(255);
	scp->sc_flags = RN;
	scp->sc_level = RN;
	scp->sc_monster = (unsigned short) RN;
	scp->sc_uid = RN;
    }

    signal(SIGINT, SIG_DFL);  /* SIGINT를 기본 처리로 복원 */

#ifdef MASTER
    /* MASTER 모드: "names" 입력 시 실제 이름 표시, "edit" 입력 시 점수 편집 */
    if (wizard)
	if (strcmp(prbuf, "names") == 0)
	    prflags = 1;
	else if (strcmp(prbuf, "edit") == 0)
	    prflags = 2;
#endif
    rd_score(top_ten);  /* 점수 파일 읽기 */
    /*
     * Insert her in list if need be
     * 현재 점수가 상위권이면 목록에 삽입
     */
    sc2 = NULL;
    if (!noscore)  /* noscore가 아니면 (마법사 모드가 아니면) */
    {
	uid = md_getuid();  /* 현재 사용자 ID */
	for (scp = top_ten; scp < endp; scp++)
	    if (amount > scp->sc_score)  /* 현재 점수보다 낮은 항목 발견 */
		break;
	    else if (!allscore &&	/* only one score per nowin uid */
                                    /* allscore가 아니면 uid당 하나의 점수만 */
		flags != 2 && scp->sc_uid == uid && scp->sc_flags != 2)
		    scp = endp;  /* 이미 같은 UID의 점수가 있으면 삽입 안 함 */
	if (scp < endp)
	{
	    if (flags != 2 && !allscore)
	    {
		/* 같은 UID의 기존 점수를 찾아 덮어쓸 위치 결정 */
		for (sc2 = scp; sc2 < endp; sc2++)
		{
		    if (sc2->sc_uid == uid && sc2->sc_flags != 2)
			break;
		}
		if (sc2 >= endp)
		    sc2 = endp - 1;
	    }
	    else
		sc2 = endp - 1;
	    /* 삽입 위치 이후의 항목들을 한 칸씩 밀기 */
	    while (sc2 > scp)
	    {
		*sc2 = sc2[-1];
		sc2--;
	    }
	    /* 새 점수 항목 설정 */
	    scp->sc_score = amount;
	    strncpy(scp->sc_name, whoami, MAXSTR);
	    scp->sc_flags = flags;
	    if (flags == 2)
		scp->sc_level = max_level;  /* 완승: 최고 레벨 기록 */
	    else
		scp->sc_level = level;      /* 그 외: 현재 레벨 기록 */
	    scp->sc_monster = monst;     /* 사망 원인 몬스터 */
	    scp->sc_uid = uid;
	    sc2 = scp;  /* 삽입된 위치 저장 (출력 시 강조용) */
	}
    }
    /*
     * Print the list
     * 점수 목록 출력
     */
    if (flags != -1)  /* -1이면 점수 목록만 출력 (-s 옵션) */
	putchar('\n');
    printf("Top %s %s:\n", Numname, allscore ? "Scores" : "Rogueists");
    printf("   Score Name\n");
    for (scp = top_ten; scp < endp; scp++)
    {
	if (scp->sc_score) {
	    if (sc2 == scp)        /* 새로 삽입된 점수이면 강조 표시 */
            md_raw_standout();
	    printf("%2d %5d %s: %s on level %d", (int) (scp - top_ten + 1),
		scp->sc_score, scp->sc_name, reason[scp->sc_flags],
		scp->sc_level);
	    if (scp->sc_flags == 0 || scp->sc_flags == 3)  /* 사망 유형이면 */
		printf(" by %s", killname((char) scp->sc_monster, TRUE));  /* 킬러 이름 */
#ifdef MASTER
	    if (prflags == 1)
	    {
	    printf(" (%s)", md_getrealname(scp->sc_uid));
	    }
	    else if (prflags == 2)
	    {
		fflush(stdout);
		(void) fgets(prbuf,10,stdin);
		if (prbuf[0] == 'd')  /* 'd' 입력으로 점수 삭제 */
		{
		    for (sc2 = scp; sc2 < endp - 1; sc2++)
			*sc2 = *(sc2 + 1);
		    sc2 = endp - 1;
		    sc2->sc_score = 0;
		    for (i = 0; i < MAXSTR; i++)
			sc2->sc_name[i] = (char) rnd(255);
		    sc2->sc_flags = RN;
		    sc2->sc_level = RN;
		    sc2->sc_monster = (unsigned short) RN;
		    scp--;
		}
	    }
	    else
#endif /* MASTER */
                printf(".");
	    if (sc2 == scp)
		    md_raw_standend();  /* 강조 종료 */
            putchar('\n');
	}
	else
	    break;  /* 점수가 0이면 더 이상 항목 없음 */
    }
    /*
     * Update the list file
     * 점수 파일 업데이트 (새 점수가 삽입된 경우)
     */
    if (sc2 != NULL)
    {
	if (lock_sc())  /* 점수 파일 잠금 (동시 접근 방지) */
	{
	    fp = signal(SIGINT, SIG_IGN);  /* 저장 중 SIGINT 무시 */
	    wr_score(top_ten);  /* 점수 파일 쓰기 */
	    unlock_sc();
	    signal(SIGINT, fp);
	}
    }
}

/*
 * death:
 *	Do something really fun when he dies
 *	플레이어가 사망했을 때 호출되는 함수.
 *	tombstone이 FALSE이면 간단한 텍스트 메시지를 출력하고,
 *	TRUE이면 묘비(rip[]) ASCII 아트를 화면에 표시한다.
 *	사망 시 금화의 10%를 잃는다.
 *	이후 score() 함수를 호출하여 점수를 저장한다.
 */

void
death(char monst)
{
    char **dp, *killer;  /* 묘비 줄 포인터, 킬러 이름 */
    struct tm *lt;       /* 현재 시간 구조체 */
    static time_t date;  /* 현재 시간 */
    struct tm *localtime();

    signal(SIGINT, SIG_IGN);  /* 사망 중 SIGINT 무시 */
    purse -= purse / 10;      /* 사망 시 금화 10% 손실 */
    signal(SIGINT, leave);    /* 이후 SIGINT는 leave() 호출 */
    clear();
    killer = killname(monst, FALSE);  /* 킬러 이름 가져오기 */
    if (!tombstone)  /* 간단한 텍스트 사망 메시지 */
    {
	mvprintw(LINES - 2, 0, "Killed by ");
	killer = killname(monst, FALSE);
	if (monst != 's' && monst != 'h')  /* 굶주림/체온저하 제외 */
	    printw("a%s ", vowelstr(killer));
	printw("%s with %d gold", killer, purse);
    }
    else  /* 묘비 ASCII 아트 표시 */
    {
	time(&date);
	lt = localtime(&date);
	move(8, 0);
	dp = rip;
	while (*dp)
	    addstr(*dp++);  /* 묘비 그리기 */
	mvaddstr(17, center(killer), killer);  /* 킬러 이름 */
	if (monst == 's' || monst == 'h')
	    mvaddch(16, 32, ' ');  /* "killed by" 단어 공백 처리 */
	else
	    mvaddstr(16, 33, vowelstr(killer));  /* "a" 또는 "an" */
	mvaddstr(14, center(whoami), whoami);  /* 플레이어 이름 */
	sprintf(prbuf, "%d Au", purse);
	move(15, center(prbuf));
	addstr(prbuf);  /* 금화 표시 */
	sprintf(prbuf, "%4d", 1900+lt->tm_year);
	mvaddstr(18, 26, prbuf);  /* 사망 연도 */
    }
    move(LINES - 1, 0);
    refresh();
    score(purse, amulet ? 3 : 0, monst);  /* 점수 저장 (부적 소지 여부 구분) */
    printf("[Press return to continue]");
    fflush(stdout);
    (void) fgets(prbuf,10,stdin);
    my_exit(0);
}

/*
 * center:
 *	Return the index to center the given string
 *	묘비 텍스트를 중앙에 배치하기 위한 x 좌표를 계산하는 함수.
 *	묘비 중앙 위치(28)에서 문자열 절반 길이를 뺀 값을 반환한다.
 */
int
center(char *str)
{
    return 28 - (((int)strlen(str) + 1) / 2);
}

/*
 * total_winner:
 *	Code for a winner
 *	플레이어가 게임을 완전히 이겼을 때(부적을 갖고 지하 1층으로 귀환)
 *	호출되는 함수.
 *	"You Made It" 특수 화면을 표시하고,
 *	배낭의 모든 아이템 가치를 계산하여 금화에 더한 최종 점수를 저장한다.
 */

void
total_winner()
{
    THING *obj;         /* 아이템 포인터 */
    struct obj_info *op; /* 아이템 정보 포인터 */
    int worth = 0;      /* 아이템 가치 */
    int oldpurse;       /* 이전 금화량 (배낭 아이템 가치 합산 전) */

    clear();
    standout();
    /* 대형 ASCII 아트: "YOU MADE IT" */
    addstr("                                                               \n");
    addstr("  @   @               @   @           @          @@@  @     @  \n");
    addstr("  @   @               @@ @@           @           @   @     @  \n");
    addstr("  @   @  @@@  @   @   @ @ @  @@@   @@@@  @@@      @  @@@    @  \n");
    addstr("   @@@@ @   @ @   @   @   @     @ @   @ @   @     @   @     @  \n");
    addstr("      @ @   @ @   @   @   @  @@@@ @   @ @@@@@     @   @     @  \n");
    addstr("  @   @ @   @ @  @@   @   @ @   @ @   @ @         @   @  @     \n");
    addstr("   @@@   @@@   @@ @   @   @  @@@@  @@@@  @@@     @@@   @@   @  \n");
    addstr("                                                               \n");
    addstr("     Congratulations, you have made it to the light of day!    \n");
    standend();
    addstr("\nYou have joined the elite ranks of those who have escaped the\n");
    addstr("Dungeons of Doom alive.  You journey home and sell all your loot at\n");
    addstr("a great profit and are admitted to the Fighters' Guild.\n");
    mvaddstr(LINES - 1, 0, "--Press space to continue--");
    refresh();
    wait_for(' ');
    clear();
    mvaddstr(0, 0, "   Worth  Item\n");
    oldpurse = purse;
    /* 배낭의 모든 아이템 가치 계산 */
    for (obj = pack; obj != NULL; obj = next(obj))
    {
	switch (obj->o_type)
	{
	    case FOOD:
		worth = 2 * obj->o_count;  /* 음식: 개당 2 금화 */
	    when WEAPON:
		worth = weap_info[obj->o_which].oi_worth;
		worth *= 3 * (obj->o_hplus + obj->o_dplus) + obj->o_count;  /* 보너스 반영 */
		obj->o_flags |= ISKNOW;
	    when ARMOR:
		worth = arm_info[obj->o_which].oi_worth;
		worth += (9 - obj->o_arm) * 100;  /* 방어력 반영 */
		worth += (10 * (a_class[obj->o_which] - obj->o_arm));
		obj->o_flags |= ISKNOW;
	    when SCROLL:
		worth = scr_info[obj->o_which].oi_worth;
		worth *= obj->o_count;
		op = &scr_info[obj->o_which];
		if (!op->oi_know)  /* 미식별이면 절반 가치 */
		    worth /= 2;
		op->oi_know = TRUE;
	    when POTION:
		worth = pot_info[obj->o_which].oi_worth;
		worth *= obj->o_count;
		op = &pot_info[obj->o_which];
		if (!op->oi_know)
		    worth /= 2;
		op->oi_know = TRUE;
	    when RING:
		op = &ring_info[obj->o_which];
		worth = op->oi_worth;
		if (obj->o_which == R_ADDSTR || obj->o_which == R_ADDDAM ||
		    obj->o_which == R_PROTECT || obj->o_which == R_ADDHIT)
		{
			if (obj->o_arm > 0)
			    worth += obj->o_arm * 100;  /* 보정 수치 반영 */
			else
			    worth = 10;
		}
		if (!(obj->o_flags & ISKNOW))
		    worth /= 2;
		obj->o_flags |= ISKNOW;
		op->oi_know = TRUE;
	    when STICK:
		op = &ws_info[obj->o_which];
		worth = op->oi_worth;
		worth += 20 * obj->o_charges;  /* 충전량 반영 */
		if (!(obj->o_flags & ISKNOW))
		    worth /= 2;
		obj->o_flags |= ISKNOW;
		op->oi_know = TRUE;
	    when AMULET:
		worth = 1000;  /* 부적: 1000 금화 */
	}
	if (worth < 0)
	    worth = 0;
	printw("%c) %5d  %s\n", obj->o_packch, worth, inv_name(obj, FALSE));
	purse += worth;  /* 총 금화에 아이템 가치 추가 */
    }
    printw("   %5d  Gold Pieces          ", oldpurse);  /* 기존 금화 */
    refresh();
    score(purse, 2, ' ');  /* 완승 점수(flags=2) 저장 */
    my_exit(0);
}

/*
 * killname:
 *	Convert a code to a monster name
 *	사망 원인 코드를 이름 문자열로 변환하는 함수.
 *	대문자이면 monsters[] 배열에서 몬스터 이름을 가져온다.
 *	소문자이면 nlist 배열에서 특수 사망 원인 이름을 가져온다.
 *	- 'a': 화살, 'b': 볼트, 'd': 다트
 *	- 'h': 체온저하, 's': 굶주림
 *	doart: TRUE이면 "a " 또는 "an " 관사를 앞에 붙인다.
 */
char *
killname(char monst, bool doart)
{
    struct h_list *hp;  /* 특수 사망 원인 리스트 포인터 */
    char *sp;           /* 이름 문자열 */
    bool article;       /* 관사 필요 여부 */
    /* nlist: 특수 사망 원인 코드와 이름 매핑 */
    static struct h_list nlist[] = {
	{'a',	"arrow",		TRUE},   /* 화살 (관사 필요) */
	{'b',	"bolt",			TRUE},   /* 볼트 */
	{'d',	"dart",			TRUE},   /* 다트 */
	{'h',	"hypothermia",	FALSE},  /* 체온저하 (관사 불필요) */
	{'s',	"starvation",	FALSE},  /* 굶주림 */
	{'\0'}
    };

    if (isupper(monst))  /* 대문자이면 몬스터 이름 */
    {
	sp = monsters[monst-'A'].m_name;
	article = TRUE;
    }
    else  /* 소문자이면 특수 사망 원인 */
    {
	sp = "Wally the Wonder Badger";  /* 기본값: 알 수 없는 원인 */
	article = FALSE;
	for (hp = nlist; hp->h_ch; hp++)
	    if (hp->h_ch == monst)
	    {
		sp = hp->h_desc;
		article = hp->h_print;
		break;
	    }
    }
    if (doart && article)  /* 관사 붙이기 */
	sprintf(prbuf, "a%s ", vowelstr(sp));
    else
	prbuf[0] = '\0';
    strcat(prbuf, sp);
    return prbuf;
}

/*
 * death_monst:
 *	Return a monster appropriate for a random death.
 *	랜덤 사망 테스트(-d 옵션)용 무작위 사망 원인 코드를 반환하는 함수.
 *	모든 몬스터 알파벳(A-Z)과 특수 원인(화살, 볼트, 체온저하, 굶주림, 다트),
 *	그리고 공백(Wally the Wonder Badger)을 포함한다.
 */
char
death_monst()
{
    static char poss[] =
    {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
	'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'h', 'd', 's',
	' '	/* This is provided to generate the "Wally the Wonder Badger"
		   message for killer */
                /* 공백은 "Wally the Wonder Badger" 메시지를 생성하기 위한 것 */
    };

    return poss[rnd(sizeof poss / sizeof (char))];
}

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <curses.h>
#include "rogue.h"
#include "score.h"

static char *rip[] = {
"                       __________\n",
"                      /          \\\n",
"                     /    REST    \\\n",
"                    /      IN      \\\n",
"                   /     PEACE      \\\n",
"                  /                  \\\n",
"                  |                  |\n",
"                  |                  |\n",
"                  |   killed by a    |\n",
"                  |                  |\n",
"                  |       1980       |\n",
"                 *|     *  *  *      | *\n",
"         ________)/\\\\_//(\\/(/\\)/\\//\\/|_)_______\n",
    0
};

/*
 * score:
 *	Figure score and post it.
 */
/* VARARGS2 */

void
score(int amount, int flags, char monst)
{
    SCORE *scp;
    int i;
    SCORE *sc2;
    SCORE *top_ten, *endp;
# ifdef MASTER
    int prflags = 0;
# endif
    void (*fp)(int);
    unsigned int uid;
    static char *reason[] = {
	"killed",
	"quit",
	"A total winner",
	"killed with Amulet"
    };

    start_score();

 if (flags >= 0
#ifdef MASTER
            || wizard
#endif
        )
    {
	mvaddstr(LINES - 1, 0 , "[Press return to continue]");
        refresh();
        wgetnstr(stdscr,prbuf,80);
 	endwin();
        printf("\n");
        resetltchars();
	/*
	 * free up space to "guarantee" there is space for the top_ten
	 */
	delwin(stdscr);
	delwin(curscr);
	if (hw != NULL)
	    delwin(hw);
    }

    top_ten = (SCORE *) malloc(numscores * sizeof (SCORE));
    endp = &top_ten[numscores];
    for (scp = top_ten; scp < endp; scp++)
    {
	scp->sc_score = 0;
	for (i = 0; i < MAXSTR; i++)
	    scp->sc_name[i] = (unsigned char) rnd(255);
	scp->sc_flags = RN;
	scp->sc_level = RN;
	scp->sc_monster = (unsigned short) RN;
	scp->sc_uid = RN;
    }

    signal(SIGINT, SIG_DFL);

#ifdef MASTER
    if (wizard)
	if (strcmp(prbuf, "names") == 0)
	    prflags = 1;
	else if (strcmp(prbuf, "edit") == 0)
	    prflags = 2;
#endif
    rd_score(top_ten);
    /*
     * Insert her in list if need be
     */
    sc2 = NULL;
    if (!noscore)
    {
	uid = md_getuid();
	for (scp = top_ten; scp < endp; scp++)
	    if (amount > scp->sc_score)
		break;
	    else if (!allscore &&	/* only one score per nowin uid */
		flags != 2 && scp->sc_uid == uid && scp->sc_flags != 2)
		    scp = endp;
	if (scp < endp)
	{
	    if (flags != 2 && !allscore)
	    {
		for (sc2 = scp; sc2 < endp; sc2++)
		{
		    if (sc2->sc_uid == uid && sc2->sc_flags != 2)
			break;
		}
		if (sc2 >= endp)
		    sc2 = endp - 1;
	    }
	    else
		sc2 = endp - 1;
	    while (sc2 > scp)
	    {
		*sc2 = sc2[-1];
		sc2--;
	    }
	    scp->sc_score = amount;
	    strncpy(scp->sc_name, whoami, MAXSTR);
	    scp->sc_flags = flags;
	    if (flags == 2)
		scp->sc_level = max_level;
	    else
		scp->sc_level = level;
	    scp->sc_monster = monst;
	    scp->sc_uid = uid;
	    sc2 = scp;
	}
    }
    /*
     * Print the list
     */
    if (flags != -1)
	putchar('\n');
    printf("Top %s %s:\n", Numname, allscore ? "Scores" : "Rogueists");
    printf("   Score Name\n");
    for (scp = top_ten; scp < endp; scp++)
    {
	if (scp->sc_score) {
	    if (sc2 == scp)
            md_raw_standout();
	    printf("%2d %5d %s: %s on level %d", (int) (scp - top_ten + 1),
		scp->sc_score, scp->sc_name, reason[scp->sc_flags],
		scp->sc_level);
	    if (scp->sc_flags == 0 || scp->sc_flags == 3)
		printf(" by %s", killname((char) scp->sc_monster, TRUE));
#ifdef MASTER
	    if (prflags == 1)
	    {
	    printf(" (%s)", md_getrealname(scp->sc_uid));
	    }
	    else if (prflags == 2)
	    {
		fflush(stdout);
		(void) fgets(prbuf,10,stdin);
		if (prbuf[0] == 'd')
		{
		    for (sc2 = scp; sc2 < endp - 1; sc2++)
			*sc2 = *(sc2 + 1);
		    sc2 = endp - 1;
		    sc2->sc_score = 0;
		    for (i = 0; i < MAXSTR; i++)
			sc2->sc_name[i] = (char) rnd(255);
		    sc2->sc_flags = RN;
		    sc2->sc_level = RN;
		    sc2->sc_monster = (unsigned short) RN;
		    scp--;
		}
	    }
	    else
#endif /* MASTER */
                printf(".");
	    if (sc2 == scp)
		    md_raw_standend();
            putchar('\n');
	}
	else
	    break;
    }
    /*
     * Update the list file
     */
    if (sc2 != NULL)
    {
	if (lock_sc())
	{
	    fp = signal(SIGINT, SIG_IGN);
	    wr_score(top_ten);
	    unlock_sc();
	    signal(SIGINT, fp);
	}
    }
}

/*
 * death:
 *	Do something really fun when he dies
 */

void
death(char monst)
{
    char **dp, *killer;
    struct tm *lt;
    static time_t date;
    struct tm *localtime();

    signal(SIGINT, SIG_IGN);
    purse -= purse / 10;
    signal(SIGINT, leave);
    clear();
    killer = killname(monst, FALSE);
    if (!tombstone)
    {
	mvprintw(LINES - 2, 0, "Killed by ");
	killer = killname(monst, FALSE);
	if (monst != 's' && monst != 'h')
	    printw("a%s ", vowelstr(killer));
	printw("%s with %d gold", killer, purse);
    }
    else
    {
	time(&date);
	lt = localtime(&date);
	move(8, 0);
	dp = rip;
	while (*dp)
	    addstr(*dp++);
	mvaddstr(17, center(killer), killer);
	if (monst == 's' || monst == 'h')
	    mvaddch(16, 32, ' ');
	else
	    mvaddstr(16, 33, vowelstr(killer));
	mvaddstr(14, center(whoami), whoami);
	sprintf(prbuf, "%d Au", purse);
	move(15, center(prbuf));
	addstr(prbuf);
	sprintf(prbuf, "%4d", 1900+lt->tm_year);
	mvaddstr(18, 26, prbuf);
    }
    move(LINES - 1, 0);
    refresh();
    score(purse, amulet ? 3 : 0, monst);
    printf("[Press return to continue]");
    fflush(stdout);
    (void) fgets(prbuf,10,stdin);
    my_exit(0);
}

/*
 * center:
 *	Return the index to center the given string
 */
int
center(char *str)
{
    return 28 - (((int)strlen(str) + 1) / 2);
}

/*
 * total_winner:
 *	Code for a winner
 */

void
total_winner()
{
    THING *obj;
    struct obj_info *op;
    int worth = 0;
    int oldpurse;

    clear();
    standout();
    addstr("                                                               \n");
    addstr("  @   @               @   @           @          @@@  @     @  \n");
    addstr("  @   @               @@ @@           @           @   @     @  \n");
    addstr("  @   @  @@@  @   @   @ @ @  @@@   @@@@  @@@      @  @@@    @  \n");
    addstr("   @@@@ @   @ @   @   @   @     @ @   @ @   @     @   @     @  \n");
    addstr("      @ @   @ @   @   @   @  @@@@ @   @ @@@@@     @   @     @  \n");
    addstr("  @   @ @   @ @  @@   @   @ @   @ @   @ @         @   @  @     \n");
    addstr("   @@@   @@@   @@ @   @   @  @@@@  @@@@  @@@     @@@   @@   @  \n");
    addstr("                                                               \n");
    addstr("     Congratulations, you have made it to the light of day!    \n");
    standend();
    addstr("\nYou have joined the elite ranks of those who have escaped the\n");
    addstr("Dungeons of Doom alive.  You journey home and sell all your loot at\n");
    addstr("a great profit and are admitted to the Fighters' Guild.\n");
    mvaddstr(LINES - 1, 0, "--Press space to continue--");
    refresh();
    wait_for(' ');
    clear();
    mvaddstr(0, 0, "   Worth  Item\n");
    oldpurse = purse;
    for (obj = pack; obj != NULL; obj = next(obj))
    {
	switch (obj->o_type)
	{
	    case FOOD:
		worth = 2 * obj->o_count;
	    when WEAPON:
		worth = weap_info[obj->o_which].oi_worth;
		worth *= 3 * (obj->o_hplus + obj->o_dplus) + obj->o_count;
		obj->o_flags |= ISKNOW;
	    when ARMOR:
		worth = arm_info[obj->o_which].oi_worth;
		worth += (9 - obj->o_arm) * 100;
		worth += (10 * (a_class[obj->o_which] - obj->o_arm));
		obj->o_flags |= ISKNOW;
	    when SCROLL:
		worth = scr_info[obj->o_which].oi_worth;
		worth *= obj->o_count;
		op = &scr_info[obj->o_which];
		if (!op->oi_know)
		    worth /= 2;
		op->oi_know = TRUE;
	    when POTION:
		worth = pot_info[obj->o_which].oi_worth;
		worth *= obj->o_count;
		op = &pot_info[obj->o_which];
		if (!op->oi_know)
		    worth /= 2;
		op->oi_know = TRUE;
	    when RING:
		op = &ring_info[obj->o_which];
		worth = op->oi_worth;
		if (obj->o_which == R_ADDSTR || obj->o_which == R_ADDDAM ||
		    obj->o_which == R_PROTECT || obj->o_which == R_ADDHIT)
		{
			if (obj->o_arm > 0)
			    worth += obj->o_arm * 100;
			else
			    worth = 10;
		}
		if (!(obj->o_flags & ISKNOW))
		    worth /= 2;
		obj->o_flags |= ISKNOW;
		op->oi_know = TRUE;
	    when STICK:
		op = &ws_info[obj->o_which];
		worth = op->oi_worth;
		worth += 20 * obj->o_charges;
		if (!(obj->o_flags & ISKNOW))
		    worth /= 2;
		obj->o_flags |= ISKNOW;
		op->oi_know = TRUE;
	    when AMULET:
		worth = 1000;
	}
	if (worth < 0)
	    worth = 0;
	printw("%c) %5d  %s\n", obj->o_packch, worth, inv_name(obj, FALSE));
	purse += worth;
    }
    printw("   %5d  Gold Pieces          ", oldpurse);
    refresh();
    score(purse, 2, ' ');
    my_exit(0);
}

/*
 * killname:
 *	Convert a code to a monster name
 */
char *
killname(char monst, bool doart)
{
    struct h_list *hp;
    char *sp;
    bool article;
    static struct h_list nlist[] = {
	{'a',	"arrow",		TRUE},
	{'b',	"bolt",			TRUE},
	{'d',	"dart",			TRUE},
	{'h',	"hypothermia",		FALSE},
	{'s',	"starvation",		FALSE},
	{'\0'}
    };

    if (isupper(monst))
    {
	sp = monsters[monst-'A'].m_name;
	article = TRUE;
    }
    else
    {
	sp = "Wally the Wonder Badger";
	article = FALSE;
	for (hp = nlist; hp->h_ch; hp++)
	    if (hp->h_ch == monst)
	    {
		sp = hp->h_desc;
		article = hp->h_print;
		break;
	    }
    }
    if (doart && article)
	sprintf(prbuf, "a%s ", vowelstr(sp));
    else
	prbuf[0] = '\0';
    strcat(prbuf, sp);
    return prbuf;
}

/*
 * death_monst:
 *	Return a monster appropriate for a random death.
 */
char
death_monst()
{
    static char poss[] =
    {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
	'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'h', 'd', 's',
	' '	/* This is provided to generate the "Wally the Wonder Badger"
		   message for killer */
    };

    return poss[rnd(sizeof poss / sizeof (char))];
}
