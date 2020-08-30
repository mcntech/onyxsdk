#ifndef __DTCP_RCV_H__
#define __DTCP_RCV_H__

#include "JdRtpRcv.h"

class CDtcpRcv : public CRtpRcv
{
public:
	int Configure();
	virtual int Read(char *pData, int nMaxLen);
public:
	unsigned char m_Key[16];
};

#endif //__DTCP_RCV_H__