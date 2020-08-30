#ifndef __JD_RTP_CLNT_H__
#define __JD_RTP_CLNT_H__
#include "JdRtsp.h"
#include "JdSdp.h"
#include <map>

#define RTSP_CLIENT_NAME        "OnyxRtspClient"

#ifndef _WIN32
	#define strnicmp strncasecmp
//	#define _stricmp strcasecmp
#endif


typedef struct _RTSPRespHeaders
{
	int n;
} RTSPRespHeaders;
typedef std::map <std::string, std::string> header_map_t;

/*
 a=rtpmap:<payload type> <encoding name>/<clock rate>[/<encoding parameters>]
*/
class CAttribRtpmap
{
public:
	CAttribRtpmap(const char *rtmap_value)
	{
		char *pTmp;
		char szParam[128] = {0};
		sscanf (rtmap_value,"%d %64s", &nPlType, szParam);
		if(strlen(szParam)){
			pTmp = strtok(szParam, "/");
			if(pTmp) {
				EncodingName = pTmp;
				pTmp = strtok(NULL, "/");
				if(pTmp) {
					nClockRate = atoi(pTmp);
					pTmp = strtok(NULL, "/");
					if(pTmp) {
						EncodingParam = pTmp;
					}
				}
			}
		}
	}
	~CAttribRtpmap(){}
	int nPlType;
	std::string EncodingName;
	int nClockRate;
	std::string EncodingParam;
};

class CAttribControl
{
public:
	CAttribControl(const char *resource_url, const char *control_value)
	{
		if (strnicmp(control_value,"rtsp://", strlen("rtsp://")) == 0){
			fRelativeUri = 0;
			uri = control_value;
		} else {
			fRelativeUri = 1;
			uri = resource_url;
			uri += "/";
			uri += control_value;
		}
	}
	~CAttribControl(){}
	std::string uri;
	int fRelativeUri;
};

//"%d packetization-mode=%d;profile-level-id=%d;sprop-parameter-sets=%s,%s;"
class CAttribFmtp
{
#define MAX_SPS_SIZE	256
public:
	CAttribFmtp(const char *fmtp_value) :mSpsSize(0), mSpsConfig(NULL)
	{
		mSpsSize = 0;
		const char *pTmp;
		std::string strSpsConfig;
		sscanf (fmtp_value,"%d",&mPlType);
		if(mPlType) {
			pTmp=strstr(fmtp_value,"sprop-parameter-sets=");
			if(pTmp){
				pTmp += strlen("sprop-parameter-sets=");
				int len = 0;
				const char *pTmp1 = strstr(pTmp,",");
				if(pTmp1 && pTmp1 > pTmp)
					len = pTmp1 - pTmp;
				else 
					len = strlen(pTmp);
				strSpsConfig.assign(pTmp, len); 
				mSpsConfig = (unsigned char *)malloc(strSpsConfig.size());
				mSpsSize = Base64DecodeBinaryToBuffer(
					mSpsConfig, (const unsigned char *)strSpsConfig.c_str(), strSpsConfig.size());
			}
		}
	}

	int Base64DecodeBinaryToBuffer(unsigned char *pDst, const unsigned char *pSrc, int srcSize)
	{
		static const int b64[256] = {
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
			52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
			-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
			15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
			-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
			41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
			-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
		};                                                    /* Base64 Buffer    */
		const unsigned char *pStrt = pDst;                           /* Start Pointer    */
		const unsigned char *pTmp = pSrc;                    /* Temporary Pointer*/

		int level;                                            /* Level of buffer  */
		int last;                                             /* Last in buffer   */

		for( level=0, last=0; (int)(pDst - pStrt ) < srcSize && *pTmp != '\0'; pTmp++ ){
			const int c = b64[(unsigned int)*pTmp];
			if( c == -1 )
				continue;

			switch( level )
			{
				case 0:
					level++;
					break;
				case 1:
					*pDst++ = ( last << 2 ) | ( ( c >> 4)&0x03 );
					level++;
					break;
				case 2:
					*pDst++ = ( ( last << 4 )&0xf0 ) | ( ( c >> 2 )&0x0f );
					level++;
					break;
				case 3:
					*pDst++ = ( ( last &0x03 ) << 6 ) | c;
					level = 0;
			}
			last = c;
		}
		return pDst - pStrt; /* returning the size of decoded buffer */
	}

	~CAttribFmtp()
	{
		if(mSpsConfig)
			free(mSpsConfig);
	}
	int           mPlType;
	unsigned char *mSpsConfig;
	int           mSpsSize;
};


class CJdRtspClntSession : public CRtspSession
{
public:
	CJdRtspClntSession()
	{
		m_hSock = -1;
		m_contentLength = 0;
		m_pszTransport = NULL;
		m_pVRtp = NULL;
		m_pARtp = NULL;
	}
	virtual ~CJdRtspClntSession()
	{
		if(m_pVRtp)
			m_pVRtp->CloseSession();
		delete m_pVRtp;

		if(m_pARtp)
			m_pARtp->CloseSession();
		delete m_pARtp;

		if(m_pszTransport)
			free(m_pszTransport);
	}

	virtual int Open(const char *pszUrl, int *pnVidCodec, int *pnAudCodec);
	virtual int SendSetup(  char *szStrmType,					// audio or video
					unsigned short rtpport, 
					unsigned short rtcpport);

	virtual void SendPlay(char *szStrmType);
	virtual void Close();
	int GetVideoCodec();
	bool GetVideoCodecConfig(unsigned char *pCfg, int *pnSize);
	int GetAudioCodec();
	int GetVData(char *pData, int nMaxLen)
	{
		if(m_pVRtp)
			return m_pVRtp->Read(pData, nMaxLen);
	}
	int GetAData(char *pData, int nMaxLen)
	{
		if(m_pARtp)
			return m_pARtp->Read(pData, nMaxLen);
	}

	virtual int CreateHeader(
				int   nHeaderId,  
				char  *pszTrack, 
				char  *pBuff, 
				int   nMaxLen);

	virtual int CreateSetupHeader(
				char           *pszTrack,
				unsigned short usRtpPort,
				unsigned short usRtcpPort,
				char           *pBuff, 
				int             nMaxLen);

	virtual void HandleAnswerSetup(char *szStrmType, char *headerBuf);
	virtual void HandleAnswerDescribe(char *headerBuf);
	//virtual void HandleAnswerPlay();
	//virtual void HandleAnswerTearDown();
	
	int ParseResponseMessage(char *pData, int nLen);

	int	m_hSock;
	int m_contentLength;

public:
	int makeSocket(
			const char *host, 
			unsigned short port, 
			int sock_type);

	char *m_pszTransport;
	std::string m_pszUrl;
	CSdp m_sdp;
	header_map_t resp_headers;
};

#endif	// __JD_RTP_CLNT_H__