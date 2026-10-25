#ifndef VC_PAIR_APP_H
#define VC_PAIR_APP_H
#include <PalmOS.h>
enum { PairForm=2000, PairOpenButton=1011 };
void vp_open(const unsigned char *,unsigned long,unsigned long);
Boolean vp_event(EventType *);
Boolean vp_active(void);
void vp_close(void);
#endif
