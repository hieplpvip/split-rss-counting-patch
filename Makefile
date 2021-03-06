obj-m = split-rss-counting-patch.o

split-rss-counting-patch-objs:= core.o patcher.o utils.o

# This is here to avoid 'smart' optimization of the compiler.
ccflags-y := -O0

all: lkm rsstest

lkm:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

rsstest:
	gcc -o rsstest rsstest.c

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f rsstest

test: lkm
	sudo dmesg -C
	sudo insmod split-rss-counting-patch.ko
	sudo rmmod split-rss-counting-patch.ko
	dmesg

load: lkm
	sudo dmesg -C
	sudo insmod split-rss-counting-patch.ko
	dmesg

unload: lkm
	sudo dmesg -C
	sudo rmmod split-rss-counting-patch.ko
	dmesg
