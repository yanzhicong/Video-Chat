#pragma once

extern "C"
{
#include <libavdevice\avdevice.h>
#include <libavcodec\avcodec.h>
#include <libswscale\swscale.h>
#include <libavutil\adler32.h>
#include <libavutil\opt.h>
#include <libavutil\time.h>
#include <libavformat\avformat.h>
}

#include "PacketQueue.h"

enum FrameType
{
	FrameTypeAudio,
	FrameTypeVideo,
	FrameTypeData,
	FrameTypeNone,
	FrameTypeSubtitle
};

class AVInputSuite
{
public:
	AVInputSuite();
	virtual ~AVInputSuite();

	BOOL OpenCameraInput();

	BOOL OpenFileInput(const char *filepath);

	BOOL HasVideoStream()const;
	BOOL HasAudioStream()const;

	
#define AUDIO_PACKET 1
#define VIDEO_PACKET 2
#define OTHER_PACKET 3
#define NONE_PACKET 0
	//取包
	int GetPacket(AVPacket *pkt);

	//解码视频帧
	BOOL DecodeVideoFrame(AVPacket *pkt, AVFrame *pFrame);

	//解码音频帧
	int DecodeAudioFrame(AVPacket *pkt, AVFrame *pFrame, int *got_frame);

	//音频重采样
	int AudioResampling(AVFrame *pAudioDecodeFrame,
		AVSampleFormat out_sample_fmt,
		int64_t out_channels,
		int out_sample_rate,
		uint8_t* out_buf);

	BOOL IsVideoPacket(AVPacket *pkt);
	BOOL IsAudioPacket(AVPacket *pkt);

	void Close();

	int GetVideoWidth()const;
	int GetVideoHeight()const;
	AVPixelFormat GetVideoPixFmt()const;

	BOOL IsInputOpen()const;

protected:

	AVFormatContext *m_pFormatCtx;

	int m_nVideoStream;
	AVCodecContext *m_pVideoCodecCtx;
	AVCodec *m_pVideoCodec;

	int m_nAudioStream;
	AVCodecContext *m_pAudioCodecCtx;
	AVCodec *m_pAudioCodec;

	BOOL m_bIsInputOpen;

private:
	AVInputSuite(AVInputSuite&);
	AVInputSuite& operator = (AVInputSuite&);

	CCriticalSection m_CriticalSection;

	BOOL SetStreamAndCodec();
	void CloseInAndReleaseAllMemory();
};


class AVOutputSuite
{
public:
	AVOutputSuite();
	virtual ~AVOutputSuite();

	BOOL OpenFileOutput(const char *filename);

	BOOL OpenRTPOutput(const char *rtpurl);

	BOOL OpenRTMPOutput(const char *rtmpurl);

	BOOL OpenVideoStream(int width, int height, AVCodecID id = AV_CODEC_ID_H264, int bitrate = 144000);

	BOOL OpenAudioStream();

	BOOL WriteHeader();

	BOOL WriteVideoFrame(AVFrame **pFrame);

	BOOL WriteAudio(const char *samples, const int size);

	void WriteAudioFrame(AVFrame **pFrame);

	void SetSwsCtx(const int srcWidth, const int srcHeight, AVPixelFormat srcPixFmt);

	//关闭
	void Close();

	BOOL IsOutputOpen();

	//生成sdp文件
	void CreateSdpFile(const char *sdpurl);

protected:

	AVFormatContext *m_pFormatCtx;

	AVCodecContext *m_pVideoCodecCtx;
	AVCodec *m_pVideoCodec;
	AVStream *m_pVideoStream;
	AVFrame *m_pVideoFrame;
	uint8_t *m_pVideoBuffer;
	SwsContext *m_pVideoSwsCtx;

	AVCodecContext *m_pAudioCodecCtx;
	AVCodec *m_pAudioCodec;
	AVStream *m_pAudioStream;
	AVFrame *m_pAudioFrame;
	uint8_t *m_pAudioBuffer;
	int m_nAudioFrameSize;
	int m_nAudioFrameIndex;


	int frames;
	int64_t start_time;
	int64_t prev_time;

	BOOL m_bIsOutputOpen;

private:
	AVOutputSuite(AVOutputSuite&);
	AVOutputSuite& operator = (AVOutputSuite &);
};

