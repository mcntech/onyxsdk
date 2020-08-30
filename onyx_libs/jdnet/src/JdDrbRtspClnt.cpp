#ifdef WIN32

#include <sys/types.h>
#include <sys/stat.h>
#include <winsock2.h>
#include <io.h>
#define close	closesocket
#define snprintf	_snprintf
#define write	_write
#else
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <strings.h>
#include <netdb.h>
#include <string.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include "JdSdp.h"
#include "JdDrbRtspClnt.h"
#include "JdHttpClnt.h"
#include "JdNetUtil.h"

#ifdef WIN32
#define close		closesocket
#define open		_open
#define O_RDONLY	_O_RDONLY
#define O_BINARY	_O_BINARY
#else // Linux
#define O_BINARY	0
#endif

#define DEF_RTSP_PORT 			554
#define DEF_RTP_PORT 			59427

#define SDP_BUF_SIZE			2048
#define REQUEST_BUF_SIZE 		1024
#define HEADER_BUF_SIZE 		1024

#define DRB_STATUS_TIMEOUT		30000

#ifdef WIN32
#define OSAL_WAIT(x)  Sleep(x)
#else
#define OSAL_WAIT(x)  usleep(x * 1000)
#endif


int CJdDrbRtspClnt::PrepareSdp(char *pDescription)
{
	CSdp *pSdp = new CSdp;
	char *pData = pDescription;
	int res;
	char *szLocalAddress = NULL;
	char *pszAddr = inet_ntoa(m_HostNatAddr);//"192.168.1.105"; //TODO: Get from Stun

	pSdp->m_pszSessionName = strdup("TestSession");
	COrigin *pOrigin = new COrigin("-", 123456,1,pszAddr);
	pSdp->m_pOrigin = pOrigin;


	CMediaDescription *pMediaDescr = new CMediaDescription("video", m_usClientNatRtpPort, 33, "Test Session");
	/* Add NAT Address */
	CConnection *pConnection = new CConnection("IN", "IP4", pszAddr);
	pMediaDescr->AddConnection(pConnection);

	/* Add LAN Address */
	pszAddr = inet_ntoa(m_HostLanAddr);
	pConnection = new CConnection("IN", "IP4", pszAddr);
	pMediaDescr->AddConnection(pConnection);


	CAttribute *pAttribute = new CAttribute(ATRRIB_RTPMAP,"33 MP2T/90000");
	pMediaDescr->AddAttribute(pAttribute);

	pAttribute = new CAttribute(ATRRIB_FRAME_RATE,"30");
	pMediaDescr->AddAttribute(pAttribute);

	/* Add lan rtp port */
	{
		char szTmp[16];
		sprintf(szTmp,"%u",mLocalRtpPort);
		pAttribute = new CAttribute(LAN_RTP_PORT, szTmp);
		pMediaDescr->AddAttribute(pAttribute);
	}
	
	CBandWitdh *pBandwidth = new CBandWitdh("CT",256);
	pMediaDescr->AddBandwidth(pBandwidth);


	pSdp->Add(pMediaDescr);

	/* Find length and discard*/
	int len = pSdp->Encode(pData, HEADER_BUF_SIZE);

	len = pSdp->Encode(pData, HEADER_BUF_SIZE - strlen(pData));

	delete pSdp;

	return len;
}

int CJdDrbRtspClnt::FindServer(char *pDescript, int nMaxLen)
{
	int nLen = 0;
	if(strcmp(m_pszHost,"localhost")==0){
		int fd = open(m_pszPathDescribe,O_RDONLY);
		if(fd) {
			nLen = read(fd, pDescript, nMaxLen);
			close(fd);
		}
	} else {
		nLen = HttpGetResource(m_pszHost, m_pszPathDescribe, pDescript, nMaxLen);
	}
	return nLen;
}

int CJdDrbRtspClnt::Announce()
{
	int res = 0;
	char *pszDescript = (char *)malloc(SDP_BUF_SIZE);
	int nLen = PrepareSdp(pszDescript);
	if(strcmp(m_pszHost,"localhost")==0){
		int fd = open(m_pszPathAnnounce, O_RDWR | O_CREAT | O_TRUNC
#ifdef WIN32
	,_S_IREAD | _S_IWRITE
#else
	 ,S_IRWXU | S_IRWXG | S_IRWXO
#endif
		);
		if(fd) {
			write(fd, pszDescript, nLen);
			close(fd);
		}
	} else {
		res = HttpPutResource(m_pszHost, m_pszPathAnnounce, pszDescript, nLen, 0);
	}
	if(pszDescript)
		free(pszDescript);
	return res;
}

void CJdDrbRtspClnt::SendRtspCommand(char *pszCmdBuf, int nLen)
{
	int res = 0;
	if(strcmp(m_pszHost,"localhost")==0){
		int fd = open(m_pszPathAction, O_RDWR | O_CREAT | O_TRUNC
#ifdef WIN32
	,_S_IREAD | _S_IWRITE
#else
	 ,S_IRWXU | S_IRWXG | S_IRWXO
#endif
		);
		if(fd) {
			write(fd, pszCmdBuf, nLen);
			close(fd);
		}
	} else {
		res = HttpPutResource(m_pszHost, m_pszPathAction, pszCmdBuf, nLen, 0);
	}
}

int CJdDrbRtspClnt::GetRtspStatus(char *headerBuf, int nMaxLen, long lTimeOutMs)
{
	int done = 0;
	int fd = -1;
	unsigned long ulSeq = -1;
	if(strcmp(m_pszHost,"localhost")==0){
		int nBytesRead = 0;
		while(!done) {
			nBytesRead = 0;
			if(fd == -1)
				fd = open(m_pszPathResponse,O_RDONLY);
			if(fd != -1) {
				nBytesRead = read(fd,headerBuf,nMaxLen);
			}
			if(nBytesRead == 0){
				OSAL_WAIT(1000);
				lTimeOutMs -= 1000;
				continue;
			} else {
				/* No update i.e. no new request */
				GetCSeq(headerBuf, &ulSeq);
				if(m_ulSeq == ulSeq){
					OSAL_WAIT(1000);
					lTimeOutMs -= 1000;
					continue;
				} else {
					done = 1;
				}
			}
		}
		if(fd == -1)
			close(fd);
		return nBytesRead;
	} else {
		int nBytesRead = 0;
		while(!done && lTimeOutMs > 0) {
			nBytesRead = 0;
			nBytesRead = HttpGetResource(m_pszHost, m_pszPathResponse, headerBuf, nMaxLen);

			if(nBytesRead == 0){
				OSAL_WAIT(1000);
				lTimeOutMs -= 1000;
				continue;
			} else {
				/* No update i.e. no new request */
				GetCSeq(headerBuf, &ulSeq);
				if(m_ulSeq != ulSeq){
					OSAL_WAIT(1000);
					lTimeOutMs -= 1000;
					continue;
				} else {
					done = 1;
				}
			}
		}
	}
}

int CJdDrbRtspClnt::Open(const char *pszUrl)
{
	int nLen = 0;
	int fd = -1;
	int res = -1;
	char *pszHost = NULL;
	char *pszPath = NULL;
	char *pszSdpData = (char *)malloc(SDP_BUF_SIZE);

	char *szTmpUrl = strdup(pszUrl);
	m_pszUrl = strdup(pszUrl);
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

	nLen = FindServer(pszSdpData,SDP_BUF_SIZE);
	if(nLen > 0) {
		res = Announce();
		//if(res==0)
			//res = SendSetup(pszSdpData);
	} else {
		fprintf(stderr,"Failed to open device at %s\n",pszUrl);
	}
Exit:
	if(fd != -1)
		close(fd);
	free(szTmpUrl);
	free(pszSdpData);

	return res;
}

int CJdDrbRtspClnt::SendSetup(char *pszSdpData)
{
	int res = -1;
	const char *pszMedia;
	char cmdBuf[HEADER_BUF_SIZE];
	char statusBuf[HEADER_BUF_SIZE];
	in_addr			srv_addr;
	unsigned short	usServerRtpPort;
	unsigned short	usServerRtcpPort = 0;
	CSdp Sdp;
	const char *pszTmp = NULL;

	Sdp.Parse(pszSdpData);
	if(Sdp.m_NumeMediaDescriptions) {
		CMediaDescription *pMediaDescript = Sdp.m_listMediaDescription[0];
		if(pMediaDescript->m_NumConnections &&  pMediaDescript->m_listConnections[0]->m_pszAddress){
			srv_addr.s_addr = inet_addr(pMediaDescript->m_listConnections[0]->m_pszAddress);
			usServerRtpPort = pMediaDescript->m_usPort;
			pszTmp = pMediaDescript->GetAttributeValue(RTCP_PORT, 0);
			if(pszTmp)
				usServerRtpPort = atoi(pszTmp);
			/* Check if server is in LAN and PICK LAN Address */
			if(srv_addr.s_addr == m_HostNatAddr.s_addr) {
				// TODO: Obtining local port
				if(pMediaDescript->m_NumConnections > 1 &&  pMediaDescript->m_listConnections[1]->m_pszAddress){
					pszTmp = NULL;
					srv_addr.s_addr = inet_addr(pMediaDescript->m_listConnections[1]->m_pszAddress);
					pszTmp = pMediaDescript->GetAttributeValue(LAN_RTP_PORT, 0);
					if(pszTmp)
						usServerRtpPort = atoi(pszTmp);
				}
			}
		}
		m_PeerAddr.s_addr = srv_addr.s_addr;
		mRemoteRtpPort = usServerRtpPort;
		if(usServerRtcpPort == 0)
			mRemoteRtcpPort = mRemoteRtpPort + 1;
	}

	if(mLocalRtpPort == 0){
		mLocalRtpPort = DEF_RTP_PORT;
	}
	mLocalRtcpPort = mLocalRtpPort + 1;

	snprintf(cmdBuf, HEADER_BUF_SIZE, 
			"SETUP %s RTSP/%s\r\n" \
			"CSeq: %d\r\n" \
			"Transport: RTP/AVP;unicast;client_port=%u-%u\r\n" \
			"\r\n",
			m_pszUrl, RTSP_VERSION, 
			++m_ulSeq,
			mLocalRtpPort, mLocalRtcpPort);

	int len =  strlen(cmdBuf);

	if(len) {
		SendRtspCommand(cmdBuf, len);
		GetRtspStatus(statusBuf, HEADER_BUF_SIZE, DRB_STATUS_TIMEOUT);
		HandleAnswerSetup("video", statusBuf);
		res = 0;
	}
#if 0
	if(pMediaDescript)
		delete pMediaDescript;
#endif
	return res;
}

void CJdDrbRtspClnt::SendPlay()
{
	int nLen = 0;
	char headerBuf[HEADER_BUF_SIZE];

	nLen = FindServer(headerBuf,SDP_BUF_SIZE);
	if(nLen > 0) {
		//res = Announce();
		//if(res==0)
		SendSetup(headerBuf);
	}
	int len = CreateHeader(RTSP_METHOD_PLAY, "video", headerBuf, HEADER_BUF_SIZE);
	SendRtspCommand(headerBuf, strlen(headerBuf));
	GetRtspStatus(headerBuf, HEADER_BUF_SIZE, DRB_STATUS_TIMEOUT);
}

void CJdDrbRtspClnt::Close()
{
	int bufsize = REQUEST_BUF_SIZE;
	int tempSize;
	char *requestBuf = (char *)malloc(bufsize);
	char szTemp[128];
	char headerBuf[HEADER_BUF_SIZE];
	int ret = -1;

	if(m_pVRtp){
		m_pVRtp->Stop();
	}

	int len = CreateHeader(RTSP_METHOD_TEARDOWN, "video", headerBuf, HEADER_BUF_SIZE);
	SendRtspCommand(headerBuf, strlen(headerBuf));
	GetRtspStatus(headerBuf, HEADER_BUF_SIZE, DRB_STATUS_TIMEOUT);

	if(m_pVRtp){
		m_pVRtp->CloseSession();
		delete m_pVRtp;
		m_pVRtp = NULL;
	}
Exit:
	return;
}
