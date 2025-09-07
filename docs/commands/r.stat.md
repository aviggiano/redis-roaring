# R.STAT

| Category            | Description                                                                                                                |
| ------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| Syntax              | `R.STAT key [JSON]`                                                                                                        |
| Time complexity     | O(1)                                                                                                                       |
| Supports structures | Bitmap32, Bitmap64                                                                                                         |
| Command description | Returns statistical information about a Roaring bitmap, including container counts, memory usage, and cardinality metrics. |

## Parameter

- **key**: The name of the Roaring bitmap key.
- **JSON**: if this parameter is specified, the statistics information is returned in JSON format.

## Output

- If the operation is successful, the expected return value is described here.
- If the key does not exist, `nil` is returned.
- Otherwise, an error message is returned.

## Examples

### Basic Usage

```
$ redis-cli
127.0.0.1:6379> R.STAT mykey

type: bitmap
cardinality: 6
number of containers: 1
max value: 6
min value: 1
number of array containers: 1
  array container values: 6
  array container bytes: 12
bitset  containers: 0
  bitset  container values: 0
  bitset  container bytes: 0
run containers: 0
  run container values: 0
  run container bytes: 0
```

### JSON Format

```
$ redis-cli
127.0.0.1:6379> R.STAT mykey JSON

{
  "type": "bitmap",
  "cardinality": "6",
  "number_of_containers": "1",
  "max_value": "6",
  "min_value": "1",
  "array_container": {
    "number_of_containers": "1",
    "container_cardinality": "6",
    "container_allocated_bytes": "12"
  },
  "bitset_container": {
    "number_of_containers": "0",
    "container_cardinality": "0",
    "container_allocated_bytes": "0"
  },
  "run_container": {
    "number_of_containers": "0",
    "container_cardinality": "0",
    "container_allocated_bytes": "0"
  }
}
```

## Usage Notes

- Supports both Bitmap32 and Bitmap64 bitmap key.
- JSON output returns numbers as strings to avoid JavaScript precision limits (values > 2^53 - 1) due to IEEE 754 standard
