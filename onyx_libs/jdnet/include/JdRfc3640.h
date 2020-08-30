/*
 *  JdRfc5391.h
 *
 *  Copyright 2012 MCN Technologies Inc.. All rights reserved.
 *
 */
#ifndef __JD_RFC_3640_H__
#define __JD_RFC_3640_H__

#include "JdRtp.h"
#include "JdRfcBase.h"
#include "JdMediaResMgr.h"
#include "JdRtpSnd.h"

class CJdRfc3640Tx : public IMediaDelivery
{
public:
	CJdRfc3640Tx(CRtpSnd *pRtpSnd, unsigned char ucPlType, unsigned long ulSsrc);

	~CJdRfc3640Tx(){}

	int Deliver(char *pData, int nLen, long long llPts);
	
public:
	char          *m_Buffer;
	RTP_PKT_T     m_RtpHdr;
	int           mMaxBuffLen;
	CRtp         *mRtp;
};

class CJdRfc3640Rx : public CJdRfcBase
{
public:
	CJdRfc3640Rx();
	CJdRfc3640Rx(int nBuffLen);
	~CJdRfc3640Rx();

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

private:
	virtual void Init(int nBuffLen);

public:
	char *m_Buffer;
	RTP_PKT_T m_RtpHdr;
	int m_nMaxBuffLen;
	unsigned long m_ulTimeStamp;
};

#endif // __JD_RFC_3640_H__