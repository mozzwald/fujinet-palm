# Module-delivered CS1 diagnostic B

ROM contains one HsSU/HsCd Setup database. Handspring copies Setup into RAM;
the stack-only install handler clones/retypes it into CS1 Module B/appl-FNMC.
B has distinct preferences and install/result journals; A/FNMB and its malformed
first result remain untouched. Existing name/version/code collision protection
is unchanged. Native policy tests include preservation of A while installing B.
No standalone diagnostic PRC upload is part of the bench workflow.

The diagnostic uses SDK CsBase+CsSize for CS1, explicit MOVE.W access helpers,
330ns only, RAM-PC/resource proof and lifecycle guards. It runs four bounded
token writes with polling and interspersed CS0 FEED/BEEF reads. It implements no
NIO service or USB data transport. The normal journal is32 BE32 fields (128bytes)
with CRC32; the mandatory compiled-code build gate checks128-byte clear,
allocation and write arguments, preventing sizeof(pointer) truncation. Guard
failure detail is in word29/on screen, without bypassing any guard.

Build with existing Springboard Docker toolchain, an absolute ABI include path
from sibling fujinet-nio, and make rom. Export linked text/nm/sections and run
verify-cs1setup-code.py; verify-cs1setup-rom.py checks header/resource/token/CRC
independently. make check runs adverse installation and coexistence tests.
136 host tests passed with CS1_MODULE_STATIC set to the independently verified
B manifest; build outputs and earlier firmware fixtures must exist first.

Current bench limitation: replacing RP2350 ROM and soft-resetting the Prism did
not refresh its OS-managed RAM Setup copy. Read-only inspection found exact old
A resources/receipt and no B/app receipt. Do not assume reflash updates a retained
Setup, delete OS-owned Setup, or call lifecycle APIs speculatively. Supported
refresh and actual B RAM installation must be established before another Run.
Physical CS1 qualification remains incomplete; opening an old diagnostic is not
proof the new payload installed. Firmware reset quarantine requires coordinated
engineer recovery, not repeated resets. No hardware action is automatic.
