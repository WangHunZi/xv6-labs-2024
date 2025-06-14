#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void find(char *path, char *name) {
  struct dirent de;
  struct stat st;
  int fd;
  if ((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type == T_FILE) {
    char *p = path + strlen(path);
    while ((p > path) && ((*(--p)) != '/'));
    if (*p == '/') p ++;
    if (strcmp(p, name) == 0) {
      printf("%s\n", path);
    }
  } else if (st.type == T_DIR) {
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0) {
        continue;
      }
      if (de.name[0] == '.') {
        continue;
      }
      char buf[512], *p;
      if(strlen(path) + 1 + DIRSIZ + 1 > sizeof (buf)){
        printf("find: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf+strlen(buf);
      *p++ = '/';
      memmove(p, de.name, DIRSIZ);
      if(stat(buf, &st) < 0) {
        printf("cannot stat %s\n", buf);
        continue;
      }
      if (st.type == T_DIR) {
        if (strcmp(de.name, name) == 0) {
          printf("%s\n", buf);
        }
        find(buf, name);
      } else if (st.type == T_FILE) {
        if (strcmp(de.name, name) == 0) {
          printf("%s\n", buf);
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(2, "Usage: find <path> <name>\n");
    exit(1);
  }
  char *path = argv[1];
  char *name = argv[2];
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    exit(1);
  }
  find(path, name);
  close(fd);
  exit(0);
}
