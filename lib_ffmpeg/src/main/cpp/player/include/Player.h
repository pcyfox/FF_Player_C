//
// Created by LN on 2021/1/4.
//

#include <android/native_window_jni.h>
#include "StateListener.h"

#ifndef PLAYER_PLAYER_H
#define PLAYER_PLAYER_H


static bool LOG_DEBUG = false;

void SetDebug(bool isDebug);

int save(char *url, char *dest);

int SetResource(char *resource);

int Configure(char *dest, ANativeWindow *window, int w, int h);

int OnWindowChange(ANativeWindow *window, int w, int h);

int Play();

int Pause(int delay);

int Resume();

int Stop();

void SetStateChangeListener(void (*listener)(PlayState));

int PauseRecord();

int StopRecord();

int StartRecord();

int PauseRecord();

int ResumeRecord();

int Release();

#endif



