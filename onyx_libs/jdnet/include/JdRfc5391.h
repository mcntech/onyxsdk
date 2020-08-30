/*
 *  JdRfc5391.h
 *
 *  Copyright 2012 MCN Technologies Inc.. All rights reserved.
 *
 */
#ifndef __JD_RFC_5391_H__
#define __JD_RFC_3984_H__

#include "JdRtp.h"
#include "JdRfcBase.h"

class CJdRfc5391 : public CJdRfcBase
{
public:
	CJdRfc5391();
	CJdRfc5391(int nBuffLen);
	~CJdRfc5391();

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

#endif // __JD_RFC_3984_H__