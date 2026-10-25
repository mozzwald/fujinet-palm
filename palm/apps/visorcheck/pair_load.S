/* One ROM longword load. m68k-palmos C ABI: argument at4(sp), result in d0.
 * No instrumentation, stores or branches between the two hardware halfwords. */
.text
.even
.globl vc_pair_load
vc_pair_load:
    move.l 4(%sp),%a0
    move.l (%a0),%d0
    rts
