/*
 * save and restore routines
 *
 * @(#)save.c	4.33 (Berkeley) 06/01/83
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

/*
 * [파일 개요]
 * 이 파일은 게임의 저장(save)과 복원(restore) 기능을 담당합니다.
 * 주요 기능:
 *   - save_game()  : 플레이어가 명시적으로 게임을 저장하는 명령 처리
 *   - auto_save()  : HUP 시그널(터미널 종료 등) 수신 시 자동 저장
 *   - save_file()  : 실제 파일에 게임 상태를 기록하고 종료
 *   - restore()    : 저장 파일에서 게임을 복원하며 무결성을 검증
 *   - encwrite()   : 간단한 XOR 기반 암호화로 데이터를 파일에 기록
 *   - encread()    : 암호화된 데이터를 파일에서 읽어 복호화
 *   - rd_score()   : 점수 파일(scoreboard)에서 상위 10위 점수 읽기
 *   - wr_score()   : 점수 파일에 상위 10위 점수 기록
 *
 * 저장 파일 형식:
 *   1) 버전 문자열 (암호화)
 *   2) 화면 크기 "LINES x COLS\n" (암호화)
 *   3) rs_save_file()이 기록하는 게임 상태 데이터
 *
 * 보안 검사 (restore 시):
 *   - 버전 불일치 거부
 *   - 화면 크기 불일치 거부
 *   - 하드 링크 / 심볼릭 링크로부터 복원 거부 (치트 방지)
 *   - HP <= 0인 채로 저장된 파일 거부
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <curses.h>
#include "rogue.h"
#include "score.h"

typedef struct stat STAT;

extern char version[], encstr[];

static STAT sbuf;

/*
 * save_game:
 *	Implement the "save game" command
 *
 * [한국어] 플레이어가 'S' 명령으로 게임을 저장할 때 호출된다.
 *   - 기존 file_name이 있으면 덮어쓸지 물어본다.
 *   - 파일명을 새로 입력받고, 이미 존재하는 파일이면 덮어쓰기를 확인한다.
 *   - fopen()으로 파일을 열고 save_file()에 넘긴다.
 *   - save_file()은 저장 후 exit(0)으로 게임을 종료한다.
 */

void
save_game()
{
    FILE *savef;     /* 저장 파일 포인터 */
    int c;           /* 사용자 입력 문자 */
    auto char buf[MAXSTR];  /* 파일명 입력 버퍼 */

    /*
     * get file name
     * [한국어] 저장 파일명을 결정한다.
     *   기존 file_name이 있으면 재사용 여부를 물어보고,
     *   없거나 'N'을 선택하면 새 파일명을 입력받는다.
     */
    mpos = 0;
over:
    if (file_name[0] != '\0')
    {
	for (;;)
	{
	    msg("save file (%s)? ", file_name);
	    c = readchar();
	    mpos = 0;
	    if (c == ESCAPE)
	    {
		msg("");
		return;
	    }
	    else if (c == 'n' || c == 'N' || c == 'y' || c == 'Y')
		break;
	    else
		msg("please answer Y or N");
	}
	if (c == 'y' || c == 'Y')
	{
	    addstr("Yes\n");
	    refresh();
	    strcpy(buf, file_name);
	    goto gotfile;
	}
    }

    do
    {
	mpos = 0;
	msg("file name: ");
	buf[0] = '\0';
	if (get_str(buf, stdscr) == QUIT)
	{
quit_it:
	    msg("");
	    return;
	}
	mpos = 0;
gotfile:
	/*
	 * test to see if the file exists
	 */
	if (stat(buf, &sbuf) >= 0)
	{
	    for (;;)
	    {
		msg("File exists.  Do you wish to overwrite it?");
		mpos = 0;
		if ((c = readchar()) == ESCAPE)
		    goto quit_it;
		if (c == 'y' || c == 'Y')
		    break;
		else if (c == 'n' || c == 'N')
		    goto over;
		else
		    msg("Please answer Y or N");
	    }
	    msg("file name: %s", buf);
	    md_unlink(file_name);
	}
	strcpy(file_name, buf);
	if ((savef = fopen(file_name, "w")) == NULL)
	    msg(strerror(errno));
    } while (savef == NULL);

    save_file(savef);
    /* NOTREACHED */
}

/*
 * auto_save:
 *	Automatically save a file.  This is used if a HUP signal is
 *	recieved
 *
 * [한국어] SIGHUP(터미널 연결 끊김 등) 수신 시 자동으로 게임을 저장한다.
 *   - 모든 시그널을 무시(md_ignoreallsignals)한 후 저장을 시도한다.
 *   - file_name이 설정되어 있고 파일을 열 수 있으면 save_file()을 호출한다.
 *   - 저장 성공 여부와 관계없이 exit(0)으로 종료한다.
 *   - sig 매개변수는 사용하지 않음(NOOP으로 처리)
 */

void
auto_save(int sig)  /* sig: 수신된 시그널 번호 (사용하지 않음) */
{
    FILE *savef;
    NOOP(sig);

    md_ignoreallsignals();
    if (file_name[0] != '\0' && ((savef = fopen(file_name, "w")) != NULL ||
	(md_unlink_open_file(file_name, savef) >= 0 && (savef = fopen(file_name, "w")) != NULL)))
	    save_file(savef);
    exit(0);
}

/*
 * save_file:
 *	Write the saved game on the file
 *
 * [한국어] 열린 파일 포인터(savef)에 게임 상태 전체를 기록하고 프로세스를 종료한다.
 *   저장 순서:
 *     1) 커서를 화면 마지막 줄로 이동하고 curses 종료
 *     2) 파일 권한을 0400(읽기 전용)으로 설정
 *     3) 버전 문자열 암호화 기록
 *     4) 화면 크기("LINES x COLS\n") 암호화 기록
 *     5) rs_save_file()로 나머지 게임 상태 기록
 *     6) fflush/fclose 후 exit(0)
 */

void
save_file(FILE *savef)  /* savef: 쓰기 모드로 열린 저장 파일 포인터 */
{
    char buf[80];
    mvcur(0, COLS - 1, LINES - 1, 0); 
    putchar('\n');
    endwin();
    resetltchars();
    md_chmod(file_name, 0400);
    encwrite(version, strlen(version)+1, savef);
    sprintf(buf,"%d x %d\n", LINES, COLS);
    encwrite(buf,80,savef);
    rs_save_file(savef);
    fflush(savef);
    fclose(savef);
    exit(0);
}

/*
 * restore:
 *	Restore a saved game from a file with elaborate checks for file
 *	integrity from cheaters
 *
 * [한국어] 저장 파일에서 게임을 복원한다. 치트 방지를 위한 여러 검증을 수행한다.
 *   - file: 복원할 파일 경로 ("-r"이면 전역 file_name 사용)
 *   - envp: 환경 변수 배열 (복원 후 environ에 설정)
 *   검증 단계:
 *     1) 파일 열기 실패 시 FALSE 반환
 *     2) 버전 문자열 불일치 시 거부
 *     3) 저장 당시보다 화면이 작으면 거부 (LINES, COLS 검사)
 *     4) rs_restore_file()로 게임 상태 복원
 *     5) 하드 링크(st_nlink != 1) 또는 심볼릭 링크에서 복원 거부
 *     6) HP <= 0이면 거부 ("He's dead, Jim")
 *     7) 성공 시 playit()으로 게임 재개
 */
bool
restore(char *file, char **envp)  /* file: 저장 파일 경로, envp: 환경 변수 배열 */
{
    FILE *inf;           /* 저장 파일 읽기 포인터 */
    int syml;            /* 심볼릭 링크 여부 */
    extern char **environ;
    auto char buf[MAXSTR]; /* 버전/크기 문자열 읽기 버퍼 */
    auto STAT sbuf2;     /* 저장 파일의 stat 정보 */
    int lines, cols;     /* 저장 당시의 화면 크기 */

    if (strcmp(file, "-r") == 0)
	file = file_name;

	md_tstphold();

	if ((inf = fopen(file,"r")) == NULL)
    {
	perror(file);
	return FALSE;
    }
    stat(file, &sbuf2);
    syml = is_symlink(file);

    fflush(stdout);
    encread(buf, (unsigned) strlen(version) + 1, inf);
    if (strcmp(buf, version) != 0)
    {
	printf("Sorry, saved game is out of date.\n");
	return FALSE;
    }
    encread(buf,80,inf);
    sscanf(buf,"%d x %d\n", &lines, &cols);

    initscr();                          /* Start up cursor package */
    keypad(stdscr, 1);

    if (lines > LINES)
    {
        endwin();
        printf("Sorry, original game was played on a screen with %d lines.\n",lines);
        printf("Current screen only has %d lines. Unable to restore game\n",LINES);
        return(FALSE);
    }
    if (cols > COLS)
    {
        endwin();
        printf("Sorry, original game was played on a screen with %d columns.\n",cols);
        printf("Current screen only has %d columns. Unable to restore game\n",COLS);
        return(FALSE);
    }

    hw = newwin(LINES, COLS, 0, 0);
    setup();

    rs_restore_file(inf);
    /*
     * we do not close the file so that we will have a hold of the
     * inode for as long as possible
     * [한국어] 파일을 닫지 않고 inode를 점유한 채로 유지한다.
     *   이렇게 하면 복원 직후 다른 프로세스가 같은 저장 파일로
     *   게임을 재시작하는 것을 어렵게 만든다.
     */

    if (
#ifdef MASTER
	!wizard &&
#endif
        md_unlink_open_file(file, inf) < 0)
    {
	printf("Cannot unlink file\n");
	return FALSE;
    }
    mpos = 0;
/*    printw(0, 0, "%s: %s", file, ctime(&sbuf2.st_mtime)); */
/*
    printw("%s: %s", file, ctime(&sbuf2.st_mtime));
*/
    clearok(stdscr,TRUE);
    /*
     * defeat multiple restarting from the same place
     * [한국어] 동일 저장 파일로 여러 번 재시작하는 치트를 막는다.
     *   하드 링크(st_nlink != 1) 또는 심볼릭 링크이면 복원을 거부한다.
     */
#ifdef MASTER
    if (!wizard)
#endif
	if (sbuf2.st_nlink != 1 || syml)
	{
	    endwin();
	    printf("\nCannot restore from a linked file\n");
	    return FALSE;
	}

    if (pstats.s_hpt <= 0)
    {
	endwin();
	printf("\n\"He's dead, Jim\"\n");
	return FALSE;
    }

	md_tstpresume();

    environ = envp;
    strcpy(file_name, file);
    clearok(curscr, TRUE);
    srand(md_getpid());
    msg("file name: %s", file);
    playit();
    /*NOTREACHED*/
    return(0);
}

/*
 * encwrite:
 *	Perform an encrypted write
 *
 * [한국어] XOR 기반 스트림 암호화를 적용하여 데이터를 파일에 기록한다.
 *   - start: 기록할 원본 데이터 버퍼
 *   - size: 기록할 바이트 수
 *   - outf: 출력 파일 포인터
 *   암호화 방식:
 *     - e1은 encstr, e2는 statlist를 순환하는 키 스트림 포인터
 *     - fb는 이전 키 바이트들의 곱 누산으로 갱신되는 피드백 값
 *     - 각 바이트에 *e1 ^ *e2 ^ fb를 XOR하여 암호화
 *   - 반환값: 실제로 기록된 바이트 수
 */

size_t
encwrite(char *start, size_t size, FILE *outf)
{
    char *e1, *e2, fb;  /* e1: encstr 키 포인터, e2: statlist 키 포인터, fb: 피드백 값 */
    int temp;           /* 키 바이트 임시 저장 */
    extern char statlist[];
    size_t o_size = size;  /* 원래 요청 크기 (반환값 계산용) */
    e1 = encstr;
    e2 = statlist;
    fb = 0;

    while(size)
    {
	if (putc(*start++ ^ *e1 ^ *e2 ^ fb, outf) == EOF)
            break;

	temp = *e1++;
	fb = fb + ((char) (temp * *e2++));
	if (*e1 == '\0')
	    e1 = encstr;
	if (*e2 == '\0')
	    e2 = statlist;
	size--;
    }

    return(o_size - size);
}

/*
 * encread:
 *	Perform an encrypted read
 *
 * [한국어] 파일에서 데이터를 읽고 encwrite()와 동일한 XOR 키 스트림으로 복호화한다.
 *   - start: 복호화된 데이터를 저장할 버퍼
 *   - size: 읽을 바이트 수
 *   - inf: 입력 파일 포인터
 *   복호화 방식: encwrite()와 동일한 키 스트림(e1, e2, fb)을 사용하여
 *   읽은 각 바이트에 XOR 적용 (XOR의 대칭성으로 복호화)
 *   - 반환값: 실제로 읽은 바이트 수 (fread 반환값)
 */
size_t
encread(char *start, size_t size, FILE *inf)
{
    char *e1, *e2, fb;  /* encwrite()와 동일한 키 스트림 변수 */
    int temp;
    size_t read_size;   /* fread()가 실제로 읽은 바이트 수 */
    extern char statlist[];

    fb = 0;

    if ((read_size = fread(start,1,size,inf)) == 0 || read_size == -1)
	return(read_size);

    e1 = encstr;
    e2 = statlist;

    while (size--)
    {
	*start++ ^= *e1 ^ *e2 ^ fb;
	temp = *e1++;
	fb = fb + (char)(temp * *e2++);
	if (*e1 == '\0')
	    e1 = encstr;
	if (*e2 == '\0')
	    e2 = statlist;
    }

    return(read_size);
}

static char scoreline[100];  /* 점수 데이터 한 줄을 임시 저장하는 버퍼 */
/*
 * read_scrore
 *	Read in the score file
 *
 * [한국어] 점수 파일(scoreboard)에서 상위 numscores개의 점수 데이터를 읽는다.
 *   각 점수 항목: sc_name(이름), sc_uid(사용자ID), sc_score(점수),
 *     sc_flags(플래그), sc_monster(처치한 몬스터), sc_level(달성 레벨),
 *     sc_time(시간)
 *   scoreboard가 NULL이면 아무것도 하지 않는다.
 */
void
rd_score(SCORE *top_ten)
{
    unsigned int i;

	if (scoreboard == NULL)
		return;

	rewind(scoreboard); 

	for(i = 0; i < numscores; i++)
    {
        encread(top_ten[i].sc_name, MAXSTR, scoreboard);
        encread(scoreline, 100, scoreboard);
        sscanf(scoreline, " %u %d %u %hu %d %x \n",
            &top_ten[i].sc_uid, &top_ten[i].sc_score,
            &top_ten[i].sc_flags, &top_ten[i].sc_monster,
            &top_ten[i].sc_level, &top_ten[i].sc_time);
    }

	rewind(scoreboard); 
}

/*
 * write_scrore
 *	Read in the score file
 *
 * [한국어] 상위 numscores개의 점수 데이터를 점수 파일(scoreboard)에 기록한다.
 *   rd_score()와 대칭 구조이며, scoreline 버퍼를 0으로 초기화 후
 *   sprintf로 형식화한 점수 데이터를 encwrite()로 암호화하여 저장한다.
 *   scoreboard가 NULL이면 아무것도 하지 않는다.
 */
void
wr_score(SCORE *top_ten)
{
    unsigned int i;

	if (scoreboard == NULL)
		return;

	rewind(scoreboard);

    for(i = 0; i < numscores; i++)
    {
          memset(scoreline,0,100);
          encwrite(top_ten[i].sc_name, MAXSTR, scoreboard);
          sprintf(scoreline, " %u %d %u %hu %d %x \n",
              top_ten[i].sc_uid, top_ten[i].sc_score,
              top_ten[i].sc_flags, top_ten[i].sc_monster,
              top_ten[i].sc_level, top_ten[i].sc_time);
          encwrite(scoreline,100,scoreboard);
    }

	rewind(scoreboard); 
}
