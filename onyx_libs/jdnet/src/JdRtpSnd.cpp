/*
 *  JdRtpSnd.cpp
 *
 *  Copyright 2010 MCN Technologies Inc.. All rights reserved.
 *
 */

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
#include <arpa/inet.h>

#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "JdRtp.h"
#include "JdRtpSnd.h"
#include "JdDbg.h"

#define DEFAULT_READ_TIMEOUT	30		
static int modDbgLevel = CJdDbg::LVL_ERR;

/* Globals */
static int timeout = DEFAULT_READ_TIMEOUT;


int CRtpSnd::Write(char *pData, int lLen)
{
	fd_set rfds;
	struct timeval tv;
	int ret = -1;
	int	selectRet;
	int bytesWritten= 0;
	/* Begin reading the body of the file */

	struct sockaddr_in peerAddr;
	peerAddr.sin_addr = m_SockAddr;
	peerAddr.sin_port = htons(mRemoteRtpPort);
	peerAddr.sin_family = PF_INET;
	
	JDBG_LOG(CJdDbg::LVL_TRACE,("sendto addr=0x%x port=%d", *((unsigned long *)&m_SockAddr), mRemoteRtpPort));
	ret = sendto(m_hRtpSock, pData, lLen, 0, (struct sockaddr *)&peerAddr, sizeof(struct sockaddr));
	if(ret == -1)	{
		return -1;
	}
	return lLen;
}


void CRtpSnd::CreateHeader(char *pData, RTP_PKT_T *pHdr)
{
	pData[0] = ((pHdr->v << 6) & 0xC0) | ((pHdr->p << 5) & 0x20) | ((pHdr->x << 4) & 0x10) | ( pHdr->cc & 0x0F);
	pData[1] = ((pHdr->m << 7) & 0x80) | (pHdr->pt & 0x7F);
	pData[2] = (unsigned char)(pHdr->usSeqNum >> 8); pData[3] = (unsigned char)(pHdr->usSeqNum);
	pData[4] = (unsigned char)(pHdr->ulTimeStamp >> 24); pData[5] = (unsigned char)(pHdr->ulTimeStamp >> 16);
	pData[6] = (unsigned char)(pHdr->ulTimeStamp >> 8); pData[7] = (unsigned char)pHdr->ulTimeStamp;
	pData[8] = (unsigned char)(pHdr->ulSsrc >> 24); pData[9] = (unsigned char)(pHdr->ulSsrc >> 16);
	pData[10] = (unsigned char)(pHdr->ulSsrc >> 8); pData[11] = (unsigned char)pHdr->ulSsrc;

}
