#ifndef __JD_DTCP_SND__
#define __JD_DTCP_SND__
#include "JdRtpSnd.h"

class CDtcpSnd : public CRtpSnd
{
public:

	int Configure(
		)
	{
	}
	virtual int Write(char *pData, int nMaxLen);
public:
	unsigned char m_Key[16];
};

#endif //__JD_DTCP_SND__