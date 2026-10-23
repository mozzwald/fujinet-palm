# Prism automated result collection

This is bring-up tooling, not a new FujiBus feature. `prism_collect.py` uses
Python's standard library, serial palm-sync, and the existing RP2350 USB CDC
debug console. Legacy pilot-link support remains available explicitly. The RP firmware, ROM image and bus timing are unchanged.
Visor ROM Check **0.4.x or0.5.0** must be installed with its session control database.
It offers **Run** (normal timing), **Pass** and **Fail** (synthetic, no bus access),
Cancel and Details. The 390ns experiment is unavailable in this collection build;
the 0.3 fallback remains separate. Do not rebuild or flash the module ROM.

Version0.4.1 adds completion sounds: confirmation for a saved PASS and error for
FAIL (or a result-save failure). Sounds play after reading and persistence, never
in the bus callback, and honor system sound settings. Cancel/ordinary exit does
not replay a completed-result sound. Result records identify0x401; the collector
accepts both0x400 and0x401 so existing session history remains valid.

For the prepared prism-20261021 session,0.4.1 is a PRC-only update; retain its
original control PDB, token, sequence and saved results. The session's initial
PRC hash still identifies the original0.4 bundle, while app-update-0.4.1/UPDATE.json
records the separately installed maintenance PRC and source hashes. Do not use
install to reapply the original bundle/control for this update.

## Gates and preparation

Host tests and a PRC build are software evidence only. Hardware gates, in order:

1. Install PRC + control PDB, then fetch that small PDB intact using serial palm-sync.
2. Retrieve synthetic Pass, then synthetic Fail (every word mismatches).
3. Repeat the complete collection at least five times. Verify missed HotSync,
   disconnect/retry and prior-record retention on the actual device.
4. Only then use normal ROM Run. No timing changes or ROM launch in this phase.

The user's 2026-10-21 remote-work request authorizes preparing these components
and offline fixtures before the physical transfer gate can be demonstrated.
It does not mark any hardware gate complete.

### Full ROM with the optional RX-first responder

After the separate RX-first firmware has passed its short real-ROM check,
`run --kind rom --latency-full-rom` selects its explicit full-image arm. This
requires firmware announcing `ENGINE=rx-first-v2 full_words=N` with N exactly
matching the session's full ROM size in words. The collector still verifies the
ROM CRC/identity, CPU-ROM response and marker, then sends `B` and checks the
fresh run and exact budget before cueing the existing Palm **Run**, then **Read**.
The current ROM is7248 bytes/3624words. No PRC update or session/control rewrite
is needed; the original guarded, one-tick reader and normal wait settings apply.

```sh
./tools/prism_collect.py run /path/to/session --kind rom --latency-full-rom
```

The flag is required explicitly and is valid only for kind`rom` and CPU response.
Unsupported engines, including RX-firstv1's64-word image, are rejected before
reserving/arming a Palm run. There is no automatic fallback. Full-engine metadata
is retained in pending state and the paired report, including across a HotSync
retry. Both armed and final budgets and all counters must match the full image.
Missing engine/config evidence cannot become a legacy full-ROM PASS.

Without this flag the original full-ROM firmware identity/`b` procedure remains
unchanged. Short diagnostics continue to use their64-word`b` arm. These firmware
choices do not change the ROM content, Palm transfer protocol or FujiBus protocol.
Readback success does not qualify sampling margin, release, rapid cycles or ROM
execution. Scope trigger/no-trigger status must still be preserved independently.

From the fujinet-palm root:

```sh
./tools/prism_collect.py doctor
./tools/prism_collect.py setup /absolute/path/to/private/prism-session
./tools/prism_collect.py install /absolute/path/to/private/prism-session --transport palm-sync
# Exit the checker to the launcher if replacing an installed app, then HotSync.
./tools/prism_collect.py probe /absolute/path/to/private/prism-session --transport palm-sync
# Press HotSync again; the tool compares the retrieved control record byte-for-byte.
```

`setup` creates an immutable reference ROM, PRC copy, session.json, a small
VisorCheckControl.pdb, and offline pass/fail fixtures. It refuses an existing
folder. Keep the session directory outside public code repos. Do not manually
edit session.json or reinstall its control PDB midway through a pending run.
`doctor` inventories devices without opening them; setup also accesses no device.

The transient Prism USB device must be Handspring **082d:0100**. The tool waits
for the HotSync port (second serial port, historically ttyUSB1), not the generic
port. It never unloads the kernel visor driver or uses raw USB. Install/probe
have a ten-minute button window, adjustable with `--timeout`.

Standing Prism instruction (2026-10-21): **always use serial palm-sync** for
installation and retrieval. Pilot-link failed to bind the HotSync port; serial
palm-sync passed the exact control round-trip and subsequent collections.
Select it explicitly for every new session; run/collect inherit that selection.
Do not automatically retry pilot-link or switch transports after a failure.
Retain transport.log and retry collection with the existing saved result:


```sh
./tools/prism_collect.py install /path/to/session --transport palm-sync
./tools/prism_collect.py probe /path/to/session --transport palm-sync
```

A successful probe records the selected transport; later run/collect commands
use it. Do not infer successful retrieval from a process exit status. The
existing palm-sync server can swallow transfer errors, so every fetched PDB is
validated independently. No fallback is assumed proven before its probe passes.

## Synthetic collection first

```sh
./tools/prism_collect.py run /path/to/session --kind synthetic-pass
# At the cue: open Visor ROM Check 0.4 and tap Pass. After it saves, press HotSync.
./tools/prism_collect.py run /path/to/session --kind synthetic-fail
# At the cue: tap Fail, wait for save, then press HotSync.
```

These tests do not open the RP console or access the Springboard bus. Keep the
RP stopped before validation. Synthetic FAIL is expected: it flips the low bit
of every received word, exercising all 3624 mismatches rather than only eight.
Reports explicitly label synthetic tests and cannot qualify ROM hardware.

## One-command ROM collection after the gates pass

Close minicom and all other RP console consumers first; leave module/probes
seated and the scope stopped. This uses the existing bounded `b` responder and
one-tick-spaced reads. There is no firmware flash, hardware reset or timing write.

```sh
./tools/prism_collect.py run /path/to/session
```

The command identifies the RP USB CDC device (2e8a:0009; specify `--rp /dev/ttyACM0`
if necessary), verifies stop/readiness and the firmware banner's ROM size/CRC,
then sends b and confirms a fresh RP run, zero counters and the exact word budget.
Only when cued, tap **Run**, confirm **Read**, wait for the result to save, then
press HotSync. The host captures final counters and anomalies before fetching.
RP identification briefly lowers/raises CDC DTR to request the existing debug
banner; this is not a chip reset. Firmware that cannot supply the expected banner
is refused. DTR/serial readiness still needs target validation.

Each Palm result uses the installed 128-bit host session token plus a durable
monotonic sequence. The host expects exactly the next sequence and selected
kind. It records RP arming/final diagnostics under that same identity. Extra,
stale or missing Palm runs, unexpected RP counters/run/budget, CRC disagreement,
or malformed data cannot produce PASS. Do not tap extra test buttons between
host runs. No filename/timestamp/count-only correlation is used.

Output under run-NNNNNN-<attempt> includes raw fetched PDB(s), transfer commands/logs,
RP console/arming/final/stop records for bus tests, and report.json/report.md.
The report includes every mismatch and timing diagnostics. Exit codes: 0 PASS,
1 valid FAIL/INCOMPLETE, 2 COLLECTION ERROR, 130 user interruption. A synthetic
FAIL therefore returns 1 even when its infrastructure test worked correctly.

## Retention and recovery

Palm VisorCheckResults is append-only: one STARTED marker before any read, then
one final record after reading. Captured words are buffered in RAM; there are no
Data Manager allocations/writes in the read callback. A crash leaves STARTED,
which is INCOMPLETE, not PASS. Cancellation/ordinary app exit save partial data.
Low memory/storage or a failed write leaves earlier runs intact; the UI reports
RESULT NOT SAVED and the host refuses to infer success. The on-screen mismatch
page still shows only the first eight; the database/report contains all of them.

Missed or failed HotSync leaves the host pending run and Palm records intact:

```sh
./tools/prism_collect.py collect /path/to/session
# Retry HotSync; do not tap Run again.
```

There is no automatic deletion on either side, even after retrieval. Host evidence
is retained, and only verified collection advances its next expected sequence.
If a run is still executing after cancellation, timeout or interruption, the
controller sends x and requires stopped=1 where communication remains available.
If it cannot verify stopping, do not rearm or touch the module; investigate first.
A cancelled checker cannot directly stop the RP over USB: host completion timeout
(default180s) may be needed, followed by collect and a fresh HotSync.

Do not clear a pending journal to force another run. A crash between reserving a
Palm sequence and writing STARTED can leave a sequence gap. Preserve the session
and retrieve/decode the existing PDB for investigation; create and install a new
session token to recover unambiguously. New sessions do not delete old records.
The tool deliberately refuses to relabel an unexpected run as the requested one.

The older timing-restoration journal is still honored. A pending/malformed FNVC
preference ID1 blocks all tests; do not erase it to bypass a restore warning.
The collection sequence uses a separate preference ID2. Keep module seated;
unnoticed removal/replacement at the same mapping cannot be reliably identified.
No responder timing/reliability or ROM execution claims follow from this tooling.

## Validation and format

```sh
make -C palm/apps/visorcheck check
python3 -m unittest discover -s tools/tests -p test_prism_collect.py -v
PALM_IMAGE=fujinet-palm-springboard-tools ./docker/palm-build.sh make -C palm/apps/visorcheck
```

Tests include the real C serializer decoded by Python, the real storage code
against a Palm Data Manager mock, malformed/truncated records, every mismatch,
partial/crashed runs, duplicate/stale IDs, failed transfers and five simulated
full workflows. Simulated workflows are not the five physical acceptance runs.

PDB: creator FNVC; types VCTL (VisorCheckControl) and VRSL (VisorCheckResults).
All application integers are unsigned big-endian, independently of C layout.
Control record: VCC1, version u32, session16, expected-size u32, expected-CRC u32,
ROM SHA25632, CRC32 of preceding64 bytes; total68 bytes.
Result record: VCR1 + version u32; session16 at8; ROM SHA25632 at24;
twenty u32 fields at56 (sequence, kind, status, expected size/CRC, RAM-reference
CRC, completed bytes, bus CRC, mismatch count, guard reason/error, original/
requested/applied/restored ns, setup/restore errors, Palm start/end seconds,
application version0x400/0x401). Total-length u32 at136, header-length144 at140.
After header: completed raw bytes, then mismatch-count tuples (offset u32,
expected u16, actual u16), then CRC32 of all preceding record bytes.
Kinds:1 ROM,2 synthetic-pass,3 synthetic-fail. Status:0 STARTED,1 PASS,2 FAIL,
3 CANCELLED,4 ERROR. STARTED has no captured bytes/end time. One final maximum
per run. With the current image a worst-case final record is36388 bytes. The
format caps images at12000 bytes to stay below Palm's single-record size limit;
larger images require a reviewed chunked format, not silent truncation.

A busy console or failed identity preflight does not consume a pending Palm run;
its attempt logs are kept separately. Once an arm command may have been sent,
the pending run remains until validated retrieval or explicit session recovery.

## Latency/constant diagnostic (checker0.5.0)

The separate RP `springboard_rom_latency.uf2` uses the same150MHz, GPIO/pad setup
and guard/release program. It supports CPU ROM-prefix, CPU constant and PIO-only
constant responses. PIO-only means no CPU-generated TX reply; CPU still counts,
drains RX and acknowledges the already-released IRQ park. Marker modes select
existing-instruction side-set for request, reply, preload or drive; they do not
add PIO cycles. See the bridge's LATENCY_TEST.md for exact stages and safety
limits, including the known rapid-deselect blind interval.

Use an existing verified session, **without setup or control reinstall**. First
install only the new PRC with serial palm-sync (close the app to launcher), then
require the0.5.0 title. The original ROM SHA/config CRC remains the session
identity. Result kinds4/5/6 select a128-byte zero/ones/ROM-prefix payload; their
expected CRC and mismatch values are independently derived by the collector.
Reference CRC/SHA still identify the original ROM. STARTED and final records
carry the same test metadata. Wire envelope remainsVCR1; app_version0x500.
Old0x400/0x401 records and earlier sequence numbers remain valid.

Individual, host-cued commands from fujinet-palm (never run as an unattended batch):

```sh
./tools/prism_collect.py run /path/to/session --kind rom-prefix
./tools/prism_collect.py run /path/to/session --kind constant-zero --responder cpu
./tools/prism_collect.py run /path/to/session --kind constant-zero --responder pio
./tools/prism_collect.py run /path/to/session --kind constant-ones --responder cpu
./tools/prism_collect.py run /path/to/session --kind constant-ones --responder pio
./tools/prism_collect.py run /path/to/session --kind rom-prefix --marker request
./tools/prism_collect.py run /path/to/session --kind rom-prefix --marker reply
./tools/prism_collect.py run /path/to/session --kind rom-prefix --marker preload
```

Each reads64 guarded words at offsets0..126; at the cue use ROM64,Zero orOnes,
then Read. Wait tone/save and serial HotSync. The collector rejects baseline
firmware for these tests and rejects latency firmware for ordinary full-ROM run.
It selects and verifies response/marker/64-word budget before reserving/arming,
and saves rp-config.json beside RP association/counters. PIO response requires
posted0,CPU response posted64; both require requested/completed/enabled64 and
zero anomalies. Missing/wrong mode metadata cannot produce a complete PASS.
Transfer failure keeps the sequence pending; collect retries without a new read.

Select only one experimental variable at a time. Within the same diagnostic
binary/probe pair, compare CPU0000 -> PIO0000 -> CPU0000, then change payload
and compare CPUFFFF -> PIOFFFF -> CPUFFFF. Keep markerdrive throughout those
comparisons. Profile request/reply/preload separately with fixed CPU-ROM prefix.
No clock, drive-strength, synchronization bypass or Palm timing changes.
