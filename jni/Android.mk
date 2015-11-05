
# Build the unit tests.
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

PROJECT_ROOT=../

LOCAL_LDLIBS := -llog
LOCAL_LDLIBS += -L$(LOCAL_PATH) 

$(warning $(LOCAL_LDLIBS))

LOCAL_MODULE := mediacodec

LOCAL_MODULE_TAGS := tests

LOCAL_SHARED_LIBRARIES := \


LOCAL_SRC_FILES := \
    	mediacodec.c \
    	media-jni.cpp


LOCAL_CFLAGS := 	\
	-I$(PROJECT_ROOT)include	
	

LOCAL_CXXFLAGS := -DENABLE_LOG


include $(BUILD_SHARED_LIBRARY)
