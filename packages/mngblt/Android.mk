LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=\
        mng_usb.c

#LOCAL_MODULE_TAGS := eng

LOCAL_MODULE := mngblt

LOCAL_SHARED_LIBRARIES := liblog

include $(BUILD_EXECUTABLE)
