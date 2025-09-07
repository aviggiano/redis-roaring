# R.SETINTARRAY

| Category            | Description                                                 |
| ------------------- | ----------------------------------------------------------- |
| Syntax              | `R.SETINTARRAY key value [value1 value2 ... valueN]`        |
| Time complexity     | O(C)                                                        |
| Supports structures | Bitmap32                                                    |
| Command description | Creates a Roaring key based on the specified integer array. |

## Parameter

- **key**: The name of the Roaring bitmap key.
- **value**: The integer offset of the bit that you want to process. Valid values: 0 to 4294967296.

## Output

- If the operation is successful, `OK` is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.SETINTARRAY foo 1 3
OK
```

## Usage Notes

- If the key already exists, this command overwrites the data in the key.
- If the key doesn't exist, a new Roaring bitmap is created automatically.
