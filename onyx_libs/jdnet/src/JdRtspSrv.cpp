/*
 *  JdRtpSrv.cpp
 *
 *  Copyright 2010 MCN Technologies Inc.. All rights reserved.
 *
 */

#ifdef WIN32
#include <winsock2.h>
#include <io.h>
#include <ws2tcpip.h>
#define close	closesocket
#define snprintf	_snprintf
#define write	_write
#define socklen_t	int
#else
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <strings.h>
#include <netdb.h>
#include <string.h>
#define O_BINARY	0
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include "JdSdp.h"
#include "JdRtpSnd.h"
#include "JdRtspSrv.h"
#include "JdRfc2250.h"
#include "JdNetUtil.h"
#include "JdRfc3984.h"
#include "JdRfc3640.h"

#define REQUEST_BUF_SIZE 		1024
#define HEADER_BUF_SIZE 		1024
#define DEFAULT_REDIRECTS       3       /* Number of HTTP redirects to follow */
#define MAX_TIME_LEN			128
#define MAX_SERVERS				1
#define MAX_FILE_PATH			1024

static int modDbgLevel = CJdDbg::LVL_TRACE;
int CJdRtspSrvSession::CreateSetupRepy(char *pBuff, int nMaxLen, CRtspRequest *clntReq, CRtspRequestTransport *pTrans)
{
	int tempSize = 0;
	char szTime[MAX_TIME_LEN];
	GetTime(szTime, MAX_TIME_LEN);
	// TODO: use std::string separate common code
	if(pTrans->mMulticast) {
		snprintf(pBuff, nMaxLen, 
				"RTSP/%s 200 OK\r\n" \
				"CSeq: %d\r\n" \
				"%s\r\n"
				"Session: %u\r\n" \
				"Transport: RTP/AVP;multicast;destination=%s;port=%u-%u;ttl=16\r\n" \
				"\r\n",
				RTSP_VERSION, 
				clntReq->mCSeq,
				szTime,
				m_ulSessionId,
				pTrans->mDestination,
				pTrans->mServerRtpPort, pTrans->mServerRtcpPort);
	} else {
		snprintf(pBuff, nMaxLen, 
				"RTSP/%s 200 OK\r\n" \
				"CSeq: %d\r\n" \
				"%s\r\n"
				"Session: %u\r\n" \
				"Transport: RTP/AVP;unicast;client_port=%u-%u;server_port=%u-%u\r\n" \
				"\r\n",
				RTSP_VERSION, 
				clntReq->mCSeq,
				szTime,
				m_ulSessionId,
				pTrans->mClientRtpPort, pTrans->mClientRtcpPort,
				pTrans->mServerRtpPort, pTrans->mServerRtcpPort);
	}
	return strlen(pBuff);
}

int CJdRtspSrvSession::CreatePlayReply(char *pBuff, int nMaxLen, CRtspRequest *clntReq)
{
	snprintf(pBuff, nMaxLen, 
			"RTSP/%s 200 OK\r\n" \
			"CSeq: %d\r\n" \
			"Session: %u\r\n" \
			"\r\n",
			RTSP_VERSION, 
			clntReq->mCSeq,
			m_ulSessionId);
	return strlen(pBuff);

}

int CJdRtspSrvSession::CreateTearDownreply(char *pBuff, int nMaxLen, CRtspRequest *clntReq)
{
	snprintf(pBuff, nMaxLen, 
			"RTSP/%s 200 OK\r\n" \
			"CSeq: %d\r\n" \
			"\r\n",
			RTSP_VERSION, 
			clntReq->mCSeq);
	return strlen(pBuff);

}
struct RTSP_STATUS_T {
	int		nStstusId;
	char	*pszStatus;
} RTSP_STATUS_LIST[] =
{
	{100, "Continue"},
	{200, "OK"},
    {201, "Created"},
    {250, "Low on Storage Space"},
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Moved Temporarily"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Time-out"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Request Entity Too Large"},
    {414, "Request-URI Too Large"},
    {415, "Unsupported Media Type"},
    {451, "Parameter Not Understood"},
    {452, "Conference Not Found"},
    {453, "Not Enough Bandwidth"},
    {454, "Session Not Found"},
    {455, "Method Not Valid in This State"},
    {456, "Header Field Not Valid for Resource"},
    {457, "Invalid Range"},
    {458, "Parameter Is Read-Only"},
    {459, "Aggregate operation not allowed"},
    {460, "Only aggregate operation allowed"},
    {461, "Unsupported transport"},
    {462, "Destination unreachable"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Time-out"},
    {505, "RTSP Version not supported"},
    {551, "Option not supported"}
};
const char *GetStatusMsg(int nStatusId)
{
	static const char *pszUnknown ="Unknown Error";
	int nCount = sizeof(RTSP_STATUS_LIST) / sizeof(struct RTSP_STATUS_T);
	for (int i=0; i < nCount; i++){
		if(RTSP_STATUS_LIST[i].nStstusId == nStatusId)
			return RTSP_STATUS_LIST[i].pszStatus;
	}
	return pszUnknown;
}

void CJdRtspSrvSession::PrepreRtspStatus(char *statusBuf, int nStatusId)
{
	const char *pszMsg = GetStatusMsg(nStatusId);
	snprintf(statusBuf, HEADER_BUF_SIZE, 
			"RTSP/%s %d %s\r\n" \
			"\r\n",
			RTSP_VERSION, nStatusId, pszMsg);
}
void CJdRtspSrvSession::SendRtspStatus(int hSock, char *statusBuf, int nLen)
{
	send(hSock, statusBuf, strlen(statusBuf), 0);
}


bool CJdRtspSrvSession::PlayTrack(CMediaTrack *pTrack)
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

void CJdRtspSrvSession::HandlePlay(CRtspRequest &clntReq)
{
	bool res = false;
	char headerBuf[HEADER_BUF_SIZE];
	std::string nameAggregate;
	std::string nameTrack;

	JDBG_LOG(CJdDbg::LVL_SETUP,("Enter"));
	CMediaUtil::ParseResourceName(clntReq.mUrlMedia, nameAggregate, nameTrack);
	CMediaAggregate *pMediaAggregate = mMediaResMgr->GetMediaAggregate(nameAggregate.c_str());
	
	mAggregateStream = nameAggregate;

	if(!nameTrack.empty()) {
		CMediaTrack *pTrack = pMediaAggregate->GetTrack(nameTrack);
		res = PlayTrack(pTrack);
	} else {
		int nTracks = pMediaAggregate->GetTrackCount();
		for (int i=0; i < nTracks; i++) {
			CMediaTrack *pTrack = pMediaAggregate->GetTrack(i);
			res = PlayTrack(pTrack);
		}
	}
	
	if(res == true) {
		if(CreatePlayReply(headerBuf, HEADER_BUF_SIZE, &clntReq) > 0)
			send(m_hSock, headerBuf, strlen(headerBuf), 0);
	} else {
		PrepreRtspStatus(headerBuf, 404);
		SendRtspStatus(m_hSock, headerBuf, strlen(headerBuf));
	}
	JDBG_LOG(CJdDbg::LVL_SETUP,("Leave"));
}

void CJdRtspSrvSession::HandleTearDown(CRtspRequest &clntReq)
{
	char headerBuf[HEADER_BUF_SIZE];
	bool res;

	RemoveMediaDeliveries();

	if(m_hSock >= 0){
		//if(res) 
		if(1) {
			if(CreateTearDownreply(headerBuf, HEADER_BUF_SIZE, &clntReq) > 0)
				send(m_hSock, headerBuf, strlen(headerBuf), 0);
		} else {
			PrepreRtspStatus(headerBuf, 404);
			SendRtspStatus(m_hSock, headerBuf, strlen(headerBuf));
		}
	}

	return;
}

void CJdRtspSrvSession::HandleSetup(CRtspRequest &clntReq, CRtspRequestTransport &Transport)
{
	int res = -1;
	char respBuf[HEADER_BUF_SIZE];

	std::string nameAggregate;
	std::string nameTrack;
	CRtpSnd *pRtp = NULL;
	int addrlen = sizeof(struct sockaddr);
	struct sockaddr_in sa;
	CMediaTrack *pTrack = NULL;
	if(m_ulSessionId == 0){
		srand(time(NULL));
		m_ulSessionId = rand();
	}
	CMediaUtil::ParseResourceName(clntReq.mUrlMedia, nameAggregate, nameTrack);
	CMediaAggregate *pMediaAggregate = mMediaResMgr->GetMediaAggregate(nameAggregate.c_str());
	if(pMediaAggregate == NULL){
		JdDbg(CJdDbg::LVL_ERR,("%s : Can not find MediaAggregate for %s\n",__FUNCTION__,nameAggregate.c_str()));
		goto Exit;
	}
	pTrack = pMediaAggregate->GetTrack(nameTrack);
	if(pTrack == NULL) {
		JdDbg(CJdDbg::LVL_ERR,("%s : Can not find track for %s\n",__FUNCTION__,nameTrack.c_str()));
		goto Exit;
	}
	getpeername(m_hSock, (struct sockaddr *)&sa, (socklen_t *)&addrlen );
	m_PeerAddr = sa.sin_addr;

	pRtp = new CRtpSnd();
	pRtp->m_SockAddr = m_PeerAddr;

	/* Specify Server ports */
	Transport.mServerRtpPort = pTrack->mUdpPortBase;
	Transport.mServerRtcpPort = pTrack->mUdpPortBase + 1;
	if(pTrack->mfMacast) {
		Transport.mDestination = pTrack->mMacastAddr;
		Transport.mMulticast = pTrack->mfMacast;
	}
	Transport.GetRtpPorts(pRtp , CRtp::MODE_SERVER);

	if(CreateSetupRepy(respBuf, HEADER_BUF_SIZE, &clntReq, &Transport) > 0){
		SendRtspStatus(m_hSock, respBuf, strlen(respBuf));
	}

	if(pTrack->mCodecType == CMediaTrack::CODEC_TYPE_VIDEO) {
		m_pVRtp = pRtp;
	} else {
		m_pARtp = pRtp;
	}
	res = 0;
Exit:
	if(res == -1) {
		PrepreRtspStatus(respBuf, 453);
		SendRtspStatus(m_hSock, respBuf, strlen(respBuf));
	}
	return;
}

int CJdRtspSrvSession::GenerateSdp(CSdp *pSdp, const char *pszMedia)
{
	char *pszAddr = NULL;;
	struct sockaddr_in	addr;
	int res = -1;
	COrigin *pOrigin = NULL;
	int nTracks = 0;
	CMediaAggregate *pMediaAggregate = NULL;

	JDBG_LOG(CJdDbg::LVL_SETUP,("Enter"));
	pMediaAggregate = mMediaResMgr->GetMediaAggregate(pszMedia);
	if(pMediaAggregate == NULL){
		JdDbg(CJdDbg::LVL_ERR,("%s : No MediaAggregate\n",__FUNCTION__));
		goto Exit;
	}
	nTracks = pMediaAggregate->GetTrackCount();
	addrinfo* results;
	addrinfo hint;
	memset(&hint, 0, sizeof(hint));
	hint.ai_family    = PF_INET; //is_v4 ? PF_INET : PF_INET6;
	//hint.ai_flags     = AI_PASSIVE;
	hint.ai_socktype  = SOCK_STREAM; //is_dgram ? SOCK_DGRAM : SOCK_STREAM;

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

int CJdRtspSrvSession::HandleDescribe(CRtspRequest &clntReq)
{
	CSdp *pSdp = new CSdp;
	char respBuf[HEADER_BUF_SIZE];
	char *pData = respBuf;
	int res = -1;
	int len;

	JDBG_LOG(CJdDbg::LVL_SETUP,("Enter"));
	res = GenerateSdp(pSdp, clntReq.mUrlMedia.c_str());
	if(res != 0)
		goto Exit;

	/* Find length and discard*/
	len = pSdp->Encode(pData, HEADER_BUF_SIZE);

	snprintf(pData, HEADER_BUF_SIZE, 
			"RTSP/%s 200 OK\r\n" \
			"CSeq: %d\r\n" \
			"Content-Length: %d\r\n" \
			"\r\n",
			RTSP_VERSION, 
			clntReq.mCSeq,
			len);

	pData += strlen(pData);
	len = pSdp->Encode(pData, HEADER_BUF_SIZE - strlen(pData));
	delete pSdp;

	send(m_hSock, respBuf, strlen(respBuf), 0);

Exit:
	if(res == -1){
		PrepreRtspStatus(respBuf, 404);
		SendRtspStatus(m_hSock, respBuf, strlen(respBuf));
	}
	JDBG_LOG(CJdDbg::LVL_SETUP,("Leave"));
	return 0;
}

void CJdRtspSrvSession::HandleOptions(CRtspRequest &clntReq)
{
	char headerBuf[HEADER_BUF_SIZE];
	CSdp *pSdp = new CSdp;
	char *pData = headerBuf;
	int res;
	unsigned long ulSeq;

	snprintf(pData, HEADER_BUF_SIZE, 
			"RTSP/%s 200 OK\r\n" \
			"CSeq: %d\r\n" \
			"Public: DESCRIBE,SETUP,TEARDOWN,PLAY\r\n" \
			"\r\n",
			RTSP_VERSION, 
			clntReq.mCSeq);

	send(m_hSock, pData, strlen(pData), 0);
}

void *CJdRtspSrvSession::sessionThread( void *arg )
{
	CJdRtspSrvSession *pRtpSession = (CJdRtspSrvSession *)arg;
	pRtpSession->threadProcessRequests();
	return NULL;
}

void CJdRtspSrvSession::RemoveMediaDeliveries()
{
	CMediaAggregate *pMediaAggregate = mMediaResMgr->GetMediaAggregate(mAggregateStream.c_str());

	foreach_in_map(IMediaDelivery *, mTrackDeliveries){
		CMediaTrack *pTrack = pMediaAggregate->GetTrack((*it).first);
		if(pTrack) {
			pTrack->DeleteDelivery((*it).second);
			delete (*it).second;
		}
	}
	mTrackDeliveries.clear();
}

void *CJdRtspSrvSession::threadProcessRequests() 
{
	char headerBuf[HEADER_BUF_SIZE];
	char statusBuf[HEADER_BUF_SIZE];
	int cnt;
	int nRtspMethodId = 0;
	CRtspRequest clntReq;
#ifdef SO_NOSIGPIPE
        int set = 1;
        int ret = setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
        printf("%s\n", strerror(errno));
        assert(ret == 0);
#endif
	memset(headerBuf, 0, HEADER_BUF_SIZE);
	while(true) {
		unsigned long ulSessionId = 0;
		int nBytes = ReadHeader(m_hSock, headerBuf, 0/*Infinite Wait*/);
		if(nBytes <= 0){
			goto Exit;
		}

		CRtspUtil::ParseClientRequest(headerBuf,clntReq);

		switch(clntReq.mMethod)
		{
			case RTSP_METHOD_SETUP:
				{
					CRtspRequestTransport Transport;
					Transport.ParseTransport(headerBuf);
					HandleSetup(clntReq, Transport);
				}
				break;
			case RTSP_METHOD_PLAY:
				HandlePlay(clntReq);
				break;
			case RTSP_METHOD_TEARDOWN:
				HandleTearDown(clntReq);
				goto Exit;
				break;

			case RTSP_METHOD_DESCRIBE:
				HandleDescribe(clntReq);
				break;
			case RTSP_METHOD_OPTIONS:
				HandleOptions(clntReq);
				break;

			case RTSP_METHOD_ANNOUNCE:
			case RTSP_METHOD_GET_PARAMETER:
			case RTSP_METHOD_PAUSE:
			case RTSP_METHOD_RECORD:
			case RTSP_METHOD_SET_PARAMETER:
			default:
				{
					PrepreRtspStatus(statusBuf, 405);
					SendRtspStatus(m_hSock, statusBuf, strlen(statusBuf));
					fprintf(stderr,"Unsupported RTSP Method: %s\n",headerBuf);
					break;
				}

		}
	}
Exit:
	RemoveMediaDeliveries();
	if(m_hSock != -1)
		close(m_hSock);
	JdDbg(CJdDbg::LVL_ERR,("%s : Leave\n",__FUNCTION__));
	return NULL;
}

void CJdRtspSrv::ServerCleanup(void *arg) 
{
	CJdRtspSrv *pServ = (CJdRtspSrv *)arg;
	close(pServ->m_hSock);
}

void *CJdRtspSrv::ServerThread( void *arg ) 
{
	SESSION_CONTEXT_T *pSessionCtx;
	struct sockaddr_in addr, client_addr;
	int addr_len = sizeof(struct sockaddr_in);

	CJdRtspSrv *pServ = (CJdRtspSrv *)arg;

	/* set cleanup handler to cleanup ressources */
#ifdef WIN32
#else
	pthread_cleanup_push(CJdRtspSrv::ServerCleanup, pServ);
#endif
	/* open socket for server */
	pServ->m_hSock = socket(PF_INET, SOCK_STREAM, 0);
	if ( pServ->m_hSock < 0 ) {
		fprintf(stderr, "socket failed\n");
		goto Exit;
	}
	printf("%s:%s:%d\n",__FILE__, __FUNCTION__, __LINE__);
	/* ignore "socket already in use" errors */
	{
		int on = 1;
		if (setsockopt(pServ->m_hSock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on)) < 0) {
			perror("setsockopt(SO_REUSEADDR) failed");
			goto Exit;
		}
	}
	/* configure server address to listen to all local IPs */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(pServ->mRtspPort); /* is already in right byteorder */
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if ( bind(pServ->m_hSock, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) {
		perror("bind");
		goto Exit;
	}

	/* start listening on socket */
	if ( listen(pServ->m_hSock, 10) != 0 ) {
		fprintf(stderr, "listen failed\n");
		goto Exit;
	}

	/* create a child for every client that connects */
	while ( !pServ->m_Stop ) {

		int hSock = accept(pServ->m_hSock, (struct sockaddr *)&client_addr, 
				(socklen_t *)&addr_len);
		if(hSock != -1){
			CJdRtspSrvSession *pSession = new CJdRtspSrvSession(pServ->mMediaResMgr);

			pSession->m_hSock = hSock;
#ifdef WIN32
			DWORD dwThreadId;
			pSession->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CJdRtspSrvSession::sessionThread, pSession, 0, &dwThreadId);
#else
			pthread_create(&pSession->hThread, NULL, CJdRtspSrvSession::sessionThread, pSession);
			pthread_detach(pSession->hThread);
#endif
			
		} else {
			fprintf(stderr,"\n!!!Exiting due to socket error!!!\n");
		}
	}
#ifdef WIN32
#else
	pthread_cleanup_pop(1);
#endif
Exit:
	return NULL;
}

int CJdRtspSrv::Stop(int id) 
{
	m_Stop	 = 1;
	if(m_hSock != -1){
		close(m_hSock);
	}
#ifdef WIN32
	WSACleanup();
#endif


#ifdef WIN32
#else
	pthread_cancel(m_thrdHandle);
#endif
	return 0;
}

int CJdRtspSrv::Run(
		int						id,			// Int server ID
		unsigned short			usPort		// RTSP Port for this instance
		) 
{
#ifdef WIN32
	DWORD dwThreadId;
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
    wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        printf("WSAStartup failed with error: %d\n", err);
        return 1;
    }
#endif
	m_Stop = 0;
	mRtspPort = usPort;
#ifdef WIN32
	m_thrdHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ServerThread, this, 0, &dwThreadId);
#else
	pthread_create(&m_thrdHandle, NULL, ServerThread,this);
	pthread_detach(m_thrdHandle);
#endif
	return 0;
}

