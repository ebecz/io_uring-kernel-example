#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/uio.h>
#include <linux/workqueue.h>

static char dummy_data[1024];

struct workqueue_struct *workqueue;

static int sample_open(struct inode *inode, struct file *file) {
  pr_info("I have been awoken\n");
  return 0;
}

static int sample_close(struct inode *inodep, struct file *filp) {
  pr_info("Sleepy time\n");
  return 0;
}

static void describe(const struct iov_iter *iov_iter) {
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
        pr_info("\tbase:%pS\tlen=%ld\n", iov_iter->iov[i].iov_base,
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
}

// This function is inspired on block/bio.c:bio_map_user_iov
static void inspect_pages(struct iov_iter *iov_iter) {
  int num_pages;
  struct page **pages;
  const void *to_free;
  struct iov_iter clone;
  size_t len;
  int npages;

  // iov_iter_get_pages_alloc return a mapping to the first iov only
  // if we want to map all of then, we going to need to interate over it
  // So we don't have an array of num_pages after the return of
  // iov_iter_get_pages_alloc as I tought it would be on the first place
  num_pages = iov_iter_npages(iov_iter, INT_MAX);
  pr_info("Total of pages:%d\n", num_pages);

  // We will clone it because we want to inspect the memory without afect the
  // iterator There is no documentation but the return of dup_ite must be free
  // by the user
  to_free = dup_iter(&clone, iov_iter, GFP_KERNEL);

  // TODO: Remove the following lines, once we know what they do
  // iov_iter_single_seg_count(&clone);
  // length;

  len = iov_iter_count(&clone);
  pr_info("Total len=%ld\n", len);
  while (len > 0) {
    int i;
    size_t offs, bytes;
    char us[32] = {
        0,
    };
    int us_len = 0;
    // We are going to use maxsize = PAGE_SIZE, it means it will return at most
    // two pages
    bytes = iov_iter_get_pages_alloc(&clone, &pages, PAGE_SIZE, &offs);
    pr_info("\tpage size bytes:%ld\n", bytes);
    pr_info("\tpage offs:%ld\n", offs);

    // We update how many bytes left to be mapped and advance the iterator
    len -= bytes;
    iov_iter_advance(&clone, bytes);

    // To be sure the actual number of pages,
    // we must calculate if the data crossed a page bondary
    npages = DIV_ROUND_UP(offs + bytes, PAGE_SIZE);
    pr_info("\tnumber of pages for this mapping:%d\n", npages);

    for (i = 0; i < npages; i++) {
      struct page *page = pages[i];
      size_t n = PAGE_SIZE - offs;
      void *myaddr;

      if (n > bytes) n = bytes;

      pr_info("\t\t%ld bytes on the page %d\n", n, i);

      // kmap configures the MMU so we can access the memory from this context
      myaddr = kmap(page) + offs;

      // As you will se the addr are not continuos here like they are on the userspace
      pr_info("\t\tkernel addr:%pS to %pS\n", myaddr, myaddr + n - 1);
      pr_info("\t\tphysical addr:%llx to %llx\n",  virt_to_phys(myaddr), virt_to_phys(myaddr + n - 1));

      offs = 0;
      bytes -= n;

      // Let's inspect the memory
      for (; (us_len < sizeof(us) - 1) && n; us_len++, n--) {
        us[us_len] = *((char *)myaddr);
        myaddr++;
      }

      kunmap(page);
    }

    pr_info("\t\tuser string:%6s\n", us);
    kvfree(pages);
  }

  kfree(to_free);
}

static struct kiocb *riocb = NULL;
static int rcount;
static void complete_read(struct work_struct *w)
{
  pr_info("delayed work %s\n", __func__);
  if (riocb && riocb->ki_complete) {
    riocb->ki_complete(riocb, rcount, 0);
    pr_info("delayed work called\n");
  }
  riocb = NULL;
}

DECLARE_DELAYED_WORK(read_work, complete_read);

ssize_t sample_read_iter(struct kiocb *iocb, struct iov_iter *to) {
  size_t len = iov_iter_count(to);
  describe(to);
  inspect_pages(to);
  pr_info("Ykukky - I just throw up %ld bytes\n", len);
  rcount = copy_to_iter(dummy_data, sizeof(dummy_data), to);
  if (is_sync_kiocb(iocb)) {
    pr_info("Synchronous read request\n");
    return rcount;
  } else {
    pr_info("Asynchronous read request\n");
    riocb = iocb;
    queue_delayed_work(workqueue, &read_work, msecs_to_jiffies(2000));
    return -EIOCBQUEUED;
  }
}

static struct kiocb *wiocb = NULL;
static int wcount;
static void complete_write(struct work_struct *w)
{
  pr_info("delayed work %s\n", __func__);
  if (wiocb && wiocb->ki_complete) {
    wiocb->ki_complete(wiocb, wcount, 0);
    pr_info("delayed work called\n");
  }
  wiocb = NULL;
}

DECLARE_DELAYED_WORK(write_work, complete_write);

ssize_t sample_write_iter(struct kiocb *iocb, struct iov_iter *from) {
  size_t len = iov_iter_count(from);
  describe(from);
  inspect_pages(from);
  pr_info("Yummy - I just ate %ld bytes\n", len);
  wcount = copy_from_iter(dummy_data, sizeof(dummy_data), from);
  if (is_sync_kiocb(iocb)) {
    pr_info("Synchronous write request\n");
    return wcount;
  } else {
    pr_info("Asynchronous write request\n");
    wiocb = iocb;
    queue_delayed_work(workqueue, &write_work, msecs_to_jiffies(1000));
    return -EIOCBQUEUED;
  }
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

  workqueue = create_singlethread_workqueue("RW testing\n");
  if (workqueue == NULL) {
    pr_err("Unable to create an workqueue\n");
    return -1;
  }

  error = misc_register(&sample_device);
  if (error) {
    destroy_workqueue(workqueue);
    pr_err("can't misc_register :(\n");
    return error;
  }

  pr_info("I'm in\n");
  return 0;
}

static void __exit misc_exit(void) {
  misc_deregister(&sample_device);
  cancel_delayed_work_sync(&read_work);
  cancel_delayed_work_sync(&write_work);
  destroy_workqueue(workqueue);
  pr_info("I'm out\n");
}

module_init(misc_init) module_exit(misc_exit)

    MODULE_DESCRIPTION("Simple Misc Driver");
MODULE_AUTHOR("Nick Glynn <n.s.glynn@gmail.com>");
MODULE_LICENSE("GPL");
