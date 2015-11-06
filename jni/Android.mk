
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

#mediacodec.c 
LOCAL_SRC_FILES := \
    	MediaCodecJava.cpp \
    	media-jni.cpp \
    	ijksdl_mutex.c


LOCAL_CFLAGS := 	\
	-I$(PROJECT_ROOT)include	
	

LOCAL_CXXFLAGS := -DENABLE_LOG -pthread


include $(BUILD_SHARED_LIBRARY)
