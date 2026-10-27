.text
.even
.globl va_pc
va_pc:
 lea va_pc(%pc),%a0
 move.l %a0,%d0
 rts
