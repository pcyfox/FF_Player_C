//
// Created by LN on 2021/8/24.
//

#include "include/Looper.h"
#include <fcntl.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#include "android_log.h"
#endif
#ifdef __cplusplus
}
#endif
#define LOOPER_MSG_LENGTH 81

Looper *Looper::g_MainLooper = NULL;

Looper *Looper::GetInstance() {
    if (!g_MainLooper) {
        g_MainLooper = new Looper();
    }
    return g_MainLooper;
}

Looper::Looper() {
    pthread_mutex_init(&looper_mutex_, NULL);
}

Looper::~Looper() {
    if (aLooper && readPipe != -1) {
        ALooper_removeFd(aLooper, readPipe);
    }
    if (readPipe != -1) {
        close(readPipe);
    }
    if (writePipe != -1) {
        close(writePipe);
    }
    pthread_mutex_destroy(&looper_mutex_);
}

void Looper::init() {
    int msgPipe[2];
    pipe(msgPipe);
    readPipe = msgPipe[0];
    writePipe = msgPipe[1];
    aLooper = ALooper_prepare(0);
    int ret = ALooper_addFd(aLooper, readPipe, 1, ALOOPER_EVENT_INPUT, Looper::handle_message,
                            NULL);
    if (ret < 0) {
        LOGD("Looper init fail");
    }
}


void Looper::send(const char *msg) {
    pthread_mutex_lock(&looper_mutex_);
    write(writePipe, msg, strlen(msg));
    pthread_mutex_unlock(&looper_mutex_);
}

int Looper::handle_message(int fd, int events, void *data) {
    char buffer[LOOPER_MSG_LENGTH];
    memset(buffer, 0, LOOPER_MSG_LENGTH);
    read(fd, buffer, sizeof(buffer));
    LOGD("receive msg %s", buffer);
    return 1;
}