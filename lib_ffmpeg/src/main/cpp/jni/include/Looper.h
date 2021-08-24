//
// Created by LN on 2021/8/24.
//

#ifndef FF_PLAYER_C_LOOPER_H
#define FF_PLAYER_C_LOOPER_H


#include <android/looper.h>
#include <string>

class Looper {
public:
    static Looper *GetInstance();

    ~Looper();

    void init();

    void send(const char *msg);

private:
    static Looper *g_MainLooper;

    Looper();

    ALooper *aLooper;
    int readPipe;
    int writePipe;
    pthread_mutex_t looper_mutex_;

    static int handle_message(int fd, int events, void *data);
};


#endif //FF_PLAYER_C_LOOPER_H
