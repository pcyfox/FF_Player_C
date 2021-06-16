package com.pcyfox.lib_ffmpeg;

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
