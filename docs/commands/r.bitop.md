# R.BITOP

| Category            | Description                                                                  |
| ------------------- | ---------------------------------------------------------------------------- |
| Syntax              | `R.BITOP <AND, OR, XOR, NOT, ANDOR, ONE> destkey key [key1 key2 ... keyN]`   |
| Time complexity     | O(C)                                                                         |
| Supports structures | Bitmap32                                                                     |
| Command description | Performs set operations on Roaring Bitmaps and stores the result in destkey. |

## Parameter

- **operation**: The type of set operation.
- **destkey**: The destination key that stores the result (Roaring data structure).
- **key**: The key of the Roaring data structure. You can specify multiple keys.

## Output

- If the operation is successful, the integer number of bits in the result that are set to 1 is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.BITOP OR destkey srckey1 srckey2 srckey3
(integer) 6
```

## Usage Notes

- Empty source keys are automatically skipped during the bitwise operation
- All keys must be of the same Roaring bitmap type (Bitmap32)
- The operation creates the destination key if it doesn't exist
