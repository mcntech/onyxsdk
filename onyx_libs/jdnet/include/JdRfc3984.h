/*
 *  JdRfc2250.h
 *
 *  Copyright 2010 MCN Technologies Inc.. All rights reserved.
 *
 */
#ifndef __JD_RFC_3984_H__
#define __JD_RFC_3984_H__

#include "JdRtp.h"
#include "JdRtpSnd.h"
#include "JdRfcBase.h"
#include "JdMediaResMgr.h"

class CJdRfc3984Tx : public IMediaDelivery
{
public:
	CJdRfc3984Tx(CRtpSnd *pRtpSnd, unsigned char ucPlType, unsigned long ulSsrc);

	~CJdRfc3984Tx(){}

	int Deliver(char *pData, int nLen, long long llPts);
	
public:
	char          *m_Buffer;
	RTP_PKT_T     m_RtpHdr;
	int           mMaxBuffLen;
	CRtp         *mRtp;
	bool         mWaitingForSps;
};

class CJdRfc3984Rx : public CJdRfcBase
{
public:
	CJdRfc3984Rx();
	CJdRfc3984Rx(int nBuffLen);
	~CJdRfc3984Rx();

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
	int     m_TranslateNaluHdr;
public:
	char          *m_Buffer;
	RTP_PKT_T     m_RtpHdr;
	int           m_nMaxBuffLen;
	unsigned long m_ulTimeStamp;
};

#endif // __JD_RFC_3984_H__