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
#include "JdRtp.h"
#include "JdDbg.h"

#ifdef WIN32
#else
#define INT32 int
#define SOCKET_ERROR -1
#endif

#define DEFAULT_READ_TIMEOUT	10		//Seconds to wait before giving up
static int modDbgLevel = CJdDbg::LVL_ERR;
/* Globals */
static int timeout = DEFAULT_READ_TIMEOUT;

int CRtp::CreateSession()
	{
		int sock;										/* Socket descriptor */
		struct sockaddr_in dest_sa;							/* Socket address */
		struct sockaddr_in local_sa;
		int ret;
		JDBG_LOG(CJdDbg::LVL_TRACE,("LocalPort=%d", mLocalRtpPort));
		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if(sock == -1) {  
			JDBG_LOG(CJdDbg::LVL_ERR,("Failed to create socket"));
			return -1; 
		}
		int on = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on)) < 0) {
			JDBG_LOG(CJdDbg::LVL_ERR,("setsockopt(SO_REUSEADDR) failed"));
			return -1;
		}

        int windowSize = 1024*1024;
        int windowSizeLen = sizeof(INT32);
        if ( setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&windowSize, windowSizeLen) == SOCKET_ERROR )  {
			JDBG_LOG(CJdDbg::LVL_ERR,("setsockopt(SO_RCVBUF) failed"));
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
			if(mMode == CRtp::MODE_CLIENT) {
				struct ip_mreq mreq;
				mreq.imr_multiaddr.s_addr = m_SockAddr.s_addr;
				mreq.imr_interface.s_addr = htonl(INADDR_ANY);
				if ( setsockopt( sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq)) < 0){
					JDBG_LOG(CJdDbg::LVL_ERR,("setsockopt(IP_ADD_MEMBERSHIP) failed"));
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
		m_hRtpSock = sock;


		local_sa.sin_port = htons(mLocalRtcpPort); /* is already in right byteorder */
		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if(sock == -1) {  
			JDBG_LOG(CJdDbg::LVL_ERR,("socket failed"));
			return -1; 
		}

		if ( bind(sock, (struct sockaddr*)&local_sa, sizeof(local_sa)) != 0 ) {
			JDBG_LOG(CJdDbg::LVL_ERR,("bind failed"));
			return -1; 
		}
#ifdef EN_USE_PEER_PORT
		dest_sa.sin_port = htons(usRtcpDestPort);      	/* Put portnum into sockaddr */
		ret = connect(sock, (struct sockaddr *)&dest_sa, sizeof(dest_sa));

		if(ret == -1) {  
			perror("connect");
			return -1; 
		}
#endif
		m_hRtcpSock = sock;
		return 0;
	}
int CRtp::Read(char *pData, int nMaxLen)
{
	fd_set rfds;
	struct timeval tv;
	int ret = -1;
	int	selectRet;
	/* Begin reading the body of the file */

	FD_ZERO(&rfds);
	FD_SET(m_hRtpSock, &rfds);

	tv.tv_sec = timeout; 
	tv.tv_usec = 0;

	if(timeout >= 0)
		selectRet = select(m_hRtpSock+1, &rfds, NULL, NULL, &tv);
	else		/* No timeout, can block indefinately */
		selectRet = select(m_hRtpSock+1, &rfds, NULL, NULL, NULL);

	if(selectRet == 0) {
		return -1;
	} else if(selectRet == -1)	{
		JDBG_LOG(CJdDbg::LVL_ERR,("select failed"));
		return -1;
	}

	ret = recv(m_hRtpSock, pData, nMaxLen, 0);

	return ret;
}

int CRtp::Write(char *pData, int lLen)
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
	
	JDBG_LOG(CJdDbg::LVL_TRACE,("sendto addr=0x%x port=%d", *((unsigned long *)&m_SockAddr), mRemoteRtpPort));
	ret = sendto(m_hRtpSock, pData, lLen, 0, (struct sockaddr *)&peerAddr, sizeof(struct sockaddr));
	if(ret == -1)	{
		JDBG_LOG(CJdDbg::LVL_ERR,("sendto failed"));
		return -1;
	}
	return lLen;
}


void CRtp::CreateHeader(char *pData, RTP_PKT_T *pHdr)
{
	pData[0] = ((pHdr->v << 6) & 0xC0) | ((pHdr->p << 5) & 0x20) | ((pHdr->x << 4) & 0x10) | ( pHdr->cc & 0x0F);
	pData[1] = ((pHdr->m << 7) & 0x80) | (pHdr->pt & 0x7F);
	pData[2] = (unsigned char)(pHdr->usSeqNum >> 8); pData[3] = (unsigned char)(pHdr->usSeqNum);
	pData[4] = (unsigned char)(pHdr->ulTimeStamp >> 24); pData[5] = (unsigned char)(pHdr->ulTimeStamp >> 16);
	pData[6] = (unsigned char)(pHdr->ulTimeStamp >> 8); pData[7] = (unsigned char)pHdr->ulTimeStamp;
	pData[8] = (unsigned char)(pHdr->ulSsrc >> 24); pData[9] = (unsigned char)(pHdr->ulSsrc >> 16);
	pData[10] = (unsigned char)(pHdr->ulSsrc >> 8); pData[11] = (unsigned char)pHdr->ulSsrc;

}
void CRtp::ParseHeader(char *pData, RTP_PKT_T *pHdr)
{
	pHdr->v = (pData[0] & 0xC0 ) >> 6;
	pHdr->p = (pData[0] & 0x20 ) >> 5;
	pHdr->x = (pData[0] & 0x10 ) >> 4;
	pHdr->cc = (pData[0] & 0x0F );
	pHdr->m = (pData[1] & 0x80 ) >> 7;
	pHdr->pt = (pData[1] & 0x7F );
	pHdr->usSeqNum = (((unsigned short)pData[2] & 0x00ff) << 8) | (((unsigned short)pData[3] & 0x00ff ));
	pHdr->ulTimeStamp = ((unsigned long)pData[4] << 24) | ((unsigned long)pData[5] << 16) | ((unsigned long)pData[6] << 8) | ((unsigned long)pData[7]);
	pHdr->ulSsrc= ((unsigned long)pData[8] << 24) | ((unsigned long)pData[9] << 16) | ((unsigned long)pData[10] << 8) | ((unsigned long)pData[11]);
}

