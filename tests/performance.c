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
  printf("Operation: %s\n", operation);
  printf("Total time: %.2f s\n", 1E-9 * ns);
  printf("Time/op: %.2f us\n", us_per_op);
  printf("\n");
}

int main(int argc, char* argv[]) {
  size_t count;
  size_t* howmany = NULL;
  uint32_t** numbers = read_all_integer_files("./deps/CRoaring/benchmarks/realdata/census1881",
                                              ".txt", &howmany, &count);

  count = 1;
  assert(numbers != NULL);

  printf("Constructing %d bitmaps\n\n", (int) count);
  redisContext* c = create_context();

  {
    const char* ops[] = {
      "R.SETBIT",
      "SETBIT"
    };

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      size_t N = 0;
      timer_ns(ops[op], N);
      for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < howmany[i]; j++) {
          redisReply* reply = redisCommand(c, "%s %d-%d %d 1", ops[op], op, i, numbers[i][j]);
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
      "R.GETBIT",
      "GETBIT"
    };

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      size_t N = 0;
      timer_ns(ops[op], N);
      for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < howmany[i]; j++) {
          redisReply* reply = redisCommand(c, "%s %d-%d %d", ops[op], op, i);
          log("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
          freeReplyObject(reply);
        }
        N += howmany[i];
      }
      timer_ns(ops[op], N);
    }
  }

  redisFree(c);
  return 0;
}