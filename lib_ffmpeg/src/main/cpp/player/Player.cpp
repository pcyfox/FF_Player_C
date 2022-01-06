
#include <android/native_window_jni.h>
#include <android/native_window.h>


#include "Player.h"
#include <PlayerResult.h>
#include<queue>
#include <pthread.h>
#include "Utils.h"
#include "util.hpp"

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


static void StartRelease(PlayerContext *playerContext, RecorderContext *recorderContext) {
    LOGW("StartRelease() called !");
    if (playerContext) {
        delete playerContext;
        playerContext = NULL;
    }
    if (recorderContext) {
        delete recorderContext;
        recorderContext = NULL;
    }
}


void *OpenResourceThread(void *info) {
    if (info == nullptr) {
        return nullptr;
    }
    auto *playerContext = (PlayerContext *) info;
    char *url = playerContext->resource.url;
    if (playerContext->inputContext != NULL) {
        LOGI("close input context!before open resource");
        avformat_close_input(&playerContext->inputContext);
        playerContext->inputContext = avformat_alloc_context();
    } else {
        playerContext->inputContext = avformat_alloc_context();
    }
    //打开多媒体文件，根据文件后缀名解析,第三个参数是显式制定文件类型，当文件后缀与文件格式不符
    //或者根本没有后缀时需要填写，
    LOGI("OpenResource() start open=%s", url);
    playerContext->SetPlayState(EXECUTING, true);
    playerContext->resource.check();

    AVDictionary *opts = NULL;
    if (playerContext->resource.type == RTP) {
        av_dict_set(&opts, "stimeout", "6000000", 0);//设置超时6秒
    }

    int ret = avformat_open_input(&playerContext->inputContext, url, NULL, &opts);
    if (ret != 0) {
        LOGE("OpenResource():can't open source: %s ,msg:%s \n", url, av_err2str(ret));
        playerContext->inputContext = NULL;
        playerContext->SetPlayState(ERROR, true);
        return (void *) PLAYER_RESULT_ERROR;
    }

    PlayState state = playerContext->GetPlayState();
    if (state == STOPPED || state == RELEASE || state == ERROR) {
        LOGW("close input context!after open resource,with state=%s",
             StateListener::PlayerStateToString(state).c_str());
        return NULL;
    }

    /* find stream info  */
    LOGI("----------open resource success!,start to find stream info-------------");
    if (avformat_find_stream_info(playerContext->inputContext, opts == NULL ? NULL : &opts) < 0) {
        playerContext->SetPlayState(ERROR, true);
        LOGE("could not find stream info\n");
        av_freep(&opts);
        delete playerContext;
        return (void *) PLAYER_RESULT_ERROR;
    }
    av_freep(&opts);

    LOGI("----------find stream info success!-------------");

    LOGD("----------fmt_ctx:streams number=%d,bit rate=%ld\n",
         playerContext->inputContext->nb_streams,
         playerContext->inputContext->bit_rate);

    playerContext->inputVideoStream = findStream(playerContext->inputContext, AVMEDIA_TYPE_VIDEO);
    if (playerContext->inputVideoStream == NULL) {
        playerContext->SetPlayState(ERROR, true);
        LOGE("----------find video stream fail!-------------");
        return (void *) PLAYER_RESULT_ERROR;
    }

    LOGI("----------find video stream success!-------------");

    //is H264?
    if (playerContext->inputVideoStream->codecpar->codec_id != AV_CODEC_ID_H264) {
        LOGE("sorry this player only support h.264 now!");
        playerContext->SetPlayState(ERROR, true);
        return (void *) PLAYER_RESULT_ERROR;
    }

    AVCodecParameters *codecpar = playerContext->inputVideoStream->codecpar;
    if (codecpar == nullptr) {
        playerContext->SetPlayState(ERROR, true);
        LOGE("OpenResource() fail:can't get video stream params");
        return (void *) PLAYER_RESULT_ERROR;
    }

    if (IS_DEBUG) {
        dumpStreamInfo(codecpar);
    }


    if (playerContext->isOpenAudio) {
        playerContext->inputAudioStream = findStream(playerContext->inputContext,
                                                     AVMEDIA_TYPE_AUDIO);
        if (playerContext->inputAudioStream) {
            LOGI("find audio stream success!");
            AVCodecID codec_id = playerContext->inputAudioStream->codecpar->codec_id;
            if (AV_CODEC_ID_AAC == codec_id) {
                playerContext->mediaDecodeContext.configAudio("audio/mp4a-latm");
            } else {
                LOGE("not support audio type:%d", codec_id);
            }
        } else {
            LOGW("find audio stream fail!");
        }
    }

    playerContext->SetPlayState(PREPARED, true);
    return nullptr;
}


void *RecordPktThread(void *info) {
    auto *recorderInfo = (RecorderContext *) info;
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


void *DecodeThread(void *info) {
    auto *playerInfo = (PlayerContext *) info;
    while (true) {
        PlayState state = playerInfo->GetPlayState();
        if (state == PAUSE) {
            playerInfo->videoPacketQueue.clearAVPacket();
            continue;
        }
        if (state != STARTED) {
            break;
        }

        AVPacket *packet = NULL;
        int ret = playerInfo->videoPacketQueue.getAvPacket(&packet);
        if (ret == PLAYER_RESULT_OK) {
            uint8_t *data = packet->data;
            int length = packet->size;
            playerInfo->mediaDecodeContext.decodeVideo(data, length, 0);
        }
    }
    LOGI("-------Decode Stop!---------");
    return NULL;
}


int ProcessPacket(AVPacket *packet, AVCodecParameters *codecpar, PlayerContext *playerInfo,
                  RecorderContext *recorderInfo) {
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
        !playerInfo->isOnlyRecordMedia) { playerInfo->videoPacketQueue.putAvPacket(packet); }
    return PLAYER_RESULT_OK;
}


void *DeMuxThread(void *param) {
    auto *player = (Player *) param;
    PlayerContext *playerContext = player->playerContext;
    if (playerContext == NULL) {
        LOGE("Player is not init");
        return NULL;
    }

    if (playerContext->GetPlayState() == STARTED) {
        LOGE("DeMux()  called  player is STARTED");
        return NULL;
    }
    AVStream *i_video_stream = playerContext->inputVideoStream;
    AVCodecParameters *i_av_codec_parameters = i_video_stream->codecpar;
    int video_stream_index = i_video_stream->index;

    float delay = 0;
    int num = playerContext->inputVideoStream->avg_frame_rate.num;
    int den = playerContext->inputVideoStream->avg_frame_rate.den;

    if (playerContext->resource.type != RTP) {
        float fps = (float) num / (float) den;
        delay = 1.0f / fps * 1000000;
        LOGI("--------------------Start DeMux----------------------FPS:%f,span=%d", fps,
             (unsigned int) delay);
    }

    playerContext->SetPlayState(STARTED, true);
    if (!playerContext->isOnlyRecordMedia) {
        LOGI("--------------------DeMux() change state to STARTED----------------------");
        player->StartDecodeThread();
    }
    PlayState state;
    while ((state = playerContext->GetPlayState()) != STOPPED) {
        if (state == UNINITIALIZED || state == ERROR || state == RELEASE) {
            playerContext->videoPacketQueue.clearAVPacket();
            LOGD("DeMux() stop,due to state-STOPPED!");
            break;
        }
        RecorderContext *recorderInfo = player->recorderContext;
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
        int ret = av_read_frame(playerContext->inputContext, i_pkt);
        if (ret == 0 && i_pkt->size > 0) {
            if (i_pkt->stream_index == video_stream_index) {
                ProcessPacket(i_pkt, i_av_codec_parameters, playerContext, recorderInfo);
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

    if (playerContext->inputContext != nullptr) {
        LOGI("-----------DeMux close ----------------");
        avformat_close_input(&playerContext->inputContext);
        playerContext->inputContext = nullptr;
        LOGI("close input context!");
    }
    playerContext->SetPlayState(STOPPED, true);
    LOGI("-----------DeMux stop over! ----------------");
    playerContext->videoPacketQueue.clearAVPacket();
    if (playerContext->GetPlayState() == RELEASE) {
        StartRelease(playerContext, nullptr);
    }
    return nullptr;
}

void Player::StartDecodeThread() const {
    LOGI("StartDecodeThread() called");
    pthread_create(&playerContext->decode_thread, NULL, DecodeThread, playerContext);
    pthread_setname_np(playerContext->decode_thread, "decode_thread");
    pthread_detach(playerContext->decode_thread);
}

void Player::StartDeMuxThread() {
    LOGI("start deMux thread");
    pthread_create(&playerContext->deMux_thread, NULL, DeMuxThread, this);
    pthread_setname_np(playerContext->deMux_thread, "deMux_thread");
    pthread_detach(playerContext->deMux_thread);
}

void Player::StartOpenResourceThread() const {
    if (!playerContext->resource.url) {
        LOGE("StartOpenResourceThread() fail,urlis null");
        return;
    }
    LOGI("start open resource thread");
    pthread_create(&playerContext->open_resource_thread, NULL, OpenResourceThread, playerContext);
    pthread_setname_np(playerContext->open_resource_thread, "open_resource_thread");
    pthread_detach(playerContext->open_resource_thread);
}


void Player::StartRecorderThread() const {
    if (playerContext != NULL && recorderContext != NULL) {
        recorderContext->inputVideoStream = playerContext->inputVideoStream;
    } else {
        LOGE("Start recorder thread fail!");
        return;
    }
    LOGI("Start recorder thread");
    pthread_create(&recorderContext->recorder_thread, NULL, RecordPktThread, recorderContext);
    pthread_setname_np(recorderContext->recorder_thread, "recorder_thread");
    pthread_detach(recorderContext->recorder_thread);
}

int Player::InitPlayerContext() {
    LOGD("InitPlayerContext() called");
    if (!playerContext) {
        playerContext = new PlayerContext;
        playerContext->id = playerId;
    } else {
        LOGW("InitPlayerContext(),playerInfo is not NULL,it may be inited!");
    }

    playerContext->SetStateListener(playStateListener);
    if (playerContext->GetPlayState() == INITIALIZED) {
        return PLAYER_RESULT_OK;
    }

    LOGI("InitPlayerContext():state-> INITIALIZED");
    playerContext->SetPlayState(INITIALIZED, true);
    LOGD("init player info over");
    return PLAYER_RESULT_OK;
}


int Player::SetResource(char *url) {
    LOGI("---------SetResource() called with:url=%s\n", url);
    if (NULL != playerContext) {
        PlayState state = playerContext->GetPlayState();
        if (state == STARTED) {
            LOGE("player is not stopped!");
            return PLAYER_RESULT_ERROR;
        }
    }

    if (InitPlayerContext()) {
        playerContext->resource.url = url;
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
    if (playerContext == NULL) {
        LOGE("player info not init !");
        return PLAYER_RESULT_ERROR;
    }
    if (isOnlyRecorderNode) {
        playerContext->isOnlyRecordMedia = true;
        StartOpenResourceThread();
        return PLAYER_RESULT_OK;
    }

    playerContext->mediaDecodeContext.config(playerContext->mine, window, w, h);
    if (playerContext->GetPlayState() != ERROR) {
        if (!playerContext->resource.url) {
            return PLAYER_RESULT_ERROR;
        }
        StartOpenResourceThread();
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
    if (playerContext && playerContext->GetPlayState() == PAUSE) {
        AVCodecParameters *codecpar = nullptr;
        codecpar = playerContext->inputVideoStream->codecpar;
        if (codecpar == nullptr) {
            playerContext->SetPlayState(ERROR, true);
            LOGE("OpenResource() fail:can't get video stream params");
            return PLAYER_RESULT_ERROR;
        }
        int ret = playerContext->mediaDecodeContext.init(playerContext->mine,
                                                         window, w, h,
                                                         codecpar->extradata,
                                                         codecpar->extradata_size,
                                                         codecpar->extradata,
                                                         codecpar->extradata_size
        );

        if (ret == PLAYER_RESULT_ERROR) {
            return PLAYER_RESULT_ERROR;
        }
        ret = playerContext->mediaDecodeContext.start();
        if (ret == PLAYER_RESULT_OK) {
            LOGI("--------OnWindowChange() success!state->STARTED");
            StartDecodeThread();
            playerContext->SetPlayState(STARTED, true);
        }
    } else {
        LOGE("player not init or it not pause");
        return PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_OK;
}


int Player::Start() {
    LOGI("--------Play()  start-------");
    if (playerContext == NULL) {
        LOGE("player not init,playerInfo == NULL!");
        return PLAYER_RESULT_ERROR;
    }
    PlayState state = playerContext->GetPlayState();
    if (state != PREPARED) {
        LOGE("player not PREPARED!");
        return PLAYER_RESULT_ERROR;
    }
    StartDeMuxThread();
    return PLAYER_RESULT_OK;
}

int Player::Play() {
    LOGI("--------Play()  called-------");
    if (playerContext == NULL) {
        LOGE("player not init,playerInfo == NULL!");
        return PLAYER_RESULT_ERROR;
    }

    if (playerContext->isOnlyRecordMedia) {
        LOGE("player is only recorder mode!");
        return PLAYER_RESULT_ERROR;
    }

    PlayState state = playerContext->GetPlayState();
    if (state == STARTED) {
        LOGE("player is started!");
        return PLAYER_RESULT_ERROR;
    }

    if (state == PAUSE) {
        playerContext->SetPlayState(STARTED, true);
        return PLAYER_RESULT_OK;
    }

    if (state != PREPARED) {
        LOGE("player not PREPARED!");
        return PLAYER_RESULT_ERROR;
    }


    AVCodecParameters *codecpar =
            playerContext->inputVideoStream->codecpar;
    // init decoder
    int status = playerContext->mediaDecodeContext.init(
            codecpar->extradata,
            codecpar->extradata_size,
            codecpar->extradata,
            codecpar->extradata_size
    );

    if (status != PLAYER_RESULT_OK) {
        LOGE("init AMediaCodec fail!");
        playerContext->mediaDecodeContext.release();
        return PLAYER_RESULT_ERROR;
    }
    status = playerContext->mediaDecodeContext.start();
    if (status != PLAYER_RESULT_OK) {
        LOGE("start AMediaCodec fail!\n");
        playerContext->mediaDecodeContext.release();
        return PLAYER_RESULT_ERROR;
    }
    LOGI("------------AMediaCodec start success!!\n");
    Start();
    return PLAYER_RESULT_OK;
}


int Player::Pause(int delay) const {
    LOGI("--------Pause()  called-------");
    if (playerContext == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    if (playerContext->GetPlayState() != STARTED) {
        LOGE("--------Pause()  called fail ,player not started------");
        return PLAYER_RESULT_ERROR;
    }
    playerContext->SetPlayState(PAUSE, true);
    return PLAYER_RESULT_OK;
}

int Player::Resume() const {
    LOGI("--------Resume()  called-------");
    if (playerContext == NULL) {
        return PLAYER_RESULT_ERROR;
    }
    if (playerContext->GetPlayState() != PAUSE) {
        LOGE("--------Pause()  called fail, player not pause------");
        return PLAYER_RESULT_ERROR;
    }
    //  playerInfo->packetQueue.clearAVPacket();
    playerContext->SetPlayState(STARTED, true);
    return PLAYER_RESULT_OK;
}


int Player::Stop() const {
    if (playerContext == NULL) {
        LOGE("Stop() called with playerInfo is NULL");
        return PLAYER_RESULT_ERROR;
    }
    PlayState state = playerContext->GetPlayState();
    LOGI("--------Stop()  called,state=%d-------", state);
    if (state == PREPARED || state == STARTED || state == PAUSE) {
        playerContext->SetPlayState(STOPPED, false);
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
    if (recorderContext == NULL) {
        recorderContext = new RecorderContext;
        recorderContext->id = playerId;
        if (playerContext != NULL) {
            recorderContext->inputVideoStream = playerContext->inputVideoStream;
        }
        LOGI("---------PrepareRecorder()  new RecorderInfo ");
    }
    recorderContext->SetStateListener(recorderStateListener);
    recorderContext->storeFile = outPath;
    recorderContext->SetRecordState(RECORD_PREPARED);
    return PLAYER_RESULT_OK;
}

int Player::StartRecord() const {
    if (!playerContext || !recorderContext) {
        LOGE("------StartRecord() player init or recorder not prepare");
        return PLAYER_RESULT_ERROR;
    }
    RecordState state = recorderContext->GetRecordState();
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
        recorderContext->SetRecordState(RECORD_START);
        StartRecorderThread();
    } else {
        LOGE("start recorder in illegal state=%d", state);
        recorderContext->SetRecordState(RECORD_ERROR);
        return PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_OK;
}

int Player::StopRecord() const {
    LOGI("------StopRecord() called------");
    if (recorderContext != NULL &&
        recorderContext->GetRecordState() >= RECORD_PREPARED) {
        recorderContext->SetRecordState(RECORD_STOP);
        return PLAYER_RESULT_OK;
    } else {
        return PLAYER_RESULT_ERROR;
    }
}


int Player::PauseRecord() const {
    LOGI("------PauseRecord() called------");
    if (recorderContext && recorderContext->GetRecordState() != RECORD_START) {
        recorderContext->SetRecordState(RECORD_PAUSE);
        return PLAYER_RESULT_OK;
    } else {
        LOGE("PauseRecord() called fail in error state");
        return PLAYER_RESULT_ERROR;
    }
}

int Player::ResumeRecord() const {
    LOGI("------ResumeRecord() called------");
    if (recorderContext && recorderContext->GetRecordState() == RECORD_PAUSE) {
        recorderContext->SetRecordState(RECORDING);
        return PLAYER_RESULT_OK;
    } else {
        return PLAYER_RESULT_ERROR;
    }
}

int Player::Release() {
    LOGD("Release() called!");
    if (playerContext && playerContext->GetPlayState() == STOPPED) {
        StartRelease(playerContext, recorderContext);
        return PLAYER_RESULT_OK;
    }
    if (playerContext) {
        playerContext->SetPlayState(RELEASE, true);
    }
    if (recorderContext) {
        recorderContext->SetRecordState(RECORDER_RELEASE);
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
    if (recorderContext) {
        recorderContext->SetStateListener(listener);
    }
}

void Player::SetPlayStateChangeListener(void (*listener)(PlayState, int)) {
    playStateListener = listener;
    if (playerContext) {
        playerContext->SetStateListener(listener);
    }
}










