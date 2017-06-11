#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "hiredis.h"
#include "benchmarks/numbersfromtextfiles.h"

/*
 * macros
 */
//#define debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define debug(fmt, ...)

/*
 * function declarations
 */
void print(const char* fmt, ...);
redisContext* create_context();
void print_header();
void timer_ns(const char* operation, size_t N);
int main(int argc, char* argv[]);

/*
 * function definitions
 */
void print(const char* fmt, ...) {
  static FILE* fp = NULL;
  if (fp == NULL) {
    fp = fopen("/tmp/test.out", "w");
    assert(fp != NULL);
  }

  va_list args_stdout;
  va_list args_fp;
  va_start(args_stdout, fmt);
  va_start(args_fp, fmt);
  vfprintf(stdout, fmt, args_stdout);
  vfprintf(fp, fmt, args_fp);
  va_end(args_stdout);
  va_end(args_fp);
}

redisContext* create_context() {
  redisContext* c;
  const char hostname[] = "127.0.0.1";
  int port = 6379;

  struct timeval timeout = {2, 0};
  c = redisConnectWithTimeout(hostname, port, timeout);
  assert(c != NULL);
  assert(c->err == 0);
  return c;
}

void print_header() {
  print("| %12s ", "OP");
  print("| %8s ", "TIME (s)");
  print("| %12s |\n", "TIME/OP (us)");

  print("| %12s ", "------------");
  print("| %8s ", "--------");
  print("| %12s |\n", "------------");
}

void timer_ns(const char* operation, size_t N) {
  static struct timespec start = {0, 0};
  static bool ticking = false;
  if (!ticking) {
    clock_gettime(CLOCK_MONOTONIC, &start);
    ticking = true;
    return;
  }
  ticking = false;

  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  unsigned long ns = (end.tv_sec - start.tv_sec) * 1000000000UL + (end.tv_nsec - start.tv_nsec);

  double us_per_op = 1E-3 * ns / N;
  print("| %12s ", operation);
  print("| %8.2f ", 1E-9 * ns);
  print("| %12.2f |\n", us_per_op);
}

int main(int argc, char* argv[]) {
  size_t count;
  size_t* howmany = NULL;
  uint32_t** numbers = read_all_integer_files("./deps/CRoaring/benchmarks/realdata/census1881",
                                              ".txt", &howmany, &count);

  assert(numbers != NULL);

  print_header();
  redisContext* c = create_context();

  {
    const char* ops[] = {
      "R.SETBIT",
      "SETBIT"
    };

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      size_t N = 0;
      timer_ns(ops[op], N);
      const int bits[] = {1, 0, 1};
      for (size_t b = 0; b < sizeof(bits) / sizeof(*bits); b++) {
        for (size_t i = 0; i < count; i++) {
          for (size_t j = 0; j < howmany[i]; j++) {
            redisReply* reply = redisCommand(c, "%s %d-%d %d %d", ops[op], op, i, numbers[i][j], bits[b]);
            debug("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
            freeReplyObject(reply);
          }
          N += howmany[i];
        }
      }
      timer_ns(ops[op], N);
    }
  }

  {
    const char* ops[] = {
      "R.GETBIT",
      "GETBIT"
    };

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      size_t N = 0;
      timer_ns(ops[op], N);
      for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < howmany[i]; j++) {
          redisReply* reply = redisCommand(c, "%s %d-%d %d", ops[op], op, i, numbers[i][j]);
          debug("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
          freeReplyObject(reply);
        }
        N += howmany[i];
      }
      timer_ns(ops[op], N);
    }
  }

  {
    const char* ops[] = {
      "R.BITCOUNT",
      "BITCOUNT"
    };

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      size_t N = 0;
      timer_ns(ops[op], N);
      for (size_t i = 0; i < count; i++) {
        redisReply* reply = redisCommand(c, "%s %d-%d", ops[op], op, i);
        debug("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
        freeReplyObject(reply);
        N += howmany[i];
      }
      timer_ns(ops[op], N);
    }
  }

  {
    const char* ops[] = {
      "R.BITPOS",
      "BITPOS"
    };

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      size_t N = 0;
      timer_ns(ops[op], N);
      const int bits[] = {1, 0};
      for (size_t b = 0; b < sizeof(bits) / sizeof(*bits); b++) {
        for (size_t i = 0; i < count; i++) {
          redisReply* reply = redisCommand(c, "%s %d-%d %d", ops[op], op, i, bits[b]);
          debug("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
          freeReplyObject(reply);
          N += howmany[i];
        }
      }
      timer_ns(ops[op], N);
    }
  }

  {
    const char* ops[] = {
      "R.BITOP",
      "BITOP"
    };

    const char type[] = "NOT";

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      size_t N = 0;
      char operation[256];
      snprintf(operation, sizeof(operation), "%s %s", ops[op], type);
      timer_ns(operation, N);
      for (size_t i = 0; i < count; i++) {
        redisReply* reply = redisCommand(c, "%s %s dest-%d-%d %d-%d", ops[op], type, op, i, op, i);
        debug("reply %s %s %lld\n", operation, reply->str, reply->integer);
        freeReplyObject(reply);
        N += howmany[i];
      }
      timer_ns(operation, N);
    }
  }

  {
    const char* ops[] = {
      "R.BITOP",
      "BITOP"
    };

    const char* types[] = {
      "AND",
      "OR",
      "XOR"
    };

    for (size_t t = 0; t < sizeof(types) / sizeof(*types); t++) {
      for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
        size_t N = 0;
        char operation[256];
        snprintf(operation, sizeof(operation), "%s %s", ops[op], types[t]);
        timer_ns(operation, N);
        for (size_t i = 0; i < count; i++) {
          redisReply* reply = redisCommand(c, "%s %s dest-%d-%d-%d %d-%d %d-%d",
                                           ops[op], types[t], t, op, i, op, 2 * i, op, 2 * i + 1);
          debug("reply %s %s %lld\n", operation, reply->str, reply->integer);
          freeReplyObject(reply);
          N += howmany[i];
        }
        timer_ns(operation, N);
      }
    }
  }

  printf("\n");

  for (size_t i = 0; i < count; i++) {
    free(numbers[i]);
  }
  free(numbers);
  free(howmany);
  redisFree(c);

  return 0;
}