#ifndef __JD_RTP_SRV_H__
#define __JD_RTP_SRV_H__

#include <assert.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <vector>
#define read	_read
#else
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include "JdSdp.h"

#include "JdRtsp.h"
#include "JdRtp.h"
#include "JdRtpSnd.h"
#include "JdMediaResMgr.h"
#include "JdDbg.h"

class CJdRtspSrv;
class CJdRtspSrvSession;

#define DEF_SRV_RTP_PORT_START	59527	// up to 59427+MAX Sessions * 2

typedef enum _RTSP_STATE_T
{
	RTSP_SETUP,
	RTSP_PLAY,
	RTSP_PUASE,
	RTSP_STOP,
	RTSP_TEARDOWN,
} RTSP_STATE_T;

typedef int (*pfnSetStreamState_t)(RTSP_STATE_T, CJdRtspSrvSession *);

typedef struct _SESSION_CONTEXT_T {
	int						sockClnt;
} SESSION_CONTEXT_T;



/**
 * RTSP Server Session 
 */
class CJdRtspSrvSession : public CRtspSession
{
public:
	CJdRtspSrvSession(
		CMediaResMgr *pMediaResMgr):mJdDbg("CJdRtspSrvSession", CJdDbg::LVL_MSG)
	{
		mMediaResMgr = pMediaResMgr;
	}
	~CJdRtspSrvSession()
	{
	}
	
	int GenerateSdp(CSdp *pSdp, const char *pszMedia);
	virtual void HandlePlay(CRtspRequest &);
	virtual void HandleTearDown(CRtspRequest &);
	void HandleSetup(CRtspRequest &, CRtspRequestTransport &);
	int HandleDescribe(CRtspRequest &);
	void HandleOptions(CRtspRequest &);
	
	virtual int CreateSetupRepy(char *pBuff, int nMaxLen, CRtspRequest *clntReq, CRtspRequestTransport *pTrans);
	virtual int CreatePlayReply(char *pBuff, int nMaxLen, CRtspRequest *clntReq);
	virtual int CreateTearDownreply(char *pBuff, int nMaxLen, CRtspRequest *clntReq);

	void SendFile(char *parameter);


	static void *sessionThread( void *arg );
	void *threadProcessRequests();

	void PrepreRtspStatus(char *statusbuff, int nStatusId);
	void SendRtspStatus(int hSock, char *statusBuf, int nLen);


	bool PlayTrack(CMediaTrack *pTrack);
	void RemoveMediaDeliveries();

public:
	CRtpSnd					*m_pVRtp;
	CRtpSnd					*m_pARtp;
	int						m_hSock;
	CMediaResMgr  		    *mMediaResMgr;
	std::string             mAggregateStream;
	std::map<std::string, IMediaDelivery *> mTrackDeliveries;

#ifdef WIN32
	HANDLE hThread;
#else
	pthread_t hThread;
#endif
	CJdDbg mJdDbg;
};

/**
 * RTSP Server. Creates CJdRtspSrvSession::ClientThread for each client connection.
 */
class CJdRtspSrv
{
public:
	CJdRtspSrv(CMediaResMgr	*pMediaResMgr)
	{
		m_hSock = -1;
		mMediaResMgr = pMediaResMgr;
		m_Stop = 0;
	}
	~CJdRtspSrv()
	{
		m_hSock = -1;
	}

	static void ServerCleanup(void *arg);
	static void *ServerThread( void *arg );
	int Stop(int id);
	int Run(int id,	unsigned short	rtspPort);
	int RegisterCallback(
			const char				*pszFolder,	// Base folder to located the video resource
			pfnSetStreamState_t		pfnCallbak	// Callback RTSP Session State
		);

	CMediaResMgr *GetResourceManager()
	{
		return mMediaResMgr;
	}

public:
	int						m_hSock;
#ifdef WIN32
	HANDLE					m_thrdHandle;
#else
	pthread_t				m_thrdHandle;
#endif
	unsigned short			mRtspPort;
	int						m_Stop;
	CMediaResMgr			*mMediaResMgr;
};

#endif // __JD_RTP_SRV_H__