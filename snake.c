/*==============================================================
 *  snake.c  ——  第 3 步：完整可玩的贪吃蛇
 *--------------------------------------------------------------
 *  功能：
 *    - W A S D 控制方向（不用方向键，原因见 README）
 *    - 吃到食物 +10 分，蛇变长，速度变快
 *    - 撞墙或撞到自己 → 游戏结束
 *    - P 暂停，R 重开，Q 退出
 *    - 画面原地刷新，不闪、不滚屏
 *
 *  编译（Windows / MinGW）：
 *      gcc snake.c -o snake.exe
 *      snake.exe
 *
 *  编译（Linux / macOS）：
 *      gcc snake.c -o snake
 *      ./snake
 *==============================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*==============================================================
 *  【平台适配层】
 *  这一段你暂时可以跳过，只需要知道它解决了三件事：
 *    1. gotoxy(x,y)   把光标移到屏幕第 x 列第 y 行（从 0 开始数）
 *    2. key_hit()     检查"玩家有没有按键"（有就返回真，没有就返回假）
 *    3. get_key()     读入玩家按下的那一个字符
 *  Windows 和 Linux 的写法不一样，所以用 #ifdef 分开写。
 *  这就是 C 语言的"条件编译"：编译器只会把满足条件的那一段
 *  编进程序里，另一段当作不存在。
 *==============================================================*/
#ifdef _WIN32
#include <conio.h>
#include <windows.h>

static void gotoxy(int x, int y)
{
    COORD pos;
    pos.X = (SHORT)x;
    pos.Y = (SHORT)y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}
static void hide_cursor(void)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(h, &info);
    info.bVisible = FALSE;              /* FALSE = 隐藏光标 */
    SetConsoleCursorInfo(h, &info);
}
static void show_cursor(void)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(h, &info);
    info.bVisible = TRUE;
    SetConsoleCursorInfo(h, &info);
}
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
static int  key_hit(void)    { return _kbhit(); }
static int  get_key(void)    { return _getch(); }
static void term_init(void)    { hide_cursor(); }
static void term_restore(void) { show_cursor(); }

#else   /* ---------- macOS / Linux ---------- */
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

static struct termios g_old_term;

static void gotoxy(int x, int y)
{
    printf("\033[%d;%dH", y + 1, x + 1);   /* ANSI 转义序列：光标移到指定行列 */
    fflush(stdout);
}
static void hide_cursor(void) { printf("\033[?25l"); fflush(stdout); }
static void show_cursor(void) { printf("\033[?25h"); fflush(stdout); }
static void sleep_ms(int ms)  { usleep(ms * 1000); }

/* 关掉"行缓冲"，这样按一个键就立刻能读到，不用等回车 */
static void term_init(void)
{
    struct termios t;
    tcgetattr(STDIN_FILENO, &g_old_term);
    t = g_old_term;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN]  = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    hide_cursor();
}
static void term_restore(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_old_term);
    show_cursor();
}
static int key_hit(void)
{
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}
static int get_key(void)
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return 0;
}
#endif

/*==============================================================
 *  【游戏常量】
 *==============================================================*/
#define WIDTH    20                    /* 棋盘宽（列） */
#define HEIGHT   20                    /* 棋盘高（行） */
#define MAX_LEN  (WIDTH * HEIGHT)      /* 蛇最长有多少节 */

#define START_SPEED 160                /* 一开始每帧等 160 毫秒 */
#define MIN_SPEED   55                 /* 最快也不能快过 55 毫秒 */

/* 方向用整数表示：0上 1下 2左 3右。
 * 用 enum（枚举）比直接写 0/1/2/3 可读性好得多。 */
enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

/* 方向对应的坐标偏移量。查表比写一堆 if 干净：
 * 想往 up 走，就把 x 加 dx[DIR_UP]、y 加 dy[DIR_UP]。 */
static const int dx[4] = {  0,  0, -1,  1 };
static const int dy[4] = { -1,  1,  0,  0 };

/*==============================================================
 *  【全局变量】
 *  小项目里用全局变量很方便；等以后写大程序，要学着把它们
 *  收进结构体里再传参（这是进阶话题）。
 *==============================================================*/
static int snake_x[MAX_LEN];   /* 蛇每一节的列坐标 */
static int snake_y[MAX_LEN];   /* 蛇每一节的行坐标 */
static int len;                /* 蛇当前长度（节数） */

static int food_x, food_y;     /* 食物坐标 */

static int dir;                /* 当前正在走的方向 */
static int next_dir;           /* 玩家最新按下的方向 */

static int score;              /* 分数 */
static int speed_ms;           /* 每帧间隔，越小越快 */

/*==============================================================
 *  画一帧
 *--------------------------------------------------------------
 *  为什么先拼到 buf 再整块输出？
 *  如果一边算一边 printf，光标会一路往下跑，屏幕会闪。
 *  先把整屏内容拼进一个 char 数组，再一次性 writing 出去，
 *  画面就稳了。这是"双缓冲"思想的极简版。
 *==============================================================*/
static void draw(void)
{
    static char buf[HEIGHT + 2][WIDTH + 3];   /* static 只分配一次 */
    int x, y, i;

    /* 1) 清空成空格 */
    for (y = 0; y < HEIGHT + 2; y++) {
        for (x = 0; x < WIDTH + 2; x++) {
            buf[y][x] = ' ';
        }
        buf[y][WIDTH + 2] = '\0';
    }

    /* 2) 画边框 */
    for (y = 0; y < HEIGHT + 2; y++) {
        for (x = 0; x < WIDTH + 2; x++) {
            if (y == 0 || y == HEIGHT + 1 || x == 0 || x == WIDTH + 1) {
                buf[y][x] = '#';
            }
        }
    }

    /* 3) 画食物 */
    buf[food_y + 1][food_x + 1] = '*';

    /* 4) 画蛇（注意从尾画到头，这样蛇头会盖住身体） */
    for (i = len - 1; i >= 0; i--) {
        char ch = (i == 0) ? '@' : 'O';
        buf[snake_y[i] + 1][snake_x[i] + 1] = ch;
    }

    /* 5) 一次性输出到屏幕左上角 */
    gotoxy(0, 0);
    for (y = 0; y < HEIGHT + 2; y++) {
        printf("%s\n", buf[y]);
    }
    printf("得分: %-4d 长度: %-4d 速度: %-4d\n", score, len, speed_ms);
    printf("W/A/S/D 移动   P 暂停   Q 退出\n");
    fflush(stdout);
}

/*==============================================================
 *  随机生成食物（不能生成在蛇身上）
 *==============================================================*/
static void new_food(void)
{
    int i, bad;
    do {
        food_x = rand() % WIDTH;
        food_y = rand() % HEIGHT;

        bad = 0;
        for (i = 0; i < len; i++) {
            if (snake_x[i] == food_x && snake_y[i] == food_y) {
                bad = 1;
                break;
            }
        }
    } while (bad);
}

/*==============================================================
 *  初始化 / 重开一局
 *==============================================================*/
static void init_game(void)
{
    int i;

    len = 3;
    for (i = 0; i < len; i++) {
        snake_x[i] = 8 - i;    /* 蛇头在 (8,10)，往左排开 */
        snake_y[i] = 10;
    }

    dir      = DIR_RIGHT;
    next_dir = DIR_RIGHT;
    score    = 0;
    speed_ms = START_SPEED;

    new_food();
}

/*==============================================================
 *  读键盘，把玩家按的键翻译成方向
 *  返回 1 表示"玩家要求退出程序"
 *==============================================================*/
static int read_input(void)
{
    while (key_hit()) {
        int c = get_key();

        switch (c) {
        /* 注意这里没有 break——"case 大小写都接受"是故意写成贯穿的：
         * 'w' 和 'W' 都要处理成"上"，所以落到同一个分支。
         * 这是 switch 里唯一推荐使用贯穿（fall-through）的场景。 */
        case 'w': case 'W': next_dir = DIR_UP;    break;
        case 's': case 'S': next_dir = DIR_DOWN;  break;
        case 'a': case 'A': next_dir = DIR_LEFT;  break;
        case 'd': case 'D': next_dir = DIR_RIGHT; break;

        case 'q': case 'Q':
            return 1;                       /* 退出 */

        case 'p': case 'P':
            /* 暂停：一直卡在这里，直到再按一次 P 或 Q */
            printf("\n\n  ===== 已暂停，按 P 继续 =====");
            while (1) {
                int k = get_key();
                if (k == 'p' || k == 'P') break;
                if (k == 'q' || k == 'Q') return 1;
                sleep_ms(30);
            }
            break;

        default:
            break;                          /* 其它键忽略 */
        }
    }
    return 0;
}

/*==============================================================
 *  主流程
 *==============================================================*/
int main(void)
{
    int quit  = 0;
    int over, grow, win, i, check_end;
    int hx, hy, tail_x, tail_y;

    srand((unsigned)time(NULL));
    term_init();          /* 隐藏光标；Linux 下还要关行缓冲 */
    atexit(term_restore); /* 程序无论怎么退出，都保证把终端还原。
                           * atexit() 注册的函数会在 main 返回时自动调用，
                           * 属于"资源清理"的好习惯。 */

    do {
        init_game();

        /* ---------------- 一局游戏的主循环 ---------------- */
        over = 0;
        win  = 0;
        while (!over) {

            /* ① 处理输入 */
            if (read_input()) { quit = 1; break; }

            /* ② 防止"180 度掉头"：蛇不能直接反向撞自己。
             *    比如正在往右走，按左键必须忽略。 */
            if (next_dir != (dir ^ 1)) {
                dir = next_dir;   /* dir ^ 1 就是"相反方向"：0<->1, 2<->3 */
            }

            /* ③ 算出蛇头的新位置 */
            hx = snake_x[0] + dx[dir];
            hy = snake_y[0] + dy[dir];

            /* ④ 判断撞墙 */
            if (hx < 0 || hx >= WIDTH || hy < 0 || hy >= HEIGHT) {
                over = 1;
                break;
            }

            /* ⑤ 判断这一步会不会吃到食物（决定尾巴要不要保留） */
            grow = (hx == food_x && hy == food_y);

            /* ⑥ 判断撞自己。
             *    要吃食物时，尾巴这一帧不动，所以整条蛇都要检查；
             *    不吃食物时，尾巴会挪走，最后一节不算（否则会误判）。 */
            check_end = grow ? len : len - 1;
            for (i = 0; i < check_end; i++) {
                if (snake_x[i] == hx && snake_y[i] == hy) {
                    over = 1;
                    break;
                }
            }
            if (over) break;

            /* ⑦ 记住旧尾巴位置（长身体时要用） */
            tail_x = snake_x[len - 1];
            tail_y = snake_y[len - 1];

            /* ⑧ 身体从后往前挪一格 —— 顺序千万不能反！ */
            for (i = len - 1; i > 0; i--) {
                snake_x[i] = snake_x[i - 1];
                snake_y[i] = snake_y[i - 1];
            }

            /* ⑨ 蛇头前进 */
            snake_x[0] = hx;
            snake_y[0] = hy;

            /* ⑩ 吃到食物：加分、变长、提速、刷新食物 */
            if (grow) {
                snake_x[len] = tail_x;
                snake_y[len] = tail_y;
                len++;
                score += 10;

                if (speed_ms > MIN_SPEED) speed_ms -= 5;  /* 越吃越快 */

                /* 极端情况：蛇已经铺满整个棋盘，没地方放食物了 */
                if (len >= MAX_LEN) {
                    win  = 1;
                    over = 1;
                    break;
                }
                new_food();
            }

            /* ⑪ 画出来，然后等一会儿 */
            draw();
            sleep_ms(speed_ms);
        }

        if (quit) break;

        /* ---------------- 结算画面 ---------------- */
        draw();
        if (win) {
            printf("\n  ===== 恭喜通关！你填满了整个棋盘 =====\n");
        } else {
            printf("\n  ===== 游戏结束 =====\n");
        }
        printf("  最终得分: %d    蛇的长度: %d\n", score, len);
        printf("  按 R 再来一局，按 Q 退出\n");

        /* 等玩家按 R 或 Q */
        while (1) {
            int k = get_key();
            if (k == 'r' || k == 'R') break;   /* 跳出内层，外层 do-while 重开 */
            if (k == 'q' || k == 'Q') { quit = 1; break; }
            sleep_ms(30);
        }
    } while (!quit);

    return 0;
}
