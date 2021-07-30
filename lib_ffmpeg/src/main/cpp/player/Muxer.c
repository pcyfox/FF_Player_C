//
// Created by LN on 2021/1/13.
//
#include "include/Muxer.h"

#define LOG_TAG "Muxer------------->"


static void
ReleaseResource(AVFormatContext **in_fmt1, AVFormatContext **in_fmt2, AVFormatContext **ou_fmt3) {
    if (in_fmt1 && *in_fmt1) {
        avformat_close_input(in_fmt1);
    }
    if (in_fmt2 && *in_fmt2) {
        avformat_close_input(in_fmt2);
    }
    if (ou_fmt3 && *ou_fmt3) {
        avformat_free_context(*ou_fmt3);
    }
}

/**
                * 时间基(time_base)是时间戳(timestamp)的单位，时间戳值乘以时间基，可以得到实际的时刻值(以秒等为单位)。
                * 时间基可以理解为将1s平均分成若干份，如AV_TIME_BASE=1000000，就是将1s分成1000000份，如果帧率为25，那么每一帧的间隔（也是时间戳的间隔）为：1000000*（1、25）
                * 例如，如果一个视频帧的 dts 是 40，pts 是 160，其 time_base 是 1/1000 秒，那么可以计算出此视频帧的解码时刻是 40 毫秒(40/1000)，显示时刻是 160 毫秒(160/1000)。
                * FFmpeg 中时间戳(pts/dts)的类型是 int64_t 类型，把一个 time_base 看作一个时钟脉冲，则可把 dts/pts 看作时钟脉冲的计数。
                * 三种时间基 tbr、tbn 和 tbc
                * 不同的封装格式具有不同的时间基。在 FFmpeg 处理音视频过程中的不同阶段，也会采用不同的时间基。
                * FFmepg 中有三种时间基，命令行中 tbr、tbn 和 tbc 的打印值就是这三种时间基的倒数：
                * tbn：对应容器中的时间基。值是 AVStream.time_base 的倒数
                * tbc：对应编解码器中的时间基。值是 AVCodecContext.time_base 的倒数
                * tbr：从视频流中猜算得到，可能是帧率或场率(帧率的 2 倍)
                *
                * 除以上三种时间基外，FFmpeg 还有一个内部时间基 AV_TIME_BASE(以及分数形式的 AV_TIME_BASE_Q)
                * AV_TIME_BASE 及 AV_TIME_BASE_Q 用于 FFmpeg 内部函数处理，使用此时间基计算得到时间值表示的是微秒
                *
                * 时间值形式转换
                * av_q2d()将时间从 AVRational 形式转换为 double 形式。AVRational 是分数类型，double 是双精度浮点数类型，转换的结果单位是秒。转换前后的值基于同一时间基，仅仅是数值的表现形式不同而已。
                *
                * av_rescale_q()用于不同时间基的转换，用于将时间值从一种时间基转换为另一种时间基
                * av_packet_rescale_ts()用于将 AVPacket 中各种时间值从一种时间基转换为另一种时间基
                *
                * int64_t av_rescale_rnd(int64_t a, int64_t b, int64_t c, enum AVRounding rnd)
                * 作用是计算 "a * b / c" 的值并分五种方式来取整
                * 将以 "时钟基c" 表示的 数值a 转换成以 "时钟基b" 来表示
                *
                * AVStream.time_base 是 AVPacket 中 pts 和 dts 的时间单位，输入流与输出流中 time_base 按如下方式确定：
                * 对于输入流：打开输入文件后，调用 avformat_find_stream_info()可获取到每个流中的 time_base
                * 对于输出流：打开输出文件后，调用 avformat_write_header()可根据输出文件封装格式确定每个流的 time_base 并写入输出文件中
                * 不同封装格式具有不同的时间基，在转封装(将一种封装格式转换为另一种封装格式)过程中，时间基转换相关代码如下
                * 方式1：
                * av_read_frame(ifmt_ctx, &pkt);
                * pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
                * pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
                * pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
                * 方式2；
                * av_read_frame(ifmt_ctx, &pkt);
                * 将 packet 中的各时间值从输入流封装格式时间基转换到输出流封装格式时间基
                * av_packet_rescale_ts(&pkt, in_stream->time_base, out_stream->time_base);
                * 这里流里的时间基in_stream->time_base和out_stream->time_base，是容器中的时间基，就是 tbn
                *
*/
int MuxAVFile(char *audio_srcPath, char *video_srcPath, char *destPath) {

    LOGIX(LOG_TAG, "start to mux file audio path=%s,video path=%s,dest path=%s", audio_srcPath,
          video_srcPath, destPath);

    AVFormatContext *audio_fmtCtx = NULL, *video_fmtCtx = NULL, *out_fmtCtx = NULL;
    int audio_stream_index = -1;
    int video_stream_index = -1;

    AVStream *audio_in_stream = NULL, *video_in_stream = NULL;
    AVStream *audio_ou_stream = NULL, *video_out_stream = NULL;

    int ret = PLAYER_RESULT_ERROR;
    // 解封装音频文件
    LOGDX(LOG_TAG, "open input audio");
    ret = avformat_open_input(&audio_fmtCtx, audio_srcPath, NULL, NULL);
    if (ret < 0) {
        LOGE("open input audio fail %d", ret);
        return PLAYER_RESULT_ERROR;
    }
    LOGDX(LOG_TAG, "find audio stream info ");
    ret = avformat_find_stream_info(audio_fmtCtx, NULL);
    if (ret < 0) {
        LOGEX(LOG_TAG, "find audio stream info audio fail");
        ReleaseResource(&audio_fmtCtx, NULL, NULL);
        return PLAYER_RESULT_ERROR;
    }
    //找到音频流index
    audio_stream_index = av_find_best_stream(audio_fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    LOGDX(LOG_TAG, "find audio stream index=%d", audio_stream_index);
    if (audio_stream_index < 0) {
        LOGWX(LOG_TAG, "not found audio stream!");
    }

    LOGDX(LOG_TAG, "open input video");
    // 解封装视频文件
    ret = avformat_open_input(&video_fmtCtx, video_srcPath, NULL, NULL);
    if (ret < 0) {
        LOGE("open input video fail %d", ret);
        ReleaseResource(&audio_fmtCtx, &video_fmtCtx, NULL);
        return PLAYER_RESULT_ERROR;
    }

    LOGDX(LOG_TAG, "find video stream info");
    ret = avformat_find_stream_info(video_fmtCtx, NULL);
    if (ret < 0) {
        LOGEX(LOG_TAG, "find video stream_info fail");
        ReleaseResource(&audio_fmtCtx, &video_fmtCtx, NULL);
        return PLAYER_RESULT_ERROR;
    }
    //找到视频流index
    video_stream_index = av_find_best_stream(video_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    LOGDX(LOG_TAG, "find video stream index=%d", video_stream_index);
    if (video_stream_index < 0) {
        LOGEX(LOG_TAG, "not found video stream!");
        return PLAYER_RESULT_ERROR;
    }
    LOGDX(LOG_TAG, "alloc output context by file name");
    // 根据文件后缀名创建封装上下文，用于封装到对应的文件中
    ret = avformat_alloc_output_context2(&out_fmtCtx, NULL, NULL, destPath);
    if (ret < 0) {
        LOGEX(LOG_TAG, "avformat alloc_output_context2 fail %d", ret);
        ReleaseResource(&audio_fmtCtx, &video_fmtCtx, NULL);
        return PLAYER_RESULT_ERROR;
    }

    LOGDX(LOG_TAG, "new audio out stream,and copy codec parameters");
    //获取创建音频输出流,并从输入流中拷贝编码参数
    if (audio_stream_index >= 0) {
        audio_ou_stream = avformat_new_stream(out_fmtCtx, NULL);//创建音频输出流
        audio_in_stream = audio_fmtCtx->streams[audio_stream_index];//获取音频输入流
        //拷贝输入流中的编码信息到输出流中
        ret = avcodec_parameters_copy(audio_ou_stream->codecpar, audio_in_stream->codecpar);
        if (ret < 0) {
            LOGE("copy audio codecpar fail");
        }
    }

    LOGDX(LOG_TAG, "new video out stream,and copy codec parameters");
    if (video_stream_index >= 0) {
        video_out_stream = avformat_new_stream(out_fmtCtx, NULL);
        video_in_stream = video_fmtCtx->streams[video_stream_index];
        if (avcodec_parameters_copy(video_out_stream->codecpar, video_in_stream->codecpar) < 0) {
            LOGEX(LOG_TAG, "copy video codecpar  fail!");
            ReleaseResource(&audio_fmtCtx, &video_fmtCtx, &out_fmtCtx);
            return PLAYER_RESULT_ERROR;
        }
    }

    LOGDX(LOG_TAG, "create and open out file");
    //打开输出文件
    if (!(out_fmtCtx->flags & AVFMT_NOFILE)) {
        if (avio_open(&out_fmtCtx->pb, destPath, AVIO_FLAG_WRITE) < 0) {
            LOGEX(LOG_TAG, "open file fail! file=%s", destPath);
            return PLAYER_RESULT_ERROR;
        }
    }

    // 写入头文件
    if ((ret = avformat_write_header(out_fmtCtx, NULL)) < 0) {
        LOGEX(LOG_TAG, "avformat write header fail %d", ret);
        ReleaseResource(&audio_fmtCtx, &video_fmtCtx, &out_fmtCtx);
        return PLAYER_RESULT_ERROR;
    }

    LOGDX(LOG_TAG, "start to write file header success!");

    AVPacket *audioPacket = av_packet_alloc();
    AVPacket *videoPacket = av_packet_alloc();

    bool audio_finish = false, video_finish = false;
    bool found_audio = false, found_video = false;
    int64_t org_pts = 0;
    static int64_t video_pkt_index = 0;

    LOGDX(LOG_TAG, "real start to mux pkt");
    do {
        //读取音频数据
        if (!audio_finish && !found_audio && audio_stream_index != -1) {
            if (av_read_frame(audio_fmtCtx, audioPacket) < 0) {
                audio_finish = true;
            }
            if (!audio_finish && audioPacket->stream_index != audio_stream_index) {
                av_packet_unref(audioPacket);
                continue;
            }

            if (!audio_finish) {
                found_audio = true;
                audioPacket->stream_index = audio_ou_stream->index;
                /**
                 * 将AVPacket中的时间基表示的pts、dts转换为以输出流中的时间基来表示表示:a * bq / cq,AV_ROUND_UP:四舍五入向上取整
                 */
                audioPacket->pts = av_rescale_q_rnd(org_pts, audio_in_stream->time_base,
                                                    audio_ou_stream->time_base, AV_ROUND_UP);

                audioPacket->dts = audioPacket->pts;
                org_pts += audioPacket->duration;
                audioPacket->duration = av_rescale_q_rnd(audioPacket->duration,
                                                         audio_in_stream->time_base,
                                                         audio_ou_stream->time_base, AV_ROUND_UP);

                audioPacket->pos = -1;

            }
        }

        if (!video_finish && !found_video && video_stream_index != -1) {
            //读取视频数据
            if (av_read_frame(video_fmtCtx, videoPacket) < 0) {
                video_finish = true;
            }

            if (!video_finish && videoPacket->stream_index != video_stream_index) {
                av_packet_unref(videoPacket);
                continue;
            }

            if (!video_finish) {
                found_video = true;
                videoPacket->stream_index = video_out_stream->index;
                if (videoPacket->dts == AV_NOPTS_VALUE) {
                    LOGDX(LOG_TAG, "video pkt not have pts,need calculate by frame rate");
                    AVRational o_time_base = video_out_stream->time_base;
                    int64_t frame_duration =
                            (double) AV_TIME_BASE / av_q2d(video_in_stream->r_frame_rate);
                    //计算出时间基并并转为out stream中对应的时间基，不同的封装格式通常会有不同的时间紧
                    videoPacket->pts = (double) (video_pkt_index * frame_duration) /
                                       (double) (av_q2d(o_time_base) * AV_TIME_BASE);
                    videoPacket->dts = videoPacket->pts;
                    videoPacket->duration =
                            (double) frame_duration / (double) (av_q2d(o_time_base) * AV_TIME_BASE);
                } else {
                    videoPacket->pts = av_rescale_q_rnd(videoPacket->pts,
                                                        video_in_stream->time_base,
                                                        video_out_stream->time_base,
                                                        AV_ROUND_INF);
                    videoPacket->dts = av_rescale_q_rnd(videoPacket->dts,
                                                        video_in_stream->time_base,
                                                        video_out_stream->time_base,
                                                        AV_ROUND_INF);
                    videoPacket->duration = av_rescale_q_rnd(videoPacket->duration,
                                                             video_in_stream->time_base,
                                                             video_out_stream->time_base,
                                                             AV_ROUND_INF);
                    AVRational tb = video_in_stream->time_base;
                }
            }
            video_pkt_index++;
        }

        // 写入数据 视频在前
        if (found_video && found_audio && !video_finish && !audio_finish) {
            if ((ret = av_compare_ts(audioPacket->pts, audio_ou_stream->time_base,
                                     videoPacket->pts, video_out_stream->time_base)) > 0) {
                // 写入视频
                if ((ret = av_write_frame(out_fmtCtx, videoPacket)) < 0) {
                    LOGEX(LOG_TAG, "av_write_frame video  fail %d,pts=%lld", ret,
                          videoPacket->pts);
                    ReleaseResource(&audio_fmtCtx, &video_fmtCtx, &out_fmtCtx);
                    break;
                }
                found_video = false;
                LOGDX(LOG_TAG, "write video pts=%lld", videoPacket->pts);
                av_packet_unref(videoPacket);
            } else {
                // 写入音频
                if ((ret = av_write_frame(out_fmtCtx, audioPacket)) < 0) {
                    LOGEX(LOG_TAG, "av_write_frame audio  fail %d", ret);
                    ReleaseResource(&audio_fmtCtx, &video_fmtCtx, &out_fmtCtx);
                    break;
                }
                found_audio = false;
                LOGDX(LOG_TAG, "write audio pts=%lld", audioPacket->pts);
                av_packet_unref(audioPacket);
            }
        } else if (!video_finish && found_video && !found_audio) {
            // 只有视频
            if ((ret = av_write_frame(out_fmtCtx, videoPacket)) < 0) {
                LOGEX(LOG_TAG, "only write video  fail %d", ret);
                ReleaseResource(&audio_fmtCtx, &video_fmtCtx, &out_fmtCtx);
                break;
            }
            LOGDX(LOG_TAG, "only write video");
            found_video = false;
            av_packet_unref(videoPacket);
        } else if (!found_video && found_audio && !audio_finish) {
            // 只有音频
            if ((ret = av_write_frame(out_fmtCtx, audioPacket)) < 0) {
                LOGEX(LOG_TAG, "only write audio fail %d", ret);
                break;
            }
            LOGDX(LOG_TAG, "only write video");
            found_audio = false;
            av_packet_unref(audioPacket);
        }
    } while (!audio_finish && !video_finish);
    // 结束写入
    if (out_fmtCtx) {
        ret = av_write_trailer(out_fmtCtx);
        LOGIX(LOG_TAG, "write file trailer over!,ret=%d", ret);
    }
    video_pkt_index = 0;
    // 释放内存
    ReleaseResource(&audio_fmtCtx, &video_fmtCtx, &out_fmtCtx);
    LOGDX(LOG_TAG, "mux over,ret=%d, audio path=%s,video path=%s,dest path=%s", ret, audio_srcPath,
          video_srcPath, destPath);
    return ret >= 0 ? PLAYER_RESULT_OK : PLAYER_RESULT_ERROR;
}