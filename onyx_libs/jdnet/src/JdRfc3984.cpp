/*
 *  JdRfc3984.cpp
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

#include "JdRfc3984.h"
#include "JdRtp.h"
#include "JdRtpRcv.h"
#include "JdDbg.h"

#ifndef WIN32

#define BYTE unsigned char
#define WORD unsigned short
#define DWORD unsigned long
#define __int64 long long
#define UNALIGNED
#define ULONGLONG	long long
#endif

static int modDbgLevel = CJdDbg::LVL_SETUP;

enum nalUnitType
{
    NAL_UNKNOWN     = 0,
    NAL_SLICE       = 1,
    NAL_SLICE_DPA   = 2,
    NAL_SLICE_DPB   = 3,
    NAL_SLICE_DPC   = 4,
    NAL_SLICE_IDR   = 5,
    NAL_SEI         = 6,
    NAL_SPS         = 7,
    NAL_PPS         = 8,
    NAL_AU_DELIMITER= 9,
    NAL_END_OF_SEQ  = 10            
};


#define DATA_BUFFER_SIZE	2048
#define MAX_MTU_SIZE		1400
void CJdRfc3984Rx::Init(int nBuffLen)
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
	m_TranslateNaluHdr = 1;
}

CJdRfc3984Rx::CJdRfc3984Rx()
{
	Init(DATA_BUFFER_SIZE);
}

CJdRfc3984Rx::CJdRfc3984Rx(int nBuffLen)
{
	Init(nBuffLen);
}

CJdRfc3984Rx::~CJdRfc3984Rx()
{
	if(m_Buffer)
		free(m_Buffer);
}

int CJdRfc3984Rx::GetBuffer(
		char **ppBuffer, 
		long *plMaxLen) 
{
	*ppBuffer = m_Buffer + RTP_HDR_SIZE;
	*plMaxLen = DATA_BUFFER_SIZE;
	return 0;
}


int CJdRfc3984Rx::SendData(
		CRtp *pRtp,
		char *pData,
		long lLen) 
{
	return 0;
}

const char  nalHdr[4] = {0x00, 0x00, 0x00, 0x01}; // nal header

int CJdRfc3984Rx::GetData(
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
			return -1;;
	}
	
	CRtp::ParseHeader(m_Buffer, &m_RtpHdr);
	
	nBytes -= RTP_HDR_SIZE;
/*

   F: 1 bit
      forbidden_zero_bit.  The H.264 specification declares a value of
      1 as a syntax violation.

   NRI: 2 bits
      nal_ref_idc.  A value of 00 indicates that the content of the NAL
      unit is not used to reconstruct reference pictures for inter
      picture prediction.  Such NAL units can be discarded without
      risking the integrity of the reference pictures.  Values greater
      than 00 indicate that the decoding of the NAL unit is required to
      maintain the integrity of the reference pictures.

   Type: 5 bits
      nal_unit_type.  This component specifies the NAL unit payload type
      as defined in table 7-1 of [1], and later within this memo.  For a
      reference of all currently defined NAL unit types and their
      semantics, please refer to section 7.4.1 in [1].

	+---------------+
	|0|1|2|3|4|5|6|7|
	+-+-+-+-+-+-+-+-+
	|F|NRI|  Type   |
	+---------------+
Table 1.  Summary of NAL unit types and their payload structures

      Type   Packet    Type name                        Section
      ---------------------------------------------------------
      0      undefined                                    -
      1-23   NAL unit  Single NAL unit packet per H.264   5.6
      24     STAP-A    Single-time aggregation packet     5.7.1
      25     STAP-B    Single-time aggregation packet     5.7.1
      26     MTAP16    Multi-time aggregation packet      5.7.2
      27     MTAP24    Multi-time aggregation packet      5.7.2
      28     FU-A      Fragmentation unit                 5.8
      29     FU-B      Fragmentation unit                 5.8
      30-31  undefined                                    -

The FU indicator octet has the following format:

      +---------------+
      |0|1|2|3|4|5|6|7|
      +-+-+-+-+-+-+-+-+
      |F|NRI|  Type   |
      +---------------+

   Values equal to 28 and 29 in the Type field of the FU indicator octet
   identify an FU-A and an FU-B, respectively.  The use of the F bit is
   described in section 5.3.  The value of the NRI field MUST be set
   according to the value of the NRI field in the fragmented NAL unit.

   The FU header has the following format:

      +---------------+
      |0|1|2|3|4|5|6|7|
      +-+-+-+-+-+-+-+-+
      |S|E|R|  Type   |
      +---------------+

*/
	if(m_TranslateNaluHdr) {
		char *pRtpData = m_Buffer + RTP_HDR_SIZE;	// TODO: Handle header extension
		int nFUIndicatorType = pRtpData[0] & 0x1f;
		int nNRI = pRtpData[0] & 0x60;
        if (nFUIndicatorType >= NAL_SLICE && nFUIndicatorType <= NAL_END_OF_SEQ) {
			nOutBytes = 4;
			memcpy(pData, nalHdr, 4);
			memcpy (pData + nOutBytes, pRtpData, nBytes );
			nOutBytes += nBytes;
        } else {
			int fNaluStart = pRtpData[1] & 0x80;
			int fNaluEnd = pRtpData[1] & 0x40;
			int nNaluType = pRtpData[1] & 0x1f;
			/* fragmented Frame */
			if(fNaluStart) {
				unsigned char tempCh = nNRI  | nNaluType;

				nOutBytes = 4;
				memcpy(pData, nalHdr, 4);

				nBytes -= 2;
				memcpy (pData + nOutBytes, &tempCh, 1);
				nOutBytes += 1;
				memcpy (pData + nOutBytes, pRtpData + 2, nBytes );
				nOutBytes += nBytes;
			} else if (fNaluEnd){
				nBytes -= 2;
				memcpy (pData, pRtpData + 2, nBytes);
				nOutBytes += nBytes;
			} else /* Middle Packet */{
				nBytes -= 2;
				memcpy (pData, pRtpData + 2, nBytes);
				nOutBytes += nBytes;
			}
		}
	} else {
		memcpy(pData, m_Buffer + RTP_HDR_SIZE, nBytes);
		nOutBytes = nBytes;
	}
	return nOutBytes;
}

CJdRfc3984Tx::CJdRfc3984Tx(CRtpSnd *pRtpSnd, unsigned char ucPlType, unsigned long ulSsrc)
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
	mWaitingForSps = true;
}
/** 
 * Accepts AVC1 packets (no start code) and transmits 
 * and packetizes in to rfc3984 compliant packets
 */
int CJdRfc3984Tx::Deliver(char *pData, int nNaluSize, long long llPts)
{
	long lSrcOffset = 0;
	char *pRtpData = NULL;
	long nHdr3984Len = 0;
	bool fFragmented;
	
	JDBG_LOG(CJdDbg::LVL_STRM,("Enter"));

	unsigned char nalTypeAndIdc = pData[0];
	if((nalTypeAndIdc & 0x1F) == 9){
		return 0;  // Skip Access units
	}

	if(mWaitingForSps){
		if((nalTypeAndIdc & 0x1F) == NAL_SPS){
			mWaitingForSps = false;
		} else {
			JDBG_LOG(CJdDbg::LVL_STRM,("Waiting For Sps"));
			return 0; // Skip until we find key frame
		}
	}

	m_RtpHdr.ulTimeStamp = (unsigned long)llPts;

	if(nNaluSize > MAX_MTU_SIZE - RTP_HDR_SIZE) {
		/* Fragmented */
		unsigned char Fu[2] = {0};
		
		Fu[0] = (nalTypeAndIdc & 0x60)/*nal_ref_idc*/ | 0x1C /*Fragmented*/;
		while(lSrcOffset < nNaluSize) {

			long lPlLen = MAX_MTU_SIZE - RTP_HDR_SIZE - 2;
			m_RtpHdr.m = 0;

			if(lPlLen > (nNaluSize - lSrcOffset)){
				lPlLen = (nNaluSize - lSrcOffset);
				m_RtpHdr.m = 1;
			}
			m_RtpHdr.usSeqNum++;
			mRtp->CreateHeader(m_Buffer, &m_RtpHdr);

			if(lSrcOffset == 0){
				// Nalu start packet
				Fu[1] = 0x80  | (nalTypeAndIdc & 0x1f);
				lSrcOffset++; // Skip NAL Type
			} else if(lPlLen == (nNaluSize - lSrcOffset)){
				// Nalu end packet
				Fu[1] = 0x40 | (nalTypeAndIdc & 0x1f);

			} else {
				// Nalu middle packet
				Fu[1] = nalTypeAndIdc & 0x1f;
			}
			memcpy(m_Buffer + RTP_HDR_SIZE, Fu, 2);
			memcpy(m_Buffer + RTP_HDR_SIZE + 2, pData + lSrcOffset, lPlLen);
			mRtp->Write(m_Buffer, lPlLen + RTP_HDR_SIZE + 2);
			lSrcOffset += lPlLen;
		}
	} else {
		if((nalTypeAndIdc & 0x1F) == NAL_SPS || (nalTypeAndIdc & 0x1F) == NAL_PPS ||(nalTypeAndIdc & 0x1F) ==  NAL_AU_DELIMITER)
			m_RtpHdr.m = 0;
		else
			m_RtpHdr.m = 1;
		m_RtpHdr.usSeqNum++;
		mRtp->CreateHeader(m_Buffer, &m_RtpHdr);
		memcpy(m_Buffer + RTP_HDR_SIZE, pData, nNaluSize);
		mRtp->Write(m_Buffer, nNaluSize + RTP_HDR_SIZE);
	}
	JDBG_LOG(CJdDbg::LVL_STRM,("Leave"));
	return 0;
}