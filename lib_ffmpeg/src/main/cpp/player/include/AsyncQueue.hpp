//
// Created by Dwayne on 20/11/24.
//

#ifndef AUDIO_PRACTICE_QUEUE_H
#define AUDIO_PRACTICE_QUEUE_H

#include "queue"
#include "pthread.h"
#include "PlayerResult.h"

#ifdef __cplusplus
extern "C" {
#include "libavcodec/avcodec.h"
#endif

#ifdef __cplusplus
}
#endif


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

    int getAvPacket(T **packet);

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
    //LOGW("AsyncQueue Deleted");
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
    pthread_cond_signal(&condPacket);
    pthread_mutex_unlock(&mutexPacket);
}

template<class _TYPE>
void AsyncQueue<_TYPE>::noticeQueue() {
    pthread_cond_signal(&condPacket);
}

template<class _TYPE>
int AsyncQueue<_TYPE>::putAvPacket(_TYPE *packet) {
    if (quit) { return PLAYER_RESULT_ERROR; }
    pthread_mutex_lock(&mutexPacket);
    if (queuePacket.size() > 260) {
        LOGE("AsyncQueue size is to large!");
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
int AsyncQueue<_TYPE>::getAvPacket(_TYPE **packet) {
    if (quit) { return PLAYER_RESULT_ERROR; }
    pthread_mutex_lock(&mutexPacket);
    if (queuePacket.size() > 0) {
        *packet = queuePacket.front();
        queuePacket.pop();
        pthread_mutex_unlock(&mutexPacket);
        return PLAYER_RESULT_OK;
    } else {
        pthread_mutex_unlock(&mutexPacket);
        return PLAYER_RESULT_ERROR;
    }
}


#endif