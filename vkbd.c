
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/input.h>

#define VKBD_PROC_NAME	"kbd"
#define VKBD_INPUT_LEN	5

#define VKBD_RELEASE	0
#define VKBD_PRESS	1

static long vkbd_last_entry = 0;
static struct input_dev *vkbd_dev = NULL;
static struct proc_dir_entry * vkbd_input;

static int read_last_vkbd_event(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char buff[VKBD_INPUT_LEN+1];
	snprintf(buff, VKBD_INPUT_LEN+1, "%+04ld",vkbd_last_entry);

	if(off > VKBD_INPUT_LEN) {
		*eof = 1;
		return 0;
	}

	if(count+off > VKBD_INPUT_LEN) {
		count = VKBD_INPUT_LEN - off;
	}

	memcpy(page, buff+off, count);
	return count;
}

static int write_event_to_vkbd(struct file *file, const char *buffer, unsigned long count, void *data)
{
	short key = 0;
	short type = VKBD_PRESS;
	char buff[VKBD_INPUT_LEN+1];
	memset(buff, 0, sizeof(char)*(VKBD_INPUT_LEN+1));

	if(count < VKBD_INPUT_LEN) {
		return count;
	}

	if(copy_from_user(buff, buffer, VKBD_INPUT_LEN)) {
		return -EFAULT;
	}

	vkbd_last_entry = simple_strtol(buff, NULL, 10);
	if(vkbd_last_entry == 0) {
		vkbd_last_entry = simple_strtol(buff+1, NULL, 10);
	}

	key = vkbd_last_entry;
	if(key < 0) {
		type = VKBD_RELEASE;
		key = -key;
	}

	printk(KERN_INFO "event %s code %d\n", type==VKBD_RELEASE?"VKBD_RELEASE":"VKBD_PRESS", key);
	input_report_key(vkbd_dev, key, type);
	input_sync(vkbd_dev);

	return VKBD_INPUT_LEN;
}

static int __init vkbd_init(void)	
{
	int err, i;
	printk(KERN_INFO "loading virtual keyboard driver\n");

	vkbd_input = create_proc_entry(VKBD_PROC_NAME, 0666, NULL);
	if(vkbd_input == NULL) {
		remove_proc_entry(VKBD_PROC_NAME, NULL);
		printk(KERN_ERR "cannot initialize /proc/kbd\n");
		return -1;
	}

	vkbd_input->owner = THIS_MODULE;
	vkbd_input->write_proc = write_event_to_vkbd;
	vkbd_input->read_proc = read_last_vkbd_event;
	vkbd_input->uid = 0;
	vkbd_input->gid = 0;
	vkbd_input->size = VKBD_INPUT_LEN+1;

	vkbd_dev = input_allocate_device();

	vkbd_dev->name = "vkbd";
	vkbd_dev->id.bustype = BUS_VIRTUAL;
	vkbd_dev->id.product = 0x0000;
	vkbd_dev->id.vendor = 0x0000;
	vkbd_dev->id.version = 0x0000;

	vkbd_dev->evbit[0] = BIT_MASK(EV_KEY); // | BIT_MASK(EV_REP);
	       
	for(i = 0; i < BIT_WORD(KEY_MAX); i++) {
		vkbd_dev->keybit[i] = 0xffff;
	}

	err = input_register_device(vkbd_dev);
	if(err) {
		printk(KERN_ERR "cannot register input device\n");
		input_free_device(vkbd_dev);
		return -1;
	}

	return 0;
}

static void __exit vkbd_end(void)	
{
	printk(KERN_INFO "unloading virtual keyboard driver\n");
	remove_proc_entry(VKBD_PROC_NAME, NULL);
	
	input_unregister_device(vkbd_dev);
	input_free_device(vkbd_dev);
}

module_init(vkbd_init);
module_exit(vkbd_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("blunderer <blunderer@blunderer.org>");
MODULE_DESCRIPTION("emulate kbd event thru /proc/kbd");

