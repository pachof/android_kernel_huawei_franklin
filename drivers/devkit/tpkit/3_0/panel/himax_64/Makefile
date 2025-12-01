# Makefile for the Himax touchscreen drivers. //himax_dsm.o
# drivers/input/touchscreen
ccflags-y += -I$(srctree)/drivers/devkit/tpkit
obj-$(CONFIG_HIMAX_TS_KIT_3_0) += himax_platform.o himax_ic.o himax_fw_update.o himax_factory_test.o  himax_debug.o

EXTRA_CFLAGS += -Iarch/arm/mach-hisi
EXTRA_CFLAGS += -Idrivers/hisi/tzdriver
EXTRA_CFLAGS += -Idrivers/tzdriver
EXTRA_CFLAGS += -Idrivers/devkit/lcdkit/lcdkit3.0/kernel/common/include

