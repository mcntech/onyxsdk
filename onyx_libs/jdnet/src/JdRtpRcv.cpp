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
#include "JdRtpRcv.h"

#define DEFAULT_READ_TIMEOUT	5		/* Seconds to wait before giving up


#define CHECK_ALLOC(x) if(x==NULL) {fprintf(stderr,"malloc failed at %s %d, exiting..\n", __FUNCTION__, __LINE__); goto Exit;}

/* Globals */
static int timeout = DEFAULT_READ_TIMEOUT;


int CRtpRcv::Read(char *pData, int nMaxLen)
{
	fd_set rfds;
	struct timeval tv;
	int ret = -1;
	int	selectRet;
	/* Begin reading the body of the file */

	FD_ZERO(&rfds);
	FD_SET(m_hRtpSock, &rfds);

	tv.tv_sec = timeout; 
	tv.tv_usec = 0;

	if(timeout >= 0)
		selectRet = select(m_hRtpSock+1, &rfds, NULL, NULL, &tv);
	else		/* No timeout, can block indefinately */
		selectRet = select(m_hRtpSock+1, &rfds, NULL, NULL, NULL);

	if(selectRet == 0) {
		return -1;
	} else if(selectRet == -1)	{
		return -1;
	}

	ret = recv(m_hRtpSock, pData, nMaxLen, 0);

	return ret;
}


void CRtpRcv::ParseHeader(char *pData, RTP_PKT_T *pHdr)
{
	pHdr->v = (pData[0] & 0xC0 ) >> 6;
	pHdr->p = (pData[0] & 0x20 ) >> 5;
	pHdr->x = (pData[0] & 0x10 ) >> 4;
	pHdr->cc = (pData[0] & 0x0F );
	pHdr->m = (pData[1] & 0x80 ) >> 7;
	pHdr->pt = (pData[1] & 0x7F );
	pHdr->usSeqNum = ((unsigned short)pData[2] << 8) | ((unsigned short)pData[3] );
	pHdr->ulTimeStamp = ((unsigned long)pData[4] << 24) | ((unsigned long)pData[5] << 16) | ((unsigned long)pData[6] << 8) | ((unsigned long)pData[7]);
	pHdr->ulSsrc= ((unsigned long)pData[8] << 24) | ((unsigned long)pData[9] << 16) | ((unsigned long)pData[10] << 8) | ((unsigned long)pData[11]);
}

