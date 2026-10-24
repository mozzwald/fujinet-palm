#include "image_read.h"

static unsigned long crc_byte(unsigned long crc, unsigned char value) {
  unsigned bit;
  crc ^= value;
  for (bit = 0; bit < 8; ++bit)
    crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
  return crc;
}
struct visor_image_read visor_image_begin(const struct visor_read_ops *ops,
    unsigned short card, unsigned long length, const unsigned char *expected) {
  struct visor_image_read s;
  s.guard = visor_read_check(ops, card);
  s.length = length;
  s.completed = 0;
  s.crc = 0xffffffffUL;
  s.expected = expected;
  s.mismatch_count = 0;
  if (s.guard.reason == visorReadReady &&
      (!length || (length & 1) || length > 65536 || length > s.guard.size))
    s.guard.reason = visorReadBadMapping;
  return s;
}
unsigned long visor_image_local_crc(const unsigned char *data, unsigned long length) {
  unsigned long crc = 0xffffffffUL, i;
  for (i = 0; i < length; ++i) crc = crc_byte(crc, data[i]);
  return crc ^ 0xffffffffUL;
}
void visor_image_step(struct visor_image_read *s,
    const struct visor_read_ops *ops, unsigned short card) {
  struct visor_read_result current;
  unsigned short word;
  if (s->guard.reason != visorReadReady || s->completed >= s->length) return;
  current = visor_read_check(ops, card);
  if (current.reason != visorReadReady) {
    s->guard.reason = current.reason;
    s->guard.error = current.error;
    s->guard.attribute = current.attribute;
    return;
  }
  if (current.base != s->guard.base || current.size != s->guard.size) {
    s->guard.reason = visorReadMappingChanged;
    return;
  }
  s->guard.error = ops->read_word(ops->context,
      s->guard.base + s->completed, &word);
  if (s->guard.error) { s->guard.reason = visorReadAccessFailed; return; }
  visor_image_accept_word(s, word);
}
void visor_image_accept_word(struct visor_image_read *s, unsigned short word) {
  if (s->guard.reason != visorReadReady || s->completed >= s->length) return;
  if (s->expected) {
    unsigned short expected = (unsigned short)(
        ((unsigned short)s->expected[s->completed] << 8) |
        s->expected[s->completed + 1]);
    if (word != expected) {
      if (s->mismatch_count < 8) {
        unsigned i = (unsigned)s->mismatch_count;
        s->first[i].offset = s->completed;
        s->first[i].expected = expected;
        s->first[i].actual = word;
      }
      ++s->mismatch_count;
    }
  }
  s->crc = crc_byte(crc_byte(s->crc, (unsigned char)(word >> 8)),
                    (unsigned char)word);
  s->completed += 2;
}
