/*
 * global variable initializaton
 * 게임 초기화와 관련된 함수들을 담은 파일.
 * 플레이어 초기 스탯/아이템, 포션 색상, 스크롤 이름, 반지 보석, 지팡이 재질,
 * 아이템 출현 확률 등을 초기화한다.
 *
 * @(#)init.c	4.31 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <stdlib.h>
#include <curses.h>
#include <ctype.h>
#include <string.h>
#include "rogue.h"

/*
 * init_player:
 *	Roll her up
 *	플레이어를 초기화하는 함수.
 *	최대 스탯을 현재 스탯으로 설정하고,
 *	초기 아이템(음식, 갑옷, 메이스, 활, 화살)을 배낭에 추가한다.
 */
void
init_player()
{
    register THING *obj;  /* 아이템 포인터 */

    pstats = max_stats;   /* 플레이어 스탯을 최대 스탯으로 초기화 */
    food_left = HUNGERTIME;  /* 초기 음식 양 설정 */
    /*
     * Give him some food
     * 초기 음식 지급
     */
    obj = new_item();
    obj->o_type = FOOD;    /* 음식 유형 */
    obj->o_count = 1;
    add_pack(obj, TRUE);   /* 배낭에 추가 (TRUE: 조용히) */
    /*
     * And his suit of armor
     * 초기 갑옷 지급: 링 메일(RING_MAIL)
     * a_class[RING_MAIL] - 1: 기본 방어력에서 1 감소 (약간 고급품)
     */
    obj = new_item();
    obj->o_type = ARMOR;
    obj->o_which = RING_MAIL;
    obj->o_arm = a_class[RING_MAIL] - 1;  /* 방어력 = 기본값 - 1 */
    obj->o_flags |= ISKNOW;  /* 갑옷 정보 인식 표시 */
    obj->o_count = 1;
    cur_armor = obj;  /* 현재 갑옷으로 설정 */
    add_pack(obj, TRUE);
    /*
     * Give him his weaponry.  First a mace.
     * 초기 무기 지급: 메이스(MACE) +1/+1
     */
    obj = new_item();
    init_weapon(obj, MACE);  /* 메이스 초기화 (weapons.c 참조) */
    obj->o_hplus = 1;   /* 명중 보너스 +1 */
    obj->o_dplus = 1;   /* 피해 보너스 +1 */
    obj->o_flags |= ISKNOW;
    add_pack(obj, TRUE);
    cur_weapon = obj;   /* 현재 무기로 설정 */
    /*
     * Now a +1 bow
     * 활(BOW) +1 지급
     */
    obj = new_item();
    init_weapon(obj, BOW);
    obj->o_hplus = 1;
    obj->o_flags |= ISKNOW;
    add_pack(obj, TRUE);
    /*
     * Now some arrows
     * 화살(ARROW) 25~39개 지급
     */
    obj = new_item();
    init_weapon(obj, ARROW);
    obj->o_count = rnd(15) + 25;  /* 25~39개 */
    obj->o_flags |= ISKNOW;
    add_pack(obj, TRUE);
}

/*
 * Contains defintions and functions for dealing with things like
 * potions and scrolls
 * 포션, 스크롤, 반지, 지팡이 등의 외관(색상, 이름, 보석, 재질)을 정의하는 데이터와 함수들
 */

/* rainbow: 포션의 색상 이름 목록 */
char *rainbow[] = {
    "amber",       /* 호박색 */
    "aquamarine",  /* 청록색 */
    "black",       /* 검정 */
    "blue",        /* 파랑 */
    "brown",       /* 갈색 */
    "clear",       /* 투명 */
    "crimson",     /* 진홍색 */
    "cyan",        /* 청록 */
    "ecru",        /* 베이지 */
    "gold",        /* 금색 */
    "green",       /* 녹색 */
    "grey",        /* 회색 */
    "magenta",     /* 자홍색 */
    "orange",      /* 주황색 */
    "pink",        /* 분홍색 */
    "plaid",       /* 격자무늬 */
    "purple",      /* 보라색 */
    "red",         /* 빨강 */
    "silver",      /* 은색 */
    "tan",         /* 황갈색 */
    "tangerine",   /* 귤색 */
    "topaz",       /* 황옥색 */
    "turquoise",   /* 청록옥색 */
    "vermilion",   /* 주홍색 */
    "violet",      /* 보라 */
    "white",       /* 흰색 */
    "yellow",      /* 노랑 */
};

#define NCOLORS (sizeof rainbow / sizeof (char *))
/* NCOLORS: 색상 개수 */
int cNCOLORS = NCOLORS;  /* C 파일에서 접근 가능한 색상 개수 변수 */

/* sylls: 스크롤 이름을 만드는 데 사용되는 음절 목록 */
/* 이 음절들을 무작위로 조합하여 스크롤의 가짜 이름을 생성한다 */
static char *sylls[] = {
    "a", "ab", "ag", "aks", "ala", "an", "app", "arg", "arze", "ash",
    "bek", "bie", "bit", "bjor", "blu", "bot", "bu", "byt", "comp",
    "con", "cos", "cre", "dalf", "dan", "den", "do", "e", "eep", "el",
    "eng", "er", "ere", "erk", "esh", "evs", "fa", "fid", "fri", "fu",
    "gan", "gar", "glen", "gop", "gre", "ha", "hyd", "i", "ing", "ip",
    "ish", "it", "ite", "iv", "jo", "kho", "kli", "klis", "la", "lech",
    "mar", "me", "mi", "mic", "mik", "mon", "mung", "mur", "nej",
    "nelg", "nep", "ner", "nes", "nes", "nih", "nin", "o", "od", "ood",
    "org", "orn", "ox", "oxy", "pay", "ple", "plu", "po", "pot",
    "prok", "re", "rea", "rhov", "ri", "ro", "rog", "rok", "rol", "sa",
    "san", "sat", "sef", "seh", "shu", "ski", "sna", "sne", "snik",
    "sno", "so", "sol", "sri", "sta", "sun", "ta", "tab", "tem",
    "ther", "ti", "tox", "trol", "tue", "turs", "u", "ulk", "um", "un",
    "uni", "ur", "val", "viv", "vly", "vom", "wah", "wed", "werg",
    "wex", "whon", "wun", "xo", "y", "yot", "yu", "zant", "zeb", "zim",
    "zok", "zon", "zum",
};

/* STONE 구조체: 반지 보석 이름과 가격 */
STONE stones[] = {
    { "agate",		 25},   /* 마노 */
    { "alexandrite",	 40},   /* 알렉산드라이트 */
    { "amethyst",	 50},   /* 자수정 */
    { "carnelian",	 40},   /* 홍옥수 */
    { "diamond",	300},   /* 다이아몬드 */
    { "emerald",	300},   /* 에메랄드 */
    { "germanium",	225},   /* 게르마늄 */
    { "granite",	  5},   /* 화강암 */
    { "garnet",		 50},   /* 가넷 */
    { "jade",		150},   /* 비취 */
    { "kryptonite",	300},   /* 크립토나이트 */
    { "lapis lazuli",	 50},   /* 청금석 */
    { "moonstone",	 50},   /* 문스톤 */
    { "obsidian",	 15},   /* 흑요석 */
    { "onyx",		 60},   /* 줄마노 */
    { "opal",		200},   /* 단백석 */
    { "pearl",		220},   /* 진주 */
    { "peridot",	 63},   /* 페리도트 */
    { "ruby",		350},   /* 루비 */
    { "sapphire",	285},   /* 사파이어 */
    { "stibotantalite",	200},   /* 안티모니탄탈라이트 */
    { "tiger eye",	 50},   /* 호안석 */
    { "topaz",		 60},   /* 황옥 */
    { "turquoise",	 70},   /* 터키옥 */
    { "taaffeite",	300},   /* 타아페이트 */
    { "zircon",	 	 80},   /* 지르콘 */
};

#define NSTONES (sizeof stones / sizeof (STONE))
/* NSTONES: 보석 종류 수 */
int cNSTONES = NSTONES;  /* 외부 접근 가능한 보석 종류 수 */

/* wood: 지팡이(staff) 재질 이름 목록 */
char *wood[] = {
    "avocado wood",   /* 아보카도 나무 */
    "balsa",          /* 발사 나무 */
    "bamboo",         /* 대나무 */
    "banyan",         /* 반얀 나무 */
    "birch",          /* 자작나무 */
    "cedar",          /* 삼나무 */
    "cherry",         /* 벚나무 */
    "cinnibar",       /* 진사 */
    "cypress",        /* 사이프러스 */
    "dogwood",        /* 층층나무 */
    "driftwood",      /* 유목 */
    "ebony",          /* 흑단 */
    "elm",            /* 느릅나무 */
    "eucalyptus",     /* 유칼립투스 */
    "fall",           /* 가을 나무 */
    "hemlock",        /* 독미나리 */
    "holly",          /* 호랑가시나무 */
    "ironwood",       /* 철목 */
    "kukui wood",     /* 쿠쿠이 나무 */
    "mahogany",       /* 마호가니 */
    "manzanita",      /* 만자니타 */
    "maple",          /* 단풍나무 */
    "oaken",          /* 참나무 */
    "persimmon wood", /* 감나무 */
    "pecan",          /* 피칸 */
    "pine",           /* 소나무 */
    "poplar",         /* 포플러 */
    "redwood",        /* 레드우드 */
    "rosewood",       /* 장미목 */
    "spruce",         /* 가문비나무 */
    "teak",           /* 티크 */
    "walnut",         /* 호두나무 */
    "zebrawood",      /* 얼룩말 나무 */
};

#define NWOOD (sizeof wood / sizeof (char *))
/* NWOOD: 지팡이 나무 재질 수 */
int cNWOOD = NWOOD;

/* metal: 마법봉(wand) 재질 이름 목록 */
char *metal[] = {
    "aluminum",    /* 알루미늄 */
    "beryllium",   /* 베릴륨 */
    "bone",        /* 뼈 */
    "brass",       /* 황동 */
    "bronze",      /* 청동 */
    "copper",      /* 구리 */
    "electrum",    /* 일렉트럼 (금은 합금) */
    "gold",        /* 금 */
    "iron",        /* 철 */
    "lead",        /* 납 */
    "magnesium",   /* 마그네슘 */
    "mercury",     /* 수은 */
    "nickel",      /* 니켈 */
    "pewter",      /* 주석합금 */
    "platinum",    /* 백금 */
    "steel",       /* 강철 */
    "silver",      /* 은 */
    "silicon",     /* 실리콘 */
    "tin",         /* 주석 */
    "titanium",    /* 티타늄 */
    "tungsten",    /* 텅스텐 */
    "zinc",        /* 아연 */
};

#define NMETAL (sizeof metal / sizeof (char *))
/* NMETAL: 마법봉 금속 재질 수 */
int cNMETAL = NMETAL;
/* MAX3: 세 값 중 최대값을 구하는 매크로 (used[] 배열 크기 결정에 사용) */
#define MAX3(a,b,c)	(a > b ? (a > c ? a : c) : (b > c ? b : c))

/* used[]: 이미 사용된 색상/보석/재질을 추적하는 배열 (중복 방지) */
static bool used[MAX3(NCOLORS, NSTONES, NWOOD)];

/*
 * init_colors:
 *	Initialize the potion color scheme for this time
 *	포션의 색상을 무작위로 초기화하는 함수.
 *	각 포션 유형에 rainbow[] 배열의 색상을 하나씩 고유하게 할당한다.
 *	같은 게임 세션에서 포션 색상은 고정되지만 세션마다 다르다.
 */
void
init_colors()
{
    register int i, j;

    /* 사용된 색상 배열 초기화 */
    for (i = 0; i < NCOLORS; i++)
	used[i] = FALSE;
    /* 각 포션 유형에 색상을 할당 */
    for (i = 0; i < MAXPOTIONS; i++)
    {
	do
	    j = rnd(NCOLORS);  /* 랜덤 색상 선택 */
	until (!used[j]);      /* 사용되지 않은 색상이면 선택 */
	used[j] = TRUE;        /* 해당 색상 사용 표시 */
	p_colors[i] = rainbow[j];  /* 포션 색상 배열에 저장 */
    }
}

/*
 * init_names:
 *	Generate the names of the various scrolls
 *	스크롤의 가짜 이름을 무작위로 생성하는 함수.
 *	음절(sylls[])을 무작위로 조합하여 의미 없는 마법 주문어 형태의 이름을 만든다.
 *	예: "aks el do" 같은 형태의 이름
 */
#define MAXNAME	40	/* Max number of characters in a name */
                    /* 스크롤 이름의 최대 문자 수 */

void
init_names()
{
    register int nsyl;      /* 현재 단어의 음절 수 */
    register char *cp, *sp; /* 버퍼 포인터, 음절 포인터 */
    register int i, nwords; /* 스크롤 인덱스, 단어 수 */

    for (i = 0; i < MAXSCROLLS; i++)
    {
	cp = prbuf;             /* 임시 버퍼 시작 */
	nwords = rnd(3) + 2;    /* 이름을 구성하는 단어 수: 2~4개 */
	while (nwords--)
	{
	    nsyl = rnd(3) + 1;  /* 단어당 음절 수: 1~3개 */
	    while (nsyl--)
	    {
		sp = sylls[rnd((sizeof sylls) / (sizeof (char *)))];  /* 랜덤 음절 선택 */
		if (&cp[strlen(sp)] > &prbuf[MAXNAME])  /* 길이 초과 방지 */
			break;
		while (*sp)
		    *cp++ = *sp++;  /* 음절 복사 */
	    }
	    *cp++ = ' ';  /* 단어 구분자 */
	}
	*--cp = '\0';  /* 마지막 공백 제거 및 문자열 종료 */
	s_names[i] = (char *) malloc((unsigned) strlen(prbuf)+1);  /* 메모리 할당 */
	strcpy(s_names[i], prbuf);  /* 스크롤 이름 저장 */
    }
}

/*
 * init_stones:
 *	Initialize the ring stone setting scheme for this time
 *	반지의 보석을 무작위로 초기화하는 함수.
 *	각 반지 유형에 stones[] 배열의 보석을 하나씩 고유하게 할당한다.
 *	보석 가격은 해당 반지의 기본 가치에 더해진다.
 */
void
init_stones()
{
    register int i, j;

    for (i = 0; i < NSTONES; i++)
	used[i] = FALSE;
    for (i = 0; i < MAXRINGS; i++)
    {
	do
	    j = rnd(NSTONES);
	until (!used[j]);
	used[j] = TRUE;
	r_stones[i] = stones[j].st_name;              /* 반지 보석 이름 저장 */
	ring_info[i].oi_worth += stones[j].st_value;  /* 보석 가치 추가 */
    }
}

/*
 * init_materials:
 *	Initialize the construction materials for wands and staffs
 *	마법봉(wand)과 지팡이(staff)의 재질을 무작위로 초기화하는 함수.
 *	50% 확률로 금속(wand) 또는 나무(staff) 재질을 선택하고,
 *	각 지팡이에 고유한 재질을 할당한다.
 */
void
init_materials()
{
    register int i, j;
    register char *str;      /* 재질 이름 문자열 */
    static bool metused[NMETAL];  /* 사용된 금속 재질 추적 */

    for (i = 0; i < NWOOD; i++)
	used[i] = FALSE;      /* 나무 재질 사용 여부 초기화 */
    for (i = 0; i < NMETAL; i++)
	metused[i] = FALSE;   /* 금속 재질 사용 여부 초기화 */
    for (i = 0; i < MAXSTICKS; i++)
    {
	for (;;)  /* 사용 가능한 재질을 찾을 때까지 반복 */
	    if (rnd(2) == 0)  /* 50% 확률로 금속 시도 */
	    {
		j = rnd(NMETAL);
		if (!metused[j])
		{
		    ws_type[i] = "wand";   /* 금속이면 "wand" (마법봉) */
		    str = metal[j];
		    metused[j] = TRUE;
		    break;
		}
	    }
	    else  /* 50% 확률로 나무 시도 */
	    {
		j = rnd(NWOOD);
		if (!used[j])
		{
		    ws_type[i] = "staff";  /* 나무이면 "staff" (지팡이) */
		    str = wood[j];
		    used[j] = TRUE;
		    break;
		}
	    }
	ws_made[i] = str;  /* 재질 이름 저장 */
    }
}

/* MASTER 모드에서만 사용되는 매크로: 아이템 유형별 최대 개수와 이름을 함께 제공 */
#ifdef MASTER
# define	NT	NUMTHINGS, "things"
# define	MP	MAXPOTIONS, "potions"
# define	MS	MAXSCROLLS, "scrolls"
# define	MR	MAXRINGS, "rings"
# define	MWS	MAXSTICKS, "sticks"
# define	MW	MAXWEAPONS, "weapons"
# define	MA	MAXARMORS, "armor"
#else
# define	NT	NUMTHINGS
# define	MP	MAXPOTIONS
# define	MS	MAXSCROLLS
# define	MR	MAXRINGS
# define	MWS	MAXSTICKS
# define	MW	MAXWEAPONS
# define	MA	MAXARMORS
#endif

/*
 * sumprobs:
 *	Sum up the probabilities for items appearing
 *	아이템 출현 확률을 누적합으로 변환하는 함수.
 *	각 항목의 확률을 개별 값에서 누적 확률로 변환한다.
 *	예: [10, 20, 70] → [10, 30, 100]
 *	이후 rnd(100)의 결과와 비교하여 아이템 유형을 결정한다 (pick_one 함수 참조).
 */
void
sumprobs(struct obj_info *info, int bound
#ifdef MASTER
	, char *name  /* MASTER 모드에서 오류 검사용 이름 */
#endif
)
{
#ifdef MASTER
	struct obj_info *start = info;
#endif
    struct obj_info *endp;  /* 배열의 끝 포인터 */

    endp = info + bound;
    while (++info < endp)
	info->oi_prob += (info - 1)->oi_prob;  /* 이전 항목의 누적 확률을 더함 */
#ifdef MASTER
    badcheck(name, start, bound);  /* 총합이 100인지 검증 */
#endif
}

/*
 * init_probs:
 *	Initialize the probabilities for the various items
 *	모든 아이템 유형의 출현 확률을 누적합으로 초기화하는 함수.
 *	게임 시작 시 한 번 호출된다.
 */
void
init_probs()
{
    sumprobs(things, NT);      /* 아이템 카테고리 확률 초기화 */
    sumprobs(pot_info, MP);    /* 포션 확률 초기화 */
    sumprobs(scr_info, MS);    /* 스크롤 확률 초기화 */
    sumprobs(ring_info, MR);   /* 반지 확률 초기화 */
    sumprobs(ws_info, MWS);    /* 지팡이/마법봉 확률 초기화 */
    sumprobs(weap_info, MW);   /* 무기 확률 초기화 */
    sumprobs(arm_info, MA);    /* 갑옷 확률 초기화 */
}

#ifdef MASTER
/*
 * badcheck:
 *	Check to see if a series of probabilities sums to 100
 *	확률의 합이 100이 되는지 검증하는 MASTER 모드 전용 함수.
 *	합이 100이 아니면 문제가 있는 항목들을 출력한다.
 */
void
badcheck(char *name, struct obj_info *info, int bound)
{
    register struct obj_info *end;

    if (info[bound - 1].oi_prob == 100)  /* 마지막 누적값이 100이면 정상 */
	return;
    printf("\nBad percentages for %s (bound = %d):\n", name, bound);
    for (end = &info[bound]; info < end; info++)
	printf("%3d%% %s\n", info->oi_prob, info->oi_name);
    printf("[hit RETURN to continue]");
    fflush(stdout);
    while (getchar() != '\n')
	continue;
}
#endif

/*
 * pick_color:
 *	If he is halucinating, pick a random color name and return it,
 *	otherwise return the given color.
 *	환각 상태이면 랜덤 색상 이름을 반환하고,
 *	그렇지 않으면 지정된 실제 색상을 반환하는 함수.
 *	포션 색상 표시나 스크롤 이름에 사용된다.
 */
char *
pick_color(char *col)
{
    return (on(player, ISHALU) ? rainbow[rnd(NCOLORS)] : col);
}

#include <stdlib.h>
#include <curses.h>
#include <ctype.h>
#include <string.h>
#include "rogue.h"

/*
 * init_player:
 *	Roll her up
 */
void
init_player()
{
    register THING *obj;

    pstats = max_stats;
    food_left = HUNGERTIME;
    /*
     * Give him some food
     */
    obj = new_item();
    obj->o_type = FOOD;
    obj->o_count = 1;
    add_pack(obj, TRUE);
    /*
     * And his suit of armor
     */
    obj = new_item();
    obj->o_type = ARMOR;
    obj->o_which = RING_MAIL;
    obj->o_arm = a_class[RING_MAIL] - 1;
    obj->o_flags |= ISKNOW;
    obj->o_count = 1;
    cur_armor = obj;
    add_pack(obj, TRUE);
    /*
     * Give him his weaponry.  First a mace.
     */
    obj = new_item();
    init_weapon(obj, MACE);
    obj->o_hplus = 1;
    obj->o_dplus = 1;
    obj->o_flags |= ISKNOW;
    add_pack(obj, TRUE);
    cur_weapon = obj;
    /*
     * Now a +1 bow
     */
    obj = new_item();
    init_weapon(obj, BOW);
    obj->o_hplus = 1;
    obj->o_flags |= ISKNOW;
    add_pack(obj, TRUE);
    /*
     * Now some arrows
     */
    obj = new_item();
    init_weapon(obj, ARROW);
    obj->o_count = rnd(15) + 25;
    obj->o_flags |= ISKNOW;
    add_pack(obj, TRUE);
}

/*
 * Contains defintions and functions for dealing with things like
 * potions and scrolls
 */

char *rainbow[] = {
    "amber",
    "aquamarine",
    "black",
    "blue",
    "brown",
    "clear",
    "crimson",
    "cyan",
    "ecru",
    "gold",
    "green",
    "grey",
    "magenta",
    "orange",
    "pink",
    "plaid",
    "purple",
    "red",
    "silver",
    "tan",
    "tangerine",
    "topaz",
    "turquoise",
    "vermilion",
    "violet",
    "white",
    "yellow",
};

#define NCOLORS (sizeof rainbow / sizeof (char *))
int cNCOLORS = NCOLORS;

static char *sylls[] = {
    "a", "ab", "ag", "aks", "ala", "an", "app", "arg", "arze", "ash",
    "bek", "bie", "bit", "bjor", "blu", "bot", "bu", "byt", "comp",
    "con", "cos", "cre", "dalf", "dan", "den", "do", "e", "eep", "el",
    "eng", "er", "ere", "erk", "esh", "evs", "fa", "fid", "fri", "fu",
    "gan", "gar", "glen", "gop", "gre", "ha", "hyd", "i", "ing", "ip",
    "ish", "it", "ite", "iv", "jo", "kho", "kli", "klis", "la", "lech",
    "mar", "me", "mi", "mic", "mik", "mon", "mung", "mur", "nej",
    "nelg", "nep", "ner", "nes", "nes", "nih", "nin", "o", "od", "ood",
    "org", "orn", "ox", "oxy", "pay", "ple", "plu", "po", "pot",
    "prok", "re", "rea", "rhov", "ri", "ro", "rog", "rok", "rol", "sa",
    "san", "sat", "sef", "seh", "shu", "ski", "sna", "sne", "snik",
    "sno", "so", "sol", "sri", "sta", "sun", "ta", "tab", "tem",
    "ther", "ti", "tox", "trol", "tue", "turs", "u", "ulk", "um", "un",
    "uni", "ur", "val", "viv", "vly", "vom", "wah", "wed", "werg",
    "wex", "whon", "wun", "xo", "y", "yot", "yu", "zant", "zeb", "zim",
    "zok", "zon", "zum",
};

STONE stones[] = {
    { "agate",		 25},
    { "alexandrite",	 40},
    { "amethyst",	 50},
    { "carnelian",	 40},
    { "diamond",	300},
    { "emerald",	300},
    { "germanium",	225},
    { "granite",	  5},
    { "garnet",		 50},
    { "jade",		150},
    { "kryptonite",	300},
    { "lapis lazuli",	 50},
    { "moonstone",	 50},
    { "obsidian",	 15},
    { "onyx",		 60},
    { "opal",		200},
    { "pearl",		220},
    { "peridot",	 63},
    { "ruby",		350},
    { "sapphire",	285},
    { "stibotantalite",	200},
    { "tiger eye",	 50},
    { "topaz",		 60},
    { "turquoise",	 70},
    { "taaffeite",	300},
    { "zircon",	 	 80},
};

#define NSTONES (sizeof stones / sizeof (STONE))
int cNSTONES = NSTONES;

char *wood[] = {
    "avocado wood",
    "balsa",
    "bamboo",
    "banyan",
    "birch",
    "cedar",
    "cherry",
    "cinnibar",
    "cypress",
    "dogwood",
    "driftwood",
    "ebony",
    "elm",
    "eucalyptus",
    "fall",
    "hemlock",
    "holly",
    "ironwood",
    "kukui wood",
    "mahogany",
    "manzanita",
    "maple",
    "oaken",
    "persimmon wood",
    "pecan",
    "pine",
    "poplar",
    "redwood",
    "rosewood",
    "spruce",
    "teak",
    "walnut",
    "zebrawood",
};

#define NWOOD (sizeof wood / sizeof (char *))
int cNWOOD = NWOOD;

char *metal[] = {
    "aluminum",
    "beryllium",
    "bone",
    "brass",
    "bronze",
    "copper",
    "electrum",
    "gold",
    "iron",
    "lead",
    "magnesium",
    "mercury",
    "nickel",
    "pewter",
    "platinum",
    "steel",
    "silver",
    "silicon",
    "tin",
    "titanium",
    "tungsten",
    "zinc",
};

#define NMETAL (sizeof metal / sizeof (char *))
int cNMETAL = NMETAL;
#define MAX3(a,b,c)	(a > b ? (a > c ? a : c) : (b > c ? b : c))

static bool used[MAX3(NCOLORS, NSTONES, NWOOD)];

/*
 * init_colors:
 *	Initialize the potion color scheme for this time
 */
void
init_colors()
{
    register int i, j;

    for (i = 0; i < NCOLORS; i++)
	used[i] = FALSE;
    for (i = 0; i < MAXPOTIONS; i++)
    {
	do
	    j = rnd(NCOLORS);
	until (!used[j]);
	used[j] = TRUE;
	p_colors[i] = rainbow[j];
    }
}

/*
 * init_names:
 *	Generate the names of the various scrolls
 */
#define MAXNAME	40	/* Max number of characters in a name */

void
init_names()
{
    register int nsyl;
    register char *cp, *sp;
    register int i, nwords;

    for (i = 0; i < MAXSCROLLS; i++)
    {
	cp = prbuf;
	nwords = rnd(3) + 2;
	while (nwords--)
	{
	    nsyl = rnd(3) + 1;
	    while (nsyl--)
	    {
		sp = sylls[rnd((sizeof sylls) / (sizeof (char *)))];
		if (&cp[strlen(sp)] > &prbuf[MAXNAME])
			break;
		while (*sp)
		    *cp++ = *sp++;
	    }
	    *cp++ = ' ';
	}
	*--cp = '\0';
	s_names[i] = (char *) malloc((unsigned) strlen(prbuf)+1);
	strcpy(s_names[i], prbuf);
    }
}

/*
 * init_stones:
 *	Initialize the ring stone setting scheme for this time
 */
void
init_stones()
{
    register int i, j;

    for (i = 0; i < NSTONES; i++)
	used[i] = FALSE;
    for (i = 0; i < MAXRINGS; i++)
    {
	do
	    j = rnd(NSTONES);
	until (!used[j]);
	used[j] = TRUE;
	r_stones[i] = stones[j].st_name;
	ring_info[i].oi_worth += stones[j].st_value;
    }
}

/*
 * init_materials:
 *	Initialize the construction materials for wands and staffs
 */
void
init_materials()
{
    register int i, j;
    register char *str;
    static bool metused[NMETAL];

    for (i = 0; i < NWOOD; i++)
	used[i] = FALSE;
    for (i = 0; i < NMETAL; i++)
	metused[i] = FALSE;
    for (i = 0; i < MAXSTICKS; i++)
    {
	for (;;)
	    if (rnd(2) == 0)
	    {
		j = rnd(NMETAL);
		if (!metused[j])
		{
		    ws_type[i] = "wand";
		    str = metal[j];
		    metused[j] = TRUE;
		    break;
		}
	    }
	    else
	    {
		j = rnd(NWOOD);
		if (!used[j])
		{
		    ws_type[i] = "staff";
		    str = wood[j];
		    used[j] = TRUE;
		    break;
		}
	    }
	ws_made[i] = str;
    }
}

#ifdef MASTER
# define	NT	NUMTHINGS, "things"
# define	MP	MAXPOTIONS, "potions"
# define	MS	MAXSCROLLS, "scrolls"
# define	MR	MAXRINGS, "rings"
# define	MWS	MAXSTICKS, "sticks"
# define	MW	MAXWEAPONS, "weapons"
# define	MA	MAXARMORS, "armor"
#else
# define	NT	NUMTHINGS
# define	MP	MAXPOTIONS
# define	MS	MAXSCROLLS
# define	MR	MAXRINGS
# define	MWS	MAXSTICKS
# define	MW	MAXWEAPONS
# define	MA	MAXARMORS
#endif

/*
 * sumprobs:
 *	Sum up the probabilities for items appearing
 */
void
sumprobs(struct obj_info *info, int bound
#ifdef MASTER
	, char *name
#endif
)
{
#ifdef MASTER
	struct obj_info *start = info;
#endif
    struct obj_info *endp;

    endp = info + bound;
    while (++info < endp)
	info->oi_prob += (info - 1)->oi_prob;
#ifdef MASTER
    badcheck(name, start, bound);
#endif
}

/*
 * init_probs:
 *	Initialize the probabilities for the various items
 */
void
init_probs()
{
    sumprobs(things, NT);
    sumprobs(pot_info, MP);
    sumprobs(scr_info, MS);
    sumprobs(ring_info, MR);
    sumprobs(ws_info, MWS);
    sumprobs(weap_info, MW);
    sumprobs(arm_info, MA);
}

#ifdef MASTER
/*
 * badcheck:
 *	Check to see if a series of probabilities sums to 100
 */
void
badcheck(char *name, struct obj_info *info, int bound)
{
    register struct obj_info *end;

    if (info[bound - 1].oi_prob == 100)
	return;
    printf("\nBad percentages for %s (bound = %d):\n", name, bound);
    for (end = &info[bound]; info < end; info++)
	printf("%3d%% %s\n", info->oi_prob, info->oi_name);
    printf("[hit RETURN to continue]");
    fflush(stdout);
    while (getchar() != '\n')
	continue;
}
#endif

/*
 * pick_color:
 *	If he is halucinating, pick a random color name and return it,
 *	otherwise return the given color.
 */
char *
pick_color(char *col)
{
    return (on(player, ISHALU) ? rainbow[rnd(NCOLORS)] : col);
}
