#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "config.h"

#define STATUS_TMP STATUS_FILE ".tmp"
#define MAX_CLICKABLES 16

struct clickable {
	int x, y, w, h;
	const char *cmd;
};

struct state {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct wl_seat *seat;
	struct wl_pointer *pointer;

	uint32_t width;
	uint32_t height;
	int configured;
	int running;
	int dirty;
	int buffer_busy;
	int pointer_inside;
	int pointer_x;
	int pointer_y;

	struct wl_buffer *buffer;
	uint32_t *pixels;
	size_t pixels_size;

	uint32_t tags_occ;
	uint32_t tags_sel;
	uint32_t tags_foc;
	uint32_t tags_urg;

	uint64_t cpu_prev_total;
	uint64_t cpu_prev_idle;
	int cpu_usage;
	int mem_usage;
	int volume;
	int muted;
	int wifi_usage;

	struct clickable clickables[MAX_CLICKABLES];
	int nclickables;

	char inbuf[8192];
	size_t inlen;
};

static volatile sig_atomic_t sig_running = 1;

static void
sig_handler(int sig)
{
	(void)sig;
	sig_running = 0;
}

static void
die(const char *msg)
{
	fprintf(stderr, "sdwlb: %s\n", msg);
	exit(1);
}

static uint32_t
premultiply(uint32_t color)
{
	uint32_t a = (color >> 24) & 0xff;
	uint32_t r = ((color >> 16) & 0xff) * a / 255;
	uint32_t g = ((color >> 8) & 0xff) * a / 255;
	uint32_t b = (color & 0xff) * a / 255;
	return (a << 24) | (r << 16) | (g << 8) | b;
}

static void
add_clickable(struct state *s, int x, int y, int w, int h, const char *cmd)
{
	struct clickable *c;

	if (!cmd || !cmd[0] || w <= 0 || h <= 0)
		return;
	if (s->nclickables >= MAX_CLICKABLES)
		return;

	c = &s->clickables[s->nclickables++];
	c->x = x;
	c->y = y;
	c->w = w;
	c->h = h;
	c->cmd = cmd;
}

static void
fill_rect(struct state *s, int x, int y, int w, int h, uint32_t color)
{
	uint32_t sa, inv, src, sr, sg, sb;
	int yy, xx;

	if (w <= 0 || h <= 0)
		return;

	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (x + w > (int)s->width)
		w = (int)s->width - x;
	if (y + h > (int)s->height)
		h = (int)s->height - y;
	if (w <= 0 || h <= 0)
		return;

	sa = (color >> 24) & 0xff;
	inv = 255 - sa;
	src = premultiply(color);
	sr = (src >> 16) & 0xff;
	sg = (src >> 8) & 0xff;
	sb = src & 0xff;

	for (yy = y; yy < y + h; yy++) {
		uint32_t *row = s->pixels + (size_t)yy * s->width;

		for (xx = x; xx < x + w; xx++) {
			uint32_t dst, da, dr, dg, db, oa, or_, og, ob;

			if (sa == 255) {
				row[xx] = src;
				continue;
			}
			if (sa == 0)
				continue;

			dst = row[xx];
			da = (dst >> 24) & 0xff;
			dr = (dst >> 16) & 0xff;
			dg = (dst >> 8) & 0xff;
			db = dst & 0xff;

			oa = sa + da * inv / 255;
			or_ = sr + dr * inv / 255;
			og = sg + dg * inv / 255;
			ob = sb + db * inv / 255;

			row[xx] = (oa << 24) | (or_ << 16) | (og << 8) | ob;
		}
	}
}

static void
draw_tags(struct state *s)
{
	int h = (int)s->height;
	int x = BAR_PADDING;
	int count = TAG_COUNT;
	int i;

	if (count <= 0) {
		uint32_t mask = s->tags_occ | s->tags_sel |
		                s->tags_foc | s->tags_urg;

		count = 1;
		while (mask > 1) {
			mask >>= 1;
			count++;
		}
	}
	if (count > 32)
		count = 32;

	for (i = 0; i < count; i++) {
		uint32_t bit = 1u << i;
		uint32_t color;

		if (s->tags_sel & bit)
			color = COL_TAG_FOCUSED;
		else if (s->tags_occ & bit)
			color = COL_TAG_OCCUPIED;
		else
			color = COL_TAG_EMPTY;

		fill_rect(s, x, 0, TAG_BLOCK_W, h, color);
		x += TAG_BLOCK_W + BLOCK_GAP;
	}
}

static void
draw_clock(struct state *s)
{
	time_t now;
	struct tm tm;
	int span, minutes, block_minutes, filled, current, total_w, x, h, i;
	int clock_x, minute_h, top_h, minute_blocks;

	if (CLOCK_BLOCKS <= 0)
		return;

	now = time(NULL);
	localtime_r(&now, &tm);

	span = CLOCK_SPAN_HOURS > 0 ? CLOCK_SPAN_HOURS : 12;
	minutes = (tm.tm_hour % span) * 60 + tm.tm_min;
	block_minutes = (span * 60) / CLOCK_BLOCKS;
	if (block_minutes <= 0)
		block_minutes = 1;

	filled = minutes / block_minutes;
	if (filled > CLOCK_BLOCKS)
		filled = CLOCK_BLOCKS;
	current = filled < CLOCK_BLOCKS ? filled : CLOCK_BLOCKS - 1;

	total_w = CLOCK_BLOCKS * CLOCK_BLOCK_W +
	          (CLOCK_BLOCKS - 1) * BLOCK_GAP;
	x = ((int)s->width - total_w) / 2;
	if (x < 0)
		x = 0;
	h = (int)s->height;

	minute_h = CLOCK_MINUTE_H;
	if (minute_h < 0)
		minute_h = 0;
	if (minute_h > h)
		minute_h = h;
	top_h = h - minute_h;

	minute_blocks = tm.tm_min * CLOCK_BLOCKS / 60;
	if (minute_blocks > CLOCK_BLOCKS)
		minute_blocks = CLOCK_BLOCKS;

	clock_x = x;
	for (i = 0; i < CLOCK_BLOCKS; i++) {
		uint32_t color;

		if (i < filled)
			color = COL_CLOCK_FILLED;
		else if (i == current)
			color = COL_CLOCK_NOW;
		else
			color = COL_CLOCK_EMPTY;

		fill_rect(s, x, 0, CLOCK_BLOCK_W, top_h, color);
		if (minute_h > 0) {
			fill_rect(s, x, top_h, CLOCK_BLOCK_W, minute_h,
			          COL_CLOCK_EMPTY);
			if (i < minute_blocks)
				fill_rect(s, x, top_h, CLOCK_BLOCK_W, minute_h,
				          COL_CLOCK_MINUTE);
		}
		x += CLOCK_BLOCK_W + BLOCK_GAP;
	}

	add_clickable(s, clock_x, 0, total_w, h, CLOCK_CMD);
}

static int
sound_width(void)
{
	return SOUND_BAR_W;
}

static int
wifi_width(void)
{
	if (WIFI_BLOCKS <= 0 || WIFI_BLOCK_W <= 0)
		return 0;
	return WIFI_BLOCKS * WIFI_BLOCK_W +
	       (WIFI_BLOCKS - 1) * RIGHT_BLOCK_GAP;
}

static int
draw_sound(struct state *s, int x)
{
	int h = (int)s->height;
	int width = sound_width();
	int fill_w;
	int start = x;

	if (width <= 0)
		return x;

	/* Fondo de la barra */
	fill_rect(s, x, 0, width, h, COL_SOUND_BG);

	/* Relleno continuo proporcional al volumen */
	fill_w = s->volume * width / 100;
	if (fill_w < 0)
		fill_w = 0;
	if (fill_w > width)
		fill_w = width;

	if (fill_w > 0)
		fill_rect(s, x, 0, fill_w, h,
			s->muted ? COL_SOUND_MUTED : COL_SOUND_FG);

	add_clickable(s, start, 0, width, h, SOUND_CMD);
	return start + width;
}

static int
draw_wifi(struct state *s, int x)
{
	int h = (int)s->height;
	int width = wifi_width();
	int start = x;
	int lit, i;

	if (width <= 0)
		return x;

	lit = s->wifi_usage * WIFI_BLOCKS / 100;
	if (lit < 0)
		lit = 0;
	if (lit > WIFI_BLOCKS)
		lit = WIFI_BLOCKS;

	for (i = 0; i < WIFI_BLOCKS; i++) {
		fill_rect(s, x, 0, WIFI_BLOCK_W, h,
		          i < lit ? COL_WIFI_ON : COL_WIFI_OFF);
		x += WIFI_BLOCK_W + RIGHT_BLOCK_GAP;
	}

	add_clickable(s, start, 0, width, h, WIFI_CMD);
	return start + width;
}

static int
draw_metric(struct state *s, int x, int usage, uint32_t color,
            const char *cmd)
{
	int h = (int)s->height;
	int seg_w, i, filled;
	int total_w = METRIC_BLOCK_W;
	int high;

	if (total_w <= 0)
		return x;

	if (usage < 0)
		usage = 0;
	if (usage > 100)
		usage = 100;

	high = usage >= 100;

	seg_w = (total_w - (METRIC_SEGMENTS - 1) * METRIC_SEG_GAP) /
	        METRIC_SEGMENTS;
	if (seg_w <= 0)
		seg_w = total_w / METRIC_SEGMENTS;

	/* Redondeo hacia arriba: cualquier uso > 0% muestra al menos 1 segmento */
	filled = (usage * METRIC_SEGMENTS + 99) / 100;
	if (filled > METRIC_SEGMENTS)
		filled = METRIC_SEGMENTS;

	/* Fondo de las 5 barritas (vacías) para que la estructura se vea */
	for (i = 0; i < METRIC_SEGMENTS; i++) {
		int sx = x + i * (seg_w + METRIC_SEG_GAP);
		fill_rect(s, sx, 0, seg_w, h, COL_METRIC_EMPTY);
	}

	/* Segmentos llenados */
	for (i = 0; i < METRIC_SEGMENTS; i++) {
		int sx = x + i * (seg_w + METRIC_SEG_GAP);

		if (i < filled)
			fill_rect(s, sx, 0, seg_w, h,
				high ? COL_METRIC_HIGH : color);
	}

	add_clickable(s, x, 0, total_w, h, cmd);
	return x + total_w;
}

static int
draw_custom(struct state *s, int x)
{
	int h = (int)s->height;

	if (CUSTOM_BLOCK_W <= 0)
		return x;

	fill_rect(s, x, 0, CUSTOM_BLOCK_W, h, COL_CUSTOM);
	add_clickable(s, x, 0, CUSTOM_BLOCK_W, h, CUSTOM_CMD);
	return x + CUSTOM_BLOCK_W;
}

static void
draw_right(struct state *s)
{
	int sw = sound_width();
	int ww = wifi_width();
	int mw = METRIC_BLOCK_W > 0 ? METRIC_BLOCK_W : 0;
	int cw = CUSTOM_BLOCK_W > 0 ? CUSTOM_BLOCK_W : 0;
	int groups = 0;
	int total_w, x;

	if (sw > 0)
		groups++;
	if (ww > 0)
		groups++;
	if (mw > 0)
		groups += 2;
	if (cw > 0)
		groups++;

	total_w = sw + ww + mw * 2 + cw;
	if (groups > 1)
		total_w += (groups - 1) * RIGHT_MODULE_GAP;

	x = (int)s->width - BAR_PADDING - total_w;
	if (x < 0)
		x = 0;

	if (sw > 0) {
		x = draw_sound(s, x);
		x += RIGHT_MODULE_GAP;
	}
	if (ww > 0) {
		x = draw_wifi(s, x);
		x += RIGHT_MODULE_GAP;
	}
	if (mw > 0) {
		x = draw_metric(s, x, s->cpu_usage, COL_CPU, CPU_CMD);
		x += RIGHT_MODULE_GAP;
	}
	if (mw > 0) {
		x = draw_metric(s, x, s->mem_usage, COL_MEM, MEM_CMD);
		x += RIGHT_MODULE_GAP;
	}
	if (cw > 0)
		draw_custom(s, x);
}

static void
draw(struct state *s)
{
	if (!s->configured || !s->buffer || s->buffer_busy)
		return;

	s->nclickables = 0;
	memset(s->pixels, 0, s->pixels_size);
	fill_rect(s, 0, 0, (int)s->width, (int)s->height, COL_BG);
	draw_tags(s);
	draw_clock(s);
	draw_right(s);

	wl_surface_attach(s->surface, s->buffer, 0, 0);
	wl_surface_damage(s->surface, 0, 0,
	                  (int32_t)s->width, (int32_t)s->height);
	wl_surface_commit(s->surface);

	s->buffer_busy = 1;
	s->dirty = 0;
}

static int
read_cpu_usage(struct state *s)
{
	FILE *f;
	char line[256];
	unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
	unsigned long long total, idle_all, dt, di;
	int usage;

	f = fopen("/proc/stat", "r");
	if (!f)
		return s->cpu_usage;
	if (!fgets(line, sizeof line, f)) {
		fclose(f);
		return s->cpu_usage;
	}
	fclose(f);

	if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
	           &user, &nice, &system, &idle, &iowait, &irq,
	           &softirq, &steal) < 4)
		return s->cpu_usage;

	total = user + nice + system + idle + iowait + irq + softirq + steal;
	idle_all = idle + iowait;

	if (s->cpu_prev_total == 0) {
		s->cpu_prev_total = total;
		s->cpu_prev_idle = idle_all;
		return s->cpu_usage;
	}

	dt = total - s->cpu_prev_total;
	di = idle_all - s->cpu_prev_idle;
	s->cpu_prev_total = total;
	s->cpu_prev_idle = idle_all;

	if (dt == 0)
		return s->cpu_usage;

	usage = (int)((dt - di) * 100 / dt);
	if (usage < 0)
		usage = 0;
	if (usage > 100)
		usage = 100;

	return usage;
}

static int
read_mem_usage(void)
{
	FILE *f;
	char line[256];
	unsigned long long total = 0, avail = 0;
	int usage;

	f = fopen("/proc/meminfo", "r");
	if (!f)
		return 0;

	while (fgets(line, sizeof line, f)) {
		if (sscanf(line, "MemTotal: %llu", &total) == 1)
			continue;
		if (sscanf(line, "MemAvailable: %llu", &avail) == 1)
			continue;
	}
	fclose(f);

	if (total == 0)
		return 0;

	usage = (int)((total - avail) * 100 / total);
	if (usage < 0)
		usage = 0;
	if (usage > 100)
		usage = 100;

	return usage;
}

static void
read_volume(struct state *s)
{
	FILE *p;
	char line[512];
	int volume = -1, muted = -1;

	p = popen("amixer sget Master < /dev/null 2>/dev/null", "r");
	if (!p)
		return;

	while (fgets(line, sizeof line, p)) {
		char *pct;
		int value;

		if (!strstr(line, "Playback"))
			continue;

		pct = strchr(line, '[');
		if (pct && sscanf(pct, "[%d%%", &value) == 1)
			volume = value;

		if (strstr(line, "[off]"))
			muted = 1;
		else if (strstr(line, "[on]"))
			muted = 0;
	}
	pclose(p);

	if (volume >= 0) {
		if (volume > 100)
			volume = 100;
		s->volume = volume;
	}
	if (muted >= 0)
		s->muted = muted;
}

static void
read_wifi(struct state *s)
{
	FILE *f;
	char line[256];
	char iface[64];
	unsigned int status;
	float link, level, noise;
	int pct = 0;
	int i;

	f = fopen("/proc/net/wireless", "r");
	if (!f) {
		s->wifi_usage = 0;
		return;
	}

	for (i = 0; i < 2; i++) {
		if (!fgets(line, sizeof line, f)) {
			fclose(f);
			s->wifi_usage = 0;
			return;
		}
	}

	if (fgets(line, sizeof line, f)) {
		if (sscanf(line, " %63[^:]: %x %f %f %f",
		           iface, &status, &link, &level, &noise) >= 4) {
			if (level > -50)
				pct = 100;
			else if (level < -100)
				pct = 0;
			else
				pct = (int)((level + 100) * 2);

			if (pct < 0)
				pct = 0;
			if (pct > 100)
				pct = 100;
		}
	}

	fclose(f);
	s->wifi_usage = pct;
}

static void
spawn(const char *cmd)
{
	pid_t pid;

	if (!cmd || !cmd[0])
		return;

	pid = fork();
	if (pid < 0)
		return;
	if (pid == 0) {
		int fd = open("/dev/null", O_RDONLY);

		if (fd >= 0) {
			dup2(fd, STDIN_FILENO);
			if (fd != STDIN_FILENO)
				close(fd);
		}
		execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
		_exit(127);
	}
}

static void
publish_status(struct state *s)
{
	char line[256];
	size_t len;
	int i, focused = 0;
	FILE *f;

	for (i = 0; i < 32; i++) {
		if (s->tags_sel & (1u << i)) {
			focused = i + 1;
			break;
		}
	}

	len = (size_t)snprintf(line, sizeof line, "%d", focused);
	for (i = 0; i < 32 && len < sizeof line; i++) {
		int n;

		if (i + 1 == focused)
			continue;
		if (!(s->tags_occ & (1u << i)))
			continue;

		n = snprintf(line + len, sizeof line - len, " %d", i + 1);
		if (n < 0)
			break;
		if ((size_t)n >= sizeof line - len) {
			len = sizeof line - 1;
			break;
		}
		len += (size_t)n;
	}

	f = fopen(STATUS_TMP, "w");
	if (!f)
		return;
	fprintf(f, "%s\n", line);
	fclose(f);
	rename(STATUS_TMP, STATUS_FILE);
}

static void
parse_line(struct state *s, const char *line)
{
	unsigned int occ, sel, foc, urg;
	int changed;

	if (sscanf(line, "%*s tags %u %u %u %u", &occ, &sel, &foc, &urg) != 4)
		return;

	changed = s->tags_occ != occ || s->tags_sel != sel ||
	          s->tags_foc != foc || s->tags_urg != urg;

	s->tags_occ = occ;
	s->tags_sel = sel;
	s->tags_foc = foc;
	s->tags_urg = urg;

	if (changed)
		s->dirty = 1;

	publish_status(s);
}

static void
handle_stdin(struct state *s)
{
	char buf[4096];
	ssize_t n;
	char *line, *nl;
	size_t rem;

	n = read(STDIN_FILENO, buf, sizeof buf);
	if (n < 0) {
		if (errno == EINTR || errno == EAGAIN)
			return;
		s->running = 0;
		return;
	}
	if (n == 0) {
		s->running = 0;
		return;
	}

	if (s->inlen + (size_t)n >= sizeof s->inbuf)
		s->inlen = 0;

	memcpy(s->inbuf + s->inlen, buf, (size_t)n);
	s->inlen += (size_t)n;
	s->inbuf[s->inlen] = '\0';

	line = s->inbuf;
	while ((nl = strchr(line, '\n')) != NULL) {
		*nl = '\0';
		parse_line(s, line);
		line = nl + 1;
	}

	rem = strlen(line);
	if (rem > 0)
		memmove(s->inbuf, line, rem);
	s->inlen = rem;
	s->inbuf[s->inlen] = '\0';
}

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	struct state *s = data;

	(void)buffer;
	s->buffer_busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_release,
};

static void
destroy_buffer(struct state *s)
{
	if (s->buffer) {
		wl_buffer_destroy(s->buffer);
		s->buffer = NULL;
	}
	if (s->pixels) {
		munmap(s->pixels, s->pixels_size);
		s->pixels = NULL;
		s->pixels_size = 0;
	}
	s->buffer_busy = 0;
}

static void
create_buffer(struct state *s)
{
	int stride, fd;
	size_t size;
	void *data;
	struct wl_shm_pool *pool;

	destroy_buffer(s);

	stride = (int)s->width * 4;
	size = (size_t)stride * s->height;

	fd = memfd_create("sdwlb", MFD_CLOEXEC);
	if (fd < 0)
		die("memfd_create");

	if (ftruncate(fd, (off_t)size) < 0) {
		close(fd);
		die("ftruncate");
	}

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		die("mmap");
	}

	pool = wl_shm_create_pool(s->shm, fd, (int32_t)size);
	if (!pool) {
		munmap(data, size);
		close(fd);
		die("wl_shm_create_pool");
	}

	s->buffer = wl_shm_pool_create_buffer(pool, 0,
	                                      (int32_t)s->width,
	                                      (int32_t)s->height,
	                                      stride,
	                                      WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	if (!s->buffer) {
		munmap(data, size);
		die("wl_shm_pool_create_buffer");
	}

	s->pixels = data;
	s->pixels_size = size;
	s->buffer_busy = 0;
	wl_buffer_add_listener(s->buffer, &buffer_listener, s);
}

static void
layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface,
                        uint32_t serial, uint32_t width, uint32_t height)
{
	struct state *s = data;

	zwlr_layer_surface_v1_ack_configure(surface, serial);

	if (width == 0)
		width = s->width;
	if (height == 0)
		height = BAR_HEIGHT;
	if (width == 0)
		return;

	if (width != s->width || height != s->height || !s->buffer) {
		s->width = width;
		s->height = height;
		create_buffer(s);
	}

	s->configured = 1;
	s->dirty = 1;
}

static void
layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface)
{
	struct state *s = data;

	(void)surface;
	s->running = 0;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void
handle_click(struct state *s)
{
	int i;

	for (i = s->nclickables - 1; i >= 0; i--) {
		struct clickable *c = &s->clickables[i];

		if (s->pointer_x >= c->x && s->pointer_x < c->x + c->w &&
		    s->pointer_y >= c->y && s->pointer_y < c->y + c->h) {
			spawn(c->cmd);
			break;
		}
	}
}

static void
pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial,
              struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy)
{
	struct state *s = data;

	(void)pointer;
	(void)serial;
	if (surface != s->surface)
		return;

	s->pointer_inside = 1;
	s->pointer_x = wl_fixed_to_int(sx);
	s->pointer_y = wl_fixed_to_int(sy);
}

static void
pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial,
              struct wl_surface *surface)
{
	struct state *s = data;

	(void)pointer;
	(void)serial;
	(void)surface;
	s->pointer_inside = 0;
}

static void
pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time,
               wl_fixed_t sx, wl_fixed_t sy)
{
	struct state *s = data;

	(void)pointer;
	(void)time;
	if (!s->pointer_inside)
		return;

	s->pointer_x = wl_fixed_to_int(sx);
	s->pointer_y = wl_fixed_to_int(sy);
}

static void
pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
               uint32_t time, uint32_t button, uint32_t state)
{
	struct state *s = data;

	(void)pointer;
	(void)serial;
	(void)time;
	if (state == WL_POINTER_BUTTON_STATE_PRESSED &&
	    button == BTN_LEFT && s->pointer_inside)
		handle_click(s);
}

static void
pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time,
             uint32_t axis, wl_fixed_t value)
{
	(void)data;
	(void)pointer;
	(void)time;
	(void)axis;
	(void)value;
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_enter,
	.leave = pointer_leave,
	.motion = pointer_motion,
	.button = pointer_button,
	.axis = pointer_axis,
};

static void
seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
	struct state *s = data;

	if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !s->pointer) {
		s->pointer = wl_seat_get_pointer(seat);
		if (s->pointer)
			wl_pointer_add_listener(s->pointer, &pointer_listener, s);
	} else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) &&
	           s->pointer) {
		wl_pointer_destroy(s->pointer);
		s->pointer = NULL;
		s->pointer_inside = 0;
	}
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
                const char *interface, uint32_t version)
{
	struct state *s = data;

	(void)version;

	if (strcmp(interface, wl_compositor_interface.name) == 0)
		s->compositor = wl_registry_bind(registry, name,
		                                 &wl_compositor_interface, 1);
	else if (strcmp(interface, wl_shm_interface.name) == 0)
		s->shm = wl_registry_bind(registry, name,
		                          &wl_shm_interface, 1);
	else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0)
		s->layer_shell = wl_registry_bind(registry, name,
		                                  &zwlr_layer_shell_v1_interface,
		                                  1);
	else if (strcmp(interface, wl_seat_interface.name) == 0) {
		s->seat = wl_registry_bind(registry, name,
		                           &wl_seat_interface, 1);
		if (s->seat)
			wl_seat_add_listener(s->seat, &seat_listener, s);
	}
}

static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

int
main(void)
{
	struct state s;
	struct sigaction sa;
	uint32_t anchor;

	memset(&s, 0, sizeof s);
	s.running = 1;
	umask(077);

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = sig_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	signal(SIGCHLD, SIG_IGN);

	s.display = wl_display_connect(NULL);
	if (!s.display)
		die("wl_display_connect");
	fcntl(wl_display_get_fd(s.display), F_SETFD, FD_CLOEXEC);

	s.registry = wl_display_get_registry(s.display);
	wl_registry_add_listener(s.registry, &registry_listener, &s);
	wl_display_roundtrip(s.display);

	if (!s.compositor || !s.shm || !s.layer_shell)
		die("missing wayland globals");

	s.surface = wl_compositor_create_surface(s.compositor);
	if (!s.surface)
		die("wl_compositor_create_surface");

	s.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
	    s.layer_shell, s.surface, NULL,
	    ZWLR_LAYER_SHELL_V1_LAYER_TOP, "sdwlb");
	if (!s.layer_surface)
		die("zwlr_layer_shell_v1_get_layer_surface");

	zwlr_layer_surface_v1_add_listener(s.layer_surface,
	                                   &layer_surface_listener, &s);

	anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
	         ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
#if BAR_POSITION
	anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
#else
	anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
#endif

	zwlr_layer_surface_v1_set_anchor(s.layer_surface, anchor);
	zwlr_layer_surface_v1_set_size(s.layer_surface, 0, BAR_HEIGHT);
	zwlr_layer_surface_v1_set_exclusive_zone(s.layer_surface, BAR_HEIGHT);
	zwlr_layer_surface_v1_set_keyboard_interactivity(
	    s.layer_surface,
	    ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);

	wl_surface_commit(s.surface);
	wl_display_flush(s.display);

	s.cpu_usage = read_cpu_usage(&s);
	s.mem_usage = read_mem_usage();
	read_volume(&s);
	read_wifi(&s);

	while (s.running && sig_running) {
		struct pollfd fds[2];
		int timeout = UPDATE_MS > 0 ? UPDATE_MS : 1000;
		int ret;

		fds[0].fd = wl_display_get_fd(s.display);
		fds[0].events = POLLIN;
		fds[1].fd = STDIN_FILENO;
		fds[1].events = POLLIN;

		while (wl_display_prepare_read(s.display) != 0)
			wl_display_dispatch_pending(s.display);

		wl_display_flush(s.display);

		ret = poll(fds, 2, timeout);
		if (ret < 0) {
			wl_display_cancel_read(s.display);
			if (errno == EINTR)
				continue;
			break;
		}

		if (ret > 0 && (fds[0].revents & POLLIN)) {
			wl_display_read_events(s.display);
			wl_display_dispatch_pending(s.display);
		} else {
			wl_display_cancel_read(s.display);
		}

		if (ret > 0 && (fds[0].revents & (POLLERR | POLLHUP)))
			s.running = 0;

		if (ret > 0 && (fds[1].revents & (POLLIN | POLLHUP)))
			handle_stdin(&s);

		if (ret == 0) {
			s.cpu_usage = read_cpu_usage(&s);
			s.mem_usage = read_mem_usage();
			read_volume(&s);
			read_wifi(&s);
			s.dirty = 1;
		}

		if (s.dirty && !s.buffer_busy)
			draw(&s);
	}

	destroy_buffer(&s);
	if (s.pointer)
		wl_pointer_destroy(s.pointer);
	if (s.seat)
		wl_seat_destroy(s.seat);
	if (s.layer_surface)
		zwlr_layer_surface_v1_destroy(s.layer_surface);
	if (s.surface)
		wl_surface_destroy(s.surface);
	if (s.layer_shell)
		zwlr_layer_shell_v1_destroy(s.layer_shell);
	if (s.shm)
		wl_shm_destroy(s.shm);
	if (s.compositor)
		wl_compositor_destroy(s.compositor);
	if (s.registry)
		wl_registry_destroy(s.registry);
	if (s.display)
		wl_display_disconnect(s.display);

	return 0;
}
