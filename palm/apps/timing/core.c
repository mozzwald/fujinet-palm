#include "core.h"
void vt_init(struct vt_record *r){unsigned i;for(i=0;i<VT_FIELDS;++i)r->v[i]=0;r->v[V_MAGIC]=0x46544d33UL;r->v[V_VERSION]=1;r->v[V_BUILD]=0x20261026UL;}
VTU32 vt_crc(const unsigned char *b,unsigned n){VTU32 c=0xffffffffUL;unsigned i,j;for(i=0;i<n;++i){c^=b[i];for(j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320UL:0);}return ~c;}
void vt_encode(const struct vt_record *r,unsigned char *b){unsigned i;VTU32 v;for(i=0;i<VT_FIELDS;++i){v=r->v[i];b[4*i]=v>>24;b[4*i+1]=v>>16;b[4*i+2]=v>>8;b[4*i+3]=v;}v=vt_crc(b,VT_BYTES-4);b[188]=v>>24;b[189]=v>>16;b[190]=v>>8;b[191]=v;}
int vt_decode(struct vt_record *r,const unsigned char *b,unsigned n){unsigned i;if(n!=VT_BYTES)return VT_BAD;for(i=0;i<VT_FIELDS;++i)r->v[i]=((VTU32)b[4*i]<<24)|((VTU32)b[4*i+1]<<16)|((VTU32)b[4*i+2]<<8)|b[4*i+3];return r->v[V_MAGIC]==0x46544d33UL&&r->v[V_VERSION]==1&&r->v[V_CRC]==vt_crc(b,188)?0:VT_BAD;}
int vt_restore(struct vt_record *r,const struct vt_ops *o){int e;VTU32 actual=0;
 if(!(r->v[V_FLAGS]&VT_PENDING))return 0;
 if(r->v[V_GUARDERR]){r->v[V_RESTOREERR]=VT_IDENTITY;return VT_IDENTITY;}
 e=o->guard(o->ctx,r);if(e){r->v[V_GUARDERR]=e;r->v[V_RESTOREERR]=VT_IDENTITY;return VT_IDENTITY;}
 /* No hardware mutation after observed reset/removal/generation or map change. */
 e=o->set(o->ctx,r->v[V_ORIG]);r->v[V_RESTOREERR]=e;
 if(!e){e=o->get(o->ctx,&actual);}
 r->v[V_RESTORED]=actual;
 if(e||actual!=r->v[V_ORIG]){r->v[V_RESTOREERR]=e?e:VT_RESTORE;return VT_RESTORE;}
 r->v[V_FLAGS]=(r->v[V_FLAGS]&~VT_PENDING)|VT_RESTORED;
 e=o->journal(o->ctx,r,0);if(e){r->v[V_FLAGS]|=VT_PENDING;r->v[V_LOGERR]=e;return VT_LOG;}
 return 0;
}
int vt_run(struct vt_record *r,const struct vt_ops *o,int slower){int e=0,cleanup=0;VTU32 t=0;
 if(r->v[V_FLAGS]&VT_PENDING)return VT_RESTORE;
 r->v[V_EVENT]=slower?2:1;r->v[V_STAGE]=1;
 if(!(r->v[V_FLAGS]&VT_RAM)){e=VT_BAD;goto done;}
 e=o->guard(o->ctx,r);if(e){r->v[V_GUARDERR]=e;goto done;}
 e=o->get(o->ctx,&t);if(e){r->v[V_GETERR]=e;goto done;}
 r->v[V_ORIG]=r->v[V_APPLIED]=t;
 /* Prism bench only: one conservative 30ns increment; not an API maximum. */
 if(slower){
  if(t!=330){e=VT_RANGE;goto done;}r->v[V_REQUEST]=360;r->v[V_STAGE]=2;
  r->v[V_FLAGS]|=VT_PENDING;e=o->journal(o->ctx,r,1);
  if(e){r->v[V_FLAGS]&=~VT_PENDING;r->v[V_LOGERR]=e;e=VT_LOG;goto done;}
  e=o->guard(o->ctx,r);if(e){r->v[V_GUARDERR]=e;goto cleanup;}
  e=o->set(o->ctx,360);r->v[V_SETERR]=e;if(e)goto cleanup;
  e=o->get(o->ctx,&t);r->v[V_GETERR]=e;if(e)goto cleanup;
  r->v[V_APPLIED]=t;if(t<360){e=VT_READBACK;goto cleanup;}
 }
 if(slower&&o->ready){
  e=o->ready(o->ctx,r);if(e)goto cleanup;
  e=o->get(o->ctx,&t);r->v[V_GETERR]=e;if(e)goto cleanup;
  if(t!=r->v[V_APPLIED]){e=VT_READBACK;goto cleanup;}
 }
 r->v[V_STAGE]=3;e=o->guard(o->ctx,r);if(e){r->v[V_GUARDERR]=e;goto cleanup;}
 e=o->loads(o->ctx,r->v[V_BASE]+8,&r->v[V_W0],&r->v[V_W1],&r->v[V_LONG]);r->v[V_ACCESSERR]=e;r->v[V_FLAGS]|=VT_RAN;
 if(!e&&(r->v[V_W0]!=0xfeed||r->v[V_W1]!=0xbeef||r->v[V_LONG]!=0xfeedbeefUL))e=VT_DATA;
cleanup:
 if(slower)cleanup=vt_restore(r,o);
 else {cleanup=o->guard(o->ctx,r);if(!cleanup)cleanup=o->get(o->ctx,&t);r->v[V_RESTORED]=t;if(!cleanup&&t!=r->v[V_ORIG])cleanup=VT_RESTORE;if(!cleanup)r->v[V_FLAGS]|=VT_RESTORED;}
 if(!e)e=cleanup;
done:r->v[V_ERROR]=e;if(!e)r->v[V_STAGE]=7;return e;
}
