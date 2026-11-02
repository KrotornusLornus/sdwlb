.POSIX:
.SUFFIXES:

include config.mk

PROTO_LAYER = protocols/wlr-layer-shell-unstable-v1.xml
PROTO_XDG = $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml
GEN = wlr-layer-shell-unstable-v1-protocol.c wlr-layer-shell-unstable-v1-protocol.h \
      xdg-shell-protocol.c xdg-shell-protocol.h
OBJ = mini-status-bar.o wlr-layer-shell-unstable-v1-protocol.o xdg-shell-protocol.o

all: mini-status-bar

config.h:
	cp config.def.h config.h

mini-status-bar: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

mini-status-bar.o: mini-status-bar.c config.h wlr-layer-shell-unstable-v1-protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ mini-status-bar.c

wlr-layer-shell-unstable-v1-protocol.h: $(PROTO_LAYER)
	$(WAYLAND_SCANNER) client-header $(PROTO_LAYER) $@

wlr-layer-shell-unstable-v1-protocol.c: $(PROTO_LAYER)
	$(WAYLAND_SCANNER) private-code $(PROTO_LAYER) $@

wlr-layer-shell-unstable-v1-protocol.o: wlr-layer-shell-unstable-v1-protocol.c wlr-layer-shell-unstable-v1-protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ wlr-layer-shell-unstable-v1-protocol.c

xdg-shell-protocol.h: $(PROTO_XDG)
	$(WAYLAND_SCANNER) client-header $(PROTO_XDG) $@

xdg-shell-protocol.c: $(PROTO_XDG)
	$(WAYLAND_SCANNER) private-code $(PROTO_XDG) $@

xdg-shell-protocol.o: xdg-shell-protocol.c xdg-shell-protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ xdg-shell-protocol.c

clean:
	rm -f mini-status-bar $(OBJ) $(GEN)

.PHONY: all clean
