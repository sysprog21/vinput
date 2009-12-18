
KERN_VERSION = $(shell uname -r)

#:KERN_VERSION = 2.6.31.4

obj-m += vkbd.o

all:
	make -C /lib/modules/$(KERN_VERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KERN_VERSION)/build M=$(PWD) clean

