/* Shared, host-tested installation policy. No static/global storage or Palm I/O. */
#include "policy.h"
void rp_clear(struct rp_record *r) {
 unsigned i;for(i=0;i<RP_FIELDS;++i)r->v[i]=0;
 r->v[RP_MAGIC]=0x46525032UL;r->v[RP_FORMAT]=RP_VERSION;
}
void rp_encode(const struct rp_record *r,unsigned char *b) {
 unsigned i;for(i=0;i<RP_FIELDS;++i){RPU32 v=r->v[i];
 b[4*i]=(unsigned char)(v>>24);b[4*i+1]=(unsigned char)(v>>16);
 b[4*i+2]=(unsigned char)(v>>8);b[4*i+3]=(unsigned char)v;}
}
int rp_decode(struct rp_record *r,const unsigned char *b,unsigned n) {
 unsigned i;if(n!=RP_FIELDS*4)return RP_BAD_RECEIPT;
 for(i=0;i<RP_FIELDS;++i)r->v[i]=((RPU32)b[4*i]<<24)|((RPU32)b[4*i+1]<<16)|((RPU32)b[4*i+2]<<8)|b[4*i+3];
 return r->v[RP_MAGIC]==0x46525032UL&&r->v[RP_FORMAT]==RP_VERSION?0:RP_BAD_RECEIPT;
}
int rp_name_eq(const char *a,const char *b) {
 unsigned i;for(i=0;i<32;++i){if(a[i]!=b[i])return 0;if(!a[i])return 1;}return 0;
}
int rp_in_ram(const struct rp_info *i,RPU32 pc) {
 return i->card==0&&i->ram&&!(i->heapflags&1)&&i->base&&i->size&&
 i->base<=0xffffffffUL-i->size&&pc>=i->base&&pc<i->base+i->size;
}
static int owned(const struct rp_info *i,const char *name,RPU32 type,RPU32 creator,RPU32 crc) {
 return (!name||rp_name_eq(i->name,name))&&i->type==type&&i->creator==creator&&
 i->version==RP_VERSION&&(i->attrs&1)&&!(i->attrs&0x20)&&
 rp_in_ram(i,i->base)&&i->crc==crc;
}
static int result(const struct rp_ops *o,struct rp_record *r,int e) {
 int log;r->v[RP_EVENT]=RP_INSTALL_END;r->v[RP_ERROR]=(RPU32)e;
 r->v[RP_ACTIVE]=e?0:1;log=o->save(o->ctx,r);
 if(log){r->v[RP_AUX_ERROR]=(RPU32)log;r->v[RP_ACTIVE]=0;if(!e)e=RP_LOG_ERROR;}
 return e;
}
static int cleanup(const struct rp_ops *o,struct rp_record *r,RPU32 id,int error) {
 /* Only a newly created, non-source ID can be rolled back. */
 int e;if(id&&id!=r->v[RP_SOURCE_DB]){e=o->erase(o->ctx,id);if(e)r->v[RP_AUX_ERROR]=(RPU32)e;}
 return result(o,r,error);
}
int rp_install(const struct rp_ops *o,struct rp_record *r,RPU32 source,RPU32 pc) {
 struct rp_info src,i;RPU32 id=0,old,temp;int e;
 r->v[RP_EVENT]=RP_INSTALL_BEGIN;r->v[RP_ACTIVE]=0;r->v[RP_ERROR]=0;
 r->v[RP_RUNTIME_DB]=0;r->v[RP_SOURCE_DB]=source;r->v[RP_SETUP_PC]=pc;
 r->v[RP_STAGE]=1;r->v[RP_AUX_ERROR]=0;
 e=o->save(o->ctx,r);if(e)return RP_LOG_ERROR;
 e=o->inspect(o->ctx,source,&src);if(e)return result(o,r,e);
 r->v[RP_SETUP_BASE]=src.base;r->v[RP_SETUP_SIZE]=src.size;
 r->v[RP_SETUP_HEAP]=src.heap;r->v[RP_SETUP_HEAPFLAGS]=src.heapflags;
 r->v[RP_SETUP_CARD]=src.card;r->v[RP_SOURCE_CRC]=src.crc;
 r->v[RP_RAM_CHECKS]=src.ram?1:0;
 /* PalmOS renames its RAM copy (observed HsCardSetup.787D.9362).
  * Source ownership comes from the SDK-supplied ID and executing RAM PC,
  * plus type/creator/version/resource/heap checks, never the ROM DB name.
  * Temporary and published clones still require their exact owned names. */
 if(!owned(&src,0,RP_SETUP_TYPE,RP_SETUP_CREATOR,src.crc)||
 !rp_in_ram(&src,pc))return result(o,r,RP_BAD_SOURCE);
 r->v[RP_STAGE]=2;old=o->find(o->ctx,RP_RUNTIME_NAME);
 if(old){e=o->inspect(o->ctx,old,&i);if(e)return result(o,r,e);
 if(!owned(&i,RP_RUNTIME_NAME,RP_APP_TYPE,RP_CREATOR,src.crc)||(i.attrs&(2|0x100)))return result(o,r,RP_COLLISION);
 r->v[RP_RUNTIME_DB]=old;r->v[RP_STAGE]=7;return result(o,r,0);}
 temp=o->find(o->ctx,RP_TEMP_NAME);
 if(temp){e=o->inspect(o->ctx,temp,&i);if(e)return result(o,r,e);
 if(!owned(&i,RP_TEMP_NAME,RP_SETUP_TYPE,RP_SETUP_CREATOR,src.crc)||temp==source)
 return result(o,r,RP_COLLISION);
 if(i.busy)return result(o,r,RP_BUSY);
 e=o->erase(o->ctx,temp);if(e)return result(o,r,e);}
 r->v[RP_STAGE]=3;e=o->copy(o->ctx,source,&id);
 if(id==source||id==old||!id){
 /* copy may fail before creating anything. Never erase an alias. */
 return result(o,r,e?e:RP_ALIAS);}
 /* Preflight proved the temp name absent; still require the returned ID to be
  * the newly named temp before any delete or metadata mutation. */
 temp=o->find(o->ctx,RP_TEMP_NAME);
 if(temp!=id)return result(o,r,e?e:RP_ALIAS);
 if(e)return cleanup(o,r,id,e);
 r->v[RP_STAGE]=4;e=o->inspect(o->ctx,id,&i);
 if(e)return cleanup(o,r,id,e);
 if(!owned(&i,RP_TEMP_NAME,RP_SETUP_TYPE,RP_SETUP_CREATOR,src.crc)||i.busy)
 return cleanup(o,r,id,RP_BAD_CLONE);
 r->v[RP_STAGE]=5;e=o->publish(o->ctx,id);if(e)return cleanup(o,r,id,e);
 r->v[RP_STAGE]=6;e=o->inspect(o->ctx,id,&i);if(e)return cleanup(o,r,id,e);
 if(!owned(&i,RP_RUNTIME_NAME,RP_APP_TYPE,RP_CREATOR,src.crc)||
 (i.attrs&(0x20|0x100|2)))return cleanup(o,r,id,RP_BAD_CLONE);
 r->v[RP_RUNTIME_DB]=id;r->v[RP_STAGE]=7;return result(o,r,0);
}
int rp_remove(const struct rp_ops *o,struct rp_record *r,RPU32 module) {
 /* Never touch a module mapping, callback or database during removal. The OS
  * owns Setup deletion. The separately identified runtime stays installed. */
 if(r->v[RP_MODULE]!=module)return RP_BAD_RECEIPT;
 r->v[RP_EVENT]=RP_REMOVE;r->v[RP_ACTIVE]=0;
 return o->save(o->ctx,r);
}
