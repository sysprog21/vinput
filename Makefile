KVERSION ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVERSION)/build
obj-m	:= vinput_mod.o vkbd_mod.o vts_mt_mod.o vmouse_mod.o

vinput_mod-y := vinput.o
vkbd_mod-y := vkbd.o
vts_mt_mod-y := vts_mt.o
vmouse_mod-y := vmouse.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
