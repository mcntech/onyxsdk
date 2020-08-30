#ifndef __JD_DRB_RTSP_CLNT_H__
#define __JD_DRB_RTSP_CLNT_H__

#include "JdRtspClnt.h"

class CJdDrbRtspClnt : public CJdRtspClntSession
{
public:
	CJdDrbRtspClnt():CJdRtspClntSession()
	{
	}
	~CJdDrbRtspClnt()
	{
	}
	int Open(const char *pszUrl);

	int SendSetup(char *pszDevDescript);
	void SendPlay();
	void Close();
	
	int PrepareSdp(char *pDescription);
	int Announce();
	int FindServer(char *pDescript, int nMaxLen);
	void SendRtspCommand(char *pszCmdBuf, int nLen);
	int GetRtspStatus(char *headerBuf, int nMaxLen, long TimeOut);

public:

	unsigned short mRemoteRtpPort;
	unsigned short mRemoteRtcpPort;
	unsigned short mLocalRtpPort;
	unsigned short mLocalRtcpPort;

	unsigned short m_usServerNatRtpPort;
	unsigned short m_usServerNatRtcpPort;
	unsigned short	m_usClientNatRtpPort;
	unsigned short	m_usClientNatRtcpPort;


	char *m_pszPathAnnounce;
	char *m_pszPathDescribe;
	char *m_pszPathAction;
	char *m_pszPathResponse;
	char *m_pszHost;

};

#endif	// __JD_DRB_RTSP_CLNT_H__