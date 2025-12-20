obj-m += vtfs.o

# vtfs-y := source/vtfs.o source/vtfs_ram_backend.o
vtfs-y := source/vtfs.o source/vtfs_lavnetfs_backend.o source/http.o

PWD := $(CURDIR)
KDIR := /lib/modules/$(shell uname -r)/build

EXTRA_CFLAGS := -Wall -g

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf .cache
