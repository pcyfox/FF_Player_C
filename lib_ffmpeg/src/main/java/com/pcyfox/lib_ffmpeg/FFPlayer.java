package com.pcyfox.lib_ffmpeg;

import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

@Keep
public class FFPlayer {
    private final int id;
    private static final String TAG = "FFPlayer";

    static {
        System.loadLibrary("ff_player");
    }

    private PlayState state;

    private OnPlayStateChangeListener onStateChangeListener;

    public void setOnStateChangeListener(OnPlayStateChangeListener onStateChangeListener) {
        this.onStateChangeListener = onStateChangeListener;
    }

    public PlayState getState() {
        return state;
    }

    public int getId() {
        return id;
    }

    public FFPlayer(int id) {
        this.id = id;
        init(BuildConfig.DEBUG ? 1 : 0, id);
    }

    public int startRecord() {
        return setRecordState(0, id);
    }

    public int pauseRecord() {
        return setRecordState(1, id);
    }

    public int resumeRecord() {
        return setRecordState(2, id);
    }

    public int stopRecord() {
        return setRecordState(-1, id);
    }

    //for call in native
    public void onPlayerStateChange(int state) {
        Log.d(TAG, "onPlayerStateChange() called with: state = [" + state + "]");
        for (PlayState s : PlayState.values()) {
            if (state == s.ordinal()) {
                this.state = s;
                if (onStateChangeListener != null) {
                    onStateChangeListener.onStateChange(s);
                }
            }
        }
    }


    public int config(SurfaceView surfaceView, int w, int h, boolean isOnlyRecord) {
        configPlayer(surfaceView.getHolder().getSurface(), w, h, isOnlyRecord ? 1 : -1, id);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                Log.d(TAG, "surfaceCreated() called with: holder = [" + holder + "]");
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
                Log.d(TAG, "surfaceChanged() called with: holder = [" + holder + "], format = [" + format + "], width = [" + width + "], height = [" + height + "]");
                onSurfaceChange(holder.getSurface(), width, height, id);
            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
                pause(id);
                Log.d(TAG, "surfaceDestroyed() called with: holder = [" + holder + "]");

            }
        });
        return 1;
    }

    public int prepareRecorder(String outFilePath) {
        return prepareRecorder(outFilePath, id);
    }

    public int setResource(String url) {
        return setResource(url,id);
    }

    public int play() {
        return play(id);
    }

    public int start() {
        return start(id);
    }


    public int stop() {
        return stop(id);
    }

    public int pause() {
        return pause(id);
    }

    public int resume() {
        return resume(id);
    }

    //-------------for native-------------------------
    public native int init(int isDebug, int id);

    public native int configPlayer(Surface surface, int w, int h, int isOnlyRecorderMod, int id);

    public native int onSurfaceChange(Surface surface, int w, int h, int id);

    public native int surfaceDestroyed(SurfaceHolder holder, int id);

    public native int setResource(String url, int id);

    public native int start(int id);

    public native int play(int id);

    public native int prepareRecorder(String outFilePath, int id);

    private native int setRecordState(int state, int id);

    public native int stop(int id);

    public native int pause(int id);

    public native int resume(int id);

    public native int muxAV(String audioFile, String videoFile, String outFile);
    //-------------for native-------------------------
}
