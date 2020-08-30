#ifndef __JD_DRB_RTSP_SRV_H__
#define __JD_DRB_RTSP_SRV_H__

#include "JdRtspSrv.h"

/**
 * RTSP Server Session 
 */
class CJdDrbRtspSrvSes : public CJdRtspSrvSession
{
public:
	CJdDrbRtspSrvSes(
		unsigned long ulSessionId):CJdRtspSrvSession(ulSessionId)
	{
		m_pszPathDescribe = NULL;
		m_pszPathAction = NULL;
		m_pszPathResponse = NULL;
		m_pszHost = NULL;
		fStop = 0;
	}
	~CJdDrbRtspSrvSes()
	{
		if(m_pszPathDescribe) free(m_pszPathDescribe);
		if(m_pszPathAction) free(m_pszPathAction);
		if(m_pszPathResponse) free(m_pszPathResponse);
		if(m_pszHost) free(m_pszHost);
	}
	virtual void HandlePlay(char *szRqBuff);
	virtual void HandleTearDown(char *headerBuf);
	int HandleSetup(const char *szRqBuff);

	static void *ClientThread( void *arg );

	int Open(const char *pszUrl);
	int PrepareSdp(char *pDescription);
	int Advertise(char *pDescript, int nLen);
	void CreateEmptyFile(char *szFileName);
	int FindClient(char *pDescript, int nMaxLen);
	int GetRtspCommand(char *headerBuf, int nMaxLen);
	int SendRtspStatus(char *statusBuf, int nLen);
	int Close();

private:
	int StartThread();

	char *m_pszPathAnnounce;
	char *m_pszPathDescribe;
	char *m_pszPathAction;
	char *m_pszPathResponse;
	char *m_pszHost;
	int  hSock;
	int	 fStop;
};
#endif // __JD_DRB_RTSP_SRV_H__