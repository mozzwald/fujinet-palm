#include "core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct world {VTU32 time;int sets,gets,guards,reads,journals;int fail_set,fail_get,fail_guard,fail_journal,bad_data;};
static int guard(void *p,const struct vt_record *r){struct world *w=p;(void)r;return ++w->guards==w->fail_guard?71:0;}
static int get(void *p,VTU32 *v){struct world *w=p;*v=w->time;return ++w->gets==w->fail_get?72:0;}
static int set(void *p,VTU32 v){struct world *w=p;w->time=v;return ++w->sets==w->fail_set?73:0;}
static int journal(void *p,const struct vt_record *r,int pending){struct world *w=p;assert(!!(r->v[V_FLAGS]&VT_PENDING)==pending);return ++w->journals==w->fail_journal?74:0;}
static int loads(void *p,VTU32 a,VTU32 *w0,VTU32 *w1,VTU32 *l){struct world *w=p;assert(a==0x28000008);++w->reads;*w0=0xfeed;*w1=0xbeef;*l=w->bad_data?0xfeedfeed:0xfeedbeef;return 0;}
static int cancel_ready(void *p,const struct vt_record *r){(void)p;(void)r;return 75;}
static int changed_ready(void *p,const struct vt_record *r){struct world *w=p;(void)r;w->time=330;return 0;}
static void init(struct world *w,struct vt_record *r,struct vt_ops *o){memset(w,0,sizeof(*w));w->time=330;vt_init(r);r->v[V_FLAGS]=VT_RAM;r->v[V_BASE]=0x28000000;*o=(struct vt_ops){guard,get,set,journal,loads,w,0};}
int main(void){struct world w;struct vt_record r,d;struct vt_ops o;unsigned char b[VT_BYTES];
 init(&w,&r,&o);assert(!vt_run(&r,&o,0));assert(!w.sets&&w.reads==1&&r.v[V_RESTORED]==330&&r.v[V_STAGE]==7);
 init(&w,&r,&o);assert(!vt_run(&r,&o,1));assert(w.sets==2&&w.time==330&&w.reads==1&&w.journals==2&&r.v[V_APPLIED]==360&&!(r.v[V_FLAGS]&VT_PENDING));
 vt_encode(&r,b);assert(!vt_decode(&d,b,sizeof(b)));assert(d.v[V_LONG]==0xfeedbeef);b[17]^=1;assert(vt_decode(&d,b,sizeof(b)));assert(vt_decode(&d,b,sizeof(b)-1));
 init(&w,&r,&o);w.time=360;assert(vt_run(&r,&o,1)==VT_RANGE);assert(!w.sets&&!w.reads);
 init(&w,&r,&o);w.fail_journal=1;assert(vt_run(&r,&o,1)==VT_LOG);assert(!w.sets&&!w.reads);
 init(&w,&r,&o);w.fail_set=1;assert(vt_run(&r,&o,1));assert(w.sets==2&&w.time==330&&!w.reads&&!(r.v[V_FLAGS]&VT_PENDING));
 init(&w,&r,&o);w.fail_get=2;assert(vt_run(&r,&o,1));assert(w.sets==2&&w.time==330&&!w.reads);
 init(&w,&r,&o);w.bad_data=1;assert(vt_run(&r,&o,1)==VT_DATA);assert(w.time==330&&w.sets==2);
 init(&w,&r,&o);w.fail_set=2;assert(vt_run(&r,&o,1));assert(r.v[V_FLAGS]&VT_PENDING);
 init(&w,&r,&o);w.fail_journal=2;assert(vt_run(&r,&o,1)==VT_LOG);assert(w.time==330&&(r.v[V_FLAGS]&VT_PENDING));
 init(&w,&r,&o);w.fail_guard=3;assert(vt_run(&r,&o,1));assert(w.sets==1&&!w.reads&&(r.v[V_FLAGS]&VT_PENDING));
 init(&w,&r,&o);w.fail_guard=4;assert(vt_run(&r,&o,1));assert(w.sets==1&&w.reads==1&&(r.v[V_FLAGS]&VT_PENDING));
 init(&w,&r,&o);r.v[V_FLAGS]|=VT_PENDING;r.v[V_ORIG]=330;w.time=360;assert(!vt_restore(&r,&o));assert(w.time==330&&w.sets==1);
 init(&w,&r,&o);r.v[V_FLAGS]=0;assert(vt_run(&r,&o,1)==VT_BAD);assert(!w.sets&&!w.reads);
 init(&w,&r,&o);o.ready=cancel_ready;assert(vt_run(&r,&o,1)==75);assert(!w.reads&&w.time==330&&w.sets==2);
 init(&w,&r,&o);o.ready=changed_ready;assert(vt_run(&r,&o,1)==VT_READBACK);assert(!w.reads&&w.time==330);
 puts("PASS: baseline, one-step slower, partial setter failure, readback/data/restore/journal failures, observed identity-change latch, recovery and CRC codec");return 0;}
