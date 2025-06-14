#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "Usage: xargs command [args...]\n");
    exit(1);
  }

  int i = 0;
  char c;
  char buf[512];
  while (read(0, &c, 1) > 0) {
    if (c != '\n') {
      if (i >= 511) {
        fprintf(2, "xargs: input line too long\n");
        exit(1);
      }
      buf[i ++] = c;
    } else if (c == '\n' && i <= 512) {
      buf[i] = '\0';
      char *copy[MAXARG];
      for (int j = 1; j < argc; j++) {
        copy[j - 1] = argv[j];
      }
      copy[argc - 1] = buf;
      copy[argc] = 0;
      if (fork() == 0) {
        exec(argv[1], copy);
      } else {
        wait(0);
        memset(buf, 0, i);
        i = 0;
      }
    } else {
      fprintf(2, "xargs: input line too long\n");
      exit(1);
    }
  }
  exit(0);
}
