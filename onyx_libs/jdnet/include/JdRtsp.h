#ifndef __JD_RTSP_H__
#define __JD_RTSP_H__
#include <string>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#include <time.h>
#include <fcntl.h>
#include <io.h>

#define read	_read
#else
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#endif
#include "JdRtp.h"

#define RTSP_VERSION 			"1.0"

#define DEF_CT_BITRATE			1024		// kbps
#define DEF_CT_FRAME_RATE		30			// fps

#define RTP_PT_PCMU             0
#define RTP_PT_PCMA             8
#define RTP_PT_MP2T             33
#define RTP_PT_JPEG             26
#define RTP_PT_DYNAMIC_START    96
#define RTP_PT_DYNAMIC_END	    127

/*
 * Codecs supported currently
 */
#define CODEC_UNSUPPORTED		    0x0000
#define RTP_CODEC_H264				0x0001
#define RTP_CODEC_MP2T				0x0002
#define RTP_CODEC_AAC				0x0201
#define RTP_CODEC_PCMU				0x0202
#define RTP_CODEC_PCMA				0x0203


typedef enum _RTSP_METHOD_ID_T
{
	RTSP_METHOD_DESCRIBE,
	RTSP_METHOD_ANNOUNCE,
	RTSP_METHOD_GET_PARAMETER,
	RTSP_METHOD_OPTIONS,
	RTSP_METHOD_PAUSE,
	RTSP_METHOD_PLAY,
	RTSP_METHOD_RECORD,
	RTSP_METHOD_SETUP,
	RTSP_METHOD_SET_PARAMETER,
	RTSP_METHOD_TEARDOWN,
	RTSP_METHOD_UNKNOWN
} RTSP_METHOD_ID_T;

class CRtspRequest
{
public:
	CRtspRequest()
	{
		mCSeq = 0;
		mSession = 0;
		mRangeStart = 0;
		mRangeEnd = 0;
	}
	RTSP_METHOD_ID_T   mMethod;
	std::string        mUrlRoot;
	std::string        mUrlMedia;
	unsigned long      mCSeq;

	unsigned long      mSession;

	unsigned long      mRangeStart;
	unsigned long      mRangeEnd;

};

class CRtspRequestTransport
{
public:
	CRtspRequestTransport()
	{
		mServerRtpPort = 0;
		mServerRtcpPort = 0;
		mClientRtpPort = 0;
		mClientRtcpPort = 0;
		mMulticastRtpPort = 0;
		mMulticastRtcpPort = 0;
		mMulticast = false;
	}
	
	bool GetRtpPorts(CRtp *pRtp, int nMode)
	{
		if(nMode == CRtp::MODE_CLIENT){
			if(mMulticast) {
				pRtp->mLocalRtcpPort = mMulticastRtcpPort;
				pRtp->mLocalRtpPort = mMulticastRtpPort;
			} else {
				pRtp->mLocalRtcpPort = mClientRtcpPort;
				pRtp->mLocalRtpPort = mClientRtpPort;
			}
			pRtp->mRemoteRtcpPort = mServerRtcpPort;
			pRtp->mRemoteRtpPort = mServerRtpPort;
		} else {
			if(mMulticast) {
				pRtp->mLocalRtcpPort = mMulticastRtcpPort;
				pRtp->mLocalRtpPort = mMulticastRtpPort;
				if(mDestination.size())
					pRtp->m_SockAddr.s_addr = inet_addr(mDestination.c_str());
			} else {
				pRtp->mLocalRtcpPort = mServerRtcpPort;
				pRtp->mLocalRtpPort = mServerRtpPort;
			}
			pRtp->mRemoteRtcpPort = mClientRtcpPort;
			pRtp->mRemoteRtpPort = mClientRtpPort;
		}
		return true;
	}
	/** 
	 * Finds the next transport parameter
	 * 
	 * Transport           =    "Transport" ":"
	 *							1\#transport-spec
	 * transport-spec      =    transport-protocol/profile[/lower-transport]
	 *							*parameter
	 * transport-protocol  =    "RTP"
	 * profile             =    "AVP"
	 * lower-transport     =    "TCP" | "UDP"
	 * parameter           =    ( "unicast" | "multicast" )
	 * 				   |    ";" "destination" [ "=" address ]
	 * 				   |    ";" "interleaved" "=" channel [ "-" channel ]
	 * 				   |    ";" "append"
	 * 				   |    ";" "ttl" "=" ttl
	 * 				   |    ";" "layers" "=" 1*DIGIT
	 * 				   |    ";" "port" "=" port [ "-" port ]
	 * 				   |    ";" "client_port" "=" port [ "-" port ]
	 * 				   |    ";" "server_port" "=" port [ "-" port ]
	 * 				   |    ";" "ssrc" "=" ssrc
	 * 				   |    ";" "mode" = <"> 1\#mode <">
	 * ttl                 =    1*3(DIGIT)
	 * port                =    1*5(DIGIT)
	 * ssrc                =    8*8(HEX)
	 * channel             =    1*3(DIGIT)
	 * address             =    host
	 * mode                =    <"> *Method <"> | Method
	 */

	int ParseTransport(const char *str)
	{
		CRtspRequestTransport *pTrans = this;
		const char *pTransportSpec = strstr(str, "Transport:");
		const char *p = NULL;
		const char *q = NULL;
		if (!pTransportSpec)
			return -1;

		for(p = pTransportSpec; p!=NULL; p= NextTransportSpec(p)) {
			p = pTransportSpec + strlen("Transport: ");
			if( strncmp( p, "RTP/AVP", 7 ) == 0 ){
				p += 7;
				if( strncmp( p, "/UDP", 4 ) == 0 ){
                    p += 4;
					pTrans->mLowertTransport = "UDP";
				}
                if( strchr( ";,", *p ) == NULL )
                    continue;

				 const char *pParam = NextTransportParam(p);
				 for(q = pParam; q!=NULL; q= NextTransportParam(q)) {
                    unsigned RtpPort = 0, RtcpPort = 0;
					if( strncmp(q, "multicast", 9 ) == 0) {
						pTrans->mMulticast = true;
					}else if( strncmp( q, "unicast", 7 ) == 0 ) {
                        pTrans->mMulticast = false;
					} else if( sscanf( q, "client_port=%u-%u", &RtpPort, &RtcpPort ) == 2 ) {
						pTrans->mClientRtpPort = (unsigned short)RtpPort;
						pTrans->mClientRtcpPort = (unsigned short)RtcpPort;
					} else if( sscanf( q, "port=%u-%u", &RtpPort, &RtcpPort ) == 2 ) {
						pTrans->mMulticastRtpPort = (unsigned short)RtpPort;
						pTrans->mMulticastRtcpPort = (unsigned short)RtcpPort;
					} else if( sscanf( q, "server_port=%u-%u", &RtpPort, &RtcpPort ) == 2 ) {
						pTrans->mServerRtpPort = (unsigned short)RtpPort;
						pTrans->mServerRtcpPort = (unsigned short)RtcpPort;
                    } else if( strncmp( q,"destination=", 12 ) == 0 ) {
						char szTmp[256];
						sscanf(q,"destination=%s",szTmp);
                        pTrans->mDestination = szTmp;
                    } else if( strncmp( q,"layers=", 7 ) == 0 ) {
						char szTmp[256];
						sscanf(q,"layers=%s",szTmp);
                        pTrans->mLayers = szTmp;
                    } else if( strncmp( q,"ttl=", 4 ) == 0 ) {
						char szTmp[256];
						sscanf(q,"ttl=%s",szTmp);
                        pTrans->mTtl = szTmp;
                    } else if( strncmp( q,"ssrc=", 5 ) == 0 ) {
						char szTmp[256];
						sscanf(q,"ssrc=%s",szTmp);
                        pTrans->mSsrc = szTmp;
                    } else if( strncmp( q,"mode=", 5 ) == 0 ) {
						char szTmp[256];
						sscanf(q,"mode=%s",szTmp);
                        pTrans->mMode = szTmp;
                    } else if( strncmp( q,"interleaved=", 12 ) == 0 ) {
						char szTmp[256];
						sscanf(q,"interleaved=%s",szTmp);
                        pTrans->mInterleaved = szTmp;
                    } else {
						// Unsupported option
						//break;
						continue;
                    }
				}
			}
		}
		return 0;

	}
	
	int ParseTransportHeaderFiled(const char *str)
	{
		CRtspRequestTransport *pTrans = this;

		const char *p = str;
		const char *q = NULL;

		if( strncmp( p, "RTP/AVP", 7 ) == 0 ){
			p += 7;
			if( strncmp( p, "/UDP", 4 ) == 0 ){
                p += 4;
				pTrans->mLowertTransport = "UDP";
			}
            if( strchr( ";,", *p ) == NULL )
                return 0;

				const char *pParam = NextTransportParam(p);
				for(q = pParam; q!=NULL; q= NextTransportParam(q)) {
                unsigned RtpPort = 0, RtcpPort = 0;
				if( strncmp(q, "multicast", 9 ) == 0) {
					pTrans->mMulticast = true;
				}else if( strncmp( q, "unicast", 7 ) == 0 ) {
                    pTrans->mMulticast = false;
				} else if( sscanf( q, "client_port=%u-%u", &RtpPort, &RtcpPort ) == 2 ) {
					pTrans->mClientRtpPort = (unsigned short)RtpPort;
					pTrans->mClientRtcpPort = (unsigned short)RtcpPort;
				} else if( sscanf( q, "port=%u-%u", &RtpPort, &RtcpPort ) == 2 ) {
					pTrans->mMulticastRtpPort = (unsigned short)RtpPort;
					pTrans->mMulticastRtcpPort = (unsigned short)RtcpPort;
				} else if( sscanf( q, "server_port=%u-%u", &RtpPort, &RtcpPort ) == 2 ) {
					pTrans->mServerRtpPort = (unsigned short)RtpPort;
					pTrans->mServerRtcpPort = (unsigned short)RtcpPort;
                } else if( strncmp( q,"destination=", 12 ) == 0 ) {
					char szTmp[256];
					sscanf(q,"destination=%s",szTmp);
                    pTrans->mDestination = szTmp;
                } else if( strncmp( q,"layers=", 7 ) == 0 ) {
					char szTmp[256];
					sscanf(q,"layers=%s",szTmp);
                    pTrans->mLayers = szTmp;
                } else if( strncmp( q,"ttl=", 4 ) == 0 ) {
					char szTmp[256];
					sscanf(q,"ttl=%s",szTmp);
                    pTrans->mTtl = szTmp;
                } else if( strncmp( q,"ssrc=", 5 ) == 0 ) {
					char szTmp[256];
					sscanf(q,"ssrc=%s",szTmp);
                    pTrans->mSsrc = szTmp;
                } else if( strncmp( q,"mode=", 5 ) == 0 ) {
					char szTmp[256];
					sscanf(q,"mode=%s",szTmp);
                    pTrans->mMode = szTmp;
                } else if( strncmp( q,"interleaved=", 12 ) == 0 ) {
					char szTmp[256];
					sscanf(q,"interleaved=%s",szTmp);
                    pTrans->mInterleaved = szTmp;
                } else {
					// Unsupported option
					//break;
					continue;
                }
			}
		}

		return 0;

	}

	const char *NextTransportSpec(const char *str )
	{
		/* Looks for comma */
		str = strchr( str, ',' );
		if( str == NULL )
			return NULL; /* No more transport options */

		str++; /* skips comma */
		while( strchr( "\r\n\t ", *str ) )
			str++;

		return (*str) ? str : NULL;
	}

	const char *NextTransportParam( const char *str )
	{
		while( strchr( ",;", *str ) == NULL )
			str++;

		return (*str == ';') ? (str + 1) : NULL;
	}

public:
	unsigned short mServerRtpPort;
	unsigned short mServerRtcpPort;
	unsigned short mClientRtpPort;
	unsigned short mClientRtcpPort;
	unsigned short mMulticastRtpPort;
	unsigned short mMulticastRtcpPort;
	bool           mMulticast;
	std::string	   mTransportProtocol;
	std::string	   mLowertTransport;
	std::string	   mProfile;
	std::string	   mTtl;
	std::string	   mLayers;
	std::string	   mSsrc;
	std::string	   mMode;
	std::string	   mDestination;
	std::string	   mInterleaved;
};

class CRtspUtil
{
public:
static bool ParseClientRequest(
		const char *pszRequest,
		CRtspRequest &clntReq
		)
{
	char strMethod[32]={0};
	char strUrl[256]={0};
	char strVersion[32]={0};

	sscanf(pszRequest,"%s %s %s",strMethod, strUrl, strVersion);
	clntReq.mMethod = covertMethodStringToId(strMethod);
	char *pSlash = strstr(strUrl + strlen("RTSP://"),"/");
	if(pSlash) {
		clntReq.mUrlRoot.copy(strUrl, pSlash - strUrl);
		clntReq.mUrlMedia = pSlash + 1;
	}
	const char *pIndex = strstr(pszRequest, "CSeq: ");
	if(pIndex) {
		pIndex += strlen("CSeq: ");
		sscanf(pIndex, "%d",&clntReq.mCSeq);
	}
	if(clntReq.mMethod == RTSP_METHOD_PLAY || clntReq.mMethod == RTSP_METHOD_TEARDOWN){
		pIndex = strstr(pszRequest, "Session: ");
		if(pIndex) {
			pIndex += strlen("Session: ");
			sscanf(pIndex, "%d",&clntReq.mSession);
		}
	}

	if(clntReq.mMethod == RTSP_METHOD_SETUP){

	}
	return true;
}

static RTSP_METHOD_ID_T covertMethodStringToId(const char *pszRequest)
{
	static const char *strRtspMethods[] =
	{
		"DESCRIBE",			//RTSP_METHOD_DESCRIBE,
		"ANNOUNCE",			//RTSP_METHOD_ANNOUNCE,
		"PARAMETER",		//RTSP_METHOD_GET_PARAMETER,
		"OPTIONS",			//RTSP_METHOD_OPTIONS,
		"PAUSE",			//RTSP_METHOD_PAUSE,
		"PLAY",				//RTSP_METHOD_PLAY,
		"RECORD",			//RTSP_METHOD_RECORD,
		"SETUP",			//RTSP_METHOD_SETUP,
		"SET_PARAMETER",	//RTSP_METHOD_SET_PARAMETER,
		"TEARDOWN"			//RTSP_METHOD_TEARDOWN
	};

	int nNumMethods = sizeof(strRtspMethods) / sizeof(char *);
	for(int i = 0; i < nNumMethods; i++ ) {
		if(strcmp(pszRequest, strRtspMethods[i]) == 0)
			return (RTSP_METHOD_ID_T)i;
	}
	return RTSP_METHOD_UNKNOWN;
}

};

class CRtspSession
{
public:
	CRtspSession()
	{	
		srand( (unsigned)time( NULL ) );
		m_ulSeq = rand() << 16 | rand();
		m_hSock = -1;
		m_ulSessionId = 0;

		m_usServerRtspPort;
		m_Status = 0;
		m_lCtBitrate = DEF_CT_BITRATE;
		m_lFrameRate = DEF_CT_FRAME_RATE;

	}
	virtual ~CRtspSession()
	{
	}
	void GetTime(char *szTime, int nMaxLen)
	{
		time_t now;
	    time (&now);
		struct tm *ut = gmtime(&now);
        static const char szDays[7][4] = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
        static const char szMonths[12][4] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
		
		snprintf(szTime, nMaxLen,
				"Date", "%s, %02u %s %04u %02u:%02u:%02u GMT",
				szDays[ut->tm_wday], ut->tm_mday, szMonths[ut->tm_mon],
				1900 + ut->tm_year, ut->tm_hour, ut->tm_min, ut->tm_sec);
	}




	virtual int CreateHeader(int nHeaderId, char *pszTrack, char *pBuff, int nMaxLen) {return -1;}
	virtual int SetUrl(const char *pszUri)
	{
		mUrl = pszUri;
		return 0;
	}
	static void GetCSeq(const char *szRqBuff, unsigned long *pulSeq)
	{
		*pulSeq = 0;
		const char *pIndex = strstr(szRqBuff, "CSeq: ");
		if(pIndex) {
			pIndex += strlen("CSeq: ");
			sscanf(pIndex, "%d",pulSeq);
		}
	}

public:
	unsigned long m_ulSessionId;
	unsigned long m_ulSeq;
	std::string	mUrl;		// Resource URI of the item used in setup
	int m_hSock;

	unsigned short m_usServerRtspPort;
	int m_Multicast;
	int m_Status;
	CRtp	*m_pVRtp;
	CRtp	*m_pARtp;
	in_addr m_PeerAddr;
	in_addr m_HostNatAddr;
	in_addr m_HostLanAddr;
	long	m_lCtBitrate;		// In kbps
	long	m_lFrameRate;		// fps
};

#endif // __JD_RTSP_H__