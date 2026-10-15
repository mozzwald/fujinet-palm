# Palm-MakeROM source

Imported from the user's Handspring development tools, MakeROM/Src. Source
copyright notices grant redistribution/modification and are preserved verbatim.
No reference manuals or complete Handspring SDK are redistributed here.

The original code assumes 32-bit host pointers and packed Palm structures.
`build.py` compiles it as an i386 executable against the existing container SDK,
maps `_strdup` to POSIX `strdup`, and supplies an empty `extras.h` compatibility
header. The original six source/header files are otherwise unchanged apart from
line-ending/trailing-whitespace normalization. Legacy warnings are not suppressed.

Build in the springboard toolchain image with `python3 tools/makerom/build.py`.
Do not compile this utility as a native 64-bit executable.

This product includes software developed by 3Com and its contributors.
See the complete redistribution conditions and disclaimer retained at the top
of `Crc.cpp`; retain those notices when distributing the built utility.
