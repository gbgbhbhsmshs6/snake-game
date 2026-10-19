/*==============================================================
 *  step2_snake.c  ——  第 2 步：让蛇自己跑起来
 *--------------------------------------------------------------
 *  这一步开始有"游戏"的样子了：
 *    1. 用两个一维数组存蛇身上每一节的坐标
 *    2. 每走一步，把后一节挪到前一节的位置（数组整体后移）
 *    3. 吃到食物就变长（长度 +1），撞墙就 Game Over
 *    4. 用死循环 + sleep 做出"一帧一帧"的动画
 *
 *  先不加键盘控制，让蛇一直向右跑。
 *  这样你能专心搞懂"蛇是怎么动的"这个核心问题，
 *  等逻辑彻底想通了，第 3 步再加按键，会轻松很多。
 *
 *  编译： gcc step2_snake.c -o step2
 *  运行： step2
 *==============================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>          /* Windows 的 Sleep() 在这里 */
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>           /* Linux / macOS 的 usleep() 在这里 */
#define SLEEP_MS(ms) usleep((ms) * 1000)   /* usleep 的单位是微秒 */
#endif

#define WIDTH    20
#define HEIGHT   20
#define MAX_LEN  (WIDTH * HEIGHT)   /* 蛇最长能有多长：铺满整个棋盘 */

/* ---------- 蛇的身体 ----------
 * 关键思路：不要用结构体，就用两个平行的一维数组。
 *   snake_x[i] 和 snake_y[i] 一起表示"第 i 节"的位置。
 *   约定 i = 0 是蛇头，i = len-1 是蛇尾。
 * 这样"移动"就变成了很简单的数组操作。 */
int snake_x[MAX_LEN];
int snake_y[MAX_LEN];
int len;             /* 蛇当前有几节 */

int food_x, food_y;  /* 食物位置 */

/* 画格子：把当前游戏状态画到屏幕上 */
void draw(void)
{
    /* 双层循环直接按坐标输出，不用二维数组也能画，
     * 每画一行打印一个换行即可。 */
    int x, y, i;
    int is_snake, is_food, is_border;

    for (y = 0; y < HEIGHT + 2; y++) {
        for (x = 0; x < WIDTH + 2; x++) {
            is_border = (y == 0 || y == HEIGHT + 1 ||
                         x == 0 || x == WIDTH + 1);
            is_food   = (x == food_x + 1 && y == food_y + 1);
            is_snake  = 0;

            for (i = 0; i < len; i++) {
                if (x == snake_x[i] + 1 && y == snake_y[i] + 1) {
                    is_snake = (i == 0) ? 2 : 1;  /* 2=头 1=身体 */
                    break;
                }
            }

            if (is_border)      printf("#");
            else if (is_snake == 2) printf("@");   /* 蛇头 */
            else if (is_snake == 1) printf("O");   /* 身体 */
            else if (is_food)   printf("*");       /* 食物 */
            else                printf(" ");
        }
        printf("\n");
    }
}

/* 在棋盘上随机放一颗食物，但不能放在蛇身上 */
void new_food(void)
{
    int i, bad;
    do {
        food_x = rand() % WIDTH;
        food_y = rand() % HEIGHT;

        bad = 0;
        for (i = 0; i < len; i++) {
            if (snake_x[i] == food_x && snake_y[i] == food_y) {
                bad = 1;   /* 撞到蛇了，这个位置不能用 */
                break;
            }
        }
    } while (bad);   /* 一直重试，直到找到一个安全位置 */
}

int main(void)
{
    int i, head_x, head_y, tail_x, tail_y;
    int score = 0;
    int running = 1;
    int step = 0;      /* 走了几步，用来计分和判断输赢 */

    srand((unsigned)time(NULL));

    /* ---------- 初始化：3 节长的蛇，横躺在棋盘中间 ---------- */
    len = 3;
    for (i = 0; i < len; i++) {
        snake_x[i] = 10 - i;   /* 头在 (10,10)，身体在 (9,10) (8,10) */
        snake_y[i] = 10;
    }
    new_food();

    /* ---------- 游戏主循环 ---------- */
    while (running) {
        /* 1) 先算蛇头下一步要去哪。这里固定往右走 +1 */
        head_x = snake_x[0] + 1;
        head_y = snake_y[0];

        /* 2) 判断撞墙 */
        if (head_x < 0 || head_x >= WIDTH || head_y < 0 || head_y >= HEIGHT) {
            printf("\n撞墙了！游戏结束。得分：%d\n", score);
            running = 0;
            break;
        }

        /* 3) 身体跟着走：从尾巴开始，每一节都挪到前一节的位置。
         *    必须"从后往前"挪！如果从前往后挪，前一节已经被
         *    覆盖掉了，后面的节会全部变成蛇头的位置。
         *    这是这个项目最容易写错的地方，画个图想一想就明白了。
         *    挪之前先把"旧尾巴"的位置记下来，因为一旦挪完它就
         *    被覆盖掉了，而"长身体"时正好需要这一格。 */
        tail_x = snake_x[len - 1];
        tail_y = snake_y[len - 1];

        for (i = len - 1; i > 0; i--) {
            snake_x[i] = snake_x[i - 1];
            snake_y[i] = snake_y[i - 1];
        }

        /* 4) 蛇头前进到新位置 */
        snake_x[0] = head_x;
        snake_y[0] = head_y;

        /* 5) 判断有没有吃到食物 */
        if (head_x == food_x && head_y == food_y) {
            /* 吃到食物 → 尾巴不动，长度 +1。
             * 做法就是把"旧尾巴"那一格当成新的一节补在数组末尾。 */
            snake_x[len] = tail_x;
            snake_y[len] = tail_y;
            len++;
            score += 10;
            new_food();
        }

        /* 6) 重画 + 等待一会儿，形成动画 */
        draw();
        printf("得分：%d   长度：%d   已走 %d 步\n", score, len, ++step);
        SLEEP_MS(150);   /* 每帧停 150 毫秒，越小跑得越快 */
    }

    return 0;
}
