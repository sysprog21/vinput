
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#include "vinput.h"

static LIST_HEAD(vinput_devices);
static LIST_HEAD(vinput_vdevices);

static dev_t vinput_dev;
static struct spinlock vinput_lock;
static struct class vinput_class;

struct vinput_device *vinput_get_device_by_type(const char *type)
{
	int found = 0;
	struct vinput_device *device;
	struct list_head *curr;

	spin_lock(&vinput_lock);
	list_for_each(curr, &vinput_devices) {
		device = list_entry(curr, struct vinput_device, list);
		if (strncmp(type, device->name, strlen(device->name)) == 0) {
			found = 1;;
			break;
		}
	}
	spin_unlock(&vinput_lock);

	if (found)
		return device;
	return ERR_PTR(-ENODEV);
}

struct vinput *vinput_get_vdevice_by_id(long id)
{
	int found = 0;
	struct vinput *vinput;
	struct list_head *curr;

	spin_lock(&vinput_lock);
	list_for_each(curr, &vinput_vdevices) {
		vinput = list_entry(curr, struct vinput, list);
		if (vinput->id == id) {
			found = 1;
			break;
		}
	}
	spin_unlock(&vinput_lock);

	if (found)
		return vinput;
	return NULL;
}

static int vinput_open(struct inode *inode, struct file *file)
{
	int err = 0;
	int found = 0;
	long minor = iminor(inode);
	long devno = MKDEV(MAJOR(vinput_dev), minor);
	struct list_head *curr;
	struct vinput *vinput = NULL;

	spin_lock(&vinput_lock);
	list_for_each(curr, &vinput_vdevices) {
		vinput = list_entry(curr, struct vinput, list);
		if (vinput->devno == devno) {
			found = 1;
			get_device(vinput->dev);
			break;
		}
	}
	spin_unlock(&vinput_lock);

	if (found)
		file->private_data = vinput;
	else
		err = -ENODEV;

	return err;
}

static int vinput_release(struct inode *inode, struct file *file)
{
	struct vinput *vinput = file->private_data;
	put_device(vinput->dev);
	return 0;
}

static ssize_t vinput_read(struct file *file, char __user * buffer,
			   size_t count, loff_t * offset)
{
	int len;
	char buff[VINPUT_MAX_LEN + 1];
	struct vinput *vinput = file->private_data;

	len = vinput->type->ops->read(vinput, buff, count);

	if (*offset > len) {
		count = 0;
	} else if (count + *offset > VINPUT_MAX_LEN) {
		count = len - *offset;
	}

	if (copy_to_user(buffer, buff + *offset, count)) {
		count = -EFAULT;
	}

	*offset += count;

	return count;
}

static ssize_t vinput_write(struct file *file, const char __user * buffer,
			    size_t count, loff_t * offset)
{
	char buff[VINPUT_MAX_LEN + 1];
	struct vinput *vinput = file->private_data;

	memset(buff, 0, sizeof(char) * (VINPUT_MAX_LEN + 1));

	if (count > VINPUT_MAX_LEN) {
		pr_info("vinput: Too long. %d bytes allowed\n", VINPUT_MAX_LEN);
		return -EINVAL;
	}

	if (copy_from_user(buff, buffer, VINPUT_MAX_LEN)) {
		return -EFAULT;
	}

	return vinput->type->ops->send(vinput, buff, count);
}

static struct file_operations vinput_fops = {
	.owner = THIS_MODULE,
	.open = vinput_open,
	.release = vinput_release,
	.read = vinput_read,
	.write = vinput_write,
};

static struct vinput *vinput_create_vdevice(void)
{
	int err;
	struct vinput *vinput = kzalloc(sizeof(struct vinput), GFP_KERNEL);

	try_module_get(THIS_MODULE);
	memset(vinput, 0, sizeof(struct vinput));

	spin_lock_init(&vinput->lock);

	spin_lock(&vinput_lock);
	if (list_empty(&vinput_vdevices))
		vinput->id = 0;
	else
		vinput->id =
		    list_first_entry(&vinput_vdevices, struct vinput,
				     list)->id + 1;
	list_add(&vinput->list, &vinput_vdevices);
	spin_unlock(&vinput_lock);

	/* allocate the input device */
	vinput->input = input_allocate_device();
	if (vinput->input == NULL) {
		pr_err("vinput: Cannot allocate vinput input device\n");
		err = -ENOMEM;
		goto fail_input_dev;
	}

	/* Fill in the data structures */
	cdev_init(&vinput->cdev, &vinput_fops);
	vinput->cdev.owner = vinput_fops.owner;
	vinput->devno = MKDEV(MAJOR(vinput_dev), vinput->id);

	/* Add the device */
	err = cdev_add(&vinput->cdev, vinput->devno, 1);
	if (err) {
		pr_err("vinput: Can't create char device\n");
		err = -ENODEV;
		goto fail_cdev;
	}
	vinput->dev =
	    device_create(&vinput_class, NULL, vinput->devno, NULL, "vinput%ld",
			  vinput->id);
	if (IS_ERR(vinput->dev)) {
		pr_err("vinput: Can't create device\n");
		err = -ENODEV;
		goto fail_device;
	}

	return vinput;

fail_device:
	cdev_del(&vinput->cdev);
fail_cdev:
	input_free_device(vinput->input);
fail_input_dev:
	spin_lock(&vinput_lock);
	list_del(&vinput->list);
	spin_unlock(&vinput_lock);
	module_put(THIS_MODULE);
	kfree(vinput);

	return ERR_PTR(err);
}

static int vinput_register_vdevice(struct vinput *vinput)
{
	int err = 0;

	/* register the input device */
	vinput->input->name = "vinput";
	vinput->input->dev.parent = vinput->dev;

	vinput->input->id.bustype = BUS_VIRTUAL;
	vinput->input->id.product = 0x0000;
	vinput->input->id.vendor = 0x0000;
	vinput->input->id.version = 0x0000;

	err = vinput->type->ops->init(vinput);

	dev_info(vinput->dev, "Registered virtual input %s %ld\n",
		 vinput->type->name, vinput->id);

	return err;
}

static void vinput_unregister_vdevice(struct vinput *vinput)
{
	input_unregister_device(vinput->input);
	input_free_device(vinput->input);
}

static void __vinput_destroy_vdevice(struct vinput *vinput)
{
	dev_info(vinput->dev, "Removing virtual input %ld\n", vinput->id);
	device_destroy(&vinput_class, vinput->devno);
	cdev_del(&vinput->cdev);

	list_del(&vinput->list);

	module_put(THIS_MODULE);

	kfree(vinput);
}

static void vinput_destroy_vdevice(struct vinput *vinput)
{
	spin_lock(&vinput_lock);
	__vinput_destroy_vdevice(vinput);
	spin_unlock(&vinput_lock);
}

static ssize_t export_store(struct class *class, struct class_attribute *attr,
			    const char *buf, size_t len)
{
	int err;
	struct vinput *vinput;
	struct vinput_device *device;

	device = vinput_get_device_by_type(buf);
	if (IS_ERR(device)) {
		pr_info("vinput: This virtual device isn't registered\n");
		err = PTR_ERR(device);
		goto fail;
	}

	vinput = vinput_create_vdevice();
	if (IS_ERR(vinput)) {
		err = PTR_ERR(vinput);
		goto fail;
	}

	vinput->type = device;
	err = vinput_register_vdevice(vinput);

	if (err < 0) {
		goto fail_register;
	}

	return len;
fail_register:
	vinput_destroy_vdevice(vinput);
fail:
	return err;
}

static ssize_t unexport_store(struct class *class, struct class_attribute *attr,
			      const char *buf, size_t len)
{
	int err = -ENODEV;
	unsigned long id;
	struct vinput *vinput;

	id = simple_strtol(buf, NULL, 10);
	vinput = vinput_get_vdevice_by_id(id);

	if (vinput == NULL) {
		pr_err("vinput: No such vinput device %ld\n", id);
		err = -ENODEV;
		goto failed;
	}

	vinput_unregister_vdevice(vinput);
	vinput_destroy_vdevice(vinput);

	return len;
failed:
	return err;
}

static struct class_attribute vinput_class_attrs[] = {
	__ATTR(export, 0200, NULL, export_store),
	__ATTR(unexport, 0200, NULL, unexport_store),
	__ATTR_NULL,
};

static struct class vinput_class = {
	.name = "vinput",
	.owner = THIS_MODULE,
	.class_attrs = vinput_class_attrs,
};

int vinput_register(struct vinput_device *dev)
{
	spin_lock(&vinput_lock);
	list_add(&dev->list, &vinput_devices);
	spin_unlock(&vinput_lock);
	pr_info("vinput: registered new virtual input device '%s'\n",
		dev->name);
	return 0;
}

EXPORT_SYMBOL(vinput_register);

void vinput_unregister(struct vinput_device *dev)
{
	struct list_head *curr, *next;

	spin_lock(&vinput_lock);
	list_for_each_safe(curr, next, &vinput_vdevices) {
		struct vinput *vinput = list_entry(curr, struct vinput, list);
		vinput_unregister_vdevice(vinput);
		__vinput_destroy_vdevice(vinput);
	}
	list_del(&dev->list);
	spin_unlock(&vinput_lock);
}

EXPORT_SYMBOL(vinput_unregister);

static int __init vinput_init(void)
{
	int err = 0;

	pr_info("vinput: Loading virtual input driver\n");

	err = alloc_chrdev_region(&vinput_dev, 0, MAX_VINPUT, "vinput");
	if (err < 0) {
		pr_err("vinput: Unable to allocate char dev region\n");
		goto failed_alloc;
	}

	spin_lock_init(&vinput_lock);

	err = class_register(&vinput_class);
	if (err < 0) {
		pr_err("vinput: Unable to register vinput class\n");
		goto failed_class;
	}

	return 0;
failed_class:
	class_unregister(&vinput_class);
failed_alloc:
	return err;
}

static void __exit vinput_end(void)
{
	pr_info("vinput: Unloading virtual input driver\n");

	class_unregister(&vinput_class);
}

module_init(vinput_init);
module_exit(vinput_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tristan Lelong <tristan.lelong@blunderer.org>");
MODULE_DESCRIPTION("emulate input events thru /dev/[vkbd | vts | vmouse]");
