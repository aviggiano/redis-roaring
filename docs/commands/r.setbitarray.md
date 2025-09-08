# R.SETBITARRAY

| Category            | Description                                                    |
| ------------------- | -------------------------------------------------------------- |
| Syntax              | `R.SETBITARRAY key value`                                      |
| Time complexity     | O(C)                                                           |
| Supports structures | Bitmap32                                                       |
| Command description | Creates a Roaring key based on the specified bit array string. |

## Parameter

- **key**: The key of the Roaring data structure.
- **value**: A string of 0s and 1s that represents the bit array.

## Output

- If the operation is successful, `OK` is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.SETBITARRAY foo 10101001
OK
```

## Usage Notes

- If the key already exists, this command overwrites the data in the key
