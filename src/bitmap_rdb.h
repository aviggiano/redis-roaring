#ifndef REDIS_ROARING_BITMAP_RDB_H
#define REDIS_ROARING_BITMAP_RDB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline bool BitmapSerializedArrayFits(const char* serialized_bitmap, size_t size) {
  /* CRoaring's native format uses tag 1 followed by a native-endian uint32_t
   * cardinality and that many uint32_t values. Its pinned safe deserializer
   * computes the required size with size_t arithmetic that can wrap on 32-bit
   * builds, so validate this format with subtraction before calling it. */
  const uint8_t array_uint32_tag = 1;
  const size_t header_size = 1 + sizeof(uint32_t);

  if (serialized_bitmap == NULL || size == 0) {
    return false;
  }
  if ((uint8_t) serialized_bitmap[0] != array_uint32_tag) {
    return true; /* Let CRoaring validate its other serialization formats. */
  }
  if (size < header_size) {
    return false;
  }

  uint32_t cardinality;
  memcpy(&cardinality, serialized_bitmap + 1, sizeof(cardinality));
  return (size_t) cardinality <= (size - header_size) / sizeof(uint32_t);
}

#endif
