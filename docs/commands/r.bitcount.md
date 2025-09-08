# R.BITCOUNT

| Category            | Description                                      |
| ------------------- | ------------------------------------------------ |
| Syntax              | `R.BITCOUNT key`                                 |
| Time complexity     | O(M)                                             |
| Supports structures | Bitmap32                                         |
| Command description | Counts the number of bits that have a value of 1 |

## Parameter

- **key**: The key of the Roaring data structure.

## Output

- If the operation is successful, the integer number of bits in the result that are set to 1 is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.SETINTARRAY foo 1 2 3
127.0.0.1:6379> R.BITCOUNT foo
(integer) 3
```
