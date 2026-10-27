/* All locations are obtained PC-relative. No GPIO/CS1 access or OS calls. */
.text
.even
.globl fnram_pc
fnram_pc:
    lea fnram_pc(%pc),%a0
    move.l %a0,%d0
    rts
.even
.globl fnram_loop
.globl fnram_loop_end
fnram_loop:
    moveq #127,%d1
.Louter:
    move.w #-1,%d0
.Linner:
    nop
    dbra %d0,.Linner
    dbra %d1,.Louter
    move.l #0x52504c50,%d0
    rts
fnram_loop_end:
