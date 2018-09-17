#
# SELinux: uncomment below to enable SELinux
# CONFIG_SELINUX = y

ifneq ($(filter libselinux, $(LOCAL_SHARED_LIBRARIES) $(LOCAL_STATIC_LIBRARIES)),)
 ifeq ($(CONFIG_SELINUX),y)
  LOCAL_CFLAGS := $(LOCAL_CFLAGS) -DHAVE_SELINUX
 else
  LOCAL_SHARED_LIBRARIES := $(filter-out libselinux, $(LOCAL_SHARED_LIBRARIES))
  LOCAL_STATIC_LIBRARIES := $(filter-out libselinux, $(LOCAL_STATIC_LIBRARIES))
 endif
endif

# LOCAL_SDK_VERSION: Unset if set
LOCAL_SDK_VERSION :=
