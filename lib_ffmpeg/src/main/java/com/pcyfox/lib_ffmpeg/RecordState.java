package com.pcyfox.lib_ffmpeg;

import androidx.annotation.Keep;

@Keep
public enum RecordState {
    UN_START_RECORD,
    RECORD_ERROR,
    RECORD_PREPARED,
    RECORD_START,
    RECORDING,
    RECORD_PAUSE,
    RECORD_RESUME,
    RECORD_STOP,
    RECORDER_RELEASE,
}
