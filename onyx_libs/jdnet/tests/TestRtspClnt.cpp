// TestRtspClnt.cpp : Defines the entry point for the console application.
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

#include "JdRtpRcv.h"
#include "JdRtspClnt.h"
#define MAX_PKT_SIZE	1600
int main(int argc, char* argv[])
{
	int res = 0;
	int nTotalBytes = 0;
	int nVidCodec, nAudCodec;
	char *pData = (char *)malloc(MAX_PKT_SIZE);
	if (argc < 2) {
		fprintf(stderr,"Usage : %s <url>", argv[0]);
		return 0;
	}
#ifdef WIN32
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
#endif
	CJdRtspClntSession *pRtspClnt = new CJdRtspClntSession;
	res = pRtspClnt->Open(argv[1], &nVidCodec, &nAudCodec);
	if(res < 0){
		fprintf(stderr,"Failed to open rtsp connection for %s",argv[1]);
		return 0;
	}
	pRtspClnt->SendPlay("video");
	if(res < 0){
		fprintf(stderr,"Failed to play rtsp stream %s",argv[1]);
		return 0;
	}
	while(1) {
		RTP_PKT_T PktHdr;
		int res = pRtspClnt->GetVData(pData, MAX_PKT_SIZE);
		if(res < 0)
			break;;
		CRtp::ParseHeader(pData, &PktHdr);
		printf("\nSeq=%u SSRC=%u TimeStamp=%u BYtes Read=%d Total Bytes=%d",PktHdr.usSeqNum, PktHdr.ulSsrc, PktHdr.ulTimeStamp, res, nTotalBytes);
		nTotalBytes += res;
		
	}
#ifdef WIN32
#else
#endif

	return 0;
}

