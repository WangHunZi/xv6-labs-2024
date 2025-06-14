#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int p1[2], p2[2];
  pipe(p1);
  pipe(p2);
  int pid = fork();
  char buf[2];
  if (pid == 0) {
    close(p1[1]);
    close(p2[0]);
    while (read(p1[0], buf, 1) == 0);
    close(p1[0]);
    printf("%d: received ping\n", getpid());
    write(p2[1], "c", 1);
    close(p2[1]);
    exit(0);
  } else if (pid > 0) {
    close(p1[0]);
    close(p2[1]);
    write(p1[1], "p", 1);
    close(p1[1]);
    // wait(0); // lead to deadlock?
    while (read(p2[0], buf, 1) == 0);
    printf("%d: received pong\n", getpid());
    close(p2[0]);
    wait(0);
    exit(0);
  } else {
    fprintf(2, "fork failed\n");
    exit(1);
  }
}
