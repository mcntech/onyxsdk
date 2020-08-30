// OpenMAX AL MediaPlayer command-line player
#ifdef WIN32
//#include <Windows.h>
#include <winsock2.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <assert.h>
#include "minini.h"
#include "onyx_omxext_api.h"
#include "JdRtspSrv.h"
#include "JdRtspClntRec.h"
#include "JdRfc3984.h"
#include "JdDbg.h"
#include "strmconn.h"
#include "sock_ipc.h"
#include "uimsg.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <signal.h> 
#include "OpenMAXAL/OpenMAXAL.h"
#include  "onyx_omxext_api.h"
#include "h264parser.h"
#include "TsDemuxIf.h"
#include "TsPsiIf.h"
#include "tsfilter.h"

#include "StrmOutBridgeBase.h"
#include "RtspClntBridge.h"
#include "HlsClntBridge.h"
#include "RtspPublishBridge.h"
#include "RtmpPublishBridge.h"
#include "HlsSrvBridge.h"
#include "EncOut.h"

#define EN_HTTP_LIVE_SRV

#ifdef EN_HTTP_LIVE_SRV
#include "JdHttpSrv.h"
#include "AccessUnit.h"
#include "SimpleTsMux.h"
#include "JdHttpLiveSgmt.h"
#endif
#include "OmxIf.h"

#define MAX_PATH                256
#define MAX_FILE_NAME_SIZE		256
#define MAX_CODEC_NAME_SIZE		128

#define MAX_CONSOLE_MSG			256

#define RTSP_PREFIX "rtsp://"
#define HLS_PREFIX "http://"

#define XA_MIME_MP2TS  "mp2t"
#define XA_MIME_YUY2   "yuy2"

#define SESSION_CMD_STOP   1
#define SESSION_CMD_PAUSE  2
#define SESSION_CMD_RUN    3
//#define NO_DEBUG_LOG

static int  modDbgLevel = CJdDbg::LVL_TRACE;

#ifdef NO_DEBUG_LOG
#define DBG_PRINT(...)
#else
#define DBG_PRINT(...)                                                \
          if (1)                                                      \
          {                                                           \
            printf(__VA_ARGS__);                                      \
          }
#endif
 

#define OUTPUT_STREAM_PREFIX    "output"
#define ONYX_MW_SECTION        "onyx_mw"


#ifdef WIN32
#define ONYX_PUBLISH_FILENAME	"C:/Projects/onyx/onyx_em/conf/onyx_publish.conf"
#else
#define ONYX_PUBLISH_FILENAME	"/etc/onyx_publish.conf"
#endif
#define SECTION_INPUT_PREFIX    "input"


class CRtspPublishBridge;
void PostAppQuitMessage();


#define CLIENT_MSG_NONE  0
#define CLIENT_MSG_READY 1
#define CLIENT_MSG_ERROR -1

class CUiMsg
{

public:
	int GetClntMsg(UI_MSG_T *pMsg);
	int Reply(int nCmdId, int nStatusCode);
	int ReplyStats(int nCmdId, void *pData, int nSize);
	void Abort() {m_Abort = 1;}
	int IsAbort() {return m_Abort;}
	void Start();
	void Stop();
	static void *thrdAcceptClientConnections(void *pArg);
	void AcceptClientConnections();
	int RemoveClient(spc_socket_t *pSockClient);
	int AddClient(spc_socket_t *pSockClient);
	CUiMsg()
	{
		m_pSockClient = NULL;
		m_pSockListen = NULL;
		m_Abort = 0;
	}

	static CUiMsg *GetSingleton()
	{
		if(m_Obj == NULL){
			m_Obj = new CUiMsg;
		}
		return m_Obj;
	}
	static CUiMsg *m_Obj;
	int IsClientConnected()
	{
		return m_listClientSocket.size();
	}

private:
	ClientSocketList_T m_listClientSocket;
	spc_socket_t *m_pSockClient;
	spc_socket_t *m_pSockListen;
	int m_Abort;
	int m_fRun;
	void         *m_thrdHandle;
	COsalMutex   m_Mutex;
};

CUiMsg *CUiMsg::m_Obj = NULL;

class CConsoleMsg
{
public:
	CConsoleMsg()
	{
		m_pSock = NULL;
	}

	int Run()
	{
		jdoalThreadCreate(&m_thrdHandle, thrdConsoleMsg, this);
		return 0;
	}

	int Stop()
	{
		// TODO : Terminate
		return 0;
	}
	static void* thrdConsoleMsg(void *pCtx);
private:
	void               *m_thrdHandle;
	spc_socket_t *m_pSock;
};


COutputStream::COutputStream(const char *pszConfFile, const char *pszOutputSection)
{
	ini_gets(pszOutputSection, "stream_name", "", m_szStreamName, 128, pszConfFile);
}

CRtspPublishConfig::CRtspPublishConfig(const char *pszConfFile)
{
	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTSP_PUBLISH_HOST, "", szRtspServerAddr, 64, pszConfFile);
	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTSP_PUBLISH_APPLICATION, "", szApplicationName, 64, pszConfFile);
	usRtpLocalPort =  (unsigned short)ini_getl(SECTION_RTSP_PUBLISH, "local_port",   59500, pszConfFile);
	usRtpRemotePort =  (unsigned short)ini_getl(SECTION_RTSP_PUBLISH, "remote_port",   49500, pszConfFile);
	usServerRtspPort =  (unsigned short)ini_getl(SECTION_RTSP_PUBLISH, KEY_RTSP_PUBLISH_RTSP_PORT,   1935, pszConfFile);
}

CRtmpPublishConfig::CRtmpPublishConfig(const char *pszConfFile)
{
	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTMP_PUBLISH_PRIMARY_HOST, "", szPrimarySrvAddr, 64, pszConfFile);
	usPrimarySrvPort = (unsigned short)ini_getl(SECTION_RTMP_PUBLISH, KEY_RTMP_PUBLISH_PRIMARY_PORT,   1935, pszConfFile);
	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTMP_PUBLISH_PRIMARY_APP, "", szPrimarySrvAppName, 64, pszConfFile);
	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTMP_PUBLISH_PRIMARY_STRM, "", szPrimarySrvStrmName, 64, pszConfFile);
	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTMP_PUBLISH_PRIMARY_USER, "", szPrimarySrvUserId, 64, pszConfFile);
	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTMP_PUBLISH_PRIMARY_PASSWD, "", szPrimarySrvPasswd, 64, pszConfFile);

	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTMP_PUBLISH_SECONDARY_HOST, "", szSecondarySrvAddr, 64, pszConfFile);
	usSecondarySrvPort = (unsigned short)ini_getl(SECTION_RTMP_PUBLISH, KEY_RTMP_PUBLISH_SECONDARY_PORT,   1935, pszConfFile);
	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTMP_PUBLISH_SECONDARY_APP, "", szSecondarySrvAppName, 64, pszConfFile);
	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTMP_PUBLISH_SECONDARY_STRM, "", szSecondarySrvStrmName, 64, pszConfFile);
	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTMP_PUBLISH_SECONDARY_USER, "", szSecondarySrvUserId, 64, pszConfFile);
	ini_gets(SECTION_RTSP_PUBLISH, KEY_RTMP_PUBLISH_SECONDARY_PASSWD, "", szSecondarySrvPasswd, 64, pszConfFile);

}

CRtspSrvConfig::CRtspSrvConfig(const char *pszConfFile)
{
	ini_gets(SECTION_RTSP_SERVER, KEY_RTSP_SERVER_STREAM, "", szStreamName, 64, pszConfFile);
	usServerRtspPort =  (unsigned short)ini_getl(SECTION_RTSP_SERVER, KEY_RTSP_SERVER_RTSP_PORT,   554, pszConfFile);
}

CHlsServerConfig::CHlsServerConfig(const char *pszConfFile)
{
	ini_gets(SECTION_HLS_SERVER, "application", "", m_szApplicationName, 64, pszConfFile);
	m_usServerRtspPort =  (unsigned short)ini_getl(SECTION_HLS_SERVER, "port", 8080, pszConfFile);
	m_fInternaHttpSrv = ini_getl(SECTION_HLS_SERVER, KEY_HLS_SRVR_INTERNAL, 0, pszConfFile);
	ini_gets(SECTION_HLS_SERVER, KEY_HLS_SRVR_ROOT, "", m_szServerRoot, 128, pszConfFile);
	ini_gets(SECTION_HLS_SERVER, KEY_HLS_FOLDER, "", m_szFolder, 128, pszConfFile);
	ini_gets(SECTION_HLS_SERVER, KEY_HLS_STREAM, "", m_szStream, 128, pszConfFile);
	m_nSegmentDuration = ini_getl(SECTION_HLS_SERVER, KEY_HLS_SEGMENT_DURATION, 4000, pszConfFile);
	m_fLiveOnly = ini_getl(SECTION_HLS_SERVER, KEY_HLS_LIVE_ONLY, 1, pszConfFile);
}


CHlsPublishCfg::CHlsPublishCfg(const char *pszConfFile)
{
	ini_gets(SECTION_HLS_PUBLISH, "protocol", "", m_szProtocol, 32, pszConfFile);
	ini_gets(SECTION_HLS_PUBLISH, "folder", "", m_szFolder, 128, pszConfFile);
	ini_gets(SECTION_HLS_PUBLISH, "stream", "", m_szStream, 128, pszConfFile);
	ini_gets(SECTION_HLS_PUBLISH, "http_server", "", m_pszHost, 128, pszConfFile);
	ini_gets(SECTION_HLS_PUBLISH, "s3_bucket", "", m_pszBucket, 128, pszConfFile);
	ini_gets(SECTION_HLS_PUBLISH, "access_id", "", m_szAccessId, 128, pszConfFile);
	ini_gets(SECTION_HLS_PUBLISH, "security_key", "", m_szSecKey, 128, pszConfFile);
	m_fLiveOnly = ini_getl(SECTION_HLS_PUBLISH, "live_only", 1, pszConfFile);
	m_nSegmentDuration = ini_getl(SECTION_HLS_PUBLISH, "segment_duration", 4000, pszConfFile);
	m_nDdebugLevel = ini_getl(SECTION_HLS_PUBLISH, "debug_level", 1, pszConfFile);
}

typedef struct _APP_OPTIONS
{
	char conf_file[MAX_FILE_NAME_SIZE];
	int  nLayoutId;
} APP_OPTIONS;



static void DumpHex(unsigned char *pData, int nSize)
{
	int i;
	for (i=0; i < nSize; i++)
		printf("%02x ", pData[i]);
	printf("\n");
}



void SignalHandler(int sig)
{
	DBG_PRINT("!!! omax_qt_app:SignalHandler:Recevied Siganl !!! %d\n", sig);
	CUiMsg::GetSingleton()->Abort();
}


static void usage()
{
	printf ("./omaxal_test  -c <user configuration file> -m <layout>\n");
	exit (1);
}

 
#ifdef WIN32 
	static char* letP =NULL;    // Speichert den Ort des Zeichens der
									// naechsten Option
	static char SW ='-';       // DOS-Schalter, entweder '-' oder '/'
 
	int   optind  = 1;    // Index: welches Argument ist das naechste
	char* optarg;         // Zeiger auf das Argument der akt. Option
	int   opterr  = 1;    // erlaubt Fehlermeldungen
 
 // ===========================================================================
 int getopt(int argc, char *argv[], const char *optionS)
 {
    unsigned char ch;
    char *optP;
 
    if(argc>optind)
    {
       if(letP==NULL)
       {
         // Initialize letP
          if( (letP=argv[optind])==NULL || *(letP++)!=SW )
             goto gopEOF;
          if(*letP == SW)
          {
             // "--" is end of options.
             optind++;
             goto gopEOF;
          }
       }
 
       if((ch=*(letP++))== '\0')
       {
          // "-" is end of options.
          optind++;
          goto gopEOF;
       }
       if(':'==ch || (optP=(char*)strchr(optionS,ch)) == NULL)
       {
          goto gopError;
       }
       // 'ch' is a valid option
       // 'optP' points to the optoin char in optionS
       if(':'==*(++optP))
       {
          // Option needs a parameter.
          optind++;
          if('\0'==*letP)
          {
             // parameter is in next argument
             if(argc <= optind)
                goto gopError;
             letP = argv[optind++];
          }
          optarg = letP;
          letP = NULL;
       }
       else
       {
          // Option needs no parameter.
          if('\0'==*letP)
          {
             // Move on to next argument.
             optind++;
             letP = NULL;
          }
          optarg = NULL;
       }
       return ch;
    }
 gopEOF:
    optarg=letP=NULL;
    return EOF;
     
 gopError:
    optarg = NULL;
    errno  = -1;
    return ('?');
 }
#endif

static void parse_args (int argc, char *argv[], APP_OPTIONS *pOptions)
{
	const char shortOptions[] = "c:m:";
	int argID;
	memset(pOptions, 0x00, sizeof(APP_OPTIONS));
	strcpy(pOptions->conf_file, ONYX_USER_FILE_NAME);
	for (;;)	{
		argID = getopt(argc, argv, shortOptions);

		if (argID == -1){
			break;
		}

		switch (argID)
		{
			case 'c':
				strncpy (pOptions->conf_file, optarg, MAX_FILE_NAME_SIZE);
				break;

			case 'm':
				pOptions->nLayoutId = atoi(optarg);
				break;
				

			case '?':
			default:
				usage ();
				exit (1);
		}
	}
}


#define MAX_SESSIONS 6

unsigned long ClockGet()
{
#ifdef WIN32
	return GetTickCount();
#else
	struct timeval   tv;
	gettimeofday(&tv,NULL);
	long long Timestamp =  ((long long)tv.tv_sec)*1000 + tv.tv_usec / 1000;
	return (Timestamp);
#endif
}

#define TIME_SECOND 1000
void CEncOut::ShowStats()
{
	unsigned long clk = ClockGet();
	if(clk > PrevClk + TIME_SECOND) {
		DBG_PRINT("EncOut vid=%d aud=%d\n", vid_frames, aud_frames);
		PrevClk = clk;
	}
}

//===================================================================================




class COnyxMw
{
public:
	COnyxMw();
	int Run(const char *pszConfFile, int nLayoutOption);
	int StartSession(const char *pszConfFile, int nLayoutOption);
	int StartDecodeSubsystem(const char *pszConfFile, int nLayoutOption);
	int StartPublishStreams(const char *pszConfFile);
	int StartEncodeSubsystem(const char *pszConfFile, int nLayoutOption);

	int CloseSession();
	int StopPublishStreams();
	int StopEncodeSubsystem();
	int StopDecodeSubsystem();

private:	
	void GetPublishConfig(const char *pszConfFile);
	int CreatePlayerSession(ENGINE_T *pEngine, PLAYER_SESSION_T *pSession);
	void DeletePlayerSession(PLAYER_SESSION_T *pSession);
	INPUT_TYPE_T GetInputUrl(char *pszInput, const char *pszSection, const char *pszConfFile);

public:
	char szSessionName[64];
	char szInput[MAX_FILE_NAME_SIZE];
	char szOutputSection[MAX_FILE_NAME_SIZE];
	PLAYER_SESSION_T *listSession[MAX_SESSIONS];


	ENGINE_T          *pEngine;

	char *pszConfFile;
	int                 m_nLayoutId;
	
	CRtspPublishBridge  *m_pRtspSrvBridge;
	CHlsSrvBridge       *m_pHlsSrvBridge;
	CRtmpPublishBridge	*m_pRtmpPublishBridge;

	CRtspSrvConfig      *m_pRtspSvrCfg;
	CRtspPublishConfig  *m_pRtspPublishCfg;
	CRtmpPublishConfig  *m_pRtmpPublishCfg;
	CHlsServerConfig    *m_pHlsSvrCfg;
	CHlsPublishCfg	    *m_pHlsPublishCfg;
	CEncOut              *m_pEncOut;

	int                 m_fEnableRtspPublish;
	int                 m_fEnableRtspSrv;
	int                 m_fEnableHlsSvr;
	int                 m_fEnableHlsPublish;
	int                 m_fEnableRtmpPublish;
	COutputStream       *m_pOutputStream;
	ConnCtxT            *m_pVidConnSrc;
	ConnCtxT            *m_pAudConnSrc;
};

void COnyxMw::GetPublishConfig(const char *pszConfFile)
{
	m_fEnableHlsSvr =  ini_getl(SECTION_SERVERS, EN_HLS_SERVER, 0, pszConfFile);
	m_fEnableHlsPublish =  ini_getl(SECTION_SERVERS, EN_HLS_PUBLISH, 0, pszConfFile);
	m_fEnableRtspPublish =  ini_getl(SECTION_SERVERS, EN_RTSP_PUBLISH, 0, pszConfFile);
	m_fEnableRtspSrv = ini_getl(SECTION_SERVERS, EN_RTSP_SERVER, 0, pszConfFile);
	m_fEnableRtmpPublish = ini_getl(SECTION_SERVERS, EN_RTMP_PUBLISH, 0, pszConfFile);
}

COnyxMw::COnyxMw()
{
	memset(listSession, 0x00, sizeof (PLAYER_SESSION_T *) * MAX_SESSIONS); 


	m_fEnableRtspPublish = 0;
	m_fEnableRtmpPublish = 0;
	m_fEnableRtspSrv = 0;
	m_fEnableHlsSvr = 0;
	m_fEnableHlsPublish = 0;
	m_pHlsSrvBridge = NULL;
	m_pRtspSrvBridge = NULL;
	m_pRtmpPublishBridge = NULL;
	m_pRtspPublishCfg = NULL;
	m_pRtmpPublishCfg = NULL;
	m_pRtspSvrCfg = NULL;
	m_pHlsSvrCfg = NULL;
	m_pVidConnSrc = NULL;
	m_pAudConnSrc = NULL;
};


INPUT_TYPE_T COnyxMw::GetInputUrl(char *pszInput, const char *pszSection, const char *pszConfFile)
{
	INPUT_TYPE_T nType = INPUT_TYPE_UNKNOWN;
	char szType[16];

	ini_gets(pszSection, KEY_INPUT_TYPE, "", szType, 16, pszConfFile);
	if (strcmp(szType, INPUT_STREAM_TYPE_FILE) == 0) {
		nType = INPUT_TYPE_FILE;
		ini_gets(pszSection, "location", "", pszInput, MAX_FILE_NAME_SIZE, pszConfFile);
	} else if (strcmp(szType, INPUT_STREAM_TYPE_RTSP) == 0) {
		char szHost[64];
		char szPort[64];
		char szStream[64];
		
		ini_gets(pszSection, KEY_INPUT_HOST, "", szHost, 64, pszConfFile);
		if(strlen(szHost)) {
			nType = INPUT_TYPE_RTSP;
			ini_gets(pszSection, KEY_INPUT_PORT, "554", szPort, 64, pszConfFile);
			ini_gets(pszSection, KEY_INPUT_STREAM, "v03", szStream, 64, pszConfFile);
			sprintf(pszInput, "rtsp://%s:%s/%s", szHost, szPort, szStream);
		}
	} else if (strcmp(szType, INPUT_STREAM_TYPE_HLS) == 0) {
		char szHost[64];
		char szPort[64];
		char szApplication[64];
		char szStream[64];
		
		ini_gets(pszSection, KEY_INPUT_HOST, "", szHost, 64, pszConfFile);
		if(strlen(szHost)) {
			nType = INPUT_TYPE_HLS;
			ini_gets(pszSection, KEY_INPUT_PORT, "8080", szPort, 64, pszConfFile);
			ini_gets(pszSection, KEY_INPUT_APPLICATION, "httplive", szApplication, 64, pszConfFile);
			ini_gets(pszSection, KEY_INPUT_STREAM, "channel1", szStream, 64, pszConfFile);
			sprintf(pszInput, "http://%s:%s/%s/%s.m3u8", szHost, szPort, szApplication, szStream);
		}
	} else if (strcmp(szType, INPUT_STREAM_TYPE_CAPTURE) == 0) {
		nType = INPUT_TYPE_CAPTURE;
		ini_gets(pszSection, KEY_INPUT_DEVICE, "/dev/dvm_sdi", pszInput, 64, pszConfFile);
	}
	return nType;
}

int GetPlayerSessionUserOverrides(PLAYER_SESSION_T *pSession, const char *pszConfFile, const char *szSessionName, int fEnableAudio)
{
	//if(strcmp(pSession->vid_codec_name, "h264") == 0)
	{
		pSession->nWidth =  ini_getl(szSessionName, "width",   0, pszConfFile);
		pSession->nHeight =  ini_getl(szSessionName, "height",  0, pszConfFile);
		pSession->nFrameRate =  ini_getl(szSessionName, "framerate",  30, pszConfFile);
		pSession->nDeinterlace =  ini_getl(szSessionName, "deinterlace",  0, pszConfFile);
		pSession->nSelectProg =  ini_getl(szSessionName, "program",  0, pszConfFile);
		pSession->fEnableVid = 1;
	}
	if(fEnableAudio) {
		ini_gets(szSessionName, "aud_codec", "", pSession->aud_codec_name, MAX_CODEC_NAME_SIZE, pszConfFile);
		if(strcmp(pSession->aud_codec_name, "g711u") == 0){
			pSession->fEnableAud = 1;
		}
	} else {
		strcpy(pSession->aud_codec_name, "null");
		pSession->fEnableAud = 0;
	}
	return 0;
}

int COnyxMw::StartSession(const char *pszConfFile, int nLayoutOption)
{
	int res = 0;
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);


	GetPublishConfig(ONYX_PUBLISH_FILENAME);

	StartEncodeSubsystem(pszConfFile, nLayoutOption);
	StartPublishStreams(pszConfFile);
	StartDecodeSubsystem(pszConfFile, nLayoutOption);
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));	
	return res;
}

int COnyxMw::StartPublishStreams(const char *pszConfFile)
{
	int res = 0;
	int i;
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	GetPublishConfig(ONYX_PUBLISH_FILENAME);

	if(m_fEnableRtspPublish || m_fEnableRtspSrv) {
		m_pRtspSrvBridge = new CRtspPublishBridge;
		if(m_fEnableRtspPublish)
			m_pRtspPublishCfg = new CRtspPublishConfig(ONYX_PUBLISH_FILENAME);

		if(m_fEnableRtspSrv)
			m_pRtspSvrCfg = new CRtspSrvConfig(ONYX_PUBLISH_FILENAME);

		m_pRtspSrvBridge->Run(m_pOutputStream);

		if(m_fEnableRtspSrv)
			m_pRtspSrvBridge->StartRtspServer(m_pRtspSvrCfg);

		if(m_fEnableRtspPublish)
			m_pRtspSrvBridge->ConnectToPublishServer(m_pRtspPublishCfg);

		m_pEncOut->AddOutput(m_pRtspSrvBridge);
	}

	if(m_fEnableHlsSvr || m_fEnableHlsPublish) {
		m_pHlsSrvBridge = new CHlsSrvBridge;
		if(m_fEnableHlsSvr) {
			m_pHlsSvrCfg = new CHlsServerConfig(ONYX_PUBLISH_FILENAME);
			m_pHlsSrvBridge->SetServerConfig(m_pHlsSvrCfg);
		}
		if(m_fEnableHlsPublish) {
			m_pHlsPublishCfg = new CHlsPublishCfg(ONYX_PUBLISH_FILENAME);
			m_pHlsSrvBridge->SetServerConfig(m_pHlsPublishCfg);
		}

		m_pHlsSrvBridge->Run(m_pOutputStream);
		m_pEncOut->AddOutput(m_pHlsSrvBridge);
	}

	if(m_fEnableRtmpPublish){
		m_pRtmpPublishBridge = new CRtmpPublishBridge;
		m_pRtmpPublishCfg = new CRtmpPublishConfig(ONYX_PUBLISH_FILENAME);
		m_pRtmpPublishBridge->SetServerConfig(m_pRtmpPublishCfg);
		m_pRtmpPublishBridge->Run(m_pOutputStream);
		m_pEncOut->AddOutput(m_pRtmpPublishBridge);
	}
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));	
	return res;
}

int GetVidMixerConfig(
	const char       *szConfigFile,
	int              nLayoutId,
	DISP_WINDOW_LIST **ppWndList)
{
	int i;
	char szLayoutName[32];
	char szWndName[32];
	int nDefWidth = 720;
	int nDefHeight = 480;
	int nNumWindows = 0;
	DISP_WINDOW_LIST *pListWnd = (DISP_WINDOW_LIST *)malloc(sizeof(DISP_WINDOW_LIST));
	DISP_WINDOW *pWnd;

	sprintf(szLayoutName,"layout%d", nLayoutId);
	nNumWindows = ini_getl(szLayoutName, "windows", 1, szConfigFile); 
	pListWnd->nNumWnd = nNumWindows;
	if(nNumWindows > 0 && nNumWindows < 6) {
		pWnd = (DISP_WINDOW *)malloc(nNumWindows * sizeof(DISP_WINDOW));
		pListWnd->pWndList = pWnd;
		for (i=0; i < nNumWindows; i++) {
			char szWndDetails[256];
			sprintf(szWndName,"window%d", i + 1);
			ini_gets(szLayoutName, szWndName, "",   szWndDetails, 256, szConfigFile);
			if(strlen(szWndDetails)) {
				sscanf(szWndDetails,"%d %d %d %d %d", &pWnd->nStrmSrc, &pWnd->nStartX, &pWnd->nStartY, &pWnd->nWidth, &pWnd->nHeight);
			}
			pWnd++;
		}
	}
	*ppWndList = pListWnd;
	return 0;
}

int GetAudMixerConfig(
	const char       *szConfigFile,
	int              nLayoutId,
	AUD_CHAN_LIST    **ppAChanList)
{
	int i;
	char szLayoutName[32];
	char szAChanName[32];
	int nNumWindows = 0;
	AUD_CHAN_LIST    *pAChanList = (AUD_CHAN_LIST *)malloc(sizeof(AUD_CHAN_LIST));
	AUD_CHAN_T *pAChan;
	pAChanList->nNumChan = 0;
	pAChanList->pAChanList = NULL;
	sprintf(szLayoutName,"layout%d", nLayoutId);
	nNumWindows = ini_getl(szLayoutName, "windows", 1, szConfigFile); 

	if(nNumWindows > 0 && nNumWindows < 6) {
		pAChan = (AUD_CHAN_T *)malloc(nNumWindows * sizeof(AUD_CHAN_T));
		memset(pAChan, 0x00, sizeof(AUD_CHAN_T));
		pAChanList->pAChanList = pAChan;
		for (i=0; i < nNumWindows; i++) {
			char szAChanDetails[256];
			sprintf(szAChanName,"aud%d", i + 1);
			ini_gets(szLayoutName, szAChanName, "",   szAChanDetails, 256, szConfigFile);
			if(strlen(szAChanDetails)) {
				sscanf(szAChanDetails,"%d %d", &pAChan->nStrmSrc, &pAChan->nVolPercent);
			}
			pAChan++;
			pAChanList->nNumChan++;
		}
	}

	*ppAChanList = pAChanList;
	return 0;
}

int COnyxMw::StartEncodeSubsystem(const char *pszConfFile, int nLayoutOption)
{
	int res = 0;
	int i;
	int fEnableAudio;
	AUD_CHAN_LIST    *pAChanList = NULL;
	DISP_WINDOW_LIST *pWndList = NULL;

	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	if(nLayoutOption > 0) {
		m_nLayoutId = nLayoutOption;
	} else {
		m_nLayoutId = 1;
	}

	fEnableAudio =  ini_getl(ONYX_MW_SECTION, ENABLE_AUDIO, 1, ONYX_PUBLISH_FILENAME);

	GetVidMixerConfig(pszConfFile, m_nLayoutId, &pWndList);

	if(fEnableAudio) {
		GetAudMixerConfig(pszConfFile, m_nLayoutId, &pAChanList);
	}
	pEngine = omxalInit(pszConfFile,  pWndList, pAChanList);

	m_pVidConnSrc = CreateStrmConn(MAX_VID_FRAME_SIZE, 4);
	if(fEnableAudio) {
		m_pAudConnSrc = CreateStrmConn(MAX_AUD_FRAME_SIZE, 4);
	}
	sprintf(szOutputSection,"%s%d", OUTPUT_STREAM_PREFIX, 1);
	m_pOutputStream = new COutputStream(ONYX_PUBLISH_FILENAME, szOutputSection);
	
	omxalCreateRecorder(pEngine, m_pVidConnSrc, m_pAudConnSrc);
	
	m_pEncOut = new CEncOut();
	m_pEncOut->SetSource(m_pVidConnSrc, m_pAudConnSrc);

	m_pEncOut->Run();
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));	
	return res;
}


int COnyxMw::CreatePlayerSession(ENGINE_T *pEngine, PLAYER_SESSION_T *pSession)
{
	int res = -1;
	XADataSource dataSrc;
	XADataSource *pDataSrc1 = NULL;
	XADataSource *pDataSrc2 = NULL;
	XADataLocator_URI locUri;
	XADataFormat_MIME fmtMime;
	
	if(pSession->nInputType == INPUT_TYPE_HLS) {
		int nResult;
		CHlsClntBridge *pHlsClnt = NULL;
		pHlsClnt = new CHlsClntBridge(pSession->input_file_name, pSession->fEnableAud, pSession->fEnableVid, &nResult);
		pDataSrc1 = pHlsClnt->GetDataSource1();
		pDataSrc2 = pHlsClnt->GetDataSource2();

		if(pSession->nWidth == 0) 
			pSession->nWidth = pHlsClnt->m_lWidth;
		if(pSession->nHeight == 0) 
			pSession->nHeight = pHlsClnt->m_lHeight;

		pSession->mpInStream = pHlsClnt;
	} else if(pSession->nInputType == INPUT_TYPE_RTSP) {
		int nResult;
		CRtspClntBridge *pRtspClnt = NULL;
		pRtspClnt = new CRtspClntBridge(pSession->input_file_name, pSession->fEnableAud, pSession->fEnableVid, &nResult);
		pDataSrc1 = pRtspClnt->GetDataSource1();
		pDataSrc2 = pRtspClnt->GetDataSource2();

		if(pSession->nWidth == 0) 
			pSession->nWidth = pRtspClnt->m_lWidth;
		if(pSession->nHeight == 0) 
			pSession->nHeight = pRtspClnt->m_lHeight;
		pSession->mpInStream = pRtspClnt;
	} else if(pSession->nInputType == INPUT_TYPE_FILE){
		long hr = 0;
		long lWidth = 0;
		long lHeight = 0;
		CH264Stream *pVidSrc = NULL;
		CAacStream *pAudSrc = NULL;

		CTsFilter  *pTsFilter = CTsFilter::CreateInstance(&hr);
		pTsFilter->InitFileSrc(pSession->input_file_name);
		if(pTsFilter->InitProgram(pSession->nSelectProg, &pVidSrc, &pAudSrc) != 0) {
			DBG_PRINT("CreatePlayerSession:Could not locate program %d in %s...\n", pSession->nSelectProg, pSession->input_file_name);
			delete pTsFilter;
			goto Exit;
		}
		if(pVidSrc->m_nSpsSize > 0) {
			H264::cParser Parser;
			if(Parser.ParseSequenceParameterSetMin(pVidSrc->m_Sps,pVidSrc->m_nSpsSize, &lWidth, &lHeight) != 0){
				// Errorr
			}
		}
		locUri.locatorType = XA_DATALOCATOR_URI;
		locUri.pURI = (XAchar *) pSession->input_file_name;

		fmtMime.formatType = XA_DATAFORMAT_MIME;
		fmtMime.pMimeType = (XAchar *) XA_MIME_MP2TS;
		fmtMime.containerType = XA_CONTAINERTYPE_MPEG_TS;

		dataSrc.pLocator = &locUri;
		dataSrc.pFormat = &fmtMime;
		pDataSrc1 = &dataSrc;
		if(pSession->nWidth == 0) 
			pSession->nWidth = lWidth;
		if(pSession->nHeight == 0) 
			pSession->nHeight = lHeight;
		delete pTsFilter;
	} else if(pSession->nInputType == INPUT_TYPE_CAPTURE){
#if (1) // PRODUCT_DVM
		locUri.locatorType = XA_DATALOCATOR_URI;
		locUri.pURI = (XAchar *) pSession->input_file_name;

		fmtMime.formatType = XA_DATAFORMAT_MIME;
		fmtMime.pMimeType = (XAchar *) XA_MIME_YUY2;
		fmtMime.containerType = XA_CONTAINERTYPE_RAW;

		dataSrc.pLocator = &locUri;
		dataSrc.pFormat = &fmtMime;
		pDataSrc1 = &dataSrc;
		// TODO: Get width from Capture
		//if(pSession->nWidth == 0) 
		//	pSession->nWidth = lWidth;
		//if(pSession->nHeight == 0) 
		//	pSession->nHeight = lHeight;
		
#endif
	}
	if(pDataSrc1 == NULL || pDataSrc1->pLocator == NULL) {
		goto Exit;
	}
	res = omxalPlayStream(pEngine, pSession, pDataSrc1, pDataSrc2);

	if(pSession->mpInStream) {
		pSession->mpInStream->StartStreaming();
	}

Exit:
	return res;
}

void COnyxMw::DeletePlayerSession(PLAYER_SESSION_T *pSession)
{

	if(pSession->mpInStream) {
		JDBG_LOG(CJdDbg::LVL_TRACE,("Stopping RtspClnt"));
		pSession->mpInStream->StopStreaming();
	}
	omxalStopStream(pSession);
}



int COnyxMw::StartDecodeSubsystem(const char *pszConfFile, int nLayoutOption)
{
	int res = 0;
	int i;
	int fEnableAudio;

	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	if(nLayoutOption > 0) {
		m_nLayoutId = nLayoutOption;
	} else {
		m_nLayoutId = 1;
	}
	printf("LaoutId=%d\n", m_nLayoutId);
	fEnableAudio =  ini_getl(ONYX_MW_SECTION, ENABLE_AUDIO, 1, ONYX_PUBLISH_FILENAME);	
	for (i=0; i <MAX_SESSIONS; i++) {
		INPUT_TYPE_T nInputType;
		sprintf(szSessionName, "%s%d",SECTION_INPUT_PREFIX, i+1);

		nInputType = GetInputUrl(szInput, szSessionName, pszConfFile);
		printf("InputSection=%s resource=%s\n", szSessionName,szInput);

		if(nInputType != INPUT_TYPE_UNKNOWN) {
			PLAYER_SESSION_T *pSession = listSession[i] = (PLAYER_SESSION_T *)malloc(sizeof(PLAYER_SESSION_T));
			printf("Opening %s input=%s\n", szSessionName, szInput);
			memset(pSession, 0x00, sizeof(PLAYER_SESSION_T));
			pSession->nSessionId = i;
			strcpy(pSession->input_file_name, szInput);
			pSession->nInputType = nInputType;
			ini_gets(szSessionName, "vid_codec", "h264", pSession->vid_codec_name, MAX_CODEC_NAME_SIZE, pszConfFile);
			GetPlayerSessionUserOverrides(pSession, pszConfFile, szSessionName, fEnableAudio);
			pSession->nCmd = SESSION_CMD_RUN;
			if(CreatePlayerSession(pEngine, pSession) != 0) {
				printf("!!! Can not careate session %s or %s !!!\n", szSessionName,pszConfFile);
			}
		} else {
			printf("!!! Can not find %s or %s !!!\n", szSessionName,pszConfFile);
		}
	}
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));	
	return res;
}

int COnyxMw::CloseSession()
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	StopDecodeSubsystem();
	StopPublishStreams();
	StopEncodeSubsystem();
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return 0;
}
int COnyxMw::StopDecodeSubsystem()
{

	int i;
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	for (i=0; i <MAX_SESSIONS; i++) {
		PLAYER_SESSION_T *pSession = listSession[i];
		JDBG_LOG(CJdDbg::LVL_TRACE,("Stopping Playback Seesion %d", i));
		if(pSession) {
			DeletePlayerSession(pSession);
		}
	}
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return 0;
}
int COnyxMw::StopPublishStreams()
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	if(m_pRtspSrvBridge) {
		JDBG_LOG(CJdDbg::LVL_TRACE,("RtspPublishBridge:RemoveMediaDelivery"));
		m_pRtspSrvBridge->RemoveMediaDelivery(m_pOutputStream);
		m_pEncOut->DeleteOutput(m_pRtspSrvBridge);
		delete m_pRtspSrvBridge;
		m_pRtspSrvBridge = NULL;
	}

	if(m_pRtmpPublishBridge) {
		delete m_pRtmpPublishBridge;
		m_pRtmpPublishBridge = NULL;
	}

	if(m_pHlsSrvBridge){
		JDBG_LOG(CJdDbg::LVL_TRACE,("Stopping HlsSrvBridge"));
		delete m_pHlsSrvBridge;
		m_pHlsSrvBridge = NULL;
	}
	if(m_pHlsSvrCfg) {
		delete m_pHlsSvrCfg;
		m_pHlsSvrCfg = NULL;
	}
	if(m_pHlsPublishCfg) {
		delete m_pHlsPublishCfg;
		m_pHlsPublishCfg = NULL;
	}
	if(m_pRtspPublishCfg) {
		delete m_pRtspPublishCfg;
		m_pRtspPublishCfg = NULL;
	}
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return 0;
}
int COnyxMw::StopEncodeSubsystem()
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	if(m_pEncOut) {
		JDBG_LOG(CJdDbg::LVL_TRACE,("Stopping Encoder"));
		m_pEncOut->Stop();
	}

	if(m_pEncOut) {
		delete m_pEncOut;
		m_pEncOut = NULL;
	}

	JDBG_LOG(CJdDbg::LVL_TRACE,("DeleteEngine"));
	omxalDeinit(pEngine);

	if(m_pOutputStream) {
		delete m_pOutputStream;
		m_pOutputStream = NULL;
	}
	if(m_pVidConnSrc) {
		delete m_pVidConnSrc;
		m_pVidConnSrc = NULL;
	}

	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return 0;
}

#define ENABLE_CONSOLE

int COnyxMw::Run(const char *pszConfFile, int nLayoutOption)
{
	int res = 0;
	int nResClentMsg;
	CUiMsg *pUiMsg = CUiMsg::GetSingleton();
#ifdef ENABLE_CONSOLE
	CConsoleMsg ConsoleMsg;
	ConsoleMsg.Run();
#endif

	StartSession(pszConfFile, nLayoutOption);

	pUiMsg->Start();

	while(1) {
		UI_MSG_T Msg = {0};

		if(pUiMsg->IsAbort()) {
			DBG_PRINT("\nExiting due to signal\n");
			CloseSession();
			break;
		} 
		if(!pUiMsg->IsClientConnected()){
			JD_OAL_SLEEP(100);
			continue;
		}
		nResClentMsg = pUiMsg->GetClntMsg(&Msg);
		if(nResClentMsg == CLIENT_MSG_ERROR){
			DBG_PRINT("\nError getting UI Message\n");
			break;
		} else if(nResClentMsg == CLIENT_MSG_READY){
			if(Msg.nMsgId == UI_MSG_EXIT) {
				DBG_PRINT("\nExiting due UI Exit cmd\n");
				CloseSession();
				pUiMsg->Reply(UI_MSG_EXIT, UI_STATUS_OK);
				break;
			} else if(Msg.nMsgId == UI_MSG_SELECT_LAYOUT){
				// Restart
				int nLayoutId;
				DBG_PRINT("\nRestarting due to layout change\n");
				if(Msg.Msg.Layout.nLayoutId > 0) {
					nLayoutId = Msg.Msg.Layout.nLayoutId;
				} else {
					nLayoutId = nLayoutOption;
				}
				CloseSession();
				//StopDecodeSubsystem();
				StartSession(pszConfFile, nLayoutId);
				//StartDecodeSubsystem(pszConfFile, nLayoutId);
				pUiMsg->Reply(UI_MSG_SELECT_LAYOUT, UI_STATUS_OK);
			} else if (Msg.nMsgId == UI_MSG_HLS_PUBLISH_STATS) {
				hlspublisgStats_t hlspublisgStats = {0};
				if(m_pHlsSrvBridge) {
					m_pHlsSrvBridge->GetPublishStatistics(&hlspublisgStats.nState, &hlspublisgStats.nStreamInTime, 
						&hlspublisgStats.nLostBufferTime, &hlspublisgStats.nStreamOutTime, &hlspublisgStats.nTotalSegmentTime);
					DBG_PRINT("HLS Publisg Stats nState=%d nStreamInTime=%d\n", hlspublisgStats.nState, hlspublisgStats.nStreamInTime);
					pUiMsg->ReplyStats(UI_MSG_HLS_PUBLISH_STATS, &hlspublisgStats, sizeof(hlspublisgStats));
				}
			} else {
				DBG_PRINT("\nonyx_app:Unknown Command:%d\n", Msg.nMsgId);
			}
		} else {
			DBG_PRINT("onyx_app:No Client connection\n");
		}
	}
	return res;
}

//=================================================
// Console commands
//=================================================
void CUiMsg::AcceptClientConnections()
{
	if(m_pSockListen == NULL) {
		m_pSockListen = spc_socket_listen(SOCK_STREAM, 0, NULL,  ONYX_CMD_PORT);
	}
	if(m_pSockListen) {
		while(m_fRun) {
			spc_socket_t *pSockClient = spc_socket_accept(m_pSockListen);
			if(pSockClient) {
				AddClient(pSockClient);
			}
		}
	}
}

void *CUiMsg::thrdAcceptClientConnections(void *pArg)
{
	void *ret = NULL;
	CUiMsg *pUiMsg = (CUiMsg *)pArg;
	pUiMsg->AcceptClientConnections();
	return ret;
}

void CUiMsg::Start()
{
	m_fRun = 1;
	jdoalThreadCreate(&m_thrdHandle, thrdAcceptClientConnections, this);
}

void CUiMsg::Stop()
{
	if(m_pSockListen) {
		spc_socket_close(m_pSockListen);
	}
	m_fRun = 0;
	jdoalThreadJoin(m_thrdHandle, 1000);
}

int CUiMsg::RemoveClient(spc_socket_t *pSockClient)
{
	m_Mutex.Acquire();
	for(ClientSocketList_T::iterator it = m_listClientSocket.begin(); it != m_listClientSocket.end(); ++it) {
		if(spc_socket_recv_ready(*it, 100)){
			if(pSockClient == *it){
				m_listClientSocket.erase(it);
				break;
			}
		}
	}
	m_Mutex.Release();
	return 0;
}
int CUiMsg::AddClient(spc_socket_t *pSockClient)
{
	m_Mutex.Acquire();
	m_listClientSocket.push_back(pSockClient);
	m_Mutex.Release();
	return 0;
}


int CUiMsg::GetClntMsg(UI_MSG_T *pMsg)
{
	char szMsg[MAX_CONSOLE_MSG];

	int nMsgId = 0;
	m_pSockClient = spc_socket_set_recv_ready(m_listClientSocket, 1000);
	if(m_pSockClient){
		int result = 0;
		ONYX_MSG_HDR_T MsgHdr;
		result = spc_socket_recv(m_pSockClient, (char *)&MsgHdr, sizeof(ONYX_MSG_HDR_T));
		if(result > 0) {
			switch(MsgHdr.ulCmdId){
				case UI_MSG_SELECT_LAYOUT:
				{
					uiSelectLayout_t uiSelectLayout;
					if(spc_socket_recv(m_pSockClient, (char *)&uiSelectLayout, sizeof(uiSelectLayout)) > 0) {
						pMsg->nMsgId = MsgHdr.ulCmdId;
						pMsg->Msg.Layout = uiSelectLayout;
						return CLIENT_MSG_READY;
					}
				}
				break;
				case UI_MSG_EXIT:
				{
					pMsg->nMsgId = MsgHdr.ulCmdId;
					return CLIENT_MSG_READY;
				}
				break;
				case UI_MSG_HLS_PUBLISH_STATS:
				{
					pMsg->nMsgId = MsgHdr.ulCmdId;
					return CLIENT_MSG_READY;
				}
				break;
			}
		} else if (result <= 0) {
			if(result == 0) {
				printf("Connection closed by peer ");
			} else {
				printf("socket error\n");
			}
			RemoveClient(m_pSockClient);
			spc_socket_close(m_pSockClient);
			m_pSockClient = NULL;
			return CLIENT_MSG_NONE;
		}
	}
	return CLIENT_MSG_NONE;
}

int CUiMsg::Reply(int nCmdId, int nStatusCode)
{
	if(m_pSockClient) {
		int result = 0;
		ONYX_MSG_HDR_T Hdr;
		Hdr.ulCmdId = nCmdId;
		Hdr.ulSize = sizeof(int);
		result = spc_socket_send(m_pSockClient, &Hdr, sizeof(Hdr));
		if(result > 0) {
			result = spc_socket_send(m_pSockClient, &nStatusCode, sizeof(int));
		}
		if(result <= 0){
			if(result == 0) {
				printf("Connection closed by peer ");
			} else {
				printf("socket error\n");
			}
#ifdef WIN32
			Sleep(100);
#else
			usleep(100 * 1000);
#endif
		}
	}
	return 0;
}

int CUiMsg::ReplyStats(int nCmdId, void *pData, int nSize)
{
	if(m_pSockClient) {
		int result = 0;
		ONYX_MSG_HDR_T Hdr;
		Hdr.ulCmdId = nCmdId;
		Hdr.ulSize = nSize;
		result = spc_socket_send(m_pSockClient, &Hdr, sizeof(Hdr));
		if(result > 0) {
			result = spc_socket_send(m_pSockClient, pData, nSize);
		}
		if(result <= 0){
			if(result == 0) {
				printf("Connection closed by peer ");
			} else {
				printf("socket error\n");
			}
#ifdef WIN32
			Sleep(100);
#else
			usleep(100 * 1000);
#endif
		}
	}
	return 0;
}


void PostAppQuitMessage()
{
	ONYX_MSG_HDR_T Msg;
	DBG_PRINT("PostAppQuitMessage: Enter\n");
	spc_socket_t *pSock = spc_socket_connect("127.0.0.1", ONYX_CMD_PORT);
	if(pSock) {
		Msg.ulCmdId = UI_MSG_EXIT;
		Msg.ulSize = 0;
		DBG_PRINT("PostAppQuitMessage: Send Exit\n");
		spc_socket_send(pSock, (const char *)&Msg, sizeof(Msg));
		if(spc_socket_recv_ready(pSock, 5000)) {
			DBG_PRINT("PostAppQuitMessage: Get Status\n");
			spc_socket_recv(pSock, (void *)&Msg, sizeof(Msg));
		} else {
			DBG_PRINT("PostAppQuitMessage: Get Status: Timeout\n");
		}
		spc_socket_close(pSock);
	}
	DBG_PRINT("PostAppQuitMessage: Leave\n");
}

void *CConsoleMsg::thrdConsoleMsg(void *pCtx)
{
	void *res = NULL;
	UI_MSG_T Msg;
	char szMsg[256] = {0};
	CConsoleMsg *pObj = (CConsoleMsg *)pCtx;

	while(1) {
		printf("\nEnter command ==> ");
		if(fgets(szMsg, MAX_CONSOLE_MSG, stdin) == NULL) {
			DBG_PRINT("EoF or Error reading Msg ferror=%d\n", ferror(stdin));
			clearerr(stdin);
			break;
		}

		if(szMsg[0] == 'x'){
			PostAppQuitMessage();
			break;
		} else if(szMsg[0] == 'm'){
			int nStatus;
			char cCmd; 
			int nLayoutId;
			pObj->m_pSock = spc_socket_connect("127.0.0.1", ONYX_CMD_PORT);
			sscanf(szMsg,"%c %d",&cCmd, &nLayoutId);
			Msg.nMsgId = UI_MSG_SELECT_LAYOUT;
			Msg.Msg.Layout.nLayoutId = nLayoutId;
			if(pObj->m_pSock) {
				spc_socket_send(pObj->m_pSock, (const char *)&Msg.nMsgId, sizeof(int));
				spc_socket_send(pObj->m_pSock, (const char *)&nLayoutId, sizeof(nLayoutId));

				spc_socket_recv(pObj->m_pSock, (void *)&nStatus, sizeof(int));
			}
		}
		spc_socket_close(pObj->m_pSock);
	}
	return &res;
}


int main(int argc, char **argv)
{
	COnyxMw *pOnyxMw = new COnyxMw;
	APP_OPTIONS Options = {0};
	parse_args(argc, argv, &Options);

#ifdef WIN32
	{
		WORD wVersionRequested;
		WSADATA wsaData;
		int err;
		wVersionRequested = MAKEWORD(2, 2);
		err = WSAStartup(wVersionRequested, &wsaData);
		if (err != 0) {
			printf("WSAStartup failed with error: %d\n", err);
		   exit(0);
		}
	}
#endif
	modDbgLevel =  ini_getl(ONYX_MW_SECTION, "debug_level", 2, ONYX_PUBLISH_FILENAME);
	pOnyxMw->Run(Options.conf_file, Options.nLayoutId);
}
