/*
 * Copyright 2018 Dmitriy Ponomarenko
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.pcyfox.module_recoder.audio;

import android.media.MediaRecorder;
import android.os.Build;
import android.util.Log;


import java.io.File;
import java.io.IOException;
import java.util.Timer;
import java.util.TimerTask;

import static com.pcyfox.module_recoder.audio.MediaConstants.VISUALIZATION_INTERVAL;


public class AudioRecorder implements RecorderContract.Recorder {
    private static final String TAG = "AudioRecorder";
    private MediaRecorder recorder = null;
    private File recordFile = null;
    private boolean isPrepared = false;
    private boolean isRecording = false;
    private boolean isPaused = false;
    private Timer timerProgress;
    private long progress = 0;

    private RecorderContract.RecorderCallback recorderCallback;

    private static class RecorderSingletonHolder {
        private static final AudioRecorder singleton = new AudioRecorder();

        public static AudioRecorder getSingleton() {
            return RecorderSingletonHolder.singleton;
        }
    }

    public static AudioRecorder getInstance() {
        return RecorderSingletonHolder.getSingleton();
    }

    private AudioRecorder() {
    }

    @Override
    public void setRecorderCallback(RecorderContract.RecorderCallback callback) {
        this.recorderCallback = callback;
    }

    @Override
    public void prepare(String outputFile, int channelCount, int sampleRate, int bitrate) {
        Log.d(TAG, "prepare() called with: outputFile = [" + outputFile + "], channelCount = [" + channelCount + "], sampleRate = [" + sampleRate + "], bitrate = [" + bitrate + "]");
        recordFile = new File(outputFile);
        if (!recordFile.exists()) {
            if (recorderCallback != null) {
                recorderCallback.onError(new IllegalArgumentException("can't open file! "));
            }
            return;
        }
        recorder = new MediaRecorder();
        recorder.reset();
        recorder.setAudioSource(MediaRecorder.AudioSource.MIC);
        recorder.setOutputFormat(MediaRecorder.OutputFormat.AAC_ADTS);
        recorder.setAudioEncoder(MediaRecorder.AudioEncoder.AAC);
        recorder.setAudioChannels(channelCount);
        recorder.setAudioSamplingRate(sampleRate);
        recorder.setAudioEncodingBitRate(bitrate);
        recorder.setMaxDuration(-1); //Duration unlimited or use RECORD_MAX_DURATION
        recorder.setOutputFile(recordFile.getAbsolutePath());
        try {
            recorder.prepare();
            isPrepared = true;
            if (recorderCallback != null) {
                recorderCallback.onPrepareRecord();
            }
        } catch (IOException | IllegalStateException e) {
            if (recorderCallback != null) {
                recorderCallback.onError(e);
            }
        }
    }

    @Override
    public void startRecording() {
        Log.d(TAG, "startRecording() called");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && isPaused) {
            try {
                recorder.resume();
                startRecordingTimer();
                if (recorderCallback != null) {
                    recorderCallback.onStartRecord(recordFile);
                }
                isPaused = false;
            } catch (IllegalStateException e) {
                if (recorderCallback != null) {
                    recorderCallback.onError(e);
                }
            }
        } else {
            if (isPrepared) {
                try {
                    recorder.start();
                    isRecording = true;
                    startRecordingTimer();
                    if (recorderCallback != null) {
                        recorderCallback.onStartRecord(recordFile);
                    }
                } catch (RuntimeException e) {
                    if (recorderCallback != null) {
                        recorderCallback.onError(e);
                    }
                }
            } else {
                Log.e(TAG, "Recorder is not PREPARED!!!");
            }
            isPaused = false;
        }
    }

    public boolean isPrepared() {
        return isPrepared;
    }

    @Override
    public void pauseRecording() {
        Log.d(TAG, "pauseRecording() called");
        if (isRecording) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                try {
                    recorder.pause();
                    pauseRecordingTimer();
                    if (recorderCallback != null) {
                        recorderCallback.onPauseRecord();
                    }
                    isPaused = true;
                } catch (IllegalStateException e) {
                    if (recorderCallback != null) {
                        recorderCallback.onError(e);
                    }
                }
            } else {
                stopRecording();
            }
        }
    }

    @Override
    public void stopRecording() {
        Log.d(TAG, "stopRecording() called");
        if (isRecording) {
            stopRecordingTimer();
            try {
                recorder.stop();
                if (recorderCallback != null) {
                    recorderCallback.onStopRecord(recordFile);
                }
                recorder.release();
            } catch (RuntimeException e) {
                e.printStackTrace();
            }
            recordFile = null;
            isPrepared = false;
            isRecording = false;
            isPaused = false;
            recorder = null;
        } else {
            Log.e(TAG, "Recording has already STOPPED or hasn't STARTED");
        }
    }

    private void startRecordingTimer() {
        timerProgress = new Timer();
        timerProgress.schedule(new TimerTask() {
            @Override
            public void run() {
                if (recorderCallback != null && recorder != null) {
                    try {
                        recorderCallback.onRecordProgress(progress, recorder.getMaxAmplitude());
                    } catch (IllegalStateException e) {
                        e.printStackTrace();
                    }
                    progress += VISUALIZATION_INTERVAL;
                }
            }
        }, 0, VISUALIZATION_INTERVAL);
    }

    private void stopRecordingTimer() {
        timerProgress.cancel();
        timerProgress.purge();
        progress = 0;
    }

    private void pauseRecordingTimer() {
        timerProgress.cancel();
        timerProgress.purge();
    }

    @Override
    public boolean isRecording() {
        return isRecording;
    }

    @Override
    public boolean isPaused() {
        return isPaused;
    }
}
