
#ifndef __MEDIA_CODEC_H__
#define __MEDIA_CODEC_H__


#include <stdio.h>
#include <stdlib.h>
#include <jni.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>

#include <android/log.h>

/* Faked cheap 'bool'.  */
#undef bool
#undef false
#undef true
#define bool int
#define false 0
#define true 1


#define INFO_OUTPUT_BUFFERS_CHANGED -3
#define INFO_OUTPUT_FORMAT_CHANGED  -2
#define INFO_TRY_AGAIN_LATER        -1



#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR  , "TestCase", __VA_ARGS__)


#ifdef __cplusplus
extern "C"{
#endif



int um_vdec_init( int codec, int width, int height);

int um_vdec_decode( char* in, int len);

int um_vdec_fini();

int um_vdec_render();

#ifdef __cplusplus
}
#endif



#endif
