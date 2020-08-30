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

#define HEADER_BUF_SIZE 		1024


int ReadHeader(int sock, char *pBuff, int timeout)
{
	fd_set rfds;
	struct timeval tv;
	int bytesRead = 0, newlines = 0, ret, selectRet;
	char *headerPtr = pBuff;
	while(newlines != 2 && bytesRead < HEADER_BUF_SIZE - 1) {
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		tv.tv_sec = timeout; 
		tv.tv_usec = 0;

		if(timeout > 0)
			selectRet = select(sock+1, &rfds, NULL, NULL, &tv);
		else		/* No timeout, can block indefinately */
			selectRet = select(sock+1, &rfds, NULL, NULL, NULL);
		
		if(selectRet <= 0)	{
			fprintf(stderr, "%s : select failed\n",__FUNCTION__);
			goto Exit; 
		}

		ret = recv(sock, headerPtr, 1, 0);
		if(ret <= 0) {  
			fprintf(stderr, "%s : recv failed\n",__FUNCTION__);
			goto Exit; 
		}
		bytesRead++;

		if(*headerPtr == '\r'){			/* Ignore CR */

			/* Basically do nothing special, just don't set newlines
			 *	to 0 */
			headerPtr++;
			continue;
		}
		else if(*headerPtr == '\n')		/* LF is the separator */
			newlines++;
		else
			newlines = 0;

		headerPtr++;
	}
Exit:
	if(newlines == 2){
		headerPtr -= 3;		/* Snip the trailing LF's */
		*headerPtr = '\0';
	} else {
		fprintf(stderr, "%s : no new lines. may be cgi\n",__FUNCTION__);
		pBuff[bytesRead] = 0x00;
	}
	return bytesRead;
}
/**
 * Obtains parent folder for RTSP resource
 */
char *GetParentFolderForRtspResource(char *szResource)
{
	const char *pszDirStart = szResource;
	const char *pszDirEnd = NULL;
	char *pszDir = NULL;
	if(strncmp(pszDirStart,"rtsp://", strlen("rtsp://")) == 0){
		pszDirStart += strlen("rtsp://");
		pszDirStart = strstr(pszDirStart,"/");
		if(pszDirStart)
			pszDirStart += strlen("/");
	}
	if(pszDirStart){
		pszDirEnd = strstr(pszDirStart,"/");
	}
	int len = pszDirEnd - pszDirStart;
	if(len > 0 && len < 1024){
		pszDir = (char *)malloc (len + 1);
		strncpy(pszDir,pszDirStart,len);
		pszDir[len] = 0x00;
	}
	return pszDir;
}

char *GetFileNameForDrbResource(const char *szResource)
{
	const char *pszTmp = szResource;
	char *pszFile = NULL;
	if(strncmp(pszTmp,"drb://", strlen("drb://")) == 0){
		pszTmp += strlen("drb://");
		pszTmp = strstr(pszTmp,"/");
		if(pszTmp){
			pszTmp++;
			const char *pszEnd = pszTmp;
			while(*pszEnd && (*pszEnd != '\r' || *pszEnd != '\n'))
				pszEnd++;
			int nLen = pszEnd - pszTmp;
			if(nLen > 0)
				pszFile = (char *)malloc(nLen + 1);
				memcpy(pszFile, pszTmp, nLen);
				pszFile[nLen] = 0x00;
		}
	}
	return pszFile;
}

char *GetFilepathForRtspResource(const char *pszRootFolder, char *szResource)
{
	const char *pszFile = szResource;
	if(strncmp(pszFile,"rtsp://", strlen("rtsp://")) == 0){
		pszFile += strlen("rtsp://");
		pszFile = strstr(pszFile,"/");
	}
	if(pszFile){
		char *pszPath = (char *)malloc(strlen(pszFile) + strlen(pszRootFolder) + 1);
		/* build the absolute path to the file resource */
		strcpy(pszPath, pszRootFolder);
		strcat(pszPath, pszFile);
		/* Remove trailing CR,LF */
		int lastCharPos = strlen(pszPath) - 1;
		while (lastCharPos > 0){
			if(pszPath[lastCharPos] == '\n' || pszPath[lastCharPos] == '\r' || pszPath[lastCharPos] == 0x20){
				pszPath[lastCharPos] = 0x00;
				lastCharPos--;
			} else
				break;
		}
		return pszPath;
	}
	return NULL;
}
