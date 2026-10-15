/* Narrow Handspring API declaration; selector/attributes documented in HsExt.h.
 * No private SDK headers or fixed Prism bus base addresses required. */
#ifndef VISOR_HS_CARD_H
#define VISOR_HS_CARD_H
#include <PalmOS.h>
#define VISOR_HS_CREATOR 'hsEx'
#define VISOR_HS_VERSION 0
enum {
    visorCardRemovable,
    visorCardHwInstalled,
    visorCardSwInstalled,
    visorCardCsBase,
    visorCardCsSize,
    visorCardAccessTime
};
Err VisorCardAttrGet(UInt16 cardNo, UInt16 attr, void *valueP)
    __attribute__((__callseq__("move.w #6,-(%%sp); trap #15; dc.w 0xA349")));
#endif
