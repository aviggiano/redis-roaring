#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <stdbool.h>
#include "hiredis.h"
#include "numbersfromtextfiles.h"

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif


/*
 * macros
 */
//#define debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define debug(fmt, ...)

/*
 * structs
 */

typedef struct statistics_s {
  const char* operation;
  struct timespec start;
  bool ticking;
  size_t N;
  double mean;
  double variance;
} Statistics;

/*
 * function declarations
 */
void print(const char* fmt, ...);
redisContext* create_context();
void print_header();
void static inline timer(Statistics* statistics);
Statistics statistics_init(const char* operation);
void statistics_print(Statistics* statistics);
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
  print("| %12s ", "TIME/OP (us)");
  print("| %12s |\n", "ST.DEV. (us)");

  print("| %12s ", "------------");
  print("| %12s ", "------------");
  print("| %12s |\n", "------------");
}

int clock_gettime_local(struct timespec *a) {
  int err = 0;
  //https://gist.github.com/jbenet/1087739     
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
   clock_serv_t cclock;
   mach_timespec_t mts;
   host_get_clock_service(mach_host_self(), REALTIME_CLOCK, &cclock);
   clock_get_time(cclock, &mts);
   mach_port_deallocate(mach_task_self(), cclock);
   a->tv_sec = mts.tv_sec;
   a->tv_nsec = mts.tv_nsec;
#else
  err = clock_gettime(CLOCK_MONOTONIC, a);
#endif
  return err;
}

void static inline timer(Statistics* statistics) {
  if (!statistics->ticking) {
    //clock_gettime(CLOCK_MONOTONIC, &statistics->start);
    clock_gettime_local(&statistics->start);
    statistics->ticking = true;
    return;
  }
  statistics->ticking = false;

  struct timespec end;
  //clock_gettime(CLOCK_MONOTONIC, &end);
  clock_gettime_local(&end);
  double ns = (end.tv_sec - statistics->start.tv_sec) * 1000000000UL + (end.tv_nsec - statistics->start.tv_nsec);

  // https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online_algorithm
  statistics->N++;
  double delta = ns - statistics->mean;
  statistics->mean += delta / statistics->N;
  double delta2 = ns - (statistics->mean);
  statistics->variance += delta * delta2;

}

Statistics statistics_init(const char* operation) {
  return (Statistics) {
    .operation = operation,
    .start = {0, 0},
    .ticking = false,
    .N = 0,
    .mean = 0,
    .variance = 0
  };
}

void statistics_print(Statistics* statistics) {
  double mean = 1E-3 * statistics->mean;
  double stdev = 1E-6 * sqrt(statistics->variance);

  print("| %12s ", statistics->operation);
  print("| %12.2f ", mean);
  print("| %12.2f |\n", stdev);
}

int main(int argc, char* argv[]) {
  size_t count;
  size_t* howmany = NULL;
  const char* dirbuffer = CROARING_BENCHMARK_DATA_DIR "census1881";
  printf("dirbuffer %s\n", dirbuffer);
  uint32_t** numbers = read_all_integer_files(dirbuffer,
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
      Statistics statistics = statistics_init(ops[op]);
      const int bits[] = {1, 0, 1};
      for (size_t b = 0; b < sizeof(bits) / sizeof(*bits); b++) {
        for (size_t i = 0; i < count; i++) {
          for (size_t j = 0; j < howmany[i]; j++) {
            timer(&statistics);
            redisReply* reply = redisCommand(c, "%s %d-%d %d %d", ops[op], op, i, numbers[i][j], bits[b]);
            timer(&statistics);
            debug("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
            freeReplyObject(reply);
          }
        }
      }
      statistics_print(&statistics);
    }
  }

  {
    const char* ops[] = {
      "R.GETBIT",
      "GETBIT"
    };

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      Statistics statistics = statistics_init(ops[op]);
      for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < howmany[i]; j++) {
          timer(&statistics);
          redisReply* reply = redisCommand(c, "%s %d-%d %d", ops[op], op, i, numbers[i][j]);
          timer(&statistics);
          debug("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
          freeReplyObject(reply);
        }
      }
      statistics_print(&statistics);
    }
  }

  {
    const char* ops[] = {
      "R.BITCOUNT",
      "BITCOUNT"
    };

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      Statistics statistics = statistics_init(ops[op]);
      for (size_t i = 0; i < count; i++) {
        timer(&statistics);
        redisReply* reply = redisCommand(c, "%s %d-%d", ops[op], op, i);
        timer(&statistics);
        debug("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
        freeReplyObject(reply);
      }
      statistics_print(&statistics);
    }
  }

  {
    const char* ops[] = {
      "R.BITPOS",
      "BITPOS"
    };

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      Statistics statistics = statistics_init(ops[op]);
      const int bits[] = {1, 0};
      for (size_t b = 0; b < sizeof(bits) / sizeof(*bits); b++) {
        for (size_t i = 0; i < count; i++) {
          timer(&statistics);
          redisReply* reply = redisCommand(c, "%s %d-%d %d", ops[op], op, i, bits[b]);
          timer(&statistics);
          debug("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
          freeReplyObject(reply);
        }
      }
      statistics_print(&statistics);
    }
  }

  {
    const char* ops[] = {
      "R.BITOP",
      "BITOP"
    };

    const char type[] = "NOT";

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      char operation[256];
      snprintf(operation, sizeof(operation), "%s %s", ops[op], type);
      Statistics statistics = statistics_init(operation);
      for (size_t i = 0; i < count; i++) {
        timer(&statistics);
        redisReply* reply = redisCommand(c, "%s %s dest-%d-%d %d-%d", ops[op], type, op, i, op, i);
        timer(&statistics);
        debug("reply %s %s %lld\n", operation, reply->str, reply->integer);
        freeReplyObject(reply);
      }
      statistics_print(&statistics);
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
        char operation[256];
        snprintf(operation, sizeof(operation), "%s %s", ops[op], types[t]);
        Statistics statistics = statistics_init(operation);
        for (size_t i = 0; i < count; i++) {
          timer(&statistics);
          redisReply* reply = redisCommand(c, "%s %s dest-%d-%d-%d %d-%d %d-%d",
                                           ops[op], types[t], t, op, i, op, 2 * i, op, 2 * i + 1);
          timer(&statistics);
          debug("reply %s %s %lld\n", operation, reply->str, reply->integer);
          freeReplyObject(reply);
        }
        statistics_print(&statistics);
      }
    }
  }

  {
    const char* ops[] = {
      "R.MIN",
      "MIN"
    };

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      Statistics statistics = statistics_init(ops[op]);
      for (size_t i = 0; i < count; i++) {
        timer(&statistics);
        redisReply* reply = redisCommand(c, "%s %d-%d", ops[op], op, i);
        timer(&statistics);
        debug("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
        freeReplyObject(reply);
      }
      statistics_print(&statistics);
    }
  }

  {
    const char* ops[] = {
      "R.MAX",
      "MAX"
    };

    for (size_t op = 0; op < sizeof(ops) / sizeof(*ops); op++) {
      Statistics statistics = statistics_init(ops[op]);
      for (size_t i = 0; i < count; i++) {
        timer(&statistics);
        redisReply* reply = redisCommand(c, "%s %d-%d", ops[op], op, i);
        timer(&statistics);
        debug("reply %s %s %lld\n", ops[op], reply->str, reply->integer);
        freeReplyObject(reply);
      }
      statistics_print(&statistics);
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
