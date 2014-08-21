KDIR ?= /lib/modules/$(shell uname -r)/build
obj-m	:= vinput_mod.o vkbd_mod.o

vinput_mod-y := vinput.o
vkbd_mod-y := vkbd.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
