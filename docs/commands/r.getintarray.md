# R.GETINTARRAY

| Category            | Description                                   |
| ------------------- | --------------------------------------------- |
| Syntax              | `R.GETINTARRAY key`                           |
| Time complexity     | O(C)                                          |
| Supports structures | Bitmap32                                      |
| Command description | Return an integer array from a roaring bitmap |

## Parameter

- **key**: The key of the Roaring data structure.

## Output

- If the operation is successful, the offset of the bit that has a value of 1 or 0 is returned.
- If the key does not exist, a empty array is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.SETINTARRAY foo 1 2 3
127.0.0.1:6379> R.GETINTARRAY foo
1) (integer) 1
2) (integer) 2
3) (integer) 3
```
