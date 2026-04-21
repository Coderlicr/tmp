#ifndef _TOP_MODULE_H
#define _TOP_MODULE_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

/* ---- Hardware register byte offsets (Avalon-MM, 32-bit) ----
 *
 * Ultrasonic section (read):
 *   0x00 ULTRA_STATUS    bit0=new, bit1=echo, bit2=trig.
 *                        Writing 1 to bit0 clears the "new" flag.
 *   0x04 LAST_ECHO_CNT   50-MHz ticks of last echo pulse.  cm ≈ val / 2900.
 *   0x08 CURR_ECHO_CNT   in-progress counter (debug only).
 *   0x0C SAMPLE_ID       increments once per completed measurement.
 *
 * Game section (write-only from HPS):
 *   0x10 FRUIT_X         0..639
 *   0x14 FRUIT_Y         0..479
 *   0x18 FRUIT_RADIUS    px
 *   0x1C FRUIT_CTRL      bit0 = visible
 *   0x20 SCORE
 *   0x24 GAME_STATE
 */
#define REG_ULTRA_STATUS     0x00
#define REG_LAST_ECHO_CNT    0x04
#define REG_CURR_ECHO_CNT    0x08
#define REG_SAMPLE_ID        0x0C
#define REG_FRUIT_X          0x10
#define REG_FRUIT_Y          0x14
#define REG_FRUIT_RADIUS     0x18
#define REG_FRUIT_CTRL       0x1C
#define REG_SCORE            0x20
#define REG_GAME_STATE       0x24

#define ULTRA_STATUS_NEW     0x1
#define ULTRA_STATUS_ECHO    0x2
#define ULTRA_STATUS_TRIG    0x4

/* 50 MHz counter, sound 343 m/s round-trip => ticks/cm ≈ 2915 */
#define ULTRA_TICKS_PER_CM   2900u

/* ---- Data structures shared between kernel and user space ---- */
/* Ultrasonic sample snapshot returned by MOTION_READ (name kept for
 * compatibility with the legacy driver architecture). */
typedef struct {
    unsigned int motion_detected;   /* 1 if new sample arrived this read */
    unsigned int changed_pixel_count; /* last_echo_cnt (ticks) */
    unsigned int frame_counter;     /* sample_id */
} motion_status_t;

typedef struct {
    unsigned int x;
    unsigned int y;
    unsigned int radius;
    unsigned int visible;
} fruit_params_t;

/* ---- ioctl definitions (same magic/numbers as legacy driver) ---- */
#define FRUTNINJA_MAGIC  'F'
#define MOTION_READ    _IOR(FRUTNINJA_MAGIC, 1, motion_status_t)
#define FRUIT_WRITE    _IOW(FRUTNINJA_MAGIC, 2, fruit_params_t)
#define SCORE_WRITE    _IOW(FRUTNINJA_MAGIC, 3, unsigned int)
#define GAME_ST_WRITE  _IOW(FRUTNINJA_MAGIC, 4, unsigned int)

/* ---- Game constants ---- */
#define SCREEN_W    640
#define SCREEN_H    480

#endif /* _TOP_MODULE_H */
