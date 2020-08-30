#ifndef __HLS_CLNT_BRIDGE_H__
#define __HLS_CLNT_BRIDGE_H__

#include "StrmInBridgeBase.h"
#include "JdHttpLiveClnt.h"

#define MAX_NAME_SIZE	256
class CHlsClntBridge : public CStrmInBridgeBase
{
public:
	CHlsClntBridge(const char *lpszRspServer, int fEnableAud, int fEnableVid, int *pResult);
	virtual ~CHlsClntBridge();

	int StartClient(const char *lpszRspServer);
    virtual int StartStreaming(void);
    virtual int StopStreaming(void);

private:
	void *DoBufferProcessing();
	static void *thrdStart(void *pObj);
	long SendVideoFrame(char *pData, int nSize, long long llPts);
	long SendAudioFrame(char *pData, int nSize, long long llPts);
	int InitAudioStreaming();
	int InitVideoStreaming();

public:
	CHttpLiveGet	   *m_pHlsClnt;
	int					m_nAudCodec;
	int					m_nVidCodec;
	int					m_nSrcType;
	char				*m_pData;
	long				m_lUsedLen;
	long				m_lMaxLen;
	long long			m_lPts;
	long long			m_lDts;

	char				*m_pAudData;
	long				m_lAudUsedLen;
	long				m_lAudMaxLen;
	long long			m_lAudPts;

#ifdef WIN32
	HANDLE              m_thrdHandleStream;
#else
	pthread_t			m_thrdHandleStream;
#endif

public:

	int        fEoS;
	long       read_position;
	int        frameCounter;
	unsigned short   mRtspPort;
	char             m_szRemoteHost[MAX_NAME_SIZE];
	unsigned short   m_usLocalRtpPort;
	unsigned short   m_usRemoteRtpPort;
	char             *m_pBuffer;
	PsiCallback_T     psiCallback;
	//CAvcCfgRecord    mAvcCfgRecord;
	CTsDemuxIf        *m_pDemux;
	CTsPsiIf          *m_pTsPsi;
};
#endif //__HLS_CLNT_BRIDGE_H__