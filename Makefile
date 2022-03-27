KDIR ?= /lib/modules/$(shell uname -r)/build
obj-m	:= vinput.o vkbd.o vts.o vmouse.o

.PHONY: all
all: kmod

kmod:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
