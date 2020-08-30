#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "JdDbg.h"
#include "HlsSrvBridge.h"

#ifdef NO_DEBUG_LOG
#define DBG_PRINT(...)
#else
#define DBG_PRINT(...)                                                \
          if (1)                                                      \
          {                                                           \
            printf(__VA_ARGS__);                                      \
          }
#endif

static int  modDbgLevel = CJdDbg::LVL_TRACE;
int callbackHttpRoot(int hSock, char *pszUri)
{
	return hlsServerHandler(hSock, pszUri);
}

int CHlsSrvBridge::SendVideo(unsigned char *pData, int size, unsigned long lPts)
{
	long hr = 0;
	int fSps = 0;
	CAccessUnit Au;
	Au.m_TSStreamID = 0;
	Au.m_SampleStreamType = SAMPLE_TYPE_H264;
	Au.m_pRawData = (char *)pData;
	Au.m_RawSize = size;
	Au.m_pTsData = m_pTsBuffer;
	Au.m_PTS = lPts;
	Au.m_DTS = Au.m_PTS;
	
	JDBG_LOG(CJdDbg::LVL_STRM,("size=%d", size));
	// TODO: Make more generic
	if(pData[4] == 0x27 && pData[3] == 0x01 && pData[2] == 0x00 && pData[1] == 0x00 && pData[0] == 0x00)
		fSps = 1;
	//DumpHex(pData, 32);
	//DBG_PRINT("CHlsSrvBridge::SendVideo size=%d pts=%d(ms)\n", size, lPts / 1000);
	m_TsMux.Mux(&Au);
	m_Mutex.Acquire();
	if (hlsWriteFrameData(m_pSegmenter, m_pTsBuffer, Au.m_TsSize, 1, fSps, lPts / 90) < 0 ) {
		hr = -1;
	}
	m_Mutex.Release();
	return hr;
}

int CHlsSrvBridge::SendAudio(unsigned char *pData, int size, unsigned long lPts)
{
	long hr = 0;
	int fSps = 0;
	CAccessUnit Au;
	Au.m_TSStreamID = 0;
	Au.m_SampleStreamType = SAMPLE_TYPE_AAC;
	Au.m_pRawData = (char *)pData;
	Au.m_RawSize = size;
	Au.m_pTsData = m_pTsBuffer;
	Au.m_PTS = lPts;
	Au.m_DTS = Au.m_PTS;
	
	m_TsMux.Mux(&Au);
	m_Mutex.Acquire();
	if (hlsWriteFrameData(m_pSegmenter, m_pTsBuffer, Au.m_TsSize, 1, fSps, lPts / 90) < 0 ) {
		hr = -1;
	}
	m_Mutex.Release();
	return hr;
}

int CHlsSrvBridge::Run(COutputStream *pOutputStream)
{
	strcpy(m_szFilePrefix, pOutputStream->m_szStreamName);
	m_pSegmenter = hlsCreateSegmenter();
		
	if(m_fEnableHlsServer){
		int nUpLoadType;
		if(m_pHlsServerConfig->m_fInternaHttpSrv) {
			m_pHttpSrv = new CJdHttpSrv;
			if(m_pHttpSrv) {
				DBG_PRINT("Creating HLS Server port=%d application=%s prefix=%s\n", m_pHlsServerConfig->m_usServerRtspPort, m_szResourceFolder, m_szFilePrefix);
				m_pHttpSrv->RegisterCallback(m_szResourceFolder, callbackHttpRoot);
				m_pHttpSrv->Run(0, m_pHlsServerConfig->m_usServerRtspPort);
			}
		} 
		nUpLoadType = UPLOADER_TYPE_MEM;
		char szM3u8File[128];
		//hlsSetDebugLevel(m_pHlsPublishCfg->m_nDdebugLevel);
		nUpLoadType = UPLOADER_TYPE_MEM;
		sprintf(szM3u8File, "%s.m3u8", m_pHlsServerConfig->m_szStream);
		m_pUploaderForExtHttpSrv = hlsPublishStart(-1, m_pSegmenter, szM3u8File, m_pHlsServerConfig->m_szFolder, 
			m_pHlsServerConfig->m_szServerRoot,  NULL,  NULL, NULL, 
			m_pHlsServerConfig->m_fLiveOnly, m_pHlsServerConfig->m_nSegmentDuration, nUpLoadType);
	}
		
	if(m_fEnableHlsPublish){
		int nUpLoadType;
		char szM3u8File[128];

		hlsSetDebugLevel(m_pHlsPublishCfg->m_nDdebugLevel);
		if(strcmp(m_pHlsPublishCfg->m_szProtocol,"http://") == 0)
			nUpLoadType = UPLOADER_TYPE_S3;
		else 
			nUpLoadType = UPLOADER_TYPE_MEM;
		sprintf(szM3u8File, "%s.m3u8", m_pHlsPublishCfg->m_szStream);
		m_pUploader = hlsPublishStart(-1, m_pSegmenter, szM3u8File, m_pHlsPublishCfg->m_szFolder, 
										m_pHlsPublishCfg->m_pszBucket,  m_pHlsPublishCfg->m_pszHost,  
										m_pHlsPublishCfg->m_szAccessId,  m_pHlsPublishCfg->m_szSecKey, 
										m_pHlsPublishCfg->m_fLiveOnly, m_pHlsPublishCfg->m_nSegmentDuration, nUpLoadType);
	}
	m_TsMux.Start();
	return 0;
}
