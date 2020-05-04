.PHONY := kbuild all clean

srctree := $(shell pwd)
include tools/Kbuild.include

# List of user libraries used by the kernel
KLIB_O = lib/syscall/wramp_syscall.o lib/syscall/errno.o lib/syscall/ipc.o \
		lib/ansi/string.o lib/util/util.o lib/gen/ucontext.o lib/stdlib/atoi.o\
		lib/syscall/debug.o lib/posix/_sigset.o lib/ansi/rand.o

L_HEAD = winix/limits/limits_head.o
L_TAIL = winix/limits/limits_tail.o
KERNEL_O = winix/*.o kernel/system/*.o kernel/*.o fs/*.o fs/system/*.o
KMAIN = kernel/main.s kernel/main.o 
alldir = winix lib init user kernel fs


# Check if V options is set by user, if V=1, debug mode is set
# e.g. make V=1 produces all the commands being executed through
# the building process
ifeq ("$(origin V)", "command line")
	KBUILD_VERBOSE = $(V)
endif
ifndef KBUILD_VERBOSE
	KBUILD_VERBOSE = 0
endif

ifeq ($(KBUILD_VERBOSE),1)
	quiet =
	Q =
else
	MAKEFLAGS += --no-print-directory -s
	quiet=quiet_
	Q = @
endif

export KBUILD_VERBOSE
export Q
export quiet
export srctree \\
export includes := $(shell find include -name "*.h")
export CFLAGS := -D_DEBUG



all:| tool kbuild
	$(Q)wlink $(LDFLAGS) -Ttext 1024 -v -o winix.srec \
			$(L_HEAD) $(KERNEL_O) $(KLIB_O) $(L_TAIL) \
							> tools/kdbg_srec/winix.kdbg
ifeq ($(KBUILD_VERBOSE),0)
	@echo "LD \t winix.srec"
endif

tool: 
	$(Q)-rm -f $(KMAIN)
	$(Q)$(MAKE) -C tools

kbuild: $(alldir)
$(alldir): FORCE
	$(Q)$(MAKE) $(build)=$@

clean:
	$(Q)$(MAKE) -C tools clean
	$(Q)$(MAKE) $(cleanall)='$(alldir)'


d-verbose:
	$(MAKE) all CFLAGS=-D_DEBUG=2

stat:
	@echo "C Lines: "
	@find . -type d -name "include" -prune -o -name "*.c"  -exec cat {} \; | wc -l
	@echo "Header LoC: "
	@find . -name "*.h" -exec cat {} \; | wc -l
	@echo "Assembly LoC: "
	@find . -name "*.s" -exec cat {} \; | wc -l

test:
	gcc -D_GCC_DEBUG -I./include test.c winix/bitmap.c winix/mm.c

FORCE:

.DELETE_ON_ERROR:


# DO NOT DELETE
