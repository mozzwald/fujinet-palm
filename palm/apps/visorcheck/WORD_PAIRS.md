# Two-word RAM spacing diagnostic

`ROM Word Pairs` is a separate RAM application (creator FNVE), control database
`ROMWordControl`, result database `ROMWordResults`, preference3 under FNVE.
It preserves the FNVC/FNVD checkers, their databases and the installed ROM.
It has no reset, recognition, full-ROM or ROM-execution UI and does not request
reset after installation. Always use serial palm-sync with the visor driver.

Each run is four pairs. An entry is selected **before** external access:

```
movea.l 4(sp),a0
move.w (a0),d0
; N copies of NOP, with N in 0,1,2,4,8,16,32,64
move.w 2(a0),d1
swap d0
move.w d1,d0
rts
```

There are no calls, branches, stores or traps between the two word loads.
The first half is packed into the high16 bits after both reads finish. There is
one OS tick between complete pairs, with the existing read-only presence/reset/
mapping guards and error catch. No wait-state setters are used. Returned values
and both-half mismatch counts are retained, including partial results; PASS/FAIL
completion produces a saved-result tone.

NOP count is instruction count, **not measured time**. Separate MOVE.W instructions
can already have a larger gap than the two bus cycles of one MOVE.L at zero NOPs.
Measure each bus spacing; do not infer it from an instruction-cycle table.

Build and verify in the existing toolchain:

```
PALM_IMAGE=fujinet-palm-springboard-tools docker/palm-build.sh make -C palm/apps/visorcheck verify-wordpairs
```

Output `build/wordpair/wordpair.prc` has a distinct identity. The verifier checks
every linked entry's exact opcodes, NOP count, returned-value assembly and unique
packaged bytes. Separate host codec/parser tests validate spacing metadata,
database isolation and all four returned values. Physical timing remains a
separate gate.

Example host sequence, from repository root:

```
python3 tools/wordpair_collect.py setup SESSION --rom ROM --prc palm/apps/visorcheck/build/wordpair/wordpair.prc
python3 tools/wordpair_collect.py install SESSION
python3 tools/wordpair_collect.py probe SESSION
python3 tools/wordpair_collect.py run SESSION --pattern signature --count 4 --nops 0
```

Install and probe each require a cued HotSync; the probe checks the control PDB
round-trip exactly. Do not start reads before instruments and RP are armed.
Use the app's NOPs button to match the host's requested value (display is hex).
Tap Run pair/Read exactly once, wait for the tone, then HotSync. The collector
does not poll/log UART during access. It stops RP after HotSync starts and saves
final counters before retrieving PDBs. Pattern/count/NOP mismatches are rejected
with raw evidence retained. Larger runs are unavailable in this diagnostic.

Record `active_firmware_sha256` only after verified installation. Initial spacing
runs use the unchanged drive-marker firmware. The optional `--marker preload`
is explicit opt-in for a separately verified reply-preload marker image; the
default refuses that banner. The same opt-in is available in `paired_collect.py`
for controlled comparison against the original MOVE.L diagnostic. Marker type
is saved in RP config and reports; it never proves actual data validity or Hi-Z.

Results use the existing160-byte field layout in a distinct `VPW1/version1`
envelope: app version0800, two load opcodes per returned value, one tick between
pairs, mode1 and exact NOP count in the final two metadata words. Legacy `VPL1`
records remain strict and cannot be mistaken for this diagnostic.
