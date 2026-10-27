#ifndef VT_CORE_H
#define VT_CORE_H
#ifdef VT_HOST
#include <stdint.h>
typedef uint32_t VTU32;
#else
#include <PalmOS.h>
typedef UInt32 VTU32;
#endif
#define VT_FIELDS 48
#define VT_BYTES (VT_FIELDS*4)
enum {V_MAGIC,V_VERSION,V_EVENT,V_SEQ,V_ERROR,V_CARD,V_GEN,V_SOURCE,V_RUNTIME,V_BASE,V_SIZE,V_ORIG,V_REQUEST,V_APPLIED,V_RESTORED,V_SETERR,V_GETERR,V_RESTOREERR,V_GUARDERR,V_ACCESSERR,V_W0,V_W1,V_LONG,V_FLAGS,V_PC,V_CODEBASE,V_CODESIZE,V_HEAP,V_HEAPFLAGS,V_RAM,V_CURDB,V_ROMVER,V_HSVER,V_CPU,V_OEMCO,V_OEMDEV,V_SECONDS,V_STAGE,V_IDERR,V_LOGERR,V_BUILD,V_REASON,V_R0,V_R1,V_R2,V_R3,V_R4,V_CRC};
enum {VT_PENDING=1,VT_RAM=2,VT_RAN=4,VT_RESTORED=8};
enum {VT_BAD=0x6301,VT_RANGE,VT_IDENTITY,VT_LOG,VT_READBACK,VT_DATA,VT_RESTORE};
struct vt_record {VTU32 v[VT_FIELDS];};
struct vt_ops {
 int (*guard)(void *,const struct vt_record *);
 int (*get)(void *,VTU32 *);
 int (*set)(void *,VTU32);
 int (*journal)(void *,const struct vt_record *,int);
 int (*loads)(void *,VTU32,VTU32 *,VTU32 *,VTU32 *);
 void *ctx;
 int (*ready)(void *,const struct vt_record *);
};
void vt_init(struct vt_record *);
VTU32 vt_crc(const unsigned char *,unsigned);
void vt_encode(const struct vt_record *,unsigned char *);
int vt_decode(struct vt_record *,const unsigned char *,unsigned);
int vt_run(struct vt_record *,const struct vt_ops *,int);
int vt_restore(struct vt_record *,const struct vt_ops *);
#endif
