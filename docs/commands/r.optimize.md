# R.OPTIMIZE

| Category            | Description                                                                                                                                                       |
| ------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Syntax              | `R.OPTIMIZE key`                                                                                                                                                  |
| Time complexity     | O(M)                                                                                                                                                              |
| Supports structures | Bitmap32                                                                                                                                                          |
| Command description | Optimizes the storage of a Roaring key. If the key is relatively large and is used mainly for read operations after the key is created, you can run this command. |

## Parameter

- **key**: The name of the Roaring bitmap key.

## Output

- If the operation is successful, OK is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.OPTIMIZE foo
OK
```
