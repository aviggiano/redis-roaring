#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "hiredis.h"
#include "benchmarks/numbersfromtextfiles.h"

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

int main(int argc, char* argv[]) {
  size_t count;
  size_t* howmany = NULL;
  uint32_t** numbers = read_all_integer_files("./deps/CRoaring/benchmarks/realdata/census1881",
                                              ".txt", &howmany, &count);
  assert(numbers != NULL);

  count = 20;

  printf("Constructing %d bitmaps\n\n", (int) count);
  redisContext* c = create_context();

  const char* ops[] = {
      "R.SETBIT",
      "SETBIT"
  };

  for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
    size_t N = 0;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (size_t i = 0; i < count; i++) {
      for (size_t j = 0; j < howmany[i]; j++) {
        redisReply* reply = redisCommand(c, "%s %d %d 1", ops[op], i, numbers[i][j]);
        freeReplyObject(reply);
      }
      N += howmany[i];
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    unsigned long ns = (end.tv_sec - start.tv_sec) * 1000000000UL + (end.tv_nsec - start.tv_nsec);
    unsigned long us = ns / 1000;
    unsigned long us_per_op = us / N;
    printf("Operation: %s\n", ops[op]);
    printf("Total time: %lu us\n", us);
    printf("Time/op: %lu us\n", us_per_op);
    printf("\n");
  }

  redisFree(c);
  return 0;
}