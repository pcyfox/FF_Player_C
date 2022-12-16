//
// Created by LN on 2021/1/4.
//

#include <jni.h>
#include <android/native_window_jni.h>
#include <StateListener.h>
#include "PlayerResult.h"
#include "Player.h"
#include <map>
#include <utility>
#include "VMLoader.h"


#ifdef __cplusplus
extern "C" {
#include "AndroidLog.h"
#endif
#ifdef __cplusplus
}
#endif

#include <pthread.h>
#include <algorithm>

std::map<int, Player *> playerCache;
static jclass clazz = NULL;


Player *findPlayer(int id) {
    auto iterator = playerCache.find(id);
    if (iterator != playerCache.end()) {
        return iterator->second;
    } else {
        return NULL;
    }
}

bool removePlayer(int id) {
    auto i = playerCache.find(id);
    if (i != playerCache.end()) {
        playerCache.erase(i);
        return true;
    } else {
        return false;
    }
}


const void *onPlayStateChange(PlayState state, int id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        LOGE("onStateChange() not found player with id=%d", id);
        return nullptr ;
    }
    if (player->jPlayer.jMid_onPlayStateChangeId == NULL) {
        return nullptr;
    }
    JNIEnv *env = NULL;
    int ret = vm->AttachCurrentThread(&env, NULL);
    if (ret == 0) {
        env->CallVoidMethod(player->jPlayer.jPlayerObject, player->jPlayer.jMid_onPlayStateChangeId,
                            state);
    } else {
        LOGE("onStateChange() get jEnv error");
    }
    return nullptr;
}


const void *onRecordStateChange(RecordState state, int id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        LOGE("onStateChange() not found player with id=%d", id);
        return nullptr;
    }
    if (player->jPlayer.jMid_onRecordStateChangeId == NULL) {
        return nullptr;
    }
    JNIEnv *env = NULL;
    int ret = vm->AttachCurrentThread(&env, NULL);
    if (ret == 0) {
        env->CallVoidMethod(player->jPlayer.jPlayerObject,
                            player->jPlayer.jMid_onRecordStateChangeId, state);
    } else {
        LOGE("onStateChange() get jEnv error");
    }
    return nullptr;
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_setResource(JNIEnv *env, jobject thiz, jstring url, int id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        LOGE("not found player by id=%d", id);
        return PLAYER_RESULT_ERROR;
    }
    char *inputUrl = (char *) env->GetStringUTFChars(url, 0);
    if (!inputUrl) {
        LOGE("fuck! input url is NULL");
        return PLAYER_RESULT_ERROR;
    }
    return player->SetResource(inputUrl);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_play(JNIEnv *env, jobject thiz, int id) {
    LOGI("play() called with:id=%d", id);
    Player *player = findPlayer(id);
    if (player == NULL) {
        LOGE("not found player by id=%d", id);
        return PLAYER_RESULT_ERROR;
    }
    return player->Play();
}



extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_start(JNIEnv *env, jobject thiz, int id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    return player->Start();
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_configPlayer(JNIEnv *env, jobject thiz,
                                                  jobject surface, int w, int h,
                                                  int isOnyRecorderMod, int id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    player->SetPlayStateChangeListener(reinterpret_cast<void (*)(PlayState, int) >
                                       (onPlayStateChange));

    player->SetRecordStateChangeListener(reinterpret_cast<void (*)(RecordState, int) >
                                         (onRecordStateChange));

    if (isOnyRecorderMod > 0) {
        return player->Configure(NULL, 0, 0, true);
    }
    ANativeWindow *native_window = NULL;
    native_window = ANativeWindow_fromSurface(env, surface);
    if (!native_window) {
        LOGE("create native window failQ");
        return PLAYER_RESULT_ERROR;
    }

    return player->Configure(native_window, w, h, false);
}



extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_onSurfaceChange(JNIEnv *env, jobject thiz, jobject surface,
                                                     jint w, jint h, int id) {

    Player *player = findPlayer(id);
    if (player == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    ANativeWindow *native_window = NULL;
    native_window = ANativeWindow_fromSurface(env, surface);
    if (!native_window) {
        LOGE("create native window failQ");
        return PLAYER_RESULT_ERROR;
    }
    return player->OnWindowChange(native_window, w, h);
}




extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_init(JNIEnv *env, jobject thiz, int isDebug, int id) {
    if (!vm) {
        env->GetJavaVM(&vm);
    }

    if (isDebug) {
        av_log_set_callback(ffmpeg_android_log_callback);
    } else {
        av_log_set_callback(NULL);
    }

    if (findPlayer(id) == NULL) {
        LOGI("createVideoCodec() called with: isDebug=%d,id=%d", isDebug, id);
        auto *player = new Player(id);
        Player::SetDebug(isDebug);
        playerCache.insert(std::map<int, Player *>::value_type(id, player));
        clazz = env->GetObjectClass(thiz);

        player->jPlayer.jPlayerObject = env->NewGlobalRef(thiz);
        player->jPlayer.jMid_onPlayStateChangeId = env->GetMethodID(clazz,
                                                                    "onPlayerStateChange",
                                                                    "(I)V");
        player->jPlayer.jMid_onRecordStateChangeId = env->GetMethodID(clazz,
                                                                      "onRecorderStateChange",
                                                                      "(I)V");
        return PLAYER_RESULT_OK;
    } else {
        return PLAYER_RESULT_ERROR;
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_stop(JNIEnv *env, jobject thiz, int id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    return player->Stop();
}



extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_pause(JNIEnv *env, jobject thiz, int id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    return player->Pause(0);
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_setRecordState(JNIEnv *env, jobject thiz, jint state, int id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    if (player->playerContext == NULL || player->playerContext->GetPlayState() == RELEASE) {
        return PLAYER_RESULT_ERROR;
    }
    switch (state) {
        case RECORD_START:
            return player->StartRecord();
        case RECORD_PAUSE:
            return player->PauseRecord();
        case RECORD_RESUME:
            return player->ResumeRecord();
        case RECORD_STOP:
            return player->StopRecord();
        default: {
            return -1;
        }
    }
}
/**
 * RUN ON WORK THREAD
 */
extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_resume(JNIEnv *env, jobject thiz, int id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    return player->Resume();
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_prepareRecorder(JNIEnv *env, jobject thiz,
                                                     jstring out_file_path, int id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    char *outFile = (char *) env->GetStringUTFChars(out_file_path, nullptr);
    return player->PrepareRecorder(outFile);
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_surfaceDestroyed(JNIEnv *env, jobject thiz, jobject holder,
                                                      int id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    return player->Pause(0);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_pcyfox_lib_1ffmpeg_FFPlayer_release(JNIEnv *env, jobject thiz, jint id) {
    Player *player = findPlayer(id);
    if (player == NULL) {
        return;
    }
    env->DeleteGlobalRef(player->jPlayer.jPlayerObject);
    removePlayer(id);
    player->Release();
}
