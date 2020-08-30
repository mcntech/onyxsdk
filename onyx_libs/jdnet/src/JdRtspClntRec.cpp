#include "JdRtspClntRec.h"
#include "JdNetUtil.h"

#include "JdSdp.h"
#include "JdRtpSnd.h"
#include "JdRfc2250.h"
#include "JdRfc3984.h"
#include "JdRfc3640.h"

#ifdef WIN32
#define socklen_t	int
#endif

#define REQUEST_BUF_SIZE 		1024
#define HEADER_BUF_SIZE 		1024

#define DEF_RTSP_PORT 			554
#define DEFAULT_REDIRECTS       3       /* Number of HTTP redirects to follow */
static int modDbgLevel = CJdDbg::LVL_TRACE;

#define CHECK_ALLOC(x) if(x==NULL) {fprintf(stderr,"malloc failed at %s %d, exiting..\n", __FUNCTION__, __LINE__); goto Exit;}
static int followRedirects = DEFAULT_REDIRECTS;	/* # of redirects to follow */


int CJdRtspClntRecSession::Open(const char *pszHost, const char *szAppName,  unsigned long usServerPort)
{
	int sock = -1;
	int result = -1;
	char szUrl[1024];
	m_usServerRtspPort = usServerPort;
	sprintf(szUrl, "rtsp://%s:%d/%s", pszHost,usServerPort, szAppName);
	m_pszUrl = szUrl;
	sock = makeSocket(pszHost, m_usServerRtspPort, SOCK_STREAM);		/* errorSource set within makeSocket */
	if(sock == -1) { 
		goto Exit;
	}


	/* Save the socket for further commands */
	m_hSock = sock;

	return 0;
Exit:
	if(sock != -1)
		close(sock);
	return result;
}

int CJdRtspClntRecSession::OpenRtpSnd(
	const char     *pszHost,
	const char     *nameAggregate,
	const char     *nameTrack,
	unsigned short usLocalRtpPort,
	unsigned short usRemoteRtpPort
	)
{
	int res = 0;
	mpStreamServer = new CJdRtspSrvSession(mpMediaResMgr);
	struct hostent *hp;
	
	JdDbg(CJdDbg::LVL_ERR,("Otaining host address: %s\n", pszHost));
	hp = gethostbyname(pszHost);
	CRtpSnd *pRtp = new CRtpSnd();
	memcpy((char *)&pRtp->m_SockAddr, (char *)hp->h_addr, hp->h_length);

	pRtp->mLocalRtpPort = usLocalRtpPort;
	pRtp->mLocalRtcpPort = usLocalRtpPort+1;
	pRtp->mRemoteRtpPort = usRemoteRtpPort;
	pRtp->mRemoteRtcpPort = usRemoteRtpPort+1;
	pRtp->mMulticast = false;

	mpStreamServer->m_pVRtp = pRtp;

	char *pszHostAddr = inet_ntoa(pRtp->m_SockAddr);
	if(pszHostAddr) {
		JdDbg(CJdDbg::LVL_ERR,("ip address=%s local port=%d remote port=%d\n", inet_ntoa(pRtp->m_SockAddr), usLocalRtpPort, usRemoteRtpPort));
	} else {
		JdDbg(CJdDbg::LVL_ERR,("Failed to get address\n"));
	}

	mpStreamServer->mAggregateStream = nameAggregate;
	CMediaAggregate *pMediaAggregate = mpMediaResMgr->GetMediaAggregate(nameAggregate);
	if(pMediaAggregate) {
		CMediaTrack *pTrack = pMediaAggregate->GetTrack(nameTrack);
		if(pTrack) {
			mpStreamServer->PlayTrack(pTrack);
		} else {
			JdDbg(CJdDbg::LVL_ERR,("Track for %s not found!!!\n", nameTrack));
			res = -1; goto Exit;
		}
	} else {
		JdDbg(CJdDbg::LVL_ERR,("Aggregate for %s not found!!!\n", nameTrack));
		res = -1; goto Exit;
	}
Exit:
	return res;
}

bool CJdRtspClntRecSession::PlayTrack(CMediaTrack *pTrack)
{
	IMediaDelivery *pDelivery = NULL;
	CRtpSnd *pRtp = NULL;
	bool res = false;
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	JDBG_LOG(CJdDbg::LVL_SETUP,("codec type=%d name=%s", pTrack->mCodecType, pTrack->mCodecName.c_str()));
	if(pTrack->mCodecType == CMediaTrack::CODEC_TYPE_STREAM){
		pRtp = m_pVRtp;
		if(pTrack->mCodecName.compare("MP2T") == 0){
			CJdRfc2250Tx *pVDelivery = new CJdRfc2250Tx(m_pVRtp, pTrack->mCodecId, pTrack->mSsrc);
			pDelivery = pVDelivery;
		}
	} else if(pTrack->mCodecType == CMediaTrack::CODEC_TYPE_VIDEO){
		pRtp = m_pVRtp;
		if(pTrack->mCodecName.compare("H264") == 0){
			CJdRfc3984Tx *pVDelivery = new CJdRfc3984Tx(m_pVRtp, pTrack->mCodecId, pTrack->mSsrc);
			pDelivery = pVDelivery;
		}
	} else 	if(pTrack->mCodecType == CMediaTrack::CODEC_TYPE_AUDIO){
		pRtp = m_pARtp;
		if(pTrack->mCodecName.compare(CODEC_NAME_AAC) == 0){
			CJdRfc3640Tx *pADelivery = new CJdRfc3640Tx(pRtp, pTrack->mCodecId, pTrack->mSsrc);
			pDelivery = pADelivery;
		}
	}
	if(pDelivery) {
		pRtp->CreateSession();
		pTrack->AddDelivery(pDelivery);
		// Save it in the session for future use.
		mTrackDeliveries[pTrack->mTrackName] = pDelivery;
		res = true;
		JDBG_LOG(CJdDbg::LVL_SETUP,("Delivery for %s added",  pTrack->mCodecName.c_str()));
	} else {
		JDBG_LOG(CJdDbg::LVL_SETUP,("Delivery for %s not found!!!",  pTrack->mCodecName.c_str()));
	}

	JDBG_LOG(CJdDbg::LVL_SETUP,("Leave res=%d", res));

	return res;
}

int CJdRtspClntRecSession::StartPublish(const char *nameAggregate, const char *nameTrack)
{
	int res = 0;
	CRtpSnd *pRtp = new CRtpSnd();
	CMediaAggregate *pMediaAggregate = NULL;
	pRtp->mLocalRtpPort = mTransport.mClientRtpPort;
	pRtp->mLocalRtcpPort = mTransport.mClientRtcpPort;
	pRtp->mRemoteRtpPort = mTransport.mServerRtpPort;
	pRtp->mRemoteRtcpPort = mTransport.mServerRtcpPort;
	pRtp->mMulticast = mTransport.mMulticast;
	m_pVRtp = pRtp;

	
	{
		struct sockaddr_in	addr;
		socklen_t adr_len = sizeof(addr);
		res = getpeername(m_hSock, (struct sockaddr *)&addr, &adr_len );
		if(res == 0){
			pRtp->m_SockAddr = addr.sin_addr;
		} else {
			fprintf(stderr,"%s:%s getsockname : failed\n", __FILE__, __FUNCTION__);
			goto Exit;
		}
	}
	JdDbg(CJdDbg::LVL_SETUP,("server_addr=0x%x client_port=%d server_port=%d mcast=%d\n",  *((unsigned long *)&pRtp->m_SockAddr), mTransport.mClientRtpPort,  mTransport.mServerRtpPort, pRtp->mMulticast));

	pMediaAggregate = mpMediaResMgr->GetMediaAggregate(nameAggregate);
	if(pMediaAggregate) {
		CMediaTrack *pTrack = pMediaAggregate->GetTrack(nameTrack);
		if(pTrack) {
			PlayTrack(pTrack);
		} else {
			JdDbg(CJdDbg::LVL_ERR,("Track for %s not found!!!\n", nameTrack));
			res = -1; goto Exit;
		}
	} else {
		JdDbg(CJdDbg::LVL_ERR,("Aggregate for %s not found!!!\n", nameTrack));
		res = -1; goto Exit;
	}
Exit:
	return res;
}


/**
 * Creates the Announce header
 */
int CJdRtspClntRecSession::CreateAnnounceHeader(
			const char     *pszUrl,
			const char     *pszMedia,
			int            nContentLen,
			char           *pBuff, 
			int             nMaxLen)
{
	int tempSize = 0;
	std::string control;
	snprintf(pBuff, nMaxLen, 
		"ANNOUNCE %s/%s RTSP/%s\r\n" \
		"CSeq: %d\r\n" \
		"User-Agent: %s\r\n" \
		"Content-Length: %d\r\n" \
		"Content-Type: application/sdp\r\n" \
		"\r\n",
		pszUrl, pszMedia, RTSP_VERSION, 
		++m_ulSeq,
		RTSP_CLIENT_NAME,
		nContentLen);

	return strlen(pBuff);

	return -1;
}

/**
 * Creates the Announce header
 */
int CJdRtspClntRecSession::CreateRecordHeader(
			const char     *pszUrl,
			const char     *pszMedia,
			char           *pBuff, 
			int             nMaxLen)
{
	int tempSize = 0;
	std::string control;
	snprintf(pBuff, nMaxLen, 
		"RECORD %s/%s RTSP/%s\r\n" \
		"CSeq: %d\r\n" \
		"User-Agent: %s\r\n" \
		"Session: %u\r\n" \
		"\r\n",
		pszUrl, pszMedia, RTSP_VERSION, 
		++m_ulSeq,
		RTSP_CLIENT_NAME,
		m_ulSessionId);

	return strlen(pBuff);

	return -1;
}

int CJdRtspClntRecSession::SendRecord(const char *szMedia)
{
	int bufsize = REQUEST_BUF_SIZE;
	int tempSize;
	char headerBuf[HEADER_BUF_SIZE];
	int ret = -1;
	char *pData = headerBuf;

	if(m_hSock >= 0){
		int len = CreateRecordHeader(m_pszUrl.c_str(), szMedia, pData, HEADER_BUF_SIZE);

		if(mAuth.m_AuthType == CJdAuthRfc2167::AUTH_DIGEST) {
			char *pAuthField = mAuth.MakeDigestAuth(m_Username.c_str(), m_Password.c_str(), m_pszUrl.c_str(),"ANNOUNCE");
			if(pAuthField) {
				pData += strlen(pData);
				strncpy(pData, pAuthField, strlen(pAuthField));
				pData += strlen(pAuthField);
				free(pAuthField);
			}
		}
		send(m_hSock, headerBuf, strlen(headerBuf), 0);

		ret = ReadHeader(m_hSock, headerBuf);	/* errorSource set within */
		HandleAnswerRecord(headerBuf);
		return 0;
	}
Exit:
	return -1;
}

int CJdRtspClntRecSession::HandleAnswerRecord(char *headerBuf)
{
	int nStatusCode = ParseResponseMessage(headerBuf, strlen(headerBuf));
	if(nStatusCode == 200) {
		// Do nothing
	} else if(nStatusCode == 401 && mAuth.m_AuthType == CJdAuthRfc2167::AUTH_NONE) {
	}
	return nStatusCode;
}


/**
 * Creates the setup header
 */
int CJdRtspClntRecSession::CreateSetupHeader(
			char           *pszTrack,
			unsigned short usRtpPort,
			unsigned short usRtcpPort,
			char           *pBuff, 
			int             nMaxLen)
{
	int tempSize = 0;
	std::string control;
	m_sdp.GetControl(control,pszTrack);
	CAttribControl attribControl(m_pszUrl.c_str(), control.c_str());
	{
		snprintf(pBuff, nMaxLen, 
			"SETUP %s RTSP/%s\r\n" \
			"CSeq: %d\r\n" \
			"User-Agent: %s\r\n" \
			"Transport: RTP/AVP;unicast;client_port=%u-%u\r\n" \
			"\r\n",
			attribControl.uri.c_str(), RTSP_VERSION, 
			++m_ulSeq,
			RTSP_CLIENT_NAME,
			usRtpPort, usRtcpPort);

		return strlen(pBuff);
	}
	return -1;
}

int CJdRtspClntRecSession::SendSetup( char *szStrmType,					// audio or video
				unsigned short rtpport, 
				unsigned short rtcpport)
{
	return CJdRtspClntSession::SendSetup(szStrmType, rtpport, rtcpport);
}


void CJdRtspClntRecSession::HandleAnswerSetup(char *szStrmType, char *headerBuf)
{
	int nStatusCode = ParseResponseMessage(headerBuf, strlen(headerBuf));
	if(nStatusCode == 200) {
		header_map_t::const_iterator iter = resp_headers.find("transport");
		if(iter != resp_headers.end()) {
			std::string strVal = iter->second;
			mTransport.ParseTransportHeaderFiled(strVal.c_str());	
		}
		
	} else if(nStatusCode == 401 && mAuth.m_AuthType == CJdAuthRfc2167::AUTH_NONE) {
	}
}


int CJdRtspClntRecSession::GenerateSdp(CSdp *pSdp, CMediaResMgr *pMediaResMgr, const char *pszMedia)
{
	char *pszAddr = NULL;;
	struct sockaddr_in	addr;
	int res = -1;
	COrigin *pOrigin = NULL;
	int nTracks = 0;
	CMediaAggregate *pMediaAggregate = NULL;

	JDBG_LOG(CJdDbg::LVL_SETUP,("Enter"));
	pMediaAggregate = pMediaResMgr->GetMediaAggregate(pszMedia);
	if(pMediaAggregate == NULL){
		JdDbg(CJdDbg::LVL_ERR,("%s : No MediaAggregate\n",__FUNCTION__));
		goto Exit;
	}
	nTracks = pMediaAggregate->GetTrackCount();
	{
		socklen_t adr_len = sizeof(addr);
		res = getsockname(m_hSock, (struct sockaddr *)&addr, &adr_len );
		if(res == 0){
			pszAddr = strdup(inet_ntoa(addr.sin_addr));
		} else {
			fprintf(stderr,"%s:%s getsockname : failed\n", __FILE__, __FUNCTION__);
			goto Exit;
		}
	}

	pSdp->m_pszSessionName = strdup("TestSession");
	pOrigin = new COrigin("-", 123456,1,pszAddr);
	pSdp->m_pOrigin = pOrigin;
	
	for (int i=0; i < nTracks; i++) {
		CMediaTrack *pTrack = pMediaAggregate->GetTrack(i);
		
		if(pTrack->mCodecType == CMediaTrack::CODEC_TYPE_STREAM) {
			char szTmp[128];
			CMediaDescription *pMediaDescr = new CMediaDescription(TRACK_ID_STREAM, 0/*pTrack->mUdpPortBase*/, pTrack->mCodecId, "Test Session");
			// TODO		
			//CConnection *pConnection = new CConnection("IN", "IP4", pszAddr);
			//pMediaDescr->AddConnection(pConnection);

			sprintf(szTmp, "%d %s/90000", pTrack->mCodecId, pTrack->mCodecName.c_str());
			CAttribute *pAttributeRtpmap = new CAttribute("rtpmap", szTmp);
			pMediaDescr->AddAttribute(pAttributeRtpmap);

			CAttribute *pAttributeControl = new CAttribute("control", pTrack->mTrackName.c_str());
			pMediaDescr->AddAttribute(pAttributeControl);

			pSdp->Add(pMediaDescr);
		} else if(pTrack->mCodecType == CMediaTrack::CODEC_TYPE_VIDEO) {
			char szTmp[128];
			CMediaDescription *pMediaDescr = new CMediaDescription(TRACK_ID_VIDEO, 0/*pTrack->mUdpPortBase*/, pTrack->mCodecId, "Test Session");
			// TODO		
			//CConnection *pConnection = new CConnection("IN", "IP4", pszAddr);
			//pMediaDescr->AddConnection(pConnection);

			sprintf(szTmp, "%d %s/90000", pTrack->mCodecId, pTrack->mCodecName.c_str());
			CAttribute *pAttributeRtpmap = new CAttribute("rtpmap", szTmp);
			pMediaDescr->AddAttribute(pAttributeRtpmap);

			CAttribute *pAttributeControl = new CAttribute("control", pTrack->mTrackName.c_str());
			pMediaDescr->AddAttribute(pAttributeControl);

			char szSps[128] = {0};
			char szPps[128] = {0};
			if(pTrack->mpAvcCfgRecord->mSpsSize) {
				CAvcCfgRecord::ConvertToBase64((unsigned char *)szSps, pTrack->mpAvcCfgRecord->mSps, pTrack->mpAvcCfgRecord->mSpsSize);
				CAvcCfgRecord::ConvertToBase64((unsigned char *)szPps, pTrack->mpAvcCfgRecord->mPps, pTrack->mpAvcCfgRecord->mPpsSize);
			}
#ifdef WIN32
			itoa (pTrack->CODEC_ID_H264, szTmp, 10);
#else
			sprintf(szTmp, "%d",pTrack->CODEC_ID_H264);
#endif
			std::string strTmp = szTmp;
			strTmp += " packetization-mode=1;profile-level-id=42001e;";
			if(pTrack->mpAvcCfgRecord->mSpsSize) {
				strTmp += "sprop-parameter-sets=";
				strTmp += szSps; strTmp += ",";
				strTmp += szPps; strTmp += ";";
			}
			CAttribute *pAttributeFmtp = new CAttribute("fmtp", strTmp.c_str());
			pMediaDescr->AddAttribute(pAttributeFmtp);
			pSdp->Add(pMediaDescr);
		} else {
			char szTmp[128];
			sprintf(szTmp, "%d %s/90000", pTrack->mCodecId, pTrack->mCodecName.c_str());
			CMediaDescription *pMediaDescr = new CMediaDescription(TRACK_ID_AUDIO,  0/*pTrack->mUdpPortBase*/, pTrack->mCodecId, "Test Session");
		
			CAttribute *pAttributeRtpmap = new CAttribute("rtpmap", szTmp);
			CAttribute *pAttributeControl = new CAttribute("control", pTrack->mTrackName.c_str());
			pMediaDescr->AddAttribute(pAttributeRtpmap);
			pMediaDescr->AddAttribute(pAttributeControl);
			pSdp->Add(pMediaDescr);
		}
	}
Exit:
	if(pszAddr)
		free(pszAddr);
	JDBG_LOG(CJdDbg::LVL_SETUP,("Leave res=%d", res));
	return res;
}
int CJdRtspClntRecSession::SendAnnounce(const char *szMedia)
{
	int bufsize = REQUEST_BUF_SIZE;
	int tempSize;
	char headerBuf[HEADER_BUF_SIZE];
	int ret = -1;
	CSdp *pSdp = &m_sdp;//new CSdp;
	char *pData = headerBuf;

	GenerateSdp(pSdp, mpMediaResMgr, szMedia);
	int lenSdp = pSdp->Encode(pData, HEADER_BUF_SIZE);

RETRY_WITH_AUTH:
	if(m_hSock >= 0){
		int len = CreateAnnounceHeader(m_pszUrl.c_str(), szMedia, lenSdp, pData, HEADER_BUF_SIZE);
		if(mAuth.m_AuthType == CJdAuthRfc2167::AUTH_DIGEST) {
			char *pAuthField = mAuth.MakeDigestAuth(m_Username.c_str(), m_Password.c_str(), m_pszUrl.c_str(),"ANNOUNCE");
			if(pAuthField) {
				pData += strlen(pData);
				strncpy(pData, pAuthField, strlen(pAuthField));
				pData += strlen(pAuthField);
				free(pAuthField);
			}
		}
		pData += strlen(pData);
		len = pSdp->Encode(pData, HEADER_BUF_SIZE - strlen(pData));
		send(m_hSock, headerBuf, strlen(headerBuf), 0);

		ret = ReadHeader(m_hSock, headerBuf);	/* errorSource set within */
		if(ret > 0) {
			if(HandleAnswerAnnounce(headerBuf) == 401 && mAuth.m_AuthType == CJdAuthRfc2167::AUTH_NONE){
				goto RETRY_WITH_AUTH;
			}
			return 0;
		}
	}
Exit:
	return -1;
}

int CJdRtspClntRecSession::HandleAnswerAnnounce(char *headerBuf)
{
	int nStatusCode = ParseResponseMessage(headerBuf, strlen(headerBuf));
	if(nStatusCode == 200) {
		// Do nothing
	} else if(nStatusCode == 401 && mAuth.m_AuthType == CJdAuthRfc2167::AUTH_NONE) {
		header_map_t::const_iterator iter = resp_headers.find("WWW-Authenticate");
		if(iter != resp_headers.end()) {
			std::string strVal = iter->second;
			mAuth.ParseDigestAuth(strVal.c_str());
		}
	}
	return nStatusCode;
}

void CJdRtspClntRecSession::Close()
{
	if(mpStreamServer) {
		mpStreamServer->RemoveMediaDeliveries();
	}
	CJdRtspClntSession::Close();
}
