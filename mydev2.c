#include <linux/module.h>	// for init_module() 
#include <asm/uaccess.h>	// for get_ds(), set_fs()
#include <asm/io.h>		// for inb(), outb()
#include <linux/init.h>
#include <linux/list.h>
#include <asm/segment.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/types.h>   // for dev_t typedef
#include <linux/kdev_t.h>  // for format_dev_t
#include <linux/fs.h>      // for alloc_chrdev_region()
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/kd.h>  /* Keyboard IOCTLs */
#include <linux/ioctl.h> /* ioctl()         */
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/delay.h>

#define BUF_MAX_SIZE		1024
#define	SUCCESS	0

static char buff[BUF_MAX_SIZE];
static dev_t mydev;             // (major,minor) value
struct cdev my_cdev;

struct item {
	struct list_head list;
	char c;
};

struct file* file_open(const char* path, int flags, int rights) {
    struct file* filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if(IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    return filp;
}

void file_close(struct file* file) {
    filp_close(file, NULL);
}

int file_read(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
} 

int file_write(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}

int file_sync(struct file* file) {
    vfs_fsync(file, 0);
    return 0;
}


char* itob(int i) {
	static char bits[8] = {'0','0','0','0','0','0','0','0'};
	int bits_index = 7;
	while ( i > 0 ) {
		bits[bits_index--] = (i & 1) + '0';
		i = ( i >> 1);
	}
	return bits;
}

void doit(char a ,char b)
{
    int r;
    r = (a - '0')*4 + (b - '0')*2;
    struct file*  fd;       /* Console fd (/dev/tty). Used as fd in ioctl() */
    long arg; /* Where the LED states will be put into.       */

    fd = file_open("/dev/tty7", O_NOCTTY , 0);

    arg = fd->f_op->unlocked_ioctl(fd, KDSETLED, r);
}


ssize_t my_write (struct file *flip, const char __user *buf, size_t count, loff_t *f_ops)
{

    printk(KERN_INFO "module chardrv being my_write.\n");

    struct item s_item;
    struct item *temp;

    INIT_LIST_HEAD(&s_item.list);
    int i;
    for(i=0; i<count;i++)
    {
        temp = kmalloc(sizeof(struct item), GFP_KERNEL);
        temp->c = buf[i];
        list_add(&(temp->list), &(s_item.list));
    }

    struct list_head *pos;
    char * result;
    list_for_each(pos, &s_item.list)
    {
        temp= list_entry(pos, struct item, list);
        if ((int)temp->c != 10)
        {
            printk("char is : %c\n" , temp->c);
            result = itob((int) temp->c);
            printk("ord is %s\n", result);
            int y;
            for(y=0 ; y<4 ; ++y)
            {
                doit(result[0+y*2] , result[1+y*2]);
                msleep(1000);
            }
        }
        
	}
	return count;
}

ssize_t my_read(struct file *flip, char __user *buf, size_t count, loff_t *f_ops)
{

    printk(KERN_INFO "module chardrv being my_read.\n");
	if(buff[*f_ops]=='\0')
	{
		return 0;
	}

	copy_to_user(buf, &buff[*f_ops],1);
	*f_ops+=1;

	return 1;

}

struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .read = my_read,
     .write=my_write,
};

static int __init chardrv_in(void)
{
    printk(KERN_INFO "module chardrv being loaded.\n");

    alloc_chrdev_region(&mydev, 0, 1, "mydev");
    //printk(KERN_INFO "%s\n", format_dev_t(buffer, mydev));

    cdev_init(&my_cdev, &my_fops);
    my_cdev.owner = THIS_MODULE;
    cdev_add(&my_cdev, mydev, 1);

    return 0;
}

static void __exit chardrv_out(void)
{
    printk(KERN_INFO "module chardrv being unloaded.\n");
    cdev_del(&my_cdev);
    unregister_chrdev_region(mydev, 1);
}

module_init(chardrv_in);
module_exit(chardrv_out);

MODULE_AUTHOR("Esmail Asyabi, http://webpages.iust.ac.ir/e_asyabi/");
MODULE_LICENSE("GPL");
