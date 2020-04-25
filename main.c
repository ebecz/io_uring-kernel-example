// open
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

// pwritev
#include <sys/uio.h>

// fprintf
#include <stdio.h>

// close
#include <unistd.h>

// memset
#include <string.h>

int main(int argc, const char *argv[]) {
  size_t count;
  int iovcnt = 4;
  struct iovec iov[iovcnt];
  int offset = 0;
  char buffer[iovcnt][512];
  int fd, i;

  fd = open("/dev/rw_iter", O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "Unable to open /dev/rw_iter\n");
    return -1;
  }

  memset(buffer, 1, sizeof(buffer));

  for (i = 0; i < iovcnt; i++) {
    iov[i].iov_base = buffer[i];
    iov[i].iov_len = sizeof(buffer[i]);
  }

  count = preadv(fd, iov, iovcnt, offset);
  if (count < 0) {
    fprintf(stderr, "Unable to preadv in /dev/rw_iter\n");
    return -1;
  }

  count = pwritev(fd, iov, iovcnt, offset);
  if (count < 0) {
    fprintf(stderr, "Unable to pwritev in /dev/rw_iter\n");
    return -1;
  }

  close(fd);

  return 0;
}
