LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := uvc-gadget
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := events.c uvc-gadget.c v4l2.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
include $(BUILD_EXECUTABLE)
