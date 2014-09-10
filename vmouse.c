#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/slab.h>

#include "vinput.h"

#define VINPUT_MTS "vmouse"

static int vinput_vmouse_init(struct vinput *vinput)
{
	int *buttons = kmalloc(sizeof(int), GFP_KERNEL);

	__set_bit(EV_REL, vinput->input->evbit);
	__set_bit(REL_X, vinput->input->relbit);
	__set_bit(REL_Y, vinput->input->relbit);
	__set_bit(REL_WHEEL, vinput->input->relbit);

	__set_bit(EV_KEY, vinput->input->evbit);
	__set_bit(BTN_LEFT, vinput->input->keybit);
	__set_bit(BTN_RIGHT, vinput->input->keybit);
	__set_bit(BTN_MIDDLE, vinput->input->keybit);

	*buttons = 0;
	vinput->priv_data = buttons;

	return input_register_device(vinput->input);
}

static int vinput_vmouse_kill(struct vinput *vinput)
{
	int *buttons = vinput->priv_data;
	kfree(buttons);
	return 0;
}

static int vinput_vmouse_read(struct vinput *vinput, char *buff, int len)
{ 
	return len;
}

#define VBUTTON_LEFT	0
#define VBUTTON_RIGHT	1
#define VBUTTON_MIDDLE	2

static int vinput_vmouse_send(struct vinput *vinput, char *buff, int len)
{
	int ret;
	int x, y, wheel;
	int buttons;
	int *state = vinput->priv_data;

	ret = sscanf(buff, "%d,%d,%d,%d", &x, &y, &wheel, &buttons);
	if (ret != 4) {
		dev_warn(&vinput->dev, "Invalid input format: x,y,wheel,buttons\n");
		len = -EINVAL;
	} else {
		if (x)
			input_report_rel(vinput->input, REL_X, x);
		if (y)
			input_report_rel(vinput->input, REL_Y, y);
		if (wheel)
			input_report_rel(vinput->input, REL_WHEEL, wheel);

		if ((*state | buttons) & (0x1 << VBUTTON_LEFT))
			input_report_key(vinput->input, BTN_LEFT, 1 & (buttons >> VBUTTON_LEFT));
		else if ((*state | buttons) & (0x1 << VBUTTON_RIGHT))
			input_report_key(vinput->input, BTN_RIGHT, 1 & (buttons >> VBUTTON_RIGHT));
		else if ((*state | buttons) & (0x1 << VBUTTON_MIDDLE))
			input_report_key(vinput->input, BTN_MIDDLE, 1 & (buttons >> VBUTTON_MIDDLE));

		*state = buttons;

		input_sync(vinput->input);
	}

	return len;
}

static struct vinput_ops vmouse_ops = {
	.init = vinput_vmouse_init,
	.kill = vinput_vmouse_kill,
	.send = vinput_vmouse_send,
	.read = vinput_vmouse_read,
};

static struct vinput_device vmouse_dev = {
	.name = VINPUT_MTS,
	.ops = &vmouse_ops,
};

static int __init vmouse_init(void)
{
	return vinput_register(&vmouse_dev);
}

static void __exit vmouse_end(void)
{
	vinput_unregister(&vmouse_dev);
}

module_init(vmouse_init);
module_exit(vmouse_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tristan Lelong <tristan.lelong@blunderer.org>");
MODULE_DESCRIPTION("emulate mouse input events thru /dev/vinput");
