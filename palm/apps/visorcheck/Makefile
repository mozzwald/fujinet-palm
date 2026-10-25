.DELETE_ON_ERROR:
CC = m68k-palmos-gcc
CFLAGS = -O2 -Wall -palmos3.5 -I../visorbus -Ibuild
PYTHON = python3
HOST_CC ?= cc

all: visorcheck.prc

build/expected_rom.h: ../visorbus/build/visorbus.rom ../visorbus/build/visorbus.json ../../../tools/rom-check-info.py
	mkdir -p build
	$(PYTHON) ../../../tools/rom-check-info.py ../visorbus/build/visorbus.rom ../visorbus/build/visorbus.json $@

PAIR_SOURCES = pair_app.c pair_codec.c pair_load.S
CHECK_SOURCES = visorcheck.c diagnostic.c read_spacing.c burst_read.c result_codec.c result_store.c timed_read.c image_read.c ../visorbus/read_test.c $(PAIR_SOURCES)

visorcheck: app_identity.h diagnostic.c diagnostic.h read_spacing.c read_spacing.h burst_read.c burst_read.h result_codec.c result_codec.h result_store.c result_store.h visorcheck.c timed_read.c timed_read.h image_read.c image_read.h ../visorbus/read_test.c ../visorbus/read_test.h ../visorbus/hs_card.h build/expected_rom.h $(PAIR_SOURCES) pair_codec.h pair_app.h
	$(CC) $(CFLAGS) -o $@ $(CHECK_SOURCES)

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
	./tests/build/test_pair_codec
	./tests/build/test_pair_guard
	./tests/build/test_wordpair_codec

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

# Separate reference/build outputs preserve the original FNVC RAM checker.
build/romprobe-reference/expected_rom.h: ../romprobe/build/romprobe.rom ../romprobe/build/romprobe.json ../../../tools/rom-check-info.py
	mkdir -p build/romprobe-reference
	$(PYTHON) ../../../tools/rom-check-info.py ../romprobe/build/romprobe.rom ../romprobe/build/romprobe.json $@

romprobe-checker: $(CHECK_SOURCES) pair_codec.h pair_app.h build/romprobe-reference/expected_rom.h
	$(CC) $(filter-out -Ibuild,$(CFLAGS)) -Ibuild/romprobe-reference -DVC_ROMPROBE_REFERENCE=1 -o $@ $(CHECK_SOURCES)

build/romprobe-reference/bin.stamp: visorcheck.rcp
	mkdir -p build/romprobe-reference
	sed 's/Visor ROM Check 0.6.1/ROM Check R3 + Pairs/' visorcheck.rcp > build/romprobe-reference/visorcheck.rcp
	cd build/romprobe-reference && pilrc -q visorcheck.rcp
	touch $@

romprobe-checker.prc: romprobe-checker romprobe-checker.def build/romprobe-reference/bin.stamp ../../../tools/verify-pair-load.py
	build-prc -o $@ romprobe-checker.def romprobe-checker build/romprobe-reference/*.bin
	$(PYTHON) -c 'from pathlib import Path; p=Path("$@"); b=bytearray(p.read_bytes()); b[36:48]=bytes(12); p.write_bytes(b)'

check: tests/build/test_pair_codec

tests/build/test_pair_codec: tests/test_pair_codec.c pair_codec.c pair_codec.h result_codec.c image_read.c read_spacing.c ../visorbus/read_test.c
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I. -I../visorbus tests/test_pair_codec.c pair_codec.c result_codec.c image_read.c read_spacing.c ../visorbus/read_test.c -o $@

# Instruction and PRC ownership verification is part of the distinct build.
verify-pairs: romprobe-checker.prc
	m68k-palmos-objdump -d romprobe-checker > build/romprobe-reference/disassembly.txt
	$(PYTHON) ../../../tools/verify-pair-load.py build/romprobe-reference/disassembly.txt romprobe-checker.prc
.PHONY: verify-pairs

# Separate FNVE RAM-only word-pair diagnostic; existing PRCs stay untouched.
WORD_SOURCES = wordpair_main.c pair_app.c pair_codec.c wordpair_load.S result_codec.c image_read.c read_spacing.c ../visorbus/read_test.c
build/wordpair/wordpair: $(WORD_SOURCES) pair_codec.h pair_app.h app_identity.h build/romprobe-reference/expected_rom.h
	mkdir -p build/wordpair
	$(CC) $(filter-out -Ibuild,$(CFLAGS)) -Ibuild/romprobe-reference -DVC_WORDPAIR_DIAGNOSTIC=1 -o $@ $(WORD_SOURCES)

build/wordpair/bin.stamp: wordpair.rcp
	mkdir -p build/wordpair
	cp wordpair.rcp build/wordpair/wordpair.rcp
	cd build/wordpair && pilrc -q wordpair.rcp
	touch $@

build/wordpair/wordpair.prc: build/wordpair/wordpair wordpair.def build/wordpair/bin.stamp
	build-prc -o $@ wordpair.def build/wordpair/wordpair build/wordpair/*.bin
	$(PYTHON) -c 'from pathlib import Path; p=Path("$@"); b=bytearray(p.read_bytes()); b[36:48]=bytes(12); p.write_bytes(b)'

verify-wordpairs: build/wordpair/wordpair.prc
	m68k-palmos-objdump -d build/wordpair/wordpair > build/wordpair/disassembly.txt
	$(PYTHON) ../../../tools/verify-wordpair-loads.py build/wordpair/disassembly.txt build/wordpair/wordpair.prc
.PHONY: verify-wordpairs

check: tests/build/test_wordpair_codec
tests/build/test_wordpair_codec: tests/test_wordpair_codec.c pair_codec.c pair_codec.h result_codec.c image_read.c read_spacing.c ../visorbus/read_test.c
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -DVC_WORDPAIR_DIAGNOSTIC=1 -I. -I../visorbus tests/test_wordpair_codec.c pair_codec.c result_codec.c image_read.c read_spacing.c ../visorbus/read_test.c -o $@

check: tests/build/test_pair_guard
tests/build/test_pair_guard: tests/test_pair_guard.c ../visorbus/read_test.c ../visorbus/read_test.h
	mkdir -p tests/build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I../visorbus tests/test_pair_guard.c ../visorbus/read_test.c -o $@
