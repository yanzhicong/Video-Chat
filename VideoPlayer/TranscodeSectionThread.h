#pragma once

#include "AVPlaySuite.h"

typedef struct TranscodeSectionParams TranscodeSection;

//全局变量初始化以及音频初始化
BOOL ts_Initialize();

//创建线程
TranscodeSection* ts_CreateTranscodeSection();


//设置输入源，设置之前必须删除旧的输入源，输入源在设置之前必须是打开的。
//传进去的输入源不能调用close
BOOL ts_SetInputSuite(TranscodeSection *p, AVInputSuite *pSuite);
void ts_DeleteInputSuite(TranscodeSection *p);


//添加输出源，添加之前输出源必须是打开的。
BOOL ts_AddOutputSuite(TranscodeSection *p, AVOutputSuite *pSuite);
void ts_DeleteAllOutputSuite(TranscodeSection *p);


#ifdef WIN32
//设置播放视频的控件
BOOL ts_SetWindow(TranscodeSection *p, CWnd *pDlg, UINT nImageCtrlID);
void ts_UnsetWindow(TranscodeSection *p);
#endif


void ts_OpenSound(TranscodeSection *p);
void ts_CloseSound(TranscodeSection *p);
void ts_SetVolumn(TranscodeSection *p, int volumn);


void ts_OpenRecorder(TranscodeSection *p);
void ts_CloseRecorder(TranscodeSection *p);

//退出线程，退出时会自动清除占用的内存
int ts_DestroyTranscodeSection(TranscodeSection **pp);
