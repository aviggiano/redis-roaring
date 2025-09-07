# R.MIN

| Category            | Description                                                                       |
| ------------------- | --------------------------------------------------------------------------------- |
| Syntax              | `R.DIFF destkey key1 key2`                                                        |
| Time complexity     | O(C \* M)                                                                         |
| Supports structures | Bitmap32                                                                          |
| Command description | Performs difference operation on Roaring Bitmaps and stores the result in destkey |

## Parameter

- **destkey**: The destination key that stores the result (Roaring data structure).
- **key**: The key of the Roaring data structure.

## Output

- If the operation is successful, `OK` is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.SETINTARRAY foo1 3 4 5
OK
127.0.0.1:6379> R.SETINTARRAY foo2 1 4 6
OK
127.0.0.1:6379> R.DIFF foo_dest foo1 foo2
OK
127.0.0.1:6379> R.GETINTARRAY foo_dest
1) (integer) 3
2) (integer) 5
(integer) 5
```
