LOCAL_PATH := $(call my-dir)  
  
include $(CLEAR_VARS)  
  
LOCAL_MODULE    := my_lib  
LOCAL_SRC_FILES := my_lib.c  
  
  # for native audio
LOCAL_LDLIBS    += -lOpenSLES
# for logging
LOCAL_LDLIBS    += -llog
# for native asset manager
LOCAL_LDLIBS    += -landroid

include $(BUILD_SHARED_LIBRARY)  