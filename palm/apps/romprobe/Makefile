.DELETE_ON_ERROR:
CC = m68k-palmos-gcc
CFLAGS = -O2 -Wall -palmos3.5
PYTHON = python3
MAKEROM = ../../../tools/makerom/build/Palm-MakeROM

all: romprobe.prc

romprobe: romprobe.c
	$(CC) $(CFLAGS) -o $@ romprobe.c

bin.stamp: romprobe.rcp
	pilrc -q romprobe.rcp
	touch $@

tPAT03e8.bin:
	$(PYTHON) -c 'from pathlib import Path; Path("$@").write_bytes(bytes(range(256)))'

romprobe.prc: romprobe romprobe.def bin.stamp tPAT03e8.bin
	build-prc -o $@ romprobe.def romprobe *.bin
	$(PYTHON) -c 'from pathlib import Path; p=Path("$@"); b=bytearray(p.read_bytes()); b[36:48]=bytes(12); p.write_bytes(b)'

$(MAKEROM): ../../../tools/makerom/*.cpp ../../../tools/makerom/*.h
	$(PYTHON) ../../../tools/makerom/build.py

rom: build/romprobe.rom

build/romprobe.rom: Makefile romprobe.prc $(MAKEROM) ../../../tools/verify-springboard-rom.py
	mkdir -p build
	$(MAKEROM) -op create -hdr 0x08000000 -romBlock 0x08000000 0x10000 -chChecksum -chName "FujiNet ROM R3" -chManuf "FujiNet" -chVersion 0x0100 -chZCrDate -romName "ROM ONLY R3 20261023" -autoSize -tokStr HsAT 330 -romDB romprobe.prc -o $@
	$(PYTHON) ../../../tools/verify-springboard-rom.py $@ --prc romprobe.prc --manifest build/romprobe.json --creator FNVR

clean:
	rm -f romprobe romprobe.prc *.bin bin.stamp
	rm -rf build tests/build

.PHONY: all rom clean
