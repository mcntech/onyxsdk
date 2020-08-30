/*
 *  JdRtpSrv.cpp
 *
 *  Copyright 2010 MCN Technologies Inc.. All rights reserved.
 *
 */
#include <sys/types.h>
#include <sys/stat.h>

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
#include "JdDrbRtspSrv.h"
#include "JdRfc2250.h"
#include "JdNetUtil.h"
#include "JdHttpClnt.h"

#define REQUEST_BUF_SIZE 		1024
#define HEADER_BUF_SIZE 		1024
#define DEFAULT_REDIRECTS       3       /* Number of HTTP redirects to follow */
#define MAX_TIME_LEN			128
#define MAX_SERVERS				1
#define MAX_FILE_PATH			1024


extern CJdRtspSrvSessionList gSessionList;	
extern CJdRtspCallbackList JdRtspCallbackList;

#ifdef WIN32
#define OSAL_WAIT(x)  Sleep(x)
#else
#define OSAL_WAIT(x)  usleep(x * 1000)
#endif
int CJdDrbRtspSrvSes::Open(const char *pszUrl)
{
	const char *pszMedia;
	CMediaDescription *pMediaDescript = NULL;
	int nLen = 0;
	int fd = -1;

	char *pszDescript = (char *)malloc(HEADER_BUF_SIZE);
	char *pszHost = NULL;
	char *pszPath = NULL;

	char *szTmpUrl = strdup(pszUrl);
	/* Seek to the file path portion of the szTmpUrl */
	char *charIndex = strstr(szTmpUrl, "://");
	if(charIndex != NULL)	{
		char *szTmp =  (char *)malloc(HEADER_BUF_SIZE);
		charIndex += strlen("://");
		pszHost = charIndex;
		charIndex = strchr(charIndex, '/');
		int nLenHost = charIndex - pszHost;
		if(nLenHost){
			m_pszHost = (char *)malloc(nLenHost+1);
			strncpy(m_pszHost, pszHost, nLenHost);
			m_pszHost[nLenHost] = 0;
		}
		pszPath = charIndex + 1;
		snprintf(szTmp,HEADER_BUF_SIZE,"%s/describe", pszPath);
		m_pszPathDescribe = strdup(szTmp);
		snprintf(szTmp,HEADER_BUF_SIZE,"%s/action", pszPath);
		m_pszPathAction = strdup(szTmp);
		snprintf(szTmp,HEADER_BUF_SIZE,"%s/response", pszPath);
		m_pszPathResponse = strdup(szTmp);
		snprintf(szTmp,HEADER_BUF_SIZE,"%s/announce", pszPath);
		m_pszPathAnnounce = strdup(szTmp);
	} else {
		fprintf(stderr,"Protocol field missing %s",pszUrl);
		goto Exit;
	}
	nLen = PrepareSdp(pszDescript);
	if(nLen > 0){
		Advertise(pszDescript, strlen(pszDescript));
		StartThread();
	}
Exit:
	if(fd != -1)
		close(fd);
	free(szTmpUrl);
	if(pMediaDescript)
		delete pMediaDescript;
	return 0;
}

int CJdDrbRtspSrvSes::PrepareSdp(char *pDescription)
{
	CSdp *pSdp = new CSdp;
	char *pData = pDescription;
	int res;
	char *szLocalAddress = NULL;
	char *pszAddr = inet_ntoa(m_HostNatAddr);//"192.168.1.105"; //TODO: Get from Stun

	pSdp->m_pszSessionName = strdup("TestSession");
	COrigin *pOrigin = new COrigin("-", 123456,1,pszAddr);
	pSdp->m_pOrigin = pOrigin;

	/* Add NAT Address */
	CMediaDescription *pMediaDescr = new CMediaDescription("video", m_usServerNatRtpPort, 33, "Test Session");
	CConnection *pConnection = new CConnection("IN", "IP4", pszAddr);
	pMediaDescr->AddConnection(pConnection);
	
	/* Add LAN Address */
	pszAddr = inet_ntoa(m_HostLanAddr);
	pConnection = new CConnection("IN", "IP4", pszAddr);
	pMediaDescr->AddConnection(pConnection);

	CAttribute *pAttribute = new CAttribute("rtpmap", "33 MP2T/90000");
	pMediaDescr->AddAttribute(pAttribute);

	/* Add lan rtp port */
	{
		char szTmp[16];
		sprintf(szTmp,"%u",m_usServerRtpPort);
		pAttribute = new CAttribute(LAN_RTP_PORT, szTmp);
		pMediaDescr->AddAttribute(pAttribute);
	}
	pSdp->Add(pMediaDescr);

	/* Find length and discard*/
	int len = pSdp->Encode(pData, HEADER_BUF_SIZE);

	len = pSdp->Encode(pData, HEADER_BUF_SIZE - strlen(pData));

	delete pSdp;

	return len;
}

int CJdDrbRtspSrvSes::Advertise(char *pDescript, int nLen)
{
	if(strcmp(m_pszHost,"localhost")==0){
		CreateEmptyFile(m_pszPathAction);
		CreateEmptyFile(m_pszPathResponse);
		CreateEmptyFile(m_pszPathAnnounce);
		int fd = open(m_pszPathDescribe, O_RDWR | O_CREAT | O_TRUNC
#ifdef WIN32
	,_S_IREAD | _S_IWRITE
#else
	 ,S_IRWXU | S_IRWXG | S_IRWXO
#endif
		);
		if(fd) {
			write(fd, pDescript, nLen);
			close(fd);
		}
	} else {
		int res = HttpPutResource(m_pszHost, m_pszPathAction, NULL, 0, 0);
		res = HttpPutResource(m_pszHost, m_pszPathResponse, NULL, 0, 0);
		res = HttpPutResource(m_pszHost, m_pszPathAnnounce, NULL, 0, 0);
		res = HttpPutResource(m_pszHost, m_pszPathDescribe, pDescript, nLen, 0);
	}
	return 0;
}

int CJdDrbRtspSrvSes::FindClient(char *pDescript, int nMaxLen)
{
	int nBytesRead = 0;
	int done = 0;
	int fd = -1;
	unsigned long ulSeq = -1;
	if(strcmp(m_pszHost,"localhost")==0){
		while(!done) {
			nBytesRead = 0;
			if(fd == -1)
				fd = open(m_pszPathAnnounce,O_RDONLY);
			if(fd != -1) {
				lseek(fd,0,SEEK_SET);
				nBytesRead = read(fd,pDescript,nMaxLen);
			}
			if(nBytesRead == 0){
				OSAL_WAIT(1000);
				continue;
			} else {
				done =1;
			}
		}
		if(fd != -1)
			close(fd);
		return nBytesRead;
	} else {
		int nBytesRead = 0;
		while(!done) {
			nBytesRead = 0;
			nBytesRead = HttpGetResource(m_pszHost, m_pszPathAnnounce, pDescript, nMaxLen);

			if(nBytesRead == 0){
				OSAL_WAIT(1000);
				continue;
			} else {
				/* No update i.e. no new request */
				GetCSeq(pDescript, &ulSeq);
				if(m_ulSeq == ulSeq){
					OSAL_WAIT(1000);
					continue;
				} else {
					done = 1;
				}
			}
		}
	}
	return nBytesRead;
}

void CJdDrbRtspSrvSes::CreateEmptyFile(char *szFileName)
{
	if(strcmp(m_pszHost,"localhost")==0){
		int fd = open(szFileName, O_RDWR | O_CREAT | O_TRUNC
#ifdef WIN32
	,_S_IREAD | _S_IWRITE
#else
	 ,S_IRWXU | S_IRWXG | S_IRWXO
#endif
		);
	} else {
		int res = HttpPutResource(m_pszHost, szFileName, NULL, 0, 0);
	}
}

int CJdDrbRtspSrvSes::SendRtspStatus(char *statusBuf, int nLen)
{
	int res = 0;
	if(strcmp(m_pszHost,"localhost")==0){
		int fd = open(m_pszPathResponse, O_RDWR | O_CREAT | O_TRUNC
#ifdef WIN32
	,_S_IREAD | _S_IWRITE
#else
	 ,S_IRWXU | S_IRWXG | S_IRWXO
#endif
		);
		if(fd) {
			write(fd, statusBuf, nLen);
			close(fd);
		}
	} else {
		res = HttpPutResource(m_pszHost, m_pszPathResponse, statusBuf, nLen, 0);
	}
	return res;
}

int CJdDrbRtspSrvSes::GetRtspCommand(char *headerBuf, int nMaxLen)
{
	int done = 0;
	int fd = -1;
	unsigned long ulSeq = -1;
	if(strcmp(m_pszHost,"localhost")==0){
		int nBytesRead = 0;
		while(!done) {
			nBytesRead = 0;
			if(fd == -1)
				fd = open(m_pszPathAction,O_RDONLY);
			if(fd != -1) {
				lseek(fd,0,SEEK_SET);
				nBytesRead = read(fd,headerBuf,nMaxLen);
			}
			if(nBytesRead == 0){
				OSAL_WAIT(1000);
				continue;
			} else {
				/* No update i.e. no new request */
				GetCSeq(headerBuf, &ulSeq);
				if(m_ulSeq == ulSeq){
					OSAL_WAIT(1000);
					continue;
				} else {
					done = 1;
				}
			}
		}
		if(fd != -1)
			close(fd);
		return nBytesRead;
	} else {
		int nBytesRead = 0;
		while(!done) {
			nBytesRead = 0;
			nBytesRead = HttpGetResource(m_pszHost, m_pszPathAction, headerBuf, nMaxLen);

			if(nBytesRead == 0){
				OSAL_WAIT(1000);
				continue;
			} else {
				/* No update i.e. no new request */
				GetCSeq(headerBuf, &ulSeq);
				if(m_ulSeq == ulSeq){
					OSAL_WAIT(1000);
					continue;
				} else {
					done = 1;
				}
			}
		}
		return nBytesRead;
	}
}

void CJdDrbRtspSrvSes::HandlePlay(char *szRqBuff)
{
	int res = -1;
	char headerBuf[HEADER_BUF_SIZE];
	fprintf(stderr,"\nHandling RTSP Method: %s\n",szRqBuff);	
	char *pszUri = GetUri(szRqBuff);
	SetUrl(pszUri);

	GetCSeq(szRqBuff, &m_ulSeq);

	if(CreateHeader(RTSP_METHOD_PLAY, headerBuf, HEADER_BUF_SIZE) > 0)
		if(SendRtspStatus(headerBuf, strlen(headerBuf)) != 0) {
			fprintf(stderr,"%s:Error SendRtspStatus <%s> to client.Ignoring...\n",__FUNCTION__,headerBuf);
		}

	if(m_pRtp){
		m_pRtp->Start();
		res = m_pfnSetStreamState(RTSP_PLAY, this);
	}
Exit:
	if(pszUri)
		free(pszUri);
	return;
}

void CJdDrbRtspSrvSes::HandleTearDown(char *szRqBuff)
{
	char headerBuf[HEADER_BUF_SIZE];
	int ret = -1;

	fprintf(stderr,"\nHandling RTSP Method: %s\n",szRqBuff);

	if(m_pfnSetStreamState) {
		m_pfnSetStreamState(RTSP_TEARDOWN, this);
	}


	GetCSeq(szRqBuff, &m_ulSeq);
	if(CreateHeader(RTSP_METHOD_TEARDOWN, headerBuf, HEADER_BUF_SIZE) > 0)
		if(SendRtspStatus(headerBuf, strlen(headerBuf)) != 0){
			fprintf(stderr,"%s:Error SendRtspStatus <%s>to client.Ignoring...\n",__FUNCTION__,headerBuf);
		}

	if(m_pRtp){
		m_pRtp->Stop();
		m_pRtp->CloseSession();
	}

Exit:
	return;
}
/**
 * Obtains parent folder for RTSP resource
 */
char *GetParentFolderForDrbResource(char *szResource)
{
	const char *pszDirStart = szResource;
	const char *pszDirEnd = NULL;
	char *pszDir = NULL;
	if(strncmp(pszDirStart,"drb://", strlen("drb://")) == 0){
		pszDirStart += strlen("rtsp://");
		pszDirStart = strstr(pszDirStart,"/");
		if(pszDirStart)
			pszDirStart += strlen("/");
	}
	if(pszDirStart){
		pszDirEnd = strstr(pszDirStart,"/");
	}
	int len = 0;
	if(pszDirEnd != NULL)
		len = pszDirEnd - pszDirStart;
	else
		len = strlen(pszDirStart);

	if(len > 0 && len < 1024){
		pszDir = (char *)malloc (len + 1);
		strncpy(pszDir,pszDirStart,len);
		while(len > 1 && pszDir[len - 1] == ' ')
				len--;
		pszDir[len] = 0x00;
	}
	return pszDir;
}

int CJdDrbRtspSrvSes::HandleSetup(const char *szRqBuff)
{
	int res = 0;
	char headerBuf[HEADER_BUF_SIZE];
	char statusBuf[HEADER_BUF_SIZE];
	char descrptBuf[HEADER_BUF_SIZE];
	srand(time(NULL));
	unsigned long ulSessionId = rand();
	char *pszUri = NULL;

	fprintf(stderr,"\nHandling RTSP Method: %s\n",szRqBuff);
	// TODO: Add to global session list
	CJdDrbRtspSrvSes *pSession = this;

	if(pSession){
		char *pszParentFolder = NULL;
		CMediaDescription *pMediaDescript = NULL;
		int nLen = FindClient(descrptBuf, HEADER_BUF_SIZE);
		/* Locate media description record */
		in_addr srv_addr;
		CSdp Sdp;
		Sdp.Parse(descrptBuf);
		if(Sdp.m_NumeMediaDescriptions) {
			CMediaDescription *pMediaDescript = Sdp.m_listMediaDescription[0];
			if(pMediaDescript->m_NumConnections &&  pMediaDescript->m_listConnections[0]->m_pszAddress){
				srv_addr.s_addr = inet_addr(pMediaDescript->m_listConnections[0]->m_pszAddress);
				/* Check if server i in LAN and PICK LAN Address */
				if(srv_addr.s_addr == m_HostNatAddr.s_addr) {

					if(pMediaDescript->m_NumConnections > 1 &&  pMediaDescript->m_listConnections[1]->m_pszAddress){
						srv_addr.s_addr = inet_addr(pMediaDescript->m_listConnections[1]->m_pszAddress);
					}
				}
			}
			if(pMediaDescript->m_listBandwidths){
				long lCtValue = pMediaDescript->GetBandwidthValue("CT");
				if(lCtValue)
					pSession->m_lCtBitrate = lCtValue;
			}
			if(pMediaDescript->m_listAttributes){
				const char *pszFrameRate = pMediaDescript->GetAttributeValue(ATRRIB_FRAME_RATE);
				if(pszFrameRate)	
					pSession->m_lFrameRate = atoi(pszFrameRate);
			}
			m_PeerAddr.s_addr = srv_addr.s_addr;

		}
		pSession->ParseTransport(szRqBuff);
		GetCSeq(szRqBuff, &pSession->m_ulSeq);
		CRtspParseUtil::GetUri(szRqBuff, mUrl);

		pszParentFolder = GetParentFolderForDrbResource(pszUri);
		if(pszParentFolder){
			pSession->m_pfnSetStreamState = JdRtspCallbackList.GetRtspCallback(pszParentFolder);
			free(pszParentFolder);
		}
		if(pSession->m_pfnSetStreamState) {

			pSession->m_usServerRtcpPort = pSession->m_usServerRtpPort + 1;
			if(pSession->CreateHeader(RTSP_METHOD_SETUP, headerBuf, HEADER_BUF_SIZE) > 0){
				if(SendRtspStatus(headerBuf, strlen(headerBuf)) != 0){
					fprintf(stderr,"%s:Error SendRtspStatus <%s>to client.Ignoring...\n",__FUNCTION__,headerBuf);
					goto Exit;
				}
			}

			pSession->m_pRtp = new CRtp(CRtp::MODE_SERVER);
			if(pSession->m_pRtp->CreateSession(&pSession->m_PeerAddr,	
				pSession->m_usServerRtpPort, pSession->m_usServerRtcpPort,
				pSession->mClientRtpPort, pSession->m_usClientRtcpPort) != 0){
					fprintf(stderr,"Failed to create RTP Session. Exiting...\n");
					res = -1;
					goto Exit;
			}
		} else {
			PrepreRtspStatus(statusBuf, 404, szRqBuff);
			SendRtspStatus(statusBuf, strlen(statusBuf));
		}
	} else {
		PrepreRtspStatus(statusBuf, 453, szRqBuff);
		SendRtspStatus(statusBuf, strlen(statusBuf));
		if(pSession)
			delete pSession;
	}

Exit:
	if(pszUri)
		free(pszUri);
	return res;
}

void *CJdDrbRtspSrvSes::ClientThread( void *arg ) 
{
	char headerBuf[HEADER_BUF_SIZE] = {0};
	char statusBuf[HEADER_BUF_SIZE];
	int cnt;
	int nRtspMethodId = 0;
	CJdDrbRtspSrvSes *pRtspSession = (CJdDrbRtspSrvSes *)arg;
	SESSION_CONTEXT_T *pSessionCtx = (SESSION_CONTEXT_T *)arg;
	int sock = pSessionCtx->sockClnt;
	fprintf(stderr, "%s : Enter\n",__FUNCTION__);
#ifdef SO_NOSIGPIPE
        int set = 1;
        int ret = setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
        printf("%s\n", strerror(errno));
        assert(ret == 0);
#endif
	/* What does the client want to receive? Read the request. */
	memset(headerBuf, 0, HEADER_BUF_SIZE);
	while(!pRtspSession->fStop) {
		unsigned long ulSessionId = 0;
		int nBytes = pRtspSession->GetRtspCommand(headerBuf, HEADER_BUF_SIZE);
		if(nBytes <= 0){
			fprintf(stderr,"Closing session!!!\n");
			goto Exit;
		}

		nRtspMethodId =	pRtspSession->GetRtspMethodId(headerBuf, &ulSessionId);

		if(nRtspMethodId == RTSP_METHOD_SETUP && ulSessionId == 0){
			// Create new Session
			if(pRtspSession->HandleSetup(headerBuf) == 0){
				continue;
			} else {
				fprintf(stderr,"%s:%d:Closing session!!!\n",__FUNCTION__, __LINE__);
				goto Exit;
			}
		} else {

			switch(nRtspMethodId)
			{
				case RTSP_METHOD_PLAY:
					pRtspSession->HandlePlay(headerBuf);
					break;
				case RTSP_METHOD_TEARDOWN:
					pRtspSession->HandleTearDown(headerBuf);
					/* Proceed and wait for next session */
					//goto Exit;
					continue;
					break;
				case RTSP_METHOD_ANNOUNCE:
				case RTSP_METHOD_GET_PARAMETER:
				case RTSP_METHOD_OPTIONS:
				case RTSP_METHOD_PAUSE:
				case RTSP_METHOD_RECORD:
				case RTSP_METHOD_SET_PARAMETER:
				default:
					{
						PrepreRtspStatus(statusBuf, 405, headerBuf);
						pRtspSession->SendRtspStatus(statusBuf, strlen(statusBuf));
						fprintf(stderr,"Unsupported RTSP Method: %s\n",headerBuf);
						break;
					}

			}
		}
	}
Exit:
	fprintf(stderr, "%s : Leave\n",__FUNCTION__);
	return NULL;
}
int CJdDrbRtspSrvSes::Close()
{
	fStop = 1;
	return 0;
}

int CJdDrbRtspSrvSes::StartThread() 
{
	pthread_t client;
#ifdef WIN32
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
	if( pthread_create(&client, NULL, &CJdDrbRtspSrvSes::ClientThread, this) != 0 ) {
		// ERROR
	}

	pthread_detach(client);

	return 0;
}

