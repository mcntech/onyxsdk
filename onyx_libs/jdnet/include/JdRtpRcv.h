#ifndef __RTP_RCV_H__
#define __RTP_RCV_H__

#include "JdRtp.h"

class CRtpRcv : public CRtp
{
public:
	CRtpRcv():CRtp(CRtp::MODE_CLIENT)
	{
		m_hRtpSock = -1;
		m_hRtcpSock = -1;
	}
	virtual ~CRtpRcv()
	{

	}
	virtual int Read(char *pData, int nMaxLen);
	static void ParseHeader(char *pData, RTP_PKT_T *pHdr);
};

#endif //__RTP_RCV_H__