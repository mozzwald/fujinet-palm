/* Two explicit MOVE.W reads, then ONE aligned MOVE.L. RAM stores between. */
.text
.even
.globl vt_pc
vt_pc:
    lea vt_pc(%pc),%a0
    move.l %a0,%d0
    rts
.globl vt_loads
vt_loads:
    move.l 4(%sp),%a0
    move.l 8(%sp),%a1
    move.w (%a0),%d0
    move.w %d0,(%a1)
    move.w 2(%a0),%d0
    move.w %d0,2(%a1)
    move.l 12(%sp),%a1
    move.l (%a0),%d0
    move.l %d0,(%a1)
    rts
