#ifndef VC_PAIR_CODEC_H
#define VC_PAIR_CODEC_H
#include "result_codec.h"
#define VP_HEADER 160UL
#define VP_MAX_PAIRS 10000UL
#ifdef VC_WORDPAIR_DIAGNOSTIC
#define VP_RESULTS_DB "ROMWordResults"
#define VP_CONTROL_DB "ROMWordControl"
#else
#define VP_RESULTS_DB "ROMPairResults"
#define VP_CONTROL_DB "ROMPairControl"
#endif
/* Every returned longword is retained, including both halves of every failure.
 * 10,000 values + header + CRC fit one Palm record; no mismatch truncation. */
struct vp_result {
  unsigned char session[16], sha[32];
  unsigned long sequence,status,offset,expected,requested,completed;
  unsigned long first_bad,second_bad,long_bad,base,size,before,after;
  unsigned long reason,error,started,ended,reference_crc;
  unsigned long nops; /* used only by separate two-MOVE.W diagnostic */
};
void vp_accept(struct vp_result *,unsigned char *,unsigned long);
unsigned long vp_encode(unsigned char *,unsigned long,const struct vp_result *,
                        const unsigned char *);
#endif
