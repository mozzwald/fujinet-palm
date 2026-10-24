#include "burst_read.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake { unsigned reads, calls, queries; int missing, remapped, fail_after, short_capture, bad_count, post_change; };
static int byte(void *p,unsigned short card,unsigned short attr,unsigned char *v) {
  struct fake *f=p; assert(card==1);++f->queries;
  *v=attr==visorCardReset?0:attr==visorCardHwInstalled?!f->missing:1;return 0;
}
static int wide(void *p,unsigned short card,unsigned short attr,unsigned long *v) {
  struct fake *f=p;assert(card==1);++f->queries;
  *v=attr==visorCardAccessTime?330:attr==visorCardCsBase?0x28000000UL+(f->remapped?2:0):0x1000000UL;return 0;
}
static int word(void *p,unsigned long a,unsigned short *v) {(void)p;(void)a;(void)v;assert(!"per-word reader used by burst");return 1;}
static int set(void *p,unsigned short c,unsigned long v) {(void)p;(void)c;(void)v;assert(!"wait-state write");return 1;}
static int journal(void *p,unsigned long v) {(void)p;(void)v;assert(!"journal write");return 1;}
static int capture(void *p,unsigned long address,unsigned char *dest,unsigned short count,
    unsigned long mode,unsigned long iterations,unsigned short *done) {
  struct fake *f=p;unsigned i;assert(address==0x28000000UL+f->reads*2);
  assert(count && count<=64 && iterations==vc_spacing_iterations(mode));++f->calls;*done=0;
  for(i=0;i<count;++i) {
    unsigned n=f->reads*2;
    if(f->fail_after && f->reads==(unsigned)f->fail_after)return 17;
    if(f->short_capture && i==2)return 0;
    dest[i*2]=(unsigned char)n;dest[i*2+1]=(unsigned char)(n+1);++f->reads;++*done;
  }
  if(f->bad_count)*done=(unsigned short)(count+1);
  if(f->post_change==1)f->missing=1;
  if(f->post_change==2)f->remapped=1;
  return 0;
}
int main(void) {
  struct fake f;struct visor_timed_read state;
  struct visor_read_ops bus={byte,wide,word,&f};
  struct visor_timing_ops timing={&bus,set,journal};
  struct vc_burst_ops burst={capture,&f};
  unsigned char actual[256],expected[256];unsigned i,mode;
  for(i=0;i<256;++i)expected[i]=(unsigned char)i;
  for(mode=0;mode<VC_SPACE_COUNT;++mode) {
    memset(&f,0,sizeof(f));memset(&state,0,sizeof(state));memset(actual,0,sizeof(actual));
    visor_timed_begin(&state,&timing,256,expected,0);
    while(state.active)vc_burst_step(&state,&timing,&burst,actual,sizeof(actual),mode);
    assert(f.reads==128 && f.calls==2 && state.image.completed==256);
    assert(state.image.mismatch_count==0 && (state.image.crc^0xffffffffUL)==0x29058c73UL);
    assert(!memcmp(actual,expected,256));
  }
  /* Guard rejection before any physical read; original mapping is retained. */
  for(i=0;i<2;++i) {
    memset(&f,0,sizeof(f));memset(&state,0,sizeof(state));
    visor_timed_begin(&state,&timing,128,expected,0);
    if(i)f.remapped=1;else f.missing=1;
    vc_burst_step(&state,&timing,&burst,actual,sizeof(actual),VC_SPACE_NONE);
    assert(!f.reads && !f.calls && !state.active && state.unsafe);
    assert(state.image.guard.base==0x28000000UL);
  }
  /* Preserve exactly the captured prefix on recovery/error, never count stale RAM. */
  for(i=1;i<=2;++i) {
    memset(&f,0,sizeof(f));memset(&state,0,sizeof(state));f.post_change=i;
    visor_timed_begin(&state,&timing,256,expected,0);
    vc_burst_step(&state,&timing,&burst,actual,sizeof(actual),VC_SPACE_NONE);
    assert(!state.active && state.unsafe && state.image.completed==128);
    assert(f.calls==1 && state.image.guard.base==0x28000000UL);
  }
  memset(&f,0,sizeof(f));memset(&state,0,sizeof(state));f.fail_after=3;
  visor_timed_begin(&state,&timing,128,expected,0);
  vc_burst_step(&state,&timing,&burst,actual,sizeof(actual),VC_SPACE_1US);
  assert(!state.active && state.image.completed==6 && state.image.guard.error==17);
  assert(state.image.crc==(visor_image_local_crc(expected,6)^0xffffffffUL));
  for(i=0;i<2;++i) {
    memset(&f,0,sizeof(f));memset(&state,0,sizeof(state));
    if(i)f.bad_count=1;else f.short_capture=1;
    visor_timed_begin(&state,&timing,128,expected,0);
    vc_burst_step(&state,&timing,&burst,actual,sizeof(actual),VC_SPACE_NONE);
    assert(!state.active && state.image.guard.reason==visorReadAccessFailed);
    assert(state.image.completed==(i?0:4));
  }
  memset(&f,0,sizeof(f));memset(&state,0,sizeof(state));
  visor_timed_begin(&state,&timing,128,expected,0);
  vc_burst_step(&state,&timing,&burst,actual,16,VC_SPACE_NONE);
  assert(!state.active && !f.reads);
  /* Every metadata combination is explicit; nominal microseconds are not bus gaps. */
  for(mode=0;mode<VC_SPACE_COUNT;++mode)for(i=0;i<VC_CAPTURE_COUNT;++i)
    assert(vc_spacing_valid(mode,i,vc_spacing_iterations(mode),vc_capture_words(i)));
  assert(!vc_spacing_valid(99,0,0,1));assert(!vc_spacing_valid(0,99,0,1));
  assert(!vc_spacing_valid(VC_SPACE_1US,0,0,1));
  puts("Burst reader: bounded physical captures, spacing metadata, guards, partial errors and CRC PASS");return 0;
}
