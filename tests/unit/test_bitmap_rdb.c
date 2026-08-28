#include "bitmap_rdb.h"
#include "../test-utils.h"

static void SetSerializedCardinality(char* serialized_bitmap, uint32_t cardinality) {
  memcpy(serialized_bitmap + 1, &cardinality, sizeof(cardinality));
}

void test_bitmap_rdb() {
  DESCRIBE("bitmap RDB bounds validation")
  {
    IT("Should accept a complete native array serialization")
    {
      char serialized_bitmap[17] = {1};
      SetSerializedCardinality(serialized_bitmap, 3);
      ASSERT(BitmapSerializedArrayFits(serialized_bitmap, sizeof(serialized_bitmap)),
             "three uint32_t values should fit in a 17-byte serialization");
    }

    IT("Should reject truncated cardinalities without overflowing size_t")
    {
      char serialized_bitmap[17] = {1};

      SetSerializedCardinality(serialized_bitmap, 4);
      ASSERT(!BitmapSerializedArrayFits(serialized_bitmap, sizeof(serialized_bitmap)),
             "four uint32_t values should not fit in a 17-byte serialization");

      SetSerializedCardinality(serialized_bitmap, UINT32_C(0x40000000));
      ASSERT(!BitmapSerializedArrayFits(serialized_bitmap, sizeof(serialized_bitmap)),
             "a cardinality whose byte size wraps 32-bit size_t must be rejected");

      SetSerializedCardinality(serialized_bitmap, UINT32_MAX);
      ASSERT(!BitmapSerializedArrayFits(serialized_bitmap, sizeof(serialized_bitmap)),
             "UINT32_MAX cardinality must be rejected");
    }

    IT("Should reject missing array headers and delegate other formats")
    {
      char short_array[] = {1};
      char container_format[] = {2};

      ASSERT(!BitmapSerializedArrayFits(NULL, 1), "NULL input must be rejected");
      ASSERT(!BitmapSerializedArrayFits(short_array, 0), "empty input must be rejected");
      ASSERT(!BitmapSerializedArrayFits(short_array, sizeof(short_array)),
             "a truncated array header must be rejected");
      ASSERT(BitmapSerializedArrayFits(container_format, sizeof(container_format)),
             "other formats should be delegated to CRoaring");
    }
  }
}
