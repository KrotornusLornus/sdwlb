PREFIX ?= /usr/local
CC ?= cc
PKG_CONFIG ?= pkg-config
WAYLAND_SCANNER ?= wayland-scanner

WAYLAND_PROTOCOLS ?= $(shell $(PKG_CONFIG) --variable=pkgdatadir wayland-protocols)

CFLAGS ?= -O2
CFLAGS += -std=c99 -Wall -Wextra -Wpedantic -Wno-unused-parameter
CPPFLAGS += -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS += $(shell $(PKG_CONFIG) --cflags wayland-client)
LDLIBS = $(shell $(PKG_CONFIG) --libs wayland-client)
