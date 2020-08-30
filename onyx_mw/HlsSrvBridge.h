#ifndef __HLS_SRV_BRIDGE_H__
#define __HLS_SRV_BRIDGE_H__

#include "StrmOutBridgeBase.h"
#include "JdHttpSrv.h"
#include "JdHttpLiveSgmt.h"
#include "h264parser.h"
#include "TsDemuxIf.h"
#include "TsPsiIf.h"
#include "tsfilter.h"
#include "AccessUnit.h"
#include "SimpleTsMux.h"
#include "uimsg.h"
#include <JdOsal.h>

#define MAX_TS_BUFFER_SIZE		(512 * 1024)

class CHlsSrvBridge : public CStrmOutBridge
{
public:
	CHlsSrvBridge()
	{
		m_fEnableHlsServer = 0;
		m_fEnableHlsPublish = 0;
		m_pHttpSrv = NULL;
		m_pUploader = NULL;
		m_pUploaderForExtHttpSrv = NULL;
		m_pTsBuffer = (char *)malloc(MAX_TS_BUFFER_SIZE);
	}
	
	~CHlsSrvBridge()
	{
		if(m_pHttpSrv) {
			m_pHttpSrv->Stop(0);
			delete m_pHttpSrv;
		}
		if(m_pTsBuffer)
			free(m_pTsBuffer);

		if(m_pUploader){
			hlsPublishStop(m_pUploader);
		}
		if(m_pUploaderForExtHttpSrv){
			hlsPublishStop(m_pUploaderForExtHttpSrv);
		}

	}
	int Run(COutputStream *pOutputStream);
	int SendVideo(unsigned char *pData, int size, unsigned long lPts);
	int SendAudio(unsigned char *pData, int size, unsigned long lPts);

	int SetServerConfig(CHlsServerConfig *pHlsSvrCfg)
	{ 
		sprintf(m_szResourceFolder, "/%s", pHlsSvrCfg->m_szApplicationName);
		m_fEnableHlsServer = 1;
		m_pHlsServerConfig = pHlsSvrCfg;
		return 0;
	}
	int SetServerConfig(CHlsPublishCfg *pHlsPublishConfig)
	{ 
		m_pHlsPublishCfg = pHlsPublishConfig;
		m_fEnableHlsPublish = 1;
		return 0;
	}
	
	int GetPublishStatistics(int *pnState, int *pnStreamInTime, int *pnLostBufferTime,  int *pnStreamOutTime, int *pnSegmentTime)
	{
		int res = 0;
		if(m_fEnableHlsPublish && m_pUploader){
			hlsPublishGetStats(m_pUploader, pnState, pnStreamInTime, pnLostBufferTime, pnStreamOutTime, pnSegmentTime);
		} else {
			*pnState = HLS_PUBLISH_STOPPED;
		}
		return res;
	}

public:
	void            *m_pSegmenter;
	CJdHttpSrv      *m_pHttpSrv;
	CTsMux			m_TsMux;
	void            *m_pUploader;
	void            *m_pUploaderForExtHttpSrv;

	char            *m_pTsBuffer;
	char            m_szFilePrefix[256];
	char            m_szResourceFolder[256];


	CHlsPublishCfg *m_pHlsPublishCfg;
	CHlsServerConfig *m_pHlsServerConfig;

	int             m_fEnableHlsServer;
	int             m_fEnableHlsPublish;
	COsalMutex      m_Mutex;
};
#endif