#include "kernel/types.h"
#include "user/user.h"

#define N 280

void primes(int fd) {
  int prime;
  if (read(fd, &prime, sizeof(int)) == 0) {
    close(fd);
    exit(0);
  }
  printf("prime %d\n", prime);
  int p[2];
  pipe(p);
  if (fork() == 0) {
    close(p[1]);
    close(0);
    dup(p[0]);
    close(p[0]);
    primes(0);
    exit(0);
  } else {
    close(p[0]);
    int num;
    while (read(fd, &num, sizeof(int)) > 0) {
      if (num % prime != 0) {
        write(p[1], &num, sizeof(int));
      }
    }
    close(p[1]);
    close(fd);
    wait(0);
    exit(0);
  }
}

int main(int argc, char *argv[]) {
  int p[2];
  pipe(p);
  if (fork() == 0) {
    close(p[1]);
    primes(p[0]);
    close(p[0]);
  } else {
    close(p[0]);
    for (int i = 2; i < N; i++) {
      write(p[1], &i, sizeof(i));
    }
    close(p[1]);
    wait(0);
  }
  exit(0);
}
