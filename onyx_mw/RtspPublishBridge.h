#ifndef __RTSP_PUBLISH_BRIDGE__
#define __RTSP_PUBLISH_BRIDGE__

#include "StrmOutBridgeBase.h"
#include "JdRtspSrv.h"
#include "JdRtspClntRec.h"
#include "JdRfc3984.h"

class CRtspPublishBridge : public CStrmOutBridge
{
public:
	CRtspPublishBridge();
	~CRtspPublishBridge();

	int Run(COutputStream *pOutputStream)
	{
		PrepareMediaDelivery(pOutputStream);
		return -1;
	}

//private:
	void PrepareMediaDelivery(COutputStream *pOutputStream);
	void RemoveMediaDelivery(COutputStream *pOutputStream);
	unsigned char *FindStartCode(unsigned char *p, unsigned char *end);

	int SendVideo(unsigned char *pData, int size, unsigned long lPts);
	int SendAudio(unsigned char *pData, int size, unsigned long lPts);
	int ConnectToPublishServer(CRtspPublishConfig *pRtspPublishCfg);
	int StartRtspServer(CRtspSrvConfig *pRtspSvrCfg);

public:
	CJdRtspSrv			*mRtspSrv;
	CMediaResMgr        *mMediaResMgr;
	CMediaAggregate		*mAggregate;
	CMediaTrack			*mVideoTrack;
	CMediaTrack			*mAudioTrack;
	CJdRtspClntRecSession *mRtspClntRec;
	CRtspSrvConfig      *mpRtspSrvCfg;

public:
	unsigned short   mRtspPort;
	char             m_szRemoteHost[MAX_NAME_SIZE];
	unsigned short   m_usLocalRtpPort;
	unsigned short   m_usRemoteRtpPort;
	int              m_fEnableRtspSrv;
	int              m_fEnableAud;
	int              m_fEnableVid;
	CAvcCfgRecord    mAvcCfgRecord;
};

#endif