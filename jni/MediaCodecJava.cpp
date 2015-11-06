#include <stdio.h>
#include <stdlib.h>
#include <jni.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/system_properties.h>
#include<pthread.h>

#include <android/log.h>
#include "ijksdl_mutex.h"


typedef struct SDL_AMediaCodecBufferInfo {
    int32_t offset;
    int32_t size;
    int64_t presentationTimeUs;
    uint32_t flags;
} SDL_AMediaCodecBufferInfo;

typedef struct SDL_AMediaCodecBufferInfo SDL_AMediaCodecBufferInfo;

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR  , "TestCase", __VA_ARGS__)

#define APOLLO_MIN(a, b)   ( ((a) < (b)) ? (a) : (b) )

extern JavaVM *myVm; //java virtualmachine

extern jobject vout_android_java_surf; // for surface

static pthread_key_t g_thread_key;
static pthread_once_t g_key_once = PTHREAD_ONCE_INIT;


static void SDL_JNI_ThreadDestroyed(void* value)
{
    JNIEnv *env = (JNIEnv*) value;
    if (env != NULL) {
        ALOGE("%s: [%d] didn't call SDL_JNI_DetachThreadEnv() explicity\n", __func__, (int)gettid());
        myVm->DetachCurrentThread();
        pthread_setspecific(g_thread_key, NULL);
    }
}

static void make_thread_key()
{
    pthread_key_create(&g_thread_key, SDL_JNI_ThreadDestroyed);
}

jint SDL_JNI_SetupThreadEnv(JNIEnv **p_env)
{
    JavaVM *jvm = myVm;
    if (!jvm) {
        ALOGE("SDL_JNI_GetJvm: AttachCurrentThread: NULL jvm");
        return -1;
    }

    pthread_once(&g_key_once, make_thread_key);

    JNIEnv *env = (JNIEnv*) pthread_getspecific(g_thread_key);
    if (env) {
        *p_env = env;
        return 0;
    }

    if (jvm->AttachCurrentThread(&env, NULL) == JNI_OK) {
        pthread_setspecific(g_thread_key, env);
        *p_env = env;
        return 0;
    }

    return -1;
}

void SDL_JNI_DetachThreadEnv()
{
    JavaVM *jvm = myVm;

    ALOGE("%s: [%d]\n", __func__, (int)gettid());

    pthread_once(&g_key_once, make_thread_key);

    JNIEnv *env = (JNIEnv*)pthread_getspecific(g_thread_key);
    if (!env)
        return;
    pthread_setspecific(g_thread_key, NULL);

    if (jvm->DetachCurrentThread() == JNI_OK)
        return;

    return;
}

jboolean SDL_JNI_RethrowException(JNIEnv *env)
{
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        return JNI_TRUE;
    }

    return JNI_FALSE;
}

jboolean SDL_JNI_CatchException(JNIEnv *env)
{
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return JNI_TRUE;
    }

    return JNI_FALSE;
}

int SDL_JNI_ThrowException(JNIEnv* env, const char* className, const char* msg)
{
    if (env->ExceptionCheck()) {
        jthrowable exception = env->ExceptionOccurred();
        env->ExceptionClear();

        if (exception != NULL) {
            ALOGE("Discarding pending exception (%s) to throw", className);
            env->DeleteLocalRef(exception);
        }
    }

    jclass exceptionClass = env->FindClass( className);
    if (exceptionClass == NULL) {
        ALOGE("Unable to find exception class %s", className);
        /* ClassNotFoundException now pending */
        goto fail;
    }

    if (env->ThrowNew(exceptionClass, msg) != JNI_OK) {
        ALOGE("Failed throwing '%s' '%s'", className, msg);
        /* an exception, most likely OOM, will now be pending */
        goto fail;
    }

    return 0;
fail:
    if (exceptionClass)
        env->DeleteLocalRef( exceptionClass);
    return -1;
}

int SDL_JNI_ThrowIllegalStateException(JNIEnv *env, const char* msg)
{
    return SDL_JNI_ThrowException(env, "java/lang/IllegalStateException", msg);
}



////////////////////////////////////////////////


#define MC_API_ERROR (-1)
#define MC_API_INFO_TRYAGAIN (-11)
#define MC_API_INFO_OUTPUT_FORMAT_CHANGED (-12)
#define MC_API_INFO_OUTPUT_BUFFERS_CHANGED (-13)

#define BUFFER_FLAG_CODEC_CONFIG  2
#define INFO_OUTPUT_BUFFERS_CHANGED -3
#define INFO_OUTPUT_FORMAT_CHANGED  -2
#define INFO_TRY_AGAIN_LATER        -1

struct jfields
{
    jclass media_codec_list_class, media_codec_class, media_format_class;
    jclass buffer_info_class, byte_buffer_class;
    jmethodID tostring;
    jmethodID get_codec_count, get_codec_info_at, is_encoder, get_capabilities_for_type;
    jfieldID profile_levels_field, profile_field, level_field;
    jmethodID get_supported_types, get_name;
    jmethodID create_by_codec_name, create_by_codec_type, configure, start, stop, flush, release;
    jmethodID get_output_format;
    jmethodID get_input_buffers, get_input_buffer;
    jmethodID get_output_buffers, get_output_buffer;
    jmethodID dequeue_input_buffer, dequeue_output_buffer, queue_input_buffer;
    jmethodID release_output_buffer;
    jmethodID create_video_format, create_audio_format;
    jmethodID set_integer, set_bytebuffer, get_integer;
    jmethodID buffer_info_ctor;
    jfieldID size_field, offset_field, pts_field;
};

static struct jfields jfields;

enum Types
{
    METHOD, STATIC_METHOD, FIELD
};

#define OFF(x) offsetof(struct jfields, x)

struct classname
{
    const char *name;
    int offset;
};
static const struct classname classes[] = {
    { "android/media/MediaCodecList", OFF(media_codec_list_class) },
    { "android/media/MediaCodec", OFF(media_codec_class) },
    { "android/media/MediaFormat", OFF(media_format_class) },
    { "android/media/MediaFormat", OFF(media_format_class) },
    { "android/media/MediaCodec$BufferInfo", OFF(buffer_info_class) },
    { "java/nio/ByteBuffer", OFF(byte_buffer_class) },
    { NULL, 0 },
};

//IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_createDecoderByType,
//				g_clazz.clazz,"createDecoderByType",  "(Ljava/lang/String;)Landroid/media/MediaCodec;");

struct member
{
    const char *name;
    const char *sig;
    const char *classmode;
    int offset;
    int type;
    bool critical;
};
static const struct member members[] = {
    { "toString", "()Ljava/lang/String;", "java/lang/Object", OFF(tostring), METHOD, true },

    { "getCodecCount", "()I", "android/media/MediaCodecList", OFF(get_codec_count), STATIC_METHOD, true },
    { "getCodecInfoAt", "(I)Landroid/media/MediaCodecInfo;", "android/media/MediaCodecList", OFF(get_codec_info_at), STATIC_METHOD, true },

    { "isEncoder", "()Z", "android/media/MediaCodecInfo", OFF(is_encoder), METHOD, true },
    { "getSupportedTypes", "()[Ljava/lang/String;", "android/media/MediaCodecInfo", OFF(get_supported_types), METHOD, true },
    { "getName", "()Ljava/lang/String;", "android/media/MediaCodecInfo", OFF(get_name), METHOD, true },
    { "getCapabilitiesForType", "(Ljava/lang/String;)Landroid/media/MediaCodecInfo$CodecCapabilities;", "android/media/MediaCodecInfo", OFF(get_capabilities_for_type), METHOD, true },

    { "profileLevels", "[Landroid/media/MediaCodecInfo$CodecProfileLevel;", "android/media/MediaCodecInfo$CodecCapabilities", OFF(profile_levels_field), FIELD, true },
    { "profile", "I", "android/media/MediaCodecInfo$CodecProfileLevel", OFF(profile_field), FIELD, true },
    { "level", "I", "android/media/MediaCodecInfo$CodecProfileLevel", OFF(level_field), FIELD, true },

    { "createByCodecName", "(Ljava/lang/String;)Landroid/media/MediaCodec;", "android/media/MediaCodec", OFF(create_by_codec_name), STATIC_METHOD, true },
	{ "createDecoderByType", "(Ljava/lang/String;)Landroid/media/MediaCodec;", "android/media/MediaCodec", OFF(create_by_codec_type), STATIC_METHOD, true },
    { "configure", "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V", "android/media/MediaCodec", OFF(configure), METHOD, true },
    { "start", "()V", "android/media/MediaCodec", OFF(start), METHOD, true },
    { "stop", "()V", "android/media/MediaCodec", OFF(stop), METHOD, true },
    { "flush", "()V", "android/media/MediaCodec", OFF(flush), METHOD, true },
    { "release", "()V", "android/media/MediaCodec", OFF(release), METHOD, true },
    { "getOutputFormat", "()Landroid/media/MediaFormat;", "android/media/MediaCodec", OFF(get_output_format), METHOD, true },
    { "getInputBuffers", "()[Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_input_buffers), METHOD, false },
    { "getInputBuffer", "(I)Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_input_buffer), METHOD, false },
    { "getOutputBuffers", "()[Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_output_buffers), METHOD, false },
    { "getOutputBuffer", "(I)Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_output_buffer), METHOD, false },
    { "dequeueInputBuffer", "(J)I", "android/media/MediaCodec", OFF(dequeue_input_buffer), METHOD, true },
    { "dequeueOutputBuffer", "(Landroid/media/MediaCodec$BufferInfo;J)I", "android/media/MediaCodec", OFF(dequeue_output_buffer), METHOD, true },
    { "queueInputBuffer", "(IIIJI)V", "android/media/MediaCodec", OFF(queue_input_buffer), METHOD, true },
    { "releaseOutputBuffer", "(IZ)V", "android/media/MediaCodec", OFF(release_output_buffer), METHOD, true },

    { "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;", "android/media/MediaFormat", OFF(create_video_format), STATIC_METHOD, true },
    { "createAudioFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;", "android/media/MediaFormat", OFF(create_audio_format), STATIC_METHOD, true },
    { "setInteger", "(Ljava/lang/String;I)V", "android/media/MediaFormat", OFF(set_integer), METHOD, true },
    { "getInteger", "(Ljava/lang/String;)I", "android/media/MediaFormat", OFF(get_integer), METHOD, true },
    { "setByteBuffer", "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V", "android/media/MediaFormat", OFF(set_bytebuffer), METHOD, true },

    { "<init>", "()V", "android/media/MediaCodec$BufferInfo", OFF(buffer_info_ctor), METHOD, true },
    { "size", "I", "android/media/MediaCodec$BufferInfo", OFF(size_field), FIELD, true },
    { "offset", "I", "android/media/MediaCodec$BufferInfo", OFF(offset_field), FIELD, true },
    { "presentationTimeUs", "J", "android/media/MediaCodec$BufferInfo", OFF(pts_field), FIELD, true },
    { NULL, NULL, NULL, 0, 0, false },
};

static int jstrcmp(JNIEnv* env, jobject str, const char* str2)
{
    jsize len = env->GetStringUTFLength((jstring)str);
    if (len != (jsize) strlen(str2))
        return -1;
    const char *ptr = env->GetStringUTFChars( (jstring)str, NULL);
    int ret = memcmp(ptr, str2, len);
    env->ReleaseStringUTFChars((jstring)str, ptr);
    return ret;
}

static inline bool check_exception(JNIEnv *env)
{
    if (env->ExceptionOccurred())
    {
        env->ExceptionClear();
        return true;
    }
    else
        return false;
}
#define CHECK_EXCEPTION() check_exception( env )

static bool InitJNIFields (JNIEnv *env)
{
    //static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    static int i_init_state = -1;
    bool ret;

    //vlc_mutex_lock( &lock );

    if( i_init_state != -1 )
        goto end;

    i_init_state = 0;

    for (int i = 0; classes[i].name; i++)
    {
        jclass clazz = env->FindClass(classes[i].name);
        if (CHECK_EXCEPTION())
        {
            ALOGE( "Unable to find class %s", classes[i].name);
            goto end;
        }
        *(jclass*)((uint8_t*)&jfields + classes[i].offset) =
            (jclass) env->NewGlobalRef( clazz);
    }

    jclass last_class;
    for (int i = 0; members[i].name; i++)
    {
        if (i == 0 || strcmp(members[i].classmode, members[i - 1].classmode))
            last_class = env->FindClass(members[i].classmode);

        if (CHECK_EXCEPTION())
        {
            ALOGE( "Unable to find class %s", members[i].classmode);
            goto end;
        }

        switch (members[i].type) {
        case METHOD:
            *(jmethodID*)((uint8_t*)&jfields + members[i].offset) =
                env->GetMethodID(last_class, members[i].name, members[i].sig);
            break;
        case STATIC_METHOD:
            *(jmethodID*)((uint8_t*)&jfields + members[i].offset) =
                env->GetStaticMethodID(last_class, members[i].name, members[i].sig);
            break;
        case FIELD:
            *(jfieldID*)((uint8_t*)&jfields + members[i].offset) =
                env->GetFieldID(last_class, members[i].name, members[i].sig);
            break;
        }
        if (CHECK_EXCEPTION())
        {
            ALOGE( "Unable to find the member %s in %s",
                     members[i].name, members[i].classmode);
            if (members[i].critical)
                goto end;
        }
    }
    /* getInputBuffers and getOutputBuffers are deprecated if API >= 21
     * use getInputBuffer and getOutputBuffer instead. */
    if (jfields.get_input_buffer && jfields.get_output_buffer)
    {
        jfields.get_output_buffers =
        jfields.get_input_buffers = NULL;
    }
    else if (!jfields.get_output_buffers && !jfields.get_input_buffers)
    {
        ALOGE( "Unable to find get Output/Input Buffer/Buffers");
        goto end;
    }

    i_init_state = 1;
end:
    ret = i_init_state == 1;
    if( !ret )
        ALOGE( "MediaCodec jni init failed");

    //vlc_mutex_unlock( &lock );
    return ret;
}



struct mc_api_sys
{
    jobject codec;
    jobject buffer_info;
    jobject input_buffers, output_buffers;

    bool b_started;
    int i_width;
   	int i_height;
   	int i_codec;
   	int i;

   	bool acodec_first_dequeue_output_request;
    SDL_mutex                *acodec_first_dequeue_output_mutex;
    SDL_cond                 *acodec_first_dequeue_output_cond;

    SDL_mutex                *acodec_mutex;
    SDL_cond                 *acodec_cond;
};

struct mc_api_sys* p_sys;


/*****************************************************************************
 * MediaCodec_GetName
 *****************************************************************************/
char* MediaCodec_GetName( const char *psz_mime, size_t h264_profile)
{

    int num_codecs;
    jstring jmime;
    char *psz_name = NULL;

    JNIEnv *env;
	if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
		  ALOGE( "%s: SetupThreadEnv failed", __func__);
		  return NULL;
	}

    jmime = env->NewStringUTF( psz_mime);
    if (!jmime)
        return NULL;

    num_codecs = env->CallStaticIntMethod(jfields.media_codec_list_class,
                                             jfields.get_codec_count);

    for (int i = 0; i < num_codecs; i++)
    {
        jobject codec_capabilities = NULL;
        jobject profile_levels = NULL;
        jobject info = NULL;
        jobject name = NULL;
        jobject types = NULL;
        jsize name_len = 0;
        int profile_levels_len = 0, num_types = 0;
        const char *name_ptr = NULL;
        bool found = false;

        info = env->CallStaticObjectMethod(jfields.media_codec_list_class,
                                              jfields.get_codec_info_at, i);

        name = env->CallObjectMethod(info, jfields.get_name);
        name_len = env->GetStringUTFLength((jstring)name);
        name_ptr = env->GetStringUTFChars((jstring)name, NULL);

//        if (OMXCodec_IsBlacklisted( name_ptr, name_len))
//            goto loopclean;

        if (env->CallBooleanMethod(info, jfields.is_encoder))
            goto loopclean;

        codec_capabilities = env->CallObjectMethod(info, jfields.get_capabilities_for_type,
                                                      jmime);
        if (CHECK_EXCEPTION())
        {
            ALOGE("Exception occurred in MediaCodecInfo.getCapabilitiesForType");
            goto loopclean;
        }
        else if (codec_capabilities)
        {
            profile_levels = env->GetObjectField(codec_capabilities, jfields.profile_levels_field);
            if (profile_levels)
                profile_levels_len = env->GetArrayLength( (jarray)profile_levels);
        }
        ALOGE( "Number of profile levels: %d", profile_levels_len);

        types = env->CallObjectMethod(info, jfields.get_supported_types);
        num_types = env->GetArrayLength((jarray) types);
        found = false;

        for (int j = 0; j < num_types && !found; j++)
        {
            jobject type = env->GetObjectArrayElement((jobjectArray)types, j);
            if (!jstrcmp(env, type, psz_mime))
            {
                /* The mime type is matching for this component. We
                   now check if the capabilities of the codec is
                   matching the video format. */
//                if (h264_profile)
//                {
//                    /* This decoder doesn't expose its profiles and is high
//                     * profile capable */
//                    if (!strncmp(name_ptr, "OMX.LUMEVideoDecoder", APOLLO_MIN(20, name_len)))
//                        found = true;
//
//                    for (int i = 0; i < profile_levels_len && !found; ++i)
//                    {
//                        jobject profile_level = env->GetObjectArrayElement(env, profile_levels, i);
//
//                        int omx_profile = (*env)->GetIntField(env, profile_level, jfields.profile_field);
//                        size_t codec_profile = convert_omx_to_profile_idc(omx_profile);
//                        (*env)->DeleteLocalRef(env, profile_level);
//                        if (codec_profile != h264_profile)
//                            continue;
//                        /* Some encoders set the level too high, thus we ignore it for the moment.
//                           We could try to guess the actual profile based on the resolution. */
//                        found = true;
//                    }
//                }
//                else
                    found = true;
            }
            env->DeleteLocalRef(type);
        }
        if (found)
        {
            ALOGE("using %.*s", name_len, name_ptr);
            psz_name = (char*)malloc(name_len + 1);
            if (psz_name)
            {
                memcpy(psz_name, name_ptr, name_len);
                psz_name[name_len] = '\0';
            }
        }
loopclean:
        if (name)
        {
            env->ReleaseStringUTFChars( (jstring)name, name_ptr);
            env->DeleteLocalRef(name);
        }
        if (profile_levels)
            env->DeleteLocalRef( profile_levels);
        if (types)
            env->DeleteLocalRef( types);
        if (codec_capabilities)
            env->DeleteLocalRef( codec_capabilities);
        if (info)
            env->DeleteLocalRef( info);
        if (found)
            break;
    }
    env->DeleteLocalRef(jmime);

    return psz_name;
}

/*****************************************************************************
 * Stop
 *****************************************************************************/
static int Stop()
{
//    mc_api_sys *p_sys = api->p_sys;
//    JNIEnv *env;
//
//    api->b_direct_rendering = false;
//
//    GET_ENV();

	JNIEnv *env;
	if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
		  ALOGE( "%s: SetupThreadEnv failed", __func__);
		  return -1;
	}

    if (p_sys->input_buffers)
    {
        env->DeleteGlobalRef( p_sys->input_buffers);
        p_sys->input_buffers = NULL;
    }
    if (p_sys->output_buffers)
    {
        env->DeleteGlobalRef( p_sys->output_buffers);
        p_sys->output_buffers = NULL;
    }
    if (p_sys->codec)
    {
        if (p_sys->b_started)
        {
            env->CallVoidMethod(p_sys->codec, jfields.stop);
            if (CHECK_EXCEPTION())
                ALOGE( "Exception in MediaCodec.stop");
            p_sys->b_started = false;
        }

        env->CallVoidMethod(p_sys->codec, jfields.release);
        if (CHECK_EXCEPTION())
            ALOGE("Exception in MediaCodec.release");
        env->DeleteGlobalRef( p_sys->codec);
        p_sys->codec = NULL;
    }
    if (p_sys->buffer_info)
    {
        env->DeleteGlobalRef(p_sys->buffer_info);
        p_sys->buffer_info = NULL;
    }
    ALOGE("MediaCodec via JNI closed");
    return 0;
}

/*****************************************************************************
 * Start
 *****************************************************************************/
static int Start( const char *psz_name, const char *psz_mime)
{
//    mc_api_sys *p_sys = api->p_sys;
//    JNIEnv* env = NULL;
    int i_ret = -1;
    bool b_direct_rendering = false;
    jstring jmime = NULL;
    jstring jcodec_name = NULL;
    jobject jcodec = NULL;
    jobject jformat = NULL;
    jstring jrotation_string = NULL;
    jstring jmaxinputsize_string = NULL;
    jobject jinput_buffers = NULL;
    jobject joutput_buffers = NULL;
    jobject jbuffer_info = NULL;
    jobject jsurface = NULL;
    jstring jbitrate_string = NULL;
    jstring jframerate_string = NULL;

    jstring jcolorformat_string = NULL;


    jstring jiframe_string = NULL;
//    GET_ENV();
    JNIEnv *env;
	if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
		  ALOGE( "%s: SetupThreadEnv failed", __func__);
		  return -1;
	}

    jmime = env->NewStringUTF( psz_mime);
    jcodec_name = env->NewStringUTF( psz_name);
    if (!jmime || !jcodec_name)
        goto error;

    /* This method doesn't handle errors nicely, it crashes if the codec isn't
     * found.  (The same goes for createDecoderByType.) This is fixed in latest
     * AOSP and in 4.2, but not in 4.1 devices. */
//    jcodec = env->CallStaticObjectMethod(jfields.media_codec_class,
//                                            jfields.create_by_codec_name,
//                                            jcodec_name);
    jcodec = env->CallStaticObjectMethod(jfields.media_codec_class,
                                            jfields.create_by_codec_type,
											jmime);


    if (CHECK_EXCEPTION())
    {
    	ALOGE( "Exception occurred in MediaCodec.createByCodecName");
        goto error;
    }
    p_sys->codec = env->NewGlobalRef(jcodec);


	jformat = env->CallStaticObjectMethod(jfields.media_format_class,
											 jfields.create_video_format,
											 jmime,
											 p_sys->i_width,
											 p_sys->i_height);
//	if (p_args->video.p_awh)
//		jsurface = AWindowHandler_getSurface(p_args->video.p_awh,
//											 AWindow_Video);
//	b_direct_rendering = !!jsurface;

	/* There is no way to rotate the video using direct rendering (and
	 * using a SurfaceView) before  API 21 (Lollipop). Therefore, we
	 * deactivate direct rendering if video doesn't have a normal rotation
	 * and if get_input_buffer method is not present (This method exists
	 * since API 21). */
//	if (b_direct_rendering && p_args->video.i_angle != 0
//	 && !jfields.get_input_buffer)
//		b_direct_rendering = false;
//
//	if (b_direct_rendering && p_args->video.i_angle != 0)
//	{
//		jrotation_string = (*env)->NewStringUTF(env, "rotation-degrees");
//		(*env)->CallVoidMethod(env, jformat, jfields.set_integer,
//							   jrotation_string, p_args->video.i_angle);
//	}

    /* No limits for input size */
    jmaxinputsize_string = env->NewStringUTF( "max-input-size");
    env->CallVoidMethod( jformat, jfields.set_integer,jmaxinputsize_string, 0);



     jbitrate_string = NULL;
    jbitrate_string = env->NewStringUTF( "bitrate");
    env->CallVoidMethod( jformat, jfields.set_integer, jbitrate_string, 100000);

     jframerate_string = NULL;
    jframerate_string = env->NewStringUTF( "frame-rate");
	env->CallVoidMethod( jformat, jfields.set_integer, jframerate_string, 15);

	 jcolorformat_string = NULL;
	jcolorformat_string = env->NewStringUTF( "color-format");
	env->CallVoidMethod( jformat, jfields.set_integer, jcolorformat_string, 21);

	 jiframe_string = NULL;
	jiframe_string = env->NewStringUTF( "i-frame-interval");
	env->CallVoidMethod( jformat, jfields.set_integer, jiframe_string, 5);

//    format.setInteger(MediaFormat.KEY_BIT_RATE, 100000);
//	format.setInteger(MediaFormat.KEY_FRAME_RATE, 15);
//
//	format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
//				MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar);
//
//	String mime = format.getString(MediaFormat.KEY_MIME);
//	format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 5);

//    if (b_direct_rendering)
//    {

    	ALOGE("codec MediaCodec.configure start");
        // Configure MediaCodec with the Android surface.
        env->CallVoidMethod( p_sys->codec, jfields.configure,
                               jformat, vout_android_java_surf, NULL, 0);
        if (CHECK_EXCEPTION())
        {
            ALOGE("Exception occurred in MediaCodec.configure with an output surface.");
            goto error;
        }
        ALOGE("codec MediaCodec.configure end");
//    }
//    else
//    {
//        (*env)->CallVoidMethod(env, p_sys->codec, jfields.configure,
//                               jformat, NULL, NULL, 0);
//        if (CHECK_EXCEPTION())
//        {
//            msg_Warn(api->p_obj, "Exception occurred in MediaCodec.configure");
//            goto error;
//        }
//    }

    env->CallVoidMethod(p_sys->codec, jfields.start);
    if (CHECK_EXCEPTION())
    {
        ALOGE("Exception occurred in MediaCodec.start");
        goto error;
    }
    p_sys->b_started = true;

    if (jfields.get_input_buffers && jfields.get_output_buffers)
    {

        jinput_buffers = env->CallObjectMethod( p_sys->codec,
                                                  jfields.get_input_buffers);
        if (CHECK_EXCEPTION())
        {
            ALOGE( "Exception in MediaCodec.getInputBuffers");
            goto error;
        }
        p_sys->input_buffers = env->NewGlobalRef(jinput_buffers);

        joutput_buffers = env->CallObjectMethod( p_sys->codec, jfields.get_output_buffers);
        if (CHECK_EXCEPTION())
        {
            ALOGE("Exception in MediaCodec.getOutputBuffers");
            goto error;
        }
        p_sys->output_buffers = env->NewGlobalRef(joutput_buffers);
    }
    jbuffer_info = env->NewObject(jfields.buffer_info_class,
                                     jfields.buffer_info_ctor);
    p_sys->buffer_info = env->NewGlobalRef( jbuffer_info);

//    p_sys->b_direct_rendering = b_direct_rendering;
    i_ret = 0;
   ALOGE( "MediaCodec via JNI opened");

error:
    if (jmime)
        env->DeleteLocalRef( jmime);
    if (jcodec_name)
        env->DeleteLocalRef( jcodec_name);
    if (jcodec)
        env->DeleteLocalRef( jcodec);
    if (jformat)
        env->DeleteLocalRef( jformat);
    if (jrotation_string)
        env->DeleteLocalRef( jrotation_string);
    if (jmaxinputsize_string)
        env->DeleteLocalRef( jmaxinputsize_string);
    if (jinput_buffers)
        env->DeleteLocalRef( jinput_buffers);
    if (joutput_buffers)
        env->DeleteLocalRef( joutput_buffers);
    if (jbuffer_info)
        env->DeleteLocalRef( jbuffer_info);

    if (i_ret != 0)
        Stop();
    return i_ret;
}

/*****************************************************************************
 * Flush
 *****************************************************************************/
static int Flush()
{
//    mc_api_sys *p_sys = api->p_sys;
//    JNIEnv *env = NULL;
//
//    GET_ENV();

	JNIEnv *env;
	if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
		  ALOGE( "%s: SetupThreadEnv failed", __func__);
		  return -1;
	}

	env->CallVoidMethod( p_sys->codec, jfields.flush);
    if (CHECK_EXCEPTION())
    {
        ALOGE("Exception occurred in MediaCodec.flush");
        return -1;
    }
    return 0;
}

/*****************************************************************************
 * DequeueInput
 *****************************************************************************/
static int DequeueInput(long long i_timeout)
{
//    mc_api_sys *p_sys = api->p_sys;
//    JNIEnv *env;
    int i_index;
//
//    GET_ENV();

    JNIEnv *env;
	if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
		  ALOGE( "%s: SetupThreadEnv failed", __func__);
		  return -1;
	}

    i_index = env->CallIntMethod( p_sys->codec,
                                    jfields.dequeue_input_buffer, 50);
    if (CHECK_EXCEPTION())
    {
        ALOGE("Exception occurred in MediaCodec.dequeueInputBuffer");
        return -1;
    }
    if (i_index >= 0)
        return i_index;
    else
        return MC_API_INFO_TRYAGAIN;

}

/*****************************************************************************
 * QueueInput
 *****************************************************************************/
static int QueueInput(int i_index, const void *p_buf,
                      size_t i_size, long long i_ts, bool b_config)
{
//    mc_api_sys *p_sys = api->p_sys;
//    JNIEnv *env;
    uint8_t *p_mc_buf;
    jobject j_mc_buf;
    jsize j_mc_size;
    jint jflags = b_config ? BUFFER_FLAG_CODEC_CONFIG : 0;
    JNIEnv *env;

    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
  		  ALOGE( "%s: SetupThreadEnv failed", __func__);
  		  return -1;
  	}

    ALOGE("QueueInput enter");

    //assert(i_index >= 0);

//    GET_ENV();

    if (jfields.get_input_buffers){
    	ALOGE("QueueInput enter 1");
        j_mc_buf = env->GetObjectArrayElement((jobjectArray)p_sys->input_buffers, i_index);
    } else {
        j_mc_buf = env->CallObjectMethod( p_sys->codec,
                                            jfields.get_input_buffer, i_index);
        if (CHECK_EXCEPTION())
        {
            ALOGE( "Exception in MediaCodec.getInputBuffer");
            return MC_API_ERROR;
        }
    }
    j_mc_size = env->GetDirectBufferCapacity(j_mc_buf);
    p_mc_buf = (uint8_t*)env->GetDirectBufferAddress( j_mc_buf);
    if (j_mc_size < 0)
    {
    	 ALOGE( "Java buffer has invalid size");
        env->DeleteLocalRef( j_mc_buf);
        return MC_API_ERROR;
    }

    if ((size_t) j_mc_size > i_size)
        j_mc_size = i_size;
    ALOGE("QueueInput enter 2");
    memcpy(p_mc_buf, p_buf, j_mc_size);

    env->CallVoidMethod(p_sys->codec, jfields.queue_input_buffer,
                           i_index, 0, j_mc_size, i_ts, jflags);

    ALOGE("QueueInput enter 3");
    env->DeleteLocalRef(j_mc_buf);
    ALOGE("QueueInput enter 4");
    if (CHECK_EXCEPTION())
    {
    	 ALOGE("Exception in MediaCodec.queueInputBuffer");
        return MC_API_ERROR;
    }
    ALOGE("QueueInput enter 5");
    return 0;
}

/*****************************************************************************
 * DequeueOutput
 *****************************************************************************/
static int DequeueOutput(long long i_timeout)
{
//    mc_api_sys *p_sys = api->p_sys;
//    JNIEnv *env;
    int i_index;

//    GET_ENV();
    ALOGE( "%s:  1", __func__);
    JNIEnv *env;

    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
		  ALOGE( "%s: SetupThreadEnv failed", __func__);
		  return -1;
	}
    ALOGE( "%s:  2", __func__);
    i_index = env->CallIntMethod(p_sys->codec, jfields.dequeue_output_buffer,
                                    p_sys->buffer_info, 1000);
    if (CHECK_EXCEPTION())
    {
        ALOGE( "Exception in MediaCodec.dequeueOutputBuffer");
        return MC_API_ERROR;
    }

    ALOGE( " MediaCodec.dequeueOutputBuffer i_index = %d", i_index);
    if (i_index >= 0)
        return i_index;
    else if (i_index == INFO_OUTPUT_FORMAT_CHANGED)
        return MC_API_INFO_OUTPUT_FORMAT_CHANGED;
    else if (i_index == INFO_OUTPUT_BUFFERS_CHANGED)
        return MC_API_INFO_OUTPUT_BUFFERS_CHANGED;
    else
        return MC_API_INFO_TRYAGAIN;
}

/*****************************************************************************
 * GetOutput
 *****************************************************************************/
static int GetOutput( int i_index)
{
//    mc_api_sys *p_sys = api->p_sys;
//    JNIEnv *env;
//
//    GET_ENV();

    JNIEnv *env;

    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
		  ALOGE( "%s: SetupThreadEnv failed", __func__);
		  return -1;
	}
    if (i_index >= 0)
    {
//        p_out->type = MC_OUT_TYPE_BUF;
//        p_out->u.buf.i_index = i_index;
//        p_out->u.buf.i_ts = (*env)->GetLongField(env, p_sys->buffer_info,
//                                                 jfields.pts_field);

//        if (api->b_direct_rendering)
//        {
//            p_out->u.buf.p_ptr = NULL;
//            p_out->u.buf.i_size = 0;
//        }
//        else
//        {
//            jobject buf;
//            uint8_t *ptr;
//            int offset;
//
//            if (jfields.get_output_buffers)
//                buf = (*env)->GetObjectArrayElement(env, p_sys->output_buffers,
//                                                    i_index);
//            else
//            {
//                buf = (*env)->CallObjectMethod(env, p_sys->codec,
//                                               jfields.get_output_buffer,
//                                               i_index);
//                if (CHECK_EXCEPTION())
//                {
//                    msg_Err(api->p_obj, "Exception in MediaCodec.getOutputBuffer");
//                    return MC_API_ERROR;
//                }
//            }
//            //jsize buf_size = (*env)->GetDirectBufferCapacity(env, buf);
//            ptr = (*env)->GetDirectBufferAddress(env, buf);
//
//            offset = (*env)->GetIntField(env, p_sys->buffer_info,
//                                         jfields.offset_field);
//            p_out->u.buf.p_ptr = ptr + offset;
//            p_out->u.buf.i_size = (*env)->GetIntField(env, p_sys->buffer_info,
//                                                       jfields.size_field);
//            (*env)->DeleteLocalRef(env, buf);
//        }
        return i_index;
    } else if (i_index == MC_API_INFO_OUTPUT_FORMAT_CHANGED)
    {
//        jobject format = NULL;
//        jobject format_string = NULL;
//        jsize format_len;
//        const char *format_ptr;
//
//        format = (*env)->CallObjectMethod(env, p_sys->codec,
//                                          jfields.get_output_format);
//        if (CHECK_EXCEPTION())
//        {
//            msg_Err(api->p_obj, "Exception in MediaCodec.getOutputFormat");
//            return MC_API_ERROR;
//        }
//
//        format_string = (*env)->CallObjectMethod(env, format, jfields.tostring);
//
//        format_len = (*env)->GetStringUTFLength(env, format_string);
//        format_ptr = (*env)->GetStringUTFChars(env, format_string, NULL);
//        msg_Dbg(api->p_obj, "output format changed: %.*s", format_len,
//                format_ptr);
//        (*env)->ReleaseStringUTFChars(env, format_string, format_ptr);
//
//        p_out->type = MC_OUT_TYPE_CONF;
//        if (api->b_video)
//        {
//            p_out->u.conf.video.width         = GET_INTEGER(format, "width");
//            p_out->u.conf.video.height        = GET_INTEGER(format, "height");
//            p_out->u.conf.video.stride        = GET_INTEGER(format, "stride");
//            p_out->u.conf.video.slice_height  = GET_INTEGER(format, "slice-height");
//            p_out->u.conf.video.pixel_format  = GET_INTEGER(format, "color-format");
//            p_out->u.conf.video.crop_left     = GET_INTEGER(format, "crop-left");
//            p_out->u.conf.video.crop_top      = GET_INTEGER(format, "crop-top");
//            p_out->u.conf.video.crop_right    = GET_INTEGER(format, "crop-right");
//            p_out->u.conf.video.crop_bottom   = GET_INTEGER(format, "crop-bottom");
//        }
//        else
//        {
//            p_out->u.conf.audio.channel_count = GET_INTEGER(format, "channel-count");
//            p_out->u.conf.audio.channel_mask = GET_INTEGER(format, "channel-mask");
//            p_out->u.conf.audio.sample_rate = GET_INTEGER(format, "sample-rate");
//        }
//
//        (*env)->DeleteLocalRef(env, format);
        return 1;
    }
    else if (i_index == MC_API_INFO_OUTPUT_BUFFERS_CHANGED)
    {
//        jobject joutput_buffers;
//
//        msg_Dbg(api->p_obj, "output buffers changed");
//        if (!jfields.get_output_buffers)
//            return 0;
//        (*env)->DeleteGlobalRef(env, p_sys->output_buffers);
//
//        joutput_buffers = (*env)->CallObjectMethod(env, p_sys->codec,
//                                                   jfields.get_output_buffers);
//        if (CHECK_EXCEPTION())
//        {
//            msg_Err(api->p_obj, "Exception in MediaCodec.getOutputBuffer");
//            p_sys->output_buffers = NULL;
//            return MC_API_ERROR;
//        }
//        p_sys->output_buffers = (*env)->NewGlobalRef(env, joutput_buffers);
//        (*env)->DeleteLocalRef(env, joutput_buffers);
    }
    return 0;
}

/*****************************************************************************
 * ReleaseOutput
 *****************************************************************************/
static int ReleaseOutput( int i_index, bool b_render)
{
//    mc_api_sys *p_sys = api->p_sys;
//    JNIEnv *env;
//
//    assert(i_index >= 0);
//
//    GET_ENV();

	JNIEnv *env;

	if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
		  ALOGE( "%s: SetupThreadEnv failed", __func__);
		  return -1;
	}

    env->CallVoidMethod(p_sys->codec, jfields.release_output_buffer,
                           i_index, b_render);
    if (CHECK_EXCEPTION())
    {
        ALOGE("Exception in MediaCodec.releaseOutputBuffer");
        return MC_API_ERROR;
    }
    return 0;
}



extern "C" int um_vdec_decode(char* buffer, int len)
{

	JNIEnv *env;
	if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
			ALOGE( "%s: SetupThreadEnv failed", __func__);
			return -1;
	 }
	ALOGE("Hello jni_um_vdec_decode!");

	SDL_LockMutex(p_sys->acodec_first_dequeue_output_mutex);
	while (p_sys->acodec_first_dequeue_output_request) {
		SDL_CondWaitTimeout(p_sys->acodec_first_dequeue_output_cond, p_sys->acodec_first_dequeue_output_mutex, 1000);
	}
	SDL_UnlockMutex(p_sys->acodec_first_dequeue_output_mutex);

	int inputBufferIndex = DequeueInput(0);
	ALOGE("Hello jni_um_vdec_decode! inputBufferIndex = %d", inputBufferIndex);
	if(inputBufferIndex >= 0){

//		ByteBuffer inputeBuffer = inputBuffers[inputBufferIndex];
//		inputeBuffer.clear();
//		inputeBuffer.put(bFrame, 0, frameLen);
//
//		codec.queueInputBuffer(inputBufferIndex, 0, frameLen, 0, 0);

		QueueInput(inputBufferIndex, buffer, len, 0, 0);

//		p_sys->i++;
//		if(p_sys->i%3 != 0)
//			return 0;
	}

//	BufferInfo info = new BufferInfo();
//
//	int outputBufferIndex = codec.dequeueOutputBuffer(info, 0);


#if 0
	int outputBufferIndex = DequeueOutput(0);
	ALOGE("Hello jni_um_vdec_decode! outputBufferIndex = %d", outputBufferIndex);
	if(outputBufferIndex >= 0){

		ALOGE( "codec outputBufferIndex %d" , outputBufferIndex);
		//codec.releaseOutputBuffer(outputBufferIndex, true);
		//outputBufferIndex = codec.dequeueOutputBuffer(info, 0);
		ReleaseOutput( outputBufferIndex, true);
		DequeueOutput(0);
	}else if (outputBufferIndex == MC_API_INFO_OUTPUT_BUFFERS_CHANGED) {

		ALOGE( "codec INFO_OUTPUT_BUFFERS_CHANGED");
		//outputBuffers = codec.getOutputBuffers();

		 jobject joutput_buffers;

		ALOGE( "output buffers changed");
		if (!jfields.get_output_buffers)
			return 0;
		env->DeleteGlobalRef(p_sys->output_buffers);

		joutput_buffers = env->CallObjectMethod(p_sys->codec,
												   jfields.get_output_buffers);
		if (CHECK_EXCEPTION())
		{
			ALOGE( "Exception in MediaCodec.getOutputBuffer");
			p_sys->output_buffers = NULL;
			return MC_API_ERROR;
		}
		p_sys->output_buffers = env->NewGlobalRef( joutput_buffers);
		env->DeleteLocalRef(joutput_buffers);
	} else if (outputBufferIndex == MC_API_INFO_OUTPUT_FORMAT_CHANGED) {

		ALOGE( "codec INFO_OUTPUT_FORMAT_CHANGED");
		// Subsequent data will conform to new format.
		//MediaFormat format = codec.getOutputFormat();


		jobject format = NULL;
		jstring format_string = NULL;
		jsize format_len;
		const char *format_ptr;

		format = env->CallObjectMethod(p_sys->codec, jfields.get_output_format);
		if (CHECK_EXCEPTION())
		{
			ALOGE( "Exception in MediaCodec.getOutputFormat");
			return MC_API_ERROR;
		}

		format_string = (jstring)env->CallObjectMethod( format, jfields.tostring);

		format_len = env->GetStringUTFLength(format_string);
		format_ptr = env->GetStringUTFChars(format_string, NULL);
		ALOGE( "output format changed: %.*s", format_len,format_ptr);
		env->ReleaseStringUTFChars(format_string, format_ptr);

		//p_out->type = MC_OUT_TYPE_CONF;

//			p_out->u.conf.video.width         = GET_INTEGER(format, "width");
//			p_out->u.conf.video.height        = GET_INTEGER(format, "height");
//			p_out->u.conf.video.stride        = GET_INTEGER(format, "stride");
//			p_out->u.conf.video.slice_height  = GET_INTEGER(format, "slice-height");
//			p_out->u.conf.video.pixel_format  = GET_INTEGER(format, "color-format");
//			p_out->u.conf.video.crop_left     = GET_INTEGER(format, "crop-left");
//			p_out->u.conf.video.crop_top      = GET_INTEGER(format, "crop-top");
//			p_out->u.conf.video.crop_right    = GET_INTEGER(format, "crop-right");
//			p_out->u.conf.video.crop_bottom   = GET_INTEGER(format, "crop-bottom");


		env->DeleteLocalRef(format);
	 }


#endif
	return 0;
}



extern "C" int um_vdec_init( int codec, int width, int height)
{
    JNIEnv *env;
    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
            ALOGE( "%s: SetupThreadEnv failed", __func__);
            return -1;
     }

    if (!InitJNIFields(env))
        return -1;

    p_sys = (struct mc_api_sys*)calloc(1, sizeof(struct mc_api_sys));
    if (!p_sys)
        return -1;

    p_sys->i_width = width;
    p_sys->i_height = height;
    p_sys->i_codec = codec;
    p_sys->i = 0;

    p_sys->acodec_first_dequeue_output_request = true;
    p_sys->acodec_first_dequeue_output_mutex = SDL_CreateMutex();
    p_sys->acodec_first_dequeue_output_cond  = SDL_CreateCond();

    p_sys->acodec_mutex                      = SDL_CreateMutex();
    p_sys->acodec_cond                       = SDL_CreateCond();

    const char *mime = NULL;
	size_t fmt_profile = 0;

	if(codec == 1){
		mime = "video/avc";
	}else if(codec == 2){
		mime = "video/mp4v-es";
	}else{
		ALOGE("mime is not support");
	}

	char* name = MediaCodec_GetName(mime, fmt_profile);

	Start(name, mime);

    return 0;

}

extern "C" int um_vdec_fini()
{
	ALOGE("Hello jni_um_vdec_fini!");
	Stop();

    SDL_DestroyCondP(&p_sys->acodec_first_dequeue_output_cond);
    SDL_DestroyMutexP(&p_sys->acodec_first_dequeue_output_mutex);
}

int SDL_AMediaCodec_dequeueOutputBuffer(SDL_AMediaCodecBufferInfo *info, int64_t timeoutUs)
{
    ALOGE("%s(%d)", __func__, (int)timeoutUs);

    JNIEnv *env = NULL;
    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        ALOGE("%s: SetupThreadEnv failed", __func__);
        return -1;
    }

    jint idx = -1;
    while (1) {
        idx = env->CallIntMethod(p_sys->codec, jfields.dequeue_output_buffer, p_sys->buffer_info, (jlong)timeoutUs);
        if (SDL_JNI_CatchException(env)) {
            ALOGE("%s: Exception\n", __func__);
            return -1;
        }
        if (idx == INFO_OUTPUT_BUFFERS_CHANGED) {
            ALOGE("%s: INFO_OUTPUT_BUFFERS_CHANGED\n", __func__);
//            SDL_JNI_DeleteGlobalRefP(env, &p_sys->input_buffer_array);
//            SDL_JNI_DeleteGlobalRefP(env, &p_sys->output_buffer_array);
            continue;
        } else if (idx == INFO_OUTPUT_FORMAT_CHANGED) {
            ALOGE("%s: INFO_OUTPUT_FORMAT_CHANGED\n", __func__);
        } else if (idx >= 0) {
           ALOGE("%s: buffer ready (%d) ====================\n", __func__, idx);
//            if (info) {
//                info->offset              = (*env)->GetIntField(env, opaque->output_buffer_info, g_clazz_BufferInfo.jfid_offset);
//                info->size                = (*env)->GetIntField(env, opaque->output_buffer_info, g_clazz_BufferInfo.jfid_size);
//                info->presentationTimeUs  = (*env)->GetLongField(env, opaque->output_buffer_info, g_clazz_BufferInfo.jfid_presentationTimeUs);
//                info->flags               = (*env)->GetIntField(env, opaque->output_buffer_info, g_clazz_BufferInfo.jfid_flags);
//            }
        }
        break;
    }

    return idx;
}


static int drain_output_buffer_l(JNIEnv *env, int64_t timeUs, int *dequeue_count)
{

    int                    ret      = 0;
    SDL_AMediaCodecBufferInfo bufferInfo;
    ssize_t                   output_buffer_index = 0;

    if (dequeue_count)
        *dequeue_count = 0;

    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        ALOGE("%s:create: SetupThreadEnv failed\n", __func__);
        return -1;
    }

    output_buffer_index = SDL_AMediaCodec_dequeueOutputBuffer( &bufferInfo, timeUs);
    if (output_buffer_index == INFO_OUTPUT_BUFFERS_CHANGED) {
        ALOGE("INFO_OUTPUT_FORMAT_CHANGED\n");
        // continue;
    } else if (output_buffer_index == INFO_OUTPUT_FORMAT_CHANGED) {
        ALOGE("INFO_OUTPUT_FORMAT_CHANGED\n");
//        SDL_AMediaFormat_deleteP(&p_sys->output_aformat);
//        opaque->output_aformat = SDL_AMediaCodec_getOutputFormat(opaque->acodec);
//        if (opaque->output_aformat) {
//            int width        = 0;
//            int height       = 0;
//            int color_format = 0;
//            int stride       = 0;
//            int slice_height = 0;
//            int crop_left    = 0;
//            int crop_top     = 0;
//            int crop_right   = 0;
//            int crop_bottom  = 0;
//
//            SDL_AMediaFormat_getInt32(opaque->output_aformat, "width",          &width);
//            SDL_AMediaFormat_getInt32(opaque->output_aformat, "height",         &height);
//            SDL_AMediaFormat_getInt32(opaque->output_aformat, "color-format",   &color_format);
//
//            SDL_AMediaFormat_getInt32(opaque->output_aformat, "stride",         &stride);
//            SDL_AMediaFormat_getInt32(opaque->output_aformat, "slice-height",   &slice_height);
//            SDL_AMediaFormat_getInt32(opaque->output_aformat, "crop-left",      &crop_left);
//            SDL_AMediaFormat_getInt32(opaque->output_aformat, "crop-top",       &crop_top);
//            SDL_AMediaFormat_getInt32(opaque->output_aformat, "crop-right",     &crop_right);
//            SDL_AMediaFormat_getInt32(opaque->output_aformat, "crop-bottom",    &crop_bottom);
//
//            // TI decoder could crash after reconfigure
//            // ffp_notify_msg3(ffp, FFP_MSG_VIDEO_SIZE_CHANGED, width, height);
//            // opaque->frame_width  = width;
//            // opaque->frame_height = height;
//            ALOGI(
//                "AMEDIACODEC__INFO_OUTPUT_FORMAT_CHANGED\n"
//                "    width-height: (%d x %d)\n"
//                "    color-format: (%s: 0x%x)\n"
//                "    stride:       (%d)\n"
//                "    slice-height: (%d)\n"
//                "    crop:         (%d, %d, %d, %d)\n"
//                ,
//                width, height,
//                SDL_AMediaCodec_getColorFormatName(color_format), color_format,
//                stride,
//                slice_height,
//                crop_left, crop_top, crop_right, crop_bottom);
//        }
//        // continue;
    } else if (output_buffer_index == INFO_TRY_AGAIN_LATER) {
        ALOGE("AMEDIACODEC__INFO_TRY_AGAIN_LATER\n");
        // continue;
    } else if (output_buffer_index < 0) {

    	ALOGE("output_buffer_index < 0\n");
        // enqueue packet as a fake picture
//        PacketQueue *fake_q = &opaque->fake_pictq;
//        SDL_LockMutex(fake_q->mutex);
//        if (!fake_q->abort_request && fake_q->nb_packets <= 0) {
//            SDL_CondWaitTimeout(fake_q->cond, fake_q->mutex, 1000);
//        }
//        SDL_UnlockMutex(fake_q->mutex);
//
//        if (fake_q->abort_request) {
//            ret = -1;
//            goto fail;
//        } else {
//            AVPacket pkt;
//            int dequeue_ret = ffp_packet_queue_get(&opaque->fake_pictq, &pkt, 0, &opaque->fake_pictq_serial);
//            if (dequeue_ret < 0) {
//                ret = -1;
//                goto fail;
//            } else if (dequeue_ret > 0) {
//                if (!ffp_is_flush_packet(&pkt)) {
//                    if (dequeue_count)
//                        ++*dequeue_count;
//
//                    ret = amc_queue_picture_fake(node, &pkt);
//                    av_free_packet(&pkt);
//                }
//                ret = 0;
//                goto fail;
//            }
//        }
    } else if (output_buffer_index >= 0) {
    	 ReleaseOutput( output_buffer_index, true);
    }

done:
    ret = 0;
fail:
    return ret;
}


static int drain_output_buffer(JNIEnv *env,  int64_t timeUs, int *dequeue_count)
{

    SDL_LockMutex(p_sys->acodec_mutex);

//    if (p_sys->acodec_flush_request || p_sys->acodec_reconfigure_request) {
//        // TODO: invalid picture here?
//        // let feed_input_buffer() get mutex
//        SDL_CondWaitTimeout(p_sys->acodec_cond, p_sys->acodec_mutex, 100);
//    }

    int ret = drain_output_buffer_l(env, timeUs, dequeue_count);
    SDL_UnlockMutex(p_sys->acodec_mutex);
    return ret;
}

extern "C" int um_vdec_render()
{
	 int                    dequeue_count = 0;
	JNIEnv *env;
	if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
		ALOGE( "%s: SetupThreadEnv failed", __func__);
		return -1;
	}

	int64_t timeUs = p_sys->acodec_first_dequeue_output_request ? 0 : 1000000;
	int ret = drain_output_buffer(env, timeUs, &dequeue_count);
	if (p_sys->acodec_first_dequeue_output_request) {
		SDL_LockMutex(p_sys->acodec_first_dequeue_output_mutex);
		p_sys->acodec_first_dequeue_output_request = false;
		SDL_CondSignal(p_sys->acodec_first_dequeue_output_cond);
		SDL_UnlockMutex(p_sys->acodec_first_dequeue_output_mutex);
	}
	if (ret != 0) {
		ALOGE( "============= %s error", __func__);
	}


#if 0
	ALOGE("Hello jni_um_vdec_render!");
	int outputBufferIndex = DequeueOutput(0);
	ALOGE("Hello jni_um_vdec_decode! outputBufferIndex = %d", outputBufferIndex);
	if(outputBufferIndex >= 0){

		ALOGE( "codec outputBufferIndex %d" , outputBufferIndex);
		//codec.releaseOutputBuffer(outputBufferIndex, true);
		//outputBufferIndex = codec.dequeueOutputBuffer(info, 0);
		ReleaseOutput( outputBufferIndex, true);
		DequeueOutput(0);
	}else if (outputBufferIndex == MC_API_INFO_OUTPUT_BUFFERS_CHANGED) {

		ALOGE( "codec INFO_OUTPUT_BUFFERS_CHANGED");
		//outputBuffers = codec.getOutputBuffers();

		 jobject joutput_buffers;

		ALOGE( "output buffers changed");
		if (!jfields.get_output_buffers)
			return 0;
		env->DeleteGlobalRef(p_sys->output_buffers);

		joutput_buffers = env->CallObjectMethod(p_sys->codec,
												   jfields.get_output_buffers);
		if (CHECK_EXCEPTION())
		{
			ALOGE( "Exception in MediaCodec.getOutputBuffer");
			p_sys->output_buffers = NULL;
			return MC_API_ERROR;
		}
		p_sys->output_buffers = env->NewGlobalRef( joutput_buffers);
		env->DeleteLocalRef(joutput_buffers);
	} else if (outputBufferIndex == MC_API_INFO_OUTPUT_FORMAT_CHANGED) {

		ALOGE( "codec INFO_OUTPUT_FORMAT_CHANGED");
		// Subsequent data will conform to new format.
		//MediaFormat format = codec.getOutputFormat();


		jobject format = NULL;
		jstring format_string = NULL;
		jsize format_len;
		const char *format_ptr;

		format = env->CallObjectMethod(p_sys->codec, jfields.get_output_format);
		if (CHECK_EXCEPTION())
		{
			ALOGE( "Exception in MediaCodec.getOutputFormat");
			return MC_API_ERROR;
		}

		format_string = (jstring)env->CallObjectMethod( format, jfields.tostring);

		format_len = env->GetStringUTFLength(format_string);
		format_ptr = env->GetStringUTFChars(format_string, NULL);
		ALOGE( "output format changed: %.*s", format_len,format_ptr);
		env->ReleaseStringUTFChars(format_string, format_ptr);

		//p_out->type = MC_OUT_TYPE_CONF;

//			p_out->u.conf.video.width         = GET_INTEGER(format, "width");
//			p_out->u.conf.video.height        = GET_INTEGER(format, "height");
//			p_out->u.conf.video.stride        = GET_INTEGER(format, "stride");
//			p_out->u.conf.video.slice_height  = GET_INTEGER(format, "slice-height");
//			p_out->u.conf.video.pixel_format  = GET_INTEGER(format, "color-format");
//			p_out->u.conf.video.crop_left     = GET_INTEGER(format, "crop-left");
//			p_out->u.conf.video.crop_top      = GET_INTEGER(format, "crop-top");
//			p_out->u.conf.video.crop_right    = GET_INTEGER(format, "crop-right");
//			p_out->u.conf.video.crop_bottom   = GET_INTEGER(format, "crop-bottom");


		env->DeleteLocalRef(format);
	 }

#endif

	return 0;
}
