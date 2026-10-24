#include "burst_read.h"
enum { burstBadCapture=0x8401 };
static int check_slot(struct visor_timed_read *s, const struct visor_timing_ops *o) {
  struct visor_read_result now=visor_read_check(o->bus,1);
  if(now.reason != visorReadReady) {
    s->image.guard.reason=now.reason; s->image.guard.error=now.error;
    s->image.guard.attribute=now.attribute; s->unsafe=1; return 0;
  }
  if(now.base != s->image.guard.base || now.size != s->image.guard.size) {
    s->image.guard.reason=visorReadMappingChanged; s->unsafe=1; return 0;
  }
  return 1;
}
void vc_burst_step(struct visor_timed_read *s, const struct visor_timing_ops *o,
    const struct vc_burst_ops *b, unsigned char *captured,
    unsigned long capacity, unsigned long spacing) {
  unsigned long start, remaining, i;
  unsigned short count, done=0;
  int error;
  if(!s->active) return;
  if(!captured || capacity<s->image.length || !b || !b->capture ||
      spacing>=VC_SPACE_COUNT || s->image.completed>s->image.length ||
      (s->image.completed&1)) {
    s->image.guard.reason=visorReadAccessFailed;
    s->image.guard.error=burstBadCapture; visor_timed_finish(s,o); return;
  }
  if(!check_slot(s,o)) { visor_timed_finish(s,o); return; }
  start=s->image.completed; remaining=(s->image.length-start)/2;
  count=(unsigned short)(remaining<VC_BURST_WORDS?remaining:VC_BURST_WORDS);
  if(!count) { visor_timed_finish(s,o); return; }
  error=b->capture(b->context,s->image.guard.base+start,captured+start,count,
                   spacing,vc_spacing_iterations(spacing),&done);
  if(done>count) { done=0; error=burstBadCapture; }
  /* CRC/mismatch work only after the bounded physical capture, not between
   * these words. Preserve a valid partial prefix on removal/read failure. */
  for(i=0;i<done;++i) {
    unsigned short word=(unsigned short)(((unsigned short)captured[start+i*2]<<8)|
                                         captured[start+i*2+1]);
    visor_image_accept_word(&s->image,word);
  }
  if(error || done!=count) {
    s->image.guard.reason=visorReadAccessFailed;
    s->image.guard.error=error?error:burstBadCapture;
    visor_timed_finish(s,o); return;
  }
  if(!check_slot(s,o) || s->image.completed==s->image.length)
    visor_timed_finish(s,o);
}
