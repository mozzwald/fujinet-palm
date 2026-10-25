# Paired 68000 longword reads

The RAM checker has a **Pairs** page, separate from the original ROM/pattern
tests. Install the `FNVD` distinct-reference build, displayed as **ROM Check R3
ref** in the launcher. `FNVC` and its existing control/results stay intact.
No paired-read operation calls a slot attribute setter, resets PalmOS, attempts
recognition, or executes any code in the module ROM.

Build and verify with the existing Palm toolchain:

```
make -C palm/apps/visorcheck verify-pairs
make -C palm/apps/visorcheck check
python3 -m unittest discover -s tools/tests
```

`verify-pairs` checks both linked disassembly and the actual packaged PRC.
The external assembly function is exactly:

```
movea.l 4(sp),a0
move.l  (a0),d0
rts
```

The bytes are `206f0004 2010 4e75`. The caller stores the whole returned D0;
there is no C replacement with two word loads. Physical timing must still be
measured. A tick separates *longword instructions*, isolating the two fast
halfword cycles within each instruction from later pairs. Mapping/reset/removal
guards run before each pair. The paired mode permits a physically installed,
mapped module whose ROM software was not recognized; it does not require or
fabricate SwInstalled. The original registered-reader guard is unchanged. The established module-removal catch list surrounds
the assembly call; an error preserves the previously completed values. Cancel
and application exit operate between pairs. Allocation and PDB writes occur
outside the individual ROM accesses. Sounds occur after final persistence.

Counts are 4, 100 and 10,000. The first pattern reads the aligned longword at
ROM offset8, expected `FEEDBEEF`. The second chooses existing 4-byte-aligned ROM
data with the greatest halfword bit contrast (earliest maximum). In the current
distinct image that is offset`0444`, expected`A1745C8F`; the halves differ in14
bits. Both choices use the unchanged installed ROM and slot timing.

## Collection

`tools/paired_collect.py` reuses `prism_collect`'s verified serial palm-sync
transport, PDB envelope validation, USB identity/exclusive serial preflight,
continuous RP arming and stop diagnostics. It does not poll UART between arming
and the user's post-test HotSync. The earlier continuous keepalive helper must
be stopped cleanly before this collector opens the RP console.

```
python3 tools/paired_collect.py setup SESSION --rom VERIFIED_ROM --prc palm/apps/visorcheck/romprobe-checker.prc
python3 tools/paired_collect.py install SESSION
python3 tools/paired_collect.py probe SESSION
python3 tools/paired_collect.py run SESSION --count 4 --pattern signature
# Only after the four-pair payload AND exact counter checks pass:
python3 tools/paired_collect.py run SESSION --count 100 --pattern signature
python3 tools/paired_collect.py run SESSION --count 10000 --pattern signature
```

Install and probe each use a manual serial HotSync. Probe must round-trip the
control record byte-for-byte. Run arms the current continuous responder with
limit0; no finite transaction budget applies. After the host and instruments
are ready, select the matching Count/Pattern on the Pairs page, tap Run pair and
Read **once**, wait for its saved-result tone, then HotSync. Collection stops
the RP, saves final counters and automatically retrieves the PDB. Failed or
missing RP responses remain diagnostic evidence and do not discard Palm bytes.
If transfer times out, use `collect SESSION`; the pending run is preserved.
Reinstalling or starting another host run while a run is pending is refused.

The 100/10,000 gates apply separately to each pattern and require the exact
expected `2*N` requests, posted replies, completed transactions and enable
commands, with zero cancels/empty endings. Do not escalate after a failure.
The four-pair contrasting control may still be useful to localize a failed
signature test. Never silently overwrite the session or mix extra user taps.

## PDB format

`ROMPairControl`, type`VCTL`, creator`FNVD`, holds the existing68-byte VCC1
reference/session configuration. `ROMPairResults`, type`VRSL`, creator`FNVD`,
is append-only. A separate preference ID3 owns its sequence, leaving existing
checker sequence/preferences untouched.

Each run appends a STARTED marker before the first ROM read and then one final
record. VPL1 records have a160-byte header, followed by every returned longword
as explicit big-endian bytes, followed by a record CRC32. Header byte8 holds
the16-byte session, byte24 the32-byte ROM SHA256. The26 big-endian UInt32 fields
at byte56 are, in order:

```
sequence status offset expected requested completed first_bad second_bad
long_bad base size access_before access_after reason error started ended
payload_crc32 reference_crc32 app_version(0700) record_length header_length(160)
opcodes_per_value(1) interpair_ticks(1) reserved0 reserved0
```

The largest record is40,164 bytes, including all10,000 values. Expected data is
in the header and independently verified ROM. Both halves and the full actual
value of every mismatch are therefore preserved without a truncated error list.
The host recomputes both halfword counts and all full-value mismatches, verifies
PASS/FAIL against the bytes, checks STARTED/final metadata, and checks that prior
downloaded records remain byte-identical. Reports include all values and every
mismatch (`report.json`, `mismatches.csv`), plus a concise table and examples.

## Physical comparison

Keep CH1 GPIO39, CH2 CS0 and existing analyzer wires fixed. Trigger the analyzer
on CS0 falling, **not RESET**; these tests do not reset the module or Prism.
Use the established scope settings/cursor export. At6MHz, the analyzer cannot
resolve every40–48ns inactive gap, so address transitions and the scope are
needed to identify merged pairs. A5.333-second window covers the small test,
but cannot be claimed to cover the entire10,000-pair run at tick spacing.

`requests` counts CPU-popped snapshots, not all PIO request creations. A deficit
does not alone distinguish absent RX pushes from old snapshots drained during
rearm. Posted replies do not prove timely bus presentation. GPIO39 and
`attempted` indicate enable commands; they do not prove valid data, physical
Hi-Z or the CPU sampling instant. Classify the failing stage only to the degree
supported by the pair payload, counters and scope trace. Do not optimize the
responder or attempt recognition until these paired reads are understood.

The separate FNVE [two-MOVE.W diagnostic](WORD_PAIRS.md) varies instruction
spacing without replacing this MOVE.L test. Zero-NOP and one-NOP controls each
passed four signature pairs on the October24 PIO-rearm candidate, while MOVE.L
still failed. The first scoped gaps were536–544ns/608–616ns versus36–44ns.
These are observed controls, not a qualified minimum reliability threshold.
Use the explicit marker profile: drive is an enable attempt, preload marks
reply consumption. Version2 `attempted` counts unmasked-enable outcomes through
the next instruction; a brief enable on a cancellation boundary may instead
count as dropped. Neither marker alone proves data validity or Hi-Z.

The separate persistent Core1 experiment identifies as continuous version3.
Use `paired_collect.py run ... --pattern signature --count 4 --engine persistent
--marker preload`. The collector rejects version3 without that explicit engine
and rejects version2 when the persistent engine was requested. Existing RAM
PRC, MOVE.L instruction, PDB format, serial palm-sync and escalation gates are
unchanged. A new firmware hash must pass the four-pair gate before larger counts.
