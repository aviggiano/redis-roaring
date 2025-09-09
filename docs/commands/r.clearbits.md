# R.CLEARBITS

| Category            | Description                                                            |
| ------------------- | ---------------------------------------------------------------------- |
| Syntax              | `R.CLEARBITS key offset [offset1 offset2 ... offsetN] [COUNT]` [COUNT] |
| Time complexity     | O(C)                                                                   |
| Supports structures | Bitmap32                                                               |
| Command description | Sets the value of the specified bit in a Roaring key to 0.             |

## Parameter

- **key**: The name of the Roaring bitmap key.
- **offset**: An integer that represents the offset of the bit to be set, with a value range of 0 ~ 2^32.
- **COUNT**: if this parameter is specified, the number of bits in the key that have been set to 0 is returned.

## Output

- If the operation is successful, the bit value is returned.
- If the key does not exist, `nil` is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```bash
$ redis-cli
127.0.0.1:6379> R.SETINTARRAY foo 1 2 3
OK
127.0.0.1:6379> R.CLEARBITS foo 1 3
OK
127.0.0.1:6379> R.GETINTARRAY foo
(integer) 2
```

## Usage Notes

- When `COUNT` parameter is specified, the operation may works slower for large dataset.
