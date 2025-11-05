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

## Operation Types

The valid forms to call the command are:

- `R.BITOP AND destkey srckey1 srckey2 srckey3 ... srckeyN`

  A bit in destkey is set only if it is set in **all** source bitmaps.

- `R.BITOP OR destkey srckey1 srckey2 srckey3 ... srckeyN`

  A bit in destkey is set if it is set in **at least one** source bitmap.

- `R.BITOP XOR destkey srckey1 srckey2 srckey3 ... srckeyN`

  Mostly used with two source bitmaps. A bit in destkey is set only if its value **differs** between the two source bitmaps.

- `R.BITOP NOT destkey srckey`

  NOT is a unary operator and only supports a **single** source bitmap. Sets each bit to the **inverse** of its value in the source bitmap.

- `R.BITOP ANDOR destkey X [Y1 Y2 ...]`

  A bit in destkey is set if it is set in **X** and **also** in one or more of Y1, Y2, ... (X ∩ (Y1 ∪ Y2 ∪ ...)).

- `R.BITOP ONE destkey X1 [X2 X3 ...]`

  A bit in destkey is set if it is set in **exactly one** of X1, X2, ... (symmetric difference for multiple sets).

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
