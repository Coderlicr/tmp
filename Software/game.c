/*
 * game.c -- Fruit-Ninja userspace game loop (ultrasonic + bombs)
 *
 * - Objects spawn from bottom with varied initial vx/vy -> parabolic arcs
 *   across the screen (upward with sideways drift, fall back down).
 * - 70% of spawned objects are fruit (orange), 30% are bombs (dark purple).
 * - Swipe = hand enters NEAR_CM of HC-SR04 (FAR -> NEAR edge).
 * - Swipe while an object is on-screen:
 *       fruit  -> score++
 *       bomb   -> lives--
 * - Fruit falling off bottom without being cut also costs a life.
 * - Missed bomb (falls off) is free.
 * - Lives reach 0 -> game over.
 *
 * Game-state register (GAME_ST_WRITE) bits:
 *   bit0 = current object is bomb (renders dark purple on FPGA)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "top_module.h"

/* ---- Tuning ---- */
#define Q              4            /* quarter-pixel fixed point */
#define GRAVITY_Q      2            /* qpx / frame^2 */
/* Initial downward speed (positive = down), qpx/frame */
#define VY_MIN_Q       16
#define VY_MAX_Q       32
#define VX_MAX_Q       40           /* horizontal speed magnitude, qpx/frame */
#define LINE_Y         240          /* cutting line y coordinate */
#define RADIUS_MIN     22
#define RADIUS_MAX     34
#define FRAME_US       33000        /* ~30 fps */

#define NEAR_CM        20u          /* swipe threshold in cm */
#define MAX_LIVES      3
#define BOMB_PERCENT   30           /* 0..100, chance that new object is bomb */
#define CUT_HOLDOFF_US 350000       /* brief pause after a cut */

#define DEV_PATH       "/dev/top_module"

/* ---- State ---- */
static int  fd;
static int  score;
static int  lives;
static volatile sig_atomic_t g_stop = 0;

/* object physics: x in pixels, y/vy in qpx, vx in qpx/frame */
static int  obj_x;
static int  obj_y_q;
static int  obj_vx_q;
static int  obj_vy_q;
static int  obj_radius;
static int  obj_visible;
static int  obj_is_bomb;
static int  obj_already_cut;

/* swipe detection */
static int          prev_near;
static unsigned int last_sample_id;

static void on_sigint(int s) { (void)s; g_stop = 1; }

static int rand_range(int lo, int hi)
{
    return lo + rand() % (hi - lo + 1);
}

/* ---- Hardware writes ---- */
static void write_fruit_hw(void)
{
    int y_pixel = obj_y_q / Q;
    if (y_pixel < 0)    y_pixel = 0;
    if (y_pixel > 1023) y_pixel = 1023;
    int x_pixel = obj_x;
    if (x_pixel < 0)    x_pixel = 0;
    if (x_pixel > 1023) x_pixel = 1023;

    fruit_params_t fp;
    fp.x       = (unsigned int)x_pixel;
    fp.y       = (unsigned int)y_pixel;
    fp.radius  = (unsigned int)obj_radius;
    fp.visible = (unsigned int)(obj_visible ? 1 : 0);
    ioctl(fd, FRUIT_WRITE, &fp);
}

static void write_score_hw(void)
{
    unsigned int s = (unsigned int)score;
    ioctl(fd, SCORE_WRITE, &s);
}

static void write_state_hw(void)
{
    unsigned int s = obj_is_bomb ? 1u : 0u;
    ioctl(fd, GAME_ST_WRITE, &s);
}

/* ---- Object spawn / update ---- */
/*
 * Spawn zones (always above the cutting line, guaranteed to cross it):
 *   0. TOP:         from near top edge, straight / slightly angled downward
 *   1. UPPER-LEFT:  from upper-left quadrant, drifting right-down
 *   2. UPPER-RIGHT: from upper-right quadrant, drifting left-down
 */
static void spawn_object(void)
{
    int zone = rand() % 3;
    obj_vy_q  = rand_range(VY_MIN_Q, VY_MAX_Q);    /* always downward */
    obj_radius   = rand_range(RADIUS_MIN, RADIUS_MAX);
    obj_visible  = 1;
    obj_already_cut = 0;
    obj_is_bomb  = (rand() % 100) < BOMB_PERCENT;

    switch (zone) {
    case 0: /* TOP: random x in middle third, small ±vx */
        obj_x    = rand_range(SCREEN_W / 4, 3 * SCREEN_W / 4);
        obj_y_q  = rand_range(10, 40) * Q;
        obj_vx_q = rand_range(-VX_MAX_Q / 2, VX_MAX_Q / 2);
        break;
    case 1: /* UPPER-LEFT: spawn left, drift right */
        obj_x    = rand_range(30, SCREEN_W / 4);
        obj_y_q  = rand_range(10, LINE_Y / 2) * Q;
        obj_vx_q = rand_range(VX_MAX_Q / 2, VX_MAX_Q);
        break;
    case 2: /* UPPER-RIGHT: spawn right, drift left */
        obj_x    = rand_range(3 * SCREEN_W / 4, SCREEN_W - 30);
        obj_y_q  = rand_range(10, LINE_Y / 2) * Q;
        obj_vx_q = rand_range(-VX_MAX_Q, -VX_MAX_Q / 2);
        break;
    }

    printf("  spawn %s zone=%d @ x=%d y=%d vx=%+d vy=%+d r=%d\n",
           obj_is_bomb ? "BOMB" : "fruit",
           zone, obj_x, obj_y_q / Q, obj_vx_q, obj_vy_q, obj_radius);
    fflush(stdout);
    write_state_hw();
    write_fruit_hw();
}

static void update_object(void)
{
    /* physics integration */
    obj_vy_q += GRAVITY_Q;
    obj_y_q  += obj_vy_q;
    obj_x    += obj_vx_q / Q;    /* vx is qpx/frame -> divide by Q for pixels */

    int y_pixel = obj_y_q / Q;

    /* Fell off bottom: miss handling */
    if (y_pixel > SCREEN_H + 60) {
        if (obj_visible && !obj_already_cut) {
            if (!obj_is_bomb) {
                lives--;
                printf("  MISS fruit!  lives=%d\n", lives);
                fflush(stdout);
            } else {
                printf("  (bomb dodged)\n");
            }
        }
        spawn_object();
        return;
    }
    /* Bounce off left/right edges rather than disappearing */
    if (obj_x < obj_radius)              { obj_x = obj_radius;              obj_vx_q = -obj_vx_q; }
    if (obj_x > SCREEN_W - obj_radius)   { obj_x = SCREEN_W - obj_radius;   obj_vx_q = -obj_vx_q; }
}

/* ---- Ultrasonic swipe detection ---- */
static int check_swipe(void)
{
    motion_status_t m;
    if (ioctl(fd, MOTION_READ, &m) < 0) return 0;

    int is_new = m.motion_detected || (m.frame_counter != last_sample_id);
    if (!is_new) return 0;
    last_sample_id = m.frame_counter;

    unsigned int cm = m.changed_pixel_count / ULTRA_TICKS_PER_CM;
    int curr_near = (cm > 0 && cm < NEAR_CM);

    int edge = (!prev_near && curr_near);
    prev_near = curr_near;
    return edge;
}

/* ---- Main ---- */
int main(void)
{
    signal(SIGINT, on_sigint);
    srand((unsigned)time(NULL));

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) { perror("open " DEV_PATH); return 1; }

    score = 0;
    lives = MAX_LIVES;

    printf("==============================================\n");
    printf("   FRUIT NINJA (ultrasonic + bombs)\n");
    printf("==============================================\n");
    printf("Swipe within %u cm to cut the flying object.\n", NEAR_CM);
    printf("Orange = fruit (+1 score)\n");
    printf("Purple = BOMB  (don't cut! -1 life)\n");
    printf("Missed fruit also costs a life. Start with %d lives.\n\n",
           MAX_LIVES);

    write_score_hw();
    spawn_object();

    while (!g_stop && lives > 0) {
        update_object();

        if (check_swipe()) {
            if (obj_visible && !obj_already_cut) {
                obj_already_cut = 1;
                obj_visible = 0;
                if (obj_is_bomb) {
                    lives--;
                    printf("  BOMB CUT!  lives=%d\n", lives);
                } else {
                    score++;
                    printf("  CUT fruit!  score=%d\n", score);
                    write_score_hw();
                }
                fflush(stdout);
                write_fruit_hw();         /* hide immediately */
                usleep(CUT_HOLDOFF_US);
                if (lives > 0) spawn_object();
                continue;                 /* skip end-of-frame write */
            }
        }

        write_fruit_hw();
        usleep(FRAME_US);
    }

    /* Game over */
    obj_visible = 0;
    obj_is_bomb = 0;
    write_fruit_hw();
    write_state_hw();

    printf("\n==============================================\n");
    printf("  GAME OVER.   Final score: %d\n", score);
    printf("==============================================\n");

    close(fd);
    return 0;
}
