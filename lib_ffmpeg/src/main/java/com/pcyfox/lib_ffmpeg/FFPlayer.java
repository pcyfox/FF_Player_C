package com.pcyfox.lib_ffmpeg;

import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
@Keep
public class FFPlayer {
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

    public FFPlayer() {
        init(BuildConfig.DEBUG ? 1 : 0);
    }

    public int startRecord() {
        setRecordState(0);
        return 1;
    }

    public int pauseRecord() {
        setRecordState(1);
        return 1;
    }

    public int stopRecord() {
        return setRecordState(-1);
    }

    //for call in native
    public void onPlayerStateChange(int state) {
        for (PlayState s : PlayState.values()) {
            if (state == s.ordinal()) {
                this.state = s;
                if (onStateChangeListener != null) {
                    onStateChangeListener.onStateChange(s);
                }
            }
        }
    }

    public int config(String storeDir, SurfaceView surfaceView, int w, int h) {
        configPlayer(storeDir, surfaceView.getHolder().getSurface(), w, h);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                Log.d(TAG, "surfaceCreated() called with: holder = [" + holder + "]");
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
                Log.d(TAG, "surfaceChanged() called with: holder = [" + holder + "], format = [" + format + "], width = [" + width + "], height = [" + height + "]");
                onSurfaceChange(holder.getSurface(), width, height);
            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
                pause();
                Log.d(TAG, "surfaceDestroyed() called with: holder = [" + holder + "]");

            }
        });
        return 1;
    }


    //-------------for native-------------------------
    public native int init(int isDebug);

    public native int configPlayer(String storeDir, Surface surface, int w, int h);

    public native int onSurfaceChange(Surface surface, int w, int h);

    public native int setResource(String url);

    public native int play();

    private native int setRecordState(int state);

    public native int stop();

    public native int pause();

    public native int muxAV(String audioFile, String videoFile, String outFile);
    //-------------for native-------------------------
}
