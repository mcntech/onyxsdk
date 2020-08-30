#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include "udprdr.h"


#define UDP_PROTOCOL "udp://"

static int ParseUdpSrc(const char *pszUdpSrc, struct in_addr *addr, unsigned short *pusPort, int *fMcast)
{
	int addrFirstByte = 0;
	memset(addr, sizeof(struct in_addr), 0x00);
	*pusPort = 0;
	*fMcast = 0;

	if(strncmp(pszUdpSrc,UDP_PROTOCOL,strlen(UDP_PROTOCOL)) == 0){
		const char *pTmp = pszUdpSrc + strlen(UDP_PROTOCOL);
		const char *pIndx = strchr(pTmp,':');
		if(pIndx) {
			char szAddr[32] = {0};
			if(pIndx == pTmp) {
				printf("Using as unicast address\n");
			} else {
				strncpy(szAddr, pTmp, pIndx- pTmp);
				if(inet_aton(szAddr, addr) == 0){
					fprintf(stderr, "inet_aton failed\n");
					return -1;
				}
			}
			*pusPort = atoi(pIndx+1);
		}
	}
	addrFirstByte = addr->s_addr & 0x000000FF;
	printf("addrFirstByte=%d addr=0x%x", addrFirstByte, addr->s_addr);
	if(addrFirstByte >= 224 && addrFirstByte <= 239){
		*fMcast = 1;
	} else {
		*fMcast = 0;
	}
	return 0;
}

int OpenIpcSocket(char *szSockName)
{
	struct sockaddr_un ipc_sock_addr;
	int len;
	int res = 0;
	int sock;
	// Open IPC Socket
	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	//sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		fprintf(stderr, "Failed to create IPC socket err=%d\n", sock);
		exit(1);
	}
	unlink(szSockName);
	ipc_sock_addr.sun_family = AF_UNIX;
	strcpy(ipc_sock_addr.sun_path, szSockName);

	len = sizeof(ipc_sock_addr.sun_family) + strlen(ipc_sock_addr.sun_path);
	if (bind(sock, &ipc_sock_addr, len) < 0) {
		fprintf(stderr, "Failed to bind\n");
		exit(1);
	}
	return sock;
}

int udprdrOpen(UdpRdrCtxT *pCtx, const char *szUdpSrc)
{
	int sock;										/* Socket descriptor */
	struct sockaddr_in local_sa;
	int res = 0;

	int windowSize = 1024*1024;
    int windowSizeLen = sizeof(int);
	int on = 1;

	res = ParseUdpSrc(szUdpSrc, &pCtx->McastAddr, &pCtx->usRxPort, &pCtx->fMulticast);
	if(res < 0) {
		fprintf(stderr, "Failed to parse %s", szUdpSrc);
		return -1;
	}

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock == -1) {  
		fprintf(stderr, "Failed to create socket");
		return -1;
	}
	

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on)) < 0) {
		fprintf(stderr,"setsockopt(SO_REUSEADDR) failed");
		return -1;
	}


    if ( setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&windowSize, windowSizeLen) == -1 )  {
		fprintf(stderr, "setsockopt Failed");
        return -1;
    }

	memset(&local_sa, 0, sizeof(local_sa));
	local_sa.sin_family = AF_INET;
	local_sa.sin_port = htons(pCtx->usRxPort);
	local_sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if ( bind(sock, (struct sockaddr*)&local_sa, sizeof(struct sockaddr)) != 0 ) {
		fprintf(stderr, "bind Failed \n");
		return -1;
	}

	if(pCtx->fMulticast) {
		struct ip_mreq mreq;
		mreq.imr_multiaddr.s_addr = pCtx->McastAddr.s_addr;
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		printf("Joining Multicast group");
		if ( setsockopt( sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq)) < 0){
			fprintf(stderr, "IP_ADD_MEMBERSHIP Failed \n");
			return -1;
		}
	} else {
		fprintf(stderr, "%s:%s:Listening on unicastt port %d\n", __FILE__, __FUNCTION__, pCtx->usRxPort);
	}
	pCtx->mhSock = sock;
	pCtx->mhIpcSock = OpenIpcSocket(DVM_ETH_SRC_SRV/*DVM_ETH_SRC*/);
	printf("sock =%d\n", sock);
	return 0;
}

void udprdrClose(UdpRdrCtxT *pCtx)
{
	if(pCtx) {
		close(pCtx->mhSock);
	}
}


static int Dispatch(UdpRdrCtxT *pCtx, char *pData, int nLen)
{
	int nPid = -1;
	int res=0;
	int addr_len = 0;
	struct sockaddr_un ipc_sock_addr;
	ipc_sock_addr.sun_family = AF_UNIX;
	addr_len = sizeof(ipc_sock_addr.sun_family) + strlen(ipc_sock_addr.sun_path);
	strcpy(ipc_sock_addr.sun_path, DVM_ETH_SRC);

	res = sendto(pCtx->mhIpcSock, pData, nLen, 0, &ipc_sock_addr, addr_len);
	if(res <= 0){
		fprintf(stderr,"%s:%s:Failed to send %d. err=0x%d\n", __FILE__, __FUNCTION__, nLen, errno);
	}

	while(nLen > 0){
		// Get PID
		if(pData[0] == 0x47) {
			nPid = (pData[1] &  0x1F) << 8 | (pData[2] &  0xFF);
			//fprintf(stderr,"%s:%s: PID=%d\n", __FILE__, __FUNCTION__, nPid);
			if(nPid >= 0 && nPid < MAX_PID_ID && pCtx->listPktProcess[nPid] != NULL) {
				pCtx->listPktProcess[nPid](nPid, pData, pCtx->nTsPktLen);
			}
		}
		nLen -= pCtx->nTsPktLen;
		pData += pCtx->nTsPktLen;
	}
	return 0;
}

static void *thrdUdprdrStreaming(void *pArg)
{
	fd_set rfds;
	struct timeval tv;
	int ret = -1;
	int	selectRet;
	int nMaxLen = 21 * 188;
	char *pData = (char *)malloc(nMaxLen);
	unsigned long ulFlags = 0;	
	UdpRdrCtxT *pCtx = (UdpRdrCtxT *)pArg;

	pCtx->fStreaming = 1;

	while(1) {

		FD_ZERO(&rfds);
		FD_SET(pCtx->mhSock, &rfds);
		tv.tv_sec = 0; 
		tv.tv_usec = 100000;

		selectRet = select(pCtx->mhSock+1, &rfds, NULL, NULL, &tv);
		if(selectRet == 0) {
			continue;
		} else if(selectRet == -1) {
			break;
		}

		ret = recv(pCtx->mhSock, pData, nMaxLen, 0);

		Dispatch(pCtx, pData, ret);

		if(pCtx->nUiCmd == UDPRDR_CMD_STOP){
			pCtx->fStreaming = 0;
			break;
		}
	}

	free(pData);
	fprintf(stderr,"%s:exiting\n", __FUNCTION__);
	return NULL;
}


int udprdrStart(UdpRdrCtxT *pCtx)
{
	int nThreadId;
	pthread_create(&pCtx->hThread, NULL, thrdUdprdrStreaming, pCtx);
	return 0;
}

int udprdrStop(UdpRdrCtxT *pCtx)
{
	void *res;
	int nTimeOut = 1000000; // 100 milli sec

	if(pCtx->fStreaming) {
		pCtx->nUiCmd = UDPRDR_CMD_STOP;
		while(pCtx->fStreaming && nTimeOut > 0) {
			nTimeOut -= 1000;
			usleep(1000);
		}
	}


	if(pCtx->fStreaming) {
		// Close the file to force EoS
		if (pCtx->mhSock != -1)  {
			close (pCtx->mhSock);
			pCtx->mhSock = -1;
		}

		nTimeOut = 1000000;
		while(pCtx->fStreaming && nTimeOut > 0) {
			nTimeOut -= 1000;
			usleep(1000);
		}
	}

	if(pCtx->fStreaming) {
		pthread_cancel(pCtx->hThread);
		pthread_join (pCtx->hThread, (void **) &res);
	}

	return 0;
}

void SetPktProcessCallback(UdpRdrCtxT *pCtx, int nPid, pfnPktCallback_T pFn)
{
	if(nPid >= 0 && nPid < MAX_PID_ID) {
		pCtx->listPktProcess[nPid] = pFn;
	}
}
UdpRdrCtxT *udprdrCreate()
{
	UdpRdrCtxT *pCtx =  (void *)malloc(sizeof(UdpRdrCtxT));
	memset(pCtx, 0x00, sizeof(UdpRdrCtxT));

	pCtx->nTsPktLen = 188;
	return pCtx;
}

void udprdrDelete(UdpRdrCtxT *pCtx)
{
	free(pCtx);
}
