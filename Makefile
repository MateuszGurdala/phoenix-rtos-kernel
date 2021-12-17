#
# Makefile for phoenix-rtos-kernel
#
# Copyright 2018, 2019 Phoenix Systems
#
# %LICENSE%
#

VERSION="2.97 rev: $(shell git rev-parse --short HEAD)"
CONSOLE?=vga
KERNEL=1

SIL ?= @
MAKEFLAGS += --no-print-directory

#TARGET ?= armv7m3-stm32l152xd
#TARGET ?= armv7m3-stm32l152xe
#TARGET ?= armv7m4-stm32l4x6
#TARGET ?= armv7m7-imxrt105x
#TARGET ?= armv7m7-imxrt106x
#TARGET ?= armv7m7-imxrt117x
#TARGET ?= armv7a7-imx6ull
TARGET ?= ia32-generic
#TARGET ?= riscv64-spike

include ../phoenix-rtos-build/Makefile.common
include ../phoenix-rtos-build/Makefile.$(TARGET_SUFF)

CFLAGS += $(BOARD_CONFIG)
CFLAGS += -I. -DVERSION=\"$(VERSION)\"

EXTERNAL_HEADERS_DIR := ./include
EXTERNAL_HEADERS := $(shell find $(EXTERNAL_HEADERS_DIR) -name \*.h)

SYSROOT := $(shell $(CC) $(CFLAGS) -print-sysroot)
HEADERS_INSTALL_DIR := $(SYSROOT)/usr/include/phoenix
ifeq (/,$(SYSROOT))
$(error Sysroot is not supported by toolchain. Use Phoenix-RTOS cross-toolchain to compile)
endif

OBJS = $(addprefix $(PREFIX_O), main.o syscalls.o syspage.o)

all: $(PREFIX_PROG_STRIPPED)phoenix-$(TARGET_FAMILY)-$(TARGET_SUBFAMILY).elf

include hal/$(TARGET_SUFF)/Makefile
include vm/Makefile
include proc/Makefile
include posix/Makefile
include lib/Makefile
include test/Makefile
include klog/Makefile


$(PREFIX_PROG)phoenix-$(TARGET_FAMILY)-$(TARGET_SUBFAMILY).elf: $(OBJS)
	@mkdir -p $(@D)
	@(printf "LD  %-24s\n" "$(@F)");
	$(SIL)$(LD) $(LDFLAGS) -e _start --section-start .init=$(VADDR_KERNEL_INIT) -o $@ $(OBJS) $(GCCLIB)


install-headers: $(EXTERNAL_HEADERS)
	@printf "Installing kernel headers\n"
	@mkdir -p "$(HEADERS_INSTALL_DIR)"; \
	for file in $(EXTERNAL_HEADERS); do\
		mkdir -p "$(HEADERS_INSTALL_DIR)/`dirname $${file#$(EXTERNAL_HEADERS_DIR)}`"; \
		install -p -m 644 $${file} "$(HEADERS_INSTALL_DIR)/$${file#$(EXTERNAL_HEADERS_DIR)}";\
	done


uninstall-headers:
	rm -rf "$(HEADERS_INSTALL_DIR)"/*


.PHONY: clean install-headers uninstall-headers
clean:
	@echo "rm -rf $(BUILD_DIR)"

ifneq ($(filter clean,$(MAKECMDGOALS)),)
	$(shell rm -rf $(BUILD_DIR))
	$(shell rm -f string/*.inc)
endif
