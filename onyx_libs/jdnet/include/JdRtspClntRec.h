#ifndef __JD_RTP_CLNT_REC_H__
#define __JD_RTP_CLNT_REC_H__
#include "JdRtspClnt.h"
#include "JdRtspSrv.h"
#include "JdAuthRfc2617.h"

class CJdRtspClntRecSession : public CJdRtspClntSession
{
public:
	CJdRtspClntRecSession(CMediaResMgr *pMediaResMgr) : mJdDbg("CJdRtspClntRecSession", CJdDbg::LVL_MSG)
	{
		mpStreamServer = NULL;
		mpMediaResMgr = pMediaResMgr;
	}
	virtual ~CJdRtspClntRecSession()
	{
	}

	int Open(const char *pszUrl, const char *szAppName, unsigned long usServerPort);

	int OpenRtpSnd(
		const char     *pszHost, 
		const char     *nameAggregate,
		const char     *nameTrack, 
		unsigned short usRtpLocalPort,
		unsigned short usRtpRemotePort);
	
	int SendSetup(  char *szStrmType,					// audio or video
					unsigned short rtpport, 
					unsigned short rtcpport);

	virtual void Close();

	void HandleAnswerSetup(char *szStrmType, char *headerBuf);
	int HandleAnswerAnnounce(char *headerBuf);
	void HandleAnswerRecord(){}
	int SendAnnounce(const char *szMedia);
	int GenerateSdp(CSdp *pSdp, CMediaResMgr *pMediaResMgr, const char *pszMedia);
	void HandleAnswerAnnounce(char *szStrmType, char *headerBuf);
	bool PlayTrack(CMediaTrack *pTrack);
	int StartPublish(const char *nameAggregate, const char *nameTrack);

	int CreateAnnounceHeader(
			const char     *pszUrl,
			const char     *pszMedia,
			int            nContentLen,
			char           *pBuff, 
			int             nMaxLen);

	int CreateRecordHeader(
			const char     *pszUrl,
			const char     *pszMedia,
			char           *pBuff, 
			int             nMaxLen);
	int SendRecord(const char *szMedia);
	int HandleAnswerRecord(char *headerBuf);

	int CreateSetupHeader(
			char           *pszTrack,
			unsigned short usRtpPort,
			unsigned short usRtcpPort,
			char           *pBuff, 
			int             nMaxLen);

public:	
	CRtpSnd					*m_pVRtp;
	CRtpSnd					*m_pARtp;
	CRtspRequestTransport mTransport;
	CJdRtspSrvSession *mpStreamServer;
	CMediaResMgr      *mpMediaResMgr;
	std::map<std::string, IMediaDelivery *> mTrackDeliveries;

	CJdDbg            mJdDbg;
	CJdAuthRfc2167    mAuth;
	std::string       m_Username;
	std::string       m_Password;
};

#endif	// __JD_RTP_CLNT_REC_H__