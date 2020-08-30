#ifndef __STRM_OUT_BRIDGE_BASE__
#define __STRM_OUT_BRIDGE_BASE__
#ifdef WIN32
//#include <Windows.h>
#include <winsock2.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pthread.h"
#endif
#include "strmconn.h"

#define MAX_NAME_SIZE	256

#define MAX_VID_FRAME_SIZE          (1920 * 1080)
#define MAX_AUD_FRAME_SIZE          (3 * 1024)

class COutputStream
{
public:
	COutputStream(const char *pszConfFile,  const char *pszOutputSection);

public:
	char m_szStreamName[128];
};

class CRtspPublishConfig
{
public:
	CRtspPublishConfig(const char *pszConfFile);

public:
	char szRtspServerAddr[64];
	char szApplicationName[128];
	unsigned short usRtpLocalPort;
	unsigned short usRtpRemotePort;
	unsigned short usServerRtspPort;
};

class CRtspSrvConfig
{
public:
	CRtspSrvConfig(const char *pszConfFile);

public:
	char szStreamName[128];
	unsigned short usServerRtspPort;
};

class CHlsServerConfig
{
public:
	CHlsServerConfig(const char *pszConfFile);

public:
	char           m_szApplicationName[128];
	unsigned short m_usServerRtspPort;
	int            m_fInternaHttpSrv;
	char           m_szFolder[128];
	char           m_szServerRoot[128];
	char           m_szStream[128];
	int            m_nSegmentDuration;
	int            m_fLiveOnly;
};

class CRtmpPublishConfig
{
public:
	CRtmpPublishConfig(const char *pszConfFile);

public:
	int  fEnablePrimarySrv;
	char szPrimarySrvAddr[64];
	unsigned short usPrimarySrvPort;
	char szPrimarySrvAppName[128];
	char szPrimarySrvStrmName[128];
	char szPrimarySrvUserId[128];
	char szPrimarySrvPasswd[128];

	int  fEnableSecondarySrv;
	char szSecondarySrvAddr[64];
	unsigned short usSecondarySrvPort;
	char szSecondarySrvAppName[128];
	char szSecondarySrvStrmName[128];
	char szSecondarySrvUserId[128];
	char szSecondarySrvPasswd[128];
};


class CHlsPublishCfg
{
public:
	CHlsPublishCfg(const char *pszConfFile);

public:
	char           m_szProtocol[32];
	char           m_szFolder[128];
	char           m_szStream[128];
	char           m_pszHost[128];
	char           m_pszBucket[128];
	char           m_szAccessId[128];
	char           m_szSecKey[128];
	int            m_fLiveOnly;
	int            m_nSegmentDuration;
	int            m_nDdebugLevel;
};

class CStrmOutBridge
{
public:
	virtual int Run(COutputStream *pOutputStream) = 0;
	virtual ~CStrmOutBridge(){}
	//static void *threadStreaming(void *threadsArg);

	virtual int SendVideo(unsigned char *pData, int size, unsigned long lPts) = 0;
	virtual int SendAudio(unsigned char *pData, int size, unsigned long lPts) = 0;

public:
	int            m_nUiCmd;
	int            m_fRun;
	COutputStream  *m_pOutputStream;
#ifdef WIN32
	HANDLE         m_thrdHandle;
#else
	pthread_t      m_thrdHandle;
#endif
	int        fEoS;
	long       read_position;
	int        frameCounter;
};
#endif