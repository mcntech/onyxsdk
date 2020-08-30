#ifdef WIN32
//#include <Windows.h>
#include <winsock2.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "string.h"

#include "JdDbg.h"
#include "strmconn.h"
#include "h264parser.h"
#include "TsDemuxIf.h"
#include "TsPsiIf.h"
#include "tsfilter.h"
#include "HlsClntBridge.h"
#include "JdOsal.h"

#define MAX_VID_FRAME_SIZE      (1920 * 1080)
#define MAX_AUD_FRAME_SIZE      (8 * 1024)

int CHlsClntBridge::StartClient(const char *lpszRspServer)
{
	int nResult = 0;
	int res = m_pHlsClnt->Open(lpszRspServer);
	if(res < 0){
		fprintf(stderr,"Failed to open HLS connection for %s",lpszRspServer);
		nResult = -1;
		goto Exit;
	}

		//if(m_nVidCodec == RTP_CODEC_H264) {
		if(1) {
			int nSpsSize = 256;
			unsigned char Sps[256];
			//if(m_pRtspClnt->GetVideoCodecConfig(Sps, &nSpsSize)) {
			//	H264::cParser Parser;
			//	if(Parser.ParseSequenceParameterSetMin(Sps,nSpsSize, &m_lWidth, &m_lHeight) != 0){
			//		// Errorr
			//	}
			//}
			CreateH264OutputPin();
		}

		//if(m_nAudCodec == RTP_CODEC_AAC) {
		if(1) {
			CreateMP4AOutputPin();
		}
Exit:
	return nResult;
}

CHlsClntBridge::CHlsClntBridge(const char *lpszRspServer, int fEnableAud, int fEnableVid, int *pResult) : CStrmInBridgeBase(fEnableAud, fEnableVid)
{
	m_pHlsClnt = new CHttpLiveGet;

	m_pData = (char *)malloc(MAX_VID_FRAME_SIZE);
	m_lMaxLen = MAX_VID_FRAME_SIZE;
	m_lPts = 0;
	m_lDts = 0;
	m_lUsedLen = 0;
	m_lWidth = 0;
	m_lHeight = 0;

	m_pAudData = (char *)malloc(MAX_AUD_FRAME_SIZE);
	m_lAudUsedLen = 0;
	m_lAudMaxLen = MAX_AUD_FRAME_SIZE;
	m_lAudPts = 0;
	
	*pResult = StartClient(lpszRspServer);
}


CHlsClntBridge::~CHlsClntBridge()
{
	if(m_pHlsClnt){
		m_pHlsClnt->Close();
		delete m_pHlsClnt;
	}
}



long CHlsClntBridge::SendVideoFrame(char *pData, int nSize, long long llPts)
{
	unsigned ulFlags = 0;
	long lResult = 0;
	long lStrmType = 0;

	ConnCtxT   *pConnSink = (ConnCtxT *)mDataLocatorVideo.pAddress;
	while(pConnSink->IsFull(pConnSink) && m_fRun){
		JD_OAL_SLEEP(1)
	}

	pConnSink->Write(pConnSink, m_pData, m_lUsedLen,  ulFlags, m_lPts * 1000 / 90);

	return lResult;
}

long CHlsClntBridge::SendAudioFrame(char *pData, int nSize, long long llPts)
{
	unsigned ulFlags = 0;
	long lResult = 0;
	ConnCtxT   *pConnSink = (ConnCtxT *)mDataLocatorAudio.pAddress;

	while(pConnSink->IsFull(pConnSink) && m_fRun){
		JD_OAL_SLEEP(1)
	}

	pConnSink->Write(pConnSink, pData, nSize,  ulFlags, llPts * 1000 / 90);

	return lResult;
}

int CHlsClntBridge::InitAudioStreaming()
{
    return 0;
}

int CHlsClntBridge::InitVideoStreaming()
{
    return 0;
}


class CPsiDispatch : public CPktParser
{
public:
	CPsiDispatch(CHlsClntBridge *pHlsClnt, CTsPsiIf *pTsPsi):CPktParser()
	{
		m_pTsPsi = pTsPsi;
		m_pHlsClnt = pHlsClnt;
		m_pTsPsi->RegisterCallback(this, PsiCallback);
	};
	~CPsiDispatch(){};

	static int PsiCallback(void *pCtx, int EvenId, void *pData);

	int Process(unsigned char *pTsData)
	{
		m_pTsPsi->Parse(pTsData, TS_PKT_LEN);
		return 0;
	}

	CHlsClntBridge  *m_pHlsClnt;
	CTsPsiIf        *m_pTsPsi;
};

int CPsiDispatch::PsiCallback(void *pCtx, int EvenId, void *pData)
{
	CPsiDispatch *pDispach = (CPsiDispatch *)pCtx;
	switch(EvenId)
	{
	case EVENT_NEW_PMT:
		{
			PMT_INF_T *pPmtInf = (PMT_INF_T *)pData;
			pDispach->m_pHlsClnt->m_pDemux->RegisterPktParser(pPmtInf->nPmtPid, pDispach);
		}
		break;
	case EVENT_PROGRAM_CHANGED:
		{
			int n = 0;
		}
	}
	return 0;
}

void *CHlsClntBridge::DoBufferProcessing()
{
	void *res = 0;
	long lBytes = 0;

	m_pDemux = CTsDemuxIf::CreateInstance();
	m_pTsPsi = CTsPsiIf::CreateInstance();
	CTsbuffer  *pTsBuff = new CTsbuffer(1024 * 1024);
	CAvFrame *pAvFrame = NULL;
	CPsiDispatch *pPsiDispatch = new CPsiDispatch(this, m_pTsPsi);

	m_pDemux->RegisterPktParser(0, pPsiDispatch);
	while(m_fRun){
		pTsBuff->mUsedSize =  m_pHlsClnt->Read(pTsBuff->mData, pTsBuff->mMaxSize);
		if(pTsBuff->mUsedSize <= 0) 
			goto Exit;

		pAvFrame = m_pDemux->Parse(pTsBuff);
		if(pAvFrame){
			if(pAvFrame->mCodecId == CTsDemuxIf::CODEC_ID_H264) {
				SendVideoFrame(pAvFrame->mFrmData, pAvFrame->mUsedSize, pAvFrame->mPts);
			} else if(pAvFrame->mCodecId == CTsDemuxIf::CODEC_ID_AAC) {
				SendAudioFrame(pAvFrame->mFrmData, pAvFrame->mUsedSize, pAvFrame->mPts);
			}
			pAvFrame->Reset();
		}
	}

Exit:
	if(m_pDemux)
		delete m_pDemux;
	if(pTsBuff)
		delete pTsBuff;
	if(m_pTsPsi)
		delete m_pTsPsi;
	if(pPsiDispatch)
		delete pPsiDispatch;
	return res;
}

void *CHlsClntBridge::thrdStart(void *pArg)
{
	CHlsClntBridge *pHlsClntBridg = (CHlsClntBridge *)pArg;
	return pHlsClntBridg->DoBufferProcessing();
}

int CHlsClntBridge::StartStreaming()
{
	m_fRun = 1;
#ifdef WIN32
	{
			DWORD dwThreadId;
			m_thrdHandleStream = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thrdStart, this, 0, &dwThreadId);
	}
#else
	pthread_create(&m_thrdHandleStream, NULL, thrdStart, this);
#endif

    return 0;
}

int CHlsClntBridge::StopStreaming()
{
	void *res;
	m_fRun = 0;
	if(m_thrdHandleStream){
#ifdef WIN32
		WaitForSingleObject(m_thrdHandleStream, 3000);
#else
		pthread_join(m_thrdHandleStream, &res);
#endif
	}
    return 0;
}

