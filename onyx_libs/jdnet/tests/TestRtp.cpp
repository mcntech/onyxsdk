// TestRtpRcv.cpp : Defines the entry point for the console application.
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

#include "JdRtp.h"
#ifdef WIN32
#define OSAL_WAIT(x)		Sleep(x)
#else
#define OSAL_WAIT(x)		usleep(x * 1000)
#endif
#define MAX_PKT_SIZE	1600
void *thrdRcv(void *pArg) 
{
	int res;
	RTP_PKT_T PktHdr;
	CRtp *pRtp = (CRtp *)pArg;
	char *pData = (char *)malloc(MAX_PKT_SIZE);
	long nTotalBytes;
	int nFailCount = 0;

	res = pRtp->Start();
	while(1) {
		RTP_PKT_T PktHdr;
		int res = pRtp->Read(pData, MAX_PKT_SIZE);
		if(res < 0){
			fprintf(stderr,"<R Failed:%d>",nFailCount++);			
			OSAL_WAIT(100);
			continue;
		}
		CRtp::ParseHeader(pData, &PktHdr);
		printf("\nSeq=%u SSRC=%u TimeStamp=%u BYtes Read=%d Total Bytes=%d",PktHdr.usSeqNum, PktHdr.ulSsrc, PktHdr.ulTimeStamp, res, nTotalBytes);
		nTotalBytes += res;
		
	}
	return NULL;
}

void *thrdSnd(void *pArg) 
{
	int res;
	RTP_PKT_T PktHdr;
	CRtp *pRtp = (CRtp *)pArg;
	char *pData = (char *)malloc(MAX_PKT_SIZE);
	long nTotalBytes = 0;
	int nFailCount = 0;
	res = pRtp->Start();
	while(1) {
		RTP_PKT_T PktHdr;
		CRtp::CreateHeader(pData, &PktHdr);
		int res = pRtp->Write(pData, MAX_PKT_SIZE);
		if(res < 0){
			fprintf(stderr,"<S Failed:%d>",nFailCount++);			
			OSAL_WAIT(100);
			break;
		}
		OSAL_WAIT(30);
		nTotalBytes += res;
		
	}
	return NULL;
}

int main(int argc, char* argv[])
{
	int res = 0;
	int nTotalBytes = 0;
	char *pszRes = NULL;

	in_addr PeerAddr;
	struct hostent *hp;
	pthread_t hthrdSnd;
	pthread_t hthrdRcv;
	char *pData = (char *)malloc(MAX_PKT_SIZE);
#ifdef WIN32
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
#endif
	pszRes = argv[1];
	CRtp *pRtp = new CRtp(CRtp::MODE_SERVER);
	hp = gethostbyname("localhost");
	memcpy((char *)&PeerAddr, (char *)hp->h_addr, hp->h_length);
	pRtp->m_SockAddr = PeerAddr;
	pRtp->mLocalRtpPort = 9427;
	pRtp->mLocalRtcpPort = 9428;
	pRtp->mRemoteRtpPort = 10000;
	pRtp->mRemoteRtcpPort = 10001;
	pRtp->mMulticast = false;

	res = pRtp->CreateSession();
	if(res < 0){
		fprintf(stderr,"Failed to open rtp connection for %s",pszRes);
		return 0;
	}

	//pthread_create(&hthrdRcv, NULL, thrdRcv, pRtp);
	pthread_create(&hthrdSnd, NULL, thrdSnd, pRtp);

	void *pRes;
	//pthread_join(hthrdRcv, &pRes);
	pthread_join(hthrdSnd, &pRes);
#ifdef WIN32
#else
#endif
	return 0;
}

