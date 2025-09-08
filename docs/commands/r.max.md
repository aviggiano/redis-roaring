# R.MIN

| Category            | Description                                                                  |
| ------------------- | ---------------------------------------------------------------------------- |
| Syntax              | `R.MAX key`                                                                  |
| Time complexity     | O(1)                                                                         |
| Supports structures | Bitmap32                                                                     |
| Command description | Retrieves the offset of the last bit that has a value of 1 in a Roaring key. |

## Parameter

- **key**: The name of the Roaring bitmap key.

## Output

- If the operation is successful:
  - If bits that have a value of 1 exist in the key, the integer offset of the last bit that has a value of 1 is returned.
  - If no bits that have a value of 1 exist in the key or if the key does not exist, a value of -1 is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.SETINTARRAY foo 3 4 5
OK
127.0.0.1:6379> R.MAX foo
(integer) 5
```
