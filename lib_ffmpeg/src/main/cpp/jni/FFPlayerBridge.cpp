//
// Created by LN on 2021/1/4.
//

#include <jni.h>
#include <android_log.h>
#include "FFPlayerBridge.h"
#include <android/native_window_jni.h>
#include <StateListener.h>
#include "PlayerResult.h"
#include "Player.h"

#ifdef __cplusplus
extern "C" {
#include "Muxer.h"
#endif
#ifdef __cplusplus
}
#endif

#include <pthread.h>

static jobject playerObject = NULL;
static jmethodID jMid_onStateChangeId = NULL;
static JavaVM *vm = NULL;
static int changeState = -1;

jint JNI_OnLoad(JavaVM *jvm, void *reserved) {
    JNIEnv *env = NULL;
    if (jvm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    vm = jvm;
    return JNI_VERSION_1_6;
}


void *ChangeState(void *s) {
    if (playerObject && vm) {
        JNIEnv *env = NULL;
        int ret = vm->AttachCurrentThread(&env, NULL);
        if (ret == 0 && env) {
            int iState = *(int *) s;
            env->CallVoidMethod(playerObject, jMid_onStateChangeId, iState);
        } else {
            LOGE("onStateChange() get jEnv error");
        }
        vm->DetachCurrentThread();
    }
    return NULL;
}


const void *onStateChange(PlayState state) {
    if (playerObject && vm) {
        pthread_t thread = NULL;
        changeState = state;
        pthread_create(&thread, NULL, ChangeState, &changeState);
        pthread_detach(pthread_self());
    }
    return NULL;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_setResource(JNIEnv *env, jobject thiz, jstring url) {
    char *inputUrl = (char *) env->GetStringUTFChars(url, 0);
    if (!inputUrl) {
        LOGE("fuck! input url is NULL");
        return PLAYER_RESULT_ERROR;
    }
    return SetResource(inputUrl);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_play(JNIEnv *env, jobject thiz) {
    return Play();
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_configPlayer(JNIEnv *env, jobject thiz, jstring store_dir,
                                                 jobject surface, int w, int h) {
    ANativeWindow *native_window = NULL;
    native_window = ANativeWindow_fromSurface(env, surface);
    if (!native_window) {
        LOGE("create native window failQ");
        return PLAYER_RESULT_ERROR;
    }
    SetStateChangeListener(reinterpret_cast<void (*)(PlayState)>(onStateChange));
    char *path = (char *) env->GetStringUTFChars(store_dir, 0);
    return Configure(path, native_window, w, h);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_onSurfaceChange(JNIEnv *env, jobject thiz, jobject surface,
                                                    jint w, jint h) {

    ANativeWindow *native_window = NULL;
    native_window = ANativeWindow_fromSurface(env, surface);
    if (!native_window) {
        LOGE("create native window failQ");
        return PLAYER_RESULT_ERROR;
    }
    return OnWindowChange(native_window, w, h);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_init(JNIEnv *env, jobject thiz, int isDebug) {
    if (!playerObject) {
        SetDebug(isDebug);
        playerObject = env->NewGlobalRef(thiz);
        jclass clazz = env->GetObjectClass(thiz);
        jMid_onStateChangeId = env->GetMethodID(clazz, "onPlayerStateChange", "(I)V");
    } else {
        return PLAYER_RESULT_ERROR;
    }

    return PLAYER_RESULT_OK;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_stop(JNIEnv *env, jobject thiz) {
    return Stop();
}



extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_pause(JNIEnv *env, jobject thiz) {
    return Pause(0);
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_setRecordState(JNIEnv *env, jobject thiz, jint state) {
    switch (state) {
        case 0:
            return StartRecord();
        case 1:
            return PauseRecord();
        case -1:
            return StopRecord();
    }
    return 0;
}
/**
 * RUN ON WORK THREAD
 */
extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_muxAV(JNIEnv *env, jobject thiz, jstring audio_file,
                                          jstring video_file, jstring out_file) {
    char *audioFile = (char *) env->GetStringUTFChars(audio_file, 0);
    char *videoFile = (char *) env->GetStringUTFChars(video_file, 0);
    char *outFile = (char *) env->GetStringUTFChars(out_file, 0);
    if (audioFile && videoFile && outFile) {
        return static_cast<jint>(MuxAVFile(audioFile, videoFile, outFile));
    } else {
        return PLAYER_RESULT_ERROR;
    }
}

