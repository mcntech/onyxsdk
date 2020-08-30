/*
 *  JdRfc2250.h
 *
 *  Copyright 2010 MCN Technologies Inc.. All rights reserved.
 *
 */
#ifndef __JD_RFC_H__
#define __JD_RFC_H__

#include "JdRtp.h"
#include "JdRtpSnd.h"
#include "JdRfcBase.h"
#include "JdMediaResMgr.h"
#include "JdDbg.h"

#define DATA_BUFFER_SIZE	(188 * 6)

class CJdRfc2250 : public CJdRfcBase
{
public:
	CJdRfc2250();
	CJdRfc2250(int nBuffLen);
	~CJdRfc2250();

	int GetBuffer(
		char **ppBuffer, 
		long *plMaxLen);

	int SendData(
		CRtp *pRtp,
		char *pData,
		long lLen);

	int GetData(
		CRtp *pRtp,
		char *pData,
		long lLen);
	RTP_PKT_T *GetRtnHdr() {return &m_RtpHdr;}

	static unsigned long GetPcr(unsigned char* pTsPkt, long lLen);
private:
	virtual void Init(int nBuffLen);

public:
	char *m_Buffer;
	RTP_PKT_T m_RtpHdr;
	int m_nMaxBuffLen;
	unsigned long m_ulTimeStamp;
};

class CJdRfc2250Tx : public IMediaDelivery
{
public:
	CJdRfc2250Tx(CRtpSnd *pRtpSnd, unsigned char ucPlType, unsigned long ulSsrc);
	~CJdRfc2250Tx(){}

	int Deliver(char *pData, int nLen, long long llPts);
	unsigned long GetPcr(unsigned char* pTsPkt, long lLen);
public:
	CRtp          *mRtp;
	char          *m_Buffer;
	RTP_PKT_T     m_RtpHdr;
	int           m_nMaxBuffLen;
	unsigned long m_ulTimeStamp;
	int           mMaxBuffLen;
	CJdDbg        mJdDbg;
	unsigned long m_DbgByteInterval;
	unsigned long m_BytesSent;
};
#endif // __JD_RFC_H__