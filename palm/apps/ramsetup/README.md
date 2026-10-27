# Phase 2 RAM installation candidate

Build-only experiment: **do not HotSync this Setup PRC and do not automatically flash**.
ROM discovery must cause PalmOS to copy `FN RAM Setup P2` (`HsSU`/`HsCd`) into
card-0 RAM and deliver install command `0x8000`. The handler copies that RAM
Setup database (which PalmOS may rename) to a temporary database, verifies it, then publishes the clone as
`FujiNet RAM P2` (`appl`/`FNRA`). Both identities share the same code, but normal
launch only accepts the verified RAM runtime identity. There is no CS1 access.

The install/remove entry points have no globals or A5 dependency. Install logs
receipts and reuses an existing identical runtime; foreign names and mismatched
versions/code fail without replacing the existing app. Removal records inactive
status, retains the runtime, and leaves original Setup deletion to PalmOS.

## Build and host verification

From `fujinet-palm`, using the existing Springboard toolchain Docker image:

```sh
docker run --rm --network none -v "$PWD:/src" -w /src/palm/apps/ramsetup \
  fujinet-palm-springboard-tools sh -c '
    make rom &&
    m68k-palmos-objdump -d build/ramsetup > build/ramsetup.dis &&
    m68k-palmos-nm -n build/ramsetup > build/ramsetup.nm &&
    m68k-palmos-objcopy -O binary -j .text build/ramsetup build/ramsetup.text &&
    m68k-palmos-objdump -h build/ramsetup > build/ramsetup.sections'
make -C palm/apps/ramsetup check
python3 tools/verify-ramsetup-code.py \
  --prc palm/apps/ramsetup/build/ramsetup.prc \
  --text palm/apps/ramsetup/build/ramsetup.text \
  --nm palm/apps/ramsetup/build/ramsetup.nm \
  --dis palm/apps/ramsetup/build/ramsetup.dis \
  --sections palm/apps/ramsetup/build/ramsetup.sections \
  --output palm/apps/ramsetup/build-host/static.json
```

The existing `tools/makerom/build/Palm-MakeROM` must already be built. `make rom`
independently verifies metadata, card CRC, token and every PRC resource before
publishing the ROM. The code verifier independently compares linked text with
`code/1`, checks reachable instructions for A5 use, and checks the exact
PC-relative marker and trap-free RAM loop. Zero `.data`/`.bss` is required.
The eight zero bytes after the identity resource are data padding that preserves
the frozen responder lookup table's modulo-16 SRAM alignment with this ROM.

Build the UF2 from the separate, unchanged persistent-reply target using
`fujinet-nio/bridges/rp2350-springboard/scripts/build-persistent-reply.sh` (invoke
with `bash`, three absolute ROM/manifest/output paths). Then run
`tools/verify-ramsetup-firmware.py --help` and supply ELF, BIN, UF2, ROM and
preserved baseline ELF/UF2. It checks exact embedded ROM and ELF/BIN/UF2
agreement, frozen ARM kernel, PIO instructions/template, disarmed startup and
absence of experimental CS1 symbols. One relocated PIO template pointer is
explicitly allowed and reported; no instruction differences are allowed.

After building Palm and RP artifacts, run the complete host suite:

```sh
python3 -m unittest discover -s tools/tests -v
```

`test_ramsetup.py` requires the generated Palm build, `build-host/codec.golden`,
the RP build at `/tmp/visor-ramsetup-p2-firmware`, and the adjacent private plan's
frozen baseline artifacts. These are integration fixtures, not device tests.

## Proof and collection

`ramproof.S` returns its own PC with a PC-relative LEA. Runtime records also
include actual current application DB ID, resource base/size/CRC, card number,
heap ID/flags, and membership in the PalmOS card-0 RAM heap list. The host checks
these against linked code offsets and CRC. The RAM-loop button executes a
verified trap-free instruction loop and records `0x52504C50`. Interrupts remain
enabled; occasional OS ROM reads are permitted.

`FN RAM Proof` is an append-only record PDB (`DATA`/`FNRA`), bounded to 256
records. Each record is 128 bytes, 32 big-endian words; field order is defined in
`policy.h` and `tools/ramsetup_results.py`. Preferences contain the latest
receipt. The host correlates install and runtime events by generation and both
DB IDs. PDB evidence cannot by itself qualify the scope/lifecycle checks.

Manual-only helpers:

- `tools/ramsetup_service.py`: verify bundle, refuse a running responder, prime
  a freshly flashed candidate **while Prism reset is physically held**, then
  ask for reset release. It closes the console after ACTIVE, leaving autonomous
  ROM service running. It never flashes, resets or automatically stops service.
- `tools/ramsetup_results.py fetch`: retrieve only `FN RAM Proof` using the
  established serial palm-sync transport; no uploads, RP connection or reset.

Full frozen candidate, hashes, manual test and rollback procedure:
`handspring-visor-plan/docs/visor/RAM_INSTALLATION_PROOF.md` in the adjacent
private planning repository. **Hardware installation, RAM execution and
lifecycle results remain pending.**


## Observed PalmOS rename — P2.20261026.2

The Prism creates its RAM Setup copy as `HsCardSetup.787D.9362`, not the ROM
name. The original source-name requirement rejected this legitimate copy with
`RP_BAD_SOURCE` before cloning. All seven downloaded resources matched exactly.
The fix accepts the OS-owned source name while requiring the SDK-supplied source
ID, executing PC within its card-0 RAM resource, heap, type, creator and version
checks. Temporary/runtime names remain exact; collision/rollback policy is unchanged.
Failed source validation now records inspected PC/code/heap fields before returning.

The native regression fails with the archived original policy and passes with
the correction. Wrong creator and wrong executing PC still fail before copying.
For firmware fixture tests at another build path, set `RAMSETUP_FIRMWARE_DIR`.
The fixed candidate is archived separately under `2026-10-26-p2-002`; the original
`p2-001` artifacts and October 25 baseline remain untouched. Hardware retest is pending.
