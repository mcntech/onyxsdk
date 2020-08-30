#ifndef __ENC_OUT_H__
#define __ENC_OUT_H__
#ifdef WIN32
//#include <Windows.h>
#include <winsock2.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#endif
#include <assert.h>
#include <vector>
#include "StrmOutBridgeBase.h"

typedef std::vector<CStrmOutBridge *> StrmOutArray_T;
class CEncOut
{
public:
	CEncOut();
	static void *threadStreamingVideo(void *threadsArg);
	static void *threadStreamingAudio(void *threadsArg);
	void *ProcessVideo();
	void *ProcessAudio();
	int Run();
	int Stop();
	int SetSource(ConnCtxT *pVidConnSrc, ConnCtxT *pAudConnSrc);
	int AddOutput(CStrmOutBridge *pOut);

	int DeleteOutput(CStrmOutBridge *pOut);
	void ShowStats();

public:
	int  nSessionId;
	int nCmd;
	int nPort;

	int  nSampleRate;
	int  nFrameRate;
	int  nWidth;
	int  nHeight;

	StrmOutArray_T        m_Outputs;

	int            m_fRun;
	ConnCtxT       *m_pVidConnSrc;
	ConnCtxT       *m_pAudConnSrc;

	COutputStream  *m_pOutputStream;
#ifdef WIN32
	HANDLE         m_thrdVidHandle;
#else
	pthread_t      m_thrdVidHandle;
#endif
#ifdef WIN32
	HANDLE         m_thrdAudHandle;
#else
	pthread_t      m_thrdAudHandle;
#endif

	int        fEoS;
	long       read_position;
	unsigned long  PrevClk;
	long       aud_frames;
	long       vid_frames;

};
#endif // __ENC_OUT_H__