# vinput: A collection of virtual input device drivers for Linux

## Prerequisite

The following package must be installed before building `vinput`.

In order to compile the kernel driver successfully, package versions
of currently used kernel, kernel-devel and kernel-headers need to be matched.
```shell
$ sudo apt install linux-headers-$(uname -r)
```

## Build and Run

After running `make`, you should be able to generate the following files:
* `vinput.ko` - virtual input device layer
* `vkbd.ko` - virtual keyboard
* `vmouse.ko` - virtual mouse
* `vts.ko` - virtual multitouch screen inputs

The module can be loaded to Linux kernel by runnning the command:
```shell
$ sudo insmod vinput.ko
$ sudo insmod vkbd.ko
```

## Kernel API

`vinput` is a API to allow easy development of virtual input drivers.

The drivers needs to export a `vinput_devince` function that contains the virtual device name and `vinput_ops` structure that describes:
- the init function: `init`
- the input event injection function: `send`
- the readback function: `read`

Then using `vinput_register_device` and `vinput_unregister_device` will add a new device to the list of support virtual input devices.

```c
int init(struct vinput *);
```

This function is passed a struct vinput already initialized with an allocated struct `input_dev`.
The `init` function is responsible for initializing the capabilities of the input device and register it.

```c
int send(struct vinput *, char *, int);
```

This function will receive a user string to interpret and inject the event using the `input_report_XXXX` or `input_event` call.
The string is already copied from user.

```c
int read(struct vinput *, char *, int);
```

This function is used for debugging and should fill the buffer parameter with the last event sent in the virtual input device format.
The buffer will then be copied to user.

## Userland API
`vinput` devices are created and destroyed using sysfs.
event injection is done through a `/dev` node.

The device name will be used by the userland to export a new virtual input device.

To create a `vinputX` sysfs entry and `/dev` node.
```shell
$ echo "vkbd" | sudo tee /sys/class/vinput/export
```

To unexport the device, just echo its id in unexport:
```shell
$ echo "0" | sudo tee /sys/class/vinput/unexport
```

## vkbd
This is the virtual keyboard. It supports all `KEY_MAX` keycodes.
The injection format is the `KEY_CODE` such as defined in `linux/input.h`.
A positive value means `KEY_PRESS` while a negative value is a `KEY_RELEASE`.
The keyboard supports repetition when the key stays pressed for too long.

Simulate a key press on "g" (`KEY_G` = 34)
```shell
$ echo "+34" | sudo tee /dev/vinput0
```

Simulate a key release on "g" (`KEY_G` = 34)
```shell
$ echo "-34" | sudo tee /dev/vinput0
```

## License

`vinput` is released under the GNU General Public License. Use of this source code is governed by
the GPL License version 2 that can be found in the LICENSE file.
