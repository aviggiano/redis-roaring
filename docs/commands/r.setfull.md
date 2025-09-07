# R.SETFULL

| Category            | Description                          |
| ------------------- | ------------------------------------ |
| Syntax              | `R.SETFULL key`                      |
| Time complexity     | O(N)                                 |
| Supports structures | Bitmap32                             |
| Command description | Fill up a roaring bitmap in integer. |

## Parameter

- **key**: The name of the Roaring bitmap key.

## Output

- If the operation is successful, `OK` is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.SETFULL foo
OK
127.0.0.1:6379> R.BITCOUNT foo
(integer) 4294967296
```
