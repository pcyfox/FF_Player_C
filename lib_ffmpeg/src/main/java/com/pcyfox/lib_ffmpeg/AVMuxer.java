package com.pcyfox.lib_ffmpeg;

import android.util.Log;

import androidx.annotation.Keep;

import java.io.File;

@Keep
public class AVMuxer {
    static {
        init();
    }

    private static final String TAG = "AVMuxer";

    private static OnMuxProgressListener onMuxProgressListener;

    /**
     * called by jni !
     *
     * @param progress
     */
    public static void onMuxProgress(float progress) {
        if (onMuxProgressListener != null) {
            onMuxProgressListener.onProgress(progress);
        }
        if (BuildConfig.DEBUG&&(int) (progress * 100) % 10 == 0) {
            Log.d(TAG, "onMuxProgress() called with: progress = [" + progress * 100 + "]");
        }
    }


    public static int mux(String audioFile, String videoFile, String outFile, OnMuxProgressListener listener) {
        onMuxProgressListener = listener;
        Log.d(TAG, "mux() called with: audioFile = [" + audioFile + "], videoFile = [" + videoFile + "], outFile = [" + outFile + "]");
        File video = new File(videoFile);
        if (!video.exists()) {
            Log.e(TAG, "mux fail: video not exist!");
            return -1;
        }
        File audio = new File(audioFile);
        if (!audio.exists()) {
            Log.e(TAG, "mux fail: audio not exist!");
            return -1;
        }

        return muxAV(audioFile, videoFile, outFile);
    }


    public static void release(String tag) {
        onMuxProgressListener = null;
    }


    private static native void init();

    private static native void release();

    private static native int muxAV(String audioFile, String videoFile, String outFile);


}
