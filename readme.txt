FFmppeg+MediaCodec+Surface:
功能：（1）播放H264视频并录制 (播放与录制相互独立) （2）录音  （3）音视频合成
支持：http、rtsp、rtmp等
说明：FFmpeg仅用于获取视频流数据及合成视频，MediaCodec负责解码，SurfaceView负责渲染。说有功能都是在Native层实现的


