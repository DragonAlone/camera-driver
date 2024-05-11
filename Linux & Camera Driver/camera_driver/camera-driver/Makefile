ifeq ($(KERNELRELEASE),)
X86_KERNEL_BUILD = /lib/modules/$(shell uname -r)/build
ARM_KERNEL_BUILD = /home/linux/imx6ull/kernel/linux-imx-driver
MODULE_PATH = $(shell pwd)

x86_module:
	make -C $(X86_KERNEL_BUILD) M=$(MODULE_PATH) modules

arm_module:
	make -C $(ARM_KERNEL_BUILD) M=$(MODULE_PATH) modules ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
	cp ov2640_camera_driver.ko /home/linux/imx6ull/fs/rootfs
clean:
	make -C $(X86_KERNEL_BUILD) M=$(MODULE_PATH) clean

else
	obj-m = ov2640_camera_driver.o
	ov2640_camera_driver-objs = i2c_ov2640_driver.o ov2640_reg.o
endif
