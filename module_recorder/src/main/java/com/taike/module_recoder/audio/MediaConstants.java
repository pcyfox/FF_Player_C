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

package com.taike.module_recoder.audio;

/**
 * AppConstants that may be used in multiple classes.
 */
public class MediaConstants {

    private MediaConstants() {
    }
    public static final int SHORT_RECORD_DP_PER_SECOND = 25;

    //采样率
    public static final int PLAYBACK_SAMPLE_RATE = 44100;
    public static final int RECORD_SAMPLE_RATE_44100 = 44100;
    public static final int RECORD_SAMPLE_RATE_8000 = 8000;
    public static final int RECORD_SAMPLE_RATE_16000 = 16000;
    public static final int RECORD_SAMPLE_RATE_22050 = 22050;
    public static final int RECORD_SAMPLE_RATE_32000 = 32000;
    public static final int RECORD_SAMPLE_RATE_48000 = 48000;

    //比特率
    public static final int RECORD_ENCODING_BITRATE_12000 = 12000;
    public static final int RECORD_ENCODING_BITRATE_24000 = 24000;
    public static final int RECORD_ENCODING_BITRATE_48000 = 48000;
    public static final int RECORD_ENCODING_BITRATE_96000 = 96000;
    public static final int RECORD_ENCODING_BITRATE_128000 = 128000;
    public static final int RECORD_ENCODING_BITRATE_192000 = 192000;
    public static final int RECORD_ENCODING_BITRATE_256000 = 256000;

    //声道
    public final static int RECORD_AUDIO_MONO = 1;
    public final static int RECORD_AUDIO_STEREO = 2;

    public static final int DEFAULT_RECORD_SAMPLE_RATE = RECORD_SAMPLE_RATE_44100;
    public static final int DEFAULT_RECORD_ENCODING_BITRATE = RECORD_ENCODING_BITRATE_96000;
    public static final int DEFAULT_CHANNEL_COUNT = RECORD_AUDIO_STEREO;

	/** Time interval for Recording progress visualisation. */
	public final static int VISUALIZATION_INTERVAL = 1000/SHORT_RECORD_DP_PER_SECOND; //1000 mills/25 dp per sec

	public final static int RECORD_BYTES_PER_SECOND = RECORD_ENCODING_BITRATE_48000 /8; //bits per sec converted to bytes per sec.

}
