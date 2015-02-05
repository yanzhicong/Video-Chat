//////////////////////////////////////////////////////////////////////
// TranscodeSectionThread.cpp : 实现文件
//
//	严志聪，武汉大学
//	2015.1.4
//		
//	读取输入流的数据并解码，编码成想要的数据流格式输出，或送到显示屏上显示
//	
//	单线程编解码版本，
//	使用ffmpeg进行编解码，使用pthread做线程同步处理，
//	使用SDL显示，使用wave函数采集和播放声音
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <SDL.h>
#include <vector>
#include <list>
#include <iterator>
#include <mmsystem.h>

extern "C"
{
#include <pthread.h>
}

//#include <omp.h>

#include "TranscodeSectionThread.h"
#include "PacketQueue.h"


#define SDL_AUDIO_BUFFER_SIZE 1152
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

//播放声音队列
std::list<TranscodeSection*> SoundList;
pthread_mutex_t SoundListMtx;

SDL_AudioSpec AudioSpec;
SDL_AudioSpec WantedAudioSpec;

//采集声音队列
std::list<TranscodeSection*> RecorderList;
pthread_mutex_t RecorderListMtx; 

#define BLOCK_SIZE 8192
#define BLOCK_COUNT 20

//CRITICAL_SECTION waveCriticalSection;
WAVEHDR* waveBlocks;
volatile int waveFreeBlockCount;
int waveCurrentBlock;
int waveQuit;
HWAVEIN waveHandle;

BOOL OpenRecorder();
BOOL CloseRecorder();
static WAVEHDR* allocateBlocks(int size, int count);
static void freeBlocks(WAVEHDR* blockArray);
static void CALLBACK waveInProc(HWAVEIN hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);


//在解码线程与编码线程、显示线程之间传送的数据
typedef struct VideoBuffer
{
	AVFrame *pFrame;
	//uint8_t *buffer;
	AVPacket pkt;
	BOOL HasFrame;
}VideoBuffer;


VideoBuffer* newVideoBuffer()
{
	VideoBuffer* data = new VideoBuffer;
	memset(data, 0, sizeof(VideoBuffer));

	data->pFrame = av_frame_alloc();
	av_init_packet(&data->pkt);
	data->HasFrame = FALSE;

	return data;
}


void destroyVideoBuffer(VideoBuffer *data)
{
	av_frame_free(&data->pFrame);
	av_free_packet(&data->pkt);

	delete data;
}


typedef struct AudioBuffer
{
	uint8_t *buffer;
	int bufferindex;

	PacketQueue audioq;

	AVFrame *pFrame;
	BOOL HasFrame;
}AudioBuffer;


AudioBuffer * newAudioBuffer()
{
	AudioBuffer *data = new AudioBuffer;

	data->buffer = new uint8_t[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
	data->bufferindex = 0;

	data->pFrame = av_frame_alloc();
	data->HasFrame = FALSE;

	packet_queue_init(&data->audioq);

	return data;
}


void DestoryAudioBuffer(AudioBuffer *data)
{

}


struct TranscodeSectionParams
{
	//输入
	AVInputSuite *inputSuite;
	//pthread_rwlock_t inputSuiteLock;

	int videoWidth;
	int videoHeight;
	AVPixelFormat videoPixFmt;
	AVRational videoStreamTimebase;
	AVRational videoCodecCtxTimebase;

	VideoBuffer *frame;
	//int frameindex;

	PacketQueue audioq;
	AVFrame *audioFrame;
	BOOL audioFrameDecode;
	
	uint8_t *audioBuffer;
	int audioBufIndex;
	int audioVolumn;

	//输出
	std::vector<AVOutputSuite*> *outputSuite;

#ifdef WIN32
	//视频播放窗口
	SwsContext *pSwsContext;
	AVFrame *windowFrame;
	uint8_t *buffer;
	SDL_Window *window;
	SDL_Renderer *windowRenderer;
	SDL_Texture *windowTextrue;
	SDL_Rect windowRect;

#endif

	BOOL bAudioQuit;
	
	//时间控制
	int64_t start_time;
	int64_t next_time;
	AVRational timebase;

#define SYNC_BY_RT_VIDEO	//synchonize by real time video
#define SYNC_BY_RT_AUDIO		//synchonize by real time audio
#define SYNC_BY_TIMEBASE
	int sync_by;

	//线程
	pthread_t transcodeThread;
	BOOL transcodeThreadQuit;
	pthread_rwlock_t transcodeThreadLock;

};

void* PTW32_CDECL TranscodeSectionThread(void *params);

void ts_initparams(TranscodeSection* p);
void audio_callback(void *userdata, Uint8 *stream, int len);

BOOL ts_Initialize()
{
	av_register_all();
	avcodec_register_all();
	avformat_network_init();
	avdevice_register_all();

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);

	pthread_mutex_init(&SoundListMtx, NULL);
	SDL_LockAudio();

	WantedAudioSpec.freq = 44100;
	WantedAudioSpec.format = AUDIO_S32SYS;
	WantedAudioSpec.channels = 2;
	WantedAudioSpec.silence = FALSE;
	WantedAudioSpec.samples = 1152;
	WantedAudioSpec.callback = audio_callback;
	WantedAudioSpec.userdata = NULL;
	//p->WantedAudioSpec->padding = 52428;

	//Log("Wanted Audio Spec : \n");
	//Log("Sample Rate", WantedAudioSpec.freq);
	//Log("Format", AUDIO_S16SYS);
	//Log("Channels", WantedAudioSpec.channels);
	//Log("samples", 1024);
	//Log();

	if (SDL_OpenAudio(&WantedAudioSpec, &AudioSpec) < 0)
	{
		Log("Error : Open SDL Audio Failed ! \n");
		Log(SDL_GetError());
		Log();
		return FALSE;
	}

	//Log("Get Audio Spec : \n");
	//Log("Sample Rate", AudioSpec.freq);
	//Log("Format", AudioSpec.format);
	//Log("Channels", 
	//	AudioSpec.channels);
	//Log("samples", AudioSpec.samples);
	//Log("padding", AudioSpec.padding);
	//Log();

	SDL_UnlockAudio();
	SDL_PauseAudio(0);

	pthread_mutex_init(&RecorderListMtx, NULL);

	return TRUE;
}

void ts_Exit()
{
	SDL_CloseAudio();
	pthread_mutex_destroy(&SoundListMtx);
}

TranscodeSection* ts_CreateTranscodeSection()
{
	TranscodeSection* params = new TranscodeSection;
	ts_initparams(params);

	pthread_attr_t transcodeThreadAttr;
	pthread_attr_init(&transcodeThreadAttr);
	pthread_attr_setdetachstate(&transcodeThreadAttr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&params->transcodeThread, &transcodeThreadAttr, &TranscodeSectionThread, (void*)params);
	pthread_attr_destroy(&transcodeThreadAttr);

	return params;
}


void ts_initparams(TranscodeSection* p)
{
	SDL_memset(p, 0, sizeof(TranscodeSection));

	p->frame = newVideoBuffer();
	//p->frameindex = 0;

	p->outputSuite = new std::vector<AVOutputSuite*>;

	p->start_time = av_gettime();
	p->timebase.den = 24;
	p->timebase.num = 1;
	p->next_time = av_gettime() + 40000;

	p->audioFrame = av_frame_alloc();


#ifdef WIN32
	p->windowFrame = av_frame_alloc();
#endif

	packet_queue_init(&p->audioq);

	p->audioBuffer = new uint8_t[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
	p->audioBufIndex = 0;

	pthread_rwlock_init(&p->transcodeThreadLock, NULL);
}


BOOL ts_SetInputSuite(TranscodeSection *p, AVInputSuite *pSuite)
{
	if (p == NULL || pSuite == NULL)
	{
		Log("Error : Input params for SetInputSuite is NULL");
		return FALSE;
	}
	//输入源已经存在，则返回错误
	if (p->inputSuite != NULL)
	{
		Log("Error : The inputsuite already existes. (SetInputSuite)\n");
		return FALSE;
	}
	//要设置的输入源若没有打开或打开错误， 则返回错误
	if (!pSuite->IsInputOpen())
	{
		Log("Error : The status of inputsuite is unopened. (SetInputSuite)\n");
		return FALSE;
	}

	pthread_rwlock_wrlock(&p->transcodeThreadLock);

	if (pSuite->HasVideoStream())
	{
		p->videoWidth = pSuite->GetVideoWidth();
		p->videoHeight = pSuite->GetVideoHeight();
		p->videoPixFmt = pSuite->GetVideoPixFmt();

		//若输出窗口指针不为空，则修改输出窗口的SwsContext

		if (p->window != NULL && p->windowRenderer != NULL)
		{
			if (p->pSwsContext != NULL)
			{
				sws_freeContext(p->pSwsContext);
				p->pSwsContext = NULL;
			}

			p->pSwsContext = sws_getContext(pSuite->GetVideoWidth(), pSuite->GetVideoHeight(), pSuite->GetVideoPixFmt(),
				p->windowRect.w, p->windowRect.h,
				PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
		}

		//修改每一个输出源的SwsContext
		int nCount = p->outputSuite->size();

		for (int i = 0; i < nCount; i++)
		{
			p->outputSuite->at(i)->SetSwsCtx(pSuite->GetVideoWidth(), pSuite->GetVideoHeight(), pSuite->GetVideoPixFmt());
		}
	}
	p->inputSuite = pSuite;

	pthread_rwlock_unlock(&p->transcodeThreadLock);

	return TRUE;
}


void ts_DeleteInputSuite(TranscodeSection *p)
{
	if (p == NULL)
	{
		Log("Error : Input params for DeleteInputSuite is null\n");
		return;
	}

	pthread_rwlock_wrlock(&p->transcodeThreadLock);

	packet_queue_clear(&(p->audioq));

	if (p->inputSuite != NULL)
	{
		AVInputSuite *pSuite = p->inputSuite;
		p->inputSuite = NULL;

		pSuite->Close();
		delete pSuite;
	}

	pthread_rwlock_unlock(&p->transcodeThreadLock);
}


BOOL ts_AddOutputSuite(TranscodeSection *p, AVOutputSuite *pSuite)
{
	if (p == NULL || pSuite == NULL)
	{
		Log("Error : Input params for AddOutputSuite is null\n");
		return FALSE;
	}

	if (!pSuite->IsOutputOpen())
	{
		Log("Error : the OutputSuite is not open. (AddOutputSuite)\n");
		return FALSE;
	}

	if (p->inputSuite != NULL && p->inputSuite->HasVideoStream())
	{
		pSuite->SetSwsCtx(p->inputSuite->GetVideoWidth(), p->inputSuite->GetVideoHeight(), p->inputSuite->GetVideoPixFmt());
	}

	pthread_rwlock_wrlock(&p->transcodeThreadLock);
	p->outputSuite->push_back(pSuite);
	pthread_rwlock_unlock(&p->transcodeThreadLock);

	return TRUE;
}


void ts_DeleteAllOutputSuite(TranscodeSection *p)
{
	if (p == NULL)
	{
		Log("Error : Input params for DeleteAllOutputSuite is null\n");
		return;
	}

	pthread_rwlock_wrlock(&p->transcodeThreadLock);

	while(p->outputSuite->size())
	{
		AVOutputSuite *pSuite = p->outputSuite->back();
		pSuite->Close();
		delete pSuite;
		p->outputSuite->pop_back();
	}

	pthread_rwlock_unlock(&p->transcodeThreadLock);
}

#ifdef WIN32

BOOL ts_SetWindow(TranscodeSection *p, CWnd *pDlg, UINT nImageCtrlID)
{
	if (pDlg == NULL || nImageCtrlID == 0 || p == NULL)
	{
		Log("Error : Input params for SetDialog is null\n");
		return FALSE;
	}

	pthread_rwlock_wrlock(&p->transcodeThreadLock);

	CWnd *pWnd = pDlg->GetDescendantWindow(nImageCtrlID);
	CRect rect;
	::GetWindowRect(pWnd->GetSafeHwnd(), rect);
	int width = rect.Width();
	int height = rect.Height();


	p->window = SDL_CreateWindowFrom(pWnd->GetSafeHwnd());

	if (p->window == NULL)
	{
		Log("Error : Create SDL Window Failed !\n");
		goto ts_SetWindow_Failed;
	}

	p->windowRenderer = SDL_CreateRenderer(p->window, -1, SDL_RENDERER_ACCELERATED);
	if (p->windowRenderer == NULL)
	{
		Log("Error : Create SDL Renderer Failed !\n");
		goto ts_SetWindow_Failed;
	}

	p->windowTextrue = SDL_CreateTexture(p->windowRenderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
	if (p->windowTextrue == NULL)
	{
		Log("Error : Create SDL Texture Failed !\n");
		goto ts_SetWindow_Failed;
	}

	SDL_SetRenderDrawColor(p->windowRenderer, 50, 50, 50, 255);

	p->windowRect.x = p->windowRect.y = 0;
	p->windowRect.w = width;
	p->windowRect.h = height;

	if (p->inputSuite != NULL)
	{
		p->pSwsContext = sws_getContext(p->inputSuite->GetVideoWidth(),
			p->inputSuite->GetVideoHeight(), p->inputSuite->GetVideoPixFmt(),
			width, height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
	}

	if (p->buffer != NULL)
	{
		av_free(p->buffer);
		p->buffer = NULL;
	}

	int nSize = avpicture_get_size(PIX_FMT_YUV420P, width, height);
	p->buffer = (uint8_t*)av_malloc(nSize);

	avpicture_fill((AVPicture*)p->windowFrame, p->buffer, PIX_FMT_YUV420P, width, height);


	pthread_rwlock_unlock(&p->transcodeThreadLock);

	return TRUE;

ts_SetWindow_Failed:

	pthread_rwlock_unlock(&p->transcodeThreadLock);
	ts_UnsetWindow(p);
	return FALSE;
}


void ts_UnsetWindow(TranscodeSection *p)
{
	pthread_rwlock_wrlock(&p->transcodeThreadLock);

	if (p->windowTextrue != NULL)
	{
		SDL_DestroyTexture(p->windowTextrue);
		p->windowTextrue = NULL;
	}
	if (p->windowRenderer != NULL)
	{
		SDL_DestroyRenderer(p->windowRenderer);
		p->windowRenderer = NULL;
	}
	if (p->window != NULL)
	{
		SDL_DestroyWindow(p->window);
		p->window = NULL;
	}

	pthread_rwlock_unlock(&p->transcodeThreadLock);
}

#endif


int ts_DestroyTranscodeSection(TranscodeSection **pp)
{
	pthread_rwlock_wrlock(&(*pp)->transcodeThreadLock);
	(*pp)->transcodeThreadQuit = TRUE;
	pthread_rwlock_unlock(&(*pp)->transcodeThreadLock);

	pthread_join((*pp)->transcodeThread, NULL);

	ts_DeleteAllOutputSuite(*pp);
	ts_UnsetWindow(*pp);
	ts_DeleteInputSuite(*pp);
	ts_CloseSound(*pp);

	delete (*pp)->outputSuite;

	pthread_rwlock_destroy(&(*pp)->transcodeThreadLock);

	destroyVideoBuffer((*pp)->frame);
	
	av_frame_free(&(*pp)->windowFrame);
	av_free((*pp)->buffer);

	delete *pp;
	*pp = NULL;

	return 0;
}


void ts_OpenSound(TranscodeSection *p)
{
	SoundList.push_back(p);
}


void ts_CloseSound(TranscodeSection *p)
{
	int size = SoundList.size();
	
	pthread_mutex_lock(&SoundListMtx);
	SoundList.remove(p);
	pthread_mutex_unlock(&SoundListMtx);
}


void ts_SetVolumn(TranscodeSection *p, int volumn)
{
	if (p != NULL)
	{
		volumn = min(128, volumn);
		volumn = max(0, volumn);

		p->audioVolumn = volumn;
	}
}


void ts_OpenRecorder(TranscodeSection *p)
{
	pthread_mutex_lock(&RecorderListMtx);
	if (RecorderList.size() == 0)
		OpenRecorder();
	RecorderList.push_back(p);
	pthread_mutex_unlock(&RecorderListMtx);
}


void ts_CloseRecorder(TranscodeSection *p)
{
	pthread_mutex_lock(&RecorderListMtx);
	std::list<TranscodeSection*>::iterator it;
	RecorderList.remove(p);
	pthread_mutex_unlock(&RecorderListMtx);
	if (RecorderList.size() == 0)
		CloseRecorder();
}


void* PTW32_CDECL TranscodeSectionThread(void *params)
{
	TranscodeSection *p = (TranscodeSection*)params;

	if (p == NULL)
	{
		Log("Error :the params of the Transcode Section Thread is null\n");
		return NULL;
	}

	int64_t now = av_gettime();
	p->start_time = now;
	int64_t interval = 1000000 * p->timebase.num / p->timebase.den;		//帧间时间差
	int64_t next_time = now;		//下一帧播放时间

	int get_packet = NONE_PACKET;

	pthread_rwlock_rdlock(&p->transcodeThreadLock);

	Sleep(2000);

	while (!p->transcodeThreadQuit)
	{
		pthread_rwlock_unlock(&p->transcodeThreadLock);

		//视频播放时简单的延时处理
		now = av_gettime();
		if (now < next_time)
		{
			av_usleep(next_time - now);
			//TRACE("av_usleep %d\n", next_time - now);
			Log();
		}
		else
		{
			av_usleep(3000);
			if (now > next_time + interval)		//如果现在时间与这一帧应该播放的时间过得太远，则调整时间
				next_time = av_gettime();
		}

		//Sleep(40);

		if (p->transcodeThreadQuit)
			break;

		pthread_rwlock_rdlock(&p->transcodeThreadLock);

		int time1 = av_gettime();

		//	从输入源读取数据并解码
		if (p->inputSuite != NULL)
		{
			get_packet = p->inputSuite->GetPacket(&p->frame->pkt);

			if (get_packet != NONE_PACKET)
			{
				Log("Get Packet : ");
				Log("pts", p->frame->pkt.pts);
				Log("dts", p->frame->pkt.dts);
				Log("now", av_gettime() - p->start_time);
				Log();
			}
		}

		if (get_packet == VIDEO_PACKET)
		{
			next_time += interval;

			BOOL got_frame = FALSE;
			got_frame = p->inputSuite->DecodeVideoFrame(&p->frame->pkt, p->frame->pFrame);

			if (got_frame)
			{
				p->frame->HasFrame = TRUE;
			}
			else
			{
				p->frame->HasFrame = FALSE;
			}

			av_free_packet(&p->frame->pkt);

			//	将数据编码成流输出

			if (p->frame->HasFrame)
			{
				for (int i = 0; i < p->outputSuite->size(); i++)
				{
					p->outputSuite->at(i)->WriteVideoFrame(&p->frame->pFrame);
				}

				if (p->window != NULL
					&& p->windowRenderer != NULL
					&& p->windowTextrue != NULL)
				{
					if (p->frame->HasFrame)
					{
						sws_scale(p->pSwsContext, p->frame->pFrame->data, p->frame->pFrame->linesize,
							0, p->videoHeight, p->windowFrame->data, p->windowFrame->linesize);

						SDL_UpdateYUVTexture(p->windowTextrue, &(p->windowRect),
							p->windowFrame->data[0], p->windowFrame->linesize[0],
							p->windowFrame->data[1], p->windowFrame->linesize[1],
							p->windowFrame->data[2], p->windowFrame->linesize[2]);

						SDL_RenderClear(p->windowRenderer);
						SDL_RenderCopy(p->windowRenderer, p->windowTextrue, &(p->windowRect), &(p->windowRect));
						SDL_RenderPresent(p->windowRenderer);
					}
				}
			}

			//	在屏幕上显示

		}
		else if (get_packet == AUDIO_PACKET)
		{
			packet_queue_put(&p->audioq, &p->frame->pkt);
		}
		else
		{
			av_free_packet(&p->frame->pkt);
		}


		if (p->inputSuite == NULL
			&& p->window != NULL
			&& p->windowRenderer != NULL
			&& p->windowTextrue != NULL)
		{
			SDL_RenderClear(p->windowRenderer);
			SDL_RenderDrawRect(p->windowRenderer, &p->windowRect);
			SDL_RenderPresent(p->windowRenderer);
		}
	}

	pthread_rwlock_unlock(&p->transcodeThreadLock);

	return NULL;
}


int audio_decode_frame(TranscodeSection *sec, uint8_t *audio_buf, int buf_size);

//声音回调函数
//userdata是输入，stream是输出，len是输入
//audio_callback函数的功能是调用audio_decode_frame函数，把解码后数据块audio_buf追加在stream的后面，
//通过SDL库对audio_callback的不断调用，不断解码数据，然后放到stream的末尾，
//SDL库认为stream中数据够播放一帧音频了，就播放它, 
//第三个参数len是向stream中写数据的内存分配尺度，是分配给audio_callback函数写入缓存大小。

void audio_callback(void *userdata, Uint8 *stream, int len) 
{
	SDL_memset(stream, 0, len);

	//audio_buf 的大小为 1.5 倍的声音帧的大	小以便于有一个比较好的缓冲

	if (pthread_mutex_trylock(&SoundListMtx) == 0)
	{
		int nCount = SoundList.size();
		if (nCount != 0)
		{
			std::list<TranscodeSection*>::iterator index;

			for (index = SoundList.begin(); index != SoundList.end(); index++)
			{
				TranscodeSection *sec = *index;

				while (sec->audioBufIndex < len)
				{
					int decode_size = audio_decode_frame(sec, sec->audioBuffer + sec->audioBufIndex, sizeof(sec->audioBuffer) - sec->audioBufIndex);
					if (decode_size < 0)
					{
						break;
					}
					sec->audioBufIndex += decode_size;
				}

				if (sec->audioBufIndex > 0)
				{
					SDL_MixAudio(stream, sec->audioBuffer, min(sec->audioBufIndex, len), sec->audioVolumn);
					SDL_memmove(sec->audioBuffer, sec->audioBuffer + min(sec->audioBufIndex, len), sec->audioBufIndex - min(sec->audioBufIndex, len));
					sec->audioBufIndex -= min(sec->audioBufIndex, len);
				}
			}
		}

		pthread_mutex_unlock(&SoundListMtx);
	}
}


int audio_decode_frame(TranscodeSection *sec, uint8_t *audio_buf, int buf_size) 
{
	AVPacket pkt;
	AVFrame *pFrame;
	pFrame = av_frame_alloc();

	av_init_packet(&pkt);

	if (packet_queue_get(&(sec->audioq), &pkt) < 0)
	{
		av_frame_free(&pFrame);
		return -1;
	}

	int audio_pkt_size = pkt.size;
	int decode_size = 0;
	int buf_index = 0;

	while (decode_size < audio_pkt_size)
	{
		int ret = 0;

		pthread_rwlock_rdlock(&sec->transcodeThreadLock);
		
		int size1 = sec->inputSuite->DecodeAudioFrame(&pkt, pFrame, &ret);

		if (size1 < 0 || ret == 0)
		{
			//SDL_UnlockMutex(sec->inputAudioMtx);
			pthread_rwlock_unlock(&sec->transcodeThreadLock);
			break;
		}
		decode_size += size1;

		int size2 = sec->inputSuite->AudioResampling(pFrame, 
			AV_SAMPLE_FMT_S32, 2, 44100, audio_buf + buf_index);

		pthread_rwlock_unlock(&sec->transcodeThreadLock);

		if (size2 < 0)
		{
			break;
		}

		buf_index += size2;
	}

audio_decode_frame_end:


	av_free_packet(&pkt);
	av_frame_free(&pFrame);
	return buf_index;
}


BOOL OpenRecorder()
{
	WAVEFORMATEX wfx; /* look this up in your documentation */

	wfx.nSamplesPerSec = 44100; /* sample rate */
	wfx.wBitsPerSample = 32; /* sample size */
	wfx.nChannels = 2; /* channels*/
	wfx.cbSize = 0; /* size of _extra_ info */
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nBlockAlign = (wfx.wBitsPerSample * wfx.nChannels) >> 3;
	wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

	waveBlocks = allocateBlocks(BLOCK_SIZE, BLOCK_COUNT);
	waveFreeBlockCount = BLOCK_COUNT;
	waveCurrentBlock = 0;

	//InitializeCriticalSection(&waveCriticalSection);

	if (waveInOpen(&waveHandle, WAVE_MAPPER, &wfx, (DWORD_PTR)waveInProc, (DWORD_PTR)&waveFreeBlockCount, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
	{
		Log("Error : open wave in failed.\n");
		freeBlocks(waveBlocks);
		return FALSE;
	}

	for (int i = 0; i < BLOCK_COUNT; ++i)
	{
		waveInPrepareHeader(waveHandle, &waveBlocks[i], sizeof(WAVEHDR));
		waveInAddBuffer(waveHandle, &waveBlocks[i], sizeof(WAVEHDR));
	}

	waveCurrentBlock = 0;

	waveQuit = FALSE;

	waveInStart(waveHandle);

	return TRUE;
}


static void CALLBACK waveInProc(HWAVEIN hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	if (uMsg != WIM_DATA)
		return;

	WAVEHDR *current = (WAVEHDR*)dwParam1;

	pthread_mutex_lock(&RecorderListMtx);

	std::list<TranscodeSection*>::iterator section;
	for (section = RecorderList.begin(); section != RecorderList.end(); section++)
	{
		TranscodeSection *sec = *section;
		int nCount = sec->outputSuite->size();
		for (int index = 0; index < nCount; index++)
		{
			sec->outputSuite->at(index)->WriteAudio(current->lpData, current->dwBytesRecorded);
		}
	}

	pthread_mutex_unlock(&RecorderListMtx);

	if (!waveQuit)
		waveInAddBuffer(hWaveOut, current, sizeof(WAVEHDR));
}


BOOL CloseRecorder()
{
	waveQuit = TRUE;
	Sleep(100);
	waveInClose(waveHandle);
	freeBlocks(waveBlocks);
	return TRUE;
}


WAVEHDR* allocateBlocks(int size, int count)
{
	unsigned char* buffer;
	int i;
	WAVEHDR* blocks;
	DWORD totalBufferSize = (size + sizeof(WAVEHDR)) * count;
	/*
	* allocate memory for the entire set in one go
	*/
	if ((buffer = (unsigned char *)HeapAlloc(
		GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		totalBufferSize
		)) == NULL)
	{
		fprintf(stderr, "Memory allocationerror\n");
		ExitProcess(1);
	}

	/*
	* and set up the pointers to each bit
	*/

	blocks = (WAVEHDR*)buffer;
	buffer += sizeof(WAVEHDR) * count;

	for (i = 0; i < count; i++)
	{
		blocks[i].dwBufferLength = size;
		blocks[i].lpData = (LPSTR)buffer;
		blocks[i].dwBytesRecorded = 0;
		blocks[i].dwUser = 0;
		blocks[i].dwFlags = 0;
		blocks[i].dwLoops = 1;
		blocks[i].lpNext = NULL;
		blocks[i].reserved = 0;

		buffer += size;
	}

	return blocks;
}


void freeBlocks(WAVEHDR* blockArray)
{
	/*
	* and this is why allocateBlocks works the way it does
	*/
	HeapFree(GetProcessHeap(), 0, blockArray);
}
