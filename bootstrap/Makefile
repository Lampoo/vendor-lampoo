SHELL					:= /bin/bash

### GET NUMBER OF CPUS THIS MACHINE HAS ###
NCPUS					:= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
### GET NUMBER OF CPUS THIS MACHINE HAS ###

### SET UP CROSS_COMPILER ###
# https://releases.linaro.org/components/toolchain/binaries/7.3-2018.05/arm-eabi/
CROSS_COMPILE				?= arm-eabi-
### SET UP CROSS_COMPILER ###

### EXPORT DIR PATH ###
SUBDIRS					:= $(patsubst %/, %, $(wildcard $(dir $(TOPDIR)*/)))

$(foreach d, $(SUBDIRS), $(eval $(shell echo $(basename $(d)) | tr a-z\- A-Z\_)_DIR := $(d)/))
### EXPORT DIR PATH ###

.PHONY: _all
_all: bootstrap

### DO NOT EDIT THIS FILE ###
include build/core/main.mk
### DO NOT EDIT THIS FILE ###

### INCLUDE DEVICE MAKEFILE IFANY ###
-include $(PRODUCT_DIR)$(TARGET_DEVICE)/Makefile
### INCLUDE DEVICE MAKEFILE IFANY ###

### RULE TO BUILD AND CLEAN DROID ###
PHONY += droid
CLEAN_RULE += clean
### RULE TO BUILD AND CLEAN DROID ###

.PHONY: bootstrap
bootstrap: $(PHONY)

.PHONY: bootstrap_clean
bootstrap_clean: $(CLEAN_RULE)
