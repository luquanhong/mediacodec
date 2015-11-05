#ifndef _MEDIACODEC_NATIVE_H_
#define _MEDIACODEC_NATIVE_H_

#include "jni.h"

#include <media/hardware/CryptoAPI.h>
#include <media/stagefright/foundation/ABase.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>



using namespace android;



struct JMediaCodec : public RefBase {
    JMediaCodec(
            JNIEnv *env, jobject thiz,
            const char *name, bool nameIsType, bool encoder);

    status_t initCheck() const;

    status_t configure(
            const sp<AMessage> &format,
            const sp<IGraphicBufferProducer> &bufferProducer,
            const sp<ICrypto> &crypto,
            int flags);

    status_t createInputSurface(sp<IGraphicBufferProducer>* bufferProducer);

    status_t start();
    status_t stop();

    status_t flush();

    status_t queueInputBuffer(
            size_t index,
            size_t offset, size_t size, int64_t timeUs, uint32_t flags,
            AString *errorDetailMsg);

    status_t queueSecureInputBuffer(
            size_t index,
            size_t offset,
            const CryptoPlugin::SubSample *subSamples,
            size_t numSubSamples,
            const uint8_t key[16],
            const uint8_t iv[16],
            CryptoPlugin::Mode mode,
            int64_t presentationTimeUs,
            uint32_t flags,
            AString *errorDetailMsg);

    status_t dequeueInputBuffer(size_t *index, int64_t timeoutUs);

    status_t dequeueOutputBuffer(
            JNIEnv *env, jobject bufferInfo, size_t *index, int64_t timeoutUs);

    status_t releaseOutputBuffer(size_t index, bool render);

    status_t signalEndOfInputStream();

    status_t getOutputFormat(JNIEnv *env, jobject *format) const;

    status_t getBuffers(
            JNIEnv *env, bool input, jobjectArray *bufArray) const;

    status_t getName(JNIEnv *env, jstring *name) const;

    void setVideoScalingMode(int mode);

protected:
    virtual ~JMediaCodec();

private:
    jclass mClass;
    jweak mObject;
    sp<Surface> mSurfaceTextureClient;

    sp<ALooper> mLooper;
    sp<MediaCodec> mCodec;

    DISALLOW_EVIL_CONSTRUCTORS(JMediaCodec);
};


#endif
