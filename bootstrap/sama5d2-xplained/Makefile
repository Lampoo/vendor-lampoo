ifndef PRODUCT_OUT
$(error PRODUCT_OUT not defined)
endif

KERNEL_DIR	?= $(KERNEL_AT91_DIR)

ifneq ($(KERNEL_DIR),)
PHONY					+= kernel_build
CLEAN_RULE				+= kernel_clean

LINUX_DEFCONFIG ?= $(wildcard $(PRODUCT_DIR)$(TARGET_DEVICE)/kernel_defconfig)
kernel_not_configured := $(wildcard $(KERNEL_DIR).config)
.PHONY: kernel_build kernel_clean
kernel_build:
ifeq ($(kernel_not_configured),)
ifneq ($(LINUX_DEFCONFIG),)
	$(hide)cp $(LINUX_DEFCONFIG) $(KERNEL_DIR).config
else
	$(hide)$(MAKE) -C $(KERNEL_DIR) ARCH=arm sama5_bootstrap_defconfig
endif
endif
	$(hide)$(MAKE) -C $(KERNEL_DIR) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) zImage -j$(NCPUS)
	$(hide)$(MAKE) -C $(KERNEL_DIR) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) at91-sama5d2_bootstrap.dtb

kernel_clean:
	$(hide)$(MAKE) -C $(KERNEL_DIR) ARCH=arm clean
endif

ifneq ($(AT91BOOTSTRAP_DIR),)
PHONY					+= at91bootstrap_build
CLEAN_RULE				+= at91bootstrap_clean

AT91BOOTSTRAP_DEFCONFIG ?= $(wildcard $(PRODUCT_DIR)$(TARGET_DEVICE)/at91bootstrap_defconfig)
at91bootstrap_not_configured := $(wildcard $(AT91BOOTSTRAP_DIR).config)
.PHONY: at91bootstrap_build at91bootstrap_clean
at91bootstrap_build:
ifeq ($(at91bootstrap_not_configured),)
ifneq ($(AT91BOOTSTRAP_DEFCONFIG),)
	$(hide)cp $(AT91BOOTSTRAP_DEFCONFIG) $(AT91BOOTSTRAP_DIR).config
else
	$(hide)$(MAKE) -C $(AT91BOOTSTRAP_DIR) ARCH=arm sama5_bootstrap_defconfig
endif
endif
	$(hide)$(MAKE) -C $(AT91BOOTSTRAP_DIR) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE)

at91bootstrap_clean:
	$(hide)$(MAKE) -C $(AT91BOOTSTRAP_DIR) ARCH=arm clean
endif

PRODUCT_IMAGE_OUT := $(PRODUCT_OUT)/image

SAMBA_DEFSCRIPT ?= $(abspath $(wildcard $(PRODUCT_DIR)$(TARGET_DEVICE)/samba_script.qml))
SAMBA_BOOTSCRIPT ?= $(abspath $(wildcard $(PRODUCT_DIR)$(TARGET_DEVICE)/samba_bootscript.qml))

.PHONY: image
image: bootstrap
	$(hide)mkdir -p $(PRODUCT_IMAGE_OUT)
ifneq ($(KERNEL_DIR),)
	$(hide)cp -f $(KERNEL_DIR)arch/arm/boot/zImage $(PRODUCT_IMAGE_OUT)/kernel
	$(hide)cp -f $(KERNEL_DIR)arch/arm/boot/dts/at91-sama5d2_bootstrap.dtb $(PRODUCT_IMAGE_OUT)/kernel-dtb
endif
ifneq ($(AT91BOOTSTRAP_DIR),)
	$(hide)cp -f $(AT91BOOTSTRAP_DIR)binaries/at91bootstrap.bin $(PRODUCT_IMAGE_OUT)/at91bootstrap.bin
endif
ifneq ($(SAMBA_DEFSCRIPT),)
	$(hide)cp -f $(SAMBA_DEFSCRIPT) $(PRODUCT_IMAGE_OUT)/samba_script.qml
endif
ifneq ($(SAMBA_BOOTSCRIPT),)
	$(hide)cp -f $(SAMBA_BOOTSCRIPT) $(PRODUCT_IMAGE_OUT)/samba_bootscript.qml
endif
	$(hide)cp -f $(INSTALLED_SYSTEMIMAGE) $(PRODUCT_IMAGE_OUT)/system.img
	$(hide)cp -f $(INSTALLED_USERDATAIMAGE_TARGET) $(PRODUCT_IMAGE_OUT)/userdata.img
	$(hide)echo Images generated at $(PRODUCT_IMAGE_OUT)

ifneq ($(wildcard $(PRODUCT_IMAGE_OUT)/samba_script.qml),)
.PHONY: upload
upload: SAMBA := $(abspath $(TOPDIR)external/sam-ba-3.2.2/sam-ba)
upload: $(SAMBA)
	$(hide)cd $(PRODUCT_IMAGE_OUT) && $(SAMBA) -x samba_script.qml
endif

ifneq ($(wildcard $(PRODUCT_IMAGE_OUT)/samba_bootscript.qml),)
.PHONY: bootconfig
bootconfig: SAMBA := $(abspath $(TOPDIR)external/sam-ba-3.2.2/sam-ba)
bootconfig: $(SAMBA)
	$(hide)cd $(PRODUCT_IMAGE_OUT) && $(SAMBA) -x samba_bootscript.qml
endif
