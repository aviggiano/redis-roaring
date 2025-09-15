# R.JACCARD

| Category            | Description                                                                                                               |
| ------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| Syntax              | `R.JACCARD key1 key2`                                                                                                     |
| Time complexity     | O(M)                                                                                                                      |
| Supports structures | Bitmap32                                                                                                                  |
| Command description | Retrieves the Jaccard similarity coefficient of two Roaring keys. The higher the coefficient, the higher the similarity.. |

## Parameter

- **key1**: The name of the Roaring bitmap key.
- **key2**: The name of the Roaring bitmap key.

## Output

- If the operation is successful, a double-typed Jaccard similarity coefficient is returned.
- If both keys are empty `-1` is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```bash
$ redis-cli
127.0.0.1:6379> R.SETINTARRAY foo1 1 2 3
OK

127.0.0.1:6379> R.SETINTARRAY foo2 1 2 4
OK

R.JACCARD foo1 foo2
"0.5"
```
