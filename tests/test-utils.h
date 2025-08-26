#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>

// ANSI color codes for prettier output
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_GREY    "\x1b[90m"
#define COLOR_BOLD    "\x1b[1m"
#define COLOR_RESET   "\x1b[0m"

typedef struct {
  int test_count;
  int test_failed;
  int test_passed;
} test_statistics_t;

typedef struct {
  const char* name;
  int depth;
  int test_count;
  int test_failed;
  double start_at;
} test_describe_t;

typedef struct {
  const char* name;
  bool failed;
  int depth;
  double start_at;
} test_it_t;

static int test_current_depth = 0;
static int test_total_count = 0;
static int test_total_failed = 0;
static double test_start_at = 0;
static test_describe_t* test_current_describe = NULL;
static test_it_t* test_current_it = NULL;

static char* test_current_buffer = NULL;
static size_t buffer_size = 0;
static size_t buffer_used = 0;

static inline double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static inline char* get_test_padding(void) {
  static char padding[256];
  int spaces = test_current_depth * 2;

  if (spaces > 255) spaces = 255;

  memset(padding, ' ', spaces);
  padding[spaces] = '\0';

  return padding;
}

static inline void init_test_buffer(size_t initial_size) {
  buffer_size = initial_size;
  test_current_buffer = malloc(buffer_size);
  if (test_current_buffer) {
    test_current_buffer[0] = '\0';
    buffer_used = 0;
  }
}

static inline void printf_test(const char* format, ...) {
  va_list args;
  va_start(args, format);

  // Calculate needed space
  va_list args_copy;
  va_copy(args_copy, args);
  int needed = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);

  // Grow buffer if needed
  if (buffer_used + needed + 1 > buffer_size) {
    buffer_size = (buffer_used + needed + 1) * 2;
    test_current_buffer = realloc(test_current_buffer, buffer_size);
  }

  // Append the string
  if (test_current_buffer) {
    vsnprintf(test_current_buffer + buffer_used, buffer_size - buffer_used, format, args);
    buffer_used += needed;
  }

  va_end(args);
}

// Print all buffered output and cleanup
static inline void flush_test_buffer(void) {
  if (test_current_buffer) {
    printf("%s", test_current_buffer);
    free(test_current_buffer);
    test_current_buffer = NULL;
    buffer_size = 0;
    buffer_used = 0;
  }
}

static inline void reset_test_buffer(void) {
  if (test_current_buffer) {
    free(test_current_buffer);
    test_current_buffer = NULL;
    buffer_size = 0;
    buffer_used = 0;
  }
}

static inline void increment_test_failed() {
  if (test_current_it != NULL && !test_current_it->failed) {
    test_current_it->failed = true;
    test_current_describe->test_failed++;
    test_total_failed++;
  }
}

static inline void before_describe(char* name) {
  // printf(COLOR_BLUE "%s========== %s ==========\n" COLOR_RESET, get_test_padding(), name);

  test_describe_t* describe = malloc(sizeof(test_describe_t));
  *describe = (test_describe_t){
      .name = name,
      .depth = ++test_current_depth,
      .test_count = 0,
      .test_failed = 0,
      .start_at = now_ms(),
  };

  test_current_describe = describe;
}

static inline void after_describe() {
  char* padding = get_test_padding();

  // printf(COLOR_GREY "%sDuration %.3fms\n" COLOR_RESET, padding, now_ms() - test_start_at);

  test_current_depth--;

  int count = test_current_describe->test_count;
  int failed = test_current_describe->test_failed;
  int passed = count - failed;

  // if (failed > 0) {
  //   printf(COLOR_GREY "%s  Failed " COLOR_RED COLOR_BOLD "%d failed" COLOR_GREY " | " COLOR_GREEN "passed %d" COLOR_GREY " (%d)\n" COLOR_RESET, padding, failed, passed, count);
  // }
}

static inline void before_it(char* name) {
  test_it_t* it = malloc(sizeof(test_it_t));

  *it = (test_it_t){
    .name = name,
    .depth = ++test_current_depth,
    .failed = false,
    .start_at = now_ms(),
  };

  test_current_it = it;
  test_current_describe->test_count++;
  test_total_count++;
}

static inline void after_it() {
  test_current_depth--;

  if (!test_current_it->failed) {
    printf("%s" COLOR_GREEN "✓" COLOR_RESET " %s -> %s",
      get_test_padding(),
      test_current_describe->name,
      test_current_it->name
    );
    printf(COLOR_GREY " %.3fms\n" COLOR_RESET, now_ms() - test_start_at);
    reset_test_buffer();
  } else {
    printf("%s" COLOR_RED "✗" COLOR_RESET " %s -> %s",
      get_test_padding(),
      test_current_describe->name,
      test_current_it->name
    );
    printf(COLOR_GREY " %.3fms\n" COLOR_RESET, now_ms() - test_start_at);
    flush_test_buffer();
  }
}

static inline void test_start() {
  test_start_at = now_ms();
}

static inline void test_end() {
  int count = test_total_count;
  int failed = test_total_failed;
  int passed = count - failed;

  printf(COLOR_GREY"\n-----------------------------\n\n"COLOR_RESET);

  if (failed == 0) {
    printf("   Tests " COLOR_GREEN COLOR_BOLD "%d passed" COLOR_GREY " (%d)\n" COLOR_RESET, passed, count);
  } else {
    printf("   Tests " COLOR_RED COLOR_BOLD "%d failed" COLOR_GREY " | " COLOR_GREEN "passed %d" COLOR_GREY " (%d)\n" COLOR_RESET, failed, passed, count);
  }

  printf("Duration " "%.3fms\n", now_ms() - test_start_at);

  printf("\n");
}

// Test suite macros
#define DESCRIBE(name) \
  before_describe(name); \
  for(int _once = 1; _once; _once = 0, after_describe())

#define IT(name) \
  before_it(name); \
  for(int _once = 1; _once; _once = 0, after_it())

  // Basic assertion macros
#define ASSERT(condition, ...) \
    do { \
        if (!(condition)) { \
            printf_test("%s" COLOR_RED "✗" COLOR_RESET " ASSERT(" #condition ") - FAILED\n  ", get_test_padding()); \
            printf_test(__VA_ARGS__); \
            printf_test("\n"); \
            increment_test_failed(); \
            assert(condition); \
        } \
    } while(0)

// Basic assertion macros
#define ASSERT_TRUE(condition) \
    do { \
        if (condition) { \
            printf_test("%s" COLOR_GREEN "✓" COLOR_RESET " ASSERT_TRUE(" #condition ")\n", get_test_padding()); \
        } else { \
            printf_test("%s" COLOR_RED "✗" COLOR_RESET " ASSERT_TRUE(" #condition ") - FAILED\n", get_test_padding()); \
            increment_test_failed(); \
            assert(!(condition)); \
            break; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (!(condition)) { \
            printf_test("%s" COLOR_GREEN "✓" COLOR_RESET " ASSERT_FALSE(" #condition ")\n", get_test_padding()); \
        } else { \
            printf_test("%s" COLOR_RED "✗" COLOR_RESET " ASSERT_FALSE(" #condition ") - FAILED\n", get_test_padding()); \
            increment_test_failed(); \
            assert(!(condition)); \
        } \
    } while(0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf_test("%s" COLOR_GREEN "✓" COLOR_RESET " ASSERT_NULL(" #ptr ")\n", get_test_padding()); \
        } else { \
            printf_test("%s" COLOR_RED "✗" COLOR_RESET " ASSERT_NULL(" #ptr ") - FAILED (got %p)\n", get_test_padding(), (void*)(ptr)); \
            increment_test_failed(); \
            assert((ptr) == NULL); \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            printf_test("%s" COLOR_GREEN "✓" COLOR_RESET " ASSERT_NOT_NULL(" #ptr ")\n", get_test_padding()); \
        } else { \
            printf_test("%s" COLOR_RED "✗" COLOR_RESET " ASSERT_NOT_NULL(" #ptr ") - FAILED (got NULL)\n", get_test_padding()); \
            increment_test_failed(); \
            assert((ptr) != NULL); \
        } \
    } while(0)

// Equality assertion macros
#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) == (actual)) { \
            printf_test("%s" COLOR_GREEN "✓" COLOR_RESET " ASSERT_EQ(", get_test_padding()); \
            printf_test(GET_FORMAT_SPECIFIER(expected), (expected)); \
            printf_test(", "); \
            printf_test(GET_FORMAT_SPECIFIER(actual), (actual)); \
            printf_test(")\n"); \
        } else { \
            printf_test("%s" COLOR_RED "✗" COLOR_RESET " ASSERT_EQ(", get_test_padding()); \
            printf_test(GET_FORMAT_SPECIFIER(expected), (expected)); \
            printf_test(", "); \
            printf_test(GET_FORMAT_SPECIFIER(actual), (actual)); \
            printf_test(") - FAILED\n"); \
            increment_test_failed(); \
            assert((expected) == (actual)); \
        } \
    } while(0)

// Equality assertion macros
#define ASSERT_NOT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf_test("%s" COLOR_GREEN "✓" COLOR_RESET " ASSERT_NOT_EQ(", get_test_padding()); \
            printf_test(GET_FORMAT_SPECIFIER(expected), (expected)); \
            printf_test(", "); \
            printf_test(GET_FORMAT_SPECIFIER(actual), (actual)); \
            printf_test(")\n"); \
        } else { \
            printf_test("%s" COLOR_RED "✗" COLOR_RESET " ASSERT_NOT_EQ(", get_test_padding()); \
            printf_test(GET_FORMAT_SPECIFIER(expected), (expected)); \
            printf_test(", "); \
            printf_test(GET_FORMAT_SPECIFIER(actual), (actual)); \
            printf_test(") - FAILED\n"); \
            increment_test_failed(); \
            assert((expected) == (actual)); \
        } \
    } while(0)

// String assertion macros
#define ASSERT_EQ_STR(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) == 0) { \
            printf_test("%s" COLOR_GREEN "✓" COLOR_RESET " ASSERT_EQ_STR(\"%s\", \"%s\")\n", (expected), (actual), get_test_padding()); \
        } else { \
            printf_test("%s" COLOR_RED "✗" COLOR_RESET " ASSERT_EQ_STR(\"%s\", \"%s\") - FAILED\n", (expected), (actual), get_test_padding()); \
            increment_test_failed(); \
            assert(strcmp((expected), (actual)) == 0); \
        } \
    } while(0)

// Generic format specifier detection
#define GET_FORMAT_SPECIFIER(val) _Generic((val), \
    char: "%c", \
    signed char: "%d", \
    unsigned char: "%u", \
    short: "%d", \
    unsigned short: "%u", \
    int: "%d", \
    unsigned int: "%u", \
    long: "%ld", \
    unsigned long: "%lu", \
    long long: "%lld", \
    unsigned long long: "%llu", \
    float: "%f", \
    double: "%f", \
    long double: "%Lf", \
    char*: "%s", \
    void*: "%p", \
    default: "%p")

// Generic array printing macro
#define PRINT_ARRAY(array, length, name) \
    do { \
        printf_test("%sArray %s[%zu]:\t[", get_test_padding(), name, (size_t)(length)); \
        for (size_t _i = 0; _i < (length); _i++) { \
            printf_test(GET_FORMAT_SPECIFIER((array)[0]), (array)[_i]); \
            if (_i < (length) - 1) printf(", "); \
        } \
        printf_test("]\n"); \
    } while(0)

// Auto-length version - works with static arrays
#define PRINT_ARRAY_AUTO(array, name) \
    PRINT_ARRAY(array, ARRAY_LENGTH(array), name)

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

// Array range assertion - check if array values are all zero from a given index
#define ASSERT_ARRAY_RANGE_ZERO(array, start_index, length) \
    do { \
        int _all_zero = 1; \
        for (size_t _i = (start_index); _i < (start_index) + (length); _i++) { \
            if ((array)[_i] != 0) { \
                printf_test("%s" COLOR_RED "✗" COLOR_RESET " ASSERT_ARRAY_RANGE_ZERO - FAILED at index %zu: expected 0, got ", get_test_padding(), _i); \
                printf_test(GET_FORMAT_SPECIFIER((array)[_i]), (array)[_i]); \
                printf_test("\n"); \
                _all_zero = 0; \
                increment_test_failed(); \
                break; \
            } \
        } \
        if (_all_zero) { \
            printf_test("%s" COLOR_GREEN "✓" COLOR_RESET " ASSERT_ARRAY_RANGE_ZERO [%zu..%zu]\n", get_test_padding(), \
                   (size_t)(start_index), (size_t)(start_index) + (size_t)(length) - 1); \
        } \
        assert(_all_zero); \
    } while(0)

// Generic conditional print with explicit lengths
#define PRINT_ARRAYS_ON_MISMATCH(expected, actual, exp_len, act_len, desc) \
    do { \
        printf_test("%sArrays don't match %s\n", get_test_padding(), desc); \
        PRINT_ARRAY(expected, _exp_len, "expected"); \
        PRINT_ARRAY(actual, _act_len, "actual"); \
    } while(0)

// Advanced array assertions with explicit lengths (supports different lengths)
#define ASSERT_ARRAY_EQ(expected, actual, exp_len, act_len) \
    do { \
        size_t _exp_len = (size_t)(exp_len); \
        size_t _act_len = (size_t)(act_len); \
        int _arrays_equal = 1; \
        \
        /* Check if lengths match */ \
        if (_exp_len != _act_len) { \
            printf_test("%s" COLOR_RED "✗" COLOR_RESET " ASSERT_ARRAY_EQ - FAILED (length mismatch: expected=%zu, actual=%zu)\n", get_test_padding(), _exp_len, _act_len); \
            PRINT_ARRAYS_ON_MISMATCH(expected, actual, _exp_len, _act_len, "due length comparison"); \
            _arrays_equal = 0; \
            increment_test_failed(); \
        } else { \
            /* Check elements */ \
            for (size_t _i = 0; _i < _exp_len; _i++) { \
                if ((expected)[_i] != (actual)[_i]) { \
                    printf_test("%s" COLOR_RED "✗" COLOR_RESET " ASSERT_ARRAY_EQ - FAILED\n", get_test_padding()); \
                    char *desc;\
                    asprintf(&desc, "at index %zu", _i);\
                    PRINT_ARRAYS_ON_MISMATCH(expected, actual, _exp_len, _act_len, desc); \
                    free(desc); \
                    _arrays_equal = 0; \
                    increment_test_failed(); \
                    break; \
                } \
            } \
        } \
        \
        if (_arrays_equal) { \
            printf_test("%s" COLOR_GREEN "✓" COLOR_RESET " ASSERT_ARRAY_EQ (all %zu elements match)\n", get_test_padding(), _exp_len); \
        } else { \
          assert(_arrays_equal); \
        } \
    } while(0)

// Memory cleanup helper
#define SAFE_FREE(ptr) \
    do { \
        if (ptr) { \
            free(ptr); \
            ptr = NULL; \
        } \
    } while(0)

#endif // TEST_UTILS_H
