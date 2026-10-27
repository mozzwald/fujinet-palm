.DELETE_ON_ERROR:
CC = m68k-palmos-gcc
HOST_CC ?= cc
OUT = build
.PHONY: all check
all: $(OUT)/timing.prc
$(OUT)/timing: timing.c core.c core.h loads.S
	mkdir -p $(OUT)
	$(CC) -O2 -Wall -Werror -palmos3.5 -o $@ timing.c core.c loads.S
$(OUT)/bin.stamp: timing.rcp
	mkdir -p $(OUT)
	cp timing.rcp $(OUT)/timing.rcp
	cd $(OUT) && pilrc -q timing.rcp
	touch $@
$(OUT)/timing.prc: $(OUT)/timing timing.def $(OUT)/bin.stamp
	build-prc -o $@ timing.def $(OUT)/timing $(OUT)/*.bin
	python3 -c 'from pathlib import Path; p=Path("$@"); b=bytearray(p.read_bytes()); b[36:48]=bytes(12); p.write_bytes(b)'
check: build-host/test_core
	./build-host/test_core
build-host/test_core: tests/test_core.c core.c core.h
	mkdir -p build-host
	$(HOST_CC) -DVT_HOST -std=c99 -O2 -Wall -Wextra -Werror -I. tests/test_core.c core.c -o $@
