package com.pcyfox.lib_ffmpeg;

import androidx.annotation.Keep;

@Keep
public enum PlayState {
    UNINITIALIZED,
    INITIALIZED,
    PREPARED,
    EXECUTING,
    STARTED,
    PAUSE,
    STOPPED,
    ERROR,
    RELEASE,
}
