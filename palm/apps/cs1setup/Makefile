.DELETE_ON_ERROR:
CC = m68k-palmos-gcc
ABI ?= ../../../../../fujinet-nio/bridges/rp2350-springboard/include
TOOLS = ../../../tools
MAKEROM = $(TOOLS)/makerom/build/Palm-MakeROM
all: build/setup.prc
build/setup: installer.c policy.c policy.h hs_setup.h ramproof.S ../cs1stagea/stagea.c ../cs1stagea/word_access.c ../cs1stagea/pc.S $(ABI)/visor_mailbox.h
	mkdir -p build
	$(CC) -O2 -Wall -Werror -palmos3.5 -DVA_MODULE_INSTALL=1 -I$(ABI) -o $@ installer.c policy.c ramproof.S ../cs1stagea/stagea.c ../cs1stagea/word_access.c ../cs1stagea/pc.S
build/bin.stamp: setup.rcp Makefile
	mkdir -p build
	cp setup.rcp build/
	cd build && pilrc -q setup.rcp
	python3 -c 'from pathlib import Path; Path("build/rP2I03e8.bin").write_bytes(b"FN CS1 B 000002\0"+bytes(8))'
	touch $@
build/raw.prc: build/setup setup.def build/bin.stamp
	build-prc -o $@ setup.def build/setup build/*.bin
build/result-buffer.checked: build/setup $(TOOLS)/verify-cs1-result-buffer.py
	m68k-palmos-objdump -d $< > build/setup.dis
	python3 $(TOOLS)/verify-cs1-result-buffer.py build/setup.dis --output build/result-buffer-proof.json
	touch $@
build/setup.prc: build/raw.prc build/result-buffer.checked $(TOOLS)/prepare-cs1setup-prc.py
	python3 $(TOOLS)/prepare-cs1setup-prc.py $< $@
rom: build/setup.rom
build/setup.rom: build/setup.prc
	$(MAKEROM) -op create -hdr 0x08000000 -romBlock 0x08000000 0x10000 -chChecksum -chName "CS1 Module B" -chManuf "FujiNet" -chVersion 0x0100 -chZCrDate -romName "FN CS1 Install B" -autoSize -tokStr HsAT 330 -romDB $< -o $@.unverified
	python3 $(TOOLS)/verify-cs1setup-rom.py $@.unverified --prc $< --manifest build/rom-manifest.json
	cp $@.unverified $@
check:
	mkdir -p build-host
	cc -DRP_HOST -std=c99 -O2 -Wall -Wextra -Werror -I. tests/test_coexist.c policy.c -o build-host/test_policy
	./build-host/test_policy build-host/codec.golden
