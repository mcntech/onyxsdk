#ifndef __RTMP_PUBLISH_BRIDGE__
#define __RTMP_PUBLISH_BRIDGE__
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

#include "StrmOutBridgeBase.h"
#include <vector>
extern "C" {
#include <rtmp.h>
}

#define MAX_RTMP_SERVERS        2
#define SAMPLE_TYPE_H264	1
#define SAMPLE_TYPE_AAC		2

namespace RTMPUTIL {
class CAvcCfgRecord 
{
#define MAX_SPS_SIZE		256
#define MAX_PPS_SIZE		256

public:
	CAvcCfgRecord()
	{
		mSpsPpsFound = false;
		mSentConfig = false;
		mSpsSize = 0;
		mPpsSize = 0;
		memset(mSps, 0x00, MAX_SPS_SIZE);
		memset(mPps, 0x00, MAX_SPS_SIZE);
		mSpsSize = mPpsSize = 0;
	}
	unsigned char mVersion;
	unsigned char mProfile;
	unsigned char mProfileCompat;
	unsigned char mLevel; 

	unsigned char	mSpsSize;
	unsigned char	mSps[MAX_SPS_SIZE];
	unsigned char	mPpsSize;
	unsigned char	mPps[MAX_PPS_SIZE];
	bool             mSpsPpsFound;
	bool             mSentConfig;
int UpdateAvccSps(unsigned char *pSpsData, long lSpsSize)
{
	mVersion = 1;
	mProfile = pSpsData[1]; /* profile */

	mProfileCompat = 0; /* profile compat */
	mLevel = pSpsData[3];

	lSpsSize = lSpsSize;
	memcpy( mSps, pSpsData, lSpsSize);
	mSpsSize = lSpsSize;
	//mSpsPpsFound = true;
	return 0;
}

int UpdateAvccPps(char *pPpsData, long lPpsSize)
{
	lPpsSize = lPpsSize;
	memcpy( mPps, pPpsData, lPpsSize);
	mPpsSize = lPpsSize;
	mSpsPpsFound = true;
	return 0;
}
static int ConvertToBase64(unsigned char *pDest, const unsigned char *pSrc, int nLen)
{
    unsigned char    *pTmp;
    int   i;

    pTmp = pDest;
	static const unsigned char Base64Char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (i = 0; i < nLen - 2; i += 3)   {
        *pTmp++ = Base64Char[(pSrc[i] >> 2) & 0x3F];
        *pTmp++ = Base64Char[((pSrc[i] & 0x3) << 4) | ((pSrc[i + 1] & 0xF0) >> 4)];
        *pTmp++ = Base64Char[((pSrc[i + 1] & 0xF) << 2) |((pSrc[i + 2] & 0xC0) >> 6)];
        *pTmp++ = Base64Char[pSrc[i + 2] & 0x3F];
    }

    /* Convert last 1 or 2 bytes */
    if (i < nLen)   {
        *pTmp++ = Base64Char[(pSrc[i] >> 2) & 0x3F];
        if (i == (nLen - 1))  {
            *pTmp++ = Base64Char[((pSrc[i] & 0x3) << 4)];
            *pTmp++ = '=';
        }  else  {
            *pTmp++ = Base64Char[((pSrc[i] & 0x3) << 4) | ((pSrc[i + 1] & 0xF0) >> 4)];
            *pTmp++ = Base64Char[((pSrc[i + 1] & 0xF) << 2)];
        }
        *pTmp++     = '=';
    }
    *pTmp++ = '\0';
    return pTmp - pDest;
}
};

class CAacCfgRecord
{
public:
	CAacCfgRecord() 
	{
		mInit = false;
		mSentConfig = false;
	}

	unsigned char mConfig[2];
	bool          mInit;
	bool          mSentConfig;

	void UpdateConfigFromAdts(char *pAdts)
	{
		// TODO: Validate
		int profileVal      = pAdts[2] >> 6;
		int sampleRateId    = (pAdts[2] >> 2 ) & 0x0f;
		int channelsVal     = ((pAdts[2] & 0x01) << 2) | ((pAdts[3] >> 6) & 0x03);
		mConfig[0]  = (profileVal + 1) << 3 | (sampleRateId >> 1);
		mConfig[1]  = ((sampleRateId & 0x01) << 7) | (channelsVal <<3);
		mInit = true;
	}
};
}
class CRtmpPublishBridge : public CStrmOutBridge
{
public:
	CRtmpPublishBridge();
	~CRtmpPublishBridge();

	int Run(COutputStream *pOutputStream)
	{
		PrepareMediaDelivery(pOutputStream);
		return -1;
	}
	void SetServerConfig(CRtmpPublishConfig *pRtmpPublishConfig);
//private:
	int PrepareMediaDelivery(COutputStream *pOutputStream);
	unsigned char *FindStartCode(unsigned char *p, unsigned char *end);

	int SendVideo(unsigned char *pData, int size, unsigned long lPts);
	int SendAudio(unsigned char *pData, int size, unsigned long lPts);
	int ConnectToPublishServer(CRtspPublishConfig *pRtspPublishCfg);

public:
	CRtmpPublishConfig   *m_pRtmpPublishCfg;

public:
	unsigned short   mRtmpPort;


	int              m_fEnableAud;
	int              m_fEnableVid;
	RTMPUTIL::CAvcCfgRecord    mAvcCfgRecord;
	RTMPUTIL::CAacCfgRecord	   mAacConfig;

	unsigned long    m_ulVidPtsStart;
	int WriteRtmpPacket(
		unsigned char       nStrmType, 
		unsigned long ulTimeStamp, 
		const unsigned char *pData, 
		int           size,
		const unsigned char *pCodecTag,
		int           sizeTag
		);

	std::vector      <RTMP *>m_pRtmpServers;
	char             m_urlRtmpServers[1024];
	bool             m_fRecordServer[MAX_RTMP_SERVERS];
	char             *m_pVidRtmpBuffer;
	char             *m_pAudRtmpBuffer;
};

#endif