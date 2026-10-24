.DELETE_ON_ERROR:
CC = m68k-palmos-gcc
CFLAGS = -O2 -Wall -palmos3.5 -I../visorbus -Ibuild
PYTHON = python3
HOST_CC ?= cc

all: visorcheck.prc

build/expected_rom.h: ../visorbus/build/visorbus.rom ../visorbus/build/visorbus.json ../../../tools/rom-check-info.py
	mkdir -p build
	$(PYTHON) ../../../tools/rom-check-info.py ../visorbus/build/visorbus.rom ../visorbus/build/visorbus.json $@

visorcheck: app_identity.h diagnostic.c diagnostic.h read_spacing.c read_spacing.h burst_read.c burst_read.h result_codec.c result_codec.h result_store.c result_store.h visorcheck.c timed_read.c timed_read.h image_read.c image_read.h ../visorbus/read_test.c ../visorbus/read_test.h ../visorbus/hs_card.h build/expected_rom.h
	$(CC) $(CFLAGS) -o $@ visorcheck.c diagnostic.c read_spacing.c burst_read.c result_codec.c result_store.c timed_read.c image_read.c ../visorbus/read_test.c

bin.stamp: visorcheck.rcp
	pilrc -q visorcheck.rcp
	touch $@

visorcheck.prc: visorcheck visorcheck.def bin.stamp
	build-prc -o $@ visorcheck.def visorcheck *.bin
	$(PYTHON) -c 'from pathlib import Path; p=Path("$@"); b=bytearray(p.read_bytes()); b[36:48]=bytes(12); p.write_bytes(b)'

check: tests/build/test_diagnostic tests/build/test_image tests/build/test_timing tests/build/test_codec tests/build/test_store tests/build/test_burst
	./tests/build/test_diagnostic
	./tests/build/test_image
	./tests/build/test_timing
	./tests/build/test_codec
	./tests/build/test_store
	./tests/build/test_burst
	./tests/build/test_store_romprobe

tests/build/test_image: tests/test_image.c image_read.c image_read.h ../visorbus/read_test.c ../visorbus/read_test.h
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I. -I../visorbus tests/test_image.c image_read.c ../visorbus/read_test.c -o $@

clean:
	rm -f visorcheck visorcheck.prc *.bin bin.stamp
	rm -rf build tests/build

.PHONY: all check clean

tests/build/test_timing: tests/test_timing.c timed_read.c timed_read.h image_read.c image_read.h ../visorbus/read_test.c ../visorbus/read_test.h
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I. -I../visorbus tests/test_timing.c timed_read.c image_read.c ../visorbus/read_test.c -o $@

tests/build/test_codec: tests/test_codec.c result_codec.c result_codec.h read_spacing.c read_spacing.h image_read.c ../visorbus/read_test.c
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I. -I../visorbus tests/test_codec.c result_codec.c read_spacing.c image_read.c ../visorbus/read_test.c -o $@

tests/build/test_store: app_identity.h tests/test_store.c tests/mock/PalmOS.h result_store.c result_store.h result_codec.c result_codec.h read_spacing.c read_spacing.h image_read.c ../visorbus/read_test.c
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -Wno-multichar -Itests/mock -I. -I../visorbus tests/test_store.c result_store.c result_codec.c read_spacing.c image_read.c ../visorbus/read_test.c -o $@

tests/build/test_diagnostic: tests/test_diagnostic.c diagnostic.c diagnostic.h result_codec.h
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I. tests/test_diagnostic.c diagnostic.c -o $@

tests/build/test_burst: tests/test_burst.c burst_read.c burst_read.h read_spacing.c read_spacing.h timed_read.c image_read.c ../visorbus/read_test.c
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I. -I../visorbus tests/test_burst.c burst_read.c read_spacing.c timed_read.c image_read.c ../visorbus/read_test.c -o $@

# Verify the alternate reference never uses baseline DB or preference ownership.
check: tests/build/test_store_romprobe
tests/build/test_store_romprobe: app_identity.h tests/test_store.c tests/mock/PalmOS.h result_store.c result_store.h result_codec.c result_codec.h read_spacing.c read_spacing.h image_read.c ../visorbus/read_test.c
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -Wno-multichar -DVC_ROMPROBE_REFERENCE=1 -Itests/mock -I. -I../visorbus tests/test_store.c result_store.c result_codec.c read_spacing.c image_read.c ../visorbus/read_test.c -o $@
