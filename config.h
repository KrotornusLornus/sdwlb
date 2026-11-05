#ifndef CONFIG_H
#define CONFIG_H

#define BAR_HEIGHT       5
#define BAR_POSITION     0 /* 0 = top, 1 = bottom */
#define BAR_PADDING      4
#define BLOCK_GAP        2
#define RIGHT_BLOCK_GAP  2
#define RIGHT_MODULE_GAP 6

#define TAG_COUNT        9
#define TAG_BLOCK_W      14

#define CLOCK_BLOCKS     12
#define CLOCK_SPAN_HOURS 12
#define CLOCK_BLOCK_W    10

#define METRIC_BLOCK_W   60
#define METRIC_SEGMENTS  5
#define METRIC_SEG_GAP   1

#define SOUND_BAR_W      30

#define UPDATE_MS        1000

#define STATUS_FILE      "/tmp/dwl-status"

/* Fondo oscuro y translúcido */
#define COL_BG             0xcc1a1b26

/* Tags: un solo tono azulado; ocupada = enfocada pero apagada */
#define COL_TAG_EMPTY      0xff3b4252
#define COL_TAG_OCCUPIED   0xff617990
#define COL_TAG_FOCUSED    0xff81a1c1

/* Reloj: azul, minutero blanco */
#define COL_CLOCK_EMPTY    0xff3b4252
#define COL_CLOCK_FILLED   0xff5e81ac
#define COL_CLOCK_NOW      0xff81a1c1

#define CLOCK_MINUTE_H     1
#define COL_CLOCK_MINUTE   0xffeceff4

/* CPU - naranja (calor/procesador) */
#define COL_CPU            0xffd08770
/* Memoria - verde (memoria) */
#define COL_MEM            0xffa3be8c
/* Segmento vacio de las metricas */
#define COL_METRIC_EMPTY   0xff434c5e
/* Alerta al llegar al 100 % (5/5 segmentos) */
#define COL_METRIC_HIGH    0xffbf616a

/* Sonido - cyan (audio) */
#define COL_SOUND_BG       0xff3b4252
#define COL_SOUND_FG       0xff88c0d0
#define COL_SOUND_MUTED    0xffbf616a

/* WiFi - violeta (conexion) */
#define WIFI_BLOCKS        4
#define WIFI_BLOCK_W       3
#define COL_WIFI_ON        0xffb48ead
#define COL_WIFI_OFF       0xff3b4252

/* Custom - blanco (power menu) */
#define COL_CUSTOM         0xffeceff4

#define SOUND_CMD          "footclient -a float -w 1000x600 -e alsamixer"
#define WIFI_CMD           "footclient -a float -w 1100x700 -e impala"
#define CLOCK_CMD          ""
#define CPU_CMD            "foot -a float -o font=Mono:size=16 -W 80x24 -e btop"
#define MEM_CMD            "foot -a float -o font=Mono:size=16 -W 80x24 -e btop"

#define CUSTOM_BLOCK_W     20
#define CUSTOM_CMD         "powermenu"

#endif
