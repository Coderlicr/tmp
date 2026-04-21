#ifndef _TOP_MODULE_H
#define _TOP_MODULE_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

/* ---- Register offsets (byte) ---- */
#define ULTRA_STATUS_OFF      0x00
#define LAST_ECHO_CNT_OFF     0x04
#define CURR_ECHO_CNT_OFF     0x08
#define SAMPLE_ID_OFF         0x0C
#define FRUIT_X_OFF           0x10
#define FRUIT_Y_OFF           0x14
#define FRUIT_RADIUS_OFF      0x18
#define FRUIT_CTRL_OFF        0x1C   /* bit0 = visible */
#define SCORE_OFF             0x20
#define GAME_STATE_OFF        0x24

/* ---- status bits ---- */
#define ULTRA_STATUS_NEW_BIT   0x1
#define ULTRA_STATUS_ECHO_BIT  0x2
#define ULTRA_STATUS_TRIG_BIT  0x4

/* Distance conversion: 50 MHz counter, sound 343 m/s round-trip
 * cm = ticks / (50e6 * 2 / 343 / 100) ≈ ticks / 2915
 * Use 2900 for simplicity. */
#define ULTRA_TICKS_PER_CM   2900u

/* ---- exchange structs ---- */
struct ultra_sample {
    unsigned int status;          /* raw status register (after read new is cleared) */
    unsigned int last_echo_cnt;   /* 50 MHz ticks */
    unsigned int sample_id;       /* increments each new sample */
};

struct fruit_cmd {
    unsigned short x;        /* 0..639 */
    unsigned short y;        /* 0..479 */
    unsigned short radius;   /* px */
    unsigned char  visible;  /* 0/1 */
};

/* ---- ioctls ---- */
#define TOP_MODULE_MAGIC  'q'
#define READ_ULTRA        _IOR(TOP_MODULE_MAGIC, 1, struct ultra_sample)
#define WRITE_FRUIT       _IOW(TOP_MODULE_MAGIC, 2, struct fruit_cmd)
#define WRITE_SCORE       _IOW(TOP_MODULE_MAGIC, 3, unsigned int)
#define WRITE_GAME_STATE  _IOW(TOP_MODULE_MAGIC, 4, unsigned int)

#endif /* _TOP_MODULE_H */
