//
// Created by Dwayne on 20/11/24.
//


#pragma once
#ifndef AUDIO_PRACTICE_QUEUE_H
#define AUDIO_PRACTICE_QUEUE_H

#ifndef _COMMON
#define _COMMON

#include "queue"
#include "pthread.h"
#include "Player.h"
#include "PlayerResult.h"
#include <../include/android_log.h>

#ifdef __cplusplus
extern "C" {
#include "libavcodec/avcodec.h"
#endif
#ifdef __cplusplus
}
#endif
#endif //AUDIO_PRACTICE_QUEUE_Hyy

template<class T>


class AsyncQueue {
public:
    std::queue<T *> queuePacket;
    pthread_mutex_t mutexPacket;
    pthread_cond_t condPacket;
    volatile bool quit = false;

public:
    AsyncQueue();

    ~AsyncQueue();

    int putAvPacket(T *packet);

    int getAvPacket(T *packet);

    int getQueueSize();

    void clearAVPacket();

    void noticeQueue();

};

template<class _TYPE>
AsyncQueue<_TYPE>::AsyncQueue() {
    pthread_mutex_init(&mutexPacket, NULL);
    pthread_cond_init(&condPacket, NULL);
}

template<class _TYPE>
AsyncQueue<_TYPE>::~AsyncQueue() {
    quit = true;
    clearAVPacket();
    pthread_mutex_destroy(&mutexPacket);
    pthread_cond_destroy(&condPacket);
    LOGW("AsyncQueue Deleted");
}


template<class _TYPE>
int AsyncQueue<_TYPE>::getQueueSize() {
    int size = 0;
    pthread_mutex_lock(&mutexPacket);
    size = queuePacket.size();
    pthread_mutex_unlock(&mutexPacket);
    return size;
}

template<class _TYPE>
void AsyncQueue<_TYPE>::clearAVPacket() {
    pthread_mutex_lock(&mutexPacket);
    pthread_cond_signal(&condPacket);
    while (!queuePacket.empty()) {
        AVPacket *avPacket = queuePacket.front();
        queuePacket.pop();
        av_packet_free(&avPacket);
        av_free(avPacket);
        avPacket = NULL;
    }
    LOGD("AsyncQueue clear over!");
    pthread_cond_signal(&condPacket);
    pthread_mutex_unlock(&mutexPacket);
}

template<class _TYPE>
void AsyncQueue<_TYPE>::noticeQueue() {
    pthread_cond_signal(&condPacket);
}

template<class _TYPE>
int AsyncQueue<_TYPE>::putAvPacket(_TYPE *packet) {
    pthread_mutex_lock(&mutexPacket);
    if (queuePacket.size() > 160) {
        LOGW("queue size is too large,start to clean!");
        while (!queuePacket.empty()) {
            AVPacket *avPacket = queuePacket.front();
            queuePacket.pop();
            av_packet_free(&avPacket);
            av_free(avPacket);
            avPacket = NULL;
        }
    }
    queuePacket.push(packet);
    pthread_cond_signal(&condPacket);
    pthread_mutex_unlock(&mutexPacket);
    return PLAYER_RESULT_OK;
}

template<class _TYPE>
int AsyncQueue<_TYPE>::getAvPacket(_TYPE *packet) {
    pthread_mutex_lock(&mutexPacket);
/*
    while (!quit) {
        if (queuePacket.size() > 0) {
            AVPacket *avPacket = queuePacket.front();
            if (av_packet_ref(packet, avPacket) == 0) {
                queuePacket.pop();
            }
            av_packet_unref(avPacket);
            av_packet_free(&avPacket);
            avPacket = NULL;
            break;
        } else {
            LOGW("AsyncQueue cond wait..");
            pthread_cond_wait(&condPacket, &mutexPacket);
            LOGW("AsyncQueue cond wait exit");
        }
    }
*/

    if (queuePacket.size() > 0) {
        AVPacket *avPacket = queuePacket.front();
        if (av_packet_ref(packet, avPacket) == 0) {
            queuePacket.pop();
        }
        av_packet_unref(avPacket);
        av_packet_free(&avPacket);
        avPacket = NULL;
        pthread_mutex_unlock(&mutexPacket);
        return PLAYER_RESULT_OK;
    } else {
        pthread_mutex_unlock(&mutexPacket);
        return PLAYER_RESULT_ERROR;
    }
}


#endif