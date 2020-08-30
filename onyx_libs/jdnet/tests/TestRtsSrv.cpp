// TestRtspSrv.cpp : Defines the entry point for the console application.
//
#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>

#define close		closesocket
#define snprintf	_snprintf
#define write		_write
#else
#endif
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "JdSdp.h"
#include "JdRtspSrv.h"
#include "JdDrbRtspSrv.h"
#include "JdRfc2250.h"
#include "JdStrmSched.h"
#include "JdStun.h"
#include "JdNetUtil.h"

#define MAX_PKT_SIZE	1600
#define TEST_RTSP_DRB

#ifdef WIN32
#define DEF_ROOT_FOLDER	"c:/dms/"			// TODO: Get it from application
#else
#define DEF_ROOT_FOLDER	"/data/dms/"
#endif

int gfStop = 0;

#ifdef WIN32
#define OSAL_WAIT(x)	Sleep(x)
#else
#define OSAL_WAIT(x)	uleep(x * 1000)
#endif

static pthread_t thrdHandleStream;
char *GetFilepathForRtspResource(const char *pszRootFolder, char *szResource);
static int gStop = 0;
void *SendFile(void *pArg) 
{
	int len;
	long lPlLen;
	long lTotalPlLen = 0;
	CJdRfc2250 *pRfc2250 = new CJdRfc2250;
	CJdStrmSched *pSched = new CJdStrmSched;
	char *buffer;

	CJdRtspSrvSession *pSession = (CJdRtspSrvSession *)pArg;
	CRtp *pRtp = pSession->m_pRtp;
	char *pszFilePath = GetFilepathForRtspResource(DEF_ROOT_FOLDER, pSession->m_pszUrl);
	int fdIn = open(pszFilePath, O_RDONLY | O_BINARY);
	if (fdIn  < 0 ) {
		fprintf(stderr,"File not found:%s",pszFilePath);
		return 0;
	}

	fprintf(stderr,"Starting streaming session..\n");
	gStop = 0;
	while ( !gStop) {
		pRfc2250->GetBuffer(&buffer, &lPlLen);
		len = read(fdIn, buffer, lPlLen);

		if (pRfc2250->SendData(pRtp, buffer, len ) <= 0 ) {
				fprintf(stderr,"Failed to write to socket. Exiting..\n");
				goto Exit;
		}
		lTotalPlLen += lPlLen;
		//fprintf(stderr,".");
		fprintf(stderr,"<%d:%d:%d>",timeGetTime(),pRfc2250->m_ulTimeStamp,lTotalPlLen);
		pSched->Delay((long)(pRfc2250->m_ulTimeStamp / 90), len);
	}
	fprintf(stderr,"EoF. Exiting..\n");

Exit:
	if(fdIn)
		close(fdIn);
	delete pRfc2250;
	return NULL;
}

void *RunSimCamera(void *pArg) 
{
	int len;
	long lPlLen;
	long lTotalPlLen = 0;
	CJdRfc2250 *pRfc2250 = new CJdRfc2250;
	CJdStrmSched *pSched = new CJdStrmSched;
	char *buffer;

	CJdRtspSrvSession *pSession = (CJdRtspSrvSession *)pArg;
	CRtp *pRtp = pSession->m_pRtp;
	char *pszFilePath = "c:/dms/camera/test.ts";
	int fdIn = open(pszFilePath, O_RDONLY | O_BINARY);
	if (fdIn  < 0 ) {
		fprintf(stderr,"File not found:%s",pszFilePath);
		return 0;
	}

	fprintf(stderr,"Starting streaming session..\n");
	gStop = 0;
	while ( !gStop) {
		pRfc2250->GetBuffer(&buffer, &lPlLen);
		len = read(fdIn, buffer, lPlLen);

		if (pRfc2250->SendData(pRtp, buffer, len ) <= 0 ) {
				fprintf(stderr,"Failed to write to socket. Exiting..\n");
				goto Exit;
		}
		lTotalPlLen += lPlLen;
		//fprintf(stderr,".");
		fprintf(stderr,"<%d:%d:%d>",timeGetTime(),pRfc2250->m_ulTimeStamp,lTotalPlLen);
		pSched->Delay((long)(pRfc2250->m_ulTimeStamp / 90), len);
	}
	fprintf(stderr,"EoF. Exiting..\n");

Exit:
	if(fdIn)
		close(fdIn);
	delete pRfc2250;
	return NULL;
}

int SetStreamStateFolder(RTSP_STATE_T strmState,CJdRtspSrvSession *pSession)
{
	switch(strmState)
	{
		case RTSP_PLAY:
			pthread_create(&thrdHandleStream, NULL, SendFile, pSession);
		break;

		case RTSP_TEARDOWN:
		{
			void *res;
			gStop = 1;
			pthread_join(thrdHandleStream, &res);
		}

		break;
	}
	return 0;
}

int SetStreamStateCamera(RTSP_STATE_T strmState,CJdRtspSrvSession *pSession)
{
	switch(strmState)
	{
		case RTSP_PLAY:
			pthread_create(&thrdHandleStream, NULL, RunSimCamera, pSession);
		break;

		case RTSP_TEARDOWN:
			// Do nothing

		break;
	}
	return 0;
}
typedef enum _APP_MODE_T{
	APP_MODE_DRB,
	APP_MODE_LAN,
} APP_MODE_T;

int main(int argc, char* argv[])
{
	char *pszDrbFolder = NULL;
	int appMode = APP_MODE_LAN;
	CJdDrbRtspSrvSes pJdDrbRtspSrvSes(1234);
	CJdRtspSrv	*pRtspSrv = new CJdRtspSrv;
	int res = 0;
	int nTotalBytes = 0;
	if (argc < 2) {
		appMode = APP_MODE_LAN;
	} else {
		//drb://ntvcam.s3.amazonaws.com/004
		if(strncmp(argv[1], "drb://", strlen("drb://")) == 0)
			appMode = APP_MODE_DRB;
			pszDrbFolder = GetFileNameForDrbResource(argv[1]);
	}
	CJdStunDetect JdStunDetect;
#ifdef WIN32
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
#endif
	if (appMode == APP_MODE_DRB) {
		JdStunDetect.m_usHostInternalPort = DEF_SRV_RTP_PORT_START;
		int nNatType = JdStunDetect.GetNatType("stunserver.org");
		if(nNatType == NAT_VARIANT_UNKNOWN || 
				nNatType == NAT_VARIANT_SYMMETRIC ||
				nNatType == NAT_VARIANT_FIREWALL_BLOCKS_UDP || 
				nNatType ==NAT_VARIANT_SYMMETRIC_UDP_FIREWALL) {
			fprintf(stderr,"NAT Type(%d) not supported",nNatType);
		}
		pRtspSrv->RegisterCallback("movies", SetStreamStateFolder);
		pRtspSrv->RegisterCallback("camera", SetStreamStateCamera);
		if(pszDrbFolder) {
			pRtspSrv->RegisterCallback("pszDrbFolder", SetStreamStateCamera);
			free(pszDrbFolder);
		}
		pJdDrbRtspSrvSes.m_usServerRtpPort = DEF_SRV_RTP_PORT_START;
		pJdDrbRtspSrvSes.m_usServerNatRtpPort = JdStunDetect.m_usHostExternalPort;
		pJdDrbRtspSrvSes.m_HostNatAddr = JdStunDetect.m_HostExternalAddr;
		pJdDrbRtspSrvSes.m_HostLanAddr = JdStunDetect.m_HostInternalAddr;
		pJdDrbRtspSrvSes.Open(argv[1]);
		//pRtspSrv->Run(0, 554);
	} else {
		CJdRtspSrv	*pRtspSrv = new CJdRtspSrv;
		pRtspSrv->RegisterCallback("movies", SetStreamStateFolder);
		pRtspSrv->RegisterCallback("camera", SetStreamStateCamera);

		pRtspSrv->Run(0, 554);

	}
	while(!gfStop) {
		OSAL_WAIT(1000);
	}

#ifdef WIN32
#else
#endif

	return 0;
}
