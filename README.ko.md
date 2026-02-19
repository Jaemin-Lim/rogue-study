# Rogue: 운명의 던전 탐험 — 상세 한국어 설명서

[![License](https://img.shields.io/badge/license-BSD-blue.svg)](LICENSE.TXT)

> **Rogue**는 로그라이크(Roguelike) 장르를 탄생시킨 최초의 그래픽 던전 탐험 게임입니다.  
> 이 문서는 학습 목적으로 소스코드의 구조와 동작 원리를 한국어로 자세히 설명합니다.

**원작자:** Michael Toy, Ken Arnold, Glenn Wichman (1980–1983, 1985, 1999)  
**버전:** 5.4.4

---

## 목차

1. [프로젝트 개요](#1-프로젝트-개요)
2. [각 파일의 역할](#2-각-파일의-역할)
3. [주요 데이터 구조](#3-주요-데이터-구조)
4. [주요 전역 변수](#4-주요-전역-변수)
5. [주요 함수 목록 및 역할](#5-주요-함수-목록-및-역할)
6. [게임 디자인 패턴과 기술](#6-게임-디자인-패턴과-기술)
7. [던전 생성 방식](#7-던전-생성-방식)
8. [아이템 생성 방식](#8-아이템-생성-방식)
9. [몬스터 생성 방식](#9-몬스터-생성-방식)
10. [플레이어 시스템](#10-플레이어-시스템)
11. [게임 루프 흐름](#11-게임-루프-흐름)
12. [전투 시스템](#12-전투-시스템)
13. [데몬과 퓨즈 시스템](#13-데몬과-퓨즈-시스템)
14. [저장 및 복원](#14-저장-및-복원)
15. [빌드 및 실행 방법](#15-빌드-및-실행-방법)

---

## 1. 프로젝트 개요

Rogue는 1980년대 초 Unix 시스템에서 탄생한 텍스트 기반 던전 탐험 게임입니다.  
플레이어는 절차적으로 생성(Procedurally Generated)된 던전을 탐험하며 몬스터와 싸우고, 아이템을 수집하여 26레벨 지하 깊숙이 있는 **Yendor의 부적(Amulet of Yendor)** 을 찾아 지상으로 귀환하는 것이 목표입니다.

### 핵심 특성

| 특성 | 내용 |
|------|------|
| 언어 | C (pre-C99 / C89 스타일) |
| UI | ncurses (터미널 기반) |
| 화면 크기 | 최소 24행 × 80열 |
| 맵 표현 | ASCII 문자 (`@` = 플레이어, `A`–`Z` = 몬스터 등) |
| 맵 배열 | `places[MAXCOLS * MAXLINES]` (PLACE 구조체) |
| 방 수 | 레벨당 최대 9개 (3×3 그리드) |
| 아이템 식별 | 매 게임마다 약물 색상, 두루마리 이름이 무작위로 재설정 |

---

## 2. 각 파일의 역할

모든 소스 파일은 루트 디렉터리에 있습니다.

### 핵심 진입점

| 파일 | 역할 |
|------|------|
| `main.c` | 프로그램 진입점(`main`). 초기화, 게임 루프 시작, 신호 처리 |
| `rogue.h` | 전체 프로젝트의 메인 헤더. 상수, 매크로, 타입 정의, 구조체 선언 및 함수 프로토타입 포함 |
| `extern.h` | 플랫폼 분기 정의(`#ifdef`)와 외부 변수 선언 |
| `extern.c` | 모든 전역 변수의 실제 정의 및 초기화 |
| `init.c` | 게임 시작 시 확률 테이블, 플레이어 스탯, 약물 색상, 두루마리 이름, 반지 돌 설정, 지팡이 재질 초기화 |

### 게임 루프 및 명령 처리

| 파일 | 역할 |
|------|------|
| `command.c` | 플레이어 입력 처리. 모든 키 입력을 해당 함수로 라우팅 |
| `move.c` | 플레이어 이동 처리. 이동 방향 계산, 문 통과, 통로 이동 |
| `io.c` | 화면 입출력. 메시지 출력(`msg`), 화면 갱신, 상태 표시줄 업데이트 |
| `misc.c` | 기타 게임 명령 처리 (식별, 이름 붙이기, 지도 보기 등) |
| `options.c` | 게임 옵션(`ROGUEOPTS`) 파싱 및 설정 화면 |

### 던전 및 레벨 생성

| 파일 | 역할 |
|------|------|
| `new_level.c` | 새 레벨 생성 총괄. 이전 레벨 정리 후 방, 통로, 아이템, 함정, 계단 배치 |
| `rooms.c` | 방(Room) 생성. 랜덤 크기와 위치 결정, 미로 방 생성, 방 안에 황금과 몬스터 배치 |
| `passages.c` | 통로(Passage) 생성. 연결 그래프 알고리즘으로 방들을 연결 |

### 몬스터 시스템

| 파일 | 역할 |
|------|------|
| `monsters.c` | 몬스터 생성(`new_monster`), 랜덤 몬스터 선택(`randmonster`), 방랑 몬스터 생성(`wanderer`), 몬스터 깨우기(`wake_monster`) |
| `chase.c` | 몬스터 AI. 플레이어 추적(`do_chase`), 경로 탐색(`chase`), 이동 실행(`runners`, `move_monst`) |

### 전투 시스템

| 파일 | 역할 |
|------|------|
| `fight.c` | 전투 핵심 로직. 공격 판정(`fight`, `attack`), 명중/회피 계산(`roll_em`, `swing`), 화염 투사체(`fire_bolt`) |
| `weapons.c` | 무기 타입 정의, 초기화(`init_weapon`), 투척 공격(`missile`) |
| `armor.c` | 방어구 타입 정의, 착용/탈착, 녹(rust) 처리 |

### 아이템 시스템

| 파일 | 역할 |
|------|------|
| `things.c` | 아이템 생성(`new_thing`), 인벤토리 이름 표시(`inv_name`), 아이템 버리기(`drop`), 발견 목록 표시 |
| `potions.c` | 약물(Potion) 효과 처리(`quaff`, `do_pot`) |
| `scrolls.c` | 두루마리(Scroll) 효과 처리(`read_scroll`) |
| `rings.c` | 반지(Ring) 착용/탈착 및 효과 처리 |
| `sticks.c` | 지팡이/완드(Wand/Staff) 사용 처리(`do_zap`) |

### 인벤토리 및 팩

| 파일 | 역할 |
|------|------|
| `pack.c` | 아이템 줍기(`pick_up`), 팩에 추가/제거(`add_pack`, `leave_pack`), 팩 표시(`inventory`) |
| `list.c` | 연결 리스트 조작(`_attach`, `_detach`, `_free_list`) |

### 데몬 및 타이머

| 파일 | 역할 |
|------|------|
| `daemon.c` | 데몬/퓨즈 시스템. 지연 액션 테이블 관리(`start_daemon`, `fuse`, `do_daemons`, `do_fuses`) |
| `daemons.c` | 실제 데몬 함수들: `runners`(몬스터 이동), `doctor`(체력 회복), `stomach`(배고픔), `swander`(방랑 몬스터 생성) |

### 저장/복원 및 상태 관리

| 파일 | 역할 |
|------|------|
| `save.c` | 게임 저장(`save_game`, `save_file`)과 복원(`restore`) |
| `state.c` | 직렬화/역직렬화. 게임 상태를 파일에 읽고 씀(`rs_save_file`, `rs_restore_file`) |

### 기타

| 파일 | 역할 |
|------|------|
| `rip.c` | 사망 화면 표시. 묘비 형태로 사망 정보 출력(`death`, `score`) |
| `wizard.c` | 위자드 모드 디버그 명령 처리 |
| `xcrypt.c` | 저장 파일 암호화/복호화 |
| `mach_dep.c` | 머신 의존적 코드 탐지 |
| `mdport.c` | 플랫폼 추상화 레이어. OS별 함수 구현 |
| `vers.c` | 버전 정보 |

---

## 3. 주요 데이터 구조

### 3-1. `THING` (union) — 몬스터와 오브젝트의 통합 타입

`rogue.h`에 정의된 핵심 union 타입입니다. 하나의 메모리 구조가 **몬스터(또는 플레이어)** 와 **아이템** 두 가지 역할을 모두 담당합니다.

```c
union thing {
    struct {                        /* 몬스터 / 플레이어 전용 필드 */
        union thing *_l_next, *_l_prev; /* 연결 리스트 포인터 */
        coord       _t_pos;         /* 현재 위치 (y, x) */
        bool        _t_turn;        /* 슬로우 상태일 때 이번 턴에 움직이는지 */
        char        _t_type;        /* 몬스터 종류 ('A'~'Z') */
        char        _t_disguise;    /* 미믹(X)이 위장한 모습 */
        char        _t_oldch;       /* 있던 자리의 원래 문자 */
        coord      *_t_dest;        /* 추적 목표 위치 포인터 */
        short       _t_flags;       /* 상태 비트 플래그 */
        struct stats _t_stats;      /* 전투 능력치 */
        struct room *_t_room;       /* 현재 위치한 방 포인터 */
        union thing *_t_pack;       /* 아이템 보유 목록 (연결 리스트) */
        int         _t_reserved;    /* 예약 필드 */
    } _t;
    struct {                        /* 아이템 전용 필드 */
        union thing *_l_next, *_l_prev;
        int          _o_type;       /* 아이템 종류 (POTION, SCROLL 등) */
        coord        _o_pos;        /* 바닥에 있을 때 위치 */
        char        *_o_text;       /* 두루마리 내용 */
        int          _o_launch;     /* 발사체에 필요한 발사 도구 타입 */
        char         _o_packch;     /* 인벤토리 표시 문자 (a~z) */
        char         _o_damage[8];  /* 근접 데미지 문자열 (예: "2x4") */
        char         _o_hurldmg[8]; /* 투척 데미지 문자열 */
        int          _o_count;      /* 복수 아이템 개수 */
        int          _o_which;      /* 같은 종류 중 세부 타입 번호 */
        int          _o_hplus;      /* 명중 보정값 */
        int          _o_dplus;      /* 데미지 보정값 */
        int          _o_arm;        /* 방어력 (방어구), 충전량 (지팡이), 금값 */
        int          _o_flags;      /* ISCURSED, ISKNOW 등 비트 플래그 */
        int          _o_group;      /* 황금 그룹 번호 */
        char        *_o_label;      /* 플레이어가 붙인 이름 */
    } _o;
};
typedef union thing THING;
```

**편의 매크로:** `rogue.h`는 `t_pos`, `o_type` 등 긴 멤버 경로를 짧은 이름으로 접근할 수 있도록 매크로를 정의합니다.

```c
#define t_pos    _t._t_pos
#define t_flags  _t._t_flags
#define o_type   _o._o_type
#define o_which  _o._o_which
/* ... */
```

---

### 3-2. `PLACE` — 맵 한 칸

```c
typedef struct {
    char   p_ch;     /* 해당 칸에 표시되는 문자 (FLOOR, WALL 등) */
    char   p_flags;  /* F_PASS(통로), F_SEEN(탐험됨), F_DROPPED(아이템 버려짐) 등 */
    THING *p_monst;  /* 이 칸에 있는 몬스터 포인터 (없으면 NULL) */
} PLACE;

extern PLACE places[];  /* 전체 맵: places[MAXCOLS * MAXLINES] */
```

맵 좌표 (y, x)에 접근하는 매크로:
```c
#define INDEX(y,x)  (&places[((x) << 5) + (y)])
#define chat(y,x)   (places[((x) << 5) + (y)].p_ch)    /* 문자 */
#define flat(y,x)   (places[((x) << 5) + (y)].p_flags) /* 플래그 */
#define moat(y,x)   (places[((x) << 5) + (y)].p_monst) /* 몬스터 */
```

---

### 3-3. `struct room` — 방 정보

```c
struct room {
    coord r_pos;         /* 방의 좌상단 좌표 */
    coord r_max;         /* 방의 너비·높이 (x=너비, y=높이) */
    coord r_gold;        /* 방 안 황금의 위치 */
    int   r_goldval;     /* 황금 가치 */
    short r_flags;       /* ISDARK(어두운 방), ISGONE(제거된 방), ISMAZE(미로) */
    int   r_nexits;      /* 출구 수 */
    coord r_exit[12];    /* 출구 좌표 배열 */
};
```

---

### 3-4. `struct stats` — 전투 능력치

플레이어와 몬스터 모두에 사용됩니다.

```c
struct stats {
    str_t s_str;      /* 힘 (unsigned int) */
    int   s_exp;      /* 경험치 */
    int   s_lvl;      /* 레벨 */
    int   s_arm;      /* 방어력 (낮을수록 좋음, AC 방식) */
    int   s_hpt;      /* 현재 HP */
    char  s_dmg[13];  /* 데미지 문자열 (예: "1x6+2x4") */
    int   s_maxhp;    /* 최대 HP */
};
```

---

### 3-5. `struct monster` — 몬스터 정의 테이블

`monsters.c`에 있는 `monsters[]` 배열의 각 항목입니다.

```c
struct monster {
    char        *m_name;   /* 몬스터 이름 (예: "kobold") */
    int          m_carry;  /* 아이템 보유 확률 (0~100) */
    short        m_flags;  /* ISMEAN, ISFLY, ISREGEN 등 */
    struct stats m_stats;  /* 초기 능력치 */
};
```

몬스터는 'A'~'Z' 26종이며 각각 배열 인덱스 `monsters[type - 'A']`로 접근합니다.

---

### 3-6. `struct obj_info` — 아이템 정보 테이블

```c
struct obj_info {
    char *oi_name;   /* 아이템 실제 이름 (알려진 경우) */
    int   oi_prob;   /* 등장 확률 (누적값, 0~100) */
    int   oi_worth;  /* 기본 가치 */
    char *oi_guess;  /* 플레이어가 추측하여 붙인 이름 */
    bool  oi_know;   /* 플레이어가 이 아이템을 식별했는지 */
};
```

각 아이템 종류별 전역 배열:
- `pot_info[]` — 약물 (14종)
- `scr_info[]` — 두루마리 (18종)
- `ring_info[]` — 반지 (14종)
- `ws_info[]` — 지팡이/완드 (14종)
- `weap_info[]` — 무기 (9종)
- `arm_info[]` — 방어구 (8종)
- `things[]` — 아이템 종류 별 등장 확률 (7종)

---

### 3-7. `coord` — 좌표

```c
typedef struct {
    int x;
    int y;
} coord;
```

---

### 3-8. `struct delayed_action` — 데몬/퓨즈 항목

```c
struct delayed_action {
    int    d_type;    /* EMPTY(0), BEFORE(1), AFTER(2), DAEMON(-1) */
    void (*d_func)(); /* 실행할 함수 포인터 */
    int    d_arg;     /* 함수에 전달할 인수 */
    int    d_time;    /* 남은 턴 수. DAEMON(-1)이면 매 턴 실행 */
};

struct delayed_action d_list[MAXDAEMONS]; /* MAXDAEMONS = 20 */
```

---

## 4. 주요 전역 변수

`extern.c`에 정의되며 `rogue.h`를 통해 외부에서 참조됩니다.

### 플레이어 관련

| 변수 | 타입 | 설명 |
|------|------|------|
| `player` | `THING` | 플레이어 객체 (`hero` 매크로 = `player.t_pos`) |
| `pstats` | `struct stats` | 플레이어 능력치 (`player.t_stats`의 매크로) |
| `pack` | `THING *` | 플레이어 인벤토리 연결 리스트 |
| `cur_weapon` | `THING *` | 현재 장착한 무기 |
| `cur_armor` | `THING *` | 현재 착용 중인 방어구 |
| `cur_ring[2]` | `THING *` | 현재 착용 중인 반지 (LEFT=0, RIGHT=1) |
| `purse` | `int` | 소지한 황금 |
| `level` | `int` | 현재 던전 층 (1부터 시작) |
| `max_level` | `int` | 도달한 가장 깊은 층 |
| `inpack` | `int` | 인벤토리 아이템 수 |
| `food_left` | `int` | 남은 배고픔 카운터 |
| `hungry_state` | `int` | 배고픔 단계 (0=보통, 1=배고픔, 2=허기, 3=굶주림) |
| `amulet` | `bool` | 부적 획득 여부 |

### 레벨/맵 관련

| 변수 | 타입 | 설명 |
|------|------|------|
| `rooms[MAXROOMS]` | `struct room` | 현재 레벨의 방 배열 (최대 9개) |
| `passages[MAXPASS]` | `struct room` | 통로를 방처럼 관리하는 배열 |
| `places[]` | `PLACE` | 전체 맵 배열 |
| `mlist` | `THING *` | 현재 레벨 몬스터 연결 리스트 |
| `lvl_obj` | `THING *` | 현재 레벨 바닥 아이템 연결 리스트 |
| `stairs` | `coord` | 계단 위치 |
| `ntraps` | `int` | 현재 레벨 함정 수 |
| `hero` | `coord` | 플레이어 위치 (매크로: `player.t_pos`) |

### 게임 상태

| 변수 | 타입 | 설명 |
|------|------|------|
| `playing` | `bool` | 게임 진행 중 여부 |
| `running` | `bool` | 플레이어가 달리는(자동 이동) 중인지 |
| `to_death` | `bool` | `f` 명령으로 죽을 때까지 싸우는 모드 |
| `wizard` | `int` | 위자드 모드 여부 |
| `seed` | `int` | 랜덤 시드 (`dnum`으로 초기화) |
| `dnum` | `int` | 던전 번호 (랜덤 시드 기반) |
| `terse` | `bool` | 짧은 메시지 모드 |
| `jump` | `bool` | 달리기 시 중간 과정 건너뛰기 |

---

## 5. 주요 함수 목록 및 역할

### `main.c`

| 함수 | 역할 |
|------|------|
| `main(argc, argv, envp)` | 프로그램 진입점. 환경 초기화 → 레벨 생성 → 데몬 시작 → `playit()` 호출 |
| `playit()` | 메인 게임 루프. `playing`이 true인 동안 `command()` 반복 호출 |
| `quit(sig)` | Q 키 처리. 확인 후 점수 기록 및 종료 |
| `rnd(range)` | 0 이상 range 미만 난수 반환 |
| `roll(number, sides)` | `number`개의 `sides`면 주사위 굴림 합산 |
| `fatal(s)` | 메시지 출력 후 프로그램 강제 종료 |

### `command.c`

| 함수 | 역할 |
|------|------|
| `command()` | 플레이어 입력 1회 처리. 데몬 실행 → 입력 읽기 → 명령 실행 → 몬스터 이동 |

주요 키 바인딩 (command.c 내부 switch):
```
h/j/k/l/y/u/b/n  이동 (8방향)
H/J/K/L/Y/U/B/N  해당 방향으로 달리기
e                 음식 먹기
q                 약물 마시기
r                 두루마리 읽기
w                 무기 장착
W                 방어구 착용
T                 방어구 탈착
P                 반지 착용
R                 반지 탈착
z                 지팡이 사용
t                 투척
d                 아이템 버리기
i                 인벤토리 보기
s                 탐색 (함정/비밀 통로)
f                 목표 향해 돌진
>                 계단 내려가기
S                 게임 저장
Q                 게임 종료
?                 도움말
```

### `new_level.c`

| 함수 | 역할 |
|------|------|
| `new_level()` | 새 레벨 생성 총괄. 이전 레벨 정리 → 방/통로/아이템/함정/계단 생성 |
| `put_things()` | 아이템 배치. MAXOBJ(9)번 시도, 각각 36% 확률로 아이템 생성. 26층 이상이면 부적 강제 배치 |
| `treas_room()` | 보물 방 생성 (확률: 1/20). 아이템과 강한 몬스터로 가득 채움 |
| `rnd_room()` | ISGONE이 아닌 유효한 방 번호 랜덤 선택 |

### `rooms.c`

| 함수 | 역할 |
|------|------|
| `do_rooms()` | 9개 방 생성. 일부 방 제거(left_out), 어두운 방/미로 설정, 황금/몬스터 배치 |
| `draw_room(rp)` | 방 그리기. 일반 방은 `vert`/`horiz`로 벽 그림. 미로 방은 `do_maze` 호출 |
| `do_maze(rp)` | 재귀적 미로 생성 알고리즘 |
| `dig(y, x)` | 미로 굴착. 재귀적으로 주변을 탐색하며 통로 연결 |
| `find_floor(rp, cp, limit, monst)` | 방 안에서 빈 바닥 좌표 찾기 |
| `enter_room(cp)` | 방 진입 시 처리. 방 전체를 화면에 렌더링 |
| `leave_room(cp)` | 방 탈출 시 처리. 어두운 방이면 바닥 숨김 |

### `passages.c`

| 함수 | 역할 |
|------|------|
| `do_passages()` | 방들 사이 통로 생성. 연결 그래프를 기반으로 모든 방이 연결되도록 통로 굴착 |
| `conn(r1, r2)` | 두 방 사이를 통로로 연결 |
| `add_pass()` | 통로 번호 부여 |

### `monsters.c`

| 함수 | 역할 |
|------|------|
| `randmonster(wander)` | 현재 층에 적합한 랜덤 몬스터 선택 |
| `new_monster(tp, type, cp)` | 몬스터 생성 및 초기화. 레벨에 따라 스탯 보정 |
| `wake_monster(y, x)` | 플레이어 근처 몬스터 활성화. 그리디 몬스터는 황금 수호 |
| `give_pack(tp)` | 몬스터에게 아이템 부여 (확률적) |
| `exp_add(tp)` | 몬스터 처치 경험치 계산 |
| `wanderer()` | 방랑 몬스터 생성 (플레이어 방 제외 위치에 생성) |
| `save(which)` | 플레이어의 내성 판정 |
| `save_throw(which, tp)` | 생물의 내성 판정 |

### `chase.c`

| 함수 | 역할 |
|------|------|
| `runners()` | ISRUN 상태인 모든 몬스터 이동 실행 (데몬으로 매 턴 호출) |
| `move_monst(tp)` | 몬스터 1회 이동. 슬로우/헤이스트 처리 |
| `do_chase(tp)` | 몬스터 추적 1스텝. 인접하면 공격, 아니면 이동 |
| `chase(tp, ee)` | 목표를 향한 최적 이동 방향 계산 |
| `find_dest(tp)` | 몬스터의 목표 위치 결정 |

### `fight.c`

| 함수 | 역할 |
|------|------|
| `fight(mp, weap, thrown)` | 플레이어가 몬스터를 공격 |
| `attack(mp)` | 몬스터가 플레이어를 공격 |
| `roll_em(thatt, thdef, weap, hurl)` | 공격 판정 및 데미지 계산 |
| `swing(at_lvl, op_arm, wplus)` | 명중 여부 판정 |
| `fire_bolt(start, dir, name)` | 화염/마법 투사체 발사 |
| `drain()` | 경험치 흡수 (레벨 몬스터 효과) |

### `init.c`

| 함수 | 역할 |
|------|------|
| `init_player()` | 플레이어 초기화. 능력치 설정, 초기 장비(링 메일, 메이스, 활, 화살) 지급 |
| `init_colors()` | 약물 색상 무작위 배정 (`rainbow[]` 배열에서 선택) |
| `init_names()` | 두루마리 이름 무작위 생성 (음절 조합) |
| `init_stones()` | 반지 보석 무작위 배정 (`stones[]` 배열에서 선택) |
| `init_materials()` | 지팡이/완드 재질 무작위 배정 (나무 또는 금속) |
| `init_probs()` | 아이템 등장 확률 테이블 누적합 계산 |

### `daemon.c`

| 함수 | 역할 |
|------|------|
| `start_daemon(func, arg, type)` | 데몬 등록 (매 턴 실행) |
| `fuse(func, arg, time, type)` | 퓨즈 등록 (일정 턴 후 1회 실행) |
| `kill_daemon(func)` | 데몬 제거 |
| `extinguish(func)` | 퓨즈 제거 |
| `lengthen(func, xtime)` | 퓨즈 시간 연장 |
| `do_daemons(flag)` | flag에 맞는 모든 데몬 실행 |
| `do_fuses(flag)` | flag에 맞는 모든 퓨즈 카운트 감소 및 실행 |

---

## 6. 게임 디자인 패턴과 기술

### 6-1. 데몬/퓨즈 패턴 (Observer/Timer Pattern)

Rogue의 가장 독창적인 설계 중 하나입니다. 고정 크기 배열 `d_list[20]`에 함수 포인터와 타이머를 저장하여 게임 이벤트를 비동기적으로 처리합니다.

- **데몬(Daemon):** `d_time == -1` → 매 턴(`do_daemons` 호출 시마다) 실행
- **퓨즈(Fuse):** `d_time > 0` → 지정된 턴 수 후 1회 실행 후 삭제

```
게임 시작 시 등록되는 데몬:
  runners  (AFTER)  → 매 턴: 모든 몬스터 이동
  doctor   (AFTER)  → 매 턴: 플레이어 HP 자연 회복
  stomach  (AFTER)  → 매 턴: 배고픔 카운터 감소
  swander  (AFTER)  → WANDERTIME턴 후: 방랑 몬스터 생성 (퓨즈)

상태 효과용 퓨즈 예시:
  unconfuse    → 혼란 상태 해제
  unsee        → 투명 보기 효과 종료
  stomach_heal → 회복 중단
```

### 6-2. Union 기반 다형성 (Union-based Polymorphism)

`THING` union은 C의 구조적 한계 내에서 다형성을 구현합니다. 동일한 메모리 레이아웃이 상황에 따라 몬스터/아이템으로 해석됩니다. 연결 리스트 포인터(`l_next`, `l_prev`)는 두 해석 모두에서 같은 위치에 있어 공통으로 사용됩니다.

### 6-3. 비트 플래그를 이용한 상태 관리

모든 상태 정보를 비트 플래그로 압축 저장합니다.

```c
/* 생물 상태 플래그 (t_flags) 예시 */
#define ISHASTE   0000100   /* 속도 증가 */
#define ISSLOW    0100000   /* 속도 감소 */
#define ISHELD    0000400   /* 묶임 */
#define ISHUH     0001000   /* 혼란 */
#define ISINVIS   0002000   /* 투명 */
#define ISRUN     0020000   /* 플레이어를 향해 달리는 중 */
#define ISMEAN    0004000   /* 방에 들어오면 깨어남 */
#define ISREGEN   0010000   /* 자연 회복 가능 */
#define ISFLY     0040000   /* 비행 가능 */

/* 플래그 확인 매크로 */
#define on(thing, flag) ((bool)(((thing).t_flags & (flag)) != 0))
```

### 6-4. 커스텀 제어 흐름 매크로

읽기 쉬운 코드를 위해 C의 `switch-case`를 Pascal/Ada 스타일처럼 쓸 수 있도록 매크로로 대체합니다.

```c
#define when       break; case  /* switch에서 다음 case처럼 사용 */
#define otherwise  break; default
#define until(e)   while(!(e))  /* do...until 루프 */
```

### 6-5. 연결 리스트

몬스터 목록(`mlist`), 바닥 아이템(`lvl_obj`), 인벤토리(`pack`)는 모두 `THING` 포인터로 연결된 단방향 연결 리스트입니다.

```c
/* list.c의 조작 함수들 */
attach(list, item)   /* 리스트 앞에 삽입 */
detach(list, item)   /* 리스트에서 제거 */
free_list(list)      /* 전체 리스트 해제 */

/* 순회 방법 */
for (tp = mlist; tp != NULL; tp = next(tp)) { ... }
```

### 6-6. 절차적 생성 (Procedural Generation)

매 게임마다 다른 경험을 제공하기 위해 광범위하게 무작위화를 사용합니다:
- 던전 레이아웃 (방 위치, 크기, 통로 경로)
- 아이템 외관 (약물 색상, 두루마리 이름, 반지 보석, 지팡이 재질)
- 몬스터와 아이템 배치
- 함정 위치와 종류
- 씨앗: `dnum = time(NULL) + pid` (위자드 모드에서는 `SEED` 환경변수로 고정 가능)

### 6-7. `register` 키워드

pre-C99 관례로, 자주 접근하는 지역 변수에 `register`를 붙여 컴파일러에게 레지스터 저장을 권장합니다. 현대 컴파일러는 이를 무시하지만 원본 코드 스타일을 그대로 유지합니다.

---

## 7. 던전 생성 방식

던전의 각 레벨은 `new_level()` 호출 시 새로 생성됩니다.

### 7-1. 방 생성 (`do_rooms`)

1. 화면(24×80)을 **3×3 그리드**로 나눠 9개의 구역을 만듭니다.
2. 각 구역에서 `rnd(4)`개의 방을 랜덤하게 "제거(ISGONE)" 처리합니다.
3. 남은 방들은 다음과 같이 생성됩니다:
   - **일반 방**: 최소 4×4 크기의 랜덤 직사각형
   - **어두운 방(ISDARK)**: 레벨이 높을수록 확률 증가 (`rnd(10) < level - 1`)
   - **미로 방(ISMAZE)**: 1/15 확률로 미로 형태로 변환
4. 방 안에 황금(50% 확률)과 몬스터를 배치합니다.

```
[방 배치 예시: 3×3 그리드]
+-------+-------+-------+
| room0 | room1 | room2 |
+-------+-------+-------+
| room3 | room4 | room5 |
+-------+-------+-------+
| room6 | room7 | room8 |
+-------+-------+-------+
```

### 7-2. 통로 생성 (`do_passages`)

**연결 그래프 알고리즘**을 사용합니다:
1. 인접 가능한 방들의 연결 관계를 미리 정의한 인접 행렬(`rdes[MAXROOMS]`)을 초기화합니다.
2. 랜덤으로 시작 방을 선택하여 그래프에 추가합니다.
3. 그래프 내의 방과 외부 방 사이를 무작위로 연결(`conn`)합니다.
4. 모든 방이 그래프에 포함될 때까지 반복합니다.
5. 추가로 일부 방 사이를 더 연결하여 순환 통로를 만듭니다.

`conn(r1, r2)` 함수는 두 방을 수평 또는 수직 통로로 연결합니다. 두 방이 나란히 있으면 직선 통로를, 그렇지 않으면 L자형 통로를 만듭니다.

### 7-3. 아이템 배치 (`put_things`)

1. 1/20 확률로 **보물 방(Treasure Room)** 생성: 아이템과 강한 몬스터를 가득 채운 방
2. MAXOBJ(9)번의 시도를 반복합니다. 각 시도마다 36% 확률로 아이템을 랜덤 바닥에 배치합니다.
3. 레벨 26 이상이고 아직 부적을 찾지 못했다면 **Yendor의 부적**을 강제 배치합니다.

### 7-4. 함정 배치

레벨 × 10% 확률로 함정 배치 시도. 배치 수: `rnd(level/4) + 1` (최대 MAXTRAPS=10개)

함정 종류 (NTRAPS=8):
- T_DOOR: 함정 문 (아래 층으로 떨어짐)
- T_ARROW: 화살 함정
- T_SLEEP: 수면 가스 함정
- T_BEAR: 곰 덫
- T_TELEP: 순간이동 함정
- T_DART: 독 화살 함정
- T_RUST: 녹 함정 (방어구 손상)
- T_MYST: 신비 함정

### 7-5. 미로 생성 (`do_maze` / `dig`)

재귀적 백트래킹 알고리즘을 사용합니다:
1. 2칸 단위의 격자에서 랜덤 시작점 선택
2. 방문하지 않은 인접 칸으로 이동하며 통로 생성
3. 이동 불가하면 재귀 호출 종료 (백트래킹)

---

## 8. 아이템 생성 방식

### 8-1. 아이템 종류 선택 (`new_thing`)

`things[]` 배열의 확률 테이블을 기반으로 아이템 종류를 선택합니다. 3레벨 이상 음식이 없으면 강제로 음식을 생성합니다.

| 인덱스 | 종류 | 기본 확률 |
|--------|------|-----------|
| 0 | POTION (약물) | 27% |
| 1 | SCROLL (두루마리) | 30% |
| 2 | FOOD (음식) | 17% |
| 3 | WEAPON (무기) | 9% |
| 4 | ARMOR (방어구) | 9% |
| 5 | RING (반지) | 5% |
| 6 | STICK (지팡이/완드) | 3% |

### 8-2. 세부 타입 선택

각 종류 내에서 `pick_one(info, nitems)` 함수로 세부 타입을 확률적으로 선택합니다.

**약물 (14종) 예시:**
- P_HEALING (치유): 약 15% 확률
- P_STRENGTH (힘 증가), P_POISON (독), P_CONFUSE (혼란) 등

**두루마리 (18종) 예시:**
- S_IDENTIFY 계열 (아이템 식별): 각 10% 전후
- S_MAP (지도 공개), S_TELEP (순간이동), S_ARMOR (갑옷 강화) 등

### 8-3. 외관 무작위화

게임 시작 시 `init_colors/init_names/init_stones/init_materials`로 각 게임마다 외관이 다르게 설정됩니다:

- **약물**: `rainbow[]` 배열 (27색)에서 무작위 색상 배정
- **두루마리**: `sylls[]` 배열의 음절로 무작위 이름 생성 (2~4단어)
- **반지**: `stones[]` 배열 (26종 보석)에서 무작위 설정
- **지팡이/완드**: `wood[]`(33종 나무) 또는 `metal[]`(22종 금속)에서 무작위 재질

### 8-4. 저주/강화

- **무기**: 10% 확률 저주, 5% 확률 강화 (`o_hplus` 조정)
- **방어구**: 20% 확률 저주, 8% 확률 강화 (`o_arm` 조정)
- **반지**: 일부 종류는 고정 저주 (R_AGGR, R_TELEPORT)

### 8-5. 플레이어의 아이템 식별

처음에는 외관만 보입니다 (`oi_know = FALSE`). 사용하거나 식별 두루마리를 읽으면 `oi_know = TRUE`로 변경되어 진짜 이름이 표시됩니다.

---

## 9. 몬스터 생성 방식

### 9-1. 몬스터 종류

총 26종, 알파벳 'A'~'Z'로 표시됩니다 (`monsters[type - 'A']`로 접근).

```
(대략적인 강도 순서)
K=Kobold  E=Emu     B=Bat     S=Snake    H=Hobgoblin
I=Ice Mon R=Rattlesnake O=Orc  Z=Zombie  L=Leprechaun
C=Centaur Q=Quasit  A=Aquator N=Nymph   Y=Yeti
F=Violet Fungi T=Troll W=Wraith P=Phantom X=Xeroc
U=Ur-Vile M=Medusa  V=Vampire G=Griffin J=Jabberwocky
D=Dragon
```

### 9-2. 레벨별 몬스터 선택 (`randmonster`)

```c
/* lvl_mons[] 배열 (강도 순서로 정렬) */
static char lvl_mons[] = {
    'K', 'E', 'B', 'S', 'H', 'I', 'R', 'O', 'Z', 'L',
    'C', 'Q', 'A', 'N', 'Y', 'F', 'T', 'W', 'P', 'X',
    'U', 'M', 'V', 'G', 'J', 'D'
};
```

선택 공식:
```c
d = level + (rnd(10) - 6);  /* 현재 층 ± 5 범위에서 선택 */
if (d < 0)  d = rnd(5);     /* 최소 0 이상 */
if (d > 25) d = rnd(5) + 21; /* 최대 25 이하 */
```

### 9-3. 몬스터 스탯 계산 (`new_monster`)

```
기본 레벨    = monsters[type - 'A'].m_stats.s_lvl
레벨 보정    = max(0, current_dungeon_level - AMULETLEVEL)
최종 레벨    = 기본 레벨 + 레벨 보정
HP           = roll(최종 레벨, 8)  /* 레벨 × d8 */
방어력       = 기본 방어력 - 레벨 보정
경험치       = 기본 경험치 + 레벨 보정 × 10 + exp_add()
```

레벨 30 이상: 모든 몬스터에게 ISHASTE(속도 증가) 자동 부여

### 9-4. 특수 몬스터 동작

| 플래그 | 몬스터 예 | 동작 |
|--------|-----------|------|
| ISMEAN | 대부분 | 방 진입 시 플레이어 추적 시작 |
| ISGREED | Leprechaun | 황금 지키기 |
| ISFLY | 박쥐(B) 등 | 매 턴 2번 이동 |
| ISREGEN | Troll 등 | HP 자동 회복 |
| CANHUH | Medusa(M) | 플레이어 혼란 유발 |
| CANSEE | 일부 | 투명 플레이어 감지 |

`X` 타입(Xeroc): 플레이어 근처 아이템으로 위장합니다.

---

## 10. 플레이어 시스템

### 10-1. 초기 능력치 (`max_stats`)

`extern.c`에 정의된 초기 능력치:
- 힘: 16, 경험치: 0, 레벨: 1, 방어력: 10, HP: 12, 최대 HP: 12

### 10-2. 초기 장비 (`init_player`)

| 아이템 | 설명 |
|--------|------|
| 링 메일 (Ring Mail) +1 | 초기 방어구, AC 7 |
| 메이스 (Mace) +1 공격/+1 데미지 | 초기 무기 |
| 활 (Bow) +1 | 원거리 무기 |
| 화살 25~40개 | 활에 사용 |
| 식량 1개 | 초기 식량 |

### 10-3. 레벨 시스템

경험치 획득 시 `check_level()`에서 레벨업 처리:
- 각 레벨에 필요한 경험치: `e_levels[]` 배열로 정의
- 레벨업 시 HP 증가 (d10 주사위), 레벨 변수 증가

### 10-4. 배고픔 시스템 (`stomach` 데몬)

```
food_left 초기값: HUNGERTIME = 1300
매 턴 1 감소 (일부 반지는 소모 증가/감소)

food_left <= STARVETIME(850):  배고픔(Hungry) 상태
food_left <= MORETIME(150):    허기(Weak) 상태  
food_left == 0:                굶주림(Fainting) → 가끔 기절
```

### 10-5. HP 회복 시스템 (`doctor` 데몬)

- 전투 중이 아닐 때 HEALTIME(30)턴마다 1 HP 회복
- R_REGEN 반지 착용 시 더 빠른 회복
- 레벨이 높을수록 더 많이 회복

### 10-6. 인벤토리

- 최대 MAXPACK(23) 슬롯
- 각 슬롯은 'a'~'w' 문자로 표시
- `pack_used[26]` 배열로 사용 중인 슬롯 추적

### 10-7. 힘 시스템

- `s_str`: 현재 힘 (0~31, 표시는 1~18/99 형식)
- 힘에 따라 명중률(`str_plus[]`)과 데미지(`add_dam[]`) 보정
- R_ADDSTR 반지, P_STRENGTH 약물로 변경 가능

---

## 11. 게임 루프 흐름

```
main()
  │
  ├─ md_init()                    플랫폼 초기화
  ├─ 환경 변수 파싱 (ROGUEOPTS)
  ├─ open_score()                 점수 파일 열기
  ├─ initscr()                    ncurses 초기화
  ├─ init_probs()                 확률 테이블 초기화
  ├─ init_player()                플레이어 초기화
  ├─ init_names/colors/stones/materials  아이템 외관 초기화
  ├─ new_level()                  첫 번째 레벨 생성
  │
  ├─ start_daemon(runners, ...)   몬스터 이동 데몬 등록
  ├─ start_daemon(doctor, ...)    HP 회복 데몬 등록
  ├─ fuse(swander, ..., WANDERTIME)  방랑 몬스터 퓨즈 등록
  ├─ start_daemon(stomach, ...)   배고픔 데몬 등록
  │
  └─ playit()
       │
       └─ while (playing)
            └─ command()
                 │
                 ├─ do_daemons(BEFORE)   사전 데몬 실행
                 ├─ do_fuses(BEFORE)
                 ├─ look(TRUE)           주변 탐색 및 몬스터 감지
                 ├─ status()             상태 표시줄 갱신
                 ├─ refresh()            화면 갱신
                 │
                 ├─ [입력 읽기]
                 │    running/to_death: 방향키 자동 반복
                 │    그 외: readchar() 입력 대기
                 │
                 ├─ [명령 실행]          do_move/fight/quaff/read 등
                 │
                 ├─ do_daemons(AFTER)    사후 데몬 실행
                 ├─ do_fuses(AFTER)
                 └─ (몬스터들은 runners 데몬으로 이동)
```

---

## 12. 전투 시스템

### 12-1. 명중 판정 (`swing`)

```
주사위 결과 (d20) >= (21 - 공격자 레벨 - 무기 명중 보정 - 힘 보정) - 방어구 AC
```

### 12-2. 데미지 계산 (`roll_em`)

1. 무기의 데미지 문자열 파싱 (예: "2x4" → 2d4)
2. `roll(number, sides)` 함수로 주사위 굴림
3. 데미지 보정(o_dplus)과 힘 보정(`add_dam[]`) 추가
4. 보물 방에서 몬스터가 황금을 가져간 경우 추가 분노 효과

### 12-3. 전투 결과

- 몬스터 HP <= 0: `killed()` 호출 → 경험치 부여, 아이템 드롭 처리
- 플레이어 HP <= 0: `death()` 호출 → 묘비 화면 출력

---

## 13. 데몬과 퓨즈 시스템

`d_list[20]` 배열에 최대 20개의 데몬/퓨즈를 동시에 등록할 수 있습니다.

### 주요 데몬

| 함수 | 실행 시점 | 역할 |
|------|-----------|------|
| `runners()` | 매 턴 AFTER | ISRUN 상태 몬스터 이동 |
| `doctor()` | 매 턴 AFTER | 전투 외 상황에서 HP 회복 |
| `stomach()` | 매 턴 AFTER | 배고픔 카운터 감소 |

### 주요 퓨즈

| 함수 | 지속 시간 | 역할 |
|------|-----------|------|
| `swander()` | WANDERTIME(70) | 방랑 몬스터 생성 → 완료 후 재등록 |
| `unconfuse()` | HUHDURATION(20) | 혼란 상태 해제 |
| `unsee()` | SEEDURATION(850) | 투명 보기 효과 종료 |
| `nohaste()` | 특정 턴 수 | 속도 증가 효과 종료 |
| `noslow()` | 특정 턴 수 | 속도 감소 효과 종료 |

---

## 14. 저장 및 복원

### 저장 (`save_game`)

1. 플레이어 홈 디렉터리의 `rogue.save` 파일에 저장
2. `xcrypt.c`의 암호화 함수(`encwrite`)로 저장 데이터 XOR 암호화
3. `rs_save_file()` (state.c): 게임 전체 상태(맵, 아이템, 몬스터, 플레이어) 직렬화
4. 복구 불가 방지를 위해 심볼릭 링크 저장 금지

### 복원 (`restore`)

1. `rs_restore_file()` (state.c): 저장 데이터 역직렬화
2. 포인터를 고정 오프셋으로 재계산 (바이너리 저장의 이식성 제한)
3. `restore()`는 성공하면 복귀하지 않고 직접 게임 루프 진입

---

## 15. 빌드 및 실행 방법

```bash
# 빌드
./configure
make

# 실행
./rogue

# 점수 보기
./rogue -s

# 저장 게임 복원
./rogue ~/rogue.save

# 위자드 모드 활성화 (--enable-wizardmode 빌드 필요)
./rogue ""
```

**필수 요구사항:**
- C 컴파일러 (GCC 또는 Clang)
- ncurses 라이브러리
- 터미널: 최소 24행 × 80열

자세한 빌드 방법은 [README.md](README.md)를 참고하세요.

---

## 참고

이 프로젝트는 로그라이크 장르의 원형으로, 이후 NetHack, Angband, DCSS 등 수많은 게임에 영감을 주었습니다. 소스코드는 1980년대 Unix C 프로그래밍의 전형적인 패턴을 담고 있어 C 언어 학습과 게임 프로그래밍 역사 연구에 훌륭한 자료입니다.

---

**저작권:** Copyright (C) 1980–1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman  
**라이선스:** BSD 스타일 라이선스 — [LICENSE.TXT](LICENSE.TXT) 참고
