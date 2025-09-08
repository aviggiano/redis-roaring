# R.BITPOS

| Category            | Description                                        |
| ------------------- | -------------------------------------------------- |
| Syntax              | `R.BITPOS key bit`                                 |
| Time complexity     | O(C)                                               |
| Supports structures | Bitmap32                                           |
| Command description | Return the position of the first bit set to 1 or 0 |

## Parameter

- **key**: The key of the Roaring data structure.
- **bit**: The value of the bit whose offset you want to retrieve. The bit value can be 0 or 1.

## Output

- If the operation is successful, the offset of the bit that has a value of 1 or 0 is returned.
- If the key does not exist, a value of -1 is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.SETINTARRAY foo 3 5 6
127.0.0.1:6379> R.BITPOS foo 1
(integer) 3
```
