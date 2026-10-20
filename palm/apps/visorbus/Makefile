.DELETE_ON_ERROR:
CC = m68k-palmos-gcc
CFLAGS = -O2 -Wall -palmos3.5
PYTHON = python3
MAKEROM = ../../../tools/makerom/build/Palm-MakeROM

all: visorbus.prc

visorbus: visorbus.c reset_test.c reset_test.h read_test.c read_test.h visorbus_rsc.h hs_card.h
	$(CC) $(CFLAGS) -o $@ visorbus.c reset_test.c read_test.c

bin.stamp: visorbus.rcp visorbus_rsc.h
	pilrc -q visorbus.rcp
	touch $@

tPAT03e8.bin:
	$(PYTHON) -c 'from pathlib import Path; Path("$@").write_bytes(bytes(range(256)))'

visorbus.prc: visorbus visorbus.def bin.stamp tPAT03e8.bin
	build-prc -o $@ visorbus.def visorbus *.bin
	$(PYTHON) -c 'from pathlib import Path; p=Path("$@"); b=bytearray(p.read_bytes()); b[36:48]=bytes(12); p.write_bytes(b)'

$(MAKEROM): ../../../tools/makerom/*.cpp ../../../tools/makerom/*.h
	$(PYTHON) ../../../tools/makerom/build.py

rom: build/visorbus.rom

build/visorbus.rom: visorbus.prc $(MAKEROM) ../../../tools/verify-springboard-rom.py
	mkdir -p build
	$(MAKEROM) -op create -hdr 0x08000000 -romBlock 0x08000000 0x10000 -chChecksum -chName "FujiNet Visor Test" -chManuf "FujiNet" -chVersion 0x0100 -chZCrDate -romName "Visor Bus Test ROM" -autoSize -tokStr HsAT 300 -romDB visorbus.prc -o $@
	$(PYTHON) ../../../tools/verify-springboard-rom.py $@ --prc visorbus.prc --manifest build/visorbus.json

clean:
	rm -f visorbus visorbus.prc *.bin bin.stamp
	rm -rf build tests/build

.PHONY: all rom clean

HOST_CC ?= cc
check: tests/build/test_reset tests/build/test_read
	./tests/build/test_reset
	./tests/build/test_read

tests/build/test_reset: tests/test_reset.c reset_test.c reset_test.h
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I. tests/test_reset.c reset_test.c -o $@

.PHONY: check

tests/build/test_read: tests/test_read.c read_test.c read_test.h reset_test.h
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I. tests/test_read.c read_test.c -o $@
