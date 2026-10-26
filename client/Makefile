BLOCKSDS ?= /opt/blocksds/core

NAME := soundcloud_ds
GAME_TITLE := SoundCloud DS
VERSION := $(strip $(shell cat ../VERSION))
GAME_SUBTITLE := v$(VERSION) - Search and play
GAME_AUTHOR := ivnis

SOURCEDIRS := source
INCLUDEDIRS := include
CXXFLAGS += -std=gnu++17 -Wextra -Werror

ARM7ELF := $(BLOCKSDS)/sys/arm7/main_core/arm7_dswifi_maxmod.elf
LIBS := -ldswifi9 -lmm9 -lnds9
# A 5.8 KB TCP window throttles audio on high-latency Internet connections.
# Build only the ARM9 library with a larger window, leaving the SDK untouched.
WIFI_REV := 59d2636dd851ea26a4ea49f329321d3bfb002066
WIFI_ROOT := $(abspath ../tools/.cache/dswifi-stream)
WIFI_LIB := $(WIFI_ROOT)/lib/libdswifi9.a
LIBDIRS := $(WIFI_ROOT) $(BLOCKSDS)/libs/maxmod $(BLOCKSDS)/libs/libnds

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile

$(WIFI_ROOT)/.stream-source:
	git init "$(WIFI_ROOT)"
	git -C "$(WIFI_ROOT)" fetch --depth 1 https://github.com/blocksds/dswifi.git $(WIFI_REV)
	git -C "$(WIFI_ROOT)" checkout --detach FETCH_HEAD
	sed -i 's/^#define TCP_WND .*/#define TCP_WND                     (32 * TCP_MSS)/' "$(WIFI_ROOT)/source/arm9/lwip/include/lwipopts.h"
	touch "$@"

$(WIFI_LIB): $(WIFI_ROOT)/.stream-source
	$(MAKE) -C "$(WIFI_ROOT)" -f Makefile.arm9 ENABLE_LWIP=1 "BLOCKSDS=$(BLOCKSDS)" "WONDERFUL_TOOLCHAIN=$(WONDERFUL_TOOLCHAIN)" "SHELL=$(SHELL)"

$(ELF): $(WIFI_LIB)
$(OBJS_SOURCES): | $(WIFI_ROOT)/.stream-source

$(ROM): ../VERSION

# Optional header may be created after the first compilation; __has_include
# does not add a dependency for a header that did not exist at compile time.
$(BUILDDIR)/source/main.cpp.o: $(wildcard include/config.local.h)
