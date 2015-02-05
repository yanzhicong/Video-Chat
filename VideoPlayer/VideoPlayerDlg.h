
// VideoPlayerDlg.h : 头文件
//

#pragma once

#include "TranscodeSectionThread.h"

// CVideoPlayerDlg 对话框
class CVideoPlayerDlg : public CDialogEx
{
// 构造
public:
	CVideoPlayerDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_VIDEOPLAYER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	TranscodeSection* m_window;
	TranscodeSection* m_window1;
	TranscodeSection* m_window2;
	TranscodeSection* m_window3;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnClose();
};
