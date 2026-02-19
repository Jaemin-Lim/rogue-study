/*
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 *
 * @(#)main.c	4.22 (Berkeley) 02/05/99
 */

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <curses.h>
#include "rogue.h"

/*
 * main:
 *	The main program, of course
 *	프로그램의 진입점(엔트리 포인트) 함수.
 *	초기화를 수행하고 게임 루프를 시작한다.
 *	- md_init(): 플랫폼별 초기화 (mdport.c 참조)
 *	- MASTER 모드에서 argv[1]이 빈 문자열이면 마법사 비밀번호 입력 시도
 *	- 홈 디렉토리와 환경 변수(ROGUEOPTS) 설정
 *	- 난수 시드 초기화 (현재 시간 + 프로세스 ID 사용)
 *	- 점수 파일 열기 후 권한 낮추기 (setuid/setgid 해제)
 *	- -s: 점수판 출력 후 종료
 *	- -d: 사망 화면 테스트 후 종료
 *	- 저장 파일이 있으면 복원, 없으면 새 게임 시작
 *	- 게임 객체 초기화 (방, 플레이어, 아이템 이름 등)
 *	- playit() 호출로 메인 게임 루프 시작
 */
int
main(int argc, char **argv, char **envp)
{
    char *env;    /* 환경 변수 값 포인터 */
    int lowtime;  /* 현재 시간 (초 단위) - 난수 시드용 */

    md_init();  /* 플랫폼별 초기화 (mdport.c 참조) */

#ifdef MASTER
    /*
     * Check to see if he is a wizard
     * 마법사 모드 확인: 첫 번째 인수가 빈 문자열이면 비밀번호 입력 요구
     */
    if (argc >= 2 && argv[1][0] == '\0')
	if (strcmp(PASSWD, md_crypt(md_getpass("wizard's password: "), "mT")) == 0)
	{
	    wizard = TRUE;
	    player.t_flags |= SEEMONST;  /* 모든 몬스터 감지 활성화 */
	    argv++;
	    argc--;
	}

#endif

    /*
     * get home and options from environment
     * 환경 변수에서 홈 디렉토리와 게임 옵션 설정
     */

    strncpy(home, md_gethomedir(), MAXSTR);  /* 홈 디렉토리 경로 저장 */

    /* 기본 저장 파일 경로: ~/rogue.save */
    strcpy(file_name, home);
    strcat(file_name, "rogue.save");

    /* ROGUEOPTS 환경 변수에서 게임 옵션 파싱 */
    if ((env = getenv("ROGUEOPTS")) != NULL)
	parse_opts(env);
    /* 사용자 이름이 설정되지 않았으면 시스템에서 가져오기 */
    if (env == NULL || whoami[0] == '\0')
        strucpy(whoami, md_getusername(), (int) strlen(md_getusername()));
    lowtime = (int) time(NULL);  /* 현재 시간 (초) 획득 */
#ifdef MASTER
    /* SEED 환경 변수가 있으면 지정된 시드 사용 (재현 가능한 게임용) */
    if (wizard && getenv("SEED") != NULL)
	dnum = atoi(getenv("SEED"));
    else
#endif
	dnum = lowtime + md_getpid();  /* 시간 + PID로 난수 시드 생성 */
    seed = dnum;

    open_score();  /* 점수 파일 열기 (score.c 참조) */

	/* 
     * Drop setuid/setgid after opening the scoreboard file. 
     * 점수 파일을 연 후 setuid/setgid 권한을 일반 사용자 권한으로 낮춤
     */ 

    md_normaluser();

    /*
     * check for print-score option
     * -s 옵션: 점수판 출력 후 종료
     */

	md_normaluser(); /* we drop any setgid/setuid priveldges here */

    if (argc == 2)
    {
	if (strcmp(argv[1], "-s") == 0)
	{
	    noscore = TRUE;
	    score(0, -1, 0);  /* 현재 점수 출력 */
	    exit(0);
	}
	else if (strcmp(argv[1], "-d") == 0)
	{
	    /* -d 옵션: 사망 화면 테스트 */
	    dnum = rnd(100);	/* throw away some rnd()s to break patterns */
                            /* 패턴 방지를 위해 일부 난수 버리기 */
	    while (--dnum)
		rnd(100);
	    purse = rnd(100) + 1;  /* 무작위 금액 설정 */
	    level = rnd(100) + 1;  /* 무작위 레벨 설정 */
	    initscr();
	    getltchars();
	    death(death_monst());  /* 무작위 몬스터에 의한 사망 화면 표시 */
	    exit(0);
	}
    }

    init_check();			/* check for legal startup */
                            /* 합법적인 시작 조건 확인 */
    if (argc == 2)
	if (!restore(argv[1], envp))	/* Note: restore will never return */
                                    /* 저장 파일 복원 (성공 시 복귀하지 않음) */
	    my_exit(1);
#ifdef MASTER
    if (wizard)
	printf("Hello %s, welcome to dungeon #%d", whoami, dnum);
    else
#endif
	printf("Hello %s, just a moment while I dig the dungeon...", whoami);
    fflush(stdout);

    initscr();				/* Start up cursor package */
                            /* curses 라이브러리 초기화 */
    init_probs();			/* Set up prob tables for objects */
                            /* 아이템 출현 확률 테이블 초기화 (init.c 참조) */
    init_player();			/* Set up initial player stats */
                            /* 플레이어 초기 스탯 및 초기 아이템 설정 (init.c 참조) */
    init_names();			/* Set up names of scrolls */
                            /* 스크롤 랜덤 이름 생성 (init.c 참조) */
    init_colors();			/* Set up colors of potions */
                            /* 포션 랜덤 색상 설정 (init.c 참조) */
    init_stones();			/* Set up stone settings of rings */
                            /* 반지의 랜덤 보석 설정 (init.c 참조) */
    init_materials();			/* Set up materials of wands */
                            /* 지팡이의 랜덤 재질 설정 (init.c 참조) */
    setup();  /* 터미널 신호 핸들러 등 설정 */

    /*
     * The screen must be at least NUMLINES x NUMCOLS
     * 화면 크기가 최소 NUMLINES x NUMCOLS(24x80) 이상이어야 함
     */
    if (LINES < NUMLINES || COLS < NUMCOLS)
    {
	printf("\nSorry, the screen must be at least %dx%d\n", NUMLINES, NUMCOLS);
	endwin();
	my_exit(1);
    }

    /*
     * Set up windows
     * 화면 창(window) 설정
     */
    hw = newwin(LINES, COLS, 0, 0);  /* 보조 창(hidden window) 생성 */
    idlok(stdscr, TRUE);  /* 삽입/삭제 최적화 활성화 */
    idlok(hw, TRUE);
#ifdef MASTER
    noscore = wizard;  /* 마법사 모드면 점수 저장 안 함 */
#endif
    new_level();			/* Draw current level */
                            /* 첫 번째 레벨 생성 및 화면 출력 (new_level.c 참조) */
    /*
     * Start up daemons and fuses
     * 주기적 이벤트(데몬)와 타이머 이벤트(퓨즈) 시작
     */
    start_daemon(runners, 0, AFTER);   /* 몬스터 이동 데몬 (chase.c의 runners 함수) */
    start_daemon(doctor, 0, AFTER);    /* 체력 회복 데몬 (daemons.c의 doctor 함수) */
    fuse(swander, 0, WANDERTIME, AFTER); /* 방랑 몬스터 생성 타이머 (daemons.c 참조) */
    start_daemon(stomach, 0, AFTER);   /* 배고픔 데몬 (daemons.c의 stomach 함수) */
    playit();  /* 메인 게임 루프 시작 */
    return(0);
}

/*
 * endit:
 *	Exit the program abnormally.
 *	프로그램을 비정상적으로 종료하는 함수.
 *	주로 인터럽트 신호(SIGINT) 핸들러로 사용된다.
 */

void
endit(int sig)
{
    NOOP(sig);  /* 미사용 매개변수 경고 억제 */
    fatal("Okay, bye bye!\n");
}

/*
 * fatal:
 *	Exit the program, printing a message.
 *	메시지를 출력하고 프로그램을 종료하는 함수.
 *	curses 모드를 종료하고 화면 하단에 메시지를 출력한다.
 */

void
fatal(char *s)
{
    mvaddstr(LINES - 2, 0, s);  /* 화면 하단에 메시지 출력 */
    refresh();
    endwin();  /* curses 모드 종료 */
    my_exit(0);
}

/*
 * rnd:
 *	Pick a very random number.
 *	0부터 range-1까지의 무작위 정수를 반환하는 함수.
 *	range가 0이면 0을 반환한다.
 *	RN은 extern.h에 정의된 난수 생성 매크로.
 */
int
rnd(int range)
{
    return range == 0 ? 0 : abs((int) RN) % range;
}

/*
 * roll:
 *	Roll a number of dice
 *	주사위를 굴리는 함수.
 *	number개의 sides면 주사위를 굴려 합계를 반환한다.
 *	예: roll(2, 6) = 2d6 (2개의 6면 주사위)
 */
int 
roll(int number, int sides)
{
    int dtotal = 0;  /* 주사위 합계 */

    while (number--)
	dtotal += rnd(sides)+1;  /* 1부터 sides까지의 값 */
    return dtotal;
}

/*
 * tstp:
 *	Handle stop and start signals
 *	Ctrl+Z(SIGTSTP) 신호 처리 함수.
 *	게임을 일시 정지하고 쉘로 전환했다가 복귀할 때 화면을 복원한다.
 */

void
tstp(int ignored)
{
    int y, x;    /* 현재 커서 위치 (복귀 후 복원용) */
    int oy, ox;  /* 이전 커서 위치 */

	NOOP(ignored);  /* 미사용 매개변수 경고 억제 */

    /*
     * leave nicely
     * 터미널을 정상 모드로 복원하고 일시 정지
     */
    getyx(curscr, oy, ox);  /* 현재 커서 위치 저장 */
    mvcur(0, COLS - 1, LINES - 1, 0);  /* 커서를 화면 좌하단으로 이동 */
    endwin();  /* curses 모드 종료 */
    resetltchars();  /* 터미널 특수 문자 복원 */
    fflush(stdout);
	md_tstpsignal();  /* SIGTSTP 신호 전송하여 프로세스 일시 정지 */

    /*
     * start back up again
     * 프로세스가 재개되면 curses 모드로 복귀
     */
	md_tstpresume();
    raw();      /* raw 입력 모드 활성화 */
    noecho();   /* 에코 비활성화 */
    keypad(stdscr,1);  /* 키패드 입력 활성화 */
    playltchars();  /* 게임용 특수 문자 설정 */
    clearok(curscr, TRUE);  /* 다음 refresh 시 화면 전체 재출력 */
    wrefresh(curscr);
    getyx(curscr, y, x);
    mvcur(y, x, oy, ox);  /* 이전 커서 위치로 복귀 */
    fflush(stdout);
    curscr->_cury = oy;
    curscr->_curx = ox;
}

/*
 * playit:
 *	The main loop of the program.  Loop until the game is over,
 *	refreshing things and looking at the proper times.
 *	게임의 메인 루프 함수.
 *	playing 변수가 FALSE가 될 때까지 command() 함수를 반복 호출한다.
 *	느린 터미널(1200bps 이하)에서는 간결 모드와 점프 모드를 활성화한다.
 */

void
playit()
{
    char *opts;  /* 환경 변수 옵션 포인터 */

    /*
     * set up defaults for slow terminals
     * 느린 터미널을 위한 기본값 설정
     */

    if (baudrate() <= 1200)  /* 터미널 속도가 1200bps 이하이면 */
    {
	terse = TRUE;     /* 간결 메시지 모드 */
	jump = TRUE;      /* 이동 시 화면 갱신 생략 */
	see_floor = FALSE; /* 바닥 문자 표시 안 함 */
    }

    if (md_hasclreol())  /* 터미널이 줄 끝 지우기를 지원하면 */
	inv_type = INV_CLEAR;  /* 인벤토리 표시 방식을 지우기 방식으로 설정 */

    /*
     * parse environment declaration of options
     * 환경 변수에서 옵션 재파싱
     */
    if ((opts = getenv("ROGUEOPTS")) != NULL)
	parse_opts(opts);


    oldpos = hero;       /* 이전 위치를 현재 위치로 초기화 */
    oldrp = roomin(&hero); /* 이전 방을 현재 방으로 초기화 */
    while (playing)
	command();			/* Command execution */
                        /* 명령 실행 루프 (command.c 참조) */
    endit(0);  /* 게임 종료 */
}

/*
 * quit:
 *	Have player make certain, then exit.
 *	플레이어에게 종료 의사를 확인하고 게임을 종료하는 함수.
 *	'y'를 입력하면 점수를 저장하고 종료, 다른 키를 입력하면 취소.
 */

void
quit(int sig)
{
    int oy, ox;  /* 현재 커서 위치 저장용 */

    NOOP(sig);

    /*
     * Reset the signal in case we got here via an interrupt
     * 인터럽트로 여기 도달한 경우를 위해 메시지 위치 초기화
     */
    if (!q_comm)
	mpos = 0;
    getyx(curscr, oy, ox);
    msg("really quit?");
    if (readchar() == 'y')  /* 'y' 입력 시 종료 */
    {
	signal(SIGINT, leave);  /* SIGINT 시 leave() 함수 호출 */
	clear();
	mvprintw(LINES - 2, 0, "You quit with %d gold pieces", purse);
	move(LINES - 1, 0);
	refresh();
	score(purse, 1, 0);  /* 종료 점수 저장 (score.c 참조) */
	my_exit(0);
    }
    else  /* 다른 키 입력 시 취소 */
    {
	move(0, 0);
	clrtoeol();
	status();   /* 상태 표시줄 갱신 */
	move(oy, ox);
	refresh();
	mpos = 0;
	count = 0;
	to_death = FALSE;
    }
}

/*
 * leave:
 *	Leave quickly, but curteously
 *	빠르지만 정중하게 프로그램을 종료하는 함수.
 *	주로 SIGINT 신호 처리에 사용된다.
 *	표준 출력 버퍼를 비워 대기 중인 출력을 버린다.
 */

void
leave(int sig)
{
    static char buf[BUFSIZ];  /* 정적 버퍼 (스택 오버플로우 방지) */

    NOOP(sig);

    setbuf(stdout, buf);	/* throw away pending output */
                            /* 대기 중인 출력 버리기 */

    if (!isendwin())  /* curses가 아직 종료되지 않았으면 */
    {
	mvcur(0, COLS - 1, LINES - 1, 0);  /* 커서를 화면 좌하단으로 */
	endwin();
    }

    putchar('\n');
    my_exit(0);
}

/*
 * shell:
 *	Let them escape for a while
 *	게임을 일시 중단하고 쉘 명령을 실행할 수 있게 하는 함수.
 *	'!' 명령으로 호출된다.
 *	md_shellescape()로 서브 쉘을 실행하고 복귀 후 화면을 재출력한다.
 */

void
shell()
{
    /*
     * Set the terminal back to original mode
     * 터미널을 원래 모드로 복원
     */
    move(LINES-1, 0);
    refresh();
    endwin();
    resetltchars();
    putchar('\n');
    in_shell = TRUE;  /* 쉘 실행 중 플래그 설정 */
    after = FALSE;
    fflush(stdout);
    /*
     * Fork and do a shell
     * 서브 쉘 실행 (md_shellescape는 mdport.c에서 플랫폼별 구현)
     */
    md_shellescape();

    printf("\n[Press return to continue]");
    fflush(stdout);
    noecho();
    raw();
    keypad(stdscr,1);
    playltchars();
    in_shell = FALSE;  /* 쉘 종료 플래그 해제 */
    wait_for('\n');    /* Enter 키 대기 */
    clearok(stdscr, TRUE);  /* 화면 전체 재출력 */
}

/*
 * my_exit:
 *	Leave the process properly
 *	프로세스를 올바르게 종료하는 함수.
 *	터미널 특수 문자를 복원하고 exit()를 호출한다.
 */

void
my_exit(int st)
{
    resetltchars();  /* 터미널 특수 문자 복원 */
    exit(st);
}


#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <curses.h>
#include "rogue.h"

/*
 * main:
 *	The main program, of course
 */
int
main(int argc, char **argv, char **envp)
{
    char *env;
    int lowtime;

    md_init();

#ifdef MASTER
    /*
     * Check to see if he is a wizard
     */
    if (argc >= 2 && argv[1][0] == '\0')
	if (strcmp(PASSWD, md_crypt(md_getpass("wizard's password: "), "mT")) == 0)
	{
	    wizard = TRUE;
	    player.t_flags |= SEEMONST;
	    argv++;
	    argc--;
	}

#endif

    /*
     * get home and options from environment
     */

    strncpy(home, md_gethomedir(), MAXSTR);

    strcpy(file_name, home);
    strcat(file_name, "rogue.save");

    if ((env = getenv("ROGUEOPTS")) != NULL)
	parse_opts(env);
    if (env == NULL || whoami[0] == '\0')
        strucpy(whoami, md_getusername(), (int) strlen(md_getusername()));
    lowtime = (int) time(NULL);
#ifdef MASTER
    if (wizard && getenv("SEED") != NULL)
	dnum = atoi(getenv("SEED"));
    else
#endif
	dnum = lowtime + md_getpid();
    seed = dnum;

    open_score();

	/* 
     * Drop setuid/setgid after opening the scoreboard file. 
     */ 

    md_normaluser();

    /*
     * check for print-score option
     */

	md_normaluser(); /* we drop any setgid/setuid priveldges here */

    if (argc == 2)
    {
	if (strcmp(argv[1], "-s") == 0)
	{
	    noscore = TRUE;
	    score(0, -1, 0);
	    exit(0);
	}
	else if (strcmp(argv[1], "-d") == 0)
	{
	    dnum = rnd(100);	/* throw away some rnd()s to break patterns */
	    while (--dnum)
		rnd(100);
	    purse = rnd(100) + 1;
	    level = rnd(100) + 1;
	    initscr();
	    getltchars();
	    death(death_monst());
	    exit(0);
	}
    }

    init_check();			/* check for legal startup */
    if (argc == 2)
	if (!restore(argv[1], envp))	/* Note: restore will never return */
	    my_exit(1);
#ifdef MASTER
    if (wizard)
	printf("Hello %s, welcome to dungeon #%d", whoami, dnum);
    else
#endif
	printf("Hello %s, just a moment while I dig the dungeon...", whoami);
    fflush(stdout);

    initscr();				/* Start up cursor package */
    init_probs();			/* Set up prob tables for objects */
    init_player();			/* Set up initial player stats */
    init_names();			/* Set up names of scrolls */
    init_colors();			/* Set up colors of potions */
    init_stones();			/* Set up stone settings of rings */
    init_materials();			/* Set up materials of wands */
    setup();

    /*
     * The screen must be at least NUMLINES x NUMCOLS
     */
    if (LINES < NUMLINES || COLS < NUMCOLS)
    {
	printf("\nSorry, the screen must be at least %dx%d\n", NUMLINES, NUMCOLS);
	endwin();
	my_exit(1);
    }

    /*
     * Set up windows
     */
    hw = newwin(LINES, COLS, 0, 0);
    idlok(stdscr, TRUE);
    idlok(hw, TRUE);
#ifdef MASTER
    noscore = wizard;
#endif
    new_level();			/* Draw current level */
    /*
     * Start up daemons and fuses
     */
    start_daemon(runners, 0, AFTER);
    start_daemon(doctor, 0, AFTER);
    fuse(swander, 0, WANDERTIME, AFTER);
    start_daemon(stomach, 0, AFTER);
    playit();
    return(0);
}

/*
 * endit:
 *	Exit the program abnormally.
 */

void
endit(int sig)
{
    NOOP(sig);
    fatal("Okay, bye bye!\n");
}

/*
 * fatal:
 *	Exit the program, printing a message.
 */

void
fatal(char *s)
{
    mvaddstr(LINES - 2, 0, s);
    refresh();
    endwin();
    my_exit(0);
}

/*
 * rnd:
 *	Pick a very random number.
 */
int
rnd(int range)
{
    return range == 0 ? 0 : abs((int) RN) % range;
}

/*
 * roll:
 *	Roll a number of dice
 */
int 
roll(int number, int sides)
{
    int dtotal = 0;

    while (number--)
	dtotal += rnd(sides)+1;
    return dtotal;
}

/*
 * tstp:
 *	Handle stop and start signals
 */

void
tstp(int ignored)
{
    int y, x;
    int oy, ox;

	NOOP(ignored);

    /*
     * leave nicely
     */
    getyx(curscr, oy, ox);
    mvcur(0, COLS - 1, LINES - 1, 0);
    endwin();
    resetltchars();
    fflush(stdout);
	md_tstpsignal();

    /*
     * start back up again
     */
	md_tstpresume();
    raw();
    noecho();
    keypad(stdscr,1);
    playltchars();
    clearok(curscr, TRUE);
    wrefresh(curscr);
    getyx(curscr, y, x);
    mvcur(y, x, oy, ox);
    fflush(stdout);
    curscr->_cury = oy;
    curscr->_curx = ox;
}

/*
 * playit:
 *	The main loop of the program.  Loop until the game is over,
 *	refreshing things and looking at the proper times.
 */

void
playit()
{
    char *opts;

    /*
     * set up defaults for slow terminals
     */

    if (baudrate() <= 1200)
    {
	terse = TRUE;
	jump = TRUE;
	see_floor = FALSE;
    }

    if (md_hasclreol())
	inv_type = INV_CLEAR;

    /*
     * parse environment declaration of options
     */
    if ((opts = getenv("ROGUEOPTS")) != NULL)
	parse_opts(opts);


    oldpos = hero;
    oldrp = roomin(&hero);
    while (playing)
	command();			/* Command execution */
    endit(0);
}

/*
 * quit:
 *	Have player make certain, then exit.
 */

void
quit(int sig)
{
    int oy, ox;

    NOOP(sig);

    /*
     * Reset the signal in case we got here via an interrupt
     */
    if (!q_comm)
	mpos = 0;
    getyx(curscr, oy, ox);
    msg("really quit?");
    if (readchar() == 'y')
    {
	signal(SIGINT, leave);
	clear();
	mvprintw(LINES - 2, 0, "You quit with %d gold pieces", purse);
	move(LINES - 1, 0);
	refresh();
	score(purse, 1, 0);
	my_exit(0);
    }
    else
    {
	move(0, 0);
	clrtoeol();
	status();
	move(oy, ox);
	refresh();
	mpos = 0;
	count = 0;
	to_death = FALSE;
    }
}

/*
 * leave:
 *	Leave quickly, but curteously
 */

void
leave(int sig)
{
    static char buf[BUFSIZ];

    NOOP(sig);

    setbuf(stdout, buf);	/* throw away pending output */

    if (!isendwin())
    {
	mvcur(0, COLS - 1, LINES - 1, 0);
	endwin();
    }

    putchar('\n');
    my_exit(0);
}

/*
 * shell:
 *	Let them escape for a while
 */

void
shell()
{
    /*
     * Set the terminal back to original mode
     */
    move(LINES-1, 0);
    refresh();
    endwin();
    resetltchars();
    putchar('\n');
    in_shell = TRUE;
    after = FALSE;
    fflush(stdout);
    /*
     * Fork and do a shell
     */
    md_shellescape();

    printf("\n[Press return to continue]");
    fflush(stdout);
    noecho();
    raw();
    keypad(stdscr,1);
    playltchars();
    in_shell = FALSE;
    wait_for('\n');
    clearok(stdscr, TRUE);
}

/*
 * my_exit:
 *	Leave the process properly
 */

void
my_exit(int st)
{
    resetltchars();
    exit(st);
}

