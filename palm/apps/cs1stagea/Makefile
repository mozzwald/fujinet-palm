.DELETE_ON_ERROR:
CC = m68k-palmos-gcc
ABI ?= ../../../../../fujinet-nio/bridges/rp2350-springboard/include
all: build/stagea.prc
build/stagea: stagea.c word_access.c pc.S $(ABI)/visor_mailbox.h
	mkdir -p build
	$(CC) -O2 -Wall -Werror -palmos3.5 -I$(ABI) -o $@ stagea.c word_access.c pc.S
build/bin.stamp: stagea.rcp
	mkdir -p build
	cp stagea.rcp build/
	cd build && pilrc -q stagea.rcp
	touch $@
build/stagea.prc: build/stagea stagea.def build/bin.stamp
	build-prc -o $@ stagea.def build/stagea build/*.bin
	python3 -c 'from pathlib import Path; p=Path("$@"); b=bytearray(p.read_bytes()); b[36:48]=bytes(12); p.write_bytes(b)'
