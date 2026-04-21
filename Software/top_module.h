#ifndef _TOP_MODULE_H
#define _TOP_MODULE_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

/* ---- Hardware register byte offsets (Avalon-MM, 32-bit) ---- */
#define REG_MOTION_STATUS   0x00   /* R   bit0 = motion_detected       */
#define REG_CHANGED_COUNT   0x04   /* R   number of changed pixels     */
#define REG_FRAME_COUNTER   0x08   /* R   camera frame counter         */
/* 0x0C reserved */
#define REG_FRUIT_X         0x10   /* W   fruit centre x  (0-639)      */
#define REG_FRUIT_Y         0x14   /* W   fruit centre y  (0-479)      */
#define REG_FRUIT_RADIUS    0x18   /* W   fruit radius                 */
#define REG_FRUIT_CTRL      0x1C   /* W   bit0 = fruit_visible         */
#define REG_SCORE           0x20   /* W   current score                */
#define REG_GAME_STATE      0x24   /* W   game state flags             */

/* ---- Data structures shared between kernel and user space ---- */
typedef struct {
    unsigned int motion_detected;
    unsigned int changed_pixel_count;
    unsigned int frame_counter;
} motion_status_t;

typedef struct {
    unsigned int x;
    unsigned int y;
    unsigned int radius;
    unsigned int visible;
} fruit_params_t;

/* ---- ioctl definitions ---- */
#define FRUTNINJA_MAGIC  'F'

#define MOTION_READ    _IOR(FRUTNINJA_MAGIC, 1, motion_status_t)
#define FRUIT_WRITE    _IOW(FRUTNINJA_MAGIC, 2, fruit_params_t)
#define SCORE_WRITE    _IOW(FRUTNINJA_MAGIC, 3, unsigned int)
#define GAME_ST_WRITE  _IOW(FRUTNINJA_MAGIC, 4, unsigned int)

/* ---- Game constants (shared so FPGA line position matches HPS logic) ---- */
#define SCREEN_W    640
#define SCREEN_H    480
#define LINE_Y      240

#endif /* _TOP_MODULE_H */
