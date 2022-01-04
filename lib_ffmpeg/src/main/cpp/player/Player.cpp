
#include <android/native_window_jni.h>
#include <android/native_window.h>


#include "Player.h"
#include <PlayerResult.h>
#include<queue>
#include <pthread.h>
#include "Utils.h"

#ifdef __cplusplus
extern "C" {
#include <ctime>
#include"libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#endif
#ifdef __cplusplus
}
#endif

//clang -g -o pvlib  ParseVideo.c  -lavformat -lavutil
//clang -g -o pvlib  ParseVideo.c `pkg-config --libs libavutil libavformat`

#define head_1  0x00
#define head_2  0x00
#define head_3  0x00
#define head_4  0x01

#define head_I  0x65
#define head_P  0x61


//H.264:AVCC->AnnexB
static int H264_mp4toannexb_filter(AVBSFContext *bsf_ctx, AVPacket *pkt) {
// 1 获取相应的比特流过滤器
    // FLV/MP4/MKV等结构中，h264需要h264_mp4toannexb处理。添加SPS/PPS等信息。
    // FLV封装时，可以把多个NALU放在一个VIDEO TAG中,结构为4B NALU长度+NALU1+4B NALU长度+NALU2+...,
    // 需要做的处理把4B长度换成00000001或者000001
    // annexb模式: startcode 00000001 AVCC模式: 无startcode (mp4 flv mkv)
    // 4 发送和接受
    // bitstreamfilter内部去维护内存空间
    if (av_bsf_send_packet(bsf_ctx, pkt) != 0) {
        return PLAYER_RESULT_ERROR;
        LOGE("av_bsf_send_packet fail");
    }
    int ret = -1;
    while ((ret = av_bsf_receive_packet(bsf_ctx, pkt)) != 0) {
        LOGD("av_bsf_receive_packet,ret=%d", ret);
    };
    LOGD("av_bsf_receive_packet,ret=%d", ret);
    return PLAYER_RESULT_OK;
}

static int GetStartCodeLen(const unsigned char *pkt) {
    if (pkt[0] == 0 && pkt[1] == 0 && pkt[2] == 0 && pkt[3] == 1) {
        return 4;
    } else if (pkt[0] == 0 && pkt[1] == 0 && pkt[2] == 1) {
        return 3;
    } else {
        return 0;
    }
}


AVStream *findStream(AVFormatContext *i_fmt_ctx, AVMediaType type) {
    int index = av_find_best_stream(i_fmt_ctx, type, -1, -1, NULL, 0);
    if (index == AVERROR_STREAM_NOT_FOUND) {
        return NULL;
    } else {
        return i_fmt_ctx->streams[index];
    }
}

void StartRelease(PlayerInfo *playerInfo, RecorderInfo *recorderInfo) {
    LOGW("StartRelease() called !");
    if (playerInfo) {
        delete playerInfo;
        playerInfo = NULL;
    }
    if (recorderInfo) {
        delete recorderInfo;
        recorderInfo = NULL;
    }
}


int GetNALUType(AVPacket *packet) {
    int type = -1;
    const uint8_t *buf = packet->data;
    int len = GetStartCodeLen(buf);
    if ((len == 3 || len == 4) && packet->size > len) {
        type = buf[len] & 0xFF;
    }
    return type;
}


void *OpenResource(void *info) {
    if (info == nullptr) {
        return nullptr;
    }
    auto *playerInfo = (PlayerInfo *) info;
    char *url = playerInfo->resource;
    if (playerInfo->inputContext != NULL) {
        LOGI("close input context!before open resource");
        avformat_close_input(&playerInfo->inputContext);
        playerInfo->inputContext = avformat_alloc_context();
    } else {
        playerInfo->inputContext = avformat_alloc_context();
    }
    //打开多媒体文件，根据文件后缀名解析,第三个参数是显式制定文件类型，当文件后缀与文件格式不符
    //或者根本没有后缀时需要填写，
    LOGI("OpenResource() start open=%s", url);
    playerInfo->SetPlayState(EXECUTING, true);
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "stimeout", "6000000", 0);//设置超时3秒
    int ret = avformat_open_input(&playerInfo->inputContext, url, NULL, &opts);
    if (ret != 0) {
        LOGE("OpenResource():can't open source: %s ,msg:%s \n", url, av_err2str(ret));
        playerInfo->inputContext = NULL;
        playerInfo->SetPlayState(ERROR, true);
        return (void *) PLAYER_RESULT_ERROR;
    }

    PlayState state = playerInfo->GetPlayState();
    if (state == STOPPED || state == RELEASE || state == ERROR ||
        playerInfo->inputContext == NULL) {
        LOGW("close input context!after open resource,with state=%s",
             StateListener::PlayerStateToString(state).c_str());
        return NULL;
    }

    /* find stream info  */
    LOGI("----------open resource success!,start to find stream info-------------");
    if (avformat_find_stream_info(playerInfo->inputContext, &opts) < 0) {
        playerInfo->SetPlayState(ERROR, true);
        LOGE("could not find stream info\n");
        return (void *) PLAYER_RESULT_ERROR;
    }
    av_freep(&opts);
    LOGI("----------find stream info success!-------------");
    playerInfo->resource = url;
    LOGD("----------fmt_ctx:streams number=%d,bit rate=%ld\n",
         playerInfo->inputContext->nb_streams,
         playerInfo->inputContext->bit_rate);

    //输出多媒体文件信息,第二个参数是流的索引值（默认0），第三个参数，0:输入流，1:输出流
    //    av_dump_format(fmt_ctx, 0, url, 0);
    playerInfo->inputVideoStream = findStream(playerInfo->inputContext, AVMEDIA_TYPE_VIDEO);
    if (playerInfo->inputVideoStream == NULL) {
        playerInfo->SetPlayState(ERROR, true);
        LOGE("----------find video stream fail!-------------");
        return (void *) PLAYER_RESULT_ERROR;
    }
    LOGI("----------find video stream success!-------------");
    if (playerInfo->isOpenAudio) {
        playerInfo->inputAudioStream = findStream(playerInfo->inputContext, AVMEDIA_TYPE_AUDIO);
        if (playerInfo->inputAudioStream) {
            LOGI("find audio stream success!");
            AVCodecID codec_id = playerInfo->inputAudioStream->codecpar->codec_id;
            if (AV_CODEC_ID_AAC == codec_id) {
                playerInfo->mediaDecoder.configAudio("audio/mp4a-latm");
            } else {
                LOGE("not support audio type:%d", codec_id);
            }
        } else {
            LOGW("find audio stream fail!");
        }
    }

    //is H264?
    if (playerInfo->inputVideoStream->codecpar->codec_id != AV_CODEC_ID_H264) {
        LOGE("sorry this player only support h.264 now!");
        playerInfo->SetPlayState(ERROR, true);
        return (void *) PLAYER_RESULT_ERROR;
    }
    AVCodecParameters *codecpar = nullptr;
    codecpar = playerInfo->inputVideoStream->codecpar;
    if (codecpar == nullptr) {
        playerInfo->SetPlayState(ERROR, true);
        LOGE("OpenResource() fail:can't get video stream params");
        return (void *) PLAYER_RESULT_ERROR;
    }

    if (ret == PLAYER_RESULT_ERROR) {
        return (void *) PLAYER_RESULT_ERROR;
    }
    playerInfo->SetPlayState(PREPARED, true);

    if (IS_DEBUG) {
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
            case AV_PIX_FMT_YUVJ420P://ffmpeg 默认格式
                LOGD("input video color format is YUVJ420P");
                break;
            default:
                LOGD("input video color format is other");
                break;
        }
    }


    return nullptr;
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
            delete recorderInfo;
            recorderInfo = NULL;
            return (void *) PLAYER_RESULT_ERROR;
        }
        /*
        * since all input files are supposed to be identical (framerate, dimension, color format, ...)
        * we can safely set output codec values from first input file
        */
        AVCodecParameters *i_av_codec_parameters = recorderInfo->inputVideoStream->codecpar;
        recorderInfo->o_video_stream = avformat_new_stream(recorderInfo->o_fmt_ctx, NULL);
        if (!recorderInfo->o_video_stream) {
            LOGE("record fail,not found video stream!");
            return (void *) PLAYER_RESULT_ERROR;
        } else {
            LOGI("found video stream!");
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
        } else {
            LOGI("open file success,file=%s", file);
        }
        /*write file header*/
        ret = avformat_write_header(recorderInfo->o_fmt_ctx, NULL);
        if (ret < 0) {
            LOGE("record video write file header error,ret=%d", ret);
            recorderInfo->SetRecordState(RECORD_ERROR);
            return (void *) PLAYER_RESULT_ERROR;
        } else {
            LOGI("record video write file header success!");
        }
    } else {
        LOGW("RecordPkt() called,recorder state is not RECORD_START ");
    }

    while (true) {
        AVPacket *packet;
        RecordState recordState = recorderInfo->GetRecordState();
        if (recordState == RECORDER_RELEASE) {
            break;
        }
        if (recordState == RECORD_PAUSE) {
            recorderInfo->packetQueue.clearAVPacket();
            continue;
        }

        if (recordState == RECORD_STOP) {
            /*write file trailer*/
            LOGD("--------- recordState changed to stop ---------");
            av_write_trailer(recorderInfo->o_fmt_ctx);
            avformat_close_input(&recorderInfo->o_fmt_ctx);
            recorderInfo->o_fmt_ctx = NULL;
            LOGI("--------- record stop ,and write trailer over ---------");
            break;
        }

        int ret = recorderInfo->packetQueue.getAvPacket(&packet);

        if (ret == PLAYER_RESULT_ERROR) {
            //LOGW("-----------------record,not found pkt in queue--------------");
            continue;
        }

        //等待关键帧或sps、pps的出现
        if (recorderInfo->GetRecordState() == RECORD_START) {
            int type = packet->flags;
            if (type == 0x65 || type == 0x67 || type == 0x68 || type == 24 || type == 25 ||
                type == 26 || type == 27) {
                recorderInfo->SetRecordState(RECORDING);
                LOGI("-----------------real start recording--------------");
            } else {
                LOGW("-----------------record ,current is not key frame--------------");
                continue;
            }
        }
        if (recorderInfo->GetRecordState() == RECORDING) {
            //---------------write pkt data to file-----------
            /*write packet and auto free packet*/
            av_interleaved_write_frame(recorderInfo->o_fmt_ctx, packet);
        }
        av_packet_unref(packet);
    }
    LOGI("----------------- record work stop,start to delete recordInfo--------------");
    recorderInfo->packetQueue.clearAVPacket();
    if (recorderInfo->GetRecordState() == RECORDER_RELEASE) {
        StartRelease(NULL, recorderInfo);
    }
    return (void *) PLAYER_RESULT_OK;
}


void *Decode(void *info) {
    auto *playerInfo = (PlayerInfo *) info;
    while (true) {
        PlayState state = playerInfo->GetPlayState();
        if (state == PAUSE) {
            playerInfo->packetQueue.clearAVPacket();
            continue;
        }
        if (state != STARTED) {
            break;
        }

        AVPacket *packet = NULL;
        int ret = playerInfo->packetQueue.getAvPacket(&packet);
        if (ret == PLAYER_RESULT_OK) {
            uint8_t *data = packet->data;
            int length = packet->size;
            playerInfo->mediaDecoder.decodeVideo(data, length, 0);
        }
    }
    LOGI("-------Decode Stop!---------");
    return NULL;
}


int ProcessPacket(AVPacket *packet, AVCodecParameters *codecpar, PlayerInfo *playerInfo,
                  RecorderInfo *recorderInfo) {
    //检查是否有起始码,如果有说明是标准的H.264数据
    if (packet->size < 5 || playerInfo == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    int type = GetNALUType(packet);
//    if (isDebug) {
//        LOGD("------ProcessPacket NALU type=%0x,flag=%d", type, packet->flags);
//        if (type == 0x67 && playerInfo->lastNALUType == type) {
//            LOGW("------ProcessPacket more than one SPS in this GOP");
//        }
//        printCharsHex((char *) packet->data, 22, 18, "-------before------");
//        LOGD("ProcessPacket :pos=%ld,dts=%ld,pts=%ld,duration=%ld", packet->pos, packet->dts,
//             packet->pts, packet->duration);
//    }
    //H264的打包类型有AVC1、H264 、X264 、x264
    //The main difference between these media types is the presence of start codes in the bitstream.
    // If the subtype is MEDIASUBTYPE_AVC1, the bitstream does not contain start codes.
    if (type < 0) {//MEDIASUBTYPE_AVC1
        if (playerInfo->bsf_ctx == NULL) {
            const AVBitStreamFilter *bsfilter = av_bsf_get_by_name("h264_mp4toannexb");
            if (!bsfilter) {
                return PLAYER_RESULT_ERROR;
            }
            // 2 初始化过滤器上下文
            av_bsf_alloc(bsfilter, &playerInfo->bsf_ctx); //AVBSFContext;
            // 3 添加解码器属性
            avcodec_parameters_copy(playerInfo->bsf_ctx->par_in, codecpar);
            av_bsf_init(playerInfo->bsf_ctx);
        }
        H264_mp4toannexb_filter(playerInfo->bsf_ctx, packet);
//        if (isDebug) {
//         printCharsHex((char *) packet->data, 22, 16, "-------after------");
//        }
    } else {//MEDIASUBTYPE_H264
        packet->flags = type;
    }

    //加入录制队列
    if (recorderInfo != NULL) {
        RecordState recordState = recorderInfo->GetRecordState();
        //还需要写入文件尾部信h264息
        if (recordState == RECORD_STOP ||
            recordState == RECORD_START ||
            recordState == RECORDING) {
            // Create a new packet that references the same data as src
            AVPacket *copyPkt = av_packet_clone(packet);
            if (copyPkt != NULL) {
                recorderInfo->packetQueue.putAvPacket(copyPkt);
            } else {
                LOGE("ProcessPacket clone packet fail!");
            }
        }
    }

    //加入解码队列
    if (playerInfo->GetPlayState() == STARTED &&
        !playerInfo->isOnlyRecordMedia) { playerInfo->packetQueue.putAvPacket(packet); }
    return PLAYER_RESULT_OK;
}


void StartDecodeThread(PlayerInfo *playerInfo) {
    LOGI("StartDecodeThread() called");
    pthread_create(&playerInfo->decode_thread, NULL, Decode, playerInfo);
    pthread_setname_np(playerInfo->decode_thread, "decode_thread");
    pthread_detach(playerInfo->decode_thread);
}

void Player::StartRecorderThread() const {
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


static bool isLocalFile(char *url) {
    bool isFile = false;
    FILE *file = fopen(url, "r");
    if (file) {
        isFile = true;
        fclose(file);
    }
    LOGD("isLocalFile:url=%s,isFile=%d", url, isFile);
    return isFile;
}

void *DeMux(void *param) {
    auto *player = (Player *) param;
    PlayerInfo *playerInfo = player->playerInfo;
    if (playerInfo == NULL) {
        LOGE("Player is not init");
        return NULL;
    }

    if (playerInfo->GetPlayState() == STARTED) {
        LOGE("DeMux()  called  player is STARTED");
        return NULL;
    }
    AVStream *i_video_stream = playerInfo->inputVideoStream;
    AVCodecParameters *i_av_codec_parameters = i_video_stream->codecpar;
    int video_stream_index = i_video_stream->index;

    float delay = 0;
    int num = playerInfo->inputVideoStream->avg_frame_rate.num;
    int den = playerInfo->inputVideoStream->avg_frame_rate.den;
    if (isLocalFile(playerInfo->resource)) {
        float fps = (float) num / (float) den;
        delay = 1.0f / fps * 1000000;
        LOGI("--------------------Start DeMux----------------------FPS:%f,span=%d", fps,
             (unsigned int) delay);
    }

    playerInfo->SetPlayState(STARTED, true);
    if (!playerInfo->isOnlyRecordMedia) {
        LOGI("--------------------DeMux() change state to STARTED----------------------");
        StartDecodeThread(playerInfo);
    }
    PlayState state;
    while ((state = playerInfo->GetPlayState()) != STOPPED) {
        if (state == UNINITIALIZED || state == ERROR || state == RELEASE) {
            playerInfo->packetQueue.clearAVPacket();
            LOGD("DeMux() stop,due to state-STOPPED!");
            break;
        }
        RecorderInfo *recorderInfo = player->recorderInfo;
        //只有播放暂停与录制暂停同时出现才会暂停
        if (state == PAUSE) {//播放暂停
            if (recorderInfo != NULL &&
                recorderInfo->GetRecordState() == RECORD_PAUSE ||
                recorderInfo == NULL) {//检查录制是否也要暂停
                continue;
            }
        }

        AVPacket *i_pkt = av_packet_alloc();
        if (i_pkt == NULL) {
            LOGE("DeMux fail,because alloc av packet fail!");
            return NULL;
        }
        int ret = av_read_frame(playerInfo->inputContext, i_pkt);
        if (ret == 0 && i_pkt->size > 0) {
            if (i_pkt->stream_index == video_stream_index) {
                ProcessPacket(i_pkt, i_av_codec_parameters, playerInfo, recorderInfo);
                if (delay > 0) {
                    av_usleep((unsigned int) delay);
                }
            } else {
                av_packet_free(&i_pkt);
            }
        } else if (ret == AVERROR_EOF) {
            av_packet_free(&i_pkt);
            LOGE("DeMux: end of file!");
            break;
        } else {
            av_packet_free(&i_pkt);
            LOGW("DeMux:read frame ERROR!,ret=%d", ret);
        }
    }

    if (playerInfo->inputContext != nullptr) {
        LOGI("-----------DeMux close ----------------");
        avformat_close_input(&playerInfo->inputContext);
        playerInfo->inputContext = nullptr;
        LOGI("close input context!");
    }
    playerInfo->SetPlayState(STOPPED, true);
    LOGI("-----------DeMux stop over! ----------------");
    playerInfo->packetQueue.clearAVPacket();
    if (playerInfo->GetPlayState() == RELEASE) {
        StartRelease(playerInfo, nullptr);
    }
    return nullptr;
}


void Player::StartDeMuxThread() {
    LOGI("start deMux thread");
    pthread_create(&playerInfo->deMux_thread, NULL, DeMux, this);
    pthread_setname_np(playerInfo->deMux_thread, "deMux_thread");
    pthread_detach(playerInfo->deMux_thread);
}

void Player::StartOpenResourceThread(char *res) const {
    if (!res) {
        LOGE("StartOpenResourceThread() fail,res is null");
        return;
    }

    LOGI("start open resource thread");
    playerInfo->resource = res;
    pthread_create(&playerInfo->open_resource_thread, NULL, OpenResource, playerInfo);
    pthread_setname_np(playerInfo->open_resource_thread, "open_resource_thread");
    pthread_detach(playerInfo->open_resource_thread);
}

int Player::InitPlayerInfo() {
    LOGD("InitPlayerInfo() called");
    if (!playerInfo) {
        playerInfo = new PlayerInfo;
        playerInfo->id = playerId;
    } else {
        LOGW("InitPlayerInfo(),playerInfo is not NULL,it may be inited!");
    }

    playerInfo->SetStateListener(playStateListener);
    if (playerInfo->GetPlayState() == INITIALIZED) {
        return PLAYER_RESULT_OK;
    }

    LOGI("InitPlayerInfo():state-> INITIALIZED");
    playerInfo->SetPlayState(INITIALIZED, true);
    LOGD("init player info over");
    return PLAYER_RESULT_OK;
}


int Player::SetResource(char *resource) {
    LOGI("---------SetResource() called with:resource=%s\n", resource);
    if (NULL != playerInfo) {
        PlayState state = playerInfo->GetPlayState();
        if (state == STARTED) {
            LOGE("player is not stopped!");
            return PLAYER_RESULT_ERROR;
        }
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


int Player::Configure(ANativeWindow *window, int w, int h, bool isOnlyRecorderNode) const {
    LOGI("----------Configure() called with: w=%d,h=%d,isOnlyRecorderNode=%d", w, h,
         isOnlyRecorderNode);
    if (playerInfo == NULL) {
        LOGE("player info not init !");
        return PLAYER_RESULT_ERROR;
    }
    if (isOnlyRecorderNode) {
        playerInfo->isOnlyRecordMedia = true;
        StartOpenResourceThread(playerInfo->resource);
        return PLAYER_RESULT_OK;
    } else {
        playerInfo->mediaDecoder.config(playerInfo->mine, window, w, h);
    }

    if (playerInfo->GetPlayState() != ERROR) {
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

int Player::OnWindowChange(ANativeWindow *window, int w, int h) const {
    LOGI("--------OnWindowChange() called with w=%d,h=%d", w, h);
    if (playerInfo && playerInfo->GetPlayState() == PAUSE) {
        AVCodecParameters *codecpar = nullptr;
        codecpar = playerInfo->inputVideoStream->codecpar;
        if (codecpar == nullptr) {
            playerInfo->SetPlayState(ERROR, true);
            LOGE("OpenResource() fail:can't get video stream params");
            return PLAYER_RESULT_ERROR;
        }
        int ret = playerInfo->mediaDecoder.init(playerInfo->mine,
                                                window, w, h,
                                                codecpar->extradata,
                                                codecpar->extradata_size,
                                                codecpar->extradata,
                                                codecpar->extradata_size
        );

        if (ret == PLAYER_RESULT_ERROR) {
            return PLAYER_RESULT_ERROR;
        }
        ret = playerInfo->mediaDecoder.start();
        if (ret == PLAYER_RESULT_OK) {
            LOGI("--------OnWindowChange() success!state->STARTED");
            StartDecodeThread(playerInfo);
            playerInfo->SetPlayState(STARTED, true);
        }
    } else {
        LOGE("player not init or it not pause");
        return PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_OK;
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

    if (playerInfo->isOnlyRecordMedia) {
        LOGE("player is only recorder mode!");
        return PLAYER_RESULT_ERROR;
    }

    PlayState state = playerInfo->GetPlayState();
    if (state == STARTED) {
        LOGE("player is started!");
        return PLAYER_RESULT_ERROR;
    }

    if (state == PAUSE) {
        playerInfo->SetPlayState(STARTED, true);
        return PLAYER_RESULT_OK;
    }

    if (state != PREPARED) {
        LOGE("player not PREPARED!");
        return PLAYER_RESULT_ERROR;
    }


    AVCodecParameters *codecpar =
            playerInfo->inputVideoStream->codecpar;
    // init decoder
    int status = playerInfo->mediaDecoder.init(
            codecpar->extradata,
            codecpar->extradata_size,
            codecpar->extradata,
            codecpar->extradata_size
    );

    if (status != PLAYER_RESULT_OK) {
        LOGE("init AMediaCodec fail!");
        playerInfo->mediaDecoder.release();
        return PLAYER_RESULT_ERROR;
    }
    status = playerInfo->mediaDecoder.start();
    if (status != PLAYER_RESULT_OK) {
        LOGE("start AMediaCodec fail!\n");
        playerInfo->mediaDecoder.release();
        return PLAYER_RESULT_ERROR;
    }
    LOGI("------------AMediaCodec start success!!\n");
    Start();
    return PLAYER_RESULT_OK;
}


int Player::Pause(int delay) const {
    LOGI("--------Pause()  called-------");
    if (playerInfo == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    if (playerInfo->GetPlayState() != STARTED) {
        LOGE("--------Pause()  called fail ,player not started------");
        return PLAYER_RESULT_ERROR;
    }
    playerInfo->SetPlayState(PAUSE, true);
    return PLAYER_RESULT_OK;
}

int Player::Resume() const {
    LOGI("--------Resume()  called-------");
    if (playerInfo == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    if (playerInfo->GetPlayState() != PAUSE) {
        LOGE("--------Pause()  called fail, player not pause------");
        return PLAYER_RESULT_ERROR;
    }
    //  playerInfo->packetQueue.clearAVPacket();
    playerInfo->SetPlayState(STARTED, true);
    return PLAYER_RESULT_OK;
}


int Player::Stop() const {
    if (playerInfo == NULL) {
        LOGE("Stop() called with playerInfo is NULL");
        return PLAYER_RESULT_ERROR;
    }
    PlayState state = playerInfo->GetPlayState();
    LOGI("--------Stop()  called,state=%d-------", state);
    if (state == PREPARED || state == STARTED || state == PAUSE) {
        playerInfo->SetPlayState(STOPPED, false);
        LOGI("Stop():start to stop recorde");
        //停止录制
        StopRecord();
    } else {
        LOGE("player is not start!");
        return PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_OK;
}

int Player::PrepareRecorder(char *outPath) {
    LOGI("---------PrepareRecorder() called with putPath=%s", outPath);
    if (!outPath) {
        return PLAYER_RESULT_ERROR;
    }
    if (recorderInfo == NULL) {
        recorderInfo = new RecorderInfo;
        recorderInfo->id = playerId;
        if (playerInfo != NULL) {
            recorderInfo->inputVideoStream = playerInfo->inputVideoStream;
        }
        LOGI("---------PrepareRecorder()  new RecorderInfo ");
    }
    recorderInfo->SetStateListener(recorderStateListener);
    recorderInfo->storeFile = outPath;
    recorderInfo->SetRecordState(RECORD_PREPARED);
    return PLAYER_RESULT_OK;
}

int Player::StartRecord() const {
    if (!playerInfo || !recorderInfo) {
        LOGE("------StartRecord() player init or recorder not prepare");
        return PLAYER_RESULT_ERROR;
    }
    RecordState state = recorderInfo->GetRecordState();
    LOGI("------StartRecord() called state=%d----------", state);
    if (state == RECORDING || state == RECORDER_RELEASE) {
        LOGE("------StartRecord() recorder is recording!");
        return PLAYER_RESULT_ERROR;
    }

    if (state == RECORD_ERROR) {
        LOGE("------StartRecord() recorder error!");
        return PLAYER_RESULT_ERROR;
    }

    if (state == RECORD_PREPARED) {
        recorderInfo->SetRecordState(RECORD_START);
        StartRecorderThread();
    } else {
        LOGE("start recorder in illegal state=%d", state);
        recorderInfo->SetRecordState(RECORD_ERROR);
        return PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_OK;
}

int Player::StopRecord() const {
    LOGI("------StopRecord() called------");
    if (recorderInfo != NULL &&
        recorderInfo->GetRecordState() >= RECORD_PREPARED) {
        recorderInfo->SetRecordState(RECORD_STOP);
        return PLAYER_RESULT_OK;
    } else {
        return PLAYER_RESULT_ERROR;
    }
}


int Player::PauseRecord() const {
    LOGI("------PauseRecord() called------");
    if (recorderInfo && recorderInfo->GetRecordState() != RECORD_START) {
        recorderInfo->SetRecordState(RECORD_PAUSE);
        return PLAYER_RESULT_OK;
    } else {
        LOGE("PauseRecord() called fail in error state");
        return PLAYER_RESULT_ERROR;
    }
}

int Player::ResumeRecord() const {
    LOGI("------ResumeRecord() called------");
    if (recorderInfo && recorderInfo->GetRecordState() == RECORD_PAUSE) {
        recorderInfo->SetRecordState(RECORDING);
        return PLAYER_RESULT_OK;
    } else {
        return PLAYER_RESULT_ERROR;
    }
}

int Player::Release() {
    LOGD("Release() called!");
    if (playerInfo && playerInfo->GetPlayState() == STOPPED) {
        StartRelease(playerInfo, recorderInfo);
        return PLAYER_RESULT_OK;
    }
    if (playerInfo) {
        playerInfo->SetPlayState(RELEASE, true);
    }
    if (recorderInfo) {
        recorderInfo->SetRecordState(RECORDER_RELEASE);
    }
    jPlayer.jMid_onRecordStateChangeId = nullptr;
    jPlayer.jMid_onPlayStateChangeId = nullptr;
    LOGD("Release() over!");
    return PLAYER_RESULT_OK;
}


void Player::SetDebug(bool debug) {
    LOGD("SetDebug() called with %d", debug);
    isDebug(debug);
    if (debug) {
        av_log_set_callback(ffmpeg_android_log_callback);
    } else {
        av_log_set_callback(NULL);
    }
}

Player::Player(int id) {
    playerId = id;
}

Player::~Player() {
    Release();
    LOGE("player delete over!");
}


void Player::SetRecordStateChangeListener(void (*listener)(RecordState, int)) {
    recorderStateListener = listener;
    if (recorderInfo) {
        recorderInfo->SetStateListener(listener);
    }
}

void Player::SetPlayStateChangeListener(void (*listener)(PlayState, int)) {
    playStateListener = listener;
    if (playerInfo) {
        playerInfo->SetStateListener(listener);
    }
}










