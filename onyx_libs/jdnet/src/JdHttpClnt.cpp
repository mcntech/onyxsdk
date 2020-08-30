#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
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
#include "JdHttpClnt.h"
#include "JdNetUtil.h"

#ifndef MSG_NO_SIGNAL
#define MSG_NOSIGNAL	0
#endif

#define PORT_NUMBER 			80
#define HTTP_VERSION 			"HTTP/1.0"
#define DEFAULT_USER_AGENT		"HTTP NTVRenderer"
#define VERSION					"1.0"
#define DEFAULT_READ_TIMEOUT	30		/* Seconds to wait before giving up
										 *	when no data is arriving */
#define DEFAULT_WRITE_TIMEOUT	30
#define REQUEST_BUF_SIZE 		1024
#define HEADER_BUF_SIZE 		1024
#define DEFAULT_PAGE_BUF_SIZE 	1024 * 200	/* 200K should hold most things */
#define DEFAULT_REDIRECTS       3       /* Number of HTTP redirects to follow */

/* Error sources */
#define FETCHER_ERROR	0
#define ERRNO			1
#define H_ERRNO			2

/* HTTP Fetcher error codes */
#define HF_NULLURL		2
#define HF_HEADTIMEOUT	3
#define HF_DATATIMEOUT	4
#define HF_FRETURNCODE	5
#define HF_CRETURNCODE	6
#define HF_STATUSCODE	7
#define HF_CONTENTLEN	8
#define HF_HERROR		9
#define HF_CANTREDIRECT 10
#define HF_MAXREDIRECTS 11

/* Globals */
int timeout = DEFAULT_READ_TIMEOUT;
char *userAgent = NULL;
char *referer = NULL;
int hideUserAgent = 0;
int hideReferer = 1;
static int followRedirects = DEFAULT_REDIRECTS;	/* # of redirects to follow */

int gcontentLength = 0;

static int makeSocket(const char *host);
static int CheckBufSize(char **buf, int *bufsize, int more);

int CHttpPut::GetS3Headers(char *pData, int nMaxLen)
{
	char szTmpBuf[128];
	int nUsedLen = 0;
	if(m_nAmzAcl != AMZ_ACL_NONE){
		switch(m_nAmzAcl)
		{
		case 	AMZ_ACL_PRIVATE:
			sprintf(szTmpBuf,X_AMZ_ACL ": " ACL_STR_PRIVATE "\r\n");
			break;
		case 	AMZ_ACL_PUBLIC_READ:
			sprintf(szTmpBuf,X_AMZ_ACL ": " ACL_STR_PUBLIC_READ "\r\n");
			break;
		case 	AMZ_ACL_PUBLIC_AUTH_READ:
			sprintf(szTmpBuf,X_AMZ_ACL ": " ACL_STR_AUTH_READ "\r\n");
			break;
		case 	AMZ_ACL_PUBLIC_OWNER_READ:
			sprintf(szTmpBuf,X_AMZ_ACL ": " ACL_STR_OWNER_READ "\r\n");
			break;
		case 	AMZ_ACL_PUBLIC_OWNER_FULL_CTRL:
			sprintf(szTmpBuf,X_AMZ_ACL ": " ACL_STR_OWNER_FULL_CTRL "\r\n");
			break;
		case AMZ_ACL_PUBLIC_READ_WRITE:
		default:
			sprintf(szTmpBuf,X_AMZ_ACL ": " ACL_STR_PUBLIC_READ_WRITE "\r\n");
			break;

		}
		int nLen = strlen(szTmpBuf);
		if(nUsedLen + nLen < nMaxLen){
			nUsedLen += nLen;
			strcpy(pData, szTmpBuf);
		}
	}
	if(m_nContentType != CONTENT_TYPE_NONE){
		switch(m_nContentType)
		{
		case 	CONTENT_TYPE_HTML:
			sprintf(szTmpBuf,CONTENT_STR_TYPE ": " CONTENT_STR_HTML "\r\n");
			break;
		case 	CONTENT_TYPE_M3U8:
			sprintf(szTmpBuf,CONTENT_STR_TYPE ": " CONTENT_STR_M3U8 "\r\n");
			break;
		case 	CONTENT_TYPE_MP2T:
			sprintf(szTmpBuf,CONTENT_STR_TYPE ": " CONTENT_STR_MP2T "\r\n");
			break;
		default:
			sprintf(szTmpBuf,CONTENT_STR_TYPE ": " CONTENT_STR_DEF "\r\n");
			break;

		}
		int nLen = strlen(szTmpBuf);
		if(nUsedLen + nLen < nMaxLen){
			nUsedLen += nLen;
			strcat(pData, szTmpBuf);
		} else {
			fprintf(stderr, "buffer overflow%s:%d\n",__FUNCTION__,__LINE__);
		}
	}

	return nUsedLen;
}

int CHttpPut::Open(const char *url_tmp, int nContentLen)
{
	int res = -1;
	fd_set rfds;
	struct timeval tv;
	char szTmpBuf[128];
	char *tmp, *url = NULL, *requestBuf = NULL, *host, *charIndex;
	int sock, bufsize = REQUEST_BUF_SIZE;
	int tempSize;
	
	if(url_tmp == NULL)	{
		fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
		return -1;
	}
	/* Copy the url passed in into a buffer we can work with, change, etc. */
	url = (char *)malloc(strlen(url_tmp)+1);
	if(url == NULL)	{
		fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
		return -1;
	}
	strcpy(url, url_tmp);
	
	/* Seek to the file path portion of the url */
	charIndex = strstr(url, "://");
	if(charIndex != NULL)	{
		/* url contains a protocol field */
		charIndex += strlen("://");
		host = charIndex;
		charIndex = strchr(charIndex, '/');
	} else {
		host = (char *)url;
		charIndex = strchr(url, '/');
	}
	/* Compose a request string */
	requestBuf = (char *)malloc(bufsize);
	if(requestBuf == NULL) {
		fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
		goto Exit;
	}
	requestBuf[0] = 0;

	if(charIndex == NULL)	{
		/* The url has no '/' in it, assume the user is making a root-level
		 *	request */ 
		tempSize = strlen("PUT /") + strlen(HTTP_VERSION) + 2;
		if(CheckBufSize(&requestBuf, &bufsize, tempSize) ||
			snprintf(requestBuf, bufsize, "GET / %s\r\n", HTTP_VERSION) < 0) {
			goto Exit;
		}
	} else	{
		tempSize = strlen("PUT ") + strlen(charIndex) +
          strlen(HTTP_VERSION) + 4;
	 	/* + 4 is for ' ', '\r', '\n', and NULL */
                                
		if(CheckBufSize(&requestBuf, &bufsize, tempSize) ||
				snprintf(requestBuf, bufsize, "PUT %s %s\r\n",charIndex, HTTP_VERSION) < 0){
			fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
			goto Exit;
		}
	}

	/* Null out the end of the hostname if need be */
	if(charIndex != NULL)
		*charIndex = 0;

	tempSize = (int)strlen("Host: ") + (int)strlen(host) + 3;
    /* +3 for "\r\n\0" */
	if(CheckBufSize(&requestBuf, &bufsize, tempSize + 128)){
		fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
		goto Exit;
	}
	strcat(requestBuf, "Host: ");
	strcat(requestBuf, host);
	strcat(requestBuf, "\r\n");

	if(!hideReferer && referer != NULL){	/* NO default referer */
		tempSize = (int)strlen("Referer: ") + (int)strlen(referer) + 3;
        /* + 3 is for '\r', '\n', and NULL */
		if(CheckBufSize(&requestBuf, &bufsize, tempSize))	{
			fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
			goto Exit;
		}
		strcat(requestBuf, "Referer: ");
		strcat(requestBuf, referer);
		strcat(requestBuf, "\r\n");
		}

	if(!hideUserAgent && userAgent == NULL)	{
		tempSize = (int)strlen("User-Agent: ") +
			(int)strlen(DEFAULT_USER_AGENT) + (int)strlen(VERSION) + 4;
        /* + 4 is for '\', '\r', '\n', and NULL */
		if(CheckBufSize(&requestBuf, &bufsize, tempSize))	{
			fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
			goto Exit;
		}
		strcat(requestBuf, "User-Agent: ");
		strcat(requestBuf, DEFAULT_USER_AGENT);
		strcat(requestBuf, "/");
		strcat(requestBuf, VERSION);
		strcat(requestBuf, "\r\n");
	} else if(!hideUserAgent) {
		tempSize = (int)strlen("User-Agent: ") + (int)strlen(userAgent) + 3;
        /* + 3 is for '\r', '\n', and NULL */
		if(CheckBufSize(&requestBuf, &bufsize, tempSize))	{
			fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
			goto Exit;
		}
		strcat(requestBuf, "User-Agent: ");
		strcat(requestBuf, userAgent);
		strcat(requestBuf, "\r\n");
	}

	tempSize = GetS3Headers(szTmpBuf, 128);
	if(tempSize > 0) {
		if(CheckBufSize(&requestBuf, &bufsize, tempSize)) {
			fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
			goto Exit;	
		}
		fprintf(stderr, "S3 Headers : %s\n",szTmpBuf);
		strcat(requestBuf, szTmpBuf);
	}


	sprintf(szTmpBuf,"Content-Length: %d\r\n",nContentLen);
	tempSize = strlen(szTmpBuf);
	if(CheckBufSize(&requestBuf, &bufsize, tempSize)) {
		fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
		goto Exit;	
	}
	strcat(requestBuf, szTmpBuf);
	

	tempSize = (int)strlen("Connection: Close\r\n\r\n");
	if(CheckBufSize(&requestBuf, &bufsize, tempSize)) {
		fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
		goto Exit;	
	}
	strcat(requestBuf, "Connection: Close\r\n\r\n");

	/* Now free any excess memory allocated to the buffer */
	tmp = (char *)realloc(requestBuf, strlen(requestBuf) + 1);
	if(tmp == NULL)	{
		fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
		goto Exit;	
	}
	requestBuf = tmp;

	sock = makeSocket(host);		/* errorSource set within makeSocket */
	if(sock == -1) { 
		fprintf(stderr, "%s:%d\n",__FUNCTION__,__LINE__);
		goto Exit;
	}

	if(send(sock, requestBuf, strlen(requestBuf), MSG_NOSIGNAL) == -1)	{
		fprintf(stderr, "%s:send Failed\n",__FUNCTION__);
		goto Exit;
	}
	res = 0; // success
Exit:
	if(url) free(url);
	if(requestBuf) free(requestBuf);

	m_hSock = sock;
	return res;
}

int CHttpPut::GetStatus()
{
	int ret = -1;
	char *charIndex;
	int nStatusCode;
	char headerBuf[HEADER_BUF_SIZE];
	/* Grab enough of the response to get the metadata */
	ret = ReadHeader(m_hSock, headerBuf);	/* errorSource set within */
	if(ret < 0) { 
		fprintf(stderr, "%s : ReadHeader Failed\n",__FUNCTION__);
		goto Exit; 
	}

	/* Get the return code */
	charIndex = strstr(headerBuf, "HTTP/");
	if(charIndex == NULL) { 
		fprintf(stderr, "%s : Header Invalid\n",__FUNCTION__);
		goto Exit; 
	}

	while(*charIndex != ' ')
		charIndex++;
	charIndex++;

	ret = sscanf(charIndex, "%d", &nStatusCode);
	if(ret != 1){
		fprintf(stderr, "%s : Failed to get Status Code\n",__FUNCTION__);
		goto Exit;
	}
	if(nStatusCode < 200 || nStatusCode > 307)	{
		fprintf(stderr, "%s : Status Code=%d\n",__FUNCTION__,nStatusCode);
		ret = -1;
	} else {
		ret = 0;
	}


Exit:
	return ret;
}

void CHttpPut::Close()
{
	if(m_hSock >= 0){
		close(m_hSock);
		m_hSock = -1;
	}
}

int CHttpPut::Write(char *pData, int nLen, int nTimeOut)
{
	fd_set wfds;
	struct timeval tv;
	char headerBuf[HEADER_BUF_SIZE];
	int ret = -1;
	int	selectRet;
	int bytesWritten = 0;
	int reqlen;
	/* Begin reading the body of the file */

	FD_ZERO(&wfds);
	FD_SET(m_hSock, &wfds);


	while (bytesWritten < nLen) {
		reqlen = nLen - bytesWritten;
		tv.tv_sec = nTimeOut; 
		tv.tv_usec = 0;

		if(nTimeOut > 0)
			selectRet = select(m_hSock+1, NULL, &wfds, NULL, &tv);
		else		/* No timeout, can block indefinately */
			selectRet = select(m_hSock+1, NULL, &wfds, NULL, NULL);

		if(selectRet == 0) {
			fprintf(stderr, "%s : select return 0\n",__FUNCTION__);
			return -1;
		} else if(selectRet == -1)	{
			fprintf(stderr, "%s : select failed\n",__FUNCTION__);
			return -1;
		}

		ret = send(m_hSock, pData + bytesWritten, reqlen, MSG_NOSIGNAL);
		if(ret == -1)	{
			fprintf(stderr, "%s : send failed\n",__FUNCTION__);
			return -1;
		}
		bytesWritten += ret;
		if(reqlen != ret){
			fprintf(stderr, "%s : partial write reqlen=%d written=a5d\n",__FUNCTION__, reqlen, ret);
		}
	}
	return bytesWritten;
}

int CHttpPut::Write(char *pData, int nLen)
{
	return Write(pData, nLen, DEFAULT_WRITE_TIMEOUT);
}
//===========================================================================
int CHttpGet::Open(const char *url_tmp)
{
	fd_set rfds;
	struct timeval tv;
	char headerBuf[HEADER_BUF_SIZE];
	char *tmp, *url,  *requestBuf = NULL, *host, *charIndex;
	int sock, bufsize = REQUEST_BUF_SIZE;
	int i,
		ret = -1,
		tempSize,
		selectRet,
		found = 0,	/* For redirects */
		redirectsFollowed = 0;
	gcontentLength = -1;

	if(url_tmp == NULL)	{
		return -1;
	}

	/* Copy the url passed in into a buffer we can work with, change, etc. */
	url = (char *)malloc(strlen(url_tmp)+1);
	if(url == NULL)	{
		return -1;
	}
	strcpy(url, url_tmp);
	
	do	{
		/* Seek to the file path portion of the url */
		charIndex = strstr(url, "://");
		if(charIndex != NULL)	{
			/* url contains a protocol field */
			charIndex += strlen("://");
			host = charIndex;
			charIndex = strchr(charIndex, '/');
		} else {
			host = (char *)url;
			charIndex = strchr(url, '/');
		}

		/* Compose a request string */
		requestBuf = (char *)malloc(bufsize);
		if(requestBuf == NULL) {
			free(url);
			return -1;
		}
		requestBuf[0] = 0;

		if(charIndex == NULL)	{
			/* The url has no '/' in it, assume the user is making a root-level
			 *	request */ 
			tempSize = strlen("GET /") + strlen(HTTP_VERSION) + 2;
			if(CheckBufSize(&requestBuf, &bufsize, tempSize) ||
				snprintf(requestBuf, bufsize, "GET / %s\r\n", HTTP_VERSION) < 0) {
				free(url);
				free(requestBuf);
				return -1;
			}
		} else	{
			tempSize = strlen("GET ") + strlen(charIndex) +
  	          strlen(HTTP_VERSION) + 4;
		 	/* + 4 is for ' ', '\r', '\n', and NULL */
                                    
			if(CheckBufSize(&requestBuf, &bufsize, tempSize) ||
					snprintf(requestBuf, bufsize, "GET %s %s\r\n",charIndex, HTTP_VERSION) < 0){
				free(url);
				free(requestBuf);
				return -1;
			}
		}

		/* Null out the end of the hostname if need be */
		if(charIndex != NULL)
			*charIndex = 0;

		/* Use Host: even though 1.0 doesn't specify it.  Some servers
		 *	won't play nice if we don't send Host, and it shouldn't
		 *	hurt anything */
		ret = bufsize - strlen(requestBuf); /* Space left in buffer */
		tempSize = (int)strlen("Host: ") + (int)strlen(host) + 3;
        /* +3 for "\r\n\0" */
		if(CheckBufSize(&requestBuf, &bufsize, tempSize + 128)){
			free(url);
			free(requestBuf);
			return -1;
		}
		strcat(requestBuf, "Host: ");
		strcat(requestBuf, host);
		strcat(requestBuf, "\r\n");

		if(!hideReferer && referer != NULL){	/* NO default referer */
			tempSize = (int)strlen("Referer: ") + (int)strlen(referer) + 3;
   	        /* + 3 is for '\r', '\n', and NULL */
			if(CheckBufSize(&requestBuf, &bufsize, tempSize))	{
				free(url);
				free(requestBuf);
				return -1;
			}
			strcat(requestBuf, "Referer: ");
			strcat(requestBuf, referer);
			strcat(requestBuf, "\r\n");
			}

		if(!hideUserAgent && userAgent == NULL)	{
			tempSize = (int)strlen("User-Agent: ") +
				(int)strlen(DEFAULT_USER_AGENT) + (int)strlen(VERSION) + 4;
   	        /* + 4 is for '\', '\r', '\n', and NULL */
			if(CheckBufSize(&requestBuf, &bufsize, tempSize))	{
				free(url);
				free(requestBuf);
				return -1;
			}
			strcat(requestBuf, "User-Agent: ");
			strcat(requestBuf, DEFAULT_USER_AGENT);
			strcat(requestBuf, "/");
			strcat(requestBuf, VERSION);
			strcat(requestBuf, "\r\n");
		} else if(!hideUserAgent) {
			tempSize = (int)strlen("User-Agent: ") + (int)strlen(userAgent) + 3;
   	        /* + 3 is for '\r', '\n', and NULL */
			if(CheckBufSize(&requestBuf, &bufsize, tempSize))	{
				free(url);
				free(requestBuf);
				return -1;
			}
			strcat(requestBuf, "User-Agent: ");
			strcat(requestBuf, userAgent);
			strcat(requestBuf, "\r\n");
		}

		tempSize = (int)strlen("Connection: Close\r\n\r\n");
		if(CheckBufSize(&requestBuf, &bufsize, tempSize)) {
			free(url);
			free(requestBuf);
			return -1;
		}
		strcat(requestBuf, "Connection: Close\r\n\r\n");

		/* Now free any excess memory allocated to the buffer */
		tmp = (char *)realloc(requestBuf, strlen(requestBuf) + 1);
		if(tmp == NULL)	{
			free(url);
			free(requestBuf);
			return -1;
		}
		requestBuf = tmp;

		sock = makeSocket(host);		/* errorSource set within makeSocket */
		if(sock == -1) { free(url); free(requestBuf); return -1;}

		free(url);
        url = NULL;

		if(send(sock, requestBuf, strlen(requestBuf), MSG_NOSIGNAL) == -1)	{
			close(sock);
			free(requestBuf);
			return -1;
		}

		free(requestBuf);
        requestBuf = NULL;

		/* Grab enough of the response to get the metadata */
		ret = ReadHeader(sock, headerBuf);	/* errorSource set within */
		if(ret < 0) { close(sock); return -1; }

		/* Get the return code */
		charIndex = strstr(headerBuf, "HTTP/");
		if(charIndex == NULL) {
			close(sock);
			return -1;
		}
		while(*charIndex != ' ')
			charIndex++;
		charIndex++;

		ret = sscanf(charIndex, "%d", &i);
		if(ret != 1){
			close(sock);
			return -1;
		}
		if(i<200 || i>307)	{
			close(sock);
			fprintf(stderr, "Resource not available(error code=%d)\n",i);
			return -1;
		}

		/* If a redirect, repeat operation until final URL is found or we
		 *  redirect followRedirects times.  Note the case sensitive "Location",
		 *  should probably be made more robust in the future (without relying
		 *  on the non-standard strcasecmp()).
		 * This bit mostly by Dean Wilder, tweaked by me */
		if(i >= 300) {
		    redirectsFollowed++;

			/* Pick up redirect URL, allocate new url, and repeat process */
			charIndex = strstr(headerBuf, "Location:");
			if(!charIndex)	{
				close(sock);
				return -1;
			}
			charIndex += strlen("Location:");
            /* Skip any whitespace... */
            while(*charIndex != '\0' && isspace(*charIndex))
                charIndex++;
            if(*charIndex == '\0') {
				close(sock);
				return -1;
               }

			i = strcspn(charIndex, " \r\n");
			if(i > 0) {
				url = (char *)malloc(i + 1);
				strncpy(url, charIndex, i);
				url[i] = '\0';
			} else
                /* Found 'Location:' but contains no URL!  We'll handle it as
                 * 'found', hopefully the resulting document will give the user
                 * a hint as to what happened. */
                found = 1;
		} else
			found = 1;
	} while(!found && (followRedirects < 0 || redirectsFollowed <= followRedirects) );

    if(url){ /* Redirection code may malloc this, then exceed followRedirects */
        free(url);
        url = NULL;
	}
    
    if(redirectsFollowed >= followRedirects && !found) {
        close(sock);
	    return -1;
	}
	
	charIndex = strstr(headerBuf, "Content-Length:");
	if(charIndex == NULL)
		charIndex = strstr(headerBuf, "Content-length:");

	if(charIndex != NULL) {
		ret = sscanf(charIndex + strlen("content-length: "), "%d",
			&gcontentLength);
		if(ret < 1)	{
			close(sock);
			return -1;
		}
	}
	
	m_hSock = sock;
	return 0;
}

void CHttpGet::Close()
{
	if(m_hSock >= 0){
		close(m_hSock);
		m_hSock = -1;
	}
}

int CHttpGet::Read(char *pData, int nMaxLen)
{
	fd_set rfds;
	struct timeval tv;
	char headerBuf[HEADER_BUF_SIZE];
	int ret = -1;
	int	selectRet;
	int bytesRead = 0;
	/* Begin reading the body of the file */

	FD_ZERO(&rfds);
	FD_SET(m_hSock, &rfds);


	while (bytesRead < nMaxLen) {
		int reqlen = nMaxLen - bytesRead;
		tv.tv_sec = timeout; 
		tv.tv_usec = 0;

		if(timeout >= 0)
			selectRet = select(m_hSock+1, &rfds, NULL, NULL, &tv);
		else		/* No timeout, can block indefinately */
			selectRet = select(m_hSock+1, &rfds, NULL, NULL, NULL);

		if(selectRet == 0) {
			return -1;
		} else if(selectRet == -1)	{
			return -1;
		}

		ret = recv(m_hSock, pData + bytesRead, reqlen, 0);
		if(ret == -1)	{
			return -1;
		}
        else if (ret == 0) {
            // eof
            break;
        }
		bytesRead += ret;
	}
	return bytesRead;
}

/*
 * Opens a TCP socket and returns the descriptor
 * Returns:
 *	socket descriptor, or
 *	-1 on error
 */
int makeSocket(const char *host)
{
	int sock;										/* Socket descriptor */
	struct sockaddr_in sa;							/* Socket address */
	struct hostent *hp;								/* Host entity */
	int ret;
    int port;
    char *p;
	
    /* Check for port number specified in URL */
    p = (char *)strchr(host, ':');
    if(p) {
        port = atoi(p + 1);
        *p = '\0';
	} else
		port = PORT_NUMBER;

	hp = gethostbyname(host);
	if(hp == NULL) {  
		fprintf(stderr, "%s: gethostbyname %s failed\n",__FUNCTION__,host);
		return -1; 
	}
		
	/* Copy host address from hostent to (server) socket address */
	memcpy((char *)&sa.sin_addr, (char *)hp->h_addr, hp->h_length);
	sa.sin_family = hp->h_addrtype;		/* Set service sin_family to PF_INET */
	sa.sin_port = htons(port);      	/* Put portnum into sockaddr */

	sock = socket(hp->h_addrtype, SOCK_STREAM, 0);
	if(sock == -1) {  
		fprintf(stderr, "%s: socket failed\n",__FUNCTION__);
		return -1; 
	}

	ret = connect(sock, (struct sockaddr *)&sa, sizeof(sa));
	if(ret == -1) {
		fprintf(stderr, "%s: connect failed\n",__FUNCTION__);
		return -1; 
	}

	return sock;
}



/*
 * Determines if the given NULL-terminated buffer is large enough to
 * 	concatenate the given number of characters.  If not, it attempts to
 * 	grow the buffer to fit.
 * Returns:
 *	0 on success, or
 *	-1 on error (original buffer is unchanged).
 */
int CheckBufSize(char **buf, int *bufsize, int more)
{
	char *tmp;
	int roomLeft = *bufsize - (strlen(*buf) + 1);
	if(roomLeft > more)
		return 0;
	tmp = (char *)realloc(*buf, *bufsize + more + 1);
	if(tmp == NULL)
		return -1;
	*buf = tmp;
	*bufsize += more + 1;
	return 0;
}
//============================================================

int HttpPutResource(char *pszHost, char *pszResource, char *pData, int nLen, int nTimeOut, 
			int nContentType, 
			int nAmzAcl)
{
	int res = 0;
	int nUrlLen = strlen(pszHost) + strlen(pszResource) + strlen("http://") + 2;
	char *pszUrl = (char *)malloc(nUrlLen);
	CHttpPut HttpPut;
	HttpPut.m_nAmzAcl = nAmzAcl;
	HttpPut.m_nContentType = nContentType;
	sprintf(pszUrl,"http://%s/%s",pszHost,pszResource);
	fprintf(stderr, "<%d ContentType =%d AmzAcl=%d", nLen, nAmzAcl, nContentType);
	res = HttpPut.Open(pszUrl, nLen);
	if(res == 0) {
		if(pData && nLen > 0)
			nLen = HttpPut.Write(pData, nLen, nTimeOut);
		res = HttpPut.GetStatus();
	} else {
		fprintf(stderr, "%s : Failed to open %s\n",__FUNCTION__,pszUrl);
	}
	HttpPut.Close();

	if(pszUrl)
		free(pszUrl);
	fprintf(stderr, ">");
	return res;
}

void *httpputStart(char *pszHost, char *pszResource, int nTotalLen, char *pData, int nLen)
{
	int res = 0;
	int nUrlLen = strlen(pszHost) + strlen(pszResource) + strlen("http://") + 2;
	char *pszUrl = (char *)malloc(nUrlLen);
	CHttpPut *pHttpPut = new CHttpPut;
	sprintf(pszUrl,"http://%s/%s",pszHost,pszResource);
	fprintf(stderr, "<");
	res = pHttpPut->Open(pszUrl, nTotalLen);
	if(res == 0) {
		if(pData && nLen > 0)
			nLen = pHttpPut->Write(pData, nLen);
	} else {
		fprintf(stderr, "%s : Failed to open %s\n",__FUNCTION__,pszUrl);
	}

	if(pszUrl)
		free(pszUrl);
	if(res == 0){
		return pHttpPut;
	} else {
		delete pHttpPut;
		return NULL;
	}
}

int httpputAdd(void *pCtx, char *pData, int nLen)
{
	int res = 0;

	CHttpPut *pHttpPut = (CHttpPut *)pCtx;
	fprintf(stderr, ".");
	if(pData && nLen > 0)
		nLen = pHttpPut->Write(pData, nLen);
	return nLen;
}

int httpputEnd(void *pCtx, char *pData, int nLen)
{
	int res = 0;
	CHttpPut *pHttpPut = (CHttpPut *)pCtx;

	if(pData && nLen > 0)
		nLen = pHttpPut->Write(pData, nLen);
	res = pHttpPut->GetStatus();
	delete pHttpPut;
	fprintf(stderr, ">");
	return res;
}

int HttpGetResource(char *pszHost, char *pszResource, char *pData, int nMaxLen)
{
	int nLen = 0;
	int nUrlLen = strlen(pszHost) + strlen(pszResource) + strlen("http://") + 2;
	char *pszUrl = (char *)malloc(nUrlLen);
	CHttpGet HttpGet;
	sprintf(pszUrl,"http://%s/%s",pszHost,pszResource);

	if(HttpGet.Open(pszUrl)==0 ){
		nLen = HttpGet.Read(pData, nMaxLen - 1);
		pData[nLen] = 0;
	}
	HttpGet.Close();
	if(pszUrl)
		free(pszUrl);

	return nLen;
}
