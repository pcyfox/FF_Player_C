//
// Created by LN on 2021/1/4.
//

#include <jni.h>
#include "VMLoader.h"

#ifdef __cplusplus
extern "C" {
#include "Muxer.h"
#include "AndroidLog.h"
#endif
#ifdef __cplusplus
}
#endif

#include <pthread.h>

static jmethodID jMid_onMuxProgress = NULL;
static jclass clazz = NULL;

void *onMuxProgress(float p) {
    JNIEnv *env = NULL;
    int ret = vm->AttachCurrentThread(&env, NULL);
    if (ret == 0) {
        if (jMid_onMuxProgress == NULL) {
            jMid_onMuxProgress = env->GetStaticMethodID((jclass) clazz, "onMuxProgress", "(F)V");
        }
        env->CallStaticVoidMethod(clazz, jMid_onMuxProgress, p);
    } else {
        LOGE("onStateChange() get jEnv error");
    }
    return nullptr;
}

//--------------------------------MUXER------------------------------------------------
extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_AVMuxer_muxAV(JNIEnv *env, jclass clazz, jstring audio_file,
                                          jstring video_file, jstring out_file) {


    char *audioFile = (char *) env->GetStringUTFChars(audio_file, 0);
    char *videoFile = (char *) env->GetStringUTFChars(video_file, 0);
    char *outFile = (char *) env->GetStringUTFChars(out_file, 0);
    if (audioFile && videoFile && outFile) {
        return static_cast<jint>(MuxAVFile(audioFile, videoFile, outFile,
                                           reinterpret_cast<void (*)(float)>(onMuxProgress)));
    } else {
        return PLAYER_RESULT_ERROR;
    }

}


extern "C"
JNIEXPORT void JNICALL
Java_com_pcyfox_lib_1ffmpeg_AVMuxer_init(JNIEnv *env, jclass muxer) {
    if (!vm) {
        env->GetJavaVM(&vm);
    }
    clazz = (jclass) env->NewGlobalRef(muxer);
}



extern "C"
JNIEXPORT void JNICALL
Java_com_pcyfox_lib_1ffmpeg_AVMuxer_release(JNIEnv *env, jclass clazz) {
    env->DeleteGlobalRef(clazz);
    jMid_onMuxProgress = NULL;
}