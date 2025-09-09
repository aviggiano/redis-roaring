# R.SETBIT

| Category            | Description                                                        |
| ------------------- | ------------------------------------------------------------------ |
| Syntax              | `R.GETBITS key offset [offset1 offset2 ... offsetN]`               |
| Time complexity     | O(C)                                                               |
| Supports structures | Bitmap32                                                           |
| Command description | Retrieves multiple values of the specified bit from a Roaring key. |

## Parameter

- **key**: The name of the Roaring bitmap key.
- **offset**: An integer that represents the offset of the bit to be set, with a value range of 0 ~ 2^32.

## Output

- If the operation is successful, the bit value is returned.
- If the key does not exist, an empty array is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```bash
$ redis-cli
127.0.0.1:6379> R.SETINTARRAY foo 1 2 5 6
OK
127.0.0.1:6379> R.GETBITS foo 2 3 4 5
(integer) 1
(integer) 0
(integer) 0
(integer) 1
```
