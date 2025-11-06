# R.RANGEINTARRAY

| Category            | Description                                                                        |
| ------------------- | ---------------------------------------------------------------------------------- |
| Syntax              | `R.RANGEINTARRAY key start end`                                                    |
| Time complexity     | O(C)                                                                               |
| Supports structures | Bitmap32                                                                           |
| Command description | Returns the offsets of the bits that have a value of 1 within the specified range. |

## Parameter

- **key**: The name of the Roaring bitmap key.
- **start**: The start offset in the interval (inclusive). The offset is zero-indexed, where 0 refers to the lowest set bit.
- **end**: The end offset in the interval (inclusive). The offset is zero-indexed, where 0 refers to the lowest set bit.

## Output

- If the operation is successful, the offsets of the bits that have a value of 1 are returned.
- If the key does not exist, an empty array is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.SETINTARRAY foo 1 2 3 5 6 7 8 9 10
127.0.0.1:6379> R.RANGEINTARRAY foo 0 5
1) (integer) 1
2) (integer) 2
3) (integer) 3
4) (integer) 5
5) (integer) 6
6) (integer) 7
```

## Usage Notes

- The maximum range size is limited to `100,000,000 elements` to prevent excessive memory usage
