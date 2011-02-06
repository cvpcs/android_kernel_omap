# Copyright (C) 2009 Motorola, Inc.
#####################################################################
#
# Script creation notes by: David Ding (dding@motorola.com)
# (THIS DESCRIPTION IS NOW OBSOLETE: SEE UPDATE BELOW)
#
# The intention of creating this script is for Moto-Android platform
# common Kernel and kernel modules developer to make kernel zImage and
# driver modules .ko objects. As long as it is in your execution $PATH
# you can place this script anywhere you may preferred. A suggestion
# place can be in $HOME/bin directory, then make PATH=$PATH:$HOME/bin
#
# How to use:
# -----------
# $ cd {top-moto-android-working-dir}
# $ build_kernel
#
# if you are in the wrong place to start your kernel/module build
# script will quit and reminder you go to the RIGHT place to build
#
# UPDATE: 11/21/2009: wqnt78
#
# This makefile is now invoked from kernel/Android.mk. You may build
# the kernel and modules by using the "kernel" target:
#
# source build/envsetup.sh
# lunch
# make kernel
#
# It is also invoked automatically as part of the default target.
#
######################################################################
#set -x

ROOTDIR := $(shell pwd -P)/

ifneq ($(strip $(TOPDIR)),)
    ROOTDIR := $(TOPDIR)
endif

ifeq ($(KERNEL_CROSS_COMPILE),)
	KERNEL_CROSS_COMPILE=$(ROOTDIR)$(TARGET_TOOLS_PREFIX)
endif

ifeq ($(TARGET_PRODUCT),)
	TARGET_PRODUCT := generic
endif

ifeq ($(PRODUCT_OUT),)
	PRODUCT_OUT := out/target/product/$(TARGET_PRODUCT)
endif

ifeq ($(TARGET_OUT),)
	TARGET_OUT := $(PRODUCT_OUT)/system
endif

ifeq ($(HOST_PREBUILT_TAG),)
	HOST_PREBUILT_TAG := linux-x86
endif

ifeq ($(TARGET_BUILD_VARIANT),)
	TARGET_BUILD_VARIANT := user
endif

ifeq ($(HOST_OUT_EXECUTABLES),)
	HOST_OUT_EXECUTABLES := out/host/$(HOST_PREBUILT_TAG)/bin
endif

ifeq ($(DEPMOD),)
	DEPMOD := $(shell which depmod 2> /dev/null || echo $(HOST_OUT_EXECUTABLES)/depmod$(HOST_EXECUTABLE_SUFFIX))
endif

KERNEL_SRC_DIR         := $(ROOTDIR)kernel
KERNEL_BUILD_DIR       := $(ROOTDIR)$(PRODUCT_OUT)/obj/PARTITIONS/kernel_intermediates/build
TARGET_PREBUILT_KERNEL := $(KERNEL_BUILD_DIR)/arch/arm/boot/zImage
KERNEL_CROSS_COMPILE   := $(ROOTDIR)prebuilt/$(HOST_PREBUILT_TAG)/toolchain/arm-eabi-4.4.0/bin/arm-eabi-

KERNEL_WARN_FILTER := $(KERNEL_SRC_DIR)/scripts/gcc_warn_filter.cfg
KERNEL_ERR_LOG     := $(KERNEL_BUILD_DIR)/.kbld_err_log.txt
KMOD_ERR_LOG       := $(KERNEL_BUILD_DIR)/.kmod_err_log.txt
KERNEL_FFLAG       := $(KERNEL_BUILD_DIR)/.filter_ok.txt

DEFCONFIGSRC                := ${KERNEL_SRC_DIR}/arch/arm/configs
LJAPDEFCONFIGSRC            := ${DEFCONFIGSRC}/ext_config
PRODUCT_SPECIFIC_DEFCONFIGS := $(DEFCONFIGSRC)/mapphone_defconfig
_TARGET_DEFCONFIG           := __ext_mapphone_defconfig
TARGET_DEFCONFIG            := $(DEFCONFIGSRC)/$(_TARGET_DEFCONFIG)

MOTO_MOD_INSTALL := $(TARGET_OUT)/lib/modules
WLAN_DRV_PATH := $(ROOTDIR)system/wlan/ti/wilink_6_1/platforms/os/linux
WLAN_AP_DRV_PATH := $(ROOTDIR)system/wlan/ti/WiLink_AP/platforms/os/linux

GIT_HOOKS_DIR := $(KERNEL_SRC_DIR)/.git/hooks
inst_hook: $(GIT_HOOKS_DIR)/pre-commit $(GIT_HOOKS_DIR)/checkpatch.pl

$(GIT_HOOKS_DIR)/pre-commit: $(KERNEL_SRC_DIR)/scripts/pre-commit
	@-cp -f $< $@
	@-chmod ugo+x $@

$(GIT_HOOKS_DIR)/checkpatch.pl: $(KERNEL_SRC_DIR)/scripts/checkpatch.pl
	@-cp -f $< $@
	@-chmod ugo+x $@

BLD_CONF=mapphone

ifneq ($(BLD_CONF),)
PRODUCT_SPECIFIC_DEFCONFIGS := $(DEFCONFIGSRC)/$(BLD_CONF)_defconfig
endif

ifneq ($(PRODUCT),)
PRODUCT_SPECIFIC_DEFCONFIGS += \
    ${LJAPDEFCONFIGSRC}/product/${PRODUCT}.config
endif

#Turn on kernel engineering build as default when TARGET_BUILD_VARIANT is eng, to disable it, add ENG_BLD=0 in build command
ifeq ($(TARGET_BUILD_VARIANT), user)
ENG_BLD := 0
else
ENG_BLD := 1
endif

ifeq ($(ENG_BLD), 1)
PRODUCT_SPECIFIC_DEFCONFIGS += \
    ${LJAPDEFCONFIGSRC}/eng_bld.config
endif

ifeq ($(TEST_DRV_CER), 1)
        ifeq ($(TEST_COVERAGE),)
                TEST_COVERAGE=1
        endif

        ifeq ($(TEST_KMEMLEAK),)
                TEST_KMEMLEAK=1
        endif

        ifeq ($(TEST_FAULTINJECT),)
                TEST_FAULTINJECT=1
        endif
endif

# Option to enable or disable gcov
ifeq ($(TEST_COVERAGE),1)
        PRODUCT_SPECIFIC_DEFCONFIGS += \
			${LJAPDEFCONFIGSRC}/feature/coverage.config
endif

ifeq ($(TEST_KMEMLEAK),1)
        PRODUCT_SPECIFIC_DEFCONFIGS += \
			${LJAPDEFCONFIGSRC}/feature/kmemleak.config
endif

ifeq ($(TEST_FAULTINJECT),1)
        PRODUCT_SPECIFIC_DEFCONFIGS += \
			${LJAPDEFCONFIGSRC}/feature/faultinject.config
endif

ifeq ($(TEST_MUDFLAP),1)
         PRODUCT_SPECIFIC_DEFCONFIGS += \
            ${LJAPDEFCONFIGSRC}/feature/mudflap.config
endif

#
# make kernel output directory structure
#---------------------------------------
$(KERNEL_BUILD_DIR):
	mkdir -p $(KERNEL_BUILD_DIR)

#
# make combined defconfig file
#---------------------------------------
$(TARGET_DEFCONFIG): FORCE $(PRODUCT_SPECIFIC_DEFCONFIGS)
	( perl -le 'print "# This file was automatically generated from:\n#\t" . join("\n#\t", @ARGV) . "\n"' $(PRODUCT_SPECIFIC_DEFCONFIGS) && cat $(PRODUCT_SPECIFIC_DEFCONFIGS) ) > $(TARGET_DEFCONFIG) || ( rm -f $@ && false )

.PHONY: FORCE
FORCE:

#
# make kernel configuration
#--------------------------
CONFIG_OUT := $(KERNEL_BUILD_DIR)/.config
kernel_config: $(CONFIG_OUT)
$(CONFIG_OUT): $(TARGET_DEFCONFIG) $(KERNEL_FFLAG) inst_hook | $(KERNEL_BUILD_DIR)
	@echo DEPMOD: $(DEPMOD)
	$(MAKE) -j1 -C $(KERNEL_SRC_DIR) ARCH=arm $(KERN_FLAGS) \
		CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) \
		O=$(KERNEL_BUILD_DIR) \
		KBUILD_DEFCONFIG=$(_TARGET_DEFCONFIG) \
		defconfig modules_prepare

#
# clean if warn filter changed
#-----------------------------
$(KERNEL_FFLAG): $(KERNEL_WARN_FILTER) | $(KERNEL_BUILD_DIR)
	@echo "Gcc warning filter changed, clean build will be enforced\n"
	$(MAKE) -j1 -C $(KERNEL_SRC_DIR) ARCH=arm $(KERN_FLAGS) \
                 CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) \
                 O=$(KERNEL_BUILD_DIR) clean
	@touch $(KERNEL_FFLAG)

## fail building if there are unfiltered warnings
## bypass if TEST_MUDFLAP is 1
# $(1): input log file
ifeq ($(TEST_MUDFLAP),1)
define kernel-check-gcc-warnings
endef
else
define kernel-check-gcc-warnings
	@if [ -e $1 ]; then \
		(cat $1 | \
			$(KERNEL_SRC_DIR)/scripts/chk_gcc_warn.pl $(KERNEL_SRC_DIR) \
				$(KERNEL_WARN_FILTER)) \
			|| ((rm -f $1) && false); fi
endef
endif

#
# build kernel and internal kernel modules
# ========================================
# We need to check warning no matter if build passed, failed or interuptted
.PHONY: kernel
kernel: $(CONFIG_OUT) | kernel_modules
	$(call kernel-check-gcc-warnings, $(KERNEL_ERR_LOG))
	$(MAKE) -C $(KERNEL_SRC_DIR) ARCH=arm $(KERN_FLAGS) \
		CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) O=$(KERNEL_BUILD_DIR) \
		zImage 2>&1 | tee $(KERNEL_ERR_LOG)
	$(call kernel-check-gcc-warnings, $(KERNEL_ERR_LOG))

#
# make kernel modules
#--------------------------
# We need to check warning no matter if build passed, failed or interuptted
.PHONY: kernel_modules
kernel_modules: $(CONFIG_OUT) | $(DEPMOD)
	$(call kernel-check-gcc-warnings, $(KMOD_ERR_LOG))
	$(MAKE) -C $(KERNEL_SRC_DIR) ARCH=arm $(KERN_FLAGS) \
		CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) O=$(KERNEL_BUILD_DIR) \
		DEPMOD=$(DEPMOD) INSTALL_MOD_PATH=$(KERNEL_BUILD_DIR) \
		modules 2>&1 | tee $(KMOD_ERR_LOG)
	$(call kernel-check-gcc-warnings, $(KMOD_ERR_LOG))

# To build modules (.ko) in specific folder
# It is useful for build specific module with extra options
# (e.g. TEST_DRV_CER)
kernel_dir:
	$(MAKE) -C $(KERNEL_SRC_DIR) ARCH=arm $(KERN_FLAGS) \
		CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) \
		O=$(KERNEL_BUILD_DIR) $(DIR_TO_BLD)

#NOTE: "strip" MUST be done for generated .ko files!!!
.PHONY: kernel_modules_install
kernel_modules_install: kernel_modules | $(DEPMOD)
	$(MAKE) -C $(KERNEL_SRC_DIR) ARCH=arm $(KERN_FLAGS) \
		CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) \
		O=$(KERNEL_BUILD_DIR) \
		DEPMOD=$(DEPMOD) \
		INSTALL_MOD_PATH=$(KERNEL_BUILD_DIR) \
		modules_install

kernel_clean:
	$(MAKE) -C $(KERNEL_SRC_DIR) ARCH=arm $(KERN_FLAGS) \
		CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) \
		O=$(KERNEL_BUILD_DIR) mrproper
	rm -f $(TARGET_DEFCONFIG)
	@rm -f $(KERNEL_BUILD_DIR)/.*.txt

#
#----------------------------------------------------------------------------
# To use "make ext_modules" to buld external kernel modules
#----------------------------------------------------------------------------
# build external kernel modules
#
# NOTE: "strip" MUST be done for generated .ko files!!!
# =============================
.PHONY: ext_kernel_modules
ext_kernel_modules: tiwlan_drv tiap_drv

# TODO:
# ext_modules_clean doesn't work
# wlan, graphic, SMC drivers need to be updated to fix it
ext_kernel_modules_clean: tiwlan_drv_clean - tiap_drv_clean

# wlan driver module
#-------------------
#API_MAKE = env -u MAKECMDGOALS make PREFIX=$(KERNEL_BUILD_DIR) \

API_MAKE = make PREFIX=$(KERNEL_BUILD_DIR) \
		CROSS=$(KERNEL_CROSS_COMPILE) \
		CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) \
		PROCFAMILY=OMAP_3430 PROJROOT=$(PROJROOT) \
		HOST_PLATFORM=zoom2 \
		KRNLSRC=$(KERNEL_SRC_DIR) KERNEL_DIR=$(KERNEL_BUILD_DIR)

tiwlan_drv: $(CONFIG_OUT)
	$(API_MAKE) -C $(WLAN_DRV_PATH)

tiap_drv: $(CONFIG_OUT)
	$(API_MAKE) -C $(WLAN_AP_DRV_PATH)

tiwlan_drv_clean:
	$(API_MAKE) -C $(WLAN_DRV_PATH) clean

tiap_drv_clean:
	$(API_MAKE) -C $(WLAN_AP_DRV_PATH) clean

#
# The below rules are for the Android build system
#-------------------------------------------------
ifneq ($(DO_NOT_REBUILD_THE_KERNEL),1)
.PHONY: $(TARGET_PREBUILT_KERNEL)
endif

$(TARGET_PREBUILT_KERNEL): kernel

$(INSTALLED_KERNEL_TARGET): $(TARGET_PREBUILT_KERNEL) | $(ACP)
	$(transform-prebuilt-to-target)

#
# install kernel modules into system image
#-----------------------------------------
# dummy.ko is used for system image dependency
TARGET_DUMMY_MODULE := $(MOTO_MOD_INSTALL)/dummy.ko
ALL_PREBUILT += $(TARGET_DUMMY_MODULE)
$(TARGET_DUMMY_MODULE): kernel_modules_install
	$(API_MAKE) -C $(WLAN_DRV_PATH)
	$(API_MAKE) -C $(WLAN_AP_DRV_PATH)
	mkdir -p $(MOTO_MOD_INSTALL)
	rm -f $(MOTO_MOD_INSTALL)/dummy.ko
	find $(KERNEL_BUILD_DIR)/lib/modules -name "*.ko" -exec cp -f {} \
		$(MOTO_MOD_INSTALL) \; || true
	cp $(WLAN_DRV_PATH)/tiwlan_drv.ko $(MOTO_MOD_INSTALL)
	cp $(WLAN_AP_DRV_PATH)/tiap_drv.ko $(MOTO_MOD_INSTALL)
	$(KERNEL_CROSS_COMPILE)strip --strip-debug $(MOTO_MOD_INSTALL)/*.ko
	touch $(MOTO_MOD_INSTALL)/dummy.ko

ROOTDIR :=
