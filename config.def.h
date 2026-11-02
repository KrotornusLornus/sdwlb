#ifndef CONFIG_H
#define CONFIG_H

#define BAR_HEIGHT       22
#define BAR_POSITION     0 /* 0 = top, 1 = bottom */
#define BAR_PADDING      4
#define BLOCK_GAP        2

#define TAG_COUNT        9
#define TAG_BLOCK_W      14

#define CLOCK_BLOCKS     24
#define CLOCK_SPAN_HOURS 12
#define CLOCK_BLOCK_W    4

#define METRIC_BLOCK_W   20

#define UPDATE_MS        1000

#define STATUS_FILE      "/tmp/dwl-status"

#define COL_BG             0xcc1a1b26
#define COL_TAG_EMPTY      0xff2a2e3a
#define COL_TAG_OCCUPIED   0xff565f89
#define COL_TAG_FOCUSED    0xff7aa2f7
#define COL_CLOCK_EMPTY    0xff2a2e3a
#define COL_CLOCK_FILLED   0xff9ece6a
#define COL_CLOCK_NOW      0xffffffff
#define COL_CPU_LOW        0xff7aa2f7
#define COL_CPU_HIGH       0xfff7768e
#define COL_MEM_LOW        0xff7aa2f7
#define COL_MEM_HIGH       0xfff7768e

#endif
