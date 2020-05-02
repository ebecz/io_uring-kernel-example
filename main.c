// open
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

// atoi
#include <stdlib.h>

// pwritev
#include <sys/uio.h>

// fprintf
#include <stdio.h>

// close
#include <unistd.h>

// memset
#include <string.h>

// io_uring
#include <liburing.h>
#define QD 2

int sync_rw(int fd, struct iovec *iov, int iovcnt, int offset)
{
  size_t count;

  fprintf(stderr, "Performing sync RW\n");

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
}

int async_rw(int fd, struct iovec *iov, int iovcnt, int offset)
{
  struct io_uring_sqe *sqe;
  struct io_uring_cqe *cqe;
  struct io_uring ring;
  static int const_read = 1, const_write = 2;
  int ret;

  fprintf(stderr, "Performing async RW\n");

  ret = io_uring_queue_init(QD, &ring, 0);
  if (ret < 0) {
    fprintf(stderr, "queue_init: %s\n", strerror(-ret));
    return -1;
  }

  sqe = io_uring_get_sqe(&ring);
  if (sqe == NULL) {
    fprintf(stderr, "Can't get sqe\n");
    return -1;
  }
  io_uring_prep_readv(sqe, fd, iov, iovcnt, offset);  
  io_uring_sqe_set_data(sqe, &const_read);

  sqe = io_uring_get_sqe(&ring);
  if (sqe == NULL) {
    fprintf(stderr, "Can't get sqe\n");
    return -1;
  }
  io_uring_prep_writev(sqe, fd, iov, iovcnt, offset);  
  io_uring_sqe_set_data(sqe, &const_write);

  if (io_uring_submit(&ring) != 2) {
    fprintf(stderr, "Can't do submit\n");
    return -1;
  }

  if (io_uring_wait_cqe_nr(&ring, &cqe, 1) < 0) {
    fprintf(stderr, "Can't wait cqe\n");
    return -1;
  }

  if (io_uring_cqe_get_data(cqe) == &const_write)
    fprintf(stderr, "Write completed\n");

  if (io_uring_cqe_get_data(cqe) == &const_read)
    fprintf(stderr, "Read completed\n");

  io_uring_cqe_seen(&ring, cqe);

  if (io_uring_wait_cqe_nr(&ring, &cqe, 1) < 0) {
    fprintf(stderr, "Can't wait cqe\n");
    return -1;
  }

  if (io_uring_cqe_get_data(cqe) == &const_write)
    fprintf(stderr, "Write completed\n");

  if (io_uring_cqe_get_data(cqe) == &const_read)
    fprintf(stderr, "Read completed\n");

  io_uring_cqe_seen(&ring, cqe);

  io_uring_queue_exit(&ring);
}

int main(int argc, const char *argv[]) {
  int iovcnt = 4;
  char buffer[iovcnt][512];
  struct iovec iov[iovcnt];
  int fd, i;

  fd = open("/dev/rw_iter", O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "Unable to open /dev/rw_iter\n");
    return -1;
  }

  memset(buffer, 1, sizeof(buffer));

  for (i = 0; i < iovcnt; i++) {
    printf("buffer[%d] at %p\n", i, buffer[i]);
    sprintf(buffer[i], "Elias-%d\n", i);
    iov[i].iov_base = buffer[i];
    iov[i].iov_len = sizeof(buffer[i]);
  }

  if (argc == 2 && atoi(argv[1]))
    async_rw(fd, iov, iovcnt, 0);
  else
    sync_rw(fd, iov, iovcnt, 0);

  close(fd);

  return 0;
}

