KVERSION ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVERSION)/build
obj-m	:= vinput.o vkbd.o vts_mt.o vmouse.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
