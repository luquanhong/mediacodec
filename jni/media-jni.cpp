#include <stdio.h>
#include <stdlib.h>
#include <jni.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include "mediacodec.h"
//#include "vout.h"


struct fields_t {
    jfieldID    surface;	//!< actually in android.view.Surface XXX
    jfieldID    surface_native;	//!< the surface passed from java
};

static struct fields_t fields;

JavaVM *myVm;

jobject vout_android_java_surf;


JNIEXPORT jint JNICALL jni_um_vdec_setVideoSurface(JNIEnv *env, jobject obj, jobject jsurface)
{
	vout_android_java_surf = env->NewGlobalRef( jsurface);

	return 0;
}

/*!
*	\brief	simple interface invoke without implement
*/
JNIEXPORT jint JNICALL jni_um_vdec_init(JNIEnv * env, jobject obj, jint codec, jint width, jint height)
{

	um_vdec_init(codec, width, height);

	return 0;
}


JNIEXPORT jint JNICALL jni_um_vdec_decode(JNIEnv * env, jobject obj, jbyteArray in, jint len)
{
	LOGE("Hello jni_um_vdec_decode!");

	jbyte* buffer = (jbyte*)env->GetByteArrayElements( in, 0);

	um_vdec_decode((char* )buffer, len);

	return 0;
}


/*!
*	\brief	simple interface invoke without implement
*/
JNIEXPORT jint JNICALL jni_um_vdec_fini(JNIEnv * env, jobject obj)
{

	LOGE("Hello jni_um_vdec_fini!");

	um_vdec_fini();

	return 0;
}

/*!
*	\brief	simple interface invoke without implement
*/
JNIEXPORT jint JNICALL jni_um_vdec_render(JNIEnv * env, jobject obj)
{

	LOGE("Hello jni_um_vdec_render!");

	um_vdec_render();

	return 0;
}


static const char *classPathName = "com/example/mediacodecdemo/MainActivity";

static JNINativeMethod methods[] = {
				{"um_vdec_init",		"(III)I",	(void*)jni_um_vdec_init},
				{"um_vdec_decode",		"([BI)I",					(void*)jni_um_vdec_decode},
				{"um_vdec_fini",		"()I",						(void*)jni_um_vdec_fini},
				{"um_vdec_setVideoSurface",       "(Landroid/view/Surface;)I", (void *) jni_um_vdec_setVideoSurface},
				{"um_vdec_render",		"()I",						(void*)jni_um_vdec_render},
};




static int registerNativeMethods(JNIEnv* env, const char* className,
                                JNINativeMethod* gMethods, int numMethods)
{
	jclass clazz;
	fprintf(stderr, "RegisterNatives start for '%s'", className);

	clazz = env->FindClass( className);	//!< get java class instance from my java class

	if (clazz == NULL) {
			fprintf(stderr, "Native registration unable to find class '%s'", className);
			return JNI_FALSE;
	}

	fields.surface = env->GetFieldID( clazz, "mSurface", "Landroid/view/Surface;");	//!< get the class of surface var in the java class

	if (fields.surface == NULL) {
		LOGE("Can't find GameCloudPlayer.mSurface");
		return -1;
	}


	if ( env->RegisterNatives( clazz, gMethods, numMethods) < 0) {
			fprintf(stderr, "RegisterNatives failed for '%s'", className);
			return JNI_FALSE;
	}

	return JNI_TRUE;
}



static int registerNatives(JNIEnv* env)
{
        if (!registerNativeMethods(env, classPathName,
                methods, sizeof(methods) / sizeof(methods[0]))) {
                        return JNI_FALSE;
        }

        return JNI_TRUE;
}



typedef union {
        JNIEnv* env;
        void* venv;
} UnionJNIEnvToVoid;



jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
        UnionJNIEnvToVoid uenv;
        uenv.venv = NULL;
        jint result = -1;
        JNIEnv* env = NULL;

        printf("JNI_OnLoad");

        if ( vm->GetEnv( &uenv.venv, JNI_VERSION_1_4) != JNI_OK) {
                fprintf(stderr, "GetEnv failed");
                goto bail;
        }

        env = uenv.env;

        if (registerNatives(env) != JNI_TRUE) {
                fprintf(stderr, "GetEnv failed");
                goto bail;
        }

        myVm = vm;

//        pthread_mutex_init(&vout_android_lock, NULL);
//        pthread_cond_init(&vout_android_surf_attached, NULL);

        //env->GetJavaVM(&mJvm);

        result = JNI_VERSION_1_4;
bail:
    return result;
}


void JNI_OnUnload(JavaVM* vm, void* reserved) {
//    pthread_mutex_destroy(&vout_android_lock);
//    pthread_cond_destroy(&vout_android_surf_attached);
}
