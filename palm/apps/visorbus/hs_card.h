/* Narrow Handspring API declaration; selector/attributes documented in HsExt.h.
 * No private SDK headers or fixed Prism bus base addresses required. */
#ifndef VISOR_HS_CARD_H
#define VISOR_HS_CARD_H
#include <PalmOS.h>
#define VISOR_HS_CREATOR 'hsEx'
#define VISOR_HS_VERSION 0
#include "reset_test.h"
Err VisorCardAttrGet(UInt16 cardNo, UInt16 attr, void *valueP)
    __attribute__((__callseq__("move.w #6,-(%%sp); trap #15; dc.w 0xA349")));
Err VisorCardAttrSet(UInt16 cardNo, UInt16 attr, void *valueP)
    __attribute__((__callseq__("move.w #7,-(%%sp); trap #15; dc.w 0xA349")));
/* Handspring removal recovery list, selector 9 (HsExt.h). */
MemPtr *VisorCardCatchListP(void)
    __attribute__((__callseq__("move.w #9,-(%%sp); trap #15; dc.w 0xA349")));
#endif
