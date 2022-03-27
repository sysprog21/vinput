#include <linux/device.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "vinput.h"

#define VINPUT_TS "vts"
#define VTS_CALIB_DONE 0x001f

enum vts_init_flags {
    calib_type,
    calib_x,
    calib_y,
    calib_z,
    calib_points,
};

enum vts_attributes {
    attr_type,
    attr_max_x,
    attr_max_y,
    attr_max_z,
    attr_max_points,
};

static struct device_attribute vts_attrs[];

struct mtslot {
    int updated;
    int id;
    int x;
    int y;
    int z;
};

struct vts_data {
    int registered;
    int init_flag;

    enum { TYPE_NONE, TYPE_A, TYPE_B } type;
    int max_x;
    int max_y;
    int max_z;
    int max_points;

    struct mtslot *slots;
};

static void vinput_vts_register_final(struct device *dev)
{
    int i;
    int err = 0;
    struct vinput *vinput = dev_to_vinput(dev);
    struct vts_data *drvdata = (struct vts_data *) vinput->priv_data;

    input_set_abs_params(vinput->input, ABS_X, 0, drvdata->max_x, 0, 0);
    input_set_abs_params(vinput->input, ABS_Y, 0, drvdata->max_y, 0, 0);
    input_set_abs_params(vinput->input, ABS_MT_POSITION_X, 0, drvdata->max_x, 0,
                         0);
    input_set_abs_params(vinput->input, ABS_MT_POSITION_Y, 0, drvdata->max_y, 0,
                         0);
    input_set_abs_params(vinput->input, ABS_MT_DISTANCE, 0, drvdata->max_z, 0,
                         0);
    input_set_abs_params(vinput->input, ABS_MT_PRESSURE, 0, drvdata->max_z, 0,
                         0);

    drvdata->slots =
        kzalloc(sizeof(struct mtslot) * drvdata->max_points, GFP_KERNEL);
    for (i = 0; i < drvdata->max_points; i++)
        drvdata->slots[i].id = -1;

    if (drvdata->type == TYPE_B)
        input_mt_init_slots(vinput->input, drvdata->max_points, 0);

    if (input_register_device(vinput->input)) {
        dev_err(&vinput->dev, "cannot register vinput input device\n");
        err = -ENODEV;
    }
    drvdata->registered = 1;

    return;
}

static void vinput_vts_calib_done(struct device *dev, int flag)
{
    struct vinput *vinput = dev_to_vinput(dev);
    struct vts_data *drvdata = (struct vts_data *) vinput->priv_data;

    drvdata->init_flag |= (1 << flag);

    if ((drvdata->init_flag & VTS_CALIB_DONE) == VTS_CALIB_DONE)
        vinput_vts_register_final(dev);
}

static ssize_t type_show(struct device *dev,
                         struct device_attribute *attr,
                         char *buf)
{
    struct vinput *vinput = dev_to_vinput(dev);
    struct vts_data *drvdata = (struct vts_data *) vinput->priv_data;

    if (!drvdata)
        return 0;
    if (drvdata->type == TYPE_NONE)
        return sprintf(buf, "not set\n");

    return sprintf(buf, "%c\n", drvdata->type == TYPE_A ? 'A' : 'B');
};

static ssize_t type_store(struct device *dev,
                          struct device_attribute *attr,
                          const char *buf,
                          size_t size)
{
    struct vinput *vinput = dev_to_vinput(dev);
    struct vts_data *drvdata = (struct vts_data *) vinput->priv_data;

    if (!drvdata)
        return 0;

    if (drvdata->registered)
        return -EPERM;

    if (buf[0] == 'A' || buf[0] == 'a')
        drvdata->type = TYPE_A;
    else if (buf[0] == 'B' || buf[0] == 'b')
        drvdata->type = TYPE_B;
    else
        return -EPROTONOSUPPORT;

    vinput_vts_calib_done(dev, calib_type);

    return size;
};

static ssize_t calib_show(struct device *dev,
                          struct device_attribute *attr,
                          char *buf)
{
    struct vinput *vinput = dev_to_vinput(dev);
    struct vts_data *drvdata = (struct vts_data *) vinput->priv_data;

    if (!drvdata)
        return 0;

    if (attr == &vts_attrs[attr_max_x]) {
        if (drvdata->max_x < 0)
            return sprintf(buf, "not set\n");
        else
            return sprintf(buf, "%d\n", drvdata->max_x);
    } else if (attr == &vts_attrs[attr_max_y]) {
        if (drvdata->max_y < 0)
            return sprintf(buf, "not set\n");
        else
            return sprintf(buf, "%d\n", drvdata->max_y);
    } else if (attr == &vts_attrs[attr_max_z]) {
        if (drvdata->max_z < 0)
            return sprintf(buf, "not set\n");
        else
            return sprintf(buf, "%d\n", drvdata->max_z);
    } else if (attr == &vts_attrs[attr_max_points]) {
        if (drvdata->max_points < 0)
            return sprintf(buf, "not set\n");
        else
            return sprintf(buf, "%d\n", drvdata->max_points);
    }
    return -EINVAL;
};

static ssize_t calib_store(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf,
                           size_t size)
{
    int val;
    int flag;
    int status;
    struct vinput *vinput = dev_to_vinput(dev);
    struct vts_data *drvdata = (struct vts_data *) vinput->priv_data;

    if (!drvdata)
        return 0;

    if (drvdata->registered)
        return -EPERM;

    status = kstrtoint(buf, 10, &val);
    if (status < 0)
        return status;

    if (attr == &vts_attrs[attr_max_x]) {
        drvdata->max_x = val;
        flag = calib_x;
    } else if (attr == &vts_attrs[attr_max_y]) {
        drvdata->max_y = val;
        flag = calib_y;
    } else if (attr == &vts_attrs[attr_max_z]) {
        drvdata->max_z = val;
        flag = calib_z;
    } else if (attr == &vts_attrs[attr_max_points]) {
        drvdata->max_points = val;
        flag = calib_points;
    } else {
        return -EPROTO;
    }

    vinput_vts_calib_done(dev, flag);

    return size;
};

static struct device_attribute vts_attrs[] = {
    __ATTR(type, S_IWUSR | S_IRUGO, type_show, type_store),
    __ATTR(max_x, S_IWUSR | S_IRUGO, calib_show, calib_store),
    __ATTR(max_y, S_IWUSR | S_IRUGO, calib_show, calib_store),
    __ATTR(max_z, S_IWUSR | S_IRUGO, calib_show, calib_store),
    __ATTR(max_points, S_IWUSR | S_IRUGO, calib_show, calib_store),
    __ATTR_NULL,
};

static int vinput_vts_init(struct vinput *vinput)
{
    int err = 0;
    struct vts_data *drvdata;
    struct device_attribute *attr = vts_attrs;

    drvdata = kmalloc(sizeof(struct vts_data), GFP_KERNEL);
    vinput->priv_data = drvdata;

    drvdata->registered = 0;
    drvdata->init_flag = 0;
    drvdata->type = TYPE_NONE;
    drvdata->max_x = -1;
    drvdata->max_y = -1;
    drvdata->max_points = -1;
    drvdata->slots = NULL;

    __set_bit(EV_ABS, vinput->input->evbit);
    __set_bit(EV_KEY, vinput->input->evbit);

    __set_bit(BTN_TOUCH, vinput->input->keybit);

    while (attr->attr.name) {
        dev_dbg(&vinput->dev, "Creating new attributes: %s\n", attr->attr.name);
        device_create_file(&vinput->dev, attr++);
    }

    return err;
}

static int vinput_vts_kill(struct vinput *vinput)
{
    struct vts_data *drvdata = (struct vts_data *) vinput->priv_data;
    struct device_attribute *attr = vts_attrs;

    while (attr->attr.name)
        device_remove_file(&vinput->dev, attr++);
    kfree(drvdata->slots);
    kfree(drvdata);

    return 0;
}

static int vinput_vts_read(struct vinput *vinput, char *buff, int len)
{
    struct vts_data *drvdata = (struct vts_data *) vinput->priv_data;

    if (!drvdata->registered)
        return -EINVAL;
    return len;
}

static int vinput_vts_find_slot(struct vts_data *drvdata, int id)
{
    int i;

    for (i = 0; i < drvdata->max_points; i++) {
        if (drvdata->type == TYPE_A && drvdata->slots[i].updated)
            continue;
        else if (drvdata->type == TYPE_A)
            break;
        else if (drvdata->slots[i].id == id)
            break;
    }

    if ((drvdata->type == TYPE_B) && (i == drvdata->max_points))
        for (i = 0; i < drvdata->max_points; i++)
            if (drvdata->slots[i].id == -1)
                break;

    return (i == drvdata->max_points) ? -1 : i;
}

static int vinput_vts_parse(struct vinput *vinput, char *buff, int len)
{
    char *slot;
    int slot_id;
    int id, x, y, z, ret;
    struct vts_data *drvdata = (struct vts_data *) vinput->priv_data;

    while ((slot = strsep(&buff, ";"))) {
        ret = sscanf(slot, "%d,%d,%d,%d", &id, &x, &y, &z);
        if (ret != 4) {
            dev_warn(&vinput->dev, "Invalid input format\n");
            len = -EINVAL;
            break;
        }
        slot_id = vinput_vts_find_slot(drvdata, id);
        pr_info("slot=%d\n", slot_id);

        if (slot_id < 0) {
            dev_warn(&vinput->dev, "No available slots. Max=%d\n",
                     drvdata->max_points);
            len = -EINVAL;
            break;
        }

        if (z == 0)
            drvdata->slots[slot_id].id = -1;
        else
            drvdata->slots[slot_id].id = id;
        drvdata->slots[slot_id].x = x;
        drvdata->slots[slot_id].y = y;
        drvdata->slots[slot_id].z = z;
        drvdata->slots[slot_id].updated = 1;
        pr_info("NEW TOUCH EVT[%d]: id=%d (%d,%d,%d)\n", slot_id,
                drvdata->slots[slot_id].id, x, y, z);
    }

    return len;
}

static int vinput_vts_send(struct vinput *vinput, char *buff, int len)
{
    int i;
    int ret;
    struct vts_data *drvdata = (struct vts_data *) vinput->priv_data;

    if (!drvdata->registered)
        return -EINVAL;

    /* parse slots */
    ret = vinput_vts_parse(vinput, buff, len);
    if (ret < 0)
        return ret;

    /* process slots */
    for (i = 0; i < drvdata->max_points; i++) {
        if (drvdata->slots[i].updated) {
            if (drvdata->type == TYPE_B) {
                input_mt_slot(vinput->input, i);
                input_report_abs(vinput->input, ABS_MT_TRACKING_ID,
                                 drvdata->slots[i].id);
                input_report_abs(vinput->input, ABS_MT_TOOL_TYPE,
                                 MT_TOOL_FINGER);
            }

            input_report_abs(vinput->input, ABS_MT_POSITION_X,
                             drvdata->slots[i].x);
            input_report_abs(vinput->input, ABS_MT_POSITION_Y,
                             drvdata->slots[i].y);
            if (drvdata->slots[i].z > 0)
                input_report_abs(vinput->input, ABS_MT_PRESSURE,
                                 drvdata->slots[i].z);
            else if (drvdata->slots[i].z < 0)
                input_report_abs(vinput->input, ABS_MT_DISTANCE,
                                 -drvdata->slots[i].z);

            if (drvdata->type == TYPE_A)
                input_mt_sync(vinput->input);
            drvdata->slots[i].updated = 0;
            pr_info("SEND TOUCH EVT[%d]: id=%d\n", i, drvdata->slots[i].id);
        }
    }

    input_mt_report_pointer_emulation(vinput->input, true);
    input_sync(vinput->input);

    return len;
}

static struct vinput_ops vts_ops = {
    .init = vinput_vts_init,
    .kill = vinput_vts_kill,
    .send = vinput_vts_send,
    .read = vinput_vts_read,
};

static struct vinput_device vts_dev = {
    .name = VINPUT_TS,
    .ops = &vts_ops,
};

static int __init vts_init(void)
{
    return vinput_register(&vts_dev);
}

static void __exit vts_end(void)
{
    vinput_unregister(&vts_dev);
}

module_init(vts_init);
module_exit(vts_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jean-Baptiste Theou <jbtheou@gmail.com>");
MODULE_DESCRIPTION("emulate multitouch screen input events thru /dev/vinput");
