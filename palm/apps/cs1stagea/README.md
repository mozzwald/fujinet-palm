# CS1 Stage A RAM diagnostic

Separate ordinary appl/FNMA. Does not modify ROM, wait states or lifecycle.
Do not run against the installed Phase2 ROM-only firmware: it has no CS1 service.
Opening queries only installation identity, Handspring map/timing and own RAM
code location. Explicit Run performs four sequential PROBE/PROBE_ACK exchanges,
fixed register checks, bounded STATUS polling and two explicit CS0 ROM word
checks (FEED/BEEF) after each accepted token. One write outstanding at a time.
Each subsequent token is previous observed ACK+1; it never accepts READY or a
stale ACK as success. Timeout/error stops; no automatic replay. PASS makes a tone.

CsBase/CsSize are actual API values; CS1=base+size with checked alignment/range.
The observed16MiB windows and24bit-aligned select bases are required because
this candidate decodes A1..A23 directly; other mappings fail before CS1 access.
No base address is hardcoded. Original access setting must be330; no setter. Guards use active FNRA receipt,
module generation/presence/reset/setup/map and removal exception chain. RAM
location uses app DB card0, code1 RAM-heap membership, PC-relative marker and CRC.

Uses exactly the authoritative header from sibling fujinet-nio via Makefile ABI.
Docker: mount that include directory read-only at /abi, then `make ABI=/abi`.
Word helpers are in a separate translation unit so this older compiler cannot
inline them; verified disassembly has one peripheral MOVE.W load/store each.
No byte or32bit peripheral accesses. No NIO request, backend or USB transport.

FN CS1 Stage A Log DATA/FNMA stores32BE32 fields (128bytes), final CRC over124.
Run collection through tools/cs1_stagea_collect.py and verify exact static build
manifest from tools/verify-cs1-stagea.py. Latest PASS is necessary, not proof of
physical CS1 response timing, bus release or lifecycle regression.
