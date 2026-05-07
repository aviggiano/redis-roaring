#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef REDIS_ROARING_FUZZ_REDIS_HARNESS_H
#define REDIS_ROARING_FUZZ_REDIS_HARNESS_H

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <hiredis/hiredis.h>

#include "fuzz_common.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef FUZZ_REDIS_SERVER_PATH
#error "FUZZ_REDIS_SERVER_PATH must be defined for Redis-backed fuzzers"
#endif

#ifndef FUZZ_MODULE_LIBRARY_PATH
#error "FUZZ_MODULE_LIBRARY_PATH must be defined for Redis-backed fuzzers"
#endif

typedef struct {
  pid_t pid;
  int port;
  bool cluster_enabled;
  bool appendonly;
  char dir[PATH_MAX];
  redisContext* redis;
} FuzzRedisServer;

static inline void fuzz_reply_require(redisReply* reply) {
  fuzz_require(reply != NULL);
}

static inline void fuzz_redis_free_reply(redisReply* reply) {
  if (reply != NULL) {
    freeReplyObject(reply);
  }
}

static inline void fuzz_redis_disconnect(FuzzRedisServer* server) {
  if (server->redis != NULL) {
    redisFree(server->redis);
    server->redis = NULL;
  }
}

static inline bool fuzz_remove_tree(const char* path) {
  DIR* dir = opendir(path);
  if (dir == NULL) {
    return errno == ENOENT;
  }

  struct dirent* entry = NULL;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char child[PATH_MAX];
    int n = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
    if (n < 0 || n >= (int)sizeof(child)) {
      closedir(dir);
      return false;
    }

    struct stat st;
    if (lstat(child, &st) != 0) {
      closedir(dir);
      return false;
    }

    if (S_ISDIR(st.st_mode)) {
      if (!fuzz_remove_tree(child) || rmdir(child) != 0) {
        closedir(dir);
        return false;
      }
    } else if (unlink(child) != 0) {
      closedir(dir);
      return false;
    }
  }

  closedir(dir);
  return true;
}

static inline bool fuzz_redis_prepare_dir(FuzzRedisServer* server, bool preserve_contents) {
  if (server->dir[0] == '\0') {
    char pattern[] = "/tmp/redis-roaring-fuzz-XXXXXX";
    char* dir = mkdtemp(pattern);
    if (dir == NULL) {
      return false;
    }
    strncpy(server->dir, dir, sizeof(server->dir) - 1);
    server->dir[sizeof(server->dir) - 1] = '\0';
  }

  if (preserve_contents) {
    return true;
  }

  if (!fuzz_remove_tree(server->dir)) {
    return false;
  }

  return mkdir(server->dir, 0700) == 0 || errno == EEXIST;
}

static inline int fuzz_pick_free_port(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }

  int reuse = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;

  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    close(fd);
    return -1;
  }

  socklen_t len = sizeof(addr);
  if (getsockname(fd, (struct sockaddr*)&addr, &len) != 0) {
    close(fd);
    return -1;
  }

  int port = (int)ntohs(addr.sin_port);
  close(fd);
  return port;
}

static inline redisReply* fuzz_redis_command(FuzzRedisServer* server, const char* format, ...) {
  fuzz_require(server->redis != NULL);

  va_list ap;
  va_start(ap, format);
  void* raw_reply = redisvCommand(server->redis, format, ap);
  va_end(ap);

  fuzz_require(raw_reply != NULL);
  fuzz_require(server->redis->err == 0);
  return raw_reply;
}

static inline redisReply* fuzz_redis_command_argv(FuzzRedisServer* server, int argc, const char** argv) {
  size_t* argvlen = malloc((size_t)argc * sizeof(*argvlen));
  fuzz_require(argvlen != NULL);
  for (int i = 0; i < argc; i++) {
    argvlen[i] = strlen(argv[i]);
  }

  redisReply* reply = redisCommandArgv(server->redis, argc, argv, argvlen);
  free(argvlen);

  fuzz_require(reply != NULL);
  fuzz_require(server->redis->err == 0);
  return reply;
}

static inline void fuzz_redis_expect_status(redisReply* reply, const char* expected) {
  fuzz_reply_require(reply);
  fuzz_require(reply->type == REDIS_REPLY_STATUS);
  fuzz_require(strcmp(reply->str, expected) == 0);
}

static inline void fuzz_redis_expect_int(redisReply* reply, long long expected) {
  fuzz_reply_require(reply);
  fuzz_require(reply->type == REDIS_REPLY_INTEGER);
  fuzz_require(reply->integer == expected);
}

static inline void fuzz_redis_wait_for_exit(FuzzRedisServer* server) {
  if (server->pid <= 0) {
    return;
  }

  for (int i = 0; i < 100; i++) {
    int status = 0;
    pid_t result = waitpid(server->pid, &status, WNOHANG);
    if (result == server->pid) {
      server->pid = -1;
      return;
    }
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 50000000L}, NULL);
  }

  kill(server->pid, SIGKILL);
  waitpid(server->pid, NULL, 0);
  server->pid = -1;
}

static inline void fuzz_redis_shutdown(FuzzRedisServer* server, bool save) {
  if (server->redis != NULL) {
    redisReply* reply = redisCommand(server->redis, save ? "SHUTDOWN SAVE" : "SHUTDOWN NOSAVE");
    fuzz_redis_free_reply(reply);
  }

  fuzz_redis_disconnect(server);
  fuzz_redis_wait_for_exit(server);
}

static inline bool fuzz_redis_wait_until_ready(FuzzRedisServer* server) {
  struct timeval timeout = {.tv_sec = 0, .tv_usec = 200000};

  for (int attempt = 0; attempt < 100; attempt++) {
    fuzz_redis_disconnect(server);
    server->redis = redisConnectWithTimeout("127.0.0.1", server->port, timeout);
    if (server->redis != NULL && server->redis->err == 0) {
      redisReply* reply = redisCommand(server->redis, "PING");
      if (reply != NULL && reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "PONG") == 0) {
        fuzz_redis_free_reply(reply);
        return true;
      }
      fuzz_redis_free_reply(reply);
    }

    fuzz_redis_disconnect(server);
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 50000000L}, NULL);
  }

  return false;
}

static inline bool fuzz_redis_assign_cluster_slots(FuzzRedisServer* server) {
  redisReply* reply = fuzz_redis_command(server, "CLUSTER ADDSLOTSRANGE 0 16383");
  if (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0) {
    fuzz_redis_free_reply(reply);
  } else {
    fuzz_redis_free_reply(reply);

    for (int start = 0; start <= 16383; start += 1024) {
      int end = start + 1023;
      if (end > 16383) {
        end = 16383;
      }

      char command[8192];
      int written = snprintf(command, sizeof(command), "CLUSTER ADDSLOTS");
      fuzz_require(written > 0 && written < (int)sizeof(command));

      for (int slot = start; slot <= end; slot++) {
        written += snprintf(command + written, sizeof(command) - (size_t)written, " %d", slot);
        fuzz_require(written > 0 && written < (int)sizeof(command));
      }

      reply = fuzz_redis_command(server, "%s", command);
      if (reply->type == REDIS_REPLY_ERROR) {
        fuzz_redis_free_reply(reply);
        return false;
      }

      fuzz_redis_expect_status(reply, "OK");
      fuzz_redis_free_reply(reply);
    }
  }

  for (int attempt = 0; attempt < 100; attempt++) {
    reply = fuzz_redis_command(server, "CLUSTER INFO");
    fuzz_reply_require(reply);
    if (reply->type == REDIS_REPLY_STRING && strstr(reply->str, "cluster_state:ok") != NULL) {
      fuzz_redis_free_reply(reply);
      return true;
    }
    fuzz_redis_free_reply(reply);
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 50000000L}, NULL);
  }

  return false;
}

static inline bool fuzz_redis_start(FuzzRedisServer* server, bool cluster_enabled, bool appendonly, bool preserve_contents) {
  char preserved_dir[PATH_MAX];
  preserved_dir[0] = '\0';
  if (server->dir[0] != '\0') {
    strncpy(preserved_dir, server->dir, sizeof(preserved_dir) - 1);
    preserved_dir[sizeof(preserved_dir) - 1] = '\0';
  }

  fuzz_redis_disconnect(server);
  server->pid = -1;
  server->port = -1;
  server->cluster_enabled = cluster_enabled;
  server->appendonly = appendonly;
  if (preserved_dir[0] != '\0') {
    strncpy(server->dir, preserved_dir, sizeof(server->dir) - 1);
    server->dir[sizeof(server->dir) - 1] = '\0';
  }

  if (!fuzz_redis_prepare_dir(server, preserve_contents)) {
    return false;
  }

  server->port = fuzz_pick_free_port();
  if (server->port <= 0) {
    return false;
  }

  pid_t pid = fork();
  if (pid < 0) {
    return false;
  }

  if (pid == 0) {
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
      dup2(null_fd, STDOUT_FILENO);
      dup2(null_fd, STDERR_FILENO);
      close(null_fd);
    }

    char port[16];
    snprintf(port, sizeof(port), "%d", server->port);

    const char* argv[32];
    int argc = 0;
    argv[argc++] = FUZZ_REDIS_SERVER_PATH;
    argv[argc++] = "--port";
    argv[argc++] = port;
    argv[argc++] = "--save";
    argv[argc++] = "";
    argv[argc++] = "--appendonly";
    argv[argc++] = appendonly ? "yes" : "no";
    argv[argc++] = "--bind";
    argv[argc++] = "127.0.0.1";
    argv[argc++] = "--protected-mode";
    argv[argc++] = "no";
    argv[argc++] = "--loglevel";
    argv[argc++] = "warning";
    argv[argc++] = "--dir";
    argv[argc++] = server->dir;
    argv[argc++] = "--loadmodule";
    argv[argc++] = FUZZ_MODULE_LIBRARY_PATH;

    if (cluster_enabled) {
      argv[argc++] = "--cluster-enabled";
      argv[argc++] = "yes";
      argv[argc++] = "--cluster-config-file";
      argv[argc++] = "nodes.conf";
      argv[argc++] = "--cluster-node-timeout";
      argv[argc++] = "5000";
    }

    argv[argc] = NULL;
    execv(FUZZ_REDIS_SERVER_PATH, (char* const*)argv);
    _exit(127);
  }

  server->pid = pid;
  if (!fuzz_redis_wait_until_ready(server)) {
    fuzz_redis_shutdown(server, false);
    return false;
  }

  if (cluster_enabled && !fuzz_redis_assign_cluster_slots(server)) {
    fuzz_redis_shutdown(server, false);
    return false;
  }

  return true;
}

static inline bool fuzz_redis_restart(FuzzRedisServer* server, bool preserve_contents) {
  bool cluster_enabled = server->cluster_enabled;
  bool appendonly = server->appendonly;

  fuzz_redis_shutdown(server, false);
  return fuzz_redis_start(server, cluster_enabled, appendonly, preserve_contents);
}

static inline void fuzz_redis_flushall(FuzzRedisServer* server) {
  redisReply* reply = fuzz_redis_command(server, "FLUSHALL");
  fuzz_redis_expect_status(reply, "OK");
  fuzz_redis_free_reply(reply);
}

static inline long long fuzz_redis_dbsize(FuzzRedisServer* server) {
  redisReply* reply = fuzz_redis_command(server, "DBSIZE");
  fuzz_reply_require(reply);
  fuzz_require(reply->type == REDIS_REPLY_INTEGER);
  long long value = reply->integer;
  fuzz_redis_free_reply(reply);
  return value;
}

static inline void fuzz_redis_set_sentinel(FuzzRedisServer* server) {
  redisReply* reply = fuzz_redis_command(server, "SET __fuzz_sentinel__ 1");
  fuzz_redis_expect_status(reply, "OK");
  fuzz_redis_free_reply(reply);
}

static inline void fuzz_redis_wait_for_aof_rewrite(FuzzRedisServer* server) {
  for (int attempt = 0; attempt < 200; attempt++) {
    redisReply* reply = fuzz_redis_command(server, "INFO persistence");
    fuzz_reply_require(reply);
    fuzz_require(reply->type == REDIS_REPLY_STRING);
    if (strstr(reply->str, "aof_rewrite_in_progress:0") != NULL) {
      fuzz_redis_free_reply(reply);
      return;
    }
    fuzz_redis_free_reply(reply);
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 50000000L}, NULL);
  }

  abort();
}

static inline void fuzz_redis_set_int_array32(FuzzRedisServer* server, const char* command, const char* key,
                                              size_t count, const uint32_t* values) {
  int argc = (int)count + 2;
  const char** argv = malloc((size_t)argc * sizeof(*argv));
  char** owned = malloc((size_t)count * sizeof(*owned));

  fuzz_require(argv != NULL);
  fuzz_require(owned != NULL);

  argv[0] = command;
  argv[1] = key;
  for (size_t i = 0; i < count; i++) {
    owned[i] = malloc(32);
    fuzz_require(owned[i] != NULL);
    snprintf(owned[i], 32, "%u", values[i]);
    argv[i + 2] = owned[i];
  }

  redisReply* reply = fuzz_redis_command_argv(server, argc, argv);
  if (reply->type == REDIS_REPLY_STATUS) {
    fuzz_require(strcmp(reply->str, "OK") == 0);
  } else {
    fuzz_require(reply->type == REDIS_REPLY_STRING);
    fuzz_require(strcmp(reply->str, "OK") == 0);
  }
  fuzz_redis_free_reply(reply);

  for (size_t i = 0; i < count; i++) {
    free(owned[i]);
  }
  free(owned);
  free(argv);
}

static inline void fuzz_redis_set_int_array64(FuzzRedisServer* server, const char* command, const char* key,
                                              size_t count, const uint64_t* values) {
  int argc = (int)count + 2;
  const char** argv = malloc((size_t)argc * sizeof(*argv));
  char** owned = malloc((size_t)count * sizeof(*owned));

  fuzz_require(argv != NULL);
  fuzz_require(owned != NULL);

  argv[0] = command;
  argv[1] = key;
  for (size_t i = 0; i < count; i++) {
    owned[i] = malloc(32);
    fuzz_require(owned[i] != NULL);
    snprintf(owned[i], 32, "%llu", (unsigned long long)values[i]);
    argv[i + 2] = owned[i];
  }

  redisReply* reply = fuzz_redis_command_argv(server, argc, argv);
  if (reply->type == REDIS_REPLY_STATUS) {
    fuzz_require(strcmp(reply->str, "OK") == 0);
  } else {
    fuzz_require(reply->type == REDIS_REPLY_STRING);
    fuzz_require(strcmp(reply->str, "OK") == 0);
  }
  fuzz_redis_free_reply(reply);

  for (size_t i = 0; i < count; i++) {
    free(owned[i]);
  }
  free(owned);
  free(argv);
}

static inline uint32_t* fuzz_redis_get_int_array32(FuzzRedisServer* server, const char* key, size_t* count) {
  redisReply* reply = fuzz_redis_command(server, "R.GETINTARRAY %s", key);
  fuzz_reply_require(reply);

  if (reply->type == REDIS_REPLY_ARRAY) {
    uint32_t* values = malloc(reply->elements * sizeof(*values));
    fuzz_require(values != NULL || reply->elements == 0);
    for (size_t i = 0; i < reply->elements; i++) {
      fuzz_require(reply->element[i]->type == REDIS_REPLY_INTEGER);
      fuzz_require(reply->element[i]->integer >= 0);
      values[i] = (uint32_t)reply->element[i]->integer;
    }
    *count = reply->elements;
    fuzz_redis_free_reply(reply);
    return values;
  }

  fuzz_require(reply->type == REDIS_REPLY_STRING || reply->type == REDIS_REPLY_NIL);
  fuzz_require(reply->str == NULL || reply->str[0] == '\0');
  *count = 0;
  fuzz_redis_free_reply(reply);
  return NULL;
}

static inline uint64_t* fuzz_redis_get_int_array64(FuzzRedisServer* server, const char* key, size_t* count) {
  redisReply* reply = fuzz_redis_command(server, "R64.GETINTARRAY %s", key);
  fuzz_reply_require(reply);

  if (reply->type == REDIS_REPLY_ARRAY) {
    uint64_t* values = malloc(reply->elements * sizeof(*values));
    fuzz_require(values != NULL || reply->elements == 0);
    for (size_t i = 0; i < reply->elements; i++) {
      fuzz_require(reply->element[i]->type == REDIS_REPLY_INTEGER);
      fuzz_require(reply->element[i]->integer >= 0);
      values[i] = (uint64_t)reply->element[i]->integer;
    }
    *count = reply->elements;
    fuzz_redis_free_reply(reply);
    return values;
  }

  fuzz_require(reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_STRING);
  if (reply->type == REDIS_REPLY_STRING) {
    fuzz_require(reply->str == NULL || reply->str[0] == '\0');
  }
  *count = 0;
  fuzz_redis_free_reply(reply);
  return NULL;
}

static inline void fuzz_require_uint32_arrays_equal(const uint32_t* lhs, size_t lhs_count,
                                                    const uint32_t* rhs, size_t rhs_count) {
  fuzz_require(lhs_count == rhs_count);
  for (size_t i = 0; i < lhs_count; i++) {
    fuzz_require(lhs[i] == rhs[i]);
  }
}

static inline void fuzz_require_uint64_arrays_equal(const uint64_t* lhs, size_t lhs_count,
                                                    const uint64_t* rhs, size_t rhs_count) {
  fuzz_require(lhs_count == rhs_count);
  for (size_t i = 0; i < lhs_count; i++) {
    fuzz_require(lhs[i] == rhs[i]);
  }
}

#endif
