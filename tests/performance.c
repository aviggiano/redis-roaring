#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "hiredis.h"
#include "benchmarks/numbersfromtextfiles.h"

//#define log(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define log(fmt, ...)

redisContext* create_context() {
  redisContext* c;
  const char hostname[] = "127.0.0.1";
  int port = 6379;

  struct timeval timeout = {2, 0};
  c = redisConnectWithTimeout(hostname, port, timeout);
  if (c == NULL || c->err) {
    if (c) {
      printf("Connection error: %s\n", c->errstr);
      redisFree(c);
    } else {
      printf("Connection error: can't allocate redis context\n");
    }
    exit(1);
  }
  return c;
}

void print_header() {
  printf("%16s    ", "OP");
  printf("%8s    ", "TIME (s)");
  printf("%12s\n\n", "TIME/OP (us)");
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
  printf("%16s    ", operation);
  printf("%8.2f    ", 1E-9 * ns);
  printf("%12.2f\n", us_per_op);
}

int main(int argc, char* argv[]) {
  size_t count;
  size_t* howmany = NULL;
  const char test_output[] = "/tmp/test.out";
  uint32_t** numbers = read_all_integer_files("./deps/CRoaring/benchmarks/realdata/census1881",
                                              ".txt", &howmany, &count);

  assert(numbers != NULL);

  printf("Constructing %d bitmaps\n\n", (int) count);
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
            log("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
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
          log("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
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
        for (size_t j = 0; j < howmany[i]; j++) {
          redisReply* reply = redisCommand(c, "%s %d-%d", ops[op], op, i);
          log("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
          freeReplyObject(reply);
        }
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
          for (size_t j = 0; j < howmany[i]; j++) {
            redisReply* reply = redisCommand(c, "%s %d-%d %d", ops[op], op, i, bits[b]);
            log("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
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
        for (size_t j = 0; j < howmany[i]; j++) {
          redisReply* reply = redisCommand(c, "%s %s dest-%d-%d %d-%d", ops[op], type, op, i, op, i);
          log("reply %s %s %lld\n", operation, reply->str, reply->integer);
          freeReplyObject(reply);
        }
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
          for (size_t j = 0; j < howmany[i]; j++) {
            redisReply* reply = redisCommand(c, "%s %s dest-%d-%d-%d %d-%d %d-%d",
                                             ops[op], types[t], t, op, i, op, 2 * i, op, 2 * i + 1);
            log("reply %s %s %lld\n", operation, reply->str, reply->integer);
            freeReplyObject(reply);
          }
          N += howmany[i];
        }
        timer_ns(operation, N);
      }
    }
  }

  printf("\n");

  FILE* fp;
  fp = fopen(test_output, "w");
  assert(fp != NULL);

  fprintf(fp, "| R.SETBIT | SETBIT |\n");
  fprintf(fp, "| -------- | ------ |\n");
  fprintf(fp, "| TBD      | TBD    |\n");

  fclose(fp);

  redisFree(c);
  return 0;
}