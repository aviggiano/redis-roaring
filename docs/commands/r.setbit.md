# R.SETBIT

| Category            | Description                                                                                                                |
| ------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| Syntax              | `R.SETBIT key offset value`                                                                                                |
| Time complexity     | O(1)                                                                                                                       |
| Supports structures | Bitmap32                                                                                                                   |
| Command description | Sets the specified bit in a roaring key to a value of 1 or 0 and returns the original bit value. The offset starts from 0. |

## Parameter

- **key**: The name of the Roaring bitmap key.
- **offset**: An integer that represents the offset of the bit to be set, with a value range of 0 ~ 2^32.
- **value**: The bit value to be set, which can be either 1 or 0.

## Output

- If the operation is successful, a bit value of 0 or 1 is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```bash
$ redis-cli
127.0.0.1:6379> R.SETBIT foo 0 1
(integer) 0

# Bit already set
127.0.0.1:6379> R.SETBIT foo 0 1
(integer) 1
```
