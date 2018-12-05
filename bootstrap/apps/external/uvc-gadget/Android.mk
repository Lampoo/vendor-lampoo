LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := uvc-gadget
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := events.c uvc-gadget.c v4l2.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := uvcd
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := events.c v4l2.c uvc_gadget.c looper.c hotplug.c main.c capture.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
include $(BUILD_EXECUTABLE)
