
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

#include "Player.h"
#include <PlayerResult.h>
#include<queue>
#include <pthread.h>
#include "Utils.h"

#ifdef __cplusplus
extern "C" {
#include"libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#endif
#ifdef __cplusplus
}
#endif

//clang -g -o pvlib  ParseVideo.c  -lavformat -lavutil
//clang -g -o pvlib  ParseVideo.c `pkg-config --libs libavutil libavformat`

static bool isDebug = true;


struct Info {
    PlayerInfo *playerInfo;
    RecorderInfo *recorderInfo;
};


AVStream *findVideoStream(AVFormatContext *i_fmt_ctx) {
    int index = av_find_best_stream(i_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (index == AVERROR_STREAM_NOT_FOUND) {
        LOGE("not find video stream!,streams number=%d", i_fmt_ctx->nb_streams);
        return NULL;
    } else {
        return i_fmt_ctx->streams[index];
    }
}

int GetNALUType(AVPacket *packet) {
    int nalu_type = -1;
    const uint8_t *buf = packet->data;
    bool hasLongStartCode = buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1;
    bool hasShortStartCode = buf[0] == 0 && buf[1] == 0 && buf[2] == 1;
    if (hasLongStartCode || hasShortStartCode) {
        if (hasShortStartCode) {
            nalu_type = buf[3] & 0xFF;
        } else {
            nalu_type = buf[4] & 0xFF;
        }
    }
    return nalu_type;
}


int createAMediaCodec(AMediaCodec **mMediaCodec, int width, int height, uint8_t *sps, int spsSize,
                      uint8_t *pps, int ppsSize,
                      ANativeWindow *surface, const char *mine) {

    LOGI("createAMediaCodec() called width=%d,height=%d,spsSize=%d,ppsSize=%d,mine=%s\n", width,
         height,
         spsSize, ppsSize, mine);

    if (width * height == 0) {
        LOGE("createAMediaCodec() not support video size");
        return PLAYER_RESULT_ERROR;
    }

    if (!*mMediaCodec) {
        AMediaCodec *mediaCodec = AMediaCodec_createDecoderByType(mine);
        if (!mediaCodec) {
            LOGE("createAMediaCodec() fail!");
            return PLAYER_RESULT_ERROR;
        } else {
            LOGI("createAMediaCodec() success!");
        }
        *mMediaCodec = mediaCodec;
    } else {
        AMediaCodec_flush(*mMediaCodec);
        AMediaCodec_stop(*mMediaCodec);
    }

    AMediaFormat *videoFormat = AMediaFormat_new();
    AMediaFormat_setString(videoFormat, "mime", mine);
    AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_WIDTH, width); // 视频宽度
    AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_HEIGHT, height); // 视频高度

    if (spsSize && sps) {
        AMediaFormat_setBuffer(videoFormat, "csd-0", sps, spsSize); // sps
    }
    if (ppsSize && pps) {
        AMediaFormat_setBuffer(videoFormat, "csd-1", pps, ppsSize); // pps
    }

    media_status_t status = AMediaCodec_configure(*mMediaCodec, videoFormat, surface, NULL, 0);
    if (status != AMEDIA_OK) {
        LOGE("configure AMediaCodec fail!,ret=%d", status);
        AMediaCodec_delete(*mMediaCodec);
        mMediaCodec = NULL;
        return PLAYER_RESULT_ERROR;
    } else {
        LOGD("configure AMediaCodec success!");
    }
    return PLAYER_RESULT_OK;
}


void *OpenResource(void *info) {
    if (info == NULL) {
        return NULL;
    }
    auto *playerInfo = (PlayerInfo *) info;
    char *url = playerInfo->resource;
    if (!playerInfo->inputContext) {
        playerInfo->inputContext = avformat_alloc_context();
    }
    AVFormatContext *fmt_ctx = playerInfo->inputContext;
    //打开多媒体文件，根据文件后缀名解析,第三个参数是显式制定文件类型，当文件后缀与文件格式不符
    //或者根本没有后缀时需要填写，
    LOGD("OpenResource():avformat_open_input start,url=%s", url);
    int ret = avformat_open_input(&fmt_ctx, url, NULL, NULL);
    if (ret == 0) {
        /* find stream info  */
        LOGD("----------open resource success!,start to find stream info-------------");
        if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
            LOGE("could not find stream info\n");
            fmt_ctx = NULL;
            return (void *) PLAYER_RESULT_ERROR;
        }

        LOGI("----------find stream info success!-------------");
        playerInfo->resource = url;

        LOGD("----------fmt_ctx:streams number=%d,bit rate=%lld\n", fmt_ctx->nb_streams,
             fmt_ctx->bit_rate);

        //输出多媒体文件信息,第二个参数是流的索引值（默认0），第三个参数，0:输入流，1:输出流
        //    av_dump_format(fmt_ctx, 0, url, 0);

        playerInfo->inputVideoStream = findVideoStream(playerInfo->inputContext);
        if (!playerInfo->inputVideoStream) {
            playerInfo->SetPlayState(ERROR);
            return (void *) PLAYER_RESULT_ERROR;
        }

        //is H264?
        if (playerInfo->inputVideoStream->codecpar->codec_id != AV_CODEC_ID_H264) {
            LOGE("sorry this player only support h.264 now!");
            playerInfo->SetPlayState(ERROR);
            return (void *) PLAYER_RESULT_ERROR;
        }

        AVCodecParameters *codecpar = NULL;
        codecpar = playerInfo->inputVideoStream->codecpar;

        if (isDebug) {
            uint8_t exSize = codecpar->extradata_size;
            if (exSize > 0) {
                uint8_t *extraData = codecpar->extradata;
                printCharsHex((char *) extraData, exSize, exSize - 1, "SPS-PPS");
            }
            int w = playerInfo->inputVideoStream->codecpar->width;
            int h = playerInfo->inputVideoStream->codecpar->height;
            int level = playerInfo->inputVideoStream->codecpar->level;
            int profile = playerInfo->inputVideoStream->codecpar->profile;
            int sample_rate = playerInfo->inputVideoStream->codecpar->sample_rate;
            const int codecId = playerInfo->inputVideoStream->codecpar->codec_id;
            LOGD("input video stream info :w=%d,h=%d,codec=%d,level=%d,profile=%d,sample_rate=%d",
                 w, h,
                 codecId, level, profile, sample_rate);

            int format = playerInfo->inputVideoStream->codecpar->format;
            switch (format) {
                case AV_PIX_FMT_YUV420P:
                    LOGD("input video color format is YUV420p");
                    break;
                case AV_PIX_FMT_YUYV422:   ///< packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr
                    LOGD("input video color format is YUVV422");
                    break;
                case AV_PIX_FMT_YUV422P:
                    LOGD("input video color format is YUV422P");
                    break;
                case AV_PIX_FMT_YUV444P:
                    LOGD("input video color format is YUV444P");
                    break;

                case AV_PIX_FMT_YUVJ420P:
                    LOGD("input video color format is YUVJ420P");
                    break;
                default:
                    LOGD("input video color format is other");
                    break;
            }
        }

        if (codecpar == NULL) {
            playerInfo->SetPlayState(ERROR);
            LOGE("can't get video stream params\n");
            return (void *) PLAYER_RESULT_ERROR;
        }

        if (!playerInfo->isOnlyRecorderNode) {
            if (playerInfo->window == NULL ||
                !playerInfo->windowHeight * playerInfo->windowHeight) {
                playerInfo->SetPlayState(ERROR);
                LOGE("configure error!");
                return (void *) PLAYER_RESULT_ERROR;
            }

            createAMediaCodec(&playerInfo->AMediaCodec, playerInfo->windowWith,
                              playerInfo->windowHeight,
                              codecpar->extradata,
                              codecpar->extradata_size,
                              codecpar->extradata,
                              codecpar->extradata_size,
                              playerInfo->window, playerInfo->mine);

            if (playerInfo->AMediaCodec) {
                LOGI("OpenResource()  createAMediaCodec success!,state->PREPARED");
                playerInfo->SetPlayState(PREPARED);
            } else {
                LOGE("AMediaCodec is NULL");
                playerInfo->SetPlayState(ERROR);
                return (void *) PLAYER_RESULT_ERROR;
            }
        } else {
            LOGI("OpenResource(): state->PREPARED");
            playerInfo->SetPlayState(PREPARED);
        }

    } else {
        fmt_ctx = NULL;
        LOGE("can't open source: %s msg:%s \n", url, av_err2str(ret));
        return (void *) PLAYER_RESULT_ERROR;
    }
    return (void *) PLAYER_RESULT_OK;
}

//抽取音频数据
int getAudioData(AVFormatContext *ctx, char *dest) {
    //创建一个可写的输出文件,用于存放输出数据
    FILE *dest_fd = fopen(dest, "a*");
    if (!dest_fd) {
        LOGE("open out file %s fail! \n", dest);
        return -1;
    }

    //找到音频流,第二参数流类型（FFMPEG宏）第三个流的索引号，未知，填-1，第四个，相关流的索引号，
    //如音频流对应的视频流索引号，也未知，第五个制定解解码器，最后一个是标志符解
    int index = av_find_best_stream(ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

    if (index < 0) {
        LOGE("find audio stream fail!");
        return -1;
    }
    //读取数据包（因历史原因，函数未改名字）
    AVPacket audio_pkt;
    av_init_packet(&audio_pkt);
    while (av_read_frame(ctx, &audio_pkt) >= 0) {
        if (audio_pkt.stream_index == index) {
            //TODO 每个音频包前加入头信息
            int len = fwrite(audio_pkt.data, 1, audio_pkt.size, dest_fd);

            if (len != audio_pkt.size) {//判断写入文件是否成功
                LOGE("the write data length not equals audio_pkt size \n");
            }
            av_packet_unref(&audio_pkt);
        }
    }
    return 0;
}


void *RecordPkt(void *info) {
    auto *recorderInfo = (RecorderInfo *) info;
    if (recorderInfo->GetRecordState() == RECORD_START) {
        char *file = recorderInfo->storeFile;
        /* create output context by file name*/
        avformat_alloc_output_context2(&(recorderInfo->o_fmt_ctx), NULL, NULL, file);
        if (!recorderInfo->o_fmt_ctx) {
            recorderInfo->SetRecordState(RECORD_ERROR);
            LOGE("avformat_alloc_output_context  error ,file=%s ", file);
            return (void *) PLAYER_RESULT_ERROR;
        }
        /*
        * since all input files are supposed to be identical (framerate, dimension, color format, ...)
        * we can safely set output codec values from first input file
        */
        AVCodecParameters *i_av_codec_parameters = recorderInfo->inputVideoStream->codecpar;

        recorderInfo->o_video_stream = avformat_new_stream(recorderInfo->o_fmt_ctx, NULL);
        if (!recorderInfo->o_video_stream) {
            return (void *) PLAYER_RESULT_ERROR;
        }

        recorderInfo->o_video_stream->codecpar->codec_tag = 0;
        int ret = avcodec_parameters_copy(recorderInfo->o_video_stream->codecpar,
                                          i_av_codec_parameters);
        recorderInfo->o_video_stream->codecpar->format = AV_PIX_FMT_YUV420P;
        if (ret < 0) {
            recorderInfo->SetRecordState(RECORD_ERROR);
            LOGE("Failed to copy codec parameters\n");
            return (void *) PLAYER_RESULT_ERROR;
        }
        /*open file  pb:AVIOContext*/
        if (avio_open(&recorderInfo->o_fmt_ctx->pb, file, AVIO_FLAG_WRITE) < 0) {
            LOGE("open file ERROR,file=%s", file);
            recorderInfo->SetRecordState(RECORD_ERROR);
            return (void *) PLAYER_RESULT_ERROR;
        }
        /*write file header*/
        if (avformat_write_header(recorderInfo->o_fmt_ctx, NULL) < 0) {
            LOGE("write file header ERROR");
            recorderInfo->SetRecordState(RECORD_ERROR);
            return (void *) PLAYER_RESULT_ERROR;
        }
    } else {
        LOGW("RecordPkt() called,recorder state is not RECORD_START ");
    }

    AVPacket *packet = av_packet_alloc();
    while (true) {
        RecordState recordState = recorderInfo->GetRecordState();
        if (recordState == RECORD_PAUSE) {
            continue;
        }
        if (recordState == RECORD_STOP) {
            /*write file trailer*/
            LOGD("--------- recordState changed to stop ---------");
            av_write_trailer(recorderInfo->o_fmt_ctx);
            avformat_free_context(recorderInfo->o_fmt_ctx);
            recorderInfo->o_fmt_ctx = NULL;
            LOGI("--------- record stop ,and write trailer over ---------");
            break;
        }
        int ret = recorderInfo->packetQueue.getAvPacket(packet);
        if (ret == PLAYER_RESULT_ERROR) {
            continue;
        }

        //等待关键帧或sps、pps的出现
        if (recorderInfo->GetRecordState() == RECORD_START) {
            int type = packet->flags;
            if (type == 0x65 || type == 0x67) {
                recorderInfo->SetRecordState(RECORDING);
                LOGI("-----------------real start recording--------------");
            } else {
                continue;
            }
        }
        if (recorderInfo->GetRecordState() == RECORDING) {
            //---------------write pkt data to file-----------
            /*write packet and auto free packet*/
            av_interleaved_write_frame(recorderInfo->o_fmt_ctx, packet);
            av_packet_unref(packet);
        }
    }
    LOGI("----------------- record work stop,start to delete recordInfo--------------");
    //释放资源
    //delete recorderInfo;
    //recorderInfo = NULL;
    return (void *) PLAYER_RESULT_OK;
}


void *Decode(void *info) {
    auto *playerInfo = (PlayerInfo *) info;
    AMediaCodec *codec = playerInfo->AMediaCodec;
    AVPacket *packet = av_packet_alloc();
    while (true) {
        if (playerInfo == NULL) {
            break;
        }
        PlayState state = playerInfo->GetPlayState();
        if (state == PAUSE) {
            continue;
        }
        if (state != STARTED) {
            break;
        }

        int ret = playerInfo->packetQueue.getAvPacket(packet);
        if (ret == PLAYER_RESULT_OK) {
            uint8_t *data = packet->data;
            int length = packet->size;
            // 获取buffer的索引
            ssize_t index = AMediaCodec_dequeueInputBuffer(codec, 500);
            if (index >= 0) {
                size_t out_size;
                uint8_t *inputBuf = AMediaCodec_getInputBuffer(codec, index, &out_size);
                if (inputBuf != NULL && length <= out_size) {
                    //clear buf
                    memset(inputBuf, 0, out_size);
                    // 将待解码的数据copy到硬件中
                    memcpy(inputBuf, data, length);
                    av_packet_unref(packet);
                    int64_t pts = packet->pts;
                    if (pts < 0 || !pts) {
                        pts = getCurrentTime();
                    }

                    media_status_t status = AMediaCodec_queueInputBuffer(codec, index, 0, length,
                                                                         pts, 0);
                    if (status != AMEDIA_OK) {
                        LOGE("queue input buffer error status=%d", status);
                    }
                }
            }

            AMediaCodecBufferInfo info;
            ssize_t status = AMediaCodec_dequeueOutputBuffer(codec, &info, 100);
            if (status >= 0) {
                AMediaCodec_releaseOutputBuffer(codec, status, info.size != 0);
                if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                    LOGE("video producer output EOS");
                }
            } else if (status == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
                LOGE("output buffers changed");
            } else if (status == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            } else if (status == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                LOGE("video no output buffer right now");
            } else {
                LOGE("unexpected info code: %zd", status);
            }
        }
    }
    LOGD("-------Decode over!---------");
    return NULL;
}


int ProcessPacket(AVPacket *packet, AVCodecParameters *codecpar, PlayerInfo *playerInfo,
                  RecorderInfo *recorderInfo) {
    //检查是否有起始码,如果有说明是标准的H.264数据
    if (packet->size < 5 || playerInfo == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    int type = GetNALUType(packet);
    //  LOGD("------ProcessPacket NALU type=%0x", type);
    packet->flags = type;
    if (type == 0x67 && playerInfo->lastNALUType == type) {
        LOGW("------ProcessPacket more than one SPS in this GOP");
        //   return PLAYER_RESULT_ERROR;
    }
    playerInfo->lastNALUType = type;
    if (recorderInfo != NULL) {
        RecordState state = recorderInfo->GetRecordState();
        if (state != RECORD_PREPARED && state != RECORD_PAUSE && state != RECORD_ERROR) {
            AVPacket *copyPkt = NULL;
            copyPkt = av_packet_clone(packet);
            if (copyPkt != NULL) {
                //加入录制队列
                recorderInfo->packetQueue.putAvPacket(copyPkt);
            } else {
                LOGE("ProcessPacket clone packet fail!");
            }
        }
    }
    if (playerInfo != NULL && playerInfo->GetPlayState() == STARTED &&
        !playerInfo->isOnlyRecorderNode) {
        //加入解码队列
        playerInfo->packetQueue.putAvPacket(packet);
    }
    return PLAYER_RESULT_OK;
}


void StartDecodeThread(PlayerInfo *playerInfo) {
    LOGI("StartDecodeThread() called");
    pthread_create(&playerInfo->decode_thread, NULL, Decode, playerInfo);
    pthread_setname_np(playerInfo->decode_thread, "decode_thread");
    pthread_detach(playerInfo->decode_thread);
}

void Player::StartRecorderThread() {
    if (playerInfo != NULL && recorderInfo != NULL) {
        recorderInfo->inputVideoStream = playerInfo->inputVideoStream;
    } else {
        LOGE("Start recorder thread fail!");
        return;
    }
    LOGI("Start recorder thread");
    pthread_create(&recorderInfo->recorder_thread, NULL, RecordPkt, recorderInfo);
    pthread_setname_np(recorderInfo->recorder_thread, "recorder_thread");
    pthread_detach(recorderInfo->recorder_thread);
}

void *DeMux(void *info) {
/*
    Info *inf = (Info *) info;
    PlayerInfo *playerInfo = inf->playerInfo;
    RecorderInfo *recorderInfo = inf->recorderInfo;
*/

    Player *player = (Player *) info;
    PlayerInfo *playerInfo = player->playerInfo;
    RecorderInfo *recorderInfo = player->recorderInfo;

    if (playerInfo == NULL) {
        LOGE("Player is not init");
        return NULL;
    }

    if (playerInfo->GetPlayState() == STARTED) {
        LOGE("DeMux()  called  player is STARTED");
        return NULL;
    }
    AVStream *i_video_stream = playerInfo->inputVideoStream;
    /*
    * since all input files are supposed to be identical (framerate, dimension, color format, ...)
    * we can safely set output codec values from first input file
    */
    playerInfo->SetPlayState(STARTED);
    LOGI("--------------------DeMux() change state to STARTED----------------------");

    if (!playerInfo->isOnlyRecorderNode) {
        StartDecodeThread(playerInfo);
    }
    AVCodecParameters *i_av_codec_parameters = i_video_stream->codecpar;
    int video_stream_index = i_video_stream->index;
    PlayState state;
    LOGI("--------------------Start DeMux----------------------");
    while (playerInfo != NULL && (state = playerInfo->GetPlayState()) != STOPPED ||
           state == PAUSE) {

        if (playerInfo->GetPlayState() == STOPPED) {
            LOGD("DeMux() stop,due to state-STOPPED!");
            break;
        }

        if (state == PAUSE && (recorderInfo != NULL) &&
            recorderInfo->GetRecordState() == RECORD_PAUSE) {
            continue;
        }
        AVPacket *i_pkt = NULL;
        i_pkt = av_packet_alloc();
        if (i_pkt == NULL) {
            LOGE("DeMux fail,because alloc av packet fail!");
            break;
        }
        int ret = av_read_frame(playerInfo->inputContext, i_pkt);
        if (playerInfo && ret == 0 &&
            i_pkt->stream_index == video_stream_index) {
            ProcessPacket(i_pkt, i_av_codec_parameters, playerInfo, recorderInfo);
        } else if (ret < 0) {
            LOGE("read stream of eof!");
            playerInfo->SetPlayState(STARTED);
            break;
        }
    }

    if (playerInfo->inputContext != NULL) {
        avformat_close_input(&playerInfo->inputContext);
    }
    LOGD("DeMux() stop over! ");
    static int num = 1;
    return NULL;
}


void Player::StartDeMuxThread() {
    LOGI("start deMux thread");

//    Info *info = (Info *) malloc(sizeof(struct Info));
//    info->playerInfo = playerInfo;
//    info->recorderInfo = recorderInfo;

    pthread_create(&playerInfo->deMux_thread, NULL, DeMux, this);
    pthread_setname_np(playerInfo->deMux_thread, "deMux_thread");
    pthread_detach(playerInfo->deMux_thread);
}

void Player::StartOpenResourceThread(char *res) {
    if (!res) {
        LOGE("StartOpenResourceThread() fail,res is null");
        return;
    }

    LOGD("start open resource thread");
    playerInfo->resource = res;
    pthread_create(&playerInfo
            ->open_resource_thread, NULL, OpenResource, playerInfo);
    pthread_setname_np(playerInfo
                               ->open_resource_thread, "open_resource_thread");
    pthread_detach(playerInfo
                           ->open_resource_thread);
}

int Player::InitPlayerInfo() {
    LOGD("InitPlayerInfo() called");
    if (!playerInfo) {
        playerInfo = new PlayerInfo;
        playerInfo->id = playerId;
    } else {
        LOGW("InitPlayerInfo(),playerInfo is not NULL,it may be inited!");
    }

    if (playerInfo->GetPlayState() == INITIALIZED) {
        return PLAYER_RESULT_OK;
    }

    LOGI("InitPlayerInfo():state-> INITIALIZED");
    playerInfo->SetPlayState(INITIALIZED);
    LOGD("init player info over");
    return PLAYER_RESULT_OK;
}


int Player::SetResource(char *resource) {
    LOGI("---------SetResource() called with:resource=%s\n", resource);
    if (playerInfo != NULL) {
        LOGE("player is not stopped!");
    }

    if (InitPlayerInfo()) {
        playerInfo->resource = resource;
    } else {
        LOGE("init player info ERROR");
        return PLAYER_RESULT_ERROR;
    }
    LOGD("SetResource() OK");
    return PLAYER_RESULT_OK;
}


int Player::Configure(ANativeWindow *window, int w, int h, bool isOnlyRecorderNode) {
    LOGI("----------Configure() called with: w=%d,h=%d,isOnlyRecorderNode=%d", w, h,
         isOnlyRecorderNode);
    if (!playerInfo) {
        LOGE("player info not init !");
    }
    if (isOnlyRecorderNode) {
        playerInfo->isOnlyRecorderNode = true;
        StartOpenResourceThread(playerInfo->resource);
        return PLAYER_RESULT_OK;
    }

    if (playerInfo && playerInfo->GetPlayState() != ERROR) {
        playerInfo->window = window;
        playerInfo->windowWith = w;
        playerInfo->windowHeight = h;
        char *resource = playerInfo->resource;
        if (!resource) {
            return PLAYER_RESULT_ERROR;
        }
        StartOpenResourceThread(resource);
    } else {
        LOGE("can't configure due to init player ERROR\n");
        return PLAYER_RESULT_ERROR;
    }
    LOGD("----------Configure Over-------------");
    return PLAYER_RESULT_OK;
}

int Player::OnWindowDestroy(ANativeWindow *window) {
    return PLAYER_RESULT_OK;
}

int Player::OnWindowChange(ANativeWindow *window, int w, int h) {
    LOGI("--------OnWindowChange() called with w=%d,h=%d", w, h);
    if (playerInfo && playerInfo->GetPlayState() == PAUSE) {
        playerInfo->window = window;
        playerInfo->windowWith = w;
        playerInfo->windowHeight = h;
        int ret = createAMediaCodec(&playerInfo->AMediaCodec, playerInfo->windowWith,
                                    playerInfo->windowHeight,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL,
                                    playerInfo->window, playerInfo->mine);

        AMediaCodec_start(playerInfo->AMediaCodec);
        if (ret == PLAYER_RESULT_OK) {
            LOGI("--------OnWindowChange() success!state->STARTED");
            playerInfo->SetPlayState(STARTED);
            StartDecodeThread(playerInfo);
        }
    } else {
        LOGE("player not init or it not pause");
        return PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_OK;
}


void Player::SetStateChangeListener(void (*listener)(PlayState, int)) {
    playerInfo->SetStateListener(listener);
}

int Player::Start() {
    LOGI("--------Play()  start-------");
    if (playerInfo == NULL) {
        LOGE("player not init,playerInfo == NULL!");
        return PLAYER_RESULT_ERROR;
    }
    PlayState state = playerInfo->GetPlayState();
    if (state != PREPARED) {
        LOGE("player not PREPARED!");
        return PLAYER_RESULT_ERROR;
    }
    StartDeMuxThread();
    return PLAYER_RESULT_OK;
}

int Player::Play() {
    LOGI("--------Play()  called-------");
    if (playerInfo == NULL) {
        LOGE("player not init,playerInfo == NULL!");
        return PLAYER_RESULT_ERROR;
    }

    if (playerInfo->isOnlyRecorderNode) {
        LOGE("player is only recorder mode!");
        return PLAYER_RESULT_ERROR;
    }

    PlayState state = playerInfo->GetPlayState();
    if (state == STARTED) {
        LOGE("player is started!");
        return PLAYER_RESULT_ERROR;
    }

    if (state == PAUSE) {
        playerInfo->SetPlayState(STARTED);
        return PLAYER_RESULT_OK;
    }

    if (state != PREPARED) {
        LOGE("player not PREPARED!");
        return PLAYER_RESULT_ERROR;
    }


    media_status_t status = AMediaCodec_start(playerInfo->AMediaCodec);
    if (status != AMEDIA_OK) {
        LOGE("start AMediaCodec fail!\n");
        AMediaCodec_delete(playerInfo->AMediaCodec);
        playerInfo->AMediaCodec = NULL;
        return PLAYER_RESULT_ERROR;
    } else {
        LOGI("------------AMediaCodec start success!!\n");
    }
    Start();
    return PLAYER_RESULT_OK;
}


int Player::Pause(int delay) {
    LOGI("--------Pause()  called-------");
    if (playerInfo == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    if (playerInfo->GetPlayState() != STARTED) {
        LOGE("--------Pause()  called fail ,player not started------");
        return PLAYER_RESULT_ERROR;
    }
    playerInfo->SetPlayState(PAUSE);
    return PLAYER_RESULT_OK;
}

int Player::Resume() {
    LOGI("--------Resume()  called-------");
    if (playerInfo == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    if (playerInfo->GetPlayState() != PAUSE) {
        LOGE("--------Pause()  called fail, player not pause------");
        return PLAYER_RESULT_ERROR;
    }
    playerInfo->SetPlayState(STARTED);
    return PLAYER_RESULT_OK;
}


int Player::Stop() {
    LOGI("--------Stop()  called-------");
    if (playerInfo == NULL) {
        LOGE("playerInfo is NULL");
        return PLAYER_RESULT_ERROR;
    }
    if (playerInfo->GetPlayState() != STARTED && playerInfo->GetPlayState() != PAUSE) {
        LOGE("player is not start!");
        return PLAYER_RESULT_ERROR;
    }
    playerInfo->SetPlayState(STOPPED);
    if (recorderInfo != NULL) {
        recorderInfo->SetRecordState(RECORD_STOP);
    }
    LOGI("Stop():start to stop recorde");
    //停止录制
    StopRecord();
    LOGD("--------Stop Over------");
    return PLAYER_RESULT_OK;
}

int Player::PrepareRecorder(char *outPath) {
    LOGI("---------PrepareRecorder() called with putPath=%s", outPath);
    if (!outPath) {
        return PLAYER_RESULT_ERROR;
    }
    if (recorderInfo == NULL) {
        LOGI("---------PrepareRecorder()  new RecorderInfo ");
        recorderInfo = new RecorderInfo;
        if (playerInfo != NULL) {
            recorderInfo->inputVideoStream = playerInfo->inputVideoStream;
        }
    }
    recorderInfo->storeFile = outPath;
    recorderInfo->SetRecordState(RECORD_PREPARED);
    return PLAYER_RESULT_OK;
}

int Player::StartRecord() {
    LOGI("------StartRecord() called----------");
    if (!playerInfo || !recorderInfo) {
        LOGE("------StartRecord() player init or recorder not prepare");
        return PLAYER_RESULT_ERROR;
    }
    RecordState state = recorderInfo->GetRecordState();

    if (state == RECORDING) {
        LOGE("------StartRecord() recorder is recording!");
        return PLAYER_RESULT_ERROR;
    }

    if (state == RECORD_ERROR) {
        LOGE("------StartRecord() recorder error!");
        return PLAYER_RESULT_ERROR;
    }

    if (state == RECORD_PAUSE) {
        recorderInfo->SetRecordState(RECORDING);
        return PLAYER_RESULT_OK;
    }
    if (state == RECORD_PREPARED) {
        recorderInfo->SetRecordState(RECORD_START);
        StartRecorderThread();
    } else {
        LOGE("start recorder in illegal state=%d", state);
        return PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_OK;
}

int Player::StopRecord() {
    LOGI("------StopRecord() called------");
    if (recorderInfo != NULL &&
        recorderInfo->GetRecordState() >= RECORD_START) {
        recorderInfo->SetRecordState(RECORD_STOP);
        return PLAYER_RESULT_OK;
    } else {
        return PLAYER_RESULT_ERROR;
    }
}


int Player::PauseRecord() {
    LOGI("------PauseRecord() called------");
    if (recorderInfo) {
        recorderInfo->SetRecordState(RECORD_PAUSE);
        return PLAYER_RESULT_OK;
    } else {
        return PLAYER_RESULT_ERROR;
    }
}

int Player::ResumeRecord() {
    LOGI("------ResumeRecord() called------");
    if (recorderInfo && recorderInfo->GetRecordState() == RECORD_PAUSE) {
        recorderInfo->SetRecordState(RECORD_START);
        return PLAYER_RESULT_OK;
    } else {
        return PLAYER_RESULT_ERROR;
    }
}

int Player::Release() {
    LOGD("Release() called!");
    Stop();
    if (playerInfo) {
        delete playerInfo;
        playerInfo = NULL;
    }
    if (recorderInfo) {
        delete recorderInfo;
        recorderInfo = NULL;
    }

    LOGD("Release() over!");
    return PLAYER_RESULT_OK;
}

void Player::SetDebug(bool debug) {
    LOGD("SetDebug() called with %d", debug);
    isDebug = debug;
}

Player::Player(int id) {
    playerId = id;
}

Player::~Player() {
    Release();
    LOGE("player delete over!");
}




