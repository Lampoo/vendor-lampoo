#
# Copyright (C) 2011 The Android Open-Source Project
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
#
ifeq ($(TARGET_PRODUCT),bootstrap)

PRODUCT_DIR := $(LOCAL_DIR)/

PRODUCT_MAKEFILES := $(PRODUCT_DIR)bootstrap.mk

# JAVA is not needed
JAVA_NOT_REQUIRED := true

# Disable CLANG compilation to speed up build time
CLANG_NOT_REQUIRED := $(shell [ $(PLATFORM_SDK_VERSION) -le 19 ] && echo true)
ifeq ($(CLANG_NOT_REQUIRED), true)
WITHOUT_CLANG := true
endif

endif
