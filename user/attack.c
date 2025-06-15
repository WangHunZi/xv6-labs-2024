#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  char *secret = sbrk(PGSIZE*32);
  for (char *p = secret + 32; p < secret + PGSIZE*32; p++) {
    if (p == (char *)(0x14020)) {
      printf("Found secret: %s\n", p);
      write(2, p, 8);
      break;
    }
  }
  exit(1);
}
