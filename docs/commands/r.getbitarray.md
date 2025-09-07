# R.GETBITARRAY

| Category            | Description                                                                |
| ------------------- | -------------------------------------------------------------------------- |
| Syntax              | `R.GETBITARRAY key`                                                        |
| Time complexity     | O(C)                                                                       |
| Supports structures | Bitmap32                                                                   |
| Command description | Retrieves a string that consists of bit values of 0 and 1 in a Roaring key |

## Parameter

- **key**: The name of the Roaring bitmap key.

## Output

- If the operation is successful, the string of bit array are returned.
- If the key does not exist, a empty string is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.SETBITARRAY foo 101101
127.0.0.1:6379> R.GETBITARRAY foo
"101101"
```
