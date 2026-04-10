/*
 * game.c  --  Fruit-Ninja userspace game loop for DE1-SoC
 *
 * Reads motion detection from FPGA, manages fruit physics,
 * checks fruit-vs-line intersection, and writes display
 * parameters back to FPGA via /dev/top_module ioctl.
 *
 * Build:  arm-linux-gnueabihf-gcc -O2 -Wall -o game game.c
 * Run:    ./game            (on DE1-SoC after insmod top_module.ko)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include "top_module.h"

/* ---- Physics (quarter-pixel fixed-point) ---- */
#define Q            4          /* 1 pixel = 4 units                 */
#define GRAVITY_Q    2          /* quarter-pixels per frame^2        */
#define VY_MIN      (-70)       /* initial upward speed range (fast) */
#define VY_MAX      (-50)       /* initial upward speed range (slow) */
#define RADIUS_MIN   20
#define RADIUS_MAX   35
#define FRAME_US     33000      /* ~30 fps                           */
#define MAX_LIVES    3

/* ---- Global state ---- */
static int fd;                  /* /dev/top_module file descriptor   */
static int score   = 0;
static int lives   = MAX_LIVES;

/* Fruit state (quarter-pixel for y, pixel for x) */
static int fruit_x;
static int fruit_y_q;           /* quarter-pixel y position          */
static int fruit_vy_q;          /* quarter-pixel y velocity          */
static int fruit_radius;
static int fruit_visible;
static int fruit_already_cut;   /* prevent double-counting           */

/* ---- Helpers ---- */
static void write_fruit(void)
{
    int y_pixel = fruit_y_q / Q;
    if (y_pixel < 0)   y_pixel = 0;
    if (y_pixel > 1023) y_pixel = 1023;

    fruit_params_t fp;
    fp.x       = (unsigned int)fruit_x;
    fp.y       = (unsigned int)y_pixel;
    fp.radius  = (unsigned int)fruit_radius;
    fp.visible = (unsigned int)fruit_visible;
    ioctl(fd, FRUIT_WRITE, &fp);
}

static void write_score(void)
{
    unsigned int s = (unsigned int)score;
    ioctl(fd, SCORE_WRITE, &s);
}

static void spawn_fruit(void)
{
    fruit_x         = 50 + rand() % (SCREEN_W - 100);
    fruit_y_q       = (SCREEN_H + 30) * Q;        /* just below screen  */
    fruit_vy_q      = VY_MIN + rand() % (VY_MAX - VY_MIN + 1);
    fruit_radius    = RADIUS_MIN + rand() % (RADIUS_MAX - RADIUS_MIN + 1);
    fruit_visible   = 1;
    fruit_already_cut = 0;
}

static void update_fruit(void)
{
    fruit_vy_q += GRAVITY_Q;
    fruit_y_q  += fruit_vy_q;

    /* Fell off the bottom of the screen -> miss */
    if (fruit_y_q / Q > SCREEN_H + 60) {
        if (fruit_visible && !fruit_already_cut) {
            lives--;
            printf("  MISS!  Lives left: %d\n", lives);
        }
        spawn_fruit();
    }
}

static int fruit_intersects_line(void)
{
    int y_pixel = fruit_y_q / Q;
    int diff = y_pixel - LINE_Y;
    if (diff < 0) diff = -diff;
    return diff <= fruit_radius;
}

/* ---- Main ---- */
int main(void)
{
    srand((unsigned)time(NULL));

    fd = open("/dev/top_module", O_RDWR);
    if (fd < 0) {
        perror("open /dev/top_module");
        return 1;
    }

    printf("===================================\n");
    printf("   FRUIT NINJA  --  DE1-SoC\n");
    printf("===================================\n");
    printf("Wave an object in front of the camera\n");
    printf("when the fruit touches the dashed line!\n\n");

    spawn_fruit();
    write_score();

    while (lives > 0) {
        /* 1. Read motion status from FPGA */
        motion_status_t ms;
        memset(&ms, 0, sizeof(ms));
        if (ioctl(fd, MOTION_READ, &ms)) {
            perror("MOTION_READ");
            break;
        }

        /* 2. Update fruit physics */
        update_fruit();

        /* 3. Check cut condition */
        if (ms.motion_detected
            && fruit_intersects_line()
            && fruit_visible
            && !fruit_already_cut)
        {
            score++;
            fruit_already_cut = 1;
            fruit_visible = 0;
            printf("  CUT!   Score: %d   (changed px: %u)\n",
                   score, ms.changed_pixel_count);
            write_score();

            /* Brief pause so player sees fruit disappear */
            write_fruit();
            usleep(400000);

            spawn_fruit();
        }

        /* 4. Send fruit params to FPGA */
        write_fruit();

        /* 5. Frame pacing */
        usleep(FRAME_US);
    }

    /* Game over */
    printf("\n  GAME OVER!  Final score: %d\n\n", score);
    fruit_visible = 0;
    write_fruit();

    unsigned int gs = 1;  /* 1 = game over */
    ioctl(fd, GAME_ST_WRITE, &gs);

    close(fd);
    return 0;
}
