package com.pcyfox.lib_ffmpeg;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;


@Keep
public class FFPlayer {
    private final int id;
    private static final String TAG = "FFPlayer";

    static {
        System.loadLibrary("ff_player");
    }

    private SurfaceView surfaceView;
    private int w = 0;
    private int h = 0;

    private PlayState playState;
    private RecordState recodeState;

    private OnPlayStateChangeListener onPlayStateChangeListener;
    private OnRecordStateChangeListener onRecordStateChangeListener;

    public void setOnPlayStateChangeListener(OnPlayStateChangeListener onPlayStateChangeListener) {
        this.onPlayStateChangeListener = onPlayStateChangeListener;
    }

    public void setOnRecordStateChangeListener(OnRecordStateChangeListener onRecordStateChangeListener) {
        this.onRecordStateChangeListener = onRecordStateChangeListener;
    }


    public PlayState getPlayState() {
        return playState;
    }

    public RecordState getRecodeState() {
        return recodeState;
    }

    public int getId() {
        return id;
    }

    public FFPlayer(int id) {
        this.id = id;
        init(BuildConfig.DEBUG ? 1 : 0, id);
    }


    public FFPlayer(int id, boolean debug) {
        this.id = id;
        init(debug ? 1 : 0, id);
        if (BuildConfig.DEBUG) {
            List<String> codecIndoList = new ArrayList<>();
            int numCodecs = MediaCodecList.getCodecCount();
            for (int i = 0; i < numCodecs; i++) {
                MediaCodecInfo codecInfo = MediaCodecList.getCodecInfoAt(i);
                String[] types = codecInfo.getSupportedTypes();
                for (String type : types) {
                    codecIndoList.add("name:" + codecInfo.getName() + ",type:" + type + "\n");
                }
            }
            Log.d(TAG, "FFPlayer() MediaCodecList=" + Arrays.toString(codecIndoList.toArray()));
        }
    }

    public int startRecord() {
        return setRecordState(RecordState.RECORD_START.ordinal(), id);
    }

    public int pauseRecord() {
        return setRecordState(RecordState.RECORD_PAUSE.ordinal(), id);
    }

    public int resumeRecord() {
        return setRecordState(RecordState.RECORD_RESUME.ordinal(), id);
    }

    public int stopRecord() {
        return setRecordState(RecordState.RECORD_STOP.ordinal(), id);
    }

    //for call in native
    public void onPlayerStateChange(int state) {
        Log.d(TAG, "onPlayerStateChange() called with: state = [" + state + "]");
        for (PlayState s : PlayState.values()) {
            if (state == s.ordinal()) {
                this.playState = s;
                if (onPlayStateChangeListener != null) {
                    onPlayStateChangeListener.onStateChange(s);
                }
            }
        }
    }


    public void onRecorderStateChange(int state) {
        Log.d(TAG, "onRecorderStateChange() called with: state = [" + state + "]");
        for (RecordState s : RecordState.values()) {
            if (state == s.ordinal()) {
                recodeState = s;
                if (onRecordStateChangeListener != null) {
                    onRecordStateChangeListener.onStateChange(s);
                }
            }
        }
    }


    public int onlyRecord() {
        Log.d(TAG, "onlyRecord() called with: id = [" + id + "]");
        return configPlayer(null, 0, 0, 1, id);
    }


    public int config(SurfaceView surfaceView, int w, int h, boolean isOnlyRecord) {
        Log.d(TAG, "configVideo() called with: surfaceView = [" + surfaceView + "], w = [" + w + "], h = [" + h + "], isOnlyRecord = [" + isOnlyRecord + "]");
        if (isOnlyRecord) {
            return onlyRecord();
        }
        this.surfaceView = surfaceView;
        this.w = w;
        this.h = h;

        if (surfaceView == null || w * h == 0) {
            Log.e(TAG, "configVideo() called with: surfaceView = [" + surfaceView + "], w = [" + w + "], h = [" + h + "], isOnlyRecord = [" + isOnlyRecord + "]");
            return -1;
        }
        configPlayer(surfaceView.getHolder().getSurface(), w, h, 0, id);

        SurfaceHolder.Callback callback = new SurfaceHolder.Callback() {
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
        };
        surfaceView.getHolder().addCallback(callback);
        surfaceView.setTag(callback);
        return 1;
    }

    public int prepareRecorder(String outFilePath) {
        Log.d(TAG, "prepareRecorder() called with: outFilePath = [" + outFilePath + "]");
        return prepareRecorder(outFilePath, id);
    }

    public int setResource(String url) {
        return setResource(url, id);
    }

    public int play() {
        return play(id);
    }

    public int start() {
        if (playState == PlayState.STARTED) {
            return -1;
        }
        if (playState == PlayState.EXECUTING) {
            return -1;
        }
        return start(id);
    }


    public int stop() {
        if (playState == PlayState.STOPPED) {
            return -1;
        }
        return stop(id);
    }

    public void release() {
        release(id);
        onRecordStateChangeListener = null;
        onPlayStateChangeListener = null;
        playState = PlayState.RELEASE;

        if (surfaceView != null) {
            if (surfaceView.getTag() instanceof SurfaceHolder.Callback) {
                surfaceView.getHolder().removeCallback((SurfaceHolder.Callback) surfaceView.getTag());
            }
            surfaceView.getHolder().getSurface().release();
            surfaceView.removeCallbacks(() -> {
            });
            surfaceView = null;
        }

    }

    public int pause() {
        return pause(id);
    }

    public int resume() {
        return resume(id);
    }


    public void resumeWindow() {
        if (surfaceView != null && w * h > 0) {
            onSurfaceChange(surfaceView.getHolder().getSurface(), w, h, id);
        }
    }


    //-------------for native-------------------------
    public native int init(int isDebug, int id);

    public native void release(int id);

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


    //-------------for native-------------------------
}
