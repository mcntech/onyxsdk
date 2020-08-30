#ifndef __RTP_H__
#define __RTP_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
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
#endif

#define NOT_IMPLEMENTED fprintf(stderr,"%s:Not Implemented.\n",__FUNCTION__);

typedef struct _RTP_PKT_T
{
	unsigned char v;
	unsigned char p;
	unsigned char x;
	unsigned char cc;
	unsigned char m;
	unsigned char pt;
	unsigned short usSeqNum;
	unsigned long ulTimeStamp;
	unsigned long ulSsrc;
} RTP_PKT_T;

class CRtp
{
public:
	typedef enum _TransferMode
	{
		MODE_SERVER,
		MODE_CLIENT,
		MODE_BOTH,
	} TransferMode;

	CRtp(TransferMode Mode)
	{
		m_hRtpSock = -1;
		m_hRtcpSock = -1;
		mMode = Mode;
		memset(&m_SockAddr, 0x00, sizeof(m_SockAddr));
	}
	virtual ~CRtp()
	{

	}

	int CreateSession();

	virtual void CloseSession()
	{
		if(m_hRtpSock != -1)
			close(m_hRtpSock);
		if(m_hRtcpSock != -1)
			close(m_hRtcpSock);
	}
	virtual int Start() {NOT_IMPLEMENTED; return -1;}
	virtual int Pause() {NOT_IMPLEMENTED; return -1;}
	virtual int Stop(){NOT_IMPLEMENTED; return -1;}
	virtual int Read(char *pData, int nMaxLen);
	virtual int Write(char *pData, int nMaxLen);
	static void CreateHeader(char *pData, RTP_PKT_T *pHdr);
	static void ParseHeader(char *pData, RTP_PKT_T *pHdr);

	int	           m_hRtpSock;
	int	           m_hRtcpSock;
	in_addr        m_SockAddr;
	unsigned short mRemoteRtpPort;
	unsigned short mRemoteRtcpPort;
	unsigned short mLocalRtpPort;
	unsigned short mLocalRtcpPort;
	bool           mMulticast;
	TransferMode   mMode;

};

#endif //__RTP_H__