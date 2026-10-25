/* Two explicit word reads; only unrolled NOPs between external bus accesses.
 * Select entry BEFORE the first load. C ABI result D0 = first<<16 | second. */
.text
.even
.macro word_pair n
.globl vc_words_\n
vc_words_\n:
    move.l 4(%sp),%a0
    move.w (%a0),%d0
    .rept \n
    nop
    .endr
    move.w 2(%a0),%d1
    swap %d0
    move.w %d1,%d0
    rts
.endm
word_pair 0
word_pair 1
word_pair 2
word_pair 4
word_pair 8
word_pair 16
word_pair 32
word_pair 64
