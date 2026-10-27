# Phase 3 — minimum Prism access-time experiment

Standalone `appl`/`FNTM` RAM utility, **Prism Timing P3**. Install by serial
palm-sync; never place it in ROM. Opening performs read-only queries and logs
RAM PC/code/heap proof, PalmOS/Handspring/OEM/CPU features, module generation,
API base/size, and current access time. Card comes from the successful FNRA
installation receipt; no hardcoded chip-select address or slot is used.

Physical budget: one Baseline and one Slower press, each two explicit MOVE.W
reads at offsets8/10 plus one aligned MOVE.L at8 (four halfword bus cycles).
Expect FEED,BEEF,FEEDBEEF. No full-ROM transfer, timing sweep, reinsertion or RP
firmware change. Keep the app open between these two captures.

Slower is Prism bench-only and requires original330ns. It requests360ns through
HsCardAttrSet attr5, verifies readback>=360, makes the same reads and restores330
immediately with readback.360 is one nominal VZ30ns step within the documented
0..13-state envelope, not an SDK enum, universal supported table, or maximum.
Actual rounding and physical timing must be measured. Getter/setter data is
UInt32. CS0/CS1 share this setting; no CS1 access is performed.

Before a setter: persist CRC-protected pending original/generation/map journal
and verify persistence. Cleanup attempts restoration even after setter failure
because failure may follow a partial change. Observed generation/map/install/
reset identity failure latches and prevents writing a replacement module.
Failure retains a blocked pending journal; Restore is explicit and guarded.
Opening/relaunch never automatically writes a setting. No raw controller writes.
Queries/read guards and storage are outside the assembly read sequence. IRQs
remain enabled. The existing Handspring removal recovery chain surrounds reads;
it does not certify arbitrary electrical bus-error safety. No callbacks/hooks
are retained after the operation.

`FN Timing P3 Log` DATA/FNTM holds at most32 records. Each record is192bytes:
48 big-endian words named in core.h/timing_collect.py, final CRC32 covers188
bytes. Includes original/request/applied/restored, each error, exact word/long
results and RAM-code metadata. Flags pending1,RAM2,ran4,restoreverified8.
Successful run is stage7/error0/flags14; pending crash recovery uses separate
saved preference FNTM/id2/version1. FNRA receipt is read-only.

```sh
# Existing authorized Palm toolchain image, no network needed.
docker run --rm --network none -v "$PWD:/src" -w /src/palm/apps/timing \
  fujinet-palm-springboard-tools make
make -C palm/apps/timing check
```

Export .text/nm/disassembly with the same m68k-palmos tools and run
`tools/verify-timing-app.py`. This checks exact linked code/1, PC-relative marker
and probe opcodes, including one actual MOVE.L rather than two MOVE.W loads.
`tools/timing_collect.py fetch --static STATIC_JSON --output FRESH_DIR` retrieves
results using serial palm-sync only; it never connects to RP2350 or changes
settings. The host checks record CRC, RAM evidence, data, restoration, and
baseline-before-slower identity correlation. Physical timing stays a separately
reviewed result. The utility permits no other requested slower settings.

Version2 capture correction: Slower pauses after accepted360ns and before the
read sequence. Cancel restores330; Continue rechecks the setting before reads.
Capture may include incidental OS ROM reads while paused; do not call those
the explicit probe. A verified matching v1 baseline receipt can be reused, so
installing the update does not require repeating Baseline. Pass both manifests
to the collector via --static V2 --previous-static V1.
