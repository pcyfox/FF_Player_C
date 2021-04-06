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

int PrepareRecorder(char *outPath);

int Configure(ANativeWindow *window, int w, int h);

int OnWindowChange(ANativeWindow *window, int w, int h);

int OnWindowDestroy(ANativeWindow *window);

int Play(void);

int Pause(int delay);

int Resume(void);

int Stop(void);

void SetStateChangeListener(void (*listener)(PlayState));

int PauseRecord(void);

int StopRecord(void);

int StartRecord(void);

int PauseRecord(void);

int ResumeRecord(void);

int Release(void);

#endif



