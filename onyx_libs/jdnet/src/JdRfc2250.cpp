/*
 *  JdRfc2250.cpp
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

#include "JdRfc2250.h"
#include "JdRtp.h"
#include "JdRtpRcv.h"

#ifndef WIN32

#define BYTE unsigned char
#define WORD unsigned short
#define DWORD unsigned long
#define __int64 long long
#define UNALIGNED
#define ULONGLONG	long long
#endif

#ifndef BIG_ENDIAN
#define NTOH_LL(ll)                                             \
                    ( (((ll) & 0xFF00000000000000) >> 56)  |    \
                      (((ll) & 0x00FF000000000000) >> 40)  |    \
                      (((ll) & 0x0000FF0000000000) >> 24)  |    \
                      (((ll) & 0x000000FF00000000) >> 8)   |    \
                      (((ll) & 0x00000000FF000000) << 8)   |    \
                      (((ll) & 0x0000000000FF0000) << 24)  |    \
                      (((ll) & 0x000000000000FF00) << 40)  |    \
                      (((ll) & 0x00000000000000FF) << 56) )

#define NTOH_L(l)                                       \
                    ( (((l) & 0xFF000000) >> 24)    |   \
                      (((l) & 0x00FF0000) >> 8)     |   \
                      (((l) & 0x0000FF00) << 8)     |   \
                      (((l) & 0x000000FF) << 24) )

#define NTOH_S(s)                                   \
                    ( (((s) & 0xFF00) >> 8)     |   \
                      (((s) & 0x00FF) << 8) )

#else   //  BIG_ENDIAN
//  we're already big-endian, so we do nothing
#define NTOH_LL(ll)     (ll)
#define NTOH_L(l)       (l)
#define NTOH_S(s)       (s)

#endif  //  BIG_ENDIAN

#define HTON_L(l)       NTOH_L(l)
#define HTON_S(s)       NTOH_S(s)

//  i 0-based
#define BIT_VALUE(v,b)              ((v & (0x00 | (1 << b))) >> b)
#define BYTE_OFFSET(pb,i)           (& BYTE_VALUE((pb),i))
#define BYTE_VALUE(pb,i)            (((BYTE *) (pb))[i])
#define WORD_VALUE(pb,i)            (* (UNALIGNED WORD *) BYTE_OFFSET((pb),i))
#define DWORD_VALUE(pb,i)           (* (UNALIGNED DWORD *) BYTE_OFFSET((pb),i))
#define ULONGLONG_VALUE(pb,i)       (* (UNALIGNED ULONGLONG *) BYTE_OFFSET((pb),i))

#define PID_VALUE(pb)				(NTOH_S(WORD_VALUE(pb,1)) & 0x00001fff)

#define SYNC_BYTE_VALUE(pb)                             (BYTE_VALUE((pb),0))
#define TRANSPORT_ERROR_INDICATOR_BIT(pb)               ((NTOH_S(WORD_VALUE(pb,1)) & 0x00008000) >> 15)
#define PAYLOAD_UNIT_START_INDICATOR_BIT(pb)            ((NTOH_S(WORD_VALUE(pb,1)) & 0x00004000) >> 14)
#define TRANSPORT_PRIORITY_BIT(pb)                      ((NTOH_S(WORD_VALUE(pb,1)) & 0x00002000) >> 13)
#define PID_VALUE(pb)                                   (NTOH_S(WORD_VALUE(pb,1)) & 0x00001fff)
#define TRANSPORT_SCRAMBLING_CONTROL_VALUE(pb)          ((BYTE_VALUE((pb),3) & 0x000000c0) >> 6)
#define ADAPTATION_FIELD_CONTROL_VALUE(pb)              ((BYTE_VALUE((pb),3) & 0x00000030) >> 4)
#define CONTINUITY_COUNTER_VALUE(pb)                    (BYTE_VALUE((pb),3) & 0x0000000f)


#define ADAPTATION_FIELD_LENGTH_VALUE(pb)               (BYTE_VALUE((pb),0))
#define DISCONTINUITY_INDICATOR_BIT(pb)                 ((BYTE_VALUE((pb),1) & 0x00000080) >> 7)
#define RANDOM_ACCESS_INDICATOR_BIT(pb)                 ((BYTE_VALUE((pb),1) & 0x00000040) >> 6)
#define ELEMENTARY_STREAM_PRIORITY_INDICATOR_BIT(pb)    ((BYTE_VALUE((pb),1) & 0x00000020) >> 5)
#define PCR_FLAG_BIT(pb)                                ((BYTE_VALUE((pb),1) & 0x00000010) >> 4)
#define OPCR_FLAG_BIT(pb)                               ((BYTE_VALUE((pb),1) & 0x00000008) >> 3)
#define SPLICING_POINT_FLAG_BIT(pb)                     ((BYTE_VALUE((pb),1) & 0x00000004) >> 2)
#define TRANSPORT_PRIVATE_DATA_FLAG_BIT(pb)             ((BYTE_VALUE((pb),1) & 0x00000002) >> 1)
#define ADAPTATION_FIELD_EXTENSION_FLAG_BIT(pb)         (BYTE_VALUE((pb),1) & 0x00000001)

//  pb points to PCR_BASE block
#define PCR_BASE_VALUE(pb)                              (NTOH_LL(ULONGLONG_VALUE(pb,0)) >> 31)
#define PCR_EXT_VALUE(pb)                               ((DWORD) (NTOH_LL(ULONGLONG_VALUE(pb,0)) & 0x00000000000001ff))

//  pb points to OPCR_BASE block
#define OPCR_BASE_VALUE(pb)                             PCR_BASE_VALUE(pb)
#define OPCR_EXT_VALUE(pb)                              PCR_EXT_VALUE(pb)

//  pb points to splice_countdown block
#define SPLICE_COUNTDOWN_VALUE(pb)                      BYTE_VALUE(pb,0)

//  pb points to transport_private_data_length
#define TRANSPORT_PRIVATE_DATA_LENGTH_VALUE(pb)         BYTE_VALUE(pb,0)

//  pb points to adaptation_field_extension_length
#define ADAPTATION_FIELD_EXTENSION_LENGTH_VALUE(pb)     BYTE_VALUE(pb,0)

//  increments and takes care of the wrapping case
#define INCREMENTED_CONTINUITY_COUNTER_VALUE(c) ((BYTE) (((c) + 1) & 0x0000000f))

#define TS_PKT_LEN	188

#define MAX_PID_COUNT	10

class CPidMap
{
public:
	CPidMap()
	{
		m_lCount = 0;
		m_lCc = 0;
	}
	CPidMap(long lPid)
	{
		m_lCount = 0;
		m_lCc = 0;
		m_PidMap[m_lCount++] = lPid;
	}

	void Add(long lPid)
	{
		if(m_lCount < MAX_PID_COUNT)
			m_PidMap[m_lCount++] = lPid;
	}
	int IsExist(long lPid)
	{
		for (int i=0; i < m_lCount; i++){
			if(m_PidMap[i] == lPid)
				return 1;
		}
		return 0;
	}
	int CheckAndUpdateCc(unsigned char *pTsPkt)
	{
		long lCc = CONTINUITY_COUNTER_VALUE(pTsPkt);
		if(INCREMENTED_CONTINUITY_COUNTER_VALUE(m_lCc) == lCc){
			m_lCc = lCc;
			return 1;
		}
		return 0;
	}
private:	
	long	m_lCount;
	long	m_PidMap[MAX_PID_COUNT];
	long	m_lCc;
};

CPidMap m_PcrPidMap;

unsigned long CJdRfc2250::GetPcr(unsigned char* pTsPkt, long lLen)
{
	long long llPcr = 0;
	unsigned long ulPcr = 0;

	while (lLen > 0) {
		long lPid = PID_VALUE(pTsPkt);
		int pos = 0;
		
		/* Check Adaptation field present and move the pos, bt adaptation field lenght, if required*/
		if ((0x20 & pTsPkt[pos+3]) == 0x20) {
			pos += pTsPkt[pos+4] + 1;
		
			__int64 llPcrBase = 0;
			DWORD lPcrExt = 0;
			BYTE *pAdapt = pTsPkt + 4;
			if((ADAPTATION_FIELD_CONTROL_VALUE(pTsPkt) == 0x02)&& PCR_FLAG_BIT(pAdapt)){
				llPcrBase = PCR_BASE_VALUE(pAdapt+2);
				lPcrExt = PCR_EXT_VALUE(pAdapt+2);
				llPcr = (llPcrBase << 33) | lPcrExt;
				ulPcr = llPcrBase;
				break;
			}
		}
		lLen -= TS_PKT_LEN;
		pTsPkt += TS_PKT_LEN;
	}
	return ulPcr;
}

void CJdRfc2250::Init(int nBuffLen)
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

CJdRfc2250::CJdRfc2250()
{
	Init(DATA_BUFFER_SIZE);
}

CJdRfc2250::CJdRfc2250(int nBuffLen)
{
	Init(nBuffLen);
}

CJdRfc2250::~CJdRfc2250()
{
	if(m_Buffer)
		free(m_Buffer);
}

int CJdRfc2250::GetBuffer(
		char **ppBuffer, 
		long *plMaxLen) 
{
	*ppBuffer = m_Buffer + RTP_HDR_SIZE;
	*plMaxLen = DATA_BUFFER_SIZE;
	return 0;
}
int CJdRfc2250::SendData(
		CRtp *pRtp,
		char *pData,
		long lLen) 
{
	char *pBuffer = pData - RTP_HDR_SIZE;
	m_ulTimeStamp = GetPcr((unsigned char *)pData, lLen);
	if(m_ulTimeStamp != 0)
		m_RtpHdr.ulTimeStamp = m_ulTimeStamp;

	m_RtpHdr.usSeqNum++;
	if(m_RtpHdr.usSeqNum == 0)
		m_RtpHdr.ulSsrc++;
	
	CRtp::CreateHeader(pBuffer, &m_RtpHdr);

	if (pRtp->Write(pBuffer, lLen + RTP_HDR_SIZE) < 0 ) {
			fprintf(stderr,"Failed to write to socket.\n");
			return -1;;
	}
	return lLen;
}

int CJdRfc2250::GetData(
		CRtp *pRtp,
		char *pData,
		long lLen) 
{
	int nBytesToRead = m_nMaxBuffLen;
	if(lLen < m_nMaxBuffLen - RTP_HDR_SIZE)
		nBytesToRead = lLen + RTP_HDR_SIZE;
	int nBytes = pRtp->Read(m_Buffer, nBytesToRead);
	if (nBytes < RTP_HDR_SIZE ) {
			fprintf(stderr,"Failed to read to socket.\n");
			return -1;;
	}
	CRtp::ParseHeader(m_Buffer, &m_RtpHdr);
	memcpy(pData, m_Buffer + RTP_HDR_SIZE, nBytes - RTP_HDR_SIZE);
	return (nBytes - RTP_HDR_SIZE);
}
#define DATA_BUFFER_SIZE	2048
#define MAX_MTU_SIZE		1400

CJdRfc2250Tx::CJdRfc2250Tx(CRtpSnd *pRtpSnd, unsigned char ucPlType, unsigned long ulSsrc):
	mJdDbg("CJdRfc2250Tx", CJdDbg::LVL_MSG)
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
	m_BytesSent = 0;
	m_DbgByteInterval = 0;
}

unsigned long CJdRfc2250Tx::GetPcr(unsigned char* pTsPkt, long lLen)
{
	long long llPcr = 0;
	unsigned long ulPcr = 0;

	while (lLen > 0) {
		long lPid = PID_VALUE(pTsPkt);
		int pos = 0;
		
		/* Check Adaptation field present and move the pos, bt adaptation field lenght, if required*/
		if ((0x20 & pTsPkt[pos+3]) == 0x20) {
			pos += pTsPkt[pos+4] + 1;
		
			__int64 llPcrBase = 0;
			DWORD lPcrExt = 0;
			BYTE *pAdapt = pTsPkt + 4;
			if((ADAPTATION_FIELD_CONTROL_VALUE(pTsPkt) == 0x02)&& PCR_FLAG_BIT(pAdapt)){
				llPcrBase = PCR_BASE_VALUE(pAdapt+2);
				lPcrExt = PCR_EXT_VALUE(pAdapt+2);
				llPcr = (llPcrBase << 33) | lPcrExt;
				ulPcr = llPcrBase;
				break;
			}
		}
		lLen -= TS_PKT_LEN;
		pTsPkt += TS_PKT_LEN;
	}
	return ulPcr;
}

int CJdRfc2250Tx::Deliver(char *pData, int lLen, long long llPts)
{
	char *pSrc = pData;
	int bytesPerPkt = ((MAX_MTU_SIZE - RTP_HDR_SIZE) / 188) * 188;

	if(m_DbgByteInterval > 1000000) {
		m_BytesSent += m_DbgByteInterval;
		m_DbgByteInterval = 0;
		JdDbg(CJdDbg::LVL_ERR,("BytesSent=%d\n", m_BytesSent));
	}
	m_DbgByteInterval += lLen;

	while(lLen > 0) {
		m_ulTimeStamp = GetPcr((unsigned char *)pData, lLen);
		if(m_ulTimeStamp != 0)
			m_RtpHdr.ulTimeStamp = m_ulTimeStamp;

		m_RtpHdr.usSeqNum++;
		if(m_RtpHdr.usSeqNum == 0)
			m_RtpHdr.ulSsrc++;
		
		CRtp::CreateHeader(m_Buffer, &m_RtpHdr);

		if(bytesPerPkt > lLen)
			bytesPerPkt = lLen;
		memcpy(m_Buffer + RTP_HDR_SIZE, pSrc, bytesPerPkt);
		if (mRtp->Write(m_Buffer, bytesPerPkt + RTP_HDR_SIZE) < 0 ) {
			JdDbg(CJdDbg::LVL_ERR,("Failed to write to socket.\n"));
			return -1;;
		}
		pSrc += bytesPerPkt;
		lLen -= bytesPerPkt;
	}
	return lLen;
}