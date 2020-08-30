#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#include <Ws2ipdef.h>
#define close	closesocket
#define snprintf	_snprintf
#define write	_write
#else
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <strings.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "JdUdp.h"

#define DEFAULT_READ_TIMEOUT	10		/* Seconds to wait before giving up


#define CHECK_ALLOC(x) if(x==NULL) {fprintf(stderr,"malloc failed at %s %d, exiting..\n", __FUNCTION__, __LINE__); goto Exit;}

/* Globals */
static int timeout = DEFAULT_READ_TIMEOUT;

int CUdp::CreateSession()
{
		int sock;										/* Socket descriptor */
		struct sockaddr_in dest_sa;							/* Socket address */
		struct sockaddr_in local_sa;
		int ret;

		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if(sock == -1) {  
			return -1; 
		}
		int on = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on)) < 0) {
			fprintf(stderr,"setsockopt(SO_REUSEADDR) failed");
			return -1;
		}

        int windowSize = 1024*1024;
        int windowSizeLen = sizeof(INT32);
        if ( setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&windowSize, windowSizeLen) == SOCKET_ERROR )  {
            int err = -1;
        }

		memset(&local_sa, 0, sizeof(local_sa));
		local_sa.sin_family = AF_INET;
		local_sa.sin_port = htons(mLocalRtpPort);
		local_sa.sin_addr.s_addr = htonl(INADDR_ANY);

		if ( bind(sock, (struct sockaddr*)&local_sa, sizeof(struct sockaddr)) != 0 ) {
			return -1; 
		}

		if(mMulticast) {
			if(mMode == CUdp::MODE_CLIENT) {
				struct ip_mreq mreq;
				mreq.imr_multiaddr.s_addr = m_SockAddr.s_addr;
				mreq.imr_interface.s_addr = htonl(INADDR_ANY);
				if ( setsockopt( sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq)) < 0){
					// Error
				}
			}
		}

#ifdef EN_USE_PEER_PORT
		/* Copy host address from hostent to (server) socket address */
		memcpy((char *)&dest_sa.sin_addr, (char *)pDestAddr, sizeof(in_addr));

		dest_sa.sin_port = htons(usRtpDestPort);      	/* Put portnum into sockaddr */
		dest_sa.sin_family = AF_INET;			/* Set service sin_family to PF_INET */
		ret = connect(sock, (sockaddr *)&dest_sa, sizeof(dest_sa));
		if(ret == -1) {  
			perror("connect");
			return -1; 
		}
#endif
		m_hSock = sock;
		return 0;
	}

int CUdp::Read(char *pData, int nMaxLen)
{
	fd_set rfds;
	struct timeval tv;
	int ret = -1;
	int	selectRet;
	/* Begin reading the body of the file */

	FD_ZERO(&rfds);
	FD_SET(m_hSock, &rfds);

	tv.tv_sec = timeout; 
	tv.tv_usec = 0;

	if(timeout >= 0)
		selectRet = select(m_hSock+1, &rfds, NULL, NULL, &tv);
	else		/* No timeout, can block indefinately */
		selectRet = select(m_hSock+1, &rfds, NULL, NULL, NULL);

	if(selectRet == 0) {
		return -1;
	} else if(selectRet == -1)	{
		return -1;
	}

	ret = recv(m_hSock, pData, nMaxLen, 0);

	return ret;
}

int CUdp::Write(char *pData, int lLen)
{
	fd_set rfds;
	struct timeval tv;
	int ret = -1;
	int	selectRet;
	int bytesWritten= 0;
	/* Begin reading the body of the file */

	struct sockaddr_in peerAddr;
	peerAddr.sin_addr = m_SockAddr;
	peerAddr.sin_port = htons(mRemoteRtpPort);
	peerAddr.sin_family = PF_INET;

	ret = sendto(m_hSock, pData, lLen, 0, (struct sockaddr *)&peerAddr, sizeof(struct sockaddr));
	if(ret == -1)	{
		return -1;
	}
	return lLen;
}
