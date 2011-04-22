
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/input.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>

#define VKBD_PROC_NAME	"kbd"
#define VKBD_INPUT_LEN	5

#define VKBD_RELEASE	0
#define VKBD_PRESS	1

struct vkbd_dev {
	long last_entry;
	spinlock_t lock;
	struct input_dev *device;
};

static struct vkbd_dev * vkbd = NULL;

static int vkbd_open(struct inode * inode, struct file * file) {
	struct proc_dir_entry * proc_data = PDE(inode);
	file->private_data = proc_data->data;
	return 0;
}

static ssize_t vkbd_read(struct file * file, char __user * buffer, size_t count, loff_t * offset)
{
	struct vkbd_dev * dev = file->private_data;

	char buff[VKBD_INPUT_LEN+1];

	spin_lock(&dev->lock);
	snprintf(buff, VKBD_INPUT_LEN+1, "%+04ld", dev->last_entry);
	spin_unlock(&dev->lock);

	if(*offset > VKBD_INPUT_LEN) {
		count = 0;
	} else if(count+*offset > VKBD_INPUT_LEN) {
		count = VKBD_INPUT_LEN - *offset;
	}

	if(copy_to_user(buffer, buff+*offset, count)) {
		count = -EFAULT;
	}

	*offset += count;

	return count;
}

static ssize_t vkbd_write(struct file * file, const char __user * buffer, size_t count, loff_t * offset)
{
	short key = 0;
	short type = VKBD_PRESS;
	char buff[VKBD_INPUT_LEN+1];
	struct vkbd_dev * dev = file->private_data;

	memset(buff, 0, sizeof(char)*(VKBD_INPUT_LEN+1));

	if(count < VKBD_INPUT_LEN) {
		return count;
	}

	if(copy_from_user(buff, buffer, VKBD_INPUT_LEN)) {
		return -EFAULT;
	}

	spin_lock(&dev->lock);
	dev->last_entry = simple_strtol(buff, NULL, 10);
	if(dev->last_entry == 0) {
		dev->last_entry = simple_strtol(buff+1, NULL, 10);
	}
	key = dev->last_entry;
	spin_unlock(&dev->lock);

	if(key < 0) {
		type = VKBD_RELEASE;
		key = -key;
	}

	printk(KERN_DEBUG "event %s code %d\n", type==VKBD_RELEASE?"VKBD_RELEASE":"VKBD_PRESS", key);

	input_report_key(dev->device, key, type);
	input_sync(dev->device);

	return VKBD_INPUT_LEN;
}

struct file_operations proc_ops = {
	.owner = THIS_MODULE,
	.open = vkbd_open,
	.read = vkbd_read,
	.write = vkbd_write,
};

static int __init vkbd_init(void)	
{
	int err = 0;
	int i = 0;

	printk(KERN_INFO "loading virtual keyboard driver\n");

	vkbd = kmalloc(sizeof(struct vkbd_dev), GFP_KERNEL);
	if(vkbd == NULL) {
		printk(KERN_ERR "cannot allocate vkbd device structure\n");
		err = -ENOMEM;
		goto err_alloc_device;
	}

	if(proc_create_data(VKBD_PROC_NAME, 0666, NULL, &proc_ops, vkbd) == NULL) {
		printk(KERN_ERR "cannot initialize /proc/kbd\n");
		err = -ENOMEM;
		goto err_alloc_proc;
	}

	vkbd->device = input_allocate_device();
	if(vkbd->device == NULL) {
		printk(KERN_ERR "cannot allocate vkbd device\n");
		err = -ENOMEM;
		goto err_alloc_dev;
	}

	vkbd->device->name = "vkbd";
	vkbd->device->id.bustype = BUS_VIRTUAL;
	vkbd->device->id.product = 0x0000;
	vkbd->device->id.vendor = 0x0000;
	vkbd->device->id.version = 0x0000;

	vkbd->device->evbit[0] = BIT_MASK(EV_KEY);
	       
	for(i = 0; i < BIT_WORD(KEY_MAX); i++) {
		vkbd->device->keybit[i] = 0xffff;
	}

	if(input_register_device(vkbd->device)) {
		printk(KERN_ERR "cannot register vkbd input device\n");
		err = -ENODEV;
		goto err_init_dev;
	}

	return 0;

err_init_dev:
	input_free_device(vkbd->device);
err_alloc_dev:
	remove_proc_entry(VKBD_PROC_NAME, NULL);
err_alloc_proc:
	kfree(vkbd);
err_alloc_device:

	return err;
}

static void __exit vkbd_end(void)	
{
	printk(KERN_INFO "unloading virtual keyboard driver\n");
	
	input_unregister_device(vkbd->device);
	input_free_device(vkbd->device);
	remove_proc_entry(VKBD_PROC_NAME, NULL);
	kfree(vkbd);
}

module_init(vkbd_init);
module_exit(vkbd_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("blunderer <blunderer@blunderer.org>");
MODULE_DESCRIPTION("emulate kbd event thru /proc/kbd");

