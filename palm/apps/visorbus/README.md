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

## Test on Prism

Install only when desired using the existing `tools/install-prc.sh` or your usual
PRC installer, then manually open **Visor Bus Test**. There is no automatic install
or flashing step in the build script.

- **Inspect** displays the OS/Handspring feature versions and, when reported by
  the extension API, removable card number, CS0 base, CS size, access time and
  derived CS1 base. Values are hexadecimal. It does not assume fixed Prism bus
  addresses, change access timing or access peripheral memory directly.
- **Pattern** validates its resource bytes and displays CRC32 (`29058c73` for the
  incrementing 256-byte pattern), size and app version. In a RAM-installed PRC
  this only tests RAM resources; it is not proof of Springboard ROM reads.
- On a non-Handspring Palm, Inspect reports that the Handspring API is absent.
- A passive/unrecognized module may not be exposed by the card APIs. The app
  reports that condition; it does not invent a physical address or probe it.

Initially both screens are read-only diagnostics. Direct CS1 register reads,
writes, loopback and IRQ tests will be added only with the corresponding active
firmware and phase approval. Insertion recognition is impossible with the current
NIO `springboard_passive.uf2`: embedding this ROM copies it to SRAM but does not
drive the Springboard bus.

No emulator/device run has yet been performed. The existing emulator workflow
can check the generic UI, but the checkout's documentation says it does not model
Handspring Springboard hardware. Use the Prism for those claims.

## Source/tool provenance

`tools/makerom/` imports the source supplied with the local Handspring tools;
its original redistribution permission and notices are retained. See its README
for the 32-bit build assumptions. No SDK headers/manuals were copied into this repo.
`hs_card.h` narrowly declares the documented extension selector and attributes;
the call sequence follows the provided Handspring header's GCC convention.
