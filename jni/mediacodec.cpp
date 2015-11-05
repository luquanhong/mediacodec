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


struct fields_t {
    jfieldID    surface;	//!< actually in android.view.Surface XXX
    jfieldID    surface_native;	//!< the surface passed from java
};

static  fields_t fields;


struct decoder_sys_t
{
    jclass media_codec_list_class, media_codec_class, media_format_class;
    jclass buffer_info_class, byte_buffer_class;
    jmethodID tostring;
    jmethodID get_codec_count, get_codec_info_at, is_encoder, get_capabilities_for_type;
    jfieldID profile_levels_field, profile_field, level_field;
    jmethodID get_supported_types, get_name;
    jmethodID create_by_codec_name, configure, start, stop, flush, release;
    jmethodID get_output_format, get_input_buffers, get_output_buffers;
    jmethodID dequeue_input_buffer, dequeue_output_buffer, queue_input_buffer;
    jmethodID release_output_buffer;
    jmethodID create_video_format, set_integer, set_bytebuffer, get_integer;
    jmethodID buffer_info_ctor;
    jmethodID allocate_direct, limit;
    jfieldID size_field, offset_field, pts_field;

    uint32_t nal_size;

    jobject codec;
    jobject buffer_info;
    jobject input_buffers, output_buffers;
    int pixel_format;
    int stride, slice_height;
    int crop_top, crop_left;
    char *name;

    int started;
    int decoded;

    void* native_surface;
    jobject surface;
    int i_output_buffers; /**< number of MediaCodec output buffers */
};


enum Types
{
    METHOD, STATIC_METHOD, FIELD
};


#define OFF(x) offsetof(struct decoder_sys_t, x)


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

struct member
{
    const char *name;
    const char *sig;
    const char *className;
    int offset;
    int type;
};

static const struct member members[] = {
    { "toString", "()Ljava/lang/String;", "java/lang/Object", OFF(tostring), METHOD },

    { "getCodecCount", "()I", "android/media/MediaCodecList", OFF(get_codec_count), STATIC_METHOD },
    { "getCodecInfoAt", "(I)Landroid/media/MediaCodecInfo;", "android/media/MediaCodecList", OFF(get_codec_info_at), STATIC_METHOD },

    { "isEncoder", "()Z", "android/media/MediaCodecInfo", OFF(is_encoder), METHOD },
    { "getSupportedTypes", "()[Ljava/lang/String;", "android/media/MediaCodecInfo", OFF(get_supported_types), METHOD },
    { "getName", "()Ljava/lang/String;", "android/media/MediaCodecInfo", OFF(get_name), METHOD },
    { "getCapabilitiesForType", "(Ljava/lang/String;)Landroid/media/MediaCodecInfo$CodecCapabilities;", "android/media/MediaCodecInfo", OFF(get_capabilities_for_type), METHOD },

    { "profileLevels", "[Landroid/media/MediaCodecInfo$CodecProfileLevel;", "android/media/MediaCodecInfo$CodecCapabilities", OFF(profile_levels_field), FIELD },
    { "profile", "I", "android/media/MediaCodecInfo$CodecProfileLevel", OFF(profile_field), FIELD },
    { "level", "I", "android/media/MediaCodecInfo$CodecProfileLevel", OFF(level_field), FIELD },

    { "createByCodecName", "(Ljava/lang/String;)Landroid/media/MediaCodec;", "android/media/MediaCodec", OFF(create_by_codec_name), STATIC_METHOD },
    { "configure", "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V", "android/media/MediaCodec", OFF(configure), METHOD },
    { "start", "()V", "android/media/MediaCodec", OFF(start), METHOD },
    { "stop", "()V", "android/media/MediaCodec", OFF(stop), METHOD },
    { "flush", "()V", "android/media/MediaCodec", OFF(flush), METHOD },
    { "release", "()V", "android/media/MediaCodec", OFF(release), METHOD },
    { "getOutputFormat", "()Landroid/media/MediaFormat;", "android/media/MediaCodec", OFF(get_output_format), METHOD },
    { "getInputBuffers", "()[Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_input_buffers), METHOD },
    { "getOutputBuffers", "()[Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_output_buffers), METHOD },
    { "dequeueInputBuffer", "(J)I", "android/media/MediaCodec", OFF(dequeue_input_buffer), METHOD },
    { "dequeueOutputBuffer", "(Landroid/media/MediaCodec$BufferInfo;J)I", "android/media/MediaCodec", OFF(dequeue_output_buffer), METHOD },
    { "queueInputBuffer", "(IIIJI)V", "android/media/MediaCodec", OFF(queue_input_buffer), METHOD },
    { "releaseOutputBuffer", "(IZ)V", "android/media/MediaCodec", OFF(release_output_buffer), METHOD },

    { "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;", "android/media/MediaFormat", OFF(create_video_format), STATIC_METHOD },
    { "setInteger", "(Ljava/lang/String;I)V", "android/media/MediaFormat", OFF(set_integer), METHOD },
    { "getInteger", "(Ljava/lang/String;)I", "android/media/MediaFormat", OFF(get_integer), METHOD },
    { "setByteBuffer", "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V", "android/media/MediaFormat", OFF(set_bytebuffer), METHOD },

    { "<init>", "()V", "android/media/MediaCodec$BufferInfo", OFF(buffer_info_ctor), METHOD },
    { "size", "I", "android/media/MediaCodec$BufferInfo", OFF(size_field), FIELD },
    { "offset", "I", "android/media/MediaCodec$BufferInfo", OFF(offset_field), FIELD },
    { "presentationTimeUs", "J", "android/media/MediaCodec$BufferInfo", OFF(pts_field), FIELD },

    { "allocateDirect", "(I)Ljava/nio/ByteBuffer;", "java/nio/ByteBuffer", OFF(allocate_direct), STATIC_METHOD },
    { "limit", "(I)Ljava/nio/Buffer;", "java/nio/ByteBuffer", OFF(limit), METHOD },

    { NULL, NULL, NULL, 0, 0 },
};


#define GET_INTEGER(obj, name) (*env)->CallIntMethod(env, obj, p_sys->get_integer, (*env)->NewStringUTF(env, name))



static int jstrcmp(JNIEnv* env, jobject str, const char* str2)
{
    jsize len = env->GetStringUTFLength( (jstring)str);
    if (len != (jsize) strlen(str2))
        return -1;
    const char *ptr = env->GetStringUTFChars((jstring)str, NULL);
    int ret = memcmp(ptr, str2, len);
    env->ReleaseStringUTFChars((jstring)str, ptr);

    return ret;
}



static void* get_surface(JNIEnv* env, jobject clazz)
{


    void* p = (void* )env->GetIntField(clazz, fields.surface_native);
    return (p);

}


struct decoder_sys_t *p_sys;


/*!
*	\brief	simple interface invoke without implement
*/
JNIEXPORT jint JNICALL jni_um_vdec_init(JNIEnv * env, jobject obj, jint codec, jint width, jint height)
{
	LOGE("Hello jni_um_vdec_init! ");


	const char* mime;

	if(codec == 1){

		mime = "video/avc";
	}else if(codec == 2){

		mime = "video/mp4v-es";
	}else{

		LOGE("mime is not support");
	}


	p_sys = (struct decoder_sys_t *)malloc( sizeof(struct decoder_sys_t));

	LOGE("Hello jni_um_video_surface_set!");

	jobject surface = env->GetObjectField(obj, fields.surface);
	LOGE("jni_um_vdec_setSurface(), surface=%p", surface);
	if (surface != NULL)
	{


		p_sys->surface = surface;
		p_sys->native_surface = get_surface(env, surface);


	}



	for (int i = 0; classes[i].name; i++) {

		 *(jclass*)((uint8_t*)p_sys + classes[i].offset) =
			env->FindClass( classes[i].name);

		if (env->ExceptionOccurred()) {
			LOGE("Unable to find class %s", classes[i].name);
			env->ExceptionClear();
			return -1;
		}
	}

	jclass last_class;
	for (int i = 0; members[i].name; i++) {
		if (i == 0 || strcmp(members[i].className, members[i - 1].className))
			last_class = env->FindClass(members[i].className);

		if (env->ExceptionOccurred()) {
			LOGE("Unable to find className %s", members[i].className);
			env->ExceptionClear();
			return -1;
		}

		switch (members[i].type) {
		case METHOD:
			*(jmethodID*)((uint8_t*)p_sys + members[i].offset) =
				env->GetMethodID( last_class, members[i].name, members[i].sig);
			break;
		case STATIC_METHOD:
			*(jmethodID*)((uint8_t*)p_sys + members[i].offset) =
				env->GetStaticMethodID( last_class, members[i].name, members[i].sig);
			break;
		case FIELD:
			*(jfieldID*)((uint8_t*)p_sys + members[i].offset) =
				env->GetFieldID(last_class, members[i].name, members[i].sig);
			break;
		}
		if (env->ExceptionOccurred()) {
			LOGE("Unable to find the member %s in %s",
					 members[i].name, members[i].className);
			env->ExceptionClear();
			return -1;
		}
	}




	int num_codecs = env->CallStaticIntMethod( p_sys->media_codec_list_class,
	                                                 p_sys->get_codec_count);
	jobject codec_name = NULL;

	for (int i = 0; i < num_codecs; i++) {
		jobject info =env->CallStaticObjectMethod(p_sys->media_codec_list_class,
													  p_sys->get_codec_info_at, i);
		if (env->CallBooleanMethod( info, p_sys->is_encoder)) {
			env->DeleteLocalRef( info);
			continue;
		}

		jobject codec_capabilities = env->CallObjectMethod(info, p_sys->get_capabilities_for_type,
															  env->NewStringUTF( mime));
		jobject profile_levels = NULL;
		int profile_levels_len = 0;
		if (codec_capabilities) {
			profile_levels = env->GetObjectField( codec_capabilities, p_sys->profile_levels_field);
//			if (profile_levels)
//				profile_levels_len = env->GetArrayLength( profile_levels);
		}
		LOGE("Number of profile levels: %d", profile_levels_len);
		jobject types = env->CallObjectMethod(info, p_sys->get_supported_types);
		int num_types = env->GetArrayLength( (jarray)types);
		bool found = false;

		for (int j = 0; j < num_types && !found; j++) {
			jobject type = env->GetObjectArrayElement((jobjectArray)types, j);
			if (!jstrcmp(env, type, mime))
				found = true;
			env->DeleteLocalRef( type);
		}

		if (found) {
			jstring name = (jstring)env->CallObjectMethod(info, p_sys->get_name);
			jsize name_len = env->GetStringUTFLength(name);
			const char *name_ptr = env->GetStringUTFChars(name, NULL);
			LOGE("using %.*s", name_len, name_ptr);
			p_sys->name = (char*)malloc(name_len + 1);
			memcpy(p_sys->name, name_ptr, name_len);
			p_sys->name[name_len] = '\0';
			env->ReleaseStringUTFChars( name, name_ptr);
			codec_name = name;
			break;
		}

		env->DeleteLocalRef(info);
	}


    if (!codec_name) {
        LOGE("No suitable codec matching %s was found", mime);
        return -1;
    }

    // This method doesn't handle errors nicely, it crashes if the codec isn't found.
    // (The same goes for createDecoderByType.) This is fixed in latest AOSP and in 4.2,
    // but not in 4.1 devices.
    p_sys->codec = env->CallStaticObjectMethod(p_sys->media_codec_class,
                                                  p_sys->create_by_codec_name, codec_name);
    p_sys->codec = env->NewGlobalRef(p_sys->codec);

    jobject format = env->CallStaticObjectMethod(p_sys->media_format_class,
                         p_sys->create_video_format, env->NewStringUTF(mime),
                         width, height);

//    if (p_dec->fmt_in.i_extra) {
//        // Allocate a byte buffer via allocateDirect in java instead of NewDirectByteBuffer,
//        // since the latter doesn't allocate storage of its own, and we don't know how long
//        // the codec uses the buffer.
//        int buf_size = p_dec->fmt_in.i_extra + 20;
//        jobject bytebuf = (*env)->CallStaticObjectMethod(env, p_sys->byte_buffer_class,
//                                                         p_sys->allocate_direct, buf_size);
//        uint32_t size = p_dec->fmt_in.i_extra;
//        uint8_t *ptr = (*env)->GetDirectBufferAddress(env, bytebuf);
//        if (p_dec->fmt_in.i_codec == VLC_CODEC_H264 && ((uint8_t*)p_dec->fmt_in.p_extra)[0] == 1) {
//            convert_sps_pps(p_dec, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra,
//                            ptr, buf_size,
//                            &size, &p_sys->nal_size);
//        } else {
//            memcpy(ptr, p_dec->fmt_in.p_extra, size);
//        }
//        env->CallObjectMethod(bytebuf, p_sys->limit, size);
//        env->CallVoidMethod( format, p_sys->set_bytebuffer,
//                               env->NewStringUTF( "csd-0"), bytebuf);
//        env->DeleteLocalRef( bytebuf);
//    }

    env->CallVoidMethod(p_sys->codec, p_sys->configure, format,  p_sys->surface, NULL, 0);
    if (env->ExceptionOccurred()) {
        LOGE("Exception occurred in MediaCodec.configure");
        env->ExceptionClear();
        return -1;
    }
    env->CallVoidMethod( p_sys->codec, p_sys->start);
    if (env->ExceptionOccurred()) {
        LOGE("Exception occurred in MediaCodec.start");
        env->ExceptionClear();
        return -1;
    }
    p_sys->started = 1;

    p_sys->input_buffers = env->CallObjectMethod( p_sys->codec, p_sys->get_input_buffers);
    p_sys->output_buffers = env->CallObjectMethod( p_sys->codec, p_sys->get_output_buffers);
    p_sys->buffer_info = env->NewObject( p_sys->buffer_info_class, p_sys->buffer_info_ctor);
    p_sys->input_buffers = env->NewGlobalRef( p_sys->input_buffers);
    p_sys->output_buffers = env->NewGlobalRef( p_sys->output_buffers);
    p_sys->buffer_info = env->NewGlobalRef( p_sys->buffer_info);
	p_sys->i_output_buffers = env->GetArrayLength( (jobjectArray)p_sys->output_buffers);
    env->DeleteLocalRef(format);

    //mJvm->DetachCurrentThread();
    //(*myVm)->DetachCurrentThread(myVm);
    return 0;


}




static void m_um_vdec_getOutput(JNIEnv *env){



	while (1) {

		int index = env->CallIntMethod( p_sys->codec, p_sys->dequeue_output_buffer,
										  p_sys->buffer_info, (jlong) 0);

		if (env->ExceptionOccurred()) {
			env->ExceptionClear();
			//p_sys->error_state = true;
			return;
		}

		 if (index >= 0) {

			LOGE("dequeue_output_buffer index >= 0 ");



//			(*env)->GetLongField(env, p_sys->buffer_info, p_sys->pts_field);
//
//			{
//
//				jobject buf = (*env)->GetObjectArrayElement(env, p_sys->output_buffers, index);
//				jsize buf_size = (*env)->GetDirectBufferCapacity(env, buf);
//				uint8_t *ptr = (*env)->GetDirectBufferAddress(env, buf);
//
//				int size = (*env)->GetIntField(env, p_sys->buffer_info, p_sys->size_field);
//				int offset = (*env)->GetIntField(env, p_sys->buffer_info, p_sys->offset_field);
//				ptr += offset; // Check the size parameter as well
//
//				unsigned int chroma_div;
////				GetVlcChromaSizes(p_dec->fmt_out.i_codec, p_dec->fmt_out.video.i_width,
////								  p_dec->fmt_out.video.i_height, NULL, NULL, &chroma_div);
////				CopyOmxPicture(p_sys->pixel_format, p_pic, p_sys->slice_height, p_sys->stride,
////							   ptr, chroma_div, &p_sys->architecture_specific_data);
//				(*env)->CallVoidMethod(env, p_sys->codec, p_sys->release_output_buffer, index, false);
//
//				jthrowable exception = (*env)->ExceptionOccurred(env);
//				if(exception != NULL) {
//					jclass illegalStateException = (*env)->FindClass(env, "java/lang/IllegalStateException");
//					if((*env)->IsInstanceOf(env, exception, illegalStateException)) {
//						LOGE("Codec error (IllegalStateException) in MediaCodec.releaseOutputBuffer");
//						(*env)->ExceptionClear(env);
//						(*env)->DeleteLocalRef(env, illegalStateException);
//						p_sys->error_state = true;
//					}
//				}
//				(*env)->DeleteLocalRef(env, buf);
//			}

			env->CallVoidMethod( p_sys->codec, p_sys->release_output_buffer, index, true);

			LOGE("dequeue_output_buffer index = %d ", index);
			env->CallIntMethod( p_sys->codec, p_sys->dequeue_output_buffer, p_sys->buffer_info, (jlong) 0);

			LOGE("dequeue_output_buffer index = %d  1", index);

			 return;
		 } else if (index == INFO_OUTPUT_BUFFERS_CHANGED) {

			 LOGE("dequeue_output_buffer INFO_OUTPUT_BUFFERS_CHANGED ");

			 env->DeleteGlobalRef(p_sys->output_buffers);

			 p_sys->output_buffers = env->CallObjectMethod( p_sys->codec,
															 p_sys->get_output_buffers);
			 p_sys->output_buffers = env->NewGlobalRef( p_sys->output_buffers);

		 }else if (index == INFO_OUTPUT_FORMAT_CHANGED) {

			 LOGE("dequeue_output_buffer INFO_OUTPUT_FORMAT_CHANGED ");
		 }else {

			 //(*env)->CallVoidMethod(env, p_sys->codec, p_sys->release_output_buffer, index, false);
			 //LOGE("dequeue_output_buffer index = %d ", index);

			 return;
		 }

	}

}




/*!
*	\brief	main decode function,
*	\param	in	it's nal buf
*	\param	len	it's the length of nal
*	\return	if success, it will return UM_SUCCESS
*/
JNIEXPORT jint JNICALL jni_um_vdec_decode(JNIEnv * env, jobject obj, jbyteArray in, jint len)
{
	LOGE("Hello jni_um_vdec_decode!");


	jlong timeout = 0;
	const int max_polling_attempts = 50;
	int attempts = 0;

	jbyte* buffer = (jbyte*)env->GetByteArrayElements( in, 0);

	while (true) {
		int index = env->CallIntMethod( p_sys->codec, p_sys->dequeue_input_buffer, timeout);

		if (env->ExceptionOccurred()) {
			env->ExceptionClear();
			//p_sys->error_state = true;
			break;
		}

		if (index < 0) {

		continue;
		}

		if(index >= 0){

			LOGE("dequeue_input_buffer index =%d ", index);
		}

		jobject buf = env->GetObjectArrayElement( (jobjectArray)p_sys->input_buffers, index);
		jsize size = env->GetDirectBufferCapacity( buf);
		uint8_t *bufptr = (uint8_t*)env->GetDirectBufferAddress( buf);

		if (size > len)
			size = len;

		memcpy(bufptr, buffer, size);

//		convert_h264_to_annexb(bufptr, size, p_sys->nal_size, &convert_state);

		env->CallVoidMethod( p_sys->codec, p_sys->queue_input_buffer, index, 0, size, 0, 0);
		env->DeleteLocalRef( buf);
		p_sys->decoded = true;
		break;
	}


	m_um_vdec_getOutput(env);

	return 0;
}


/*!
*	\brief	simple interface invoke without implement
*/
JNIEXPORT jint JNICALL jni_um_vdec_fini(JNIEnv * env, jobject obj)
{

	LOGE("Hello jni_um_vdec_fini!");

//	JNIEnv *env = NULL;
//	(*myVm)->AttachCurrentThread(myVm, &env, NULL);

	if (p_sys->input_buffers)
		env->DeleteGlobalRef(p_sys->input_buffers);

	if (p_sys->output_buffers)
		env->DeleteGlobalRef(p_sys->output_buffers);

	if (p_sys->codec) {

		if (p_sys->started)
			env->CallVoidMethod(p_sys->codec, p_sys->stop);

		env->CallVoidMethod(p_sys->codec, p_sys->release);
		env->DeleteGlobalRef(p_sys->codec);
	}
	if (p_sys->buffer_info)
		env->DeleteGlobalRef(p_sys->buffer_info);

//	(*myVm)->DetachCurrentThread(myVm);

	free(p_sys->name);
	free(p_sys);

	return 0;
}

/*!
*	\brief	relationship surface with native window,the surface is passed from java
*/
JNIEXPORT jint JNICALL jni_um_vdec_setSurface(JNIEnv * env, jobject obj)
{
	LOGE("Hello jni_um_video_surface_set!");

	return -1;
}


static const char *classPathName = "com/example/mediacodecdemo/MainActivity";

/*!
*	\brief	funciton table from java to native
*/
static JNINativeMethod methods[] = {
				{"um_vdec_init",		"(III)I",	(void*)jni_um_vdec_init},
				{"um_vdec_decode",		"([BI)I",					(void*)jni_um_vdec_decode},
				{"um_vdec_fini",		"()I",						(void*)jni_um_vdec_fini},
				{"um_vdec_setSurface",	"()I",						(void*)jni_um_vdec_setSurface},
};


/*!
 * 	\brief	Register several native methods for one class.
 */
static int registerNativeMethods(JNIEnv* env, const char* className,
                                JNINativeMethod* gMethods, int numMethods)
{
	jclass clazz;
	fprintf(stderr, "RegisterNatives start for '%s'", className);
	clazz = env->FindClass(className);	//!< get java class instance from my java class
	if (clazz == NULL) {
			fprintf(stderr, "Native registration unable to find class '%s'", className);
			return JNI_FALSE;
	}

	fields.surface = env->GetFieldID(clazz, "mSurface", "Landroid/view/Surface;");	//!< get the class of surface var in the java class
	if (fields.surface == NULL) {
		LOGE("Can't find GameCloudPlayer.mSurface");
		return -1;
	}

	jclass surface = env->FindClass("android/view/Surface");	//!< get java class instance from system class
	if (surface == NULL) {
		LOGE("Can't find android/view/Surface");
		return -1;
	}
	fields.surface_native = env->GetFieldID(surface, "mNativeSurface", "I");	//!< get the no of mNativeSurface var in the system


	if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
			fprintf(stderr, "RegisterNatives failed for '%s'", className);
			return JNI_FALSE;
	}

	return JNI_TRUE;
}

/*!
 * 	\brief Register native methods for all classes we know about.
 * 	\return JNI_TRUE on success.
 */
static int registerNatives(JNIEnv* env)
{
        if (!registerNativeMethods(env, classPathName,
                methods, sizeof(methods) / sizeof(methods[0]))) {
                        return JNI_FALSE;
        }

        return JNI_TRUE;
}


/*!
 * 	\brief switch c and c++
 */
typedef union {
        JNIEnv* env;
        void* venv;
} UnionJNIEnvToVoid;

/*!
 * 	\brief This is called by the VM when the shared library is first loaded.
 */
jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
        UnionJNIEnvToVoid uenv;
        uenv.venv = NULL;
        jint result = -1;
        JNIEnv* env = NULL;

        printf("JNI_OnLoad");

        if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK) {
                fprintf(stderr, "GetEnv failed");
                goto bail;
        }

        env = uenv.env;

        if (registerNatives(env) != JNI_TRUE) {
                fprintf(stderr, "GetEnv failed");
                goto bail;
        }

        result = JNI_VERSION_1_4;
bail:
    return result;
}









