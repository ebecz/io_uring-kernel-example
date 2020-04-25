#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uio.h>

static char dummy_data[1024];

static int sample_open(struct inode *inode, struct file *file) {
  pr_info("I have been awoken\n");
  return 0;
}

static int sample_close(struct inode *inodep, struct file *filp) {
  pr_info("Sleepy time\n");
  return 0;
}

void describe(const struct iov_iter *iov_iter) {
  int i;
  pr_info("offset:%ld\n", iov_iter->iov_offset);
  pr_info("count:%ld\n", iov_iter_count(iov_iter));
  switch (iov_iter_rw(iov_iter)) {
    case WRITE:
      pr_info("direction:Write\n");
      break;
    case READ:
      pr_info("direction:Read\n");
      break;
  }
  switch (iov_iter_type(iov_iter)) {
    case ITER_IOVEC:
      pr_info("\tITER_IOVEC\n");
      for (i = 0; i < iov_iter->nr_segs; i++) {
        pr_info("\tbase:%p\tlen=%ld\n", iov_iter->iov[i].iov_base,
                iov_iter->iov[i].iov_len);
      }
      break;
    case ITER_KVEC:
      pr_info("ITER_KVEC\n");
      break;
    case ITER_BVEC:
      pr_info("ITER_BVEC\n");
      break;
    case ITER_PIPE:
      pr_info("ITER_PIPE\n");
      break;
    case ITER_DISCARD:
      pr_info("ITER_DISCARD\n");
      break;
    defaut:
      pr_info("Other\n");
      break;
  }
  pr_info("nr_segs:%ld\n", iov_iter->nr_segs);
  pr_info("pages:%d\n", iov_iter_npages(iov_iter, INT_MAX));
}

ssize_t sample_read_iter(struct kiocb *iocb, struct iov_iter *to) {
  size_t len = iov_iter_count(to);
  describe(to);
  pr_info("Ykukky - I just throw up %ld bytes\n", len);
  return copy_to_iter(dummy_data, sizeof(dummy_data), to);
}

ssize_t sample_write_iter(struct kiocb *iocb, struct iov_iter *from) {
  size_t len = iov_iter_count(from);
  describe(from);
  pr_info("Yummy - I just ate %ld bytes\n", len);
  return copy_from_iter(dummy_data, sizeof(dummy_data), from);
}

static const struct file_operations sample_fops = {
    .owner = THIS_MODULE,
    .open = sample_open,
    .read_iter = sample_read_iter,
    .write_iter = sample_write_iter,
    .release = sample_close,
    .llseek = no_llseek,
};

struct miscdevice sample_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "rw_iter",
    .fops = &sample_fops,
};

static int __init misc_init(void) {
  int error;

  error = misc_register(&sample_device);
  if (error) {
    pr_err("can't misc_register :(\n");
    return error;
  }

  pr_info("I'm in\n");
  return 0;
}

static void __exit misc_exit(void) {
  misc_deregister(&sample_device);
  pr_info("I'm out\n");
}

module_init(misc_init) module_exit(misc_exit)

    MODULE_DESCRIPTION("Simple Misc Driver");
MODULE_AUTHOR("Nick Glynn <n.s.glynn@gmail.com>");
MODULE_LICENSE("GPL");
