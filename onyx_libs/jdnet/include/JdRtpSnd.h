#ifndef __RTP_SND_H__
#define __RTP_SND_H__

#include "JdRtp.h"
#include "JdMediaResMgr.h"

class CRtpSnd : public CRtp
{
public:
	CRtpSnd():CRtp(CRtp::MODE_SERVER)
	{
	}

	virtual int Write(char *pData, int nMaxLen);
	static void CreateHeader(char *pData, RTP_PKT_T *pHdr);
};

#endif //__RTP_RCV_H__