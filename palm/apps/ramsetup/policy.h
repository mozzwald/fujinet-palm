#ifndef RP_POLICY_H
#define RP_POLICY_H
#ifdef RP_HOST
#include <stdint.h>
typedef uint32_t RPU32;
#else
#include <PalmOS.h>
typedef UInt32 RPU32;
#endif
#define RP_SETUP_NAME "FN RAM Setup P2"
#define RP_RUNTIME_NAME "FujiNet RAM P2"
#define RP_TEMP_NAME "FN RAM P2 Candidate"
#define RP_CREATOR 0x464e5241UL
#define RP_SETUP_CREATOR 0x48734364UL
#define RP_SETUP_TYPE 0x48735355UL
#define RP_APP_TYPE 0x6170706cUL
#define RP_VERSION 1
#define RP_FIELDS 32
/* Fixed BE receipt/PDB record. No native C layout is persisted. */
enum { RP_MAGIC,RP_FORMAT,RP_EVENT,RP_GENERATION,RP_MODULE,RP_RESET,RP_ERROR,
 RP_SOURCE_DB,RP_RUNTIME_DB,RP_SETUP_PC,RP_SETUP_BASE,RP_SETUP_SIZE,
 RP_SETUP_HEAP,RP_SETUP_HEAPFLAGS,RP_SETUP_CARD,RP_RUN_PC,RP_RUN_BASE,
 RP_RUN_SIZE,RP_RUN_HEAP,RP_RUN_HEAPFLAGS,RP_RUN_CARD,RP_LOOP_START,
 RP_LOOP_END,RP_LOOP_RESULT,RP_ACTIVE,RP_CONTEXT,RP_STAGE,RP_AUX_ERROR,
 RP_SOURCE_CRC,RP_RUN_CRC,RP_SECONDS,RP_RAM_CHECKS };
enum { RP_INSTALL_BEGIN=1,RP_INSTALL_END,RP_REMOVE,RP_RUNTIME,RP_LOOP };
enum { RP_OK,RP_BAD_SOURCE=0x9101,RP_COLLISION,RP_BUSY,RP_ALIAS,
 RP_BAD_CLONE,RP_LOG_ERROR,RP_BAD_RECEIPT,RP_BAD_RUNTIME };
#define RP_LAUNCH_FLAGS RP_CONTEXT
#define RP_CURRENT_DB RP_CONTEXT
struct rp_record { RPU32 v[RP_FIELDS]; };
typedef char rp_word_width[(sizeof(RPU32)==4)?1:-1];
typedef char rp_record_width[(sizeof(struct rp_record)==128)?1:-1];
struct rp_info {
 char name[32];
 RPU32 id,card,type,creator,version,attrs,base,size,heap,heapflags,ram,crc,busy;
};
struct rp_ops {
 int (*inspect)(void *,RPU32,struct rp_info *);
 RPU32 (*find)(void *,const char *);
 int (*copy)(void *,RPU32,RPU32 *);
 int (*publish)(void *,RPU32);
 int (*erase)(void *,RPU32);
 int (*save)(void *,const struct rp_record *);
 void *ctx;
};
void rp_clear(struct rp_record *);
void rp_encode(const struct rp_record *,unsigned char *);
int rp_decode(struct rp_record *,const unsigned char *,unsigned);
int rp_name_eq(const char *,const char *);
int rp_in_ram(const struct rp_info *,RPU32);
int rp_install(const struct rp_ops *,struct rp_record *,RPU32 source,RPU32 pc);
int rp_remove(const struct rp_ops *,struct rp_record *,RPU32 module);
#endif
