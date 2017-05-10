#ifndef REROARING_DATA_STRUCTURE_H
#define REROARING_DATA_STRUCTURE_H

#include "roaring.h"

typedef roaring_bitmap_t Bitmap;

Bitmap* bitmap_alloc();
void bitmap_free(Bitmap* bitmap);
void bitmap_setbit(Bitmap* bitmap, uint32_t offset, char value);
char bitmap_getbit(Bitmap* bitmap, uint32_t offset);

#endif