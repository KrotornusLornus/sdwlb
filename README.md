# sdwlb

A minimalist Wayland layer-shell status bar for dwl.

sdwlb draws only colored rectangles: no text, no fonts, no Cairo, and no Pango. All configuration lives in `config.h`, in the suckless style.

## Features

- Left-aligned dwl tags with selected, occupied, and empty colors.
- Centered clock with hour blocks and a minute strip.
- CPU and memory blocks sharing a five-state color palette.
- ALSA master volume bars.
- WiFi signal bars.
- One custom clickable color block.
- Left-click actions for every block except tags.
- Translucent ARGB colors.
- Publishes a compact tag snapshot to `/tmp/dwl-status`.

## Requirements

Build dependencies:

- A C99 compiler and `make`
- `pkg-config`
- `wayland-client`
- `wayland-protocols` and `wayland-scanner`

Runtime dependencies:

- A Wayland compositor with `wlr-layer-shell` support, such as dwl.
- `amixer` from `alsa-utils` for the volume bars. If it is unavailable, the volume bars show zero.
- An interface exposed through `/proc/net/wireless` for the WiFi bars. This works with iwd. If no wireless interface is available, the WiFi bars show zero.

No Cairo, Pango, libasound, or JSON library is needed.

## Build

```sh
make
```

If `config.h` does not exist, `make` copies it from `config.def.h` first. Edit `config.h`, not `config.def.h`, for local configuration. `config.h` is ignored by git.

The build generates the layer-shell and xdg-shell protocol glue with `wayland-scanner`.

## Running

Run sdwlb as the dwl startup client so it receives dwl's status stream on stdin:

```sh
dwl -s sdwlb
```

Or from a startup script:

```sh
exec sdwlb
```

dwl writes status lines to the startup client's stdin. sdwlb parses lines with this shape:

```text
<monitor> tags <occupied> <selected> <focused> <urgent>
```

It ignores every other status line. The tag colors use the selected, occupied, and empty states; the focused and urgent masks are parsed but do not change the colors.

sdwlb exits cleanly on stdin EOF, `SIGINT`, or `SIGTERM`.

## Configuration

All options are compile-time constants in `config.h`. The template is `config.def.h`.

### Color format

Colors are 32-bit ARGB values:

```text
0xAARRGGBB
  AA = alpha
  RR = red
  GG = green
  BB = blue
```

Use `0xffRRGGBB` for opaque colors:

```c
0xff00ffff  /* opaque cyan */
0xff0000ff  /* opaque blue */
0xffffffff  /* opaque white */
```

The alpha channel is first. A value such as `0x0000ffff` has alpha `0x00`, so it is fully transparent even though its RGB part is cyan. Use a nonzero alpha, usually `0xff`, for visible colors.

`COL_BG` is intentionally translucent (`0xcc1a1b26`). The renderer premultiplies alpha internally because `wl_shm` expects premultiplied ARGB.

### Bar

| Option | Description |
| --- | --- |
| `BAR_HEIGHT` | Bar height in pixels. |
| `BAR_POSITION` | `0` for top, `1` for bottom. |
| `BAR_PADDING` | Horizontal padding at the edges. |
| `BLOCK_GAP` | Gap used by tags and clock blocks. |
| `RIGHT_BLOCK_GAP` | Internal gap between small blocks in the right-side groups. |
| `RIGHT_MODULE_GAP` | Gap between right-side modules: sound, WiFi, CPU, memory, and custom. |
| `UPDATE_MS` | Poll interval in milliseconds. |

Blocks use the full bar height. `BAR_PADDING` is horizontal only.

### Tags

| Option | Description |
| --- | --- |
| `TAG_COUNT` | Number of tag blocks. If `0` or less, it is derived from the tag masks. |
| `TAG_BLOCK_W` | Width of each tag block. |
| `COL_TAG_EMPTY` | Empty tag color. |
| `COL_TAG_OCCUPIED` | Occupied tag color. |
| `COL_TAG_FOCUSED` | Selected tag color. |

Tags are display-only. They have no click action.

### Clock

| Option | Description |
| --- | --- |
| `CLOCK_BLOCKS` | Number of clock blocks. |
| `CLOCK_SPAN_HOURS` | Number of hours represented by the clock. |
| `CLOCK_BLOCK_W` | Width of each clock block. |
| `CLOCK_MINUTE_H` | Height of the minute strip. `0` disables the strip. |
| `COL_CLOCK_EMPTY` | Empty hour color. |
| `COL_CLOCK_FILLED` | Elapsed hour color. |
| `COL_CLOCK_NOW` | Current hour color. |
| `COL_CLOCK_MINUTE` | Minute strip color. |

The upper part of each clock block represents elapsed time over `CLOCK_SPAN_HOURS`. The lower strip represents minutes over the same number of blocks:

```text
minute_blocks = minute * CLOCK_BLOCKS / 60
```

Only complete minute blocks are lit; there is no partial block. With `CLOCK_BLOCKS 12`, each minute block represents five minutes, so at `06:12` two minute blocks are lit.

### CPU and memory

| Option | Description |
| --- | --- |
| `METRIC_BLOCK_W` | Width of each metric block. |
| `COL_METRIC_0` | Color for 0-19% usage. |
| `COL_METRIC_1` | Color for 20-39% usage. |
| `COL_METRIC_2` | Color for 40-59% usage. |
| `COL_METRIC_3` | Color for 60-79% usage. |
| `COL_METRIC_4` | Color for 80-100% usage. |

CPU and memory share the same five colors. The state is:

```text
state = usage * 5 / 100
```

There is no continuous gradient.

### Sound

| Option | Description |
| --- | --- |
| `SOUND_BLOCKS` | Number of volume blocks. |
| `SOUND_BLOCK_W` | Width of each volume block. |
| `COL_SOUND_ON` | Lit block color. |
| `COL_SOUND_OFF` | Unlit block color. |
| `COL_SOUND_MUTED` | Color used for all blocks while muted. |
| `SOUND_CMD` | Command executed when the sound group is clicked. Empty disables it. |

Volume is read from `amixer sget Master`. The bar parses the playback percentage and the on/off mute state. A muted master makes every sound block use `COL_SOUND_MUTED`.

### WiFi

| Option | Description |
| --- | --- |
| `WIFI_BLOCKS` | Number of signal blocks. |
| `WIFI_BLOCK_W` | Width of each signal block. |
| `COL_WIFI_ON` | Lit block color. |
| `COL_WIFI_OFF` | Unlit block color. |
| `WIFI_CMD` | Command executed when the WiFi group is clicked. Empty disables it. |

WiFi signal is read from the first interface in `/proc/net/wireless`. The signal level in dBm is mapped as follows:

```text
-100 dBm or lower -> 0%
-50 dBm or higher -> 100%
```

The number of lit blocks is `percentage * WIFI_BLOCKS / 100`.

### Click actions

Every block except tags is a clickable zone:

| Option | Zone |
| --- | --- |
| `CLOCK_CMD` | Whole clock area, including internal gaps. |
| `SOUND_CMD` | Whole sound group, including internal gaps. |
| `WIFI_CMD` | Whole WiFi group, including internal gaps. |
| `CPU_CMD` | CPU block. |
| `MEM_CMD` | Memory block. |
| `CUSTOM_CMD` | Custom block. |

Commands are executed with `/bin/sh -c` on left-button press. An empty string disables the action. Child processes get stdin redirected to `/dev/null` so they cannot consume dwl's status stream.

### Custom block

| Option | Description |
| --- | --- |
| `CUSTOM_BLOCK_W` | Width of the custom block. |
| `COL_CUSTOM` | Custom block color. |
| `CUSTOM_CMD` | Command executed when the custom block is clicked. Empty disables it. |

The custom block displays no text or dynamic data. It is a solid color block that can launch a command.

## Published status

sdwlb writes a snapshot to `STATUS_FILE`, which defaults to `/tmp/dwl-status`:

```text
<focused-tag> <occupied-tag> ...
```

The focused tag comes first, followed by the other occupied tags. Other consumers can read this file without parsing dwl's full status stream.

The file is written to a temporary path and renamed atomically. Because sdwlb sets `umask(077)`, the resulting file mode is `0600`.

## Files

- `sdwlb.c` - the entire bar implementation.
- `config.h` - local configuration, ignored by git.
- `config.def.h` - configuration template.
- `config.mk` - compiler and pkg-config settings.
- `Makefile` - build rules.
- `protocols/wlr-layer-shell-unstable-v1.xml` - vendored layer-shell protocol definition.
