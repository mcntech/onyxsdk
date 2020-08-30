#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "EncOut.h"
#include "JdDbg.h"

static int  modDbgLevel = CJdDbg::LVL_TRACE;
CEncOut::CEncOut()
{
	m_pVidConnSrc = NULL;
	m_pAudConnSrc = NULL;
	PrevClk = 0;
	aud_frames = 0;
	vid_frames = 0;
}

int CEncOut::AddOutput(CStrmOutBridge *pOut)
{
	m_Outputs.push_back(pOut);
	return 0;
}

int CEncOut::DeleteOutput(CStrmOutBridge *pOut)
{
	for(StrmOutArray_T::iterator it = m_Outputs.begin(); it != m_Outputs.end(); ++it) {
		if(pOut == *it) {
			m_Outputs.erase(it);
			break;
		}
	}
	return 0;
}

void *CEncOut::threadStreamingVideo(void *threadsArg)
{
	CEncOut *pCtx =  (CEncOut *)threadsArg;
	return pCtx->ProcessVideo();
}

void *CEncOut::threadStreamingAudio(void *threadsArg)
{
	CEncOut *pCtx =  (CEncOut *)threadsArg;
	return pCtx->ProcessAudio();
}

void *CEncOut::ProcessVideo()
{
	char *pData = (char *)malloc(MAX_VID_FRAME_SIZE);
	long long ullPts;
	unsigned long ulFlags = 0;

	while (m_fRun)	{
		int length = 0;
		while(m_pVidConnSrc->IsEmpty(m_pVidConnSrc) && m_fRun){
#ifdef WIN32
			Sleep(1);
#else
			usleep(1000);
#endif
			ShowStats();
		}
		if(!m_fRun)
			break;
		
		JDBG_LOG(CJdDbg::LVL_STRM,("Read m_pVidConnSrc"));
		ShowStats();
		length = m_pVidConnSrc->Read(m_pVidConnSrc, pData, MAX_VID_FRAME_SIZE, &ulFlags, &ullPts);
		
		JDBG_LOG(CJdDbg::LVL_STRM,("Read length=%d %lld", length, ullPts));

		for(StrmOutArray_T::iterator it = m_Outputs.begin(); it != m_Outputs.end(); ++it) {
			CStrmOutBridge *pOut = *it;
			if(length > 0) {
				JDBG_LOG(CJdDbg::LVL_STRM,("pOut=%p", pOut));
				pOut->SendVideo((unsigned char *)pData, length, ullPts * 90 / 1000);
				vid_frames++;
			}
		}
		read_position += length;
	}
	return NULL;
}

void *CEncOut::ProcessAudio()
{
	char *pData = (char *)malloc(MAX_AUD_FRAME_SIZE);
	long long ullPts;
	unsigned long ulFlags = 0;

	while (m_fRun)	{
		int length = 0;
		while(m_pAudConnSrc->IsEmpty(m_pAudConnSrc) && m_fRun){
#ifdef WIN32
			Sleep(1);
#else
			usleep(1000);
#endif
			ShowStats();
		}
		if(!m_fRun)
			break;
		ShowStats();
		JDBG_LOG(CJdDbg::LVL_STRM,("Read m_pAudConnSrc"));
		
		length = m_pAudConnSrc->Read(m_pAudConnSrc, pData, MAX_AUD_FRAME_SIZE, &ulFlags, &ullPts);
		
		JDBG_LOG(CJdDbg::LVL_STRM,("Read length=%d %lld", length, ullPts));
		//DumpHex((unsigned char *)pData, 16);
		for(StrmOutArray_T::iterator it = m_Outputs.begin(); it != m_Outputs.end(); ++it) {
			CStrmOutBridge *pOut = *it;
			if(length > 0) {
				JDBG_LOG(CJdDbg::LVL_STRM,("pOut=%p", pOut));
				pOut->SendAudio((unsigned char *)pData, length, ullPts * 90 / 1000);
				aud_frames++;
			}
		}
		read_position += length;
	}
	return NULL;
}

int CEncOut::Run()
{
	m_fRun = 1;
#ifdef WIN32
	{
		DWORD dwThreadId;
		if(m_pVidConnSrc) {
			m_thrdVidHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadStreamingVideo, this, 0, &dwThreadId);
		}
		if(m_pAudConnSrc) {
			m_thrdAudHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadStreamingAudio, this, 0, &dwThreadId);
		}
	}
#else
	if(m_pVidConnSrc) {
		pthread_create(&m_thrdVidHandle, NULL, threadStreamingVideo, this);
	}
	if(m_pAudConnSrc) {
		pthread_create(&m_thrdAudHandle, NULL, threadStreamingAudio, this);
	}
#endif
	return 0;
}

int CEncOut::Stop()
{
	void *res;
	m_fRun =  0;
#ifdef WIN32
	if(m_pVidConnSrc) {
		WaitForSingleObject(m_thrdVidHandle, 3000);
	}
	if(m_pAudConnSrc) {
		WaitForSingleObject(m_thrdAudHandle, 3000);
	}
#else
	if(m_pVidConnSrc) {
		pthread_join(m_thrdVidHandle, &res);
	}
	if(m_pAudConnSrc) {
		pthread_join(m_thrdAudHandle, &res);
	}
#endif
    return 0;
}

int CEncOut::SetSource(ConnCtxT *pVidConnSrc, ConnCtxT *pAudConnSrc)
{
	if(m_pVidConnSrc) {
		// TODO: Set discontinuity for source change
	}
	m_pVidConnSrc = pVidConnSrc;

	if(m_pAudConnSrc) {
		// TODO: Set discontinuity for source change
	}
	m_pAudConnSrc = pAudConnSrc;
	return 0;
}