BLOCKSDS ?= /opt/blocksds/core

NAME := soundcloud_ds
GAME_TITLE := SoundCloud DS
GAME_SUBTITLE := Search. Discover. Play.
GAME_AUTHOR := ivnis

SOURCEDIRS := source
INCLUDEDIRS := include
CXXFLAGS += -std=gnu++17 -Wextra -Werror

ARM7ELF := $(BLOCKSDS)/sys/arm7/main_core/arm7_dswifi_maxmod.elf
LIBS := -ldswifi9 -lmm9 -lnds9
LIBDIRS := $(BLOCKSDS)/libs/dswifi $(BLOCKSDS)/libs/maxmod $(BLOCKSDS)/libs/libnds

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile

# Optional header may be created after the first compilation; __has_include
# does not add a dependency for a header that did not exist at compile time.
$(BUILDDIR)/source/main.cpp.o: $(wildcard include/config.local.h)
