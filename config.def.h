#ifndef CONFIG_H
#define CONFIG_H

/*
 * Colors use 32-bit ARGB: 0xAARRGGBB.
 * Use 0xffRRGGBB for opaque colors.
 * Examples:
 *   0xff00ffff = opaque cyan
 *   0x0000ffff = fully transparent cyan
 *   0x8000ffff = cyan at roughly 50% alpha
 */

/* Bar */
#define BAR_HEIGHT       22
#define BAR_POSITION     0 /* 0 = top, 1 = bottom */
#define BAR_PADDING      4
#define BLOCK_GAP        2 /* tags and clock blocks */
#define RIGHT_BLOCK_GAP  2 /* small blocks inside the right-side groups */
#define RIGHT_MODULE_GAP 4 /* space between right-side modules */

/* Tags */
#define TAG_COUNT        9
#define TAG_BLOCK_W      14

/* Clock */
#define CLOCK_BLOCKS     24
#define CLOCK_SPAN_HOURS 12
#define CLOCK_BLOCK_W    4
#define CLOCK_MINUTE_H   4 /* height of the minute strip; 0 disables it */

/* CPU and memory */
#define METRIC_BLOCK_W   20

/* Update interval in milliseconds */
#define UPDATE_MS        1000

/* Published tag snapshot */
#define STATUS_FILE      "/tmp/dwl-status"

/* Colors */
#define COL_BG             0xcc1a1b26
#define COL_TAG_EMPTY      0xff2a2e3a
#define COL_TAG_OCCUPIED   0xff565f89
#define COL_TAG_FOCUSED    0xff7aa2f7
#define COL_CLOCK_EMPTY    0xff2a2e3a
#define COL_CLOCK_FILLED   0xff9ece6a
#define COL_CLOCK_NOW      0xffffffff
#define COL_CLOCK_MINUTE   0xff9ece6a

/* Shared CPU/memory palette: 5 equal states */
#define COL_METRIC_0       0xff7aa2f7
#define COL_METRIC_1       0xff9ece6a
#define COL_METRIC_2       0xffe0af68
#define COL_METRIC_3       0xffff9e64
#define COL_METRIC_4       0xfff7768e

/* Sound */
#define SOUND_BLOCKS       10
#define SOUND_BLOCK_W      3
#define COL_SOUND_ON       0xff9ece6a
#define COL_SOUND_OFF      0xff2a2e3a
#define COL_SOUND_MUTED    0xfff7768e
#define SOUND_CMD          ""

/* WiFi */
#define WIFI_BLOCKS        4
#define WIFI_BLOCK_W       3
#define COL_WIFI_ON        0xff7aa2f7
#define COL_WIFI_OFF       0xff2a2e3a
#define WIFI_CMD           ""

/* Click actions; an empty string disables the action */
#define CLOCK_CMD          ""
#define CPU_CMD            ""
#define MEM_CMD            ""

/* Custom clickable color block */
#define CUSTOM_BLOCK_W     20
#define COL_CUSTOM         0xffbb9af7
#define CUSTOM_CMD         ""

#endif
