/*
 *  JdRfc3640.cpp
 *
 *  Copyright 2014 MCN Technologies Inc.. All rights reserved.
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

#include "JdRfc3640.h"
#include "JdRtp.h"
#include "JdRtpRcv.h"
#include "JdDbg.h"

#define DATA_BUFFER_SIZE	2048
#define MAX_MTU_SIZE		1400

static int modDbgLevel = CJdDbg::LVL_SETUP;

void CJdRfc3640Rx::Init(int nBuffLen)
{
	m_Buffer = (char *)malloc(nBuffLen + RTP_HDR_SIZE );
	m_RtpHdr.cc = 0;
	m_RtpHdr.x = 0;
	m_RtpHdr.v = 2;
	m_RtpHdr.m = 1;
	m_RtpHdr.p = 0;
	m_RtpHdr.pt = 98;
	m_RtpHdr.ulSsrc = (unsigned long)rand() << 16 | rand();;
	m_RtpHdr.ulTimeStamp = 0;
	m_RtpHdr.usSeqNum = rand();;
	m_nMaxBuffLen = nBuffLen + RTP_HDR_SIZE ;
	m_ulTimeStamp = 0;
}

CJdRfc3640Rx::CJdRfc3640Rx()
{
	Init(DATA_BUFFER_SIZE);
}

CJdRfc3640Rx::CJdRfc3640Rx(int nBuffLen)
{
	Init(nBuffLen);
}

CJdRfc3640Rx::~CJdRfc3640Rx()
{
	if(m_Buffer)
		free(m_Buffer);
}

int CJdRfc3640Rx::GetBuffer(
		char **ppBuffer, 
		long *plMaxLen) 
{
	*ppBuffer = m_Buffer + RTP_HDR_SIZE;
	*plMaxLen = DATA_BUFFER_SIZE;
	return 0;
}
int CJdRfc3640Rx::SendData(
		CRtp *pRtp,
		char *pData,
		long lLen) 
{

	return 0;
}

int CJdRfc3640Rx::GetData(
		CRtp *pRtp,
		char *pData,
		long lLen) 
{
	return 0;
}



CJdRfc3640Tx::CJdRfc3640Tx(CRtpSnd *pRtpSnd, unsigned char ucPlType, unsigned long ulSsrc)
{
	mMaxBuffLen = MAX_MTU_SIZE;
	m_Buffer = (char *)malloc(mMaxBuffLen);
	mRtp = pRtpSnd;

	m_RtpHdr.pt = ucPlType;
	
	if(ulSsrc == 0)
		ulSsrc = (unsigned long)rand() << 16 | rand();
	
	m_RtpHdr.cc = 0;
	m_RtpHdr.x = 0;
	m_RtpHdr.v = 2;
	m_RtpHdr.m = 1;
	m_RtpHdr.p = 0;
	m_RtpHdr.ulSsrc = ulSsrc;
	m_RtpHdr.ulTimeStamp = 0;
	m_RtpHdr.usSeqNum = rand();
}

static unsigned short getAdtsFrameLen(unsigned char *pAdts)
{
	unsigned short usLen = (((unsigned short)pAdts[3] << 11) & 0x1800) |
							(((unsigned short)pAdts[4] << 3) & 0x07F8) |
							(((unsigned short)pAdts[5] >> 5) & 0x0007);
	return usLen;
}

/** 
 * Accepts AVC1 packets (no start code) and transmits 
 * and packetizes in to rfc3984 compliant packets
 */
int CJdRfc3640Tx::Deliver(char *pData, int nAuSize, long long llPts)
{
	long lSrcOffset = 0;
	int nFrameSize = 0;

	
	JDBG_LOG(CJdDbg::LVL_STRM,("Enter"));

	m_RtpHdr.ulTimeStamp = (unsigned long)llPts;

	nFrameSize = getAdtsFrameLen((unsigned char *)pData);
	int nAdtsHdrSize = 7 + (( pData[1] & 0x01 ) ? 0 : 2 );
	char *pRawData = pData + nAdtsHdrSize;
	int nRawDataSize = nFrameSize - nAdtsHdrSize;

	if(nRawDataSize > MAX_MTU_SIZE - RTP_HDR_SIZE) {
		
		while(lSrcOffset < nRawDataSize) {

			long lPlLen = MAX_MTU_SIZE - RTP_HDR_SIZE;
			m_RtpHdr.m = 0;

			if(lPlLen > (nRawDataSize - lSrcOffset)){
				lPlLen = (nRawDataSize - lSrcOffset);
				m_RtpHdr.m = 1;
			}
			m_RtpHdr.usSeqNum++;
			mRtp->CreateHeader(m_Buffer, &m_RtpHdr);

			memcpy(m_Buffer + RTP_HDR_SIZE, pRawData + lSrcOffset, lPlLen);
			mRtp->Write(m_Buffer, lPlLen + RTP_HDR_SIZE);
			lSrcOffset += lPlLen;
		}
	} else {
		m_RtpHdr.m = 1;
		m_RtpHdr.usSeqNum++;
		mRtp->CreateHeader(m_Buffer, &m_RtpHdr);
		memcpy(m_Buffer + RTP_HDR_SIZE, pRawData, nRawDataSize);
		mRtp->Write(m_Buffer, nRawDataSize + RTP_HDR_SIZE);
	}
	JDBG_LOG(CJdDbg::LVL_STRM,("Leave"));
	return 0;
}