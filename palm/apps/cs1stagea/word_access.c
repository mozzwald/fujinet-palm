#include <PalmOS.h>
UInt16 va_read(UInt32 base,UInt16 offset) {
 return *(volatile UInt16 *)(base+(UInt32)offset);
}
void va_write(UInt32 base,UInt16 offset,UInt16 value) {
 *(volatile UInt16 *)(base+(UInt32)offset)=value;
}
