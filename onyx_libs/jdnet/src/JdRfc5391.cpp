/*
 *  JdRfc5391.cpp
 *
 *  Copyright 2010 MCN Technologies Inc.. All rights reserved.
 *
 */

#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#include <ws2tcpip.h>
#define close		closesocket
#define snprintf	_snprintf
#define write		_write
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
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "JdRfc5391.h"
#include "JdRtp.h"
#include "JdRtpRcv.h"

#define DATA_BUFFER_SIZE	2048
void CJdRfc5391::Init(int nBuffLen)
{
	m_Buffer = (char *)malloc(nBuffLen + RTP_HDR_SIZE );
	m_RtpHdr.cc = 0;
	m_RtpHdr.x = 0;
	m_RtpHdr.v = 2;
	m_RtpHdr.m = 1;
	m_RtpHdr.p = 0;
	m_RtpHdr.pt = 33;
	m_RtpHdr.ulSsrc = (unsigned long)rand() << 16 | rand();;
	m_RtpHdr.ulTimeStamp = 0;
	m_RtpHdr.usSeqNum = rand();;
	m_nMaxBuffLen = nBuffLen + RTP_HDR_SIZE ;
	m_ulTimeStamp = 0;
}

CJdRfc5391::CJdRfc5391()
{
	Init(DATA_BUFFER_SIZE);
}

CJdRfc5391::CJdRfc5391(int nBuffLen)
{
	Init(nBuffLen);
}

CJdRfc5391::~CJdRfc5391()
{
	if(m_Buffer)
		free(m_Buffer);
}

int CJdRfc5391::GetBuffer(
		char **ppBuffer, 
		long *plMaxLen) 
{
	*ppBuffer = m_Buffer + RTP_HDR_SIZE;
	*plMaxLen = DATA_BUFFER_SIZE;
	return 0;
}
int CJdRfc5391::SendData(
		CRtp *pRtp,
		char *pData,
		long lLen) 
{
	// To be implemented
	return 0;
}

int CJdRfc5391::GetData(
		CRtp *pRtp,
		char *pData,
		long lLen) 
{
	int nBytesToRead = m_nMaxBuffLen;
	int nOutBytes = 0;
	if(lLen < m_nMaxBuffLen - RTP_HDR_SIZE)
		nBytesToRead = lLen + RTP_HDR_SIZE;
	
	int nBytes = pRtp->Read(m_Buffer, nBytesToRead);
	if (nBytes < RTP_HDR_SIZE ) {
			fprintf(stderr,"Failed to read to socket.\n");
			return 0;
	}
	
	CRtp::ParseHeader(m_Buffer, &m_RtpHdr);
	nOutBytes = nBytes - RTP_HDR_SIZE;
	memcpy(pData, m_Buffer + RTP_HDR_SIZE, nOutBytes);

	return nOutBytes;
}
