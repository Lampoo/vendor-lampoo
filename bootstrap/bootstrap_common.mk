# Copyright (C) 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

PRODUCT_BRAND := bootstrap
PRODUCT_DEVICE := bootstrap
PRODUCT_NAME := bootstrap

# en_US only
PRODUCT_LOCALES := en_US

# A minimum runtime with shell only
PRODUCT_PACKAGES += \
	adbd \
	debuggerd \
	init \
	init.environ.rc \
	init.rc \
	logcat \
	libc \
	libc_malloc_debug \
	libcrypto \
	libcutils \
	libdl \
	liblog \
	libm \
	libstdc++ \
	libsysutils \
	libutils \
	libz \
	linker \
	mkshrc \
	property_contexts \
	reboot \
	sh \
	toolbox \
	toybox

# PRODUCT_PROPERTY_OVERRIDES +=

# PRODUCT_DEFAULT_PROPERTY_OVERRIDES +=

#PRODUCT_COPY_FILES += \
#	system/core/rootdir/init.usb.rc:root/init.usb.rc \
#	system/core/rootdir/init.usb.configfs.rc:root/init.usb.configfs.rc

PRODUCT_COPY_FILES += \
	system/core/rootdir/ueventd.rc:root/ueventd.rc \
	system/core/rootdir/etc/hosts:system/etc/hosts
