
// VideoPlayerDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "VideoPlayer.h"
#include "VideoPlayerDlg.h"
#include "afxdialogex.h"
#include "TranscodeSectionThread.h"
#include "AVPlaySuite.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CVideoPlayerDlg 对话框



CVideoPlayerDlg::CVideoPlayerDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CVideoPlayerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CVideoPlayerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CVideoPlayerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CVideoPlayerDlg 消息处理程序

BOOL CVideoPlayerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO:  在此添加额外的初始化代码

	ts_Initialize();

	m_window = ts_CreateTranscodeSection();
	m_window1 = ts_CreateTranscodeSection();
	m_window2 = ts_CreateTranscodeSection();
	m_window3 = ts_CreateTranscodeSection();

	if (!ts_SetWindow(m_window, this, IDC_VIEW)
		//|| !ts_SetWindow(m_window1, this, IDC_VIEW2)
		//|| !ts_SetWindow(m_window2, this, IDC_VIEW3)
		//|| !ts_SetWindow(m_window3, this, IDC_VIEW4))
		)
	{
		MessageBox(_T("Set window failed !"));
		return FALSE;
	}

	//ts_OpenSound(m_window);

	//ts_SetVolumn(m_window, 100);


	
	AVInputSuite *pSuite = new AVInputSuite;
	//pSuite->OpenFileInput("D:\\MOVIE\\1234.mp4");

	pSuite->OpenCameraInput();
	ts_SetInputSuite(m_window, pSuite);

	AVOutputSuite *pOSuite = new AVOutputSuite;

	//if (!pOSuite->OpenRTPOutput("rtp://127.0.0.1:10000"))
	if (!pOSuite->OpenFileOutput("D://test.h264"))
	//if (!pOSuite->OpenRTMPOutput("rtmp://localhost/publishlive/livestream"))
	{
		MessageBox(_T("Open Output Failed"));
	}
	if (!pOSuite->OpenAudioStream())
	{
		MessageBox(_T("Open AudioStream Failed"));
	}
	if (!pOSuite->OpenVideoStream(720, 480))
	{
		MessageBox(_T("Open Video Stream Failed"));
	}
	if (!pOSuite->WriteHeader())
	{
		MessageBox(_T("Write Header Failed"));
	}

	//pOSuite->CreateSdpFile("D:\\testvideo.sdp");

	ts_AddOutputSuite(m_window, pOSuite);

	ts_OpenRecorder(m_window);

	//AVOutputSuite *pOSuite2 = new AVOutputSuite;
	//if (!pOSuite2->OpenFileOutput("D:\\test.mp4")
	//	&& (!pOSuite2->OpenVideoStream(720, 480)))
	//{
	//	MessageBox(_T("Open Output Failed"));
	//}
	//pOSuite2->CreateSdpFile("D:\\testvideo.sdp");
	//pOSuite2->WriteHeader();
	//
	//ts_AddOutputSuite(m_window, pOSuite2);

	//AVInputSuite *pSuite1 = new AVInputSuite;
	//pSuite1->OpenFileInput("D:\\MOVIE\\1234.mp4");
	////pSuite->OpenCameraInput();
	//ts_SetInputSuite(m_window1, pSuite1);


	//AVInputSuite *pSuite2 = new AVInputSuite;
	//pSuite2->OpenFileInput("D:\\MOVIE\\1234.mp4");
	////pSuite->OpenCameraInput();
	//ts_SetInputSuite(m_window2, pSuite2);


	//AVInputSuite *pSuite3 = new AVInputSuite;
	//pSuite3->OpenFileInput("D:\\MOVIE\\1234.mp4");
	////pSuite->OpenCameraInput();
	//ts_SetInputSuite(m_window3, pSuite3);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CVideoPlayerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CVideoPlayerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CVideoPlayerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CVideoPlayerDlg::OnClose()
{
	// TODO:  在此添加消息处理程序代码和/或调用默认值
	ts_CloseRecorder(m_window);

	ts_DestroyTranscodeSection(&m_window);
	ts_DestroyTranscodeSection(&m_window1);
	ts_DestroyTranscodeSection(&m_window2);
	ts_DestroyTranscodeSection(&m_window3);

	CDialogEx::OnClose();
}
