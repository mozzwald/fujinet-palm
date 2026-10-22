# Visor ROM Check — RAM diagnostic

Current build **0.5.0** adds64-word ROM prefix/0000/FFFF diagnostics. The normal
Run/Pass/Fail functions and all existing session history remain supported. New
ROM64/Zero/Ones buttons use the same guarded one-word-per-tick reader, unchanged
wait states and persistent result collection. The matching separate RP latency
firmware must be selected/armed by the host first; see the latency section in
[PRISM_AUTOMATION.md](../../../tools/PRISM_AUTOMATION.md). A short pattern PASS
is not full-ROM verification. Upgrade an existing session's PRC only; retain its
original control PDB/token, sequence and records.


Previous build: **0.4.1 automated collection**, normal bus timing only. Install the
PRC together with its generated VisorCheckControl PDB. Run performs the existing
normal read; Pass/Fail generate synthetic results without bus access. All results
are appended to VisorCheckResults, including full captures and every mismatch.
See [the automation procedure](../../../tools/PRISM_AUTOMATION.md) for session
setup, installation, transport validation and the single-command collector.
Saved PASS and FAIL results play confirmation/error sounds, honoring system
volume. Always use **serial palm-sync** for Prism HotSync as documented in the
automation procedure.
No RP firmware update is required. Use `tools/test-prism-automation.sh` for the
offline checks. The sections below describe the retained 0.2/0.3 diagnostics and
fallback artifacts; 390ns is not available in the current collection build.

Separate application `Visor ROM Check`, creator FNVC. It coexists with the
FNVB Visor Bus Test RAM app and the original module ROM. Do not put this checker
in the image it checks: expected size/CRC come from the independently verified
`../visorbus/build/visorbus.rom` and its manifest.

Build from the repository root with `./tools/build-springboard.sh`; host policy
checks are `make -C palm/apps/visorcheck check`. The generated header rejects a
ROM/hash/size mismatch against the manifest. The original ROM remains unchanged.

Install `visorcheck.prc` through HotSync. Flash the updated RP2350 experimental
ROM responder, send `b` on USB and confirm `stopped=0 arm_pending=0 limit=3624`.
Tap **Normal**, confirm, then wait about 40 seconds for **Full ROM CRC PASS**.
Expected read count is 7248 bytes (hex 00001C50), CRC32 484E213E. Confirm RP stopped
with 3624 requests/posts/completions. Keep module seated; no Pulse, reset or
reinsertion. This step verifies bus readback, not ROM recognition or execution.

Each event-loop step performs one 16-bit volatile read through the existing
Handspring removal-recovery mechanism. Slot guards and mapping are rechecked
before every word. One OS tick separates steps, preserving the widely spaced
read restriction. CRC consumes the high byte then the low byte. The event loop
supports Cancel and refreshes auto-off timing. On Cancel, app exit or error,
send RP `x` and confirm `stopped=1`; the checker has no USB connection to the RP.

Keep Visor Bus Test in RAM until readback passes. Before a later module-ROM
launcher test, remove its RAM copy to avoid shadowing; retain Visor ROM Check.
The continuous responder, OS recognition, ROM launch and physical timing remain
separate milestones. Do not attempt them using the bounded responder.

## Mismatch diagnostics (0.2)

The checker embeds an independent RAM reference copy of the verified image.
Before bus reads, its RAM CRC must equal the manifest CRC. Actual Bus CRC is
still calculated exclusively from the volatile CS0 words; the reference is only
used to count mismatches and retain the first eight offset/expected/actual
triples. A wrong reference cannot substitute for received bytes. Tap Details
after a run and photograph both the CRC and mismatch pages. No RP firmware
change is required for0.2; use the same bounded b mode.

## Optional Prism timing experiment (0.3)

**Normal** retains the existing access time; **390ns** requests 390 ns through
HsCardAttrSet for this prototype on the Prism. Both use the identical guarded,
one-word-per-tick reader and reference image. The setter affects CS0 and CS1.
There is no RP firmware change. Arm RP `b` freshly before each full-image run.
Keep the same prototype seated throughout; do not test other Springboard cards
or run a card driver concurrently. The API does not provide a unique physical
card identity, so unnoticed removal/replacement cannot be detected reliably.

After completion, tap **Details** once for mismatches, twice for timing, and
again for the CRC summary. Timing values are decimal nanoseconds; zero means
not recorded/not applicable. Record Original, Request, Applied and Restored.
Normal records Original/Applied but makes no timing writes. Slower mode refuses
an original setting already at/above 390 ns and requires applied readback at
least 390 ns. A returned setting is not proof of a measured bus-cycle duration.

Restoration and verification run after completion (including CRC failure),
Cancel, app exit, read error and a setter/readback failure during setup. Existing
slot guards require a removable, physically present, OS-registered card, released
RESET and an unchanged valid mapping. SwInstalled means OS registration, not
exclusive ownership by this application. If removal, loss of registration,
reset or remapping is observed, no restoration write is attempted: tests block.

A saved FNVC preference (ID 1, version 1) records the original timing before the
first change, and is cleared only after verified restoration. A pending or
invalid journal blocks further tests across app launches. Do not delete this
record or bypass the warning to continue testing. Stop RP with `x`, preserve the
CRC/mismatch/timing pages and seek recovery instructions. A crash cannot run
cleanup; the journal records that uncertainty rather than claiming restoration.
The checker cannot stop the RP over USB: after cancellation or interruption,
send `x` and confirm `stopped=1` before rearming.

Host checks cover cleanup after pass/fail/cancel/exit/read error, setters that
change timing despite an error, readback rejection, failed restoration/journaling,
and observed slot changes. They do not replace physical timing qualification.
