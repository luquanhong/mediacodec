

#include "mediacodec.h"


extern JavaVM *myVm;
extern jobject vout_android_java_surf;



struct fields_t {
    jfieldID    surface;	//!< actually in android.view.Surface XXX
    jfieldID    surface_native;	//!< the surface passed from java
};

static struct fields_t fields;



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

    bool started;
    bool decoded;
    bool error_state;
    bool error_event_sent;

//    void* native_surface;
 //   jobject surface;

//    ArchitectureSpecificCopyData architecture_specific_data;

    /* Direct rendering members. */
    bool direct_rendering;
    int i_output_buffers; /**< number of MediaCodec output buffers */
//    picture_t** inflight_picture; /**< stores the inflight picture for each output buffer or NULL */
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
    const char *class;
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





static JNIEnv* m_um_vdec_getEnv()
{
    JNIEnv* env = NULL;
    if ( (*myVm)->GetEnv(myVm, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        return NULL;
    }

    return env;
}


static JNIEnv* m_um_vdec_getJNIEnv(bool* needsDetach) {

    *needsDetach = false;
    JNIEnv *env = m_um_vdec_getEnv();
    if (env == NULL) {

        JavaVMAttachArgs args = {JNI_VERSION_1_4, NULL, NULL};
        int result = (*myVm)->AttachCurrentThread(myVm,&env, (void*) &args);

        if (result != JNI_OK) {

            return NULL;
        }

        *needsDetach = true;
    }
    return env;
}

static void m_um_vdec_detachJNI() {

	int result = (*myVm)->DetachCurrentThread(myVm);

	if(result != JNI_OK){
        LOGE(" s_upjni_native_detachJNI thread detach failed: %#x", result);
    }
}


static int jstrcmp(JNIEnv* env, jobject str, const char* str2)
{
    jsize len = (*env)->GetStringUTFLength(env, str);
    if (len != (jsize) strlen(str2))
        return -1;
    const char *ptr = (*env)->GetStringUTFChars(env, str, NULL);
    int ret = memcmp(ptr, str2, len);
    (*env)->ReleaseStringUTFChars(env, str, ptr);
    return ret;
}



static void* get_surface(JNIEnv* env, jobject clazz)
{


    void* p = (void* )(*env)->GetIntField(env, clazz, fields.surface_native);
    return (p);

}


struct decoder_sys_t *p_sys;





int um_vdec_init( int codec, int width, int height)
{
	LOGE("Hello jni_um_vdec_init! ");

	int i, j;

	const char *mime = NULL;

	size_t fmt_profile = 0;

	if(codec == 1){

		mime = "video/avc";
	}else if(codec == 2){

		mime = "video/mp4v-es";
	}else{

		LOGE("mime is not support");
	}
    //(*myVm)->AttachCurrentThread(myVm, &env, NULL);
    LOGE("Hello jni_um_vdec_init! 4");
    bool need = false;
    JNIEnv *env = m_um_vdec_getJNIEnv(&need);

	p_sys = (struct decoder_sys_t *)malloc( sizeof(struct decoder_sys_t));


//	jobject surface = (*env)->GetObjectField(env, obj, fields.surface);
//	LOGE("jni_um_vdec_setSurface(), surface=%p", surface);
//	if (surface != NULL)
//	{
//
//
//		p_sys->surface = surface;
//		p_sys->native_surface = get_surface(env, surface);
//
//
//	}


	for ( i = 0; classes[i].name; i++) {
		*(jclass*)((uint8_t*)p_sys + classes[i].offset) =
			(*env)->FindClass(env, classes[i].name);

		if ((*env)->ExceptionOccurred(env)) {
			LOGE("Unable to find class %s", classes[i].name);
			(*env)->ExceptionClear(env);
			goto error;
		}
	}


	jclass last_class;
	for ( i = 0; members[i].name; i++) {
		if (i == 0 || strcmp(members[i].class, members[i - 1].class))
			last_class = (*env)->FindClass(env, members[i].class);

		if ((*env)->ExceptionOccurred(env)) {
			LOGE("Unable to find class %s", members[i].class);
			(*env)->ExceptionClear(env);
			goto error;
		}

		switch (members[i].type) {
		case METHOD:
			*(jmethodID*)((uint8_t*)p_sys + members[i].offset) =
				(*env)->GetMethodID(env, last_class, members[i].name, members[i].sig);
			break;
		case STATIC_METHOD:
			*(jmethodID*)((uint8_t*)p_sys + members[i].offset) =
				(*env)->GetStaticMethodID(env, last_class, members[i].name, members[i].sig);
			break;
		case FIELD:
			*(jfieldID*)((uint8_t*)p_sys + members[i].offset) =
				(*env)->GetFieldID(env, last_class, members[i].name, members[i].sig);
			break;
		}
		if ((*env)->ExceptionOccurred(env)) {
			LOGE("Unable to find the member %s in %s",
					 members[i].name, members[i].class);
			(*env)->ExceptionClear(env);
			goto error;
		}
	}


	 LOGE("Hello jni_um_vdec_init! 5");
	int num_codecs = (*env)->CallStaticIntMethod(env, p_sys->media_codec_list_class,
	                                                 p_sys->get_codec_count);
	jobject codec_name = NULL;

	for ( i = 0; i < num_codecs; i++) {
		jobject info = (*env)->CallStaticObjectMethod(env, p_sys->media_codec_list_class,
													  p_sys->get_codec_info_at, i);
		if ((*env)->CallBooleanMethod(env, info, p_sys->is_encoder)) {
			(*env)->DeleteLocalRef(env, info);
			continue;
		}

		jobject codec_capabilities = (*env)->CallObjectMethod(env, info, p_sys->get_capabilities_for_type,
															  (*env)->NewStringUTF(env, mime));
		jobject profile_levels = NULL;
		int profile_levels_len = 0;
		if (codec_capabilities) {
			profile_levels = (*env)->GetObjectField(env, codec_capabilities, p_sys->profile_levels_field);
			if (profile_levels)
				profile_levels_len = (*env)->GetArrayLength(env, profile_levels);
		}
		LOGE("Number of profile levels: %d", profile_levels_len);

		jobject types = (*env)->CallObjectMethod(env, info, p_sys->get_supported_types);
		int num_types = (*env)->GetArrayLength(env, types);
		bool found = false;
		for ( j = 0; j < num_types && !found; j++) {
			jobject type = (*env)->GetObjectArrayElement(env, types, j);
			if (!jstrcmp(env, type, mime)) {
				/* The mime type is matching for this component. We
				   now check if the capabilities of the codec is
				   matching the video format. */
//				if (p_dec->fmt_in.i_codec == VLC_CODEC_H264 && fmt_profile) {
//					for (int i = 0; i < profile_levels_len && !found; ++i) {
//						jobject profile_level = (*env)->GetObjectArrayElement(env, profile_levels, i);
//
//						int omx_profile = (*env)->GetLongField(env, profile_level, p_sys->profile_field);
//						size_t codec_profile = convert_omx_to_profile_idc(omx_profile);
//						if (codec_profile != fmt_profile)
//							continue;
//						/* Some encoders set the level too high, thus we ignore it for the moment.
//						   We could try to guess the actual profile based on the resolution. */
//						found = true;
//					}
//				}
//				else
					found = true;
			}
			(*env)->DeleteLocalRef(env, type);
		}
		if (found) {
			jobject name = (*env)->CallObjectMethod(env, info, p_sys->get_name);
			jsize name_len = (*env)->GetStringUTFLength(env, name);

			 LOGE("Hello jni_um_vdec_init! 6");
			const char *name_ptr = (*env)->GetStringUTFChars(env, name, NULL);
			LOGE("using %.*s", name_len, name_ptr);
			p_sys->name = malloc(name_len + 1);
			memcpy(p_sys->name, name_ptr, name_len);
			p_sys->name[name_len] = '\0';
			(*env)->ReleaseStringUTFChars(env, name, name_ptr);
			codec_name = name;
			break;
		}
		(*env)->DeleteLocalRef(env, info);
	}
	 LOGE("Hello jni_um_vdec_init! 7");
	if (!codec_name) {
		LOGE("No suitable codec matching %s was found", mime);
		goto error;
	}

	// This method doesn't handle errors nicely, it crashes if the codec isn't found.
	// (The same goes for createDecoderByType.) This is fixed in latest AOSP and in 4.2,
	// but not in 4.1 devices.
	p_sys->codec = (*env)->CallStaticObjectMethod(env, p_sys->media_codec_class,
												  p_sys->create_by_codec_name, codec_name);
	p_sys->codec = (*env)->NewGlobalRef(env, p_sys->codec);
	 LOGE("Hello jni_um_vdec_init! 8");
	jobject format = (*env)->CallStaticObjectMethod(env, p_sys->media_format_class,
						 p_sys->create_video_format, (*env)->NewStringUTF(env, mime),
						 width, height);




	p_sys->direct_rendering = false; //jni_IsVideoPlayerActivityCreated() && var_InheritBool(p_dec, CFG_PREFIX "dr");
//	if (p_sys->direct_rendering) {
//		jobject surf = jni_LockAndGetAndroidJavaSurface();
//		if (surf) {
//			// Configure MediaCodec with the Android surface.
//			(*env)->CallVoidMethod(env, p_sys->codec, p_sys->configure, format, surf, NULL, 0);
//			if ((*env)->ExceptionOccurred(env)) {
//				LOGE("Exception occurred in MediaCodec.configure with an output surface.");
//				(*env)->ExceptionClear(env);
//				jni_UnlockAndroidSurface();
//				goto error;
//			}
//			p_dec->fmt_out.i_codec = VLC_CODEC_ANDROID_OPAQUE;
//		} else {
//			LOGE("Failed to get the Android Surface, disabling direct rendering.");
//			p_sys->direct_rendering = false;
//		}
//		jni_UnlockAndroidSurface();
//	}
	if (!p_sys->direct_rendering) {
		(*env)->CallVoidMethod(env, p_sys->codec, p_sys->configure, format, vout_android_java_surf, NULL, 0);
		if ((*env)->ExceptionOccurred(env)) {
			LOGE("Exception occurred in MediaCodec.configure");
			(*env)->ExceptionClear(env);
			goto error;
		}
	}
	 LOGE("Hello jni_um_vdec_init! 9");
	(*env)->CallVoidMethod(env, p_sys->codec, p_sys->start);
	if ((*env)->ExceptionOccurred(env)) {
		LOGE("Exception occurred in MediaCodec.start");
		(*env)->ExceptionClear(env);
		goto error;
	}
	p_sys->started = true;
	 LOGE("Hello jni_um_vdec_init! 10");
	p_sys->input_buffers = (*env)->CallObjectMethod(env, p_sys->codec, p_sys->get_input_buffers);
	p_sys->output_buffers = (*env)->CallObjectMethod(env, p_sys->codec, p_sys->get_output_buffers);
	p_sys->buffer_info = (*env)->NewObject(env, p_sys->buffer_info_class, p_sys->buffer_info_ctor);
	p_sys->input_buffers = (*env)->NewGlobalRef(env, p_sys->input_buffers);
	p_sys->output_buffers = (*env)->NewGlobalRef(env, p_sys->output_buffers);
	p_sys->buffer_info = (*env)->NewGlobalRef(env, p_sys->buffer_info);
	p_sys->i_output_buffers = (*env)->GetArrayLength(env, p_sys->output_buffers);
//	p_sys->inflight_picture = calloc(1, sizeof(picture_t*) * p_sys->i_output_buffers);
//	if (!p_sys->inflight_picture)
//		goto error;
	(*env)->DeleteLocalRef(env, format);

	//(*myVm)->DetachCurrentThread(myVm);
	if(need == true){

		m_um_vdec_detachJNI();
	}

	return 0;

error:
	//(*myVm)->DetachCurrentThread(myVm);
	return -1;
}


static void m_um_vdec_getOutput(JNIEnv *env){



	while (1) {

		int index = (*env)->CallIntMethod(env, p_sys->codec, p_sys->dequeue_output_buffer,
										  p_sys->buffer_info, (jlong) 0);

		if ((*env)->ExceptionOccurred(env)) {
			(*env)->ExceptionClear(env);
			p_sys->error_state = true;
			return;
		}
		LOGE("dequeue_output_buffer index %d ", index);
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

			(*env)->CallVoidMethod(env, p_sys->codec, p_sys->release_output_buffer, index, true);

			LOGE("dequeue_output_buffer index = %d ", index);
			(*env)->CallIntMethod(env, p_sys->codec, p_sys->dequeue_output_buffer, p_sys->buffer_info, (jlong) 0);

			LOGE("dequeue_output_buffer index = %d  1", index);

			 return;
		 } else if (index == INFO_OUTPUT_BUFFERS_CHANGED) {

			 LOGE("dequeue_output_buffer INFO_OUTPUT_BUFFERS_CHANGED ");

			 (*env)->DeleteGlobalRef(env, p_sys->output_buffers);

			 p_sys->output_buffers = (*env)->CallObjectMethod(env, p_sys->codec,
															 p_sys->get_output_buffers);
			 p_sys->output_buffers = (*env)->NewGlobalRef(env, p_sys->output_buffers);

		 }else if (index == INFO_OUTPUT_FORMAT_CHANGED) {

			 LOGE("dequeue_output_buffer INFO_OUTPUT_FORMAT_CHANGED ");
		 }else {
			 LOGE("dequeue_output_buffer < 0  ");
			 //(*env)->CallVoidMethod(env, p_sys->codec, p_sys->release_output_buffer, index, false);
			 //LOGE("dequeue_output_buffer index = %d ", index);

			 return;
		 }

	}

}


int um_vdec_decode(char* buffer, int len)
{
	LOGE("Hello jni_um_vdec_decode!");

	jlong timeout = 0;
	const int max_polling_attempts = 50;
	int attempts = 0;

    bool need = false;

    JNIEnv *env = m_um_vdec_getJNIEnv(&need);
	//(*myVm)->AttachCurrentThread(myVm, &env, NULL);

	while (true) {
		int index = (*env)->CallIntMethod(env, p_sys->codec, p_sys->dequeue_input_buffer, timeout);

		if ((*env)->ExceptionOccurred(env)) {
			(*env)->ExceptionClear(env);
			p_sys->error_state = true;
			break;
		}

		if (index < 0) {
//			GetOutput(p_dec, env, &p_pic);
//			if (p_pic) {
//				/* If we couldn't get an available input buffer but a
//				 * decoded frame is available, we return the frame
//				 * without assigning NULL to *pp_block. The next call
//				 * to DecodeVideo will try to send the input packet again.
//				 */
//				(*myVm)->DetachCurrentThread(myVm);
//				return p_pic;
//			}
//			timeout = 30*1000;
//			++attempts;
//			/* With opaque DR the output buffers are released by the
//			   vout therefore we implement a timeout for polling in
//			   order to avoid being indefinitely stalled in this loop. */
//			if (p_sys->direct_rendering && attempts == max_polling_attempts) {
//				picture_t *invalid_picture = decoder_NewPicture(p_dec);
//				if (invalid_picture) {
//					invalid_picture->date = VLC_TS_INVALID;
//					picture_sys_t *p_picsys = invalid_picture->p_sys;
//					p_picsys->pf_display_callback = NULL;
//					p_picsys->pf_unlock_callback = NULL;
//					p_picsys->p_dec = NULL;
//					p_picsys->i_index = -1;
//					p_picsys->b_valid = false;
//				}
//				else {
//					/* If we cannot return a picture we must free the
//					   block since the decoder will proceed with the
//					   next block. */
//					block_Release(p_block);
//					*pp_block = NULL;
//				}
//				(*myVm)->DetachCurrentThread(myVm);
//				return invalid_picture;
//			}

			//LOGE("dequeue_input_buffer index < 0 ");
			continue;
		}

		if(index >= 0){

			LOGE("dequeue_input_buffer index =%d ", index);
		}

		jobject buf = (*env)->GetObjectArrayElement(env, p_sys->input_buffers, index);
		jsize size = (*env)->GetDirectBufferCapacity(env, buf);
		uint8_t *bufptr = (*env)->GetDirectBufferAddress(env, buf);

		if (size > len)
			size = len;

		memcpy(bufptr, buffer, size);

//		convert_h264_to_annexb(bufptr, size, p_sys->nal_size, &convert_state);

		(*env)->CallVoidMethod(env, p_sys->codec, p_sys->queue_input_buffer, index, 0, size, 0, 0);
		(*env)->DeleteLocalRef(env, buf);
		p_sys->decoded = true;
		break;
	}


	m_um_vdec_getOutput(env);
	//(*myVm)->DetachCurrentThread(myVm);

	if(need == true){

		m_um_vdec_detachJNI();
	}

	return 0;
}



int um_vdec_fini()
{

	LOGE("Hello jni_um_vdec_fini!");

	JNIEnv *env = NULL;
	(*myVm)->AttachCurrentThread(myVm, &env, NULL);

	if (p_sys->input_buffers)
		(*env)->DeleteGlobalRef(env, p_sys->input_buffers);

	if (p_sys->output_buffers)
		(*env)->DeleteGlobalRef(env, p_sys->output_buffers);

	if (p_sys->codec) {
		if (p_sys->started)
			(*env)->CallVoidMethod(env, p_sys->codec, p_sys->stop);
		(*env)->CallVoidMethod(env, p_sys->codec, p_sys->release);
		(*env)->DeleteGlobalRef(env, p_sys->codec);
	}
	if (p_sys->buffer_info)
		(*env)->DeleteGlobalRef(env, p_sys->buffer_info);

	(*myVm)->DetachCurrentThread(myVm);

	free(p_sys->name);
//	ArchitectureSpecificCopyHooksDestroy(p_sys->pixel_format, &p_sys->architecture_specific_data);
//	free(p_sys->inflight_picture);
	free(p_sys);

	return 0;
}

