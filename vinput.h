
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#define MAX_VINPUT		32

#define VINPUT_MAX_LEN		16

struct vinput_device;

struct vinput {
	long id;
	long devno;
	long last_entry;
	spinlock_t lock;

	struct device dev;
	struct list_head list;
	struct input_dev *input;
	struct vinput_device *type;
};

struct vinput_ops {
	int (*init) (struct vinput *);
	int (*send) (struct vinput *, char *, int);
	int (*read) (struct vinput *, char *, int);
};

struct vinput_device {
	char name[16];
	struct list_head list;
	struct vinput_ops *ops;
};

int vinput_register(struct vinput_device *dev);
void vinput_unregister(struct vinput_device *dev);
