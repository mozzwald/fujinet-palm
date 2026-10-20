# Visor Bus Test — Palm OS 3.5 / Springboard test ROM

Separate bring-up app using the existing `hello` application/event loop and Palm
build container. It does not link the older FujiNet serial protocol or NetLib
shim and does not require `fujinet-lib-palmos`.

From the repository root:

```sh
./tools/build-springboard.sh
```

This reuses/builds `fujinet-palm-toolchain`, extends it with a 32-bit host C++
compiler, builds imported MakeROM source, and creates:

- `palm/apps/visorbus/visorbus.prc` — RAM-installable test app.
- `palm/apps/visorbus/build/visorbus.rom` — actual Springboard card image.
- `palm/apps/visorbus/build/visorbus.json` — independently verified manifest.

PRC creation/modification/backup timestamps are zeroed after packaging; MakeROM
uses `-chZCrDate`. This gives repeatable artifacts with the same tools/inputs.
Record the container image ID and SDK/tool versions when collecting evidence.
The original Dockerfile fetches its SDK from upstream master; rebuilding that
base image at a later date may change inputs. This work does not silently replace
the existing legacy environment or claim a globally pinned Palm SDK.

The ROM contains one `appl`/`FNVB` resource database, `Visor Bus Test`, including
a 256-byte incrementing pattern. HsAT declares 300 ns; that is an unverified
hardware requirement, not a measured capability. No setup/welcome auto-launch,
interrupt handler, system patch, or replacement OS is installed.

`verify-springboard-rom.py` checks card/store signatures, CRC16 across the complete
image, the HsAT token, database/resource directory and byte-for-byte equality of
every resource against the PRC. Mutation tests reject corrupt headers, payloads,
tokens and invalid resource pointers. Successful parsing does not prove Palm OS
will recognize the physical module; that is a later hardware gate.

## Slot diagnostics and RESET test (0.2 and later)

Install `visorbus.prc` into RAM, then open **Visor Bus Test 0.3**. No automatic
reset, memory probe, interrupt handler, patch or timing change runs on launch.
The initial selected slot is 1; **Next** cycles through slots 0–3 without hiding
invalid-slot errors. Slot 0 is internal and is never eligible for a reset pulse.

**Inspect** is read-only. It displays OS/Handspring feature versions and the raw
value/error for each attribute: Rem (removable), Hw (hardware installed and power-on
reset finished), Sw (OS software recognition), Rst (module reset asserted), CS0
(base), Size (each select's span) and ns (access time). All numbers are hexadecimal;
Err is the four-digit API error, with 0000 meaning success. Failed getters display
-------- rather than a misleading zero. Values for unrecognized slots are queried
through the API, not dereferenced as memory. The old 0.1 No module reported screen
collapsed errors and presence states and must not be treated as a diagnosis.

**Pulse** performs only a module RESET signal test. It refuses invalid/internal
slots, getter errors, non-removable slots, Hw=0, Sw!=0 and Rst!=0. A successful
precheck opens a confirmation with Cancel as default. After confirmation, it
rechecks the slot, asserts RESET through HsCardAttrSet, waits approximately 250 ms
(rounded up to system ticks), releases it, and reads back state. Restoration is
attempted even after assertion failure, with one bounded retry if release fails.
The result screen shows guard reason, query/assert/release/readback errors and
readback when available. Restore errors are not success; report them before any
further test. This does not reset the Prism or drive RP2350 bus data.

For physical observation, first install RP2350 USB debug v2 and read `r` for a GP45
baseline. After a confirmed successful Pulse, `r` should show one additional
asserted and released transition and the released level. Core-0 sampling is not a
logic analyzer and can miss short pulses; this test establishes one control path,
not ROM/mailbox communication. Stop the noisy PIO capture with `x` if desired;
RESET monitoring continues independently. Do not use Pulse until the monitor is
ready and the correct removable prototype slot is verified.

**Pat** (Pattern) still verifies its 256-byte resource with CRC32 29058c73. In a
RAM-installed app this tests RAM resources, not Springboard ROM reads.

Run the native reset safety tests from the repository root:

```sh
make -C palm/apps/visorbus check
```

The tests cover all precondition refusals without writes, pulse ordering and
duration, assertion failure cleanup, release retry/failure and readback failures.
Versions 0.1/0.2 UI and slot diagnostics were tested on the user's Prism;
version 0.3 direct-read results remain pending. The current passive RP2350 firmware cannot answer
ROM or CS1 reads, even when a verified card image is embedded.

## Source/tool provenance

`tools/makerom/` imports the source supplied with the local Handspring tools;
its original redistribution permission and notices are retained. See its README
for the 32-bit build assumptions. No SDK headers/manuals were copied into this repo.
`hs_card.h` narrowly declares the documented extension selector and attributes;
the call sequence follows the provided Handspring header's GCC convention.

## Version 0.3: bounded CS0 read stimulus

**Read** (between Next and Pulse) is a manual eight-word probe of offsets
0, 2, 4, 6, 8, A, C, E from the runtime `HsCardAttrGet` CS0 base. **Pat** is the
existing local pattern test; it does not read the bus. Opening/Inspect/Next do
not automatically dereference module memory.

Read refuses internal/invalid/nonremovable slots, absent hardware, unregistered
slots, asserted RESET, query errors, zero/odd/overflowing mappings, regions under
16 bytes or above 16 MiB. Confirmation defaults to Cancel. State and mapping are
rechecked after confirmation and before each volatile 16-bit read; changed
mapping, attribute error or removal aborts the remaining words. No CS1, writes,
RESET changes, interrupt hooks or automatic repeats. Access time is left alone.
The Handspring selector-9 exception list provides module-removal recovery around
each dereference; this does not promise recovery from arbitrary hardware faults.
Keep the module seated during the test. Host tests exercise the policy with fake
attributes/reads; actual trap/removal handling requires device validation.

Use RP USB debug v3: `a` to clear/arm, then `s` to verify a new run with
`arm_pending=0`, `stopped=0`, `limit=0`, stalls0/0. Tap Read and confirm once;
then `x`, wait stopped, `s`, `d`. Record returned words and full trace metadata.
If the buffer filled before the test, do not attribute its prefix to these reads.
The RP remains passive: returned words are **observations**, not a ROM PASS.
No valid test-ROM response is expected until active emulation is implemented.
A zero-drop trace does not prove complete capture: inspect limit, queued words,
stalls and the independent-window/polling limitations in the RP README.

Run `make -C palm/apps/visorbus check` for reset and read policy tests.
