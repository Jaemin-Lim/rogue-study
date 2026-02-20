/*
 * Various input/output functions
 * 다양한 입출력 함수들을 담은 파일.
 * 화면 상단의 메시지 출력, 상태 표시줄 갱신, 문자 입력 등을 담당한다.
 *
 * @(#)io.c	4.32 (Berkeley) 02/05/99
 */

#include <stdarg.h>
#include <curses.h>
#include <ctype.h>
#include <string.h>
#include "rogue.h"

/*
 * msg:
 *	Display a message at the top of the screen.
 *	화면 상단(0행)에 메시지를 출력하는 함수.
 *	빈 문자열("")이면 현재 줄을 지운다.
 *	메시지가 너무 길면 "--More--"를 표시하고 사용자 입력을 기다린다.
 *	가변 인수(printf 형식)를 지원한다.
 */
#define MAXMSG	(NUMCOLS - sizeof "--More--")
/* MAXMSG: "--More--" 문자열을 제외한 최대 메시지 길이 */

static char msgbuf[2*MAXMSG+1];  /* 메시지 버퍼 (두 화면 분량) */
static int newpos = 0;  /* 현재 버퍼의 다음 쓰기 위치 */

/* VARARGS1 */
int
msg(char *fmt, ...)
{
    va_list args;  /* 가변 인수 목록 */

    /*
     * if the string is "", just clear the line
     * 빈 문자열이면 화면 상단의 메시지 줄을 지운다
     */
    if (*fmt == '\0')
    {
	move(0, 0);
	clrtoeol();  /* 줄 끝까지 지우기 */
	mpos = 0;
	return ~ESCAPE;
    }
    /*
     * otherwise add to the message and flush it out
     * 아니면 메시지 버퍼에 추가하고 출력
     */
    va_start(args, fmt);
    doadd(fmt, args);  /* 버퍼에 포맷된 문자열 추가 */
    va_end(args);
    return endmsg();  /* 버퍼 내용 화면에 출력 */
}

/*
 * addmsg:
 *	Add things to the current message
 *	현재 메시지 버퍼에 내용을 추가하는 함수.
 *	msg()와 달리 바로 출력하지 않고 버퍼에만 추가한다.
 *	여러 조각의 메시지를 연결할 때 사용한다.
 *	예: addmsg("you "); addmsg("hit "); msg("the monster");
 */
/* VARARGS1 */
void
addmsg(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    doadd(fmt, args);  /* 버퍼에 포맷된 문자열 추가 */
    va_end(args);
}

/*
 * endmsg:
 *	Display a new msg (giving him a chance to see the previous one
 *	if it is up there with the --More--)
 *	메시지 버퍼의 내용을 화면에 출력하는 함수.
 *	이전 메시지가 아직 표시 중이면 "--More--"를 보여주고
 *	사용자가 스페이스를 누를 때까지 대기한다.
 *	msg_esc가 TRUE이면 ESCAPE 키로도 건너뛸 수 있다.
 */
int
endmsg()
{
    char ch;

    /* 메시지 저장 옵션이 켜져 있으면 이전 메시지를 huh 버퍼에 저장 */
    if (save_msg)
	strcpy(huh, msgbuf);  /* huh: 마지막 메시지 저장 버퍼 (Ctrl+P로 재표시) */
    if (mpos)  /* 이미 메시지가 표시 중이면 "--More--" 표시 */
    {
	look(FALSE);
	mvaddstr(0, mpos, "--More--");
	refresh();
	if (!msg_esc)
	    wait_for(' ');  /* 스페이스 대기 */
	else
	{
	    while ((ch = readchar()) != ' ')
		if (ch == ESCAPE)  /* ESCAPE 키면 메시지 버퍼 비우기 */
		{
		    msgbuf[0] = '\0';
		    mpos = 0;
		    newpos = 0;
		    msgbuf[0] = '\0';
		    return ESCAPE;
		}
	}
    }
    /*
     * All messages should start with uppercase, except ones that
     * start with a pack addressing character
     * 모든 메시지는 대문자로 시작해야 함 (배낭 주소 문자 예외)
     * lower_msg 플래그나 두 번째 문자가 ')'이면 소문자 유지
     */
    if (islower(msgbuf[0]) && !lower_msg && msgbuf[1] != ')')
	msgbuf[0] = (char) toupper(msgbuf[0]);
    mvaddstr(0, 0, msgbuf);  /* 화면 상단에 메시지 출력 */
    clrtoeol();  /* 나머지 줄 지우기 */
    mpos = newpos;  /* 현재 메시지 길이 저장 */
    newpos = 0;
    msgbuf[0] = '\0';  /* 버퍼 초기화 */
    refresh();
    return ~ESCAPE;
}

/*
 * doadd:
 *	Perform an add onto the message buffer
 *	메시지 버퍼에 포맷된 문자열을 추가하는 내부 함수.
 *	버퍼가 가득 차면 현재 버퍼를 출력한 후 계속한다.
 */
void
doadd(char *fmt, va_list args)
{
    static char buf[MAXSTR];

    /*
     * Do the printf into buf
     * vsprintf로 포맷된 문자열을 임시 버퍼에 생성
     */
    vsprintf(buf, fmt, args);
    if (strlen(buf) + newpos >= MAXMSG)  /* 버퍼 오버플로우 예방 */
        endmsg();  /* 현재 버퍼 출력 후 비우기 */
    strcat(msgbuf, buf);  /* 메시지 버퍼에 추가 */
    newpos = (int) strlen(msgbuf);  /* 버퍼 내 현재 위치 갱신 */
}

/*
 * step_ok:
 *	Returns true if it is ok to step on ch
 *	해당 문자가 있는 칸을 밟을 수 있는지 판별하는 함수.
 *	공백(' '), 수직 벽('|'), 수평 벽('-')은 밟을 수 없다.
 *	알파벳(몬스터)은 밟을 수 없다.
 *	그 외 문자(바닥, 문, 통로, 아이템 등)는 밟을 수 있다.
 */
int
step_ok(int ch)
{
    switch (ch)
    {
	case ' ':   /* 빈 공간 (벽 또는 빈 공간) */
	case '|':   /* 수직 벽 */
	case '-':   /* 수평 벽 */
	    return FALSE;
	default:
	    return (!isalpha(ch));  /* 알파벳(몬스터)이면 FALSE */
    }
}

/*
 * readchar:
 *	Reads and returns a character, checking for gross input errors
 *	터미널에서 한 문자를 읽는 함수.
 *	Ctrl+C(ASCII 3)이 입력되면 quit()를 호출하여 게임 종료 확인.
 */
char
readchar()
{
    char ch;

    ch = (char) md_readchar();  /* 플랫폼별 문자 읽기 (mdport.c 참조) */

    if (ch == 3)  /* Ctrl+C */
    {
	quit(0);
        return(27);  /* ESC 반환 */
    }

    return(ch);
}

/*
 * status:
 *	Display the important stats line.  Keep the cursor where it was.
 *	화면 하단 상태 표시줄을 갱신하는 함수.
 *	레벨, 금화, HP, 최대HP, 힘, 방어력, 경험치 정보를 출력한다.
 *	값이 변경되지 않았으면 재출력하지 않는다 (효율성).
 *	stat_msg가 TRUE이면 화면 상단에 메시지로 출력한다.
 */
void
status()
{
    register int oy, ox, temp;
    static int hpwidth = 0;     /* HP 표시 자릿수 */
    static int s_hungry = 0;    /* 이전 배고픔 상태 */
    static int s_lvl = 0;       /* 이전 레벨 */
    static int s_pur = -1;      /* 이전 금화 (-1로 초기화하여 첫 출력 강제) */
    static int s_hp = 0;        /* 이전 HP */
    static int s_arm = 0;       /* 이전 방어력 */
    static str_t s_str = 0;     /* 이전 힘 */
    static int s_exp = 0;       /* 이전 경험치 */
    /* 배고픔 상태 이름: 0=정상, 1=배고픔, 2=허약, 3=기절 */
    static char *state_name[] =
    {
	"", "Hungry", "Weak", "Faint"
    };

    /*
     * If nothing has changed since the last status, don't
     * bother.
     * 이전 상태에서 변화가 없으면 재출력 불필요
     */
    temp = (cur_armor != NULL ? cur_armor->o_arm : pstats.s_arm);
    if (s_hp == pstats.s_hpt && s_exp == pstats.s_exp && s_pur == purse
	&& s_arm == temp && s_str == pstats.s_str && s_lvl == level
	&& s_hungry == hungry_state
	&& !stat_msg
	)
	    return;

    s_arm = temp;

    getyx(stdscr, oy, ox);  /* 현재 커서 위치 저장 */
    if (s_hp != max_hp)  /* 최대 HP가 변경되면 자릿수 재계산 */
    {
	temp = max_hp;
	s_hp = max_hp;
	for (hpwidth = 0; temp; hpwidth++)
	    temp /= 10;
    }

    /*
     * Save current status
     * 현재 상태를 정적 변수에 저장 (다음 비교를 위해)
     */
    s_lvl = level;
    s_pur = purse;
    s_hp = pstats.s_hpt;
    s_str = pstats.s_str;
    s_exp = pstats.s_exp; 
    s_hungry = hungry_state;

    if (stat_msg)  /* '@' 명령: 화면 상단에 메시지로 표시 */
    {
	move(0, 0);
        msg("Level: %d  Gold: %-5d  Hp: %*d(%*d)  Str: %2d(%d)  Arm: %-2d  Exp: %d/%ld  %s",
	    level, purse, hpwidth, pstats.s_hpt, hpwidth, max_hp, pstats.s_str,
	    max_stats.s_str, 10 - s_arm, pstats.s_lvl, pstats.s_exp,
	    state_name[hungry_state]);
    }
    else  /* 화면 하단 상태 표시줄에 출력 */
    {
	move(STATLINE, 0);  /* STATLINE = NUMLINES - 1 = 23번째 줄 */
                
        printw("Level: %d  Gold: %-5d  Hp: %*d(%*d)  Str: %2d(%d)  Arm: %-2d  Exp: %d/%d  %s",
	    level, purse, hpwidth, pstats.s_hpt, hpwidth, max_hp, pstats.s_str,
	    max_stats.s_str, 10 - s_arm, pstats.s_lvl, pstats.s_exp,
	    state_name[hungry_state]);
    }

    clrtoeol();  /* 줄 나머지 지우기 */
    move(oy, ox);  /* 커서 원래 위치로 복원 */
}

/*
 * wait_for
 *	Sit around until the guy types the right key
 *	플레이어가 지정된 키를 입력할 때까지 대기하는 함수.
 *	'\n' 이면 Enter 또는 Return 키 대기.
 *	다른 문자이면 해당 문자 대기.
 */
void
wait_for(int ch)
{
    register char c;

    if (ch == '\n')  /* Enter/Return 키 대기 */
        while ((c = readchar()) != '\n' && c != '\r')
	    continue;
    else  /* 지정된 문자 대기 */
        while (readchar() != ch)
	    continue;
}

/*
 * show_win:
 *	Function used to display a window and wait before returning
 *	보조 창(hw)의 내용을 표시하고 스페이스 키를 기다리는 함수.
 *	인벤토리나 지도 표시 후 플레이어 입력을 기다릴 때 사용된다.
 */
void
show_win(char *message)
{
    WINDOW *win;

    win = hw;  /* 보조 창(hidden window) 사용 */
    wmove(win, 0, 0);
    waddstr(win, message);  /* 메시지 출력 */
    touchwin(win);  /* 창 전체를 다시 출력 필요로 표시 */
    wmove(win, hero.y, hero.x);  /* 영웅 위치로 커서 이동 */
    wrefresh(win);
    wait_for(' ');  /* 스페이스 대기 */
    clearok(curscr, TRUE);  /* 메인 화면 전체 재출력 */
    touchwin(stdscr);
}

#include <stdarg.h>
#include <curses.h>
#include <ctype.h>
#include <string.h>
#include "rogue.h"

/*
 * msg:
 *	Display a message at the top of the screen.
 */
#define MAXMSG	(NUMCOLS - sizeof "--More--")

static char msgbuf[2*MAXMSG+1];
static int newpos = 0;

/* VARARGS1 */
int
msg(char *fmt, ...)
{
    va_list args;

    /*
     * if the string is "", just clear the line
     */
    if (*fmt == '\0')
    {
	move(0, 0);
	clrtoeol();
	mpos = 0;
	return ~ESCAPE;
    }
    /*
     * otherwise add to the message and flush it out
     */
    va_start(args, fmt);
    doadd(fmt, args);
    va_end(args);
    return endmsg();
}

/*
 * addmsg:
 *	Add things to the current message
 */
/* VARARGS1 */
void
addmsg(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    doadd(fmt, args);
    va_end(args);
}

/*
 * endmsg:
 *	Display a new msg (giving him a chance to see the previous one
 *	if it is up there with the --More--)
 */
int
endmsg()
{
    char ch;

    if (save_msg)
	strcpy(huh, msgbuf);
    if (mpos)
    {
	look(FALSE);
	mvaddstr(0, mpos, "--More--");
	refresh();
	if (!msg_esc)
	    wait_for(' ');
	else
	{
	    while ((ch = readchar()) != ' ')
		if (ch == ESCAPE)
		{
		    msgbuf[0] = '\0';
		    mpos = 0;
		    newpos = 0;
		    msgbuf[0] = '\0';
		    return ESCAPE;
		}
	}
    }
    /*
     * All messages should start with uppercase, except ones that
     * start with a pack addressing character
     */
    if (islower(msgbuf[0]) && !lower_msg && msgbuf[1] != ')')
	msgbuf[0] = (char) toupper(msgbuf[0]);
    mvaddstr(0, 0, msgbuf);
    clrtoeol();
    mpos = newpos;
    newpos = 0;
    msgbuf[0] = '\0';
    refresh();
    return ~ESCAPE;
}

/*
 * doadd:
 *	Perform an add onto the message buffer
 */
void
doadd(char *fmt, va_list args)
{
    static char buf[MAXSTR];

    /*
     * Do the printf into buf
     */
    vsprintf(buf, fmt, args);
    if (strlen(buf) + newpos >= MAXMSG)
        endmsg(); 
    strcat(msgbuf, buf);
    newpos = (int) strlen(msgbuf);
}

/*
 * step_ok:
 *	Returns true if it is ok to step on ch
 */
int
step_ok(int ch)
{
    switch (ch)
    {
	case ' ':
	case '|':
	case '-':
	    return FALSE;
	default:
	    return (!isalpha(ch));
    }
}

/*
 * readchar:
 *	Reads and returns a character, checking for gross input errors
 */
char
readchar()
{
    char ch;

    ch = (char) md_readchar();

    if (ch == 3)
    {
	quit(0);
        return(27);
    }

    return(ch);
}

/*
 * status:
 *	Display the important stats line.  Keep the cursor where it was.
 */
void
status()
{
    register int oy, ox, temp;
    static int hpwidth = 0;
    static int s_hungry = 0;
    static int s_lvl = 0;
    static int s_pur = -1;
    static int s_hp = 0;
    static int s_arm = 0;
    static str_t s_str = 0;
    static int s_exp = 0;
    static char *state_name[] =
    {
	"", "Hungry", "Weak", "Faint"
    };

    /*
     * If nothing has changed since the last status, don't
     * bother.
     */
    temp = (cur_armor != NULL ? cur_armor->o_arm : pstats.s_arm);
    if (s_hp == pstats.s_hpt && s_exp == pstats.s_exp && s_pur == purse
	&& s_arm == temp && s_str == pstats.s_str && s_lvl == level
	&& s_hungry == hungry_state
	&& !stat_msg
	)
	    return;

    s_arm = temp;

    getyx(stdscr, oy, ox);
    if (s_hp != max_hp)
    {
	temp = max_hp;
	s_hp = max_hp;
	for (hpwidth = 0; temp; hpwidth++)
	    temp /= 10;
    }

    /*
     * Save current status
     */
    s_lvl = level;
    s_pur = purse;
    s_hp = pstats.s_hpt;
    s_str = pstats.s_str;
    s_exp = pstats.s_exp; 
    s_hungry = hungry_state;

    if (stat_msg)
    {
	move(0, 0);
        msg("Level: %d  Gold: %-5d  Hp: %*d(%*d)  Str: %2d(%d)  Arm: %-2d  Exp: %d/%ld  %s",
	    level, purse, hpwidth, pstats.s_hpt, hpwidth, max_hp, pstats.s_str,
	    max_stats.s_str, 10 - s_arm, pstats.s_lvl, pstats.s_exp,
	    state_name[hungry_state]);
    }
    else
    {
	move(STATLINE, 0);
                
        printw("Level: %d  Gold: %-5d  Hp: %*d(%*d)  Str: %2d(%d)  Arm: %-2d  Exp: %d/%d  %s",
	    level, purse, hpwidth, pstats.s_hpt, hpwidth, max_hp, pstats.s_str,
	    max_stats.s_str, 10 - s_arm, pstats.s_lvl, pstats.s_exp,
	    state_name[hungry_state]);
    }

    clrtoeol();
    move(oy, ox);
}

/*
 * wait_for
 *	Sit around until the guy types the right key
 */
void
wait_for(int ch)
{
    register char c;

    if (ch == '\n')
        while ((c = readchar()) != '\n' && c != '\r')
	    continue;
    else
        while (readchar() != ch)
	    continue;
}

/*
 * show_win:
 *	Function used to display a window and wait before returning
 */
void
show_win(char *message)
{
    WINDOW *win;

    win = hw;
    wmove(win, 0, 0);
    waddstr(win, message);
    touchwin(win);
    wmove(win, hero.y, hero.x);
    wrefresh(win);
    wait_for(' ');
    clearok(curscr, TRUE);
    touchwin(stdscr);
}
