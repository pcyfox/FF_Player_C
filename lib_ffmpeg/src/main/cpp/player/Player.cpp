
#include "android_log.h"
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>


#include "Player.h"
#include <PlayerResult.h>
#include<queue>
#include <pthread.h>
#include <PlayerInfo.h>
#include <RecorderInfo.h>
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

PlayerInfo *playerInfo = NULL;
RecorderInfo *recorderInfo = NULL;
static bool isDebug = false;

static int is_under_analysis_resource = 0;

static int h2645_ps_to_nalu(const uint8_t *src, uint8_t src_size, uint8_t **out, int *out_size) {
    int i;
    int ret = 0;
    uint8_t *p = NULL;
    static const uint8_t nalu_header[] = {0x00, 0x00, 0x00, 0x01};

    if (!out || !out_size) {
        return AVERROR(EINVAL);
    }

    p = static_cast<uint8_t *>(av_malloc(sizeof(nalu_header) + src_size));

    if (!p) {
        return AVERROR(ENOMEM);
    }

    *out = p;
    *out_size = sizeof(nalu_header) + src_size;

    memcpy(p, nalu_header, sizeof(nalu_header));
    memcpy(p + sizeof(nalu_header), src, src_size);

    /* Escape 0x00, 0x00, 0x0{0-3} pattern */
    for (i = 4; i < *out_size; i++) {
        if (i < *out_size - 3 &&
            p[i + 0] == 0 &&
            p[i + 1] == 0 &&
            p[i + 2] <= 3) {
            uint8_t *new_data;

            *out_size += 1;
            new_data = static_cast<uint8_t *>(av_realloc(*out, *out_size));
            if (!new_data) {
                ret = AVERROR(ENOMEM);
                goto done;
            }
            *out = p = new_data;

            i = i + 2;
            memmove(p + i + 1, p + i, *out_size - (i + 1));
            p[i] = 0x03;
        }
    }
    done:
    if (ret < 0) {
        av_freep(out);
        *out_size = 0;
    }

    return ret;
}

static int set_start_code(AVPacket *out,
                          const uint8_t *sps_pps, uint32_t sps_pps_size,
                          const uint8_t *buf, uint32_t in_size) {

    uint32_t offset = out->size;
    uint8_t nal_header_size = 4;
    int err;
    err = av_grow_packet(out, sps_pps_size + in_size + nal_header_size);
    if (err < 0)
        return err;

    if (sps_pps) {
        memcpy(out->data + offset, sps_pps, sps_pps_size);
    }

    memcpy(out->data + sps_pps_size + nal_header_size + offset, buf, in_size);

    if (!offset) {
        AV_WB32(out->data + sps_pps_size, 1);
    } else {
        (out->data + offset + sps_pps_size)[0] =
        (out->data + offset + sps_pps_size)[1] = 0;
        (out->data + offset + sps_pps_size)[2] = 1;
    }

    return 0;
}

int h264_extra_data_to_annexb(const uint8_t *codec_extra_data, const int codec_extra_data_size,
                              AVPacket *out_extra_data, int padding) {
    uint16_t unit_size = 0;
    uint64_t total_size = 0;
    uint8_t *buffer = NULL;
    uint8_t unit_nb = 0;//sps的数量，通常只有一个
    uint8_t sps_done = 0;

    uint8_t sps_seen = 0;//是否找到sps
    uint8_t pps_seen = 0;//是否找到pps

    uint8_t sps_offset = 0;
    uint8_t pps_offset = 0;

    /**
     * AVCC
     * bits
     *  8   version ( always 0x01 )
     *  8   avc profile ( sps[0][1] )
     *  8   avc compatibility ( sps[0][2] )
     *  8   avc level ( sps[0][3] )
     *
     *  6   reserved ( all bits on )
     *  2   NALULengthSizeMinusOne    // 这个值是（前缀长度-1），值如果是3，那前缀就是4，因为4-1=3
     *
     *  3   reserved ( all bits on )
     *  5   number of SPS NALUs (usually 1)
     *
     *  repeated once per SPS:
     *  16     SPS size
     *
     *  variable   SPS NALU data
     *  8   number of PPS NALUs (usually 1)
     *
     *  repeated once per PPS
     *  16    PPS size
     *  variable PPS NALU data
     */

    const uint8_t *extra_data = codec_extra_data + 4; //extra_data存放数据的格式如上，前4个字节没用
    static const uint8_t nalu_header[4] = {0, 0, 0, 1}; //每个H264裸数据都是以 0001 4个字节为开头的
    extra_data++;//跳过一个字节，这个也没用

    sps_offset = pps_offset = -1;

    /* retrieve sps and pps unit(s) */
    unit_nb = *extra_data++ & 0x1f; /* 取 SPS 个数，理论上可以有多个, 但我没有见到过多 SPS 的情况*/

    if (!unit_nb) {
        goto pps;
    } else {
        sps_offset = 0;
        sps_seen = 1;
    }

    while (unit_nb--) {
        int err;
        unit_size = AV_RB16(extra_data);
        total_size += unit_size + 4; //加上4字节的h264 header, 即 0001

        if (total_size > INT_MAX - padding) {
            LOGE("Too big extradata size, corrupted stream or invalid MP4/AVCC bitstream\n");
            av_free(buffer);
            return AVERROR(EINVAL);
        }

        //2:表示上面 unit_size 的所占字结数
        //这句的意思是 extra_data 所指的地址，加两个字节，再加 unit 的大小所指向的地址
        //是否超过了能访问的有效地址空间
        if (extra_data + 2 + unit_size > codec_extra_data + codec_extra_data_size) {
            LOGE("Packet header is not contained in global extradata, ""corrupted stream or invalid MP4/AVCC bitstream\n");
            av_free(buffer);
            return AVERROR(EINVAL);
        }

        //分配存放 SPS 的空间
        if ((err = av_reallocp(&buffer, total_size + padding)) < 0)
            return err;

        memcpy(buffer + total_size - unit_size - 4, nalu_header, 4);
        memcpy(buffer + total_size - unit_size, extra_data + 2, unit_size);


        extra_data += 2 + unit_size;

        pps:
        //当 SPS 处理完后，开始处理 PPS
        if (!unit_nb && !sps_done++) {
            unit_nb = *extra_data++; /* number of pps unit(s) */
            if (unit_nb) {
                pps_offset = total_size;
                pps_seen = 1;
            }
        }
    }


    //余下的空间清0
    if (buffer) {
        memset(buffer + total_size, 0, padding);
    }

    if (!sps_seen)
        av_log(NULL, AV_LOG_WARNING,
               "Warning: SPS NALU missing or invalid. "
               "The resulting stream may not play.\n");

    if (!pps_seen)
        av_log(NULL, AV_LOG_WARNING,
               "Warning: PPS NALU missing or invalid. "
               "The resulting stream may not play.\n");

    out_extra_data->data = buffer;
    out_extra_data->size = total_size;

    return 0;
}

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

void get_sps_pps_form_extra_data(const uint8_t *codec_extra_data, const int codec_extra_data_size,
                                 uint8_t **pps, int *ppsSize, uint8_t **sps,
                                 int *spsSize,
                                 int padding) {


//    for (int i = 0; i < codec_extra_data_size; ++i) {
//        LOGI("extra data i=%d,data= %02x", i, *(codec_extra_data + i));
//    }


    uint16_t sps_size, pps_size;
    uint64_t sps_total_length = 0, pps_total_length = 0;//start code长度+真实数据长度
    uint8_t unit_nb = 0;//sps的数量，通常只有一个
    uint8_t sps_done = 0;

    uint8_t sps_seen = 0;//是否找到sps
    uint8_t pps_seen = 0;//是否找到pps

    uint8_t *pps_buffer = *pps;
    uint8_t *sps_buffer = *sps;


    /**
     * AVCC
     * bits
     *  8   version ( always 0x01 )
     *  8   avc profile ( sps[0][1] )
     *  8   avc compatibility ( sps[0][2] )
     *  8   avc level ( sps[0][3] )
     *byte_5:
     *  6   reserved ( all bits on )
     *  2   NALULengthSizeMinusOne    // 这个值是（前缀长度-1），值如果是3，那前缀就是4，因为4-1=3
     *byte_6:
     *  3   reserved ( all bits on )
     *  5   number of SPS NALUs (usually 1)
     *byte_7
     *  ----------------SPS---------------------------
     *  repeated once per SPS:
     *
     *
     *
     *  16     SPS size
     *
     *
     *
     *
     *  variable   SPS NALU data
     *
     *
     *
     *  ----------------PPS---------------------------
     *  8   number of PPS NALUs (usually 1)
     *
     *  repeated once per PPS
     *  16    PPS size
     *
     *  variable PPS NALU data
     */

    const uint8_t *current_extra_data = codec_extra_data + 4; //extra_data存放数据的格式如上，前4个字节没用
    static const uint8_t nalu_start_code[4] = {0, 0, 0, 1}; //每个H264裸数据都是以 0001 4个字节为开头的

    //-----byte_5
    current_extra_data++;//跳过一个字节，这个也没用,跳到第5个字节处

    //获取nalu length所占字节数
    int nalu_length_bytes = (*current_extra_data++ & 0x03) + 1;
    LOGI("nalu_length_bytes=%d", nalu_length_bytes);

    //------byte-6: 后五位是sps或pps的个数   retrieve sps and pps unit(s)
    unit_nb = *current_extra_data++ & 0x1f; /* 取 SPS 个数，并将数据移动到byte_7,理论上可以有多个, 但我没有见到过多 SPS 的情况*/

    if (!unit_nb) {
        goto __pps;
    } else {
        sps_seen = 1;
    }

    while (unit_nb--) {
        //-------byte_7、byte_8两个字节表示sps数据的长度
        sps_size = AV_RB16(current_extra_data);

        sps_total_length += sps_size + 4; //加上4字节的h264 header, 即 0001
        if (sps_total_length > INT_MAX - padding) {
            LOGE("Too big extradata size, corrupted stream or invalid MP4/AVCC bitstream\n");
            spsSize = 0;
            av_free(sps_buffer);
            return;
        }

        {
            // byte_9: 从第9个开始是nalu数据。检查包类型：
            uint8_t nalu_type = current_extra_data[1];
            nalu_type = nalu_type & 0x1f;
            LOGI("nalu_type=%d", nalu_type);
        }

        //+2:表示上面 unit_size 的所占字结数
        //这句的意思是 current_extra_data 所指的地址，加两个字节，再加 unit 的大小所指向的地址
        //是否超过了能访问的有效地址空间
        if (current_extra_data + 2 + sps_size > codec_extra_data + codec_extra_data_size) {
            LOGE("Packet header is not contained in global extradata, ""corrupted stream or invalid MP4/AVCC bitstream\n");
            spsSize = 0;
            av_free(sps_buffer);
            return;
        }

        //分配存放 SPS 的空间
        if (av_reallocp(&sps_buffer, sps_total_length + padding) < 0) {
            return;
        }

        //加入 start code
        memcpy(sps_buffer, nalu_start_code, 4);
        //再copy sps数据到buffer
        memcpy(sps_buffer + sps_total_length - sps_size, current_extra_data + 2, sps_size);
        *spsSize = sps_total_length + padding;

        //跳到pps起始处
        current_extra_data += 2 + sps_size;
        __pps:
        //当 SPS 处理完后，开始处理 PPS
        if (!unit_nb && !sps_done++) {
            // number of pps unit(s) ,强行当做只有一个 pps来处理
            unit_nb = *current_extra_data++;
            if (unit_nb) {
                pps_seen = 1;
                pps_size = AV_RB16(current_extra_data);
                pps_total_length += pps_size + 4; //加上4字节的h264 header, 即 0001
                if (av_reallocp(&pps_buffer, pps_total_length + padding) < 0) {
                    return;
                }
                //加入 start code
                memcpy(pps_buffer, nalu_start_code, 4);
                //再copy pps数据到buffer
                memcpy(pps_buffer + pps_total_length - pps_size, current_extra_data + 2, pps_size);
                *ppsSize = pps_total_length + padding;
            }

        }

    }


    if (!sps_seen)
        av_log(NULL, AV_LOG_WARNING,
               "Warning: SPS NALU missing or invalid. "
               "The resulting stream may not play.\n");

    if (!pps_seen)
        av_log(NULL, AV_LOG_WARNING,
               "Warning: PPS NALU missing or invalid. "
               "The resulting stream may not play.\n");

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


void *OpenResource(void *res) {
    char *url = (char *) res;
    if (!playerInfo || !playerInfo->inputContext) {
        LOGE("playerInfo or it's inputContext is NULL");
        return (void *) PLAYER_RESULT_ERROR;
    }
    AVFormatContext *fmt_ctx = playerInfo->inputContext;
    //打开多媒体文件，根据文件后缀名解析,第三个参数是显式制定文件类型，当文件后缀与文件格式不符
    //或者根本没有后缀时需要填写，
    is_under_analysis_resource = 1;
    LOGD("avformat_open_input start,url=%s", url);
    int ret = avformat_open_input(&fmt_ctx, url, NULL, NULL);
    if (ret == 0) {
        /* find stream info  */
        LOGD("----------open resource success!,start to find stream info-------------");
        if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
            LOGE("could not find stream info\n");
            fmt_ctx = NULL;
            is_under_analysis_resource = 0;
            return (void *) PLAYER_RESULT_ERROR;
        }

        LOGD("----------find stream info success!-------------");
        playerInfo->resource = url;

        LOGD("----------fmt_ctx:streams number=%d,bit rate=%lld\n", fmt_ctx->nb_streams,
             fmt_ctx->bit_rate);

        //输出多媒体文件信息,第二个参数是流的索引值（默认0），第三个参数，0:输入流，1:输出流
        //    av_dump_format(fmt_ctx, 0, url, 0);

        playerInfo->inputVideoStream = findVideoStream(playerInfo->inputContext);
        if (!playerInfo->inputVideoStream) {
            playerInfo->SetPlayState(ERROR);
            is_under_analysis_resource = 0;
            return (void *) PLAYER_RESULT_ERROR;
        }

        if (playerInfo->inputVideoStream->codecpar->codec_id != AV_CODEC_ID_H264) {
            LOGE("sorry this player only support h.264 now!");
            playerInfo->SetPlayState(ERROR);
            is_under_analysis_resource = 0;
            return (void *) PLAYER_RESULT_ERROR;
        }

        AVCodecParameters *codecpar = NULL;
        codecpar = playerInfo->inputVideoStream->codecpar;
        if (!codecpar) {
            playerInfo->SetPlayState(ERROR);
            is_under_analysis_resource = 0;
            LOGE("can't get video stream params\n");
            return (void *) PLAYER_RESULT_ERROR;
        }
        if (!playerInfo->window || !playerInfo->windowHeight * playerInfo->windowHeight) {
            playerInfo->SetPlayState(ERROR);
            LOGE("configure error!");
            is_under_analysis_resource = 0;
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
            LOGI("createAMediaCodec success!");
            playerInfo->SetPlayState(PREPARED);
        } else {
            LOGE("AMediaCodec is NULL");
            is_under_analysis_resource = 0;
            playerInfo->SetPlayState(ERROR);
            return (void *) PLAYER_RESULT_ERROR;
        }
    } else {
        is_under_analysis_resource = 0;
        fmt_ctx = NULL;
        LOGE("can't open source: %s msg:%s \n", url, av_err2str(ret));
        return (void *) PLAYER_RESULT_ERROR;
    }
    is_under_analysis_resource = 0;
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


void *RecordPkt(void *) {
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
        AVCodecParameters *i_av_codec_parameters = playerInfo->inputVideoStream->codecpar;
        recorderInfo->o_video_stream = avformat_new_stream(recorderInfo->o_fmt_ctx, NULL);
        if (!recorderInfo->o_video_stream) {
            return (void *) PLAYER_RESULT_ERROR;
        }

        recorderInfo->o_video_stream->codecpar->codec_tag = 0;
        int ret = avcodec_parameters_copy(recorderInfo->o_video_stream->codecpar,
                                          i_av_codec_parameters);
        if (ret < 0) {
            recorderInfo->SetRecordState(RECORD_ERROR);
            LOGE("Failed to copy codec parameters\n");
            return (void *) PLAYER_RESULT_ERROR;
        }
        /*open file  pb:AVIOContext*/
        if (avio_open(&recorderInfo->o_fmt_ctx->pb, file, AVIO_FLAG_WRITE) < 0) {
            LOGE("open file ERROR");
            recorderInfo->SetRecordState(RECORD_ERROR);
            return (void *) PLAYER_RESULT_ERROR;
        }
        /*write file header*/
        if (avformat_write_header(recorderInfo->o_fmt_ctx, NULL) < 0) {
            LOGE("write file header ERROR");
            recorderInfo->SetRecordState(RECORD_ERROR);
            return (void *) PLAYER_RESULT_ERROR;
        }
        LOGI("--------------record prepared----------------");
        recorderInfo->SetRecordState(RECORD_PREPARED);
    }
    AVPacket *packet = av_packet_alloc();
    while (true) {
        RecordState recordState = recorderInfo->GetRecordState();
        if (playerInfo->GetPlayState() == STOPPED || recordState == RECORD_STOP) {
            /*write file trailer*/
            LOGD("--------- recordState changed to stop ---------");
            av_write_trailer(recorderInfo->o_fmt_ctx);
            avformat_free_context(recorderInfo->o_fmt_ctx);
            recorderInfo->o_fmt_ctx = NULL;
            LOGI("--------- record stop ,and write trailer over ---------");
            break;
        } else if (recordState == RECORD_PAUSE) {
            continue;
        }
        int ret = recorderInfo->packetQueue.getAvPacket(packet);
        if (ret == PLAYER_RESULT_ERROR) {
            continue;
        }
        //---------------write pkt data to file-----------
        /*write packet and auto free packet*/
        //等待关键帧或sps、pps的出现
        if (recorderInfo->GetRecordState() == RECORD_PREPARED) {
            int type = GetNALUType(packet);
            if (type == 0x65 || type == 0x67) {
                recorderInfo->SetRecordState(RECORDING);
                LOGI("use idr frame in recorderInfo");
            } else if (playerInfo) {
                AVPacket *keyFrame = playerInfo->lastKeyFrame;
                if (keyFrame && keyFrame->size) {
                    av_interleaved_write_frame(recorderInfo->o_fmt_ctx, keyFrame);
                    recorderInfo->SetRecordState(RECORDING);
                    LOGI("use cache key frame in playerInfo");
                }
            }

            if (recorderInfo->GetRecordState() == RECORDING) {
                LOGI("-----------------real start recording--------------");
            }
        }
        if (recorderInfo->GetRecordState() == RECORDING) {
            if (isDebug) {
                int type = GetNALUType(packet);
                LOGD("record pkt data type= %02x", type);
            }
            av_interleaved_write_frame(recorderInfo->o_fmt_ctx, packet);
            av_packet_unref(packet);
        }
    }
    //释放资源
    delete recorderInfo;
    recorderInfo = NULL;
    LOGI("----------------- record work stop --------------");
    return (void *) PLAYER_RESULT_OK;
}


void *Decode(void *param) {
    AMediaCodec *codec = playerInfo->AMediaCodec;
    AVPacket *packet = av_packet_alloc();
    while (playerInfo->GetPlayState() == STARTED) {
        int ret = playerInfo->packetQueue.getAvPacket(packet);
        if (ret == PLAYER_RESULT_OK) {
//            LOGD("start to decode data,size=%d", packet->size);
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
                //   LOGE("video no output buffer right now");
            } else {
                LOGE("unexpected info code: %zd", status);
            }
        }
    }
    LOGD("-------Decode over!---------");
    return NULL;
}


int ProcessPacket(AVPacket *packet, AVCodecParameters *codecpar) {
    int buf_size = packet->size;
    const uint8_t *buf = packet->data;
    static int ptk_index = 1;
    //  LOGI("------- ProcessPacket -pkt index--------%d]\n", ptk_index++);
//    for (int k = 0; k < buf_size; ++k) {
//        LOGI("pkt data k=%d,data= %02x", k, *(buf + k));
//    }
    if (buf_size == 0) {
        return PLAYER_RESULT_ERROR;
    }
    //检查是否有起始码,如果有说明是标准的H.264数据
    if (buf_size > 5) {
        int nalu_type = GetNALUType(packet);
        PlayState playState = playerInfo->GetPlayState();
        if (nalu_type == 0x65) {
            if (playState != STOPPED) {
                if (!playerInfo->lastKeyFrame) {
                    playerInfo->lastKeyFrame = av_packet_alloc();
                }
                playerInfo->lastKeyFrame = av_packet_clone(packet);
            }
        }

        if (playState != STOPPED) {
            RecordState state = recorderInfo->GetRecordState();
            if (state == RECORDING || state == RECORD_PREPARED || state == RECORD_START) {
                AVPacket *copy = NULL;
                copy = av_packet_clone(packet);
                if (copy) {
                    recorderInfo->packetQueue.putAvPacket(copy);
                } else {
                    LOGE("ProcessPacket clone packet fail!");
                }
            }
        }
        //加入解码队列
        if (playState != STOPPED) {
            playerInfo->packetQueue.putAvPacket(packet);
        }
        return PLAYER_RESULT_OK;
    }
    LOGW("ProcessPacket pkt is not a full h.264 pkt!");
    int len, i, ret;
    uint8_t unit_type;
    int32_t nal_size;
    const uint8_t *buf_end;
    AVPacket *out = NULL;
    out = av_packet_alloc();
    AVPacket spspps_pkt;
    if (!buf) {
        return PLAYER_RESULT_ERROR;
    }
    buf_end = packet->data + buf_size;
    //因为每个视频帧的前 4 个字节是视频帧的长度
    //如果buf中的数据都不能满足4字节，所以后面就没有必要再进行处理了
    if (buf + 4 > buf_end)
        return PLAYER_RESULT_ERROR;

    //将前四字节转换成整型,也就是取出视频帧长度
    for (nal_size = 0, i = 0; i < 4; i++)
        nal_size = (nal_size << 8) | buf[i];

    //跳过4字节（也就是视频帧长度），从而指向真正的视频帧数据
    buf += 4;
    //视频帧的第一个字节里有NAL TYPE
    unit_type = *buf & 0x1f;
    //如果视频帧长度大于从 AVPacket 中读到的数据大小，说明这个数据包肯定是出错了
    if (nal_size > buf_end - buf || nal_size < 0)
        return PLAYER_RESULT_ERROR;

    if (unit_type == 5) {
        //在每个I帧之前都加 SPS/PPS
        h264_extra_data_to_annexb(codecpar->extradata,
                                  codecpar->extradata_size,
                                  &spspps_pkt,
                                  AV_INPUT_BUFFER_PADDING_SIZE);
        if ((ret = set_start_code(out,
                                  spspps_pkt.data, spspps_pkt.size,
                                  buf, nal_size)) < 0) { return PLAYER_RESULT_ERROR; }
    } else {
        //在每个P帧之前都加NALU start code
        if ((ret = set_start_code(out, NULL, 0, buf, nal_size)) < 0) {
            return PLAYER_RESULT_ERROR;
        }
    }

    //在每个帧之前都加NALU start code
    if (set_start_code(out, NULL, 0, buf, buf_size) < 0) {
        return PLAYER_RESULT_ERROR;
    }
    //-------------add data to queue------------
    playerInfo->packetQueue.putAvPacket(out);
    return PLAYER_RESULT_ERROR;
}


void StartDecodeThread() {
    LOGI("Start decode thread");
    pthread_create(&playerInfo->decode_thread, NULL, Decode, NULL);
    pthread_setname_np(playerInfo->decode_thread, "decode_thread");
    pthread_detach(playerInfo->decode_thread);
}

void StartRecorderThread() {
    LOGI("Start recorder thread");
    pthread_create(&recorderInfo->recorder_thread, NULL, RecordPkt, NULL);
    pthread_setname_np(recorderInfo->recorder_thread, "recorder_thread");
    pthread_detach(recorderInfo->recorder_thread);
}

void *DeMux(void *param) {
    if (!playerInfo) {
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
    StartDecodeThread();
    AVCodecParameters *i_av_codec_parameters = i_video_stream->codecpar;
    int video_stream_index = i_video_stream->index;
    PlayState state;
    while ((state = playerInfo->GetPlayState()) != STOPPED) {
        if (state == PAUSE) {
            continue;
        }
        if (playerInfo->GetPlayState() == STOPPED) {
            LOGD("DeMux() stop!");
            return NULL;
        }
        AVPacket *i_pkt = av_packet_alloc();
        if (!i_pkt || !playerInfo->inputContext) {
            return NULL;
        }
        if (playerInfo && av_read_frame(playerInfo->inputContext, i_pkt) == 0) {
            if (i_pkt->stream_index == video_stream_index) {
                ProcessPacket(i_pkt, i_av_codec_parameters);
            }
        }
    }
    LOGD("DeMux() stop!");
    /*
    * pts and dts should increase monotonically
    * pts should be >= dts
    * AV_PKT_FLAG_KEY     0x0001 ///< The packet contains a keyframe
    */
    static int num = 1;
    //释放资源
    delete playerInfo;
    playerInfo = NULL;
    return NULL;
}


void StartDeMuxThread() {
    LOGI("start deMux thread");
    pthread_create(&playerInfo->deMux_thread, NULL, DeMux, NULL);
    pthread_setname_np(playerInfo->deMux_thread, "deMux_thread");
    pthread_detach(playerInfo->deMux_thread);
}

void StartOpenResourceThread(char *res) {
    LOGD("start open resource thread");
    pthread_create(&playerInfo->open_resource_thread, NULL, OpenResource, res);
    pthread_setname_np(playerInfo->open_resource_thread, "open_resource_thread");
    pthread_detach(playerInfo->open_resource_thread);
}

int InitPlayerInfo() {
    LOGD("init player info");
    if (!playerInfo) {
        playerInfo = new PlayerInfo;
    } else {
        LOGE("InitPlayerInfo() error,playerInfo is not NULL,it may be inited!");
        return PLAYER_RESULT_ERROR;
    }

    if (playerInfo->GetPlayState() == INITIALIZED) {
        return PLAYER_RESULT_OK;
    }

    playerInfo->SetPlayState(INITIALIZED);

    playerInfo->inputContext = avformat_alloc_context();
    if (!playerInfo->inputContext) {
        return PLAYER_RESULT_ERROR;
    }
    LOGD("init player info over");
    return PLAYER_RESULT_OK;
}


void SetDebug(bool debug) {
    LOGD("SetDebug() called with %d", debug);
    isDebug = debug;
}

int SetResource(char *resource) {
    LOGD("---------SetResource() called with:resource=%s\n", resource);
    if (InitPlayerInfo()) {
        playerInfo->resource = resource;
    } else {
        LOGE("init player info ERROR");
        return PLAYER_RESULT_ERROR;
    }
    LOGD("SetResource() OK");
    return PLAYER_RESULT_OK;
}


int Configure(char *dest, ANativeWindow *window, int w, int h) {
    LOGD("----------Configure() called with: w=%d,h=%d", w, h);
    if (!playerInfo) {
        LOGE("player info not init !");
    }
    if (is_under_analysis_resource) {
        LOGE("has resource under analysis!");
        return PLAYER_RESULT_ERROR;
    }

    if (dest) {
        recorderInfo = new RecorderInfo;
        recorderInfo->storeFile = dest;
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

int OnWindowChange(ANativeWindow *window, int w, int h) {
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
            LOGI("--------OnWindowChange() success! ");
            playerInfo->SetPlayState(STARTED);
            StartDecodeThread();
        }
    } else {
        LOGE("player not init or it not pause");
        return PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_OK;
}


void SetStateChangeListener(void (*listener)(PlayState)) {
    playerInfo->SetStateListener(listener);
}


int Play() {
    LOGI("--------Play()  called-------");
    if (playerInfo->GetPlayState() == PAUSE) {
        playerInfo->SetPlayState(STARTED);
        return PLAYER_RESULT_OK;
    }
    if (playerInfo->GetPlayState() != PREPARED) {
        LOGE("player not PREPARED!\n");
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

    StartDeMuxThread();
    return PLAYER_RESULT_OK;
}


int Pause(int delay) {
    LOGI("--------Pause()  called-------");
    if (playerInfo->GetPlayState() != STARTED) {
        LOGE("--------Pause()  called-,fail player not started------");
        return PLAYER_RESULT_ERROR;
    }
    playerInfo->SetPlayState(PAUSE);
    return PLAYER_RESULT_OK;
}


int Stop() {
    LOGI("--------Stop()  called-------");
    if (playerInfo->GetPlayState() != STARTED) {
        LOGE("playerInfo is NULL");
        return PLAYER_RESULT_ERROR;
    }
    playerInfo->SetPlayState(STOPPED);
    //停止录制
    StopRecord();
    AMediaCodec_stop(playerInfo->AMediaCodec);
    LOGD("--------Stop Over------");
    return PLAYER_RESULT_OK;
}


int StartRecord() {
    LOGD("------StartRecord() called");
    if (!playerInfo || !recorderInfo) {
        LOGE("------StartRecord() player or recorder not init");
        return PLAYER_RESULT_ERROR;
    }
    if (playerInfo->GetPlayState() != STARTED) {
        LOGE("------StartRecord() player not start");
        return PLAYER_RESULT_ERROR;
    }

    if (recorderInfo->GetRecordState() == RECORDING) {
        LOGE("------StartRecord() recorder is recording!");
        return PLAYER_RESULT_ERROR;
    }
    recorderInfo->SetRecordState(RECORD_START);
    StartRecorderThread();
    return PLAYER_RESULT_OK;
}

int StopRecord() {
    LOGI("------StopRecord() called------");
    if (recorderInfo &&
        recorderInfo->GetRecordState() >= RECORD_START) {
        recorderInfo->SetRecordState(RECORD_STOP);
        return PLAYER_RESULT_OK;
    } else {
        return PLAYER_RESULT_ERROR;
    }
}


int PauseRecord() {
    if (recorderInfo) {
        recorderInfo->SetRecordState(RECORD_PAUSE);
        return PLAYER_RESULT_OK;
    } else {
        return PLAYER_RESULT_ERROR;
    }
}


int Release() {
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
    return PLAYER_RESULT_OK;
}


