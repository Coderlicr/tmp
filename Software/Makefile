# Makefile for Fruit-Ninja DE1-SoC software
#
# Two targets:
#   make game     - build userspace game program (cross-compile or native)
#   make module   - build Linux kernel module  (needs kernel headers)

# Cross-compiler prefix (leave empty when compiling natively on DE1-SoC)
CROSS_COMPILE ?= arm-linux-gnueabihf-
CC            := $(CROSS_COMPILE)gcc

ifneq (${KERNELRELEASE},)

# Called from kernel build system
obj-m := top_module.o

else

KERNEL_SOURCE := /usr/src/linux-headers-$(shell uname -r)
PWD := $(shell pwd)

default: module game

# ---------- Kernel module ----------
module:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} modules

# ---------- Userspace game ----------
game: game.c top_module.h
	$(CC) -O2 -Wall -o $@ game.c

# ---------- Utilities ----------
clean:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} clean
	${RM} game

TARFILES = Makefile README top_module.h top_module.c game.c
TARFILE = fruit_ninja_sw.tar.gz
.PHONY : tar
tar : $(TARFILE)

$(TARFILE) : $(TARFILES)
	tar zcf $(TARFILE) $(TARFILES)

endif
