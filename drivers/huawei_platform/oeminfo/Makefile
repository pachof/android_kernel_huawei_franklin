#
# Makefile for device detect devices
#
obj-$(CONFIG_HUAWEI_OEMINFO) += oeminfo_lite.o

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6853" "mt6768"))
	ifneq (,$(filter full_internal full_oversea, $(SET_SYSTEM_PARTITION)))
		ccflags-y += -DOEMINFO_WP_FEATURE
	else
		ccflags-y += -DOEMINFO_LITE_FEATURE
	endif
else
	ccflags-y += -DOEMINFO_LITE_FEATURE
endif
