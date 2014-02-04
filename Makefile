
KERN_VERSION = $(shell uname -r)

obj-m	:= vinput_mod.o vkbd_mod.o

vinput_mod-y := vinput.o
vkbd_mod-y := vkbd.o

all:
	make -C /lib/modules/$(KERN_VERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KERN_VERSION)/build M=$(PWD) clean

