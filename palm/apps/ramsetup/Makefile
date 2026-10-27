.DELETE_ON_ERROR:
CC = m68k-palmos-gcc
HOST_CC ?= cc
PYTHON ?= python3
OUT = build
HOST_OUT = build-host
TOOLS = ../../../tools
MAKEROM = $(TOOLS)/makerom/build/Palm-MakeROM
.PHONY: all rom check
all: $(OUT)/ramsetup.prc
$(OUT)/ramsetup: Makefile ramsetup.c policy.c policy.h hs_setup.h ramproof.S
	mkdir -p $(OUT)
	$(CC) -O2 -Wall -Werror -palmos3.5 -o $@ ramsetup.c policy.c ramproof.S
$(OUT)/bin.stamp: Makefile ramsetup.rcp
	mkdir -p $(OUT)
	cp ramsetup.rcp $(OUT)/ramsetup.rcp
	cd $(OUT) && pilrc -q ramsetup.rcp
	$(PYTHON) -c 'from pathlib import Path; Path("$(OUT)/rP2I03e8.bin").write_bytes(b"FN RAM P2 000002\0"+bytes(8))'
	touch $@
$(OUT)/raw.prc: $(OUT)/ramsetup ramsetup.def $(OUT)/bin.stamp
	build-prc -o $@ ramsetup.def $(OUT)/ramsetup $(OUT)/*.bin
$(OUT)/ramsetup.prc: $(OUT)/raw.prc $(TOOLS)/prepare-ramsetup-prc.py
	$(PYTHON) $(TOOLS)/prepare-ramsetup-prc.py $< $@
rom: $(OUT)/ramsetup.rom
$(OUT)/ramsetup.rom: $(OUT)/ramsetup.prc Makefile $(TOOLS)/verify-ramsetup-rom.py
	test -x $(MAKEROM)
	$(MAKEROM) -op create -hdr 0x08000000 -romBlock 0x08000000 0x10000 -chChecksum -chName "FujiNet RAM P2" -chManuf "FujiNet" -chVersion 0x0100 -chZCrDate -romName "FN RAM Install P2" -autoSize -tokStr HsAT 330 -romDB $< -o $@.unverified
	$(PYTHON) $(TOOLS)/verify-ramsetup-rom.py $@.unverified --prc $< --manifest $(OUT)/ramsetup.json
	cp $@.unverified $@
check: $(HOST_OUT)/test_policy
	./$(HOST_OUT)/test_policy $(HOST_OUT)/codec.golden
$(HOST_OUT)/test_policy: tests/test_policy.c policy.c policy.h
	mkdir -p $(HOST_OUT)
	$(HOST_CC) -DRP_HOST -std=c99 -O2 -Wall -Wextra -Werror -I. tests/test_policy.c policy.c -o $@
