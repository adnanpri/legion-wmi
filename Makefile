obj-m := legion-wmi.o

KVER := $(shell uname -r)
KDIR := /lib/modules/$(KVER)/build
PWD := $(shell pwd)
INSTALL	:= install

SYMBOL_FILE := Module.symvers

EXTRA_CFLAGS += -O2

ccflags-y += -D__CHECK_ENDIAN__

# 

.PHONY: default
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

.PHONY: clean
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# 

.PHONY: modules_install
modules_install:
	$(INSTALL) -d /lib/modules/$(KVER)/drivers/platform/x86/
	$(INSTALL) -m 644 legion-wmi.ko /lib/modules/$(KVER)/drivers/platform/x86/

.PHONY: install
install: modules_install
	depmod -A
	modprobe legion-wmi

# 

.PHONY: modules_uninstall
modules_uninstall:
	rm -f /lib/modules/$(KVER)/drivers/platform/x86/legion-wmi.ko

.PHONY: uninstall
uninstall: modules_uninstall
	modprobe -r legion-wmi
	depmod -A

# 

.PHONY: purge
purge: uninstall clean
