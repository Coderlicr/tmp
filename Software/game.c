/*
 * game.c -- Fruit-Ninja userspace game loop (ultrasonic version)
 *
 * Game rules:
 *   - Fruit appears at a random position and stays visible 3 s
 *   - Player "swipes": hand moves into < 20 cm of HC-SR04
 *     detected as FAR -> NEAR edge on ultra sample stream
 *   - Swipe while fruit visible        -> hit, score++, new fruit
 *   - Fruit timeout without swipe      -> miss, miss_count++, new fruit
 *   - 5 misses -> game over
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "top_module.h"

#define DEV_PATH        "/dev/top_module"

#define NEAR_CM         20u          /* FAR->NEAR threshold */
#define FRUIT_RADIUS    30u
#define FRUIT_LIFE_MS   3000         /* miss after 3 s without swipe */
#define POLL_INTERVAL_US 10000       /* 10 ms */
#define MAX_MISSES      5

#define SCREEN_W        640
#define SCREEN_H        480
/* keep fruit fully on screen with margin for radius */
#define FRUIT_X_MIN     (FRUIT_RADIUS + 10)
#define FRUIT_X_MAX     (SCREEN_W - FRUIT_RADIUS - 10)
#define FRUIT_Y_MIN     (FRUIT_RADIUS + 10)
#define FRUIT_Y_MAX     (SCREEN_H - FRUIT_RADIUS - 10)

/* Game state values written to GAME_STATE reg (for future use by FPGA) */
#define STATE_IDLE      0
#define STATE_PLAYING   1
#define STATE_OVER      2

static int g_fd = -1;
static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int s) { (void)s; g_stop = 1; }

static long long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int rand_range(int lo, int hi)
{
    return lo + rand() % (hi - lo + 1);
}

static int write_fruit(int x, int y, int r, int visible)
{
    struct fruit_cmd f;
    f.x = (unsigned short)x;
    f.y = (unsigned short)y;
    f.radius = (unsigned short)r;
    f.visible = visible ? 1 : 0;
    return ioctl(g_fd, WRITE_FRUIT, &f);
}

static int write_score(unsigned int s)     { return ioctl(g_fd, WRITE_SCORE, &s); }
static int write_state(unsigned int s)     { return ioctl(g_fd, WRITE_GAME_STATE, &s); }

static int read_ultra(struct ultra_sample *s)
{
    return ioctl(g_fd, READ_ULTRA, s);
}

static void spawn_fruit(int *fx, int *fy, long long *spawn_ms)
{
    *fx = rand_range(FRUIT_X_MIN, FRUIT_X_MAX);
    *fy = rand_range(FRUIT_Y_MIN, FRUIT_Y_MAX);
    *spawn_ms = now_ms();
    write_fruit(*fx, *fy, FRUIT_RADIUS, 1);
    printf("  spawn fruit @ (%d,%d)\n", *fx, *fy);
    fflush(stdout);
}

int main(void)
{
    signal(SIGINT, on_sigint);
    srand((unsigned)time(NULL));

    g_fd = open(DEV_PATH, O_RDWR);
    if (g_fd < 0) {
        fprintf(stderr, "open %s: %s\n", DEV_PATH, strerror(errno));
        return 1;
    }

    printf("=== Fruit Ninja (ultrasonic) ===\n");
    printf("Swipe hand within %u cm of HC-SR04 to cut.\n", NEAR_CM);
    printf("5 misses ends the game.  Ctrl-C to quit.\n\n");

    unsigned int score = 0;
    unsigned int misses = 0;
    int fx = 0, fy = 0;
    long long spawn_ms = 0;
    int prev_near = 0;
    unsigned int last_sample_id = 0;

    write_score(0);
    write_state(STATE_PLAYING);
    spawn_fruit(&fx, &fy, &spawn_ms);

    while (!g_stop && misses < MAX_MISSES) {
        struct ultra_sample u;
        if (read_ultra(&u) == 0) {
            /* only process when we have a fresh sample */
            int is_new = (u.status & ULTRA_STATUS_NEW_BIT) ||
                         (u.sample_id != last_sample_id);
            if (is_new) {
                last_sample_id = u.sample_id;
                unsigned int cm = u.last_echo_cnt / ULTRA_TICKS_PER_CM;
                int curr_near = (cm > 0 && cm < NEAR_CM);

                /* FAR -> NEAR rising edge = swipe */
                if (!prev_near && curr_near) {
                    /* hit: fruit is always visible when loop reaches here
                     * (we spawn a new one immediately on hit/miss).
                     * Single sensor gives no X/Y, so any swipe counts. */
                    score++;
                    printf("  swipe! (%u cm)  HIT!  score=%u\n", cm, score);
                    fflush(stdout);
                    write_score(score);
                    /* hide old fruit, spawn new */
                    write_fruit(0, 0, FRUIT_RADIUS, 0);
                    spawn_fruit(&fx, &fy, &spawn_ms);
                }
                prev_near = curr_near;
            }
        }

        /* fruit timeout -> miss */
        if (now_ms() - spawn_ms >= FRUIT_LIFE_MS) {
            misses++;
            printf("  miss (%u/%u)\n", misses, MAX_MISSES);
            fflush(stdout);
            write_fruit(0, 0, FRUIT_RADIUS, 0);
            if (misses >= MAX_MISSES) break;
            spawn_fruit(&fx, &fy, &spawn_ms);
        }

        usleep(POLL_INTERVAL_US);
    }

    /* game over */
    write_fruit(0, 0, FRUIT_RADIUS, 0);
    write_state(STATE_OVER);
    printf("\n=== GAME OVER ===\n");
    printf("Final score: %u\n", score);
    printf("Misses: %u\n", misses);

    close(g_fd);
    return 0;
}
