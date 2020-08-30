/*
 *  JdRfcBase.h
 *
 *  Copyright 2012 MCN Technologies Inc.. All rights reserved.
 *
 */
#ifndef __JD_RFC_BASE_H__
#define __JD_RFC_BASE_H__

#include "JdRtp.h"

#define RTP_HDR_SIZE		12


class CJdRfcBase
{
public:
	CJdRfcBase(){};
	CJdRfcBase(int nBuffLen){};
	virtual ~CJdRfcBase(){};

	virtual int GetBuffer(
		char **ppBuffer, 
		long *plMaxLen) = 0;

	virtual int SendData(
		CRtp *pRtp,
		char *pData,
		long lLen) = 0;

	virtual int GetData(
		CRtp *pRtp,
		char *pData,
		long lLen) = 0;
	
	virtual RTP_PKT_T *GetRtnHdr() = 0;

};

#endif // __JD_RFC_BASE_H__