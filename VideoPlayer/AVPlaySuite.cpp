#include "stdafx.h"

#include "AVPlaySuite.h"

#include <SDL.h>

int EncodeThread(void* params);

AVInputSuite::AVInputSuite()
{
	m_pFormatCtx = NULL;

	m_pVideoCodec = NULL;
	m_pVideoCodecCtx = NULL;

	m_pAudioCodec = NULL;
	m_pAudioCodecCtx = NULL;

	m_nVideoStream = -1;
	m_nAudioStream = -1;

	m_bIsInputOpen = FALSE;
}


AVInputSuite::~AVInputSuite()
{
	Close();
}


BOOL AVInputSuite::OpenCameraInput()
{
	if (m_bIsInputOpen)
		return FALSE;

	AVInputFormat *pInputFormat = av_find_input_format("dshow");

	int i;
	for (i = 0; i < 10; i++)
	{
		//重复10次来提高打开摄像头成功率
		if (avformat_open_input(&m_pFormatCtx, "video=Integrated Webcam", pInputFormat, NULL) == 0)
			break;
	}

	if (i == 10)
	{
		Log("Error : Couldn't open the camera\n");
		goto OpenCameraInputFailed;
	}

	if (!SetStreamAndCodec())
	{
		goto OpenCameraInputFailed;
	}

	m_bIsInputOpen = TRUE;

	return TRUE;

OpenCameraInputFailed:
	CloseInAndReleaseAllMemory();
	Close();
	return FALSE;
}


BOOL AVInputSuite::OpenFileInput(const char *filepath)
{
	if (m_bIsInputOpen)
		return FALSE;

	if (avformat_open_input(&m_pFormatCtx, filepath, NULL, NULL) != 0)
	{
		Log("Error : Couldn't open the file  ");
		Log(filepath);
		Log();
		goto OpenFileInputFailed;
	}

	if (!SetStreamAndCodec())
	{
		goto OpenFileInputFailed;
	}

	m_bIsInputOpen = TRUE;

	return TRUE;

OpenFileInputFailed:
	CloseInAndReleaseAllMemory();

	
	return FALSE;
}


BOOL AVInputSuite::SetStreamAndCodec()
{
	if (avformat_find_stream_info(m_pFormatCtx, NULL) < 0)
	{
		Log("Error : Couldn't find stream info ");
		Log(m_pFormatCtx->filename);
		Log("\n");
		return FALSE;
	}


	for (unsigned int i = 0; i < m_pFormatCtx->nb_streams; i++)
	{
		if (m_pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && m_nVideoStream == -1)
			m_nVideoStream = i;
		else if (m_pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO && m_nAudioStream == -1)
			m_nAudioStream = i;
	}

	if (m_nVideoStream != -1)
	{
		m_pVideoCodecCtx = m_pFormatCtx->streams[m_nVideoStream]->codec;
		m_pVideoCodec = avcodec_find_decoder(m_pVideoCodecCtx->codec_id);

		Log("Video Stream", m_nVideoStream);
		Log("Sample aspect ratio", m_pFormatCtx->streams[m_nVideoStream]->sample_aspect_ratio.num, m_pFormatCtx->streams[m_nVideoStream]->sample_aspect_ratio.den);
		Log("time_base", m_pFormatCtx->streams[m_nVideoStream]->time_base.num, m_pFormatCtx->streams[m_nVideoStream]->time_base.den);
		Log("nb_frames", m_pFormatCtx->streams[m_nVideoStream]->nb_frames);
		Log("avg_frame_rate", m_pFormatCtx->streams[m_nVideoStream]->avg_frame_rate.num, m_pFormatCtx->streams[m_nVideoStream]->avg_frame_rate.den);
		Log();

		if (m_pVideoCodec == NULL)
		{
			Log("Error : Couldn't find the video decoder\n");
			goto SetStreamAndCodecFailed;
		}

		if (avcodec_open2(m_pVideoCodecCtx, m_pVideoCodec, NULL) < 0)
		{
			Log("Error : Couldn't open the video decoder\n");
			goto SetStreamAndCodecFailed;
		}

		Log("Video Codec : ");
		Log(m_pVideoCodec->name);
		Log();

		Log("Video CodecContext : ");
		Log("width", m_pVideoCodecCtx->width);
		Log("height", m_pVideoCodecCtx->height);
		Log("time_base", m_pVideoCodecCtx->time_base.num, m_pVideoCodecCtx->time_base.den);
		Log();
	}

	if (m_nAudioStream != -1)
	{
		Log("Audio Stream", m_nAudioStream);
		Log("Sample aspect ratio", m_pFormatCtx->streams[m_nAudioStream]->sample_aspect_ratio.num, m_pFormatCtx->streams[m_nAudioStream]->sample_aspect_ratio.den);
		Log("time_base", m_pFormatCtx->streams[m_nAudioStream]->time_base.num, m_pFormatCtx->streams[m_nAudioStream]->time_base.den);
		Log("nb_frames", m_pFormatCtx->streams[m_nAudioStream]->nb_frames);
		Log("avg_frame_rate", m_pFormatCtx->streams[m_nAudioStream]->avg_frame_rate.num, m_pFormatCtx->streams[m_nAudioStream]->avg_frame_rate.den);
		Log();

		m_pAudioCodecCtx = m_pFormatCtx->streams[m_nAudioStream]->codec;
		m_pAudioCodec = avcodec_find_decoder(m_pAudioCodecCtx->codec_id);

		if (m_pAudioCodec == NULL)
		{
			Log("Error : Couldn't find the audio decoder\n");
			goto SetStreamAndCodecFailed;
		}

		if (avcodec_open2(m_pAudioCodecCtx, m_pAudioCodec, NULL) < 0)
		{
			Log("Error : Couldn't open the audio decoder\n");
			goto SetStreamAndCodecFailed;
		}

		Log("Audio Codec : ");
		Log("name : ");
		Log(m_pAudioCodec->name);
		Log();

		Log("Audio CodecContext : ");
		Log("channels", m_pAudioCodecCtx->channels);
		Log(" format : ");
		Log(av_get_sample_fmt_name(m_pAudioCodecCtx->sample_fmt));
		Log(" ");
		Log("sample_rate", m_pAudioCodecCtx->sample_rate);
		Log("time_base", m_pAudioCodecCtx->time_base.num, m_pAudioCodecCtx->time_base.den);
		Log();
	}

	return TRUE;

SetStreamAndCodecFailed:

	return FALSE;
}


int AVInputSuite::GetPacket(AVPacket *pkt)
{
	if (!m_bIsInputOpen)
		return FALSE;

	if (av_read_frame(m_pFormatCtx, pkt) >= 0)
	{
		if (IsVideoPacket(pkt))
		{
			return VIDEO_PACKET;
		}
		else if (IsAudioPacket(pkt))
		{
			return AUDIO_PACKET;
		}
		else
		{
			return OTHER_PACKET;
		}
	}
	else
	{
		return NONE_PACKET;
	}
}

BOOL AVInputSuite::DecodeVideoFrame(AVPacket *pkt, AVFrame *pFrame)
{
	if (!HasVideoStream())
		return FALSE;
	//int64_t starttime = av_gettime();

	//Log("Read Frame", av_gettime() - starttime);
	//Log();

	int got_picture;

	int ret = avcodec_decode_video2(m_pVideoCodecCtx, pFrame, &got_picture, pkt);

	//Log("Decode Frame", av_gettime() - starttime);
	//Log();

	//av_free_packet(pkt);

	if (ret < 0)
	{
		Log("Error : avcodec_decode_video returns a negative value\n");
		return FALSE;
	}

	if (got_picture == 1)
	{
		return TRUE;
	}

	return FALSE;
			/*{*/
				//Log("decode video frame : ");

				//Log("pts", pFrame->pts);
				//Log();

}


int AVInputSuite::DecodeAudioFrame(AVPacket *pkt, AVFrame *pFrame, int *got_frame)
{
	//if (!HasAudioStream())
	//{
	//	return FALSE;
	//}

	return avcodec_decode_audio4(m_pAudioCodecCtx, pFrame, got_frame, pkt);
	/*int got_frame;

	int ret = avcodec_decode_audio4(m_pAudioCodecCtx, pFrame, &got_frame, pkt);

	if (ret < 0)
	{
		Log("Error : The return value of avcodec_decode_audio4 is negative\n");
		return FALSE;
	}

	if (got_frame == 1)
	{
		return TRUE;
	}

	return FALSE;*/
}


int AVInputSuite::AudioResampling(AVFrame *pAudioDecodeFrame, AVSampleFormat out_sample_fmt, int64_t out_channels, int out_sample_rate, uint8_t* out_buf)
{
	SwrContext * swr_ctx = NULL;
	int data_size = 0;
	int ret = 0;
	int64_t src_ch_layout = m_pAudioCodecCtx->channel_layout;
	int64_t dst_ch_layout = AV_CH_LAYOUT_STEREO;
	int dst_nb_channels = 0;
	int dst_linesize = 0;
	int src_nb_samples = 0;
	int dst_nb_samples = 0;
	int max_dst_nb_samples = 0;
	uint8_t **dst_data = NULL;
	int resampled_data_size = 0;

	swr_ctx = swr_alloc();
	if (!swr_ctx)
	{
		printf("swr_alloc error \n");
		goto AudioResamplingEnd;
	}

	src_ch_layout = (m_pAudioCodecCtx->channels ==
		av_get_channel_layout_nb_channels(m_pAudioCodecCtx->channel_layout)) ?
		m_pAudioCodecCtx->channel_layout :
		av_get_default_channel_layout(m_pAudioCodecCtx->channels);

	if (out_channels == 1)
	{
		dst_ch_layout = AV_CH_LAYOUT_MONO;
		//printf("dst_ch_layout: AV_CH_LAYOUT_MONO\n");
	}
	else if (out_channels == 2)
	{
		dst_ch_layout = AV_CH_LAYOUT_STEREO;
		//printf("dst_ch_layout: AV_CH_LAYOUT_STEREO\n");
	}
	else
	{
		dst_ch_layout = AV_CH_LAYOUT_SURROUND;
		//printf("dst_ch_layout: AV_CH_LAYOUT_SURROUND\n");
	}

	if (src_ch_layout <= 0)
	{
		printf("src_ch_layout error \n");
		goto AudioResamplingEnd;
	}

	src_nb_samples = pAudioDecodeFrame->nb_samples;
	if (src_nb_samples <= 0)
	{
		printf("src_nb_samples error \n");
		goto AudioResamplingEnd;
	}

	av_opt_set_int(swr_ctx, "in_channel_layout", src_ch_layout, 0);
	av_opt_set_int(swr_ctx, "in_sample_rate", m_pAudioCodecCtx->sample_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", m_pAudioCodecCtx->sample_fmt, 0);

	av_opt_set_int(swr_ctx, "out_channel_layout", dst_ch_layout, 0);
	av_opt_set_int(swr_ctx, "out_sample_rate", out_sample_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", (AVSampleFormat)out_sample_fmt, 0);

	if ((ret = swr_init(swr_ctx)) < 0) 
	{
		printf("Failed to initialize the resampling context\n");
		goto AudioResamplingEnd;
	}

	max_dst_nb_samples = dst_nb_samples = av_rescale_rnd(src_nb_samples,
		out_sample_rate, m_pAudioCodecCtx->sample_rate, AV_ROUND_UP);
	if (max_dst_nb_samples <= 0)
	{
		printf("av_rescale_rnd error \n");
		goto AudioResamplingEnd;
	}

	dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
	ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
		dst_nb_samples, (AVSampleFormat)out_sample_fmt, 0);
	if (ret < 0)
	{
		printf("av_samples_alloc_array_and_samples error \n");
		goto AudioResamplingEnd;
	}


	dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, m_pAudioCodecCtx->sample_rate) +
		src_nb_samples, out_sample_rate, m_pAudioCodecCtx->sample_rate, AV_ROUND_UP);
	if (dst_nb_samples <= 0)
	{
		printf("av_rescale_rnd error \n");
		goto AudioResamplingEnd;
	}
	if (dst_nb_samples > max_dst_nb_samples)
	{
		av_free(dst_data[0]);
		ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
			dst_nb_samples, (AVSampleFormat)out_sample_fmt, 1);
		max_dst_nb_samples = dst_nb_samples;
	}

	if (swr_ctx)
	{
		ret = swr_convert(swr_ctx, dst_data, dst_nb_samples,
			(const uint8_t **)pAudioDecodeFrame->data, pAudioDecodeFrame->nb_samples);
		if (ret < 0)
		{
			printf("swr_convert error \n");
			goto AudioResamplingEnd;
		}

		resampled_data_size = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
			ret, (AVSampleFormat)out_sample_fmt, 1);
		if (resampled_data_size < 0)
		{
			printf("av_samples_get_buffer_size error \n");
			goto AudioResamplingEnd;
		}
	}
	else
	{
		printf("swr_ctx null error \n");
		goto AudioResamplingEnd;
	}

	memcpy(out_buf, dst_data[0], resampled_data_size);

AudioResamplingEnd:

	if (dst_data)
	{
		av_freep(&dst_data[0]);
	}
	av_freep(&dst_data);
	dst_data = NULL;

	if (swr_ctx)
	{
		swr_free(&swr_ctx);
	}
	return resampled_data_size;
}


BOOL AVInputSuite::IsVideoPacket(AVPacket *pkt)
{
	return HasVideoStream() ? pkt->stream_index == m_nVideoStream : FALSE;
}


BOOL AVInputSuite::IsAudioPacket(AVPacket *pkt)
{
	return HasAudioStream() ? pkt->stream_index == m_nAudioStream : FALSE;
}


BOOL AVInputSuite::HasVideoStream()const
{
	if (!m_bIsInputOpen)
		return FALSE;
	return m_nVideoStream != -1;
}


BOOL AVInputSuite::HasAudioStream()const
{
	if (!m_bIsInputOpen)
		return FALSE;
	return m_nAudioStream != -1;
}



void AVInputSuite::Close()
{
	if (m_bIsInputOpen)
	{
		m_bIsInputOpen = FALSE;
	}

	TRACE(_T("CLOSE AVCODEC\n"));

	CloseInAndReleaseAllMemory();
}


void AVInputSuite::CloseInAndReleaseAllMemory()
{
	if (m_pVideoCodecCtx != NULL)
	{
		avcodec_close(m_pVideoCodecCtx);
		m_pVideoCodecCtx = NULL;
	}
	m_pVideoCodec = NULL;
	m_nVideoStream = -1;

	if (m_pAudioCodecCtx != NULL)
	{
		avcodec_close(m_pAudioCodecCtx);
		m_pAudioCodecCtx = NULL;
	}
	m_pVideoCodec = NULL;
	m_nVideoStream = -1;

	if (m_pFormatCtx != NULL)
	{
		avformat_close_input(&m_pFormatCtx);
		m_pFormatCtx = NULL;
	}

	m_bIsInputOpen = FALSE;
}


int AVInputSuite::GetVideoWidth()const
{
	return HasVideoStream() ? m_pVideoCodecCtx->width : 0;
}


int AVInputSuite::GetVideoHeight()const
{
	return HasVideoStream() ? m_pVideoCodecCtx->height : 0;
}


AVPixelFormat AVInputSuite::GetVideoPixFmt()const
{
	return HasVideoStream() ? m_pVideoCodecCtx->pix_fmt : AVPixelFormat::AV_PIX_FMT_NONE;
}


BOOL AVInputSuite::IsInputOpen()const
{
	return m_bIsInputOpen;
}


AVOutputSuite::AVOutputSuite()
{
	m_bIsOutputOpen = FALSE;
	m_pFormatCtx = NULL;

	m_pVideoCodecCtx = NULL;
	m_pVideoCodec = NULL;
	m_pVideoStream = NULL;
	m_pVideoBuffer = NULL; 
	m_pVideoSwsCtx = NULL;
	m_pVideoFrame = NULL;

	m_pAudioCodecCtx = NULL;
	m_pAudioCodec = NULL;
	m_pAudioStream = NULL;
	m_pAudioFrame = NULL;
	m_pAudioBuffer = NULL;
}


AVOutputSuite::~AVOutputSuite()
{

}


BOOL AVOutputSuite::OpenFileOutput(const char *filepath)
{
	if (m_bIsOutputOpen)
		return FALSE;


	m_pFormatCtx = avformat_alloc_context();

	m_pFormatCtx->oformat = av_guess_format(NULL, filepath, NULL);

	sprintf_s(m_pFormatCtx->filename, "%s", filepath);

	//if (avformat_alloc_output_context2(&m_pFormatCtx, NULL, NULL, filepath) < 0
	//	|| m_pFormatCtx == NULL)
	//{
	//	Log("Error : alloc output context failed.\n");
	//	return FALSE;
	//}

	m_bIsOutputOpen = TRUE;

	start_time = av_gettime();

	Log("Open File Output ");
	Log(filepath);
	Log("\n");

	return TRUE;
}


BOOL AVOutputSuite::OpenRTPOutput(const char *rtpurl)
{
	if (m_bIsOutputOpen)
		return FALSE;

	if (avformat_alloc_output_context2(&m_pFormatCtx, NULL, "rtp", rtpurl) < 0)
	{
		Log("Error : alloc output context failed.\n");
		return FALSE;
	}
	if (m_pFormatCtx == NULL)
	{
		Log("Error : alloc output context failed.\n");
		return FALSE;
	}

	m_bIsOutputOpen = TRUE;

	Log("Open RTP Output ");
	Log(rtpurl);
	Log("\n");

	return TRUE;
}


BOOL AVOutputSuite::OpenRTMPOutput(const char *rtmpurl)
{
	if (m_bIsOutputOpen)
		return FALSE;

	if (avformat_alloc_output_context2(&m_pFormatCtx, NULL, "flv", rtmpurl) < 0)
	{
		Log("Error : alloc output context failed.\n");
		return FALSE;
	}
	if (m_pFormatCtx == NULL)
	{
		Log("Error : alloc output context failed.\n");
		return FALSE;
	}

	m_bIsOutputOpen = TRUE;

	Log("Open RTMP Output ");
	Log(rtmpurl);
	Log("\n");

	return TRUE;
}


BOOL AVOutputSuite::OpenVideoStream(int width, int height, AVCodecID id/* = AV_CODEC_ID_H264*/, int bitrate/* = 144000*/)
{
	if (!m_bIsOutputOpen)
	{
		return FALSE;
	}

	m_pVideoCodec = avcodec_find_encoder(id);
	if (m_pVideoCodec == NULL)
	{
		Log("Error : Couldn't find the video encoder.\n");
		return FALSE;
	}
	m_pVideoStream = avformat_new_stream(m_pFormatCtx, m_pVideoCodec);
	if (m_pVideoStream == NULL)
	{
		Log("Error : Couldn't add a video stream.\n");
		return FALSE;
	}

	m_pVideoStream->time_base.den = 24;
	m_pVideoStream->time_base.num = 1;

	//设置编码器参数
	m_pVideoCodecCtx = m_pVideoStream->codec;

	avcodec_get_context_defaults3(m_pVideoCodecCtx, NULL);

	m_pVideoCodecCtx->codec_id = id;
	m_pVideoCodecCtx->pix_fmt = PIX_FMT_YUV420P;
	m_pVideoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;

	m_pVideoCodecCtx->width = width;
	m_pVideoCodecCtx->height = height;
	m_pVideoCodecCtx->time_base.den = 24;					//一秒24帧
	m_pVideoCodecCtx->time_base.num = 1;

	m_pVideoCodecCtx->bit_rate = bitrate;			//码率
	m_pVideoCodecCtx->gop_size = 10;				//每10帧中有一帧I帧
	m_pVideoCodecCtx->max_b_frames = 1;
	m_pVideoCodecCtx->qmin = 10;
	m_pVideoCodecCtx->qmax = 51;

	//av_opt_set(m_pVideoCodecCtx->priv_data, "tune", "zerolatency", 0);
	av_opt_set(m_pVideoCodecCtx->priv_data, "preset", "ultrafast", 0);
	av_opt_set(m_pVideoCodecCtx->priv_data, "tune", "stillimage,fastdecode,zerolatency", 0);
	av_opt_set(m_pVideoCodecCtx->priv_data, "x264opts", "crf=26:vbv-maxrate=728:vbv-bufsize=364:keyint=25", 0);

	if (m_pFormatCtx->flags & AVFMT_GLOBALHEADER)
		m_pVideoCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	if (avcodec_open2(m_pVideoCodecCtx, m_pVideoCodec, NULL) < 0)
	{
		Log("Error : Couldn't open the video codec.\n");
		return FALSE;
	}

	m_pVideoFrame = av_frame_alloc();
	int nSize = avpicture_get_size(PIX_FMT_YUV420P, width, height);
	m_pVideoBuffer = (uint8_t*)av_malloc(nSize);
	ZeroMemory(m_pVideoBuffer, nSize * sizeof(uint8_t));;
	if (avpicture_fill((AVPicture*)m_pVideoFrame, m_pVideoBuffer, PIX_FMT_YUV420P, width, height) < 0)
	{
		Log("Error : Fill picture buffer error.\n");
		return FALSE;
	}

	m_pVideoStream->codec->codec_tag = 0;

	return TRUE;
}


BOOL AVOutputSuite::OpenAudioStream()
{
	if (!m_bIsOutputOpen)
	{
		return FALSE;
	}

	m_pAudioCodec = avcodec_find_encoder(AVCodecID::CODEC_ID_MP3);
	if (m_pAudioCodec == NULL)
	{
		Log("Error : Couldn't find the audio encoder.\n");
		return FALSE;
	}
	m_pAudioStream = avformat_new_stream(m_pFormatCtx, m_pAudioCodec);
	if (m_pAudioStream == NULL)
	{
		Log("Error : Couldn't add a audio stream.\n");
		return FALSE;
	}
	m_pAudioCodecCtx = m_pAudioStream->codec;

	avcodec_get_context_defaults3(m_pAudioCodecCtx, m_pAudioCodec);

	m_pAudioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
	m_pAudioCodecCtx->sample_fmt = AV_SAMPLE_FMT_S32P;
	m_pAudioCodecCtx->sample_rate = 44100;
	m_pAudioCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	m_pAudioCodecCtx->channels = av_get_channel_layout_nb_channels(m_pAudioCodecCtx->channel_layout);
	m_pAudioCodecCtx->bit_rate = 64000;
	m_pAudioCodecCtx->codec_id = CODEC_ID_MP3;
	m_pAudioCodecCtx->time_base.den = 1440;
	m_pAudioCodecCtx->time_base.num = 1;

	if (avcodec_open2(m_pAudioCodecCtx, m_pAudioCodec, NULL) < 0)
	{
		Log("Error : Couldn't open the audio codec.\n");
		return FALSE;
	}

	m_pAudioStream->codec->codec_tag = 0;

	if (m_pFormatCtx->flags & AVFMT_GLOBALHEADER)
		m_pAudioCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
	
	m_pAudioFrame = av_frame_alloc();
	m_pAudioFrame->nb_samples = m_pAudioCodecCtx->frame_size;
	m_pAudioFrame->format = m_pAudioCodecCtx->sample_fmt;

	m_nAudioFrameSize = av_samples_get_buffer_size(NULL, m_pAudioCodecCtx->channels,
		m_pAudioCodecCtx->frame_size, m_pAudioCodecCtx->sample_fmt, 1);

	m_pAudioBuffer = (uint8_t*)av_malloc(m_nAudioFrameSize);
	avcodec_fill_audio_frame(m_pAudioFrame, m_pAudioCodecCtx->channels, m_pAudioCodecCtx->sample_fmt, m_pAudioBuffer, m_nAudioFrameSize, 1);

	m_nAudioFrameIndex = 0;

	return TRUE;
}


BOOL AVOutputSuite::WriteHeader()
{
	av_dump_format(m_pFormatCtx, 0, m_pFormatCtx->filename, 1);

	if (!(m_pFormatCtx->oformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&m_pFormatCtx->pb, m_pFormatCtx->filename, AVIO_FLAG_WRITE) < 0)
		{
			Log("Error : Open Output Failed ");
			Log(m_pFormatCtx->filename);
			Log("\n");
			av_free(m_pFormatCtx);
			m_pFormatCtx = NULL;
			return FALSE;
		}
	}

	if (avformat_write_header(m_pFormatCtx, NULL) < 0)
	{
		Log("Error : write header failed\n");
		return FALSE;
	}

	return TRUE;
}


void AVOutputSuite::CreateSdpFile(const char *sdpurl)
{
	if (m_bIsOutputOpen)
	{
		char buf[1024];
		av_sdp_create(&m_pFormatCtx, 1, buf, 1024);
		
		std::ofstream sdp;
		sdp.open(sdpurl, std::ios::out | std::ios::trunc);
		if (sdp)
		{
			sdp << buf;
		}
		else
		{
			Log("Erorr : open Sdp\n");
		}
		sdp.close();
	}
}


BOOL AVOutputSuite::WriteVideoFrame(AVFrame **ppFrame)
{
	if (!m_bIsOutputOpen)
		return FALSE;
	if (m_pVideoStream == NULL
		|| m_pVideoCodec == NULL)
	{
		return FALSE;
	}

	if ((*ppFrame) != NULL)
	{
		sws_scale(m_pVideoSwsCtx, (*ppFrame)->data, (*ppFrame)->linesize,
			0, m_pVideoCodecCtx->height, m_pVideoFrame->data, m_pVideoFrame->linesize);

		int got_picture = 0;

		AVPacket pkt;
		av_init_packet(&pkt);
		//解码器分配包的大小
		pkt.data = NULL;
		pkt.size = 0;
 
		m_pVideoFrame->pts = m_pVideoStream->codec->frame_number;

		int ret = avcodec_encode_video2(m_pVideoCodecCtx, &pkt, m_pVideoFrame, &got_picture);

		if (ret < 0)
		{
			TRACE(_T("encode error\n"));
			return FALSE;
		}

		if (got_picture == 1)
		{
			pkt.stream_index = m_pVideoStream->index;

			av_packet_rescale_ts(&pkt, m_pVideoCodecCtx->time_base, m_pVideoStream->time_base);

			av_write_frame(m_pFormatCtx, &pkt);
			//TRACE(_T("Write Frame\n"));

			int64_t duration = av_gettime() - start_time;
		}

		av_free_packet(&pkt);
	}

	return TRUE;
}


BOOL AVOutputSuite::WriteAudio(const char *samples, const int size)
{
	if (!m_bIsOutputOpen)
		return FALSE;
	if (m_pAudioStream == NULL
		|| m_pAudioCodec == NULL)
		return FALSE;

	int sample_size = (size / 8) * 8;
	int sample_index = 0;

	while ((sample_size - sample_index + m_nAudioFrameIndex * 2) >= m_nAudioFrameSize)
	{
		for (; m_nAudioFrameIndex < m_nAudioFrameSize / 2;)
		{
			memcpy(m_pAudioBuffer + m_nAudioFrameIndex, samples + sample_index, 4);
			sample_index += 4;
			memcpy(m_pAudioBuffer + m_nAudioFrameIndex + m_nAudioFrameSize / 2, samples + sample_index, 4);
			sample_index += 4;
			m_nAudioFrameIndex += 4;
		}

		AVPacket pkt;
		av_new_packet(&pkt, m_nAudioFrameSize);

		int got_picture;
		int ret = avcodec_encode_audio2(m_pAudioCodecCtx, &pkt, m_pAudioFrame, &got_picture);

		if (ret < 0)
		{
			TRACE(_T("encode audio error.\n"));
			av_free_packet(&pkt);
			return FALSE;
		}

		if (got_picture == 1)
		{
			pkt.stream_index = m_pAudioStream->index;
			av_packet_rescale_ts(&pkt, m_pAudioCodecCtx->time_base, m_pAudioStream->time_base);
			TRACE(_T("Write Audio Frame.\n"));
			av_write_frame(m_pFormatCtx, &pkt);
		}

		m_nAudioFrameIndex = 0;

		av_free_packet(&pkt);
	}

	for (; sample_index < sample_size;)
	{
		memcpy(m_pAudioBuffer + m_nAudioFrameIndex, samples + sample_index, 4);
		sample_index += 4;
		memcpy(m_pAudioBuffer + m_nAudioFrameIndex + m_nAudioFrameSize / 2, samples + sample_index, 4);
		sample_index += 4;
		m_nAudioFrameIndex += 4;
	}

	return TRUE;
}


void AVOutputSuite::SetSwsCtx(const int srcWidth, const int srcHeight, AVPixelFormat srcPixFmt)
{
	TRACE(_T("SetSwsCtx\n"));
	if (!m_bIsOutputOpen)
		return;

	if (m_pVideoSwsCtx != NULL)
	{
		sws_freeContext(m_pVideoSwsCtx);
	}

	m_pVideoSwsCtx = sws_getContext(srcWidth, srcHeight, srcPixFmt, m_pVideoCodecCtx->width, m_pVideoCodecCtx->height, m_pVideoCodecCtx->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);

	//if (m_pVideoSwsCtx == NULL)
	//{
	//	Log("Error : sws_getContext Failed !\n");  
	//	m_bIsOutputOpen = FALSE;
	//}
}


void AVOutputSuite::Close()
{
	if (m_bIsOutputOpen)
	{
		m_bIsOutputOpen = FALSE;
		av_usleep(5000);

		av_write_trailer(m_pFormatCtx);

		av_frame_free(&m_pVideoFrame);
		m_pVideoFrame = NULL;
		av_free(m_pVideoBuffer);
		m_pVideoBuffer = NULL;

		av_frame_free(&m_pAudioFrame);
		m_pAudioFrame = NULL;
		av_free(m_pAudioBuffer);
		m_pAudioBuffer = NULL;
		m_nAudioFrameIndex = 0;
		m_nAudioFrameSize = 0;

		avcodec_close(m_pVideoCodecCtx);
		avcodec_free_context(&m_pVideoCodecCtx);
		m_pVideoCodecCtx = NULL;
		m_pVideoCodec = NULL;

		avcodec_close(m_pAudioCodecCtx);
		avcodec_free_context(&m_pAudioCodecCtx);
		m_pAudioCodecCtx = NULL;
		m_pAudioCodec = NULL;

		avio_close(m_pFormatCtx->pb);
		av_free(m_pFormatCtx);
		m_pFormatCtx = NULL;
	}
}


BOOL AVOutputSuite::IsOutputOpen()
{
	return m_bIsOutputOpen;
}