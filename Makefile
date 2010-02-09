obj-$(CONFIG_RKFS) = rkfs.o

rkfs-y = utils.o bitmap.o super.o file.o inode.o balloc.o ialloc.o asops.o itree.o namei.o dir.o

KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) CONFIG_RKFS=m modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) CONFIG_RKFS=m clean
