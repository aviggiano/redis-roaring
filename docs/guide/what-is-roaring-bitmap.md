# What is Roaring Bitmap?

Roaring Bitmap is a compressed bitmap data structure that efficiently stores and manipulates sets of 32-bit integers. It's designed to be both memory-efficient and fast, making it ideal for applications that need to work with large sets of integers, such as database indexing, search engines, and analytics systems.

To read more about [Roaring Bitmaps](https://roaringbitmap.org/)

## Key Features

### 🚀 **High Performance**

- Extremely fast set operations (union, intersection, difference)
- Optimized for both sparse and dense datasets
- SIMD-accelerated operations on modern processors

### 💾 **Memory Efficient**

- Adaptive compression based on data density
- Often 2-10x smaller than uncompressed bitmaps
- Minimal memory overhead

### 🔧 **Versatile Operations**

- Complete set of bitmap operations
- Range queries and bulk operations
- Serialization and deserialization support

## How It Works

Roaring Bitmap uses a hybrid approach that automatically chooses the best representation based on data characteristics:

### Three Container Types

1. **Array Container** (for sparse data)

   - Stores integers directly in a sorted array
   - Used when fewer than 4,096 values are present
   - Optimal for sparse datasets

2. **Bitmap Container** (for dense data)

   - Traditional bitmap representation
   - Used when more than 4,096 values are present
   - Efficient for dense datasets

3. **Run Container** (for consecutive sequences)
   - Stores runs of consecutive integers
   - Automatically detects and compresses sequences
   - Ideal for data with long consecutive runs

### Adaptive Structure

```
32-bit integer: [16-bit high][16-bit low]
                     ↓           ↓
                   chunk      container
```

- Divides 32-bit space into 65,536 chunks of 65,536 integers each
- Each chunk uses the most efficient container type
- Automatically adapts as data changes

## Use Cases

### 🔍 **Search Engines**

- Inverted indexes for document retrieval
- Query result filtering and ranking
- Faceted search implementations

### 📊 **Analytics & BI**

- User segmentation and cohort analysis
- Event filtering and aggregation
- Real-time dashboard queries

### 🗄️ **Databases**

- Bitmap indexes for fast lookups
- Join optimization
- Columnar storage compression

### 🎯 **Ad Tech**

- Audience targeting and segmentation
- Campaign optimization
- Real-time bidding filters
