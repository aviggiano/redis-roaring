# R.CLEAR

| Category            | Description          |
| ------------------- | -------------------- |
| Syntax              | `R.CLEAR key offset` |
| Time complexity     | O(M)                 |
| Supports structures | Bitmap32             |
| Command description | Cleanup Roaring key. |

## Parameter

- **key**: The name of the Roaring bitmap key.

## Output

- If the operation is successful, the integer number of removed bits is returned.
- If the key does not exist, nil is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```bash
$ redis-cli
127.0.0.1:6379> R.SETINTARRAY foo 1 2 3
127.0.0.1:6379> R.CLEAR foo
(integer) 3
```
