#ifndef __UDPRDR_H__
#define __UDPRDR_H__

#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#include <Ws2ipdef.h>
#define close	closesocket
#define snprintf	_snprintf
#define write	_write
#else
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <strings.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/un.h>
#endif

#define  UDPRDR_CMD_STOP        1
#define  UDPRDR_CMD_PAUSE       2
#define  UDPRDR_CMD_RUN		    3

#define  DVM_ETH_SRC        "/dev/dvm_eth"	    // UDP Named client socket
#define  DVM_ETH_SRC_SRV    "/dev/dvm_eth_srv"	// UDP Named server socket
#define  MAX_PID_ID         0x1FFF

typedef int (*pfnPktCallback_T)(int PID, char *pData, int nLen);

typedef struct _UdpRdrCtxT
{
	int                fMulticast;
	int                SockAddr;
	unsigned short     usRxPort;
	struct in_addr     McastAddr;
	int                mhSock;
	int                fStreaming;
	int                nUiCmd;
#ifdef WIN32
	HANDLE             hThread;
#else
	pthread_t          hThread;
#endif
	int                mhIpcSock;
	int                nTsPktLen;
	// TODO: Convert to a linked list.
	pfnPktCallback_T   listPktProcess[MAX_PID_ID];
} UdpRdrCtxT;


int udprdrOpen(UdpRdrCtxT *pCtx, const char *szUdpSrc);

void udprdrClose(UdpRdrCtxT *pCtx);

int udprdrStart(UdpRdrCtxT *pCtx);

int udprdrStop(UdpRdrCtxT *pCtx);

UdpRdrCtxT *udprdrCreate();

void udprdrDelete(UdpRdrCtxT *pCtx);

void SetPktProcessCallback(UdpRdrCtxT *pCtx, int nPid, pfnPktCallback_T pFn);

#endif
