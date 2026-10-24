.DELETE_ON_ERROR:
CC = m68k-palmos-gcc
CFLAGS = -O2 -Wall -palmos3.5
all: rominspect.prc
rominspect: rominspect.c ../visorbus/hs_card.h ../visorbus/reset_test.h
	$(CC) $(CFLAGS) -o $@ rominspect.c
bin.stamp: rominspect.rcp
	pilrc -q rominspect.rcp
	touch $@
rominspect.prc: rominspect rominspect.def bin.stamp
	build-prc -o $@ rominspect.def rominspect *.bin
	python3 -c 'from pathlib import Path; p=Path("$@"); b=bytearray(p.read_bytes()); b[36:48]=bytes(12); p.write_bytes(b)'
clean:
	rm -f rominspect rominspect.prc *.bin bin.stamp
.PHONY: all clean
