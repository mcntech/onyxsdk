#ifdef WIN32
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
#include "JdRtpRcv.h"
#include "JdRtspClnt.h"
#include "JdNetUtil.h"
#include <string>
#include <algorithm>
#include <cctype>

#ifdef WIN32
#define close		closesocket
#define open		_open
#define O_RDONLY	_O_RDONLY
#define O_BINARY	_O_BINARY
#else // Linux
#define O_BINARY	0
#define INT32       int
#define strnicmp strncasecmp
#define stricmp  strcasecmp
#endif

#define EN_QUIRK_VLC		// For testing with VLC
#define DEF_RTSP_PORT 			554
#define DEF_RTP_PORT 			59427


#define REQUEST_BUF_SIZE 		1024
#define HEADER_BUF_SIZE 		1024
#define DEFAULT_REDIRECTS       3       /* Number of HTTP redirects to follow */


#define CHECK_ALLOC(x) if(x==NULL) {fprintf(stderr,"malloc failed at %s %d, exiting..\n", __FUNCTION__, __LINE__); goto Exit;}

/* Globals */
static int followRedirects = DEFAULT_REDIRECTS;	/* # of redirects to follow */

/**
 * Creates the reuest header
 */
int CJdRtspClntSession::CreateHeader(int nHeaderId,  char *pszTrack,char *pBuff, int nMaxLen)
{
	int tempSize = 0;
	switch(nHeaderId)
	{
	case RTSP_METHOD_DESCRIBE:
		if(!m_pszUrl.empty()) {
			snprintf(pBuff, nMaxLen, 
					"DESCRIBE %s RTSP/%s\r\n" \
					"CSeq: %d\r\n" \
					"User-Agent: %s\r\n" \
					"Accept: application/sdp\r\n" \
					"\r\n",
					m_pszUrl.c_str(), RTSP_VERSION, 
				++m_ulSeq,
					RTSP_CLIENT_NAME);

			return strlen(pBuff);
		}

		return -1;


	case RTSP_METHOD_PLAY:
		{
			if(!m_pszUrl.empty()){
				snprintf(pBuff, nMaxLen,
					"PLAY %s RTSP/%s\r\n" \
					"CSeq: %d\r\n" \
					"User-Agent: %s\r\n" \
					"Session: %u\r\n" \
					"\r\n",
					m_pszUrl.c_str(), RTSP_VERSION,
					++m_ulSeq,
					RTSP_CLIENT_NAME,
					m_ulSessionId);

				return strlen(pBuff);
			}
		}
		return -1;

	case RTSP_METHOD_TEARDOWN:
		{
			std::string url;
			if(!m_pszUrl.empty()) {
				snprintf(pBuff, nMaxLen,
					"TEARDOWN %s RTSP/%s\r\n" \
					"CSeq: %d\r\n" \
					"User-Agent: %s\r\n" \
					"Session: %u\r\n" \
					"\r\n",
					m_pszUrl.c_str(), RTSP_VERSION,
					++m_ulSeq,
					RTSP_CLIENT_NAME,
					m_ulSessionId);

				return strlen(pBuff);
			}
		}
		return -1;

	default:
		return -1;
	}
}

/**
 * Creates the setup header
 */
int CJdRtspClntSession::CreateSetupHeader(
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

/*
     Response    =     Status-Line         ; Section 7.1
                 *(    general-header      ; Section 5
                 |     response-header     ; Section 7.1.2
                 |     entity-header )     ; Section 8.1
                       CRLF
                       [ message-body ]    ; Section 4.3
*/
int CJdRtspClntSession::ParseResponseMessage(char *pData, int nLen)
{
	resp_headers.clear();
	
    char                *pBbuff = (char *)malloc(nLen);
    char                *pHeader; 
    char                *pTmp;
	int                 statusCode = 0;
	memcpy(pBbuff,pData, nLen);
	do {
		/* Extract RTSP Version from Response Message                             */
		pHeader = strtok(pBbuff, "\r\n");
		if(pHeader == NULL)
			break;

		pTmp = (char *) strstr(pBbuff, " ");

		/* Validate Size of RTSP Version in Response Buffer                       */
		if (pHeader == NULL || pTmp == NULL || (pTmp - pHeader) > (INT32) strlen("RTSP/1.0")) {
			break;
		}
		*pTmp = 0;


		/* Extract Status Code from Response Message                              */
		pHeader = pTmp + 1;
		pTmp = (char *) strstr(pHeader, " ");
		if (pTmp == NULL) {
			break;
		}
		statusCode = atoi(pHeader);

		// Process all the headers
		pHeader = strtok(NULL, "\r\n");
		while(pHeader != NULL) {
			/* Get the name of header                                             */
			pTmp = (char *) strstr( pHeader, ":");
			if (pTmp == NULL) {
				break;
			}
			*pTmp++ = 0x00; // Remove ':'
			// Remove space chars
			std::string entity_hdr = pHeader;
			std::transform(entity_hdr.begin(), entity_hdr.end(), entity_hdr.begin(), ::tolower);

			while (*pTmp == ' ')
				pTmp++;
			// Make lower case
			resp_headers[entity_hdr.c_str()] = pTmp;
			pHeader = strtok(NULL, "\r\n");
		}
	} while(0);
	free(pBbuff);
	return statusCode;
}
#if 0
int CJdRtspClntSession::OpenWithSdp(const char *pszData)
{
	const char *pszMedia;
	CMediaDescription *pMediaDescript = NULL;
	//TODO: SDP Parsing
	/* Locate media description record */
	pszMedia = strstr(pszData,"m=");
	pMediaDescript = new CMediaDescription(pszMedia);
	if(pMediaDescript->m_NumConnections &&  pMediaDescript->m_listConnections[0]->m_pszAddress)
		m_PeerAddr.s_addr = inet_addr(pMediaDescript->m_listConnections[0]->m_pszAddress);
	if(pMediaDescript->m_usPort)
		m_usServerRtpPort = pMediaDescript->m_usPort;
	else
		m_usServerRtpPort = DEF_RTP_PORT;
	m_usServerRtcpPort = m_usServerRtpPort + 1;
	m_pVRtp	= new CRtp(CRtp::MODE_CLIENT);
	m_pVRtp->CreateSession(&m_PeerAddr,	mClientRtpPort,	m_usClientRtcpPort, m_usServerRtpPort,	m_usServerRtcpPort);

Exit:

	if(pMediaDescript)
		delete pMediaDescript;
	return 0;
}
#endif

int CJdRtspClntSession::GetVideoCodec()
{
	for (int i =0; i < m_sdp.m_NumeMediaDescriptions; i++) {
		if(stricmp(m_sdp.m_listMediaDescription[i]->m_pszMedia, "video") == 0){
			unsigned char ucPl = m_sdp.m_listMediaDescription[i]->m_ucPl;
			if(ucPl >= RTP_PT_DYNAMIC_START && ucPl <= RTP_PT_DYNAMIC_END) {
				const char *pszRtmap = m_sdp.m_listMediaDescription[i]->GetAttributeValue("rtpmap", 0);
				CAttribRtpmap Rtpmap(pszRtmap);
				if (stricmp(Rtpmap.EncodingName.c_str(), "H264") == 0){
					return RTP_CODEC_H264;
				}
			} else if (ucPl == RTP_PT_MP2T) {
				return RTP_CODEC_MP2T;
			}
		}
	}
	return CODEC_UNSUPPORTED;
}

bool CJdRtspClntSession::GetVideoCodecConfig(unsigned char *pCfg, int *pnSize)
{
	for (int i =0; i < m_sdp.m_NumeMediaDescriptions; i++) {
		if(stricmp(m_sdp.m_listMediaDescription[i]->m_pszMedia, "video") == 0){
			unsigned char ucPl = m_sdp.m_listMediaDescription[i]->m_ucPl;
			if(ucPl >= RTP_PT_DYNAMIC_START && ucPl <= RTP_PT_DYNAMIC_END) {
				const char *pszFmt = m_sdp.m_listMediaDescription[i]->GetAttributeValue("fmtp", 0);
				if(pszFmt) {
					CAttribFmtp Fmtp(pszFmt);
					if(Fmtp.mSpsSize) {
						int len = *pnSize < Fmtp.mSpsSize ? *pnSize : Fmtp.mSpsSize;
						memcpy(pCfg, Fmtp.mSpsConfig, len);
						*pnSize = len;
						return true;
					}
				}
			}
		}
	}
	return false;
}

int CJdRtspClntSession::GetAudioCodec()
{
	for (int i =0; i < m_sdp.m_NumeMediaDescriptions; i++) {
		if(stricmp(m_sdp.m_listMediaDescription[i]->m_pszMedia, "audio") == 0){
			unsigned char ucPl = m_sdp.m_listMediaDescription[i]->m_ucPl;
			if(ucPl >= RTP_PT_DYNAMIC_START && ucPl <= RTP_PT_DYNAMIC_END) {
				const char *pszRtmap = m_sdp.m_listMediaDescription[i]->GetAttributeValue("rtpmap", 0);
				CAttribRtpmap Rtpmap(pszRtmap);
				if(stricmp(Rtpmap.EncodingName.c_str(), "MP4A") == 0)
					return RTP_CODEC_AAC;
				else if(stricmp(Rtpmap.EncodingName.c_str(), "PCMU") == 0)
					return RTP_CODEC_PCMU;
			}
			else if (ucPl == RTP_PT_PCMU)
				return RTP_CODEC_PCMU;
			else if (ucPl == RTP_PT_PCMA)
				return RTP_CODEC_PCMA;
		}
	}
	return CODEC_UNSUPPORTED;
}


/**
 * Opens connection with RTSP Server
 * Handles redirection.
 * Obtains the host name form the pszUrl
 */
int CJdRtspClntSession::Open(const char *pszUrl, int *pnVidCodec, int *pnAudCodec)
{
	char headerBuf[HEADER_BUF_SIZE];
	char *pHeader = NULL;
	char *szTmpUrl = NULL, *requestBuf = NULL, *host, *charIndex;
	int sock = -1, bufsize = REQUEST_BUF_SIZE;
	int result = -1;
	if(m_usServerRtspPort == 0) {
		m_usServerRtspPort = DEF_RTSP_PORT;
	}
	int i,
		ret = -1,
		found = 0,	/* For redirects */
		redirectsFollowed = 0;

	m_pszUrl = strdup(pszUrl);
	szTmpUrl = strdup(m_pszUrl.c_str());
	do	{
		/* Seek to the file path portion of the szTmpUrl */
		charIndex = strstr(szTmpUrl, "://");
		if(charIndex != NULL)	{
			charIndex += strlen("://");
			host = charIndex;
			charIndex = strchr(charIndex, '/');
		} else {
			fprintf(stderr,"Protocol field missing %s",szTmpUrl);
			goto Exit;
		}

		/* Compose a request string */
		requestBuf = (char *)malloc(bufsize);
		pHeader = requestBuf;
		CHECK_ALLOC(requestBuf)

		int len = CreateHeader(RTSP_METHOD_DESCRIBE, NULL, pHeader, bufsize);
		pHeader += len;

		/* Null out the end of the hostname if need be */
		if(charIndex != NULL)
			*charIndex = 0;
		
		/* Extract port if supplied as part of URL */
		{
			char *p;
			/* Check for port number specified in URL */
			p = (char *)strchr(host, ':');
			if(p) {
				m_usServerRtspPort = atoi(p + 1);
				*p = '\0';
			} else
				m_usServerRtspPort = DEF_RTSP_PORT;
		}

		sock = makeSocket(host, m_usServerRtspPort, SOCK_STREAM);		/* errorSource set within makeSocket */
		if(sock == -1) { 
			goto Exit;
		}

		int sentBytes = send(sock, requestBuf, strlen(requestBuf),0);
		if(sentBytes <= 0)	{
			close(sock);
			goto Exit;
		}

		if(szTmpUrl){
			free(szTmpUrl);
			szTmpUrl = NULL;
		}
		/* Grab enough of the response to get the metadata */
		ret = ReadHeader(sock, headerBuf);	/* errorSource set within */
		if(ret < 0)  { 
			close(sock); 
			goto Exit;
		}

		/* Get the return code */
		charIndex = strstr(headerBuf, "RTSP/");
		if(charIndex == NULL) {
			goto Exit;
		}

		while(*charIndex != ' ')
			charIndex++;
		charIndex++;

		ret = sscanf(charIndex, "%d", &i);
		if(ret != 1){
			goto Exit;
		}
		if(i<200 || i>307)	{
			goto Exit;
		}

		/* 
		 * If a redirect, repeat operation until final URL is found or we redirect followRedirects times.  
		 */
		if(i >= 300) {
		    redirectsFollowed++;

			/* Pick up redirect URL, allocate new url, and repeat process */
			charIndex = strstr(headerBuf, "Location:");
			if(!charIndex)	{
				goto Exit;
			}
			charIndex += strlen("Location:");
            /* Skip any whitespace... */
            while(*charIndex != '\0' && isspace(*charIndex))
                charIndex++;
            if(*charIndex == '\0') {
				goto Exit;
			}

			i = strcspn(charIndex, " \r\n");
			if(i > 0) {
				szTmpUrl = (char *)malloc(i + 1);
				strncpy(szTmpUrl, charIndex, i);
				szTmpUrl[i] = '\0';
			} else
                /* Found 'Location:' but contains no url!  We'll handle it as
                 * 'found', hopefully the resulting document will give the user
                 * a hint as to what happened. */
                found = 1;
		} else
			found = 1;
	} while(!found && (followRedirects < 0 || redirectsFollowed <= followRedirects) );

    if(szTmpUrl){ /* Redirection code may malloc this, then exceed followRedirects */
        free(szTmpUrl);
        szTmpUrl = NULL;
	}
    
    if(redirectsFollowed >= followRedirects && !found) {
			goto Exit;
	}

	/* Save the socket for further commands */
	m_hSock = sock;

	HandleAnswerDescribe(headerBuf);

	*pnVidCodec = GetVideoCodec();
	*pnAudCodec = GetAudioCodec();
	//return SendSetup();
	return 0;
Exit:
	if(sock != -1)
		close(sock);
	if(szTmpUrl)
		free(szTmpUrl);
	if(requestBuf)
		free(requestBuf);
	return result;
}

void CJdRtspClntSession::HandleAnswerSetup(char *szStrmType, char *headerBuf)
{
	char *charIndex = strstr(headerBuf, "Session: ");
	CRtspRequestTransport Transport;
	if(charIndex != NULL) {
		sscanf(charIndex + strlen("Session: "), "%d", &m_ulSessionId);
	}
	Transport.ParseTransport(headerBuf);

	CRtp *pRtp	= new CRtp(CRtp::MODE_CLIENT);
	Transport.GetRtpPorts(pRtp, CRtp::MODE_CLIENT);
	pRtp->CreateSession();
	if(strcmp(szStrmType, "video") == 0) {
		m_pVRtp	= pRtp;
	} else if (strcmp(szStrmType, "audio") == 0){
		m_pARtp	= pRtp;
	}
}

void CJdRtspClntSession::HandleAnswerDescribe(char *headerBuf)
{
	ParseResponseMessage(headerBuf, strlen(headerBuf));
	header_map_t::const_iterator iter = resp_headers.find("content-length");
	if(iter != resp_headers.end()) {
		int nContLen = atoi((iter->second).c_str());
		if(nContLen > 0) {
			char *pBuff = (char *)malloc(nContLen);
			int ret = recv(m_hSock, pBuff, nContLen, 0);
			if(ret > 0) {
				m_sdp.Parse(pBuff);
			}
			free(pBuff);
		}
	}
}

int CJdRtspClntSession::SendSetup(
				char *szStrmType,					// audio or video
				unsigned short rtpport, 
				unsigned short rtcpport)
{
	int bufsize = REQUEST_BUF_SIZE;
	int tempSize;
	char headerBuf[HEADER_BUF_SIZE];
	int ret = -1;
	if(m_hSock >= 0){
		int len = CreateSetupHeader(szStrmType, rtpport, rtcpport, headerBuf, HEADER_BUF_SIZE);
		send(m_hSock, headerBuf, strlen(headerBuf), 0);

		ret = ReadHeader(m_hSock, headerBuf);	/* errorSource set within */
		HandleAnswerSetup(szStrmType, headerBuf);
		return 0;
	}
Exit:
	return -1;
}

void CJdRtspClntSession::SendPlay(char *szStrmType /*audio or video*/)
{
	int bufsize = REQUEST_BUF_SIZE;
	int tempSize;
	char headerBuf[HEADER_BUF_SIZE];
	int ret = -1;
	if(strcmp(szStrmType, "video") == 0){
		if(m_pVRtp)
			m_pVRtp->Start();
	} else 	if(strcmp(szStrmType, "audio") == 0){
		if(m_pARtp)
			m_pARtp->Start();
	}
	if(m_hSock >= 0){
		int len = CreateHeader(RTSP_METHOD_PLAY, szStrmType, headerBuf, HEADER_BUF_SIZE);
		send(m_hSock, headerBuf, strlen(headerBuf), 0);
		ret = ReadHeader(m_hSock, headerBuf);	/* errorSource set within */
	}
Exit:
	return;
}

void CJdRtspClntSession::Close()
{
	int bufsize = REQUEST_BUF_SIZE;
	int tempSize;
	char *requestBuf = (char *)malloc(bufsize);
	char szTemp[128];
	char headerBuf[HEADER_BUF_SIZE];
	int ret = -1;

	if(m_pVRtp){
		m_pVRtp->Stop();
		if(m_hSock >= 0){
			int len = CreateHeader(RTSP_METHOD_TEARDOWN, "video", headerBuf, HEADER_BUF_SIZE);
			send(m_hSock, requestBuf, strlen(requestBuf), 0);
			ret = ReadHeader(m_hSock, headerBuf, 1);
		}
		if(m_pVRtp){
			m_pVRtp->CloseSession();
			delete m_pVRtp;
			m_pVRtp = NULL;
		}

	}
	if(m_pARtp){
		m_pARtp->Stop();
		if(m_hSock >= 0){
			int len = CreateHeader(RTSP_METHOD_TEARDOWN, "audio", headerBuf, HEADER_BUF_SIZE);
			send(m_hSock, requestBuf, strlen(requestBuf), 0);
			ret = ReadHeader(m_hSock, headerBuf, 1);
		}
		if(m_pARtp){
			m_pARtp->CloseSession();
			delete m_pARtp;
			m_pARtp = NULL;
		}
	}
	if(m_hSock >= 0){
		close(m_hSock);
		m_hSock = -1;
	}

Exit:
	return;
}


/*
 * Opens a TCP or UDP socket and returns the descriptor
 * Returns:
 *	socket descriptor, or
 *	-1 on error
 */
int CJdRtspClntSession::makeSocket(
		const char *host, 
		unsigned short port, 
		int sock_type)
{
	int sock;										/* Socket descriptor */
	struct sockaddr_in sa;							/* Socket address */
	struct hostent *hp;								/* Host entity */
	int ret;

	hp = gethostbyname(host);
	if(hp == NULL) { 
		return -1; 
	}
		
	/* Copy host address from hostent to (server) socket address */
	memcpy((char *)&sa.sin_addr, (char *)hp->h_addr, hp->h_length);
	sa.sin_family = hp->h_addrtype;		/* Set service sin_family to PF_INET */
	sa.sin_port = htons(port);      	/* Put portnum into sockaddr */

	sock = socket(hp->h_addrtype, sock_type, 0);
	if(sock == -1) {  
		return -1; 
	}

	ret = connect(sock, (struct sockaddr *)&sa, sizeof(sa));

	if(ret == -1) {  
		return -1; 
	}

	return sock;
}

