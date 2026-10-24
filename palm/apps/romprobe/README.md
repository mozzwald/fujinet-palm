# ROM-only probe R3

Build `make rom` with the existing Palm/Springboard Docker toolchain. Input PRC
is FujiNet ROM Probe/applFNVR; never HotSync it into RAM. The resulting ROM has
no CardSetup/CardWelcome app and no reset-after-install flag. Normal launch is
manual; all other launch commands return without application work. HsAT330
requests the bench's existing330ns access setting; this is not timing proof.

The normal-launch screen shows a ROM-distinct title, current card/database,
PilotMain and tPAT resource pointers and pattern CRC29058C73. Compare pointers
with the independently reported module logical range and confirm no RAM FNVR
copy before claiming ROM execution. A screen/title or icon alone is insufficient.

`../rominspect` builds the separate FNVI RAM inspection app. It enumerates FNVR
and inspects database/code1/tPAT pointers without calling its application entry.
It does not trigger insertion, reset, registration or launch. Inspect the module
only after the candidate responder has passed physical readback qualification.

`../visorcheck/Makefile.romprobe` builds a separate FNVD RAM checker with the new
ROM reference. It uses ROMProbeControl/ROMProbeResults and FNVD preferences,
preserving the original FNVC RAM application, databases and preferences. Use
prism_collect.py --profile romprobe on every subcommand for that session. Native
storage tests cover both identities. These creator IDs are bench diagnostics.
